//------------------------------------------------------------------------------
//! Server side of a submitted report: rate limiting, resolving the target, and
//! handing the finished thing to delivery.
//!
//! Everything a client sends is treated as a claim, not a fact. The reporter's
//! identity comes from the connection, the accused come from what the server
//! itself recorded, and the description is escaped before it goes anywhere.
//------------------------------------------------------------------------------
class MrFrost_ReportSubmit
{
	//! Player id -> world time in milliseconds of their last accepted report.
	protected static ref map<int, float> s_mLastReport;

	//! Bound on the name a client claims it picked, in bytes. A Reforger name is
	//! far shorter than this even written entirely in three-byte characters; the
	//! number exists to stop a modified client, not to fit a real player.
	protected static const int MAX_NAME = 128;

	//! Ceiling on cooldownSeconds. Seconds times 1000 overflows a signed 32-bit
	//! int above 2,147,483, and the wrapped negative compares as "never on
	//! cooldown" - so the operator who asked for the strictest throttle in the
	//! system would have got the weakest one. An hour is longer than any server
	//! has reason to set, and it also catches the likelier mistake: milliseconds
	//! typed into a field that counts seconds.
	static const int MAX_COOLDOWN_SECONDS = 3600;

	//------------------------------------------------------------------------------
	//! Accepts a report from a player. Returns a key from the text table for the
	//! client to show, so the reason a report bounced is worded in the player's
	//! own language.
	static string Accept(int reporterId, MrFrost_EReportKind kind, MrFrost_EReportTarget mode, int selectedId, string selectedName, string description)
	{
		// Throttled before anything else is touched. The cooldown below only
		// stamps an *accepted* report, so every path that bounces one - no
		// description, no target, nobody nearby, feature switched off - was
		// free, and a modified client could hold the server in a loop of
		// player sweeps and string work at packet rate. This floor is short
		// enough that no human hits it and does not consume the real cooldown,
		// so a player who forgot their description is not locked out.
		if (IsFlooding(reporterId))
			return "report.cooldown";

		MrFrost_ReportConfig config = MrFrost_ReportConfigLoader.Get();

		if (!MrFrost_ReportConfigLoader.IsEnabled())
			return "report.disabled";

		// The wire carries an int, so the enum is a claim like everything else.
		// Testing only for equality let a third value past both permission
		// checks and out the other side as a bug report, on a server that had
		// switched those off.
		if (kind != MrFrost_EReportKind.BUG && kind != MrFrost_EReportKind.PLAYER)
			return "report.disabled";

		if (kind == MrFrost_EReportKind.BUG && !config.m_bAllowBugReports)
			return "report.disabled";

		if (kind == MrFrost_EReportKind.PLAYER && !config.m_bAllowPlayerReports)
			return "report.disabled";

		// TrimInPlace rather than Trim: Trim() returns a copy capped at 8191
		// bytes, and the cut is blind to characters. With maxDescription at its
		// documented maximum of 8191 the cut below would then measure a string
		// already at the limit, leave it alone, and pass half a character into
		// the embed JSON - which Discord answers with a 400, losing the report.
		description.TrimInPlace();
		if (description.IsEmpty())
			return "report.need_description";

		// The one client string that used to reach the server unbounded. It is
		// never stored and never shown - it exists only to be compared against
		// the live name - but the mismatch is logged, and the rejection it comes
		// from happens before the cooldown is stamped. That left a modified
		// client writing two log lines a second, each as long as it liked, with
		// newlines in them to make each look like several. Bounded and stripped
		// here, once, before anything reads it.
		selectedName = MrFrost_ServerContent.ForLog(selectedName, MAX_NAME);

		// Trimmed rather than rejected: a player who wrote too much should not
		// lose what they wrote, and the cut is on the server so no client can
		// send a megabyte of text at the webhook.
		//
		// Truncate rather than Substring: the limit counts bytes, and landing
		// inside a multi-byte character would leave half of one in the JSON and
		// in the log line.
		// Floored here as well as when a JSON file is read, because the addon
		// config reaches this field by another road. Truncate treats a limit of
		// zero or less as no limit at all, so a build shipping m_iMaxDescription
		// at 0 would have sent whatever a player typed straight through.
		int limit = config.m_iMaxDescription;
		if (limit <= 0)
			limit = MrFrost_ReportConfig.DEFAULT_MAX_DESCRIPTION;

		description = MrFrost_ServerContent.Truncate(description, limit);

		if (IsOnCooldown(reporterId, config))
			return "report.cooldown";

		array<int> targets = {};
		bool nobodyNearby = false;
		if (kind == MrFrost_EReportKind.PLAYER)
		{
			MrFrost_ReportTargets.Resolve(reporterId, mode, selectedId, selectedName, targets);

			if (targets.IsEmpty())
			{
				if (mode != MrFrost_EReportTarget.NEARBY)
					return "report.need_target";

				// Not a rejection. "Nobody is within 300 m" is a useful answer to a
				// player who wanted to report someone, and this used to return it
				// instead of filing the report - so with the setting on, the report
				// was thrown away, the cooldown was never spent, the player was told
				// something true about their surroundings and nothing about their
				// report, and the menu stayed open so they could send it again and
				// lose it again. The comment here claimed the opposite of what the
				// code did. The report goes either way; the setting only decides
				// whether the answer mentions that nobody was named.
				nobodyNearby = config.m_bRevealNobodyNearby;
			}
		}

		// Stamped either way. A report the server could not put anywhere is not a
		// sent report and the player is told so - but retrying immediately will
		// not help, because whatever is broken is still broken. Letting a failure
		// go unstamped took the cooldown off entirely on exactly the server that
		// was already struggling, leaving only the half-second flood floor: a
		// twentyfold rise in requests, each one an O(players) sweep and a line
		// into the log, at the moment relief was most needed.
		bool built = Build(reporterId, kind, targets, description);
		Stamp(reporterId);

		if (!built)
			return "report.failed";

		if (nobodyNearby)
			return "report.nobody_nearby";

		return "report.sent";
	}

	//------------------------------------------------------------------------------
	//! Shortest gap between two requests from one player, accepted or not.
	protected static const float REQUEST_FLOOR_MS = 500;

	//! Player id -> world time of their last request of any kind.
	protected static ref map<int, float> s_mLastRequest;

	//------------------------------------------------------------------------------
	//! Whether this player is sending faster than a person can click.
	protected static bool IsFlooding(int reporterId)
	{
		if (!s_mLastRequest)
			s_mLastRequest = new map<int, float>();

		float now = GetGame().GetWorld().GetWorldTime();

		float last;
		if (s_mLastRequest.Find(reporterId, last))
		{
			// A stamp ahead of the clock is one from before a mission restart,
			// the same case IsOnCooldown handles below.
			if (now >= last && (now - last) < REQUEST_FLOOR_MS)
				return true;
		}

		s_mLastRequest.Set(reporterId, now);
		return false;
	}

	//------------------------------------------------------------------------------
	protected static bool IsOnCooldown(int reporterId, notnull MrFrost_ReportConfig config)
	{
		if (config.m_iCooldownSeconds <= 0)
			return false;

		if (!s_mLastReport)
			return false;

		float last;
		if (!s_mLastReport.Find(reporterId, last))
			return false;

		float now = GetGame().GetWorld().GetWorldTime();

		// World time restarts at zero with the mission while this map, being
		// static, survives it. A stamp from the previous mission then sits in the
		// future, and the subtraction below would report every player as on
		// cooldown until the clock caught up again - minutes, or an hour on a
		// long-running server. Treat a stamp ahead of the clock as no cooldown;
		// the next report overwrites it anyway.
		if (now < last)
			return false;

		// Clamped here rather than where the value is read, because it arrives by
		// two roads - the JSON file and the addon config - and this is the only
		// place either one is used.
		int seconds = config.m_iCooldownSeconds;
		if (seconds > MAX_COOLDOWN_SECONDS)
			seconds = MAX_COOLDOWN_SECONDS;

		// 1000.0, not 1000: an int multiplication would still wrap for anything
		// the clamp let through if that ceiling is ever raised.
		return (now - last) < (seconds * 1000.0);
	}

	//------------------------------------------------------------------------------
	//! Drops a player's cooldown when they leave, so the next holder of that id
	//! does not inherit it.
	static void Forget(int playerId)
	{
		if (s_mLastReport)
			s_mLastReport.Remove(playerId);

		if (s_mLastRequest)
			s_mLastRequest.Remove(playerId);
	}

	//------------------------------------------------------------------------------
	protected static void Stamp(int reporterId)
	{
		if (!s_mLastReport)
			s_mLastReport = new map<int, float>();

		s_mLastReport.Set(reporterId, GetGame().GetWorld().GetWorldTime());
	}

	//------------------------------------------------------------------------------
	//! Returns false when delivery found nowhere to put the report.
	protected static bool Build(int reporterId, MrFrost_EReportKind kind, notnull array<int> targets, string description)
	{
		MrFrost_Report report = new MrFrost_Report();

		report.m_eKind = kind;
		report.m_sReporter = MrFrost_ReportTargets.Describe(reporterId);
		report.m_sDescription = description;
		report.m_sTime = Timestamp();
		report.m_sTimeUtc = TimestampUtc();
		report.m_sPosition = Position(reporterId);

		string names;
		foreach (int index, int targetId : targets)
		{
			if (index > 0)
				names += ", ";

			names += MrFrost_ReportTargets.Describe(targetId);
		}

		report.m_sTargets = names;

		if (!MrFrost_ReportDelivery.Deliver(report))
			return false;

		MrFrost_Log.Info("Report accepted from " + report.m_sReporter + " (" + report.GetKindLabel() + ").");

		return true;
	}

	//------------------------------------------------------------------------------
	//! Where the reporter was standing.
	//!
	//! Worth having for both kinds: a bug is usually somewhere specific, and a
	//! rule dispute is usually about where people were.
	protected static string Position(int reporterId)
	{
		PlayerManager playerManager = GetGame().GetPlayerManager();
		if (!playerManager)
			return string.Empty;

		IEntity entity = playerManager.GetPlayerControlledEntity(reporterId);
		if (!entity)
			return string.Empty;

		vector origin = entity.GetOrigin();
		return Math.Round(origin[0]).ToString() + " / " + Math.Round(origin[2]).ToString();
	}

	//------------------------------------------------------------------------------
	//! The server machine's wall clock, as "yyyy-mm-dd hh:mm:ss".
	//!
	//! Real time rather than mission time: a report is matched against a ban list,
	//! a recording or the server's own log, and all of those are dated. Read on
	//! the server, so every report carries one clock no matter where a player is.
	protected static string Timestamp()
	{
		return SCR_DateTimeHelper.GetDateTimeLocal();
	}

	//------------------------------------------------------------------------------
	//! The same moment in UTC, ISO-8601, for Discord's own timestamp field.
	//!
	//! Discord renders that field in each reader's local time, so a moderator in
	//! another country reads their own clock without anyone converting anything.
	protected static string TimestampUtc()
	{
		int year, month, day, hour, minute, second;
		System.GetYearMonthDayUTC(year, month, day);
		System.GetHourMinuteSecondUTC(hour, minute, second);

		return SCR_FormatHelper.FormatDate(year, month, day) + "T" + SCR_FormatHelper.FormatTime(hour, minute, second) + "Z";
	}
}
