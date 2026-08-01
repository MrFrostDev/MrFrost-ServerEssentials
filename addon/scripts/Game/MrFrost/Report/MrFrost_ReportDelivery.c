//------------------------------------------------------------------------------
//! Where a finished report goes: the server's log file and a Discord webhook.
//!
//! Both, and independently — the log is the record that survives a deleted
//! message and works with no internet at all, the webhook is what actually
//! reaches a moderator. A failure of one never stops the other.
//!
//! Server side only. The webhook URL is a secret and is never replicated.
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
//! One report, already resolved: names instead of ids, server time attached.
class MrFrost_Report
{
	MrFrost_EReportKind m_eKind;
	string m_sReporter;
	string m_sTargets;
	string m_sDescription;
	string m_sPosition;
	//! Server wall clock, for the log file and the embed field.
	string m_sTime;

	//! The same moment in ISO-8601 UTC, for Discord's timestamp field.
	string m_sTimeUtc;

	//------------------------------------------------------------------------------
	string GetKindLabel()
	{
		if (m_eKind == MrFrost_EReportKind.PLAYER)
			return "PLAYER";

		return "BUG";
	}
}

//------------------------------------------------------------------------------
class MrFrost_ReportDelivery
{
	//! Discord allows roughly five requests per two seconds per webhook. One
	//! message every second and a half stays clear of that even when several
	//! players report at once.
	protected static const int SEND_INTERVAL_MS = 1500;

	//! Held below Discord's own limits, which are 4096 for the body and 1024 for
	//! a field. The margin covers the JSON escaping, which can double a string's
	//! length without changing what it says. Over either limit Discord rejects
	//! the whole embed, so both are enforced here rather than trusted to a server
	//! owner's maxDescription.
	protected static const int MAX_DESCRIPTION = 4000;
	protected static const int MAX_FIELD = 1000;

	//! Discord counts a field's name against its own, much shorter limit. Sharing
	//! the value limit here meant a long label - a translated one, or one a server
	//! renamed - passed our check and was rejected by Discord, taking the whole
	//! report with it.
	protected static const int MAX_FIELD_NAME = 256;

	//! The title above the embed and the footer under it. Both are server text -
	//! a label a server renamed, its own name - so both are as capable of
	//! exceeding a limit as anything a player types, and neither was bounded.
	//! Held well under Discord's own 256 and 2048 so that the whole embed, whose
	//! parts share a 6000-character ceiling, cannot be pushed over it by them.
	protected static const int MAX_TITLE = 256;
	protected static const int MAX_FOOTER = 256;

	//! Discord counts every part of an embed against one ceiling of its own and
	//! rejects the whole message over it, and the per-slot limits above add up to
	//! far more than that. Held under the real 6000 so the assembled JSON has
	//! room the count does not see.
	protected static const int MAX_EMBED = 5600;

	//! Discord's limit on the name a webhook message signs itself with. Long
	//! enough for any server name; past it, every report was rejected.
	protected static const int MAX_USERNAME = 80;

	//! How many reports may wait for Discord at once. About five minutes of
	//! backlog at the send interval, already longer than a moderator wants.
	protected static const int MAX_QUEUE = 200;

	//! Nothing this long is a working image URL, and a truncated one is worse
	//! than none, so an over-long value is dropped rather than cut.
	protected static const int MAX_AVATAR_URL = 2000;

	//! Used when a server names no log file, or names something that is a path.
	protected static const string DEFAULT_LOG_FILE = "reports.log";

	//! How many send intervals of silence, with reports waiting, mean the
	//! repeating call is gone rather than merely between ticks. Three is far
	//! outside the jitter of a 1500 ms timer and still under five seconds.
	protected static const int STALL_INTERVALS = 3;

	//! Reports waiting for their turn at the webhook.
	protected static ref array<ref MrFrost_Report> s_aQueue;
	protected static bool s_bSending;

	//! World time of the last report handed to the webhook. Only read to tell a
	//! stalled sender from a busy one.
	protected static float s_fLastSendAt;

	//! Held as a ref: a RestCallback that nobody owns is deleted the moment its
	//! call finishes, and the response would land in freed memory.
	protected static ref RestCallback s_Callback;

	//------------------------------------------------------------------------------
	//! Files a report. Returns false only when the server is set up to do nothing
	//! with it at all, which is worth telling the player about.
	static bool Deliver(notnull MrFrost_Report report)
	{
		MrFrost_ReportDeliveryConfig config = MrFrost_ReportConfigLoader.GetDelivery();
		if (!config)
		{
			// No report.json on this server. Still worth writing down, so a server
			// owner who never configured anything can find reports in the log.
			config = new MrFrost_ReportDeliveryConfig();
			MrFrost_ReportConfigLoader.SetDelivery(config);
		}

		bool delivered = false;

		if (config.m_bWriteLog)
			delivered = WriteToLog(report, config) || delivered;

		if (!config.m_sWebhookUrl.IsEmpty())
		{
			// Only counts as delivered if it was actually taken. A full queue drops
			// the report, and telling the player it was sent would leave them
			// believing an incident was reported when nothing was.
			delivered = Enqueue(report) || delivered;
		}

		if (!delivered)
			// Says only what it knows. A sink that was configured and failed has
			// already said so on its own line, and the old wording - "neither is
			// configured" - contradicted it, reporting a full disk as a missing
			// setting. A server with both switched off gets this line alone, which
			// is still the whole truth for it.
			MrFrost_Log.Warn("A report reached neither the log file nor Discord.");

		return delivered;
	}

	//------------------------------------------------------------------------------
	//! Keeps logFile a file name rather than a path.
	//!
	//! The value comes from a server's JSON, and anything containing a separator
	//! or ".." would place the log outside the MrFrost folder. A server owner
	//! already controls the machine, so this is not a privilege boundary - it
	//! stops a typo from writing somewhere nobody thinks to look, and stops one
	//! shared config from scattering logs across a box that hosts several
	//! servers.
	protected static string SafeLogName(string name)
	{
		if (name.IsEmpty())
			return DEFAULT_LOG_FILE;

		if (name.Contains("/") || name.Contains("\\") || name.Contains(":") || name.Contains(".."))
		{
			MrFrost_Log.WarnOnce("logFile", "logFile must be a file name, not a path. Using "
				+ DEFAULT_LOG_FILE + " instead of: " + name);
			return DEFAULT_LOG_FILE;
		}

		// Windows keeps a handful of names reserved for devices, and opening one
		// SUCCEEDS - every report would then be written to a device that discards
		// it while the console reported each one as written. On a server keeping
		// no webhook that is the only copy. Refused here because it is the one
		// failure of this kind that can be decided without asking the OS.
		if (IsDeviceName(name))
		{
			MrFrost_Log.WarnOnce("logFile", "logFile '" + name + "' is a reserved device name on Windows, and anything written to it is discarded. Using "
				+ DEFAULT_LOG_FILE + " instead.");
			return DEFAULT_LOG_FILE;
		}

		return name;
	}

	//------------------------------------------------------------------------------
	//! Whether a file name is one Windows reserves for a device.
	//!
	//! The reservation covers the stem, so "NUL", "nul.log" and "CON.txt" are all
	//! the device. Case does not matter, and the list is the whole of it.
	protected static bool IsDeviceName(string name)
	{
		string stem = name;

		// Windows drops trailing spaces before resolving a name, so "nul " is the
		// device just as "nul" is.
		stem.TrimInPlace();

		int dot = stem.IndexOf(".");
		if (dot >= 0)
			stem = stem.Substring(0, dot);

		stem.ToLower();

		if (stem == "con" || stem == "prn" || stem == "aux" || stem == "nul")
			return true;

		// COM1-COM9 and LPT1-LPT9. COM0 and LPT0 are not reserved.
		if (stem.Length() != 4)
			return false;

		string port = stem.Substring(0, 3);
		if (port != "com" && port != "lpt")
			return false;

		int digit = stem.ToAscii(3);
		return digit >= 49 && digit <= 57;
	}

	//------------------------------------------------------------------------------
	//! Appends one line per report.
	//!
	//! Append rather than rewrite, and one line per report, so the file can be
	//! tailed while the server runs and never has to be held in memory.
	protected static bool WriteToLog(notnull MrFrost_Report report, notnull MrFrost_ReportDeliveryConfig config)
	{
		string path = MrFrost_ServerContent.FOLDER + SafeLogName(config.m_sLogFile);

		FileHandle file = FileIO.OpenFile(path, FileMode.APPEND);
		if (!file)
		{
			// The folder exists as soon as a server has any MrFrost file at all,
			// but a server running purely on addon defaults may not have one yet.
			FileIO.MakeDirectory(MrFrost_ServerContent.FOLDER);
			file = FileIO.OpenFile(path, FileMode.APPEND);
		}

		if (!file)
		{
			MrFrost_Log.Error("Could not write to " + path);
			return false;
		}

		string line = report.m_sTime + " | " + report.GetKindLabel() + " | by " + report.m_sReporter;

		if (!report.m_sTargets.IsEmpty())
			line += " | against " + report.m_sTargets;

		if (!report.m_sPosition.IsEmpty())
			line += " | at " + report.m_sPosition;

		// Newlines would break one-report-per-line, which is the only thing that
		// makes the file greppable.
		string description = report.m_sDescription;
		description.Replace("\n", " ");
		description.Replace("\r", " ");

		// Every other byte below 0x20 goes too. A description can carry them - the
		// Discord sink has stripped them since it was written and this one had
		// not, so the two records of the same report disagreed. An escape sequence
		// in a file an owner tails erases the line above it, which makes the log
		// that is meant to be the record surviving a deleted message forgeable by
		// the person it records.
		line += " | " + MrFrost_ServerContent.StripControls(description);

		// WriteLine returns void, so a write that fails after the handle opened -
		// a full volume is the realistic one - cannot be seen from here, and this
		// answers true for it. Read() and Write() do return a count; WriteLine
		// does not, and using Write() instead would mean building the line
		// terminator by hand on a handle the rest of the file treats as text. The
		// case that IS decidable, a name the OS accepts but discards, is refused
		// in SafeLogName rather than guessed at here.
		file.WriteLine(line);
		file.Close();

		MrFrost_Log.Info("Report written to " + path);
		return true;
	}

	//------------------------------------------------------------------------------
	//! Returns false when the report was dropped rather than queued.
	protected static bool Enqueue(notnull MrFrost_Report report)
	{
		if (!s_aQueue)
			s_aQueue = {};

		// Bounded. The queue drains at one report every SEND_INTERVAL_MS, so a
		// few players reporting at the cooldown floor fill it faster than it
		// empties - and an unbounded queue does not only grow, it delays the
		// reports already in it until they are useless. Past the cap the newest
		// is dropped, not the oldest: the first reports of an incident are the
		// ones a moderator wants.
		if (s_aQueue.Count() >= MAX_QUEUE)
		{
			// What the owner is told depends on whether there is a log file. The
			// line used to promise one unconditionally, and writeLog can be off.
			// Says what happened, and claims nothing about the log file. Whether a
			// line reached it is WriteToLog's business and it reports that itself.
			MrFrost_Log.Error("The Discord queue is full (" + MAX_QUEUE + " waiting) - this report was not queued.");
			return false;
		}

		s_aQueue.Insert(report);

		if (s_bSending)
		{
			// The timer is trusted, but not forever.
			//
			// s_bSending asserts that a repeating call exists and only SendNext
			// can clear it, so a call lost for any reason left the flag set, every
			// later report answered "queued", and nothing reached Discord again
			// for the life of the process while two hundred players were thanked.
			//
			// Re-arming on every enqueue was the wrong answer to that: Remove
			// followed by CallLater does not merely refresh the timer, it moves
			// the next tick to now + SEND_INTERVAL_MS. A steady trickle of reports
			// - two reporters during an incident average well under a second and a
			// half apart - pushed the tick forward every time and exactly one
			// report was ever sent. The cure was worse than the disease it was
			// written for.
			//
			// So the countdown is left alone on the ordinary path, and the stall
			// is detected instead: nothing has been posted for several intervals
			// while reports are waiting means the call really is gone.
			float now = GetGame().GetWorld().GetWorldTime();
			float silent = now - s_fLastSendAt;

			// A world clock that has gone backwards is a mission restart, which is
			// exactly the moment a timer is most likely to have been lost.
			if (silent >= 0 && silent < SEND_INTERVAL_MS * STALL_INTERVALS)
				return true;

			MrFrost_Log.Warn("The Discord sender stopped ticking with " + s_aQueue.Count() + " report(s) waiting - restarting it.");

			GetGame().GetCallqueue().Remove(SendNext);
			GetGame().GetCallqueue().CallLater(SendNext, SEND_INTERVAL_MS, true);
			SendNext();
			return true;
		}

		s_bSending = true;
		GetGame().GetCallqueue().CallLater(SendNext, SEND_INTERVAL_MS, true);
		SendNext();
		return true;
	}

	//------------------------------------------------------------------------------
	protected static void SendNext()
	{
		if (!s_aQueue || s_aQueue.IsEmpty())
		{
			GetGame().GetCallqueue().Remove(SendNext);
			s_bSending = false;
			return;
		}

		MrFrost_Report report = s_aQueue[0];
		// RemoveOrdered, not Remove: Remove() fills the hole with the last element,
		// so three reports inside one rate-limit window reached Discord as first,
		// third, second - out of step with reports.log, which is the one thing
		// the channel is meant to line up with.
		s_aQueue.RemoveOrdered(0);

		s_fLastSendAt = GetGame().GetWorld().GetWorldTime();

		Post(report);
	}

	//------------------------------------------------------------------------------
	//! Posts one report to the webhook.
	protected static void Post(notnull MrFrost_Report report)
	{
		MrFrost_ReportDeliveryConfig config = MrFrost_ReportConfigLoader.GetDelivery();
		if (!config || config.m_sWebhookUrl.IsEmpty())
			return;

		// The REST context is keyed by host; the path goes into the call itself.
		string host;
		string path;
		if (!SplitUrl(config.m_sWebhookUrl, host, path))
		{
			// Named, never printed. This is the file a server owner pastes into a
			// support channel when something is wrong.
			MrFrost_Log.Error("webhookUrl in report.json is not a URL - this report was not sent to Discord.");
			return;
		}

		RestContext context = GetGame().GetRestApi().GetContext(host);
		if (!context)
		{
			MrFrost_Log.Error("Could not get a REST context for the webhook.");
			return;
		}

		// Set immediately before every POST, and knowingly so. GetContext pools one
		// context per host and SetHeaders replaces the whole set on it - there is no
		// per-request form in the API. So any other mod on this server posting to
		// discord.com shares this object, and whatever it installed is gone after
		// this line. Re-setting ours each time makes us recover from theirs; nothing
		// here can make them recover from ours. Deliberately no SetTimeout, which
		// would clobber a second thing for no gain.
		context.SetHeaders("Content-Type,application/json");

		if (!s_Callback)
		{
			s_Callback = new RestCallback();
			s_Callback.SetOnSuccess(OnWebhookSuccess);
			s_Callback.SetOnError(OnWebhookError);
		}

		context.POST(s_Callback, path, BuildPayload(report, config));
	}

	//------------------------------------------------------------------------------
	//! Discord answers 204 with no body on success.
	protected static void OnWebhookSuccess(RestCallback callback)
	{
		MrFrost_Log.Debug("Report delivered to Discord.");
	}

	//------------------------------------------------------------------------------
	//! A failed webhook is not a lost report — the log file already has it. Said
	//! plainly in the log, because a wrong URL otherwise looks like silence.
	protected static void OnWebhookError(RestCallback callback)
	{
		if (callback.GetRestResult() == ERestResult.EREST_ERROR_TIMEOUT)
		{
			MrFrost_Log.Error("The Discord webhook timed out. The report is still in the log file.");
			return;
		}

		MrFrost_Log.Error("Discord rejected a report (HTTP " + callback.GetHttpCode()
			+ "). Check webhookUrl, webhookAvatarUrl and webhookUsername in report.json."
			+ " The report is still in the log file.");
	}

	//------------------------------------------------------------------------------
	//! Builds the Discord message as an embed.
	//!
	//! The description goes in the embed body rather than a field: Discord caps a
	//! field at 1024 characters and maxDescription is a server setting that can
	//! exceed that, while the body takes 4096. The facts follow as fields, one per
	//! row, so a long list of names never squeezes the column next to it.
	//!
	//! Colour separates the two kinds at a glance in a busy channel, and both the
	//! colours and every label here come from the server's own configuration.
	protected static string BuildPayload(notnull MrFrost_Report report, notnull MrFrost_ReportDeliveryConfig config)
	{
		string title = MrFrost_Text.Get("report.embed.bug");
		int colour = config.m_iColourBug;

		if (report.m_eKind == MrFrost_EReportKind.PLAYER)
		{
			title = MrFrost_Text.Get("report.embed.player");
			colour = config.m_iColourPlayer;
		}

		string fields = Field(MrFrost_Text.Get("report.embed.reporter"), report.m_sReporter);

		if (!report.m_sTargets.IsEmpty())
			fields += "," + Field(MrFrost_Text.Get("report.embed.against"), report.m_sTargets);

		if (!report.m_sPosition.IsEmpty())
			fields += "," + Field(MrFrost_Text.Get("report.embed.position"), report.m_sPosition);

		fields += "," + Field(MrFrost_Text.Get("report.embed.time"), report.m_sTime);

		string heading = Clamp(title, MAX_TITLE);
		string footer;
		if (!config.m_sServerName.IsEmpty())
			footer = Clamp(config.m_sServerName, MAX_FOOTER);

		// The description takes what the fixed parts left behind. Every slot is
		// capped on its own above, but four fields at their maximum plus a title
		// and a footer already exceed what Discord accepts in one embed - and it
		// answers that with a rejection of the whole message, so the report would
		// be lost rather than shortened. The description is the one part with
		// room to give; the log file keeps what a player wrote either way.
		int budget = MAX_EMBED - heading.Length() - footer.Length() - fields.Length();
		if (budget > MAX_DESCRIPTION)
			budget = MAX_DESCRIPTION;

		string description;
		if (budget > 0)
			description = Clamp(report.m_sDescription, budget);
		else
			MrFrost_Log.WarnOnce("embed.labels", "The embed labels on this server leave no room for a description. Shorten the report.embed.* overrides.");

		string embed = "{";
		embed += "\"title\":\"" + Escape(heading) + "\",";
		embed += "\"description\":\"" + Escape(description) + "\",";
		embed += "\"color\":" + colour.ToString() + ",";
		embed += "\"fields\":[" + fields + "]";

		if (!footer.IsEmpty())
			embed += ",\"footer\":{\"text\":\"" + Escape(footer) + "\"}";

		// Discord renders this in each reader's own timezone, next to the footer.
		// The "Time" field above stays the server's clock, because that is the one
		// that matches reports.log line for line.
		if (!report.m_sTimeUtc.IsEmpty())
			embed += ",\"timestamp\":\"" + report.m_sTimeUtc + "\"";

		embed += "}";

		string payload = "{\"embeds\":[" + embed + "]";

		// How the message signs itself. Discord falls back to the webhook's own
		// name and picture when these are absent, which is what a server that
		// says nothing gets.
		if (!config.m_sWebhookUsername.IsEmpty())
			payload += ",\"username\":\"" + Escape(Clamp(config.m_sWebhookUsername, MAX_USERNAME)) + "\"";

		// Discord rejects the whole message over a malformed avatar_url, which
		// would silently cost every report. A value that cannot be a URL is
		// dropped and named in the log instead.
		if (!config.m_sWebhookAvatarUrl.IsEmpty())
		{
			if (config.m_sWebhookAvatarUrl.Length() > MAX_AVATAR_URL)
				MrFrost_Log.WarnOnce("webhookAvatarUrl", "webhookAvatarUrl is too long to be a URL and was ignored.");
			else if (config.m_sWebhookAvatarUrl.StartsWith("http"))
				payload += ",\"avatar_url\":\"" + Escape(config.m_sWebhookAvatarUrl) + "\"";
			else
				MrFrost_Log.WarnOnce("webhookAvatarUrl", "webhookAvatarUrl is not a URL and was ignored: " + config.m_sWebhookAvatarUrl);
		}

		// allowed_mentions empty: a player typing @everyone into a description
		// must not be able to ping the whole Discord.
		return payload + ",\"allowed_mentions\":{\"parse\":[]}}";
	}

	//------------------------------------------------------------------------------
	//! One embed field, on a row of its own — "inline" is left off, which is
	//! Discord's default, so the fields stack instead of sharing a line.
	//!
	//! Discord rejects an empty value, so callers only pass fields they have.
	//! The name is escaped as well as the value. Both are server-supplied now
	//! that the labels come from the text table, and a quote in either one would
	//! break the JSON and cost the whole embed.
	protected static string Field(string name, string value)
	{
		// Discord rejects an embed whose field has no name, so a server that
		// overrode a label with an empty string would lose every report rather
		// than an ornament. A dash is the smallest thing that still renders.
		if (name.IsEmpty())
			name = "-";

		return "{\"name\":\"" + Escape(Clamp(name, MAX_FIELD_NAME)) + "\",\"value\":\"" + Escape(Clamp(value, MAX_FIELD)) + "\"}";
	}

	//------------------------------------------------------------------------------
	//! Keeps a value inside Discord's limit for the slot it goes into.
	//!
	//! Over the limit Discord rejects the whole embed, so a hundred names in the
	//! "Against" field would cost the entire report. Cut rather than lose it, and
	//! say so in the message: a moderator who sees the mark knows to open the log
	//! file, which never truncates anything.
	//!
	//! Counted before escaping — Discord counts the decoded text, not the JSON.
	//!
	//! The cut lands on a space wherever one is near, because Substring() counts
	//! bytes: cutting mid-character in a German or Japanese report would leave
	//! half a UTF-8 sequence, and Discord would reject the message as malformed.
	protected static string Clamp(string value, int limit)
	{
		if (value.Length() <= limit)
			return value;

		// Truncate reads a limit of zero or less as no limit at all, so the two
		// lines below would have handed back the whole value with three dots on
		// the end - longer than the input, and past the ceiling this exists to
		// hold. Only the description can reach a limit this small, and only when
		// the labels around it have taken almost everything.
		if (limit <= 0)
			return string.Empty;

		if (limit <= 3)
			return MrFrost_ServerContent.Truncate(value, limit);

		return MrFrost_ServerContent.Truncate(value, limit - 3) + "...";
	}

	//------------------------------------------------------------------------------
	//! Makes a player-supplied string safe to drop into a JSON body.
	//!
	//! Everything here comes from a text field a player typed into, so it is
	//! hostile input by default: an unescaped quote or backslash would break the
	//! JSON and lose the report.
	protected static string Escape(string value)
	{
		string result = value;

		result.Replace("\\", "\\\\");
		result.Replace("\"", "\\\"");
		result.Replace("\n", "\\n");
		result.Replace("\r", " ");
		result.Replace("\t", " ");

		// Everything below 0x20 that the lines above did not already turn into
		// something printable. JSON forbids a raw control character inside a
		// string, so one byte a player typed - or pasted - was enough for
		// Discord to reject the whole message, with an error naming the webhook
		// settings rather than the cause.
		return MrFrost_ServerContent.StripControls(result);
	}

	//------------------------------------------------------------------------------
	//! Splits "https://host/some/path" into "https://host/" and "some/path".
	protected static bool SplitUrl(string url, out string host, out string path)
	{
		int schemeEnd = url.IndexOf("://");
		if (schemeEnd < 0)
			return false;

		int hostStart = schemeEnd + 3;
		int slash = url.IndexOfFrom(hostStart, "/");

		if (slash < 0)
		{
			host = url;
			path = string.Empty;
			return true;
		}

		host = url.Substring(0, slash + 1);
		path = url.Substring(slash + 1, url.Length() - slash - 1);
		return true;
	}
}
