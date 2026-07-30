//------------------------------------------------------------------------------
//! Everything that decides whether a player may join a faction.
//!
//! Limit, whitelist and balance live together on purpose. A player turned away
//! needs to know which of them said no, because that decides what happens next:
//! a full faction frees up and is worth queueing for, a whitelist does not.
//!
//! Server side. Clients receive a stripped copy through the content transfer —
//! enough to draw the menu, without the whitelists.
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
//! Which factions the balance rule covers.
enum MrFrost_EBalanceMode
{
	//! Every playable faction.
	ALL,

	//! Only the listed keys.
	INCLUDE,

	//! Every playable faction except the listed keys.
	EXCLUDE,
}

//------------------------------------------------------------------------------
//! One faction's own settings.
[BaseContainerProps(), SCR_BaseContainerLocalizedTitleField("m_sKey")]
class MrFrost_FactionSettings
{
	[Attribute(desc: "Faction key as the mission defines it, e.g. US or USSR")]
	string m_sKey;

	[Attribute(defvalue: "-1", desc: "Players allowed on this faction. -1 is unlimited, 0 makes it unjoinable.")]
	int m_iPlayerLimit;

	[Attribute(desc: "Identity IDs allowed to join. An empty list leaves the faction open to everyone.")]
	ref array<string> m_aWhitelist;

	//------------------------------------------------------------------------------
	void MrFrost_FactionSettings()
	{
		m_iPlayerLimit = -1;
		m_aWhitelist = {};
	}

	//------------------------------------------------------------------------------
	//! A faction is gated once its list has a single name in it. An empty list is
	//! an open faction rather than a closed one — the alternative locks a faction
	//! out of the game the moment a server owner adds the key and nothing else.
	bool IsGated()
	{
		return m_aWhitelist && !m_aWhitelist.IsEmpty();
	}

	//------------------------------------------------------------------------------
	bool IsWhitelisted(string identityId)
	{
		if (!IsGated())
			return true;

		if (identityId.IsEmpty())
			return false;

		return m_aWhitelist.Contains(identityId);
	}
}

//------------------------------------------------------------------------------
//! Root of the faction settings.
[BaseContainerProps(configRoot: true)]
class MrFrost_FactionConfig
{
	[Attribute(defvalue: "1", desc: "Run the addon's faction rules. Off leaves faction selection entirely to the game.")]
	bool m_bEnabled;

	[Attribute(defvalue: "3", desc: "How many players one balanced faction may lead by. Anything below 1 behaves as 1.")]
	int m_iMaxImbalance;

	[Attribute(defvalue: "0", uiwidget: UIWidgets.ComboBox, enums: ParamEnumArray.FromEnum(MrFrost_EBalanceMode), desc: "Which factions the balance rule covers")]
	MrFrost_EBalanceMode m_eBalanceMode;

	[Attribute(desc: "Faction keys for the INCLUDE and EXCLUDE modes")]
	ref array<string> m_aBalancedFactions;

	[Attribute(defvalue: "10", desc: "Places per faction queue. 0 switches queueing off and refuses outright.")]
	int m_iQueueLimit;

	[Attribute(defvalue: "1", desc: "A player rejoining a faction they already held this session goes straight in, past balance and queue.")]
	bool m_bReturningPlayersSkipQueue;

	[Attribute(defvalue: "0", desc: "Server administrators go straight in. The player limit still applies to them.")]
	bool m_bAdminsSkipQueue;

	[Attribute(desc: "Identity IDs treated as administrators on top of anyone the engine already grants the role to. For rights that live outside the engine.")]
	ref array<string> m_aAdmins;

	[Attribute(desc: "Per-faction limits and whitelists")]
	ref array<ref MrFrost_FactionSettings> m_aFactions;

	//------------------------------------------------------------------------------
	void MrFrost_FactionConfig()
	{
		m_bEnabled = true;
		m_iMaxImbalance = 3;
		m_eBalanceMode = MrFrost_EBalanceMode.ALL;
		m_aBalancedFactions = {};
		m_iQueueLimit = 10;
		m_bReturningPlayersSkipQueue = true;
		m_bAdminsSkipQueue = false;
		m_aAdmins = {};
		m_aFactions = {};
	}

	//------------------------------------------------------------------------------
	//! Settings for a faction key, or null where the server named none.
	MrFrost_FactionSettings Find(string key)
	{
		if (!m_aFactions || key.IsEmpty())
			return null;

		foreach (MrFrost_FactionSettings settings : m_aFactions)
		{
			if (settings && settings.m_sKey == key)
				return settings;
		}

		return null;
	}

	//------------------------------------------------------------------------------
	//! Whether the balance rule applies to this faction.
	bool IsBalanced(string key)
	{
		if (m_eBalanceMode == MrFrost_EBalanceMode.ALL)
			return true;

		bool listed = m_aBalancedFactions && m_aBalancedFactions.Contains(key);

		if (m_eBalanceMode == MrFrost_EBalanceMode.INCLUDE)
			return listed;

		return !listed;
	}

	//------------------------------------------------------------------------------
	//! Never below 1: a joiner at equal counts always creates a difference of one,
	//! so 0 and 1 would mean the same thing and 0 reads like "no imbalance at all".
	int GetMaxImbalance()
	{
		if (m_iMaxImbalance < 1)
			return 1;

		return m_iMaxImbalance;
	}
}

//------------------------------------------------------------------------------
//! Picks the config the same way the other features do: the server's version
//! when it has one, the addon's otherwise.
class MrFrost_FactionConfigLoader
{
	static const ResourceName CONFIG_PATH = "{7FA1C3D2E4B50630}Configs/MrFrost/Factions.conf";

	protected static ref MrFrost_FactionConfig s_Config;
	protected static ref MrFrost_FactionConfig s_ServerConfig;

	//------------------------------------------------------------------------------
	static MrFrost_FactionConfig Get()
	{
		if (s_ServerConfig)
			return s_ServerConfig;

		if (!s_Config)
			s_Config = SCR_ConfigHelperT<MrFrost_FactionConfig>.GetConfigObject(CONFIG_PATH);

		if (!s_Config)
			s_Config = new MrFrost_FactionConfig();

		return s_Config;
	}

	//------------------------------------------------------------------------------
	static void SetServerConfig(MrFrost_FactionConfig config)
	{
		s_ServerConfig = config;
	}

	//------------------------------------------------------------------------------
	static bool IsEnabled()
	{
		MrFrost_FactionConfig config = Get();
		return config && config.m_bEnabled;
	}
}
