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
			Enqueue(report);
			delivered = true;
		}

		if (!delivered)
			MrFrost_Log.Warn("A report was made but this server has neither a log file nor a webhook configured.");

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
	protected static void Enqueue(notnull MrFrost_Report report)
	{
		if (!s_aQueue)
			s_aQueue = {};

		s_aQueue.Insert(report);

		if (s_bSending)
			return;

		s_bSending = true;
		GetGame().GetCallqueue().CallLater(SendNext, SEND_INTERVAL_MS, true);
		SendNext();
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
		s_aQueue.Remove(0);

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
			MrFrost_Log.Error("The webhook URL is not a URL: " + config.m_sWebhookUrl);
			return;
		}

		RestContext context = GetGame().GetRestApi().GetContext(host);
		if (!context)
		{
			MrFrost_Log.Error("Could not get a REST context for the webhook.");
			return;
		}

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

		string embed = "{";
		embed += "\"title\":\"" + Escape(title) + "\",";
		embed += "\"description\":\"" + Escape(Clamp(report.m_sDescription, MAX_DESCRIPTION)) + "\",";
		embed += "\"color\":" + colour.ToString() + ",";
		embed += "\"fields\":[" + fields + "]";

		if (!config.m_sServerName.IsEmpty())
			embed += ",\"footer\":{\"text\":\"" + Escape(config.m_sServerName) + "\"}";

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
			payload += ",\"username\":\"" + Escape(config.m_sWebhookUsername) + "\"";

		// Discord rejects the whole message over a malformed avatar_url, which
		// would silently cost every report. A value that cannot be a URL is
		// dropped and named in the log instead.
		if (!config.m_sWebhookAvatarUrl.IsEmpty())
		{
			if (config.m_sWebhookAvatarUrl.StartsWith("http"))
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
		return "{\"name\":\"" + Escape(Clamp(name, MAX_FIELD)) + "\",\"value\":\"" + Escape(Clamp(value, MAX_FIELD)) + "\"}";
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

		return result;
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
