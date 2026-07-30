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
	//! Server side. Also the point at which the delivery settings are taken out —
	//! they are read here and never put anywhere a client could reach.
	override bool Validate(string json)
	{
		MrFrost_ReportJson parsed = new MrFrost_ReportJson();
		parsed.ExpandFromRAW(json);

		MrFrost_Log.SetVerbose(parsed.verboseLogging);
		MrFrost_ReportConfigLoader.SetDelivery(parsed.ToDeliveryConfig());

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
