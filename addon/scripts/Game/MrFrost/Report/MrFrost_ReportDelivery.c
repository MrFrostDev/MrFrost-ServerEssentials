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

	//! Discord's own limits. Over either one it rejects the whole embed, so both
	//! are enforced here rather than trusted to a server owner's maxDescription.
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

	//! Reports waiting for their turn at the webhook.
	protected static ref array<ref MrFrost_Report> s_aQueue;
	protected static bool s_bSending;

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
			MrFrost_Log.Warn("logFile must be a file name, not a path. Using "
				+ DEFAULT_LOG_FILE + " instead of: " + name);
			return DEFAULT_LOG_FILE;
		}

		return name;
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

		line += " | " + description;

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
			return true;

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
			MrFrost_Log.Warn("The embed labels on this server leave no room for a description. Shorten the report.embed.* overrides.");

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
				MrFrost_Log.Warn("webhookAvatarUrl is too long to be a URL and was ignored.");
			else if (config.m_sWebhookAvatarUrl.StartsWith("http"))
				payload += ",\"avatar_url\":\"" + Escape(config.m_sWebhookAvatarUrl) + "\"";
			else
				MrFrost_Log.Warn("webhookAvatarUrl is not a URL and was ignored: " + config.m_sWebhookAvatarUrl);
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
	//! Removes bytes below 0x20 and leaves everything else alone.
	//!
	//! Cut rather than rebuilt: slicing around the offending byte keeps every
	//! multi-byte character intact, because a control byte can never be part
	//! of a UTF-8 sequence. In the ordinary case there is nothing to remove
	//! and the original is handed straight back.
	protected static string StripControls(string value)
	{
		int len = value.Length();

		int start = 0;
		string result;
		bool found = false;

		for (int i = 0; i < len; i++)
		{
			int c = value.ToAscii(i);

			// Negative means a byte above 0x7F arrived sign-extended - part of a
			// character, never a control.
			if (c < 0 || c >= 32)
				continue;

			found = true;

			if (i > start)
				result += value.Substring(start, i - start);

			start = i + 1;
		}

		if (!found)
			return value;

		if (start < len)
			result += value.Substring(start, len - start);

		return result;
	}

	//------------------------------------------------------------------------------
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
		return StripControls(result);
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
