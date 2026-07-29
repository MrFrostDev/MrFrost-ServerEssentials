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

	//------------------------------------------------------------------------------
	//! Accepts a report from a player. Returns a key from the text table for the
	//! client to show, so the reason a report bounced is worded in the player's
	//! own language.
	static string Accept(int reporterId, MrFrost_EReportKind kind, MrFrost_EReportTarget mode, int selectedId, string description)
	{
		MrFrost_ReportConfig config = MrFrost_ReportConfigLoader.Get();

		if (!MrFrost_ReportConfigLoader.IsEnabled())
			return "report.disabled";

		if (kind == MrFrost_EReportKind.BUG && !config.m_bAllowBugReports)
			return "report.disabled";

		if (kind == MrFrost_EReportKind.PLAYER && !config.m_bAllowPlayerReports)
			return "report.disabled";

		description = description.Trim();
		if (description.IsEmpty())
			return "report.need_description";

		// Trimmed rather than rejected: a player who wrote too much should not
		// lose what they wrote, and the cut is on the server so no client can
		// send a megabyte of text at the webhook.
		if (config.m_iMaxDescription > 0 && description.Length() > config.m_iMaxDescription)
			description = description.Substring(0, config.m_iMaxDescription);

		if (IsOnCooldown(reporterId, config))
			return "report.cooldown";

		array<int> targets = {};
		if (kind == MrFrost_EReportKind.PLAYER)
		{
			MrFrost_ReportTargets.Resolve(reporterId, mode, selectedId, targets);

			if (targets.IsEmpty())
			{
				if (mode != MrFrost_EReportTarget.NEARBY)
					return "report.need_target";

				// "Nobody is within 300 m" is a useful answer to a player who
				// wanted to report someone, and an equally useful one to a player
				// who just wanted to know whether anyone is out there. Off by
				// default: the report still goes, it simply names nobody, and the
				// reporter is told the same thing either way.
				if (config.m_bRevealNobodyNearby)
					return "report.nobody_nearby";
			}
		}

		Build(reporterId, kind, targets, description);
		Stamp(reporterId);

		return "report.sent";
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
		return (now - last) < (config.m_iCooldownSeconds * 1000);
	}

	//------------------------------------------------------------------------------
	protected static void Stamp(int reporterId)
	{
		if (!s_mLastReport)
			s_mLastReport = new map<int, float>();

		s_mLastReport.Set(reporterId, GetGame().GetWorld().GetWorldTime());
	}

	//------------------------------------------------------------------------------
	protected static void Build(int reporterId, MrFrost_EReportKind kind, notnull array<int> targets, string description)
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

		MrFrost_ReportDelivery.Deliver(report);

		MrFrost_Log.Info("Report accepted from " + report.m_sReporter + " (" + report.GetKindLabel() + ").");
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
