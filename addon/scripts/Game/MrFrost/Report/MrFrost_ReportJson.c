//------------------------------------------------------------------------------
//! MrFrost/report.json — the server's own report settings.
//!
//! Split in two on purpose. Everything under "delivery" is a secret and stays on
//! the server; the rest describes the menu and is sent to clients. The webhook
//! URL is the reason: whoever holds it can post into that channel, so it must
//! never travel to a machine the server does not control.
//------------------------------------------------------------------------------
class MrFrost_ReportDeliveryJson : JsonApiStruct
{
	bool writeLog;
	string logFile;
	string webhookUrl;
	string serverName;
	string webhookUsername;
	string webhookAvatarUrl;
	string colorPlayer;
	string colorBug;

	//------------------------------------------------------------------------------
	void MrFrost_ReportDeliveryJson()
	{
		writeLog = true;
		logFile = "reports.log";

		RegV("writeLog");
		RegV("logFile");
		RegV("webhookUrl");
		RegV("serverName");
		RegV("webhookUsername");
		RegV("webhookAvatarUrl");
		RegV("colorPlayer");
		RegV("colorBug");
	}
}

//------------------------------------------------------------------------------
//! Root of report.json.
class MrFrost_ReportJson : JsonApiStruct
{
	bool enabled;
	bool allowBugReports;
	bool allowPlayerReports;
	float nearbyRadius;
	int cooldownSeconds;
	bool revealNobodyNearby;
	int maxDescription;
	bool verboseLogging;
	string menuIcon;
	string menuIconImageset;
	ref MrFrost_ReportDeliveryJson delivery;
	ref array<ref MrFrost_JsonString> strings;

	//------------------------------------------------------------------------------
	void MrFrost_ReportJson()
	{
		enabled = true;
		allowBugReports = true;
		allowPlayerReports = true;
		nearbyRadius = 300;
		cooldownSeconds = 10;
		revealNobodyNearby = false;
		maxDescription = 1000;
		delivery = new MrFrost_ReportDeliveryJson();
		strings = {};

		RegV("enabled");
		RegV("allowBugReports");
		RegV("allowPlayerReports");
		RegV("nearbyRadius");
		RegV("cooldownSeconds");
		RegV("revealNobodyNearby");
		RegV("maxDescription");
		RegV("verboseLogging");
		RegV("menuIcon");
		RegV("menuIconImageset");
		RegV("delivery");
		RegV("strings");
	}

	//------------------------------------------------------------------------------
	MrFrost_ReportConfig ToConfig()
	{
		MrFrost_ReportConfig config = new MrFrost_ReportConfig();

		config.m_bEnabled            = enabled;
		config.m_bAllowBugReports    = allowBugReports;
		config.m_bAllowPlayerReports = allowPlayerReports;
		config.m_iCooldownSeconds    = cooldownSeconds;
		config.m_bRevealNobodyNearby = revealNobodyNearby;
		config.m_iMaxDescription     = maxDescription;
		config.m_sMenuIconName       = menuIcon;

		// A radius of zero would silently disable the nearby option rather than
		// report anything, so an unset key keeps the default.
		if (nearbyRadius > 0)
			config.m_fNearbyRadius = nearbyRadius;

		if (menuIconImageset.IsEmpty())
			config.m_MenuIconImageset = MrFrost_InfoMenuConfig.DEFAULT_IMAGESET;
		else
			config.m_MenuIconImageset = menuIconImageset;

		if (config.m_sMenuIconName.IsEmpty())
			config.m_sMenuIconName = "feedback";

		return config;
	}

	//------------------------------------------------------------------------------
	//! Server side only.
	MrFrost_ReportDeliveryConfig ToDeliveryConfig()
	{
		MrFrost_ReportDeliveryConfig result = new MrFrost_ReportDeliveryConfig();

		if (!delivery)
			return result;

		result.m_bWriteLog          = delivery.writeLog;
		result.m_sWebhookUrl        = delivery.webhookUrl;
		result.m_sServerName        = delivery.serverName;
		result.m_sWebhookUsername   = delivery.webhookUsername;
		result.m_sWebhookAvatarUrl  = delivery.webhookAvatarUrl;

		if (!delivery.logFile.IsEmpty())
			result.m_sLogFile = delivery.logFile;

		// Left at the built-in colour when the server did not name one, rather
		// than falling to black, which reads as a broken embed.
		int colour = ParseRgb(delivery.colorPlayer, "colorPlayer");
		if (colour >= 0)
			result.m_iColourPlayer = colour;

		colour = ParseRgb(delivery.colorBug, "colorBug");
		if (colour >= 0)
			result.m_iColourBug = colour;

		return result;
	}

	//------------------------------------------------------------------------------
	//! Turns "249,67,67" into the single integer Discord wants.
	//!
	//! Same notation as accentColor in infomenu.json - ordinary sRGB, the numbers
	//! a colour picker shows - so a server owner learns one format. Returns -1
	//! when there is nothing usable, which the caller reads as "keep the default".
	protected int ParseRgb(string value, string keyName)
	{
		if (value.IsEmpty())
			return -1;

		array<string> parts = {};
		value.Split(",", parts, true);

		if (parts.Count() < 3)
		{
			MrFrost_Log.Warn(keyName + " '" + value + "' is not 'r,g,b' - ignoring it.");
			return -1;
		}

		int r = Math.ClampInt(parts[0].Trim().ToInt(), 0, 255);
		int g = Math.ClampInt(parts[1].Trim().ToInt(), 0, 255);
		int b = Math.ClampInt(parts[2].Trim().ToInt(), 0, 255);

		return (r * 65536) + (g * 256) + b;
	}
}

//------------------------------------------------------------------------------
//! Ties report.json into the shared server-content transfer.
class MrFrost_ReportChannel : MrFrost_ServerContentChannel
{
	//------------------------------------------------------------------------------
	override string GetId()
	{
		return "report";
	}

	//------------------------------------------------------------------------------
	override string GetFileName()
	{
		return "report.json";
	}

	//------------------------------------------------------------------------------
	//! Runs on the client, and on the server when it hosts its own game.
	override bool Apply(string json)
	{
		MrFrost_ReportJson parsed = new MrFrost_ReportJson();
		parsed.ExpandFromRAW(json);

		MrFrost_Log.SetVerbose(parsed.verboseLogging);
		MrFrost_ReportConfigLoader.SetServerConfig(parsed.ToConfig());
		ApplyStrings(parsed);

		MrFrost_Log.Info("Using this server's report settings.");
		return true;
	}

	//------------------------------------------------------------------------------
	//! Re-emits the file without the delivery block.
	//!
	//! The transfer sends text, so reading delivery on the server is not the same
	//! as keeping it there — the bytes still travelled. This parses the file and
	//! packs it back out with delivery dropped, so what reaches a client cannot
	//! contain the webhook URL whatever a server owner put in the file.
	//!
	//! A parse failure returns nothing rather than the original: sending a file
	//! this code could not read would mean sending a secret it could not find.
	override string ForClient(string json)
	{
		MrFrost_ReportJson parsed = new MrFrost_ReportJson();
		parsed.ExpandFromRAW(json);

		parsed.delivery = null;
		parsed.UnregV("delivery");
		parsed.Pack();

		// "{}" as well as "": AsString() answers an empty object when it has no
		// data, which is not a file worth sending and not what the check below
		// was written to catch.
		string safe = parsed.AsString();
		safe.TrimInPlace();

		if (safe.Length() <= 2)
		{
			MrFrost_Log.Error("Could not repack report.json without its delivery block - sending nothing rather than risking the webhook URL.");
			return string.Empty;
		}

		// Checked rather than assumed. This function's whole job is that one
		// field never leaves the machine, and it rests on how the JSON layer
		// treats an unregistered member - which is not something the addon can
		// prove at build time. If the block survived the round trip, nothing
		// goes out and the server says why.
		// Matched as keys, not as words. An earlier version tested the whole
		// document for the bare strings, so a server whose own wording contained
		// "delivery" sent its players nothing at all.
		//
		// Whitespace removed first. Today this text comes from the serializer,
		// whose spacing is fixed, so the removal changes nothing - it is here
		// because the guard's whole premise is that the serializer might not have
		// done what it was asked. If what comes back is ever the original file
		// instead of a repack, it carries a server owner's own formatting, and a
		// space before a colon would have walked the webhook URL straight past a
		// test that only knew one spelling.
		string probe = safe;
		probe.Replace(" ", "");
		probe.Replace("	", "");

		if (probe.Contains("\"delivery\":") || probe.Contains("\"webhookUrl\":"))
		{
			MrFrost_Log.Error("report.json still carried its delivery block after repacking - refusing to send it. Reports still work; the menu falls back to the bundled settings.");
			return string.Empty;
		}

		return safe;
	}

	//------------------------------------------------------------------------------
	//! Server side. Also the point at which the delivery settings are taken out —
	//! they are read here and never put anywhere a client could reach.
	override bool Validate(string json)
	{
		// ExpandFromRAW returns void, so a file that reads as nothing is
		// indistinguishable from a deliberate one full of defaults.
		//
		// The test is that the text is a JSON object at all, not that it carries
		// particular keys: every key this file reads is optional, so requiring
		// any of them would reject "{"allowPlayerReports": false}" - a perfectly
		// ordinary file - and rejection is not soft. It costs the server its
		// whole report configuration, webhook included, for the life of the
		// process.
		// Testing the braces at each end was not enough: the damage in a
		// hand-edited file is almost always in the middle, and ExpandFromRAW
		// answers a stray comma with a struct full of defaults rather than an
		// error. A server that had switched reporting off had it switched back on
		// and was told nothing.
		if (!MrFrost_ServerContent.IsJsonObject(json))
		{
			MrFrost_Log.Error("report.json is not sound JSON - falling back to the bundled settings. Check it for a stray or missing comma.");
			return false;
		}

		MrFrost_ReportJson parsed = new MrFrost_ReportJson();
		parsed.ExpandFromRAW(json);

		MrFrost_Log.SetVerbose(parsed.verboseLogging);
		MrFrost_ReportConfigLoader.SetDelivery(parsed.ToDeliveryConfig());

		// Also the menu settings, not only delivery. Validate() is the only entry
		// point on a dedicated server - Apply() runs on clients and on a listen
		// host - so without this the authority enforced the addon's bundled
		// defaults while every client honoured the server's file. A server that
		// switched reporting off still accepted reports from a modified client.
		MrFrost_ReportConfigLoader.SetServerConfig(parsed.ToConfig());

		// Also applied here, not only in Apply(): the Discord embed is built on
		// the server, so its labels are resolved on the server. Without this a
		// server could rename them for its players and still get English in its
		// own moderation channel.
		ApplyStrings(parsed);

		MrFrost_ReportDeliveryConfig delivery = parsed.ToDeliveryConfig();
		if (delivery.m_sWebhookUrl.IsEmpty())
			MrFrost_Log.Info("No Discord webhook configured - reports are written to the log only.");
		else
			MrFrost_Log.Info("Discord webhook configured for reports.");

		return true;
	}

	//------------------------------------------------------------------------------
	protected void ApplyStrings(notnull MrFrost_ReportJson parsed)
	{
		if (!parsed.strings || parsed.strings.IsEmpty())
			return;

		map<string, string> overrides = new map<string, string>();

		foreach (MrFrost_JsonString entry : parsed.strings)
		{
			if (entry && !entry.key.IsEmpty())
				overrides.Set(entry.key, entry.text);
		}

		MrFrost_Text.SetOverrides(overrides);
	}
}
