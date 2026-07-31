//------------------------------------------------------------------------------
//! What a player can report, and where the report goes.
//!
//! Everything a server owner sets lives in MrFrost/report.json. The addon config
//! is the fallback, exactly as for the info menu.
//!
//! The webhook URL is deliberately **not** part of anything a client ever sees.
//! It is a secret: anyone holding it can post to that channel. Only the fields
//! a player needs to draw the menu are sent to clients; delivery happens on the
//! server and the URL never leaves it.
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
//! What the player is reporting.
enum MrFrost_EReportKind
{
	BUG,
	PLAYER,
}

//------------------------------------------------------------------------------
//! How the reported player or players are picked.
enum MrFrost_EReportTarget
{
	NONE,		//!< Bug report - nobody is being accused.
	SELECTED,	//!< One player the reporter picked from the list.
	KILLER,		//!< Whoever killed the reporter last.
	ATTACKER,	//!< Whoever injured the reporter last.
	NEARBY,		//!< Everyone within the configured radius.
}

//------------------------------------------------------------------------------
//! Root of the report config.
[BaseContainerProps(configRoot: true)]
class MrFrost_ReportConfig
{
	[Attribute(defvalue: "1", desc: "Off hides the menu and its pause entry. The key stays bound and does nothing")]
	bool m_bEnabled;

	[Attribute(defvalue: "{3262679C50EF4F01}UI/Textures/Icons/icons_wrapperUI.imageset", uiwidget: UIWidgets.ResourceNamePicker, params: "imageset", desc: "Imageset for the menu icon")]
	ResourceName m_MenuIconImageset;

	[Attribute(defvalue: "feedback", desc: "Sprite name for the menu icon. Leave empty to hide it.")]
	string m_sMenuIconName;

	[Attribute(defvalue: "1", desc: "Offer 'Report a bug'")]
	bool m_bAllowBugReports;

	[Attribute(defvalue: "1", desc: "Offer 'Report a player'")]
	bool m_bAllowPlayerReports;

	[Attribute(defvalue: "300", desc: "Radius in metres for the 'everyone nearby' option")]
	float m_fNearbyRadius;

	[Attribute(defvalue: "10", desc: "Seconds a player has to wait between two reports")]
	int m_iCooldownSeconds;

	[Attribute(defvalue: "0", desc: "Tell the reporter when nobody was within the radius. Off keeps the report menu from doubling as a radar.")]
	bool m_bRevealNobodyNearby;

	//! Used when a config leaves the limit at zero or below, which would
	//! otherwise mean no limit rather than the small one it looks like.
	static const int DEFAULT_MAX_DESCRIPTION = 1000;

	//! Used when a config leaves the radius at zero or below, which would make
	//! the nearby option select nobody and print a bare 0 in the menu.
	static const float DEFAULT_NEARBY_RADIUS = 300;

	[Attribute(defvalue: "1000", desc: "Longest description a player can send, in bytes, up to 8191 - an umlaut counts two. Zero or less falls back to 1000")]
	int m_iMaxDescription;

	//------------------------------------------------------------------------------
	void MrFrost_ReportConfig()
	{
		m_bEnabled = true;
		m_bAllowBugReports = true;
		m_bAllowPlayerReports = true;
		m_fNearbyRadius = DEFAULT_NEARBY_RADIUS;
		m_iCooldownSeconds = 10;
		m_bRevealNobodyNearby = false;
		m_iMaxDescription = 1000;
	}
}

//------------------------------------------------------------------------------
//! Delivery settings. Server side only — never replicated, never in a menu.
class MrFrost_ReportDeliveryConfig
{
	bool m_bWriteLog;
	string m_sLogFile;
	string m_sWebhookUrl;
	string m_sServerName;

	//! How the webhook signs its messages. Empty leaves Discord's own default,
	//! which is whatever the channel's webhook was named when it was created.
	string m_sWebhookUsername;
	string m_sWebhookAvatarUrl;

	//! Embed stripe colours, as Discord wants them: one integer, 0xRRGGBB.
	int m_iColourPlayer;
	int m_iColourBug;

	//------------------------------------------------------------------------------
	void MrFrost_ReportDeliveryConfig()
	{
		m_bWriteLog = true;
		m_sLogFile = "reports.log";

		// Red for a player report, amber for a bug, so a moderator scanning the
		// channel sees which is which before reading a word.
		m_iColourPlayer = 0xF94343;
		m_iColourBug = 0xE2A74F;
	}
}

//------------------------------------------------------------------------------
//! Picks the config the same way the info menu does: the server's version when
//! it has one, the addon's otherwise.
class MrFrost_ReportConfigLoader
{
	static const ResourceName CONFIG_PATH = "{7FA1C3D2E4B50621}Configs/MrFrost/Report.conf";

	protected static ref MrFrost_ReportConfig s_Config;
	protected static ref MrFrost_ReportConfig s_ServerConfig;

	//! Server side only. Stays null on every client.
	protected static ref MrFrost_ReportDeliveryConfig s_Delivery;

	//------------------------------------------------------------------------------
	static MrFrost_ReportConfig Get()
	{
		if (s_ServerConfig)
			return s_ServerConfig;

		if (!s_Config)
			s_Config = SCR_ConfigHelperT<MrFrost_ReportConfig>.GetConfigObject(CONFIG_PATH);

		if (!s_Config)
			s_Config = new MrFrost_ReportConfig();

		return s_Config;
	}

	//------------------------------------------------------------------------------
	//! Delivery settings, or null on a client — which is the correct answer there.
	static MrFrost_ReportDeliveryConfig GetDelivery()
	{
		return s_Delivery;
	}

	//------------------------------------------------------------------------------
	static void SetServerConfig(MrFrost_ReportConfig config)
	{
		s_ServerConfig = config;
	}

	//------------------------------------------------------------------------------
	//! Client side: forget whatever the last server sent.
	static void ClearServerConfig()
	{
		s_ServerConfig = null;
	}

	//------------------------------------------------------------------------------
	static void SetDelivery(MrFrost_ReportDeliveryConfig delivery)
	{
		s_Delivery = delivery;
	}

	//------------------------------------------------------------------------------
	//! True when this server offers reporting at all.
	static bool IsEnabled()
	{
		MrFrost_ReportConfig config = Get();
		if (!config || !config.m_bEnabled)
			return false;

		return config.m_bAllowBugReports || config.m_bAllowPlayerReports;
	}
}
