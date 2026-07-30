//------------------------------------------------------------------------------
//! factions.json: the faction rules as a server writes them.
//!
//! Split from the config object the rules read, the same way the other features
//! are split. The JSON is whatever a server owner typed; the config object is
//! what the addon trusts.
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
//! One faction's entry in the file.
class MrFrost_FactionJsonEntry : JsonApiStruct
{
	string key;
	int playerLimit;
	ref array<string> whitelist;

	//------------------------------------------------------------------------------
	void MrFrost_FactionJsonEntry()
	{
		playerLimit = -1;
		whitelist = {};

		RegV("key");
		RegV("playerLimit");
		RegV("whitelist");
	}
}

//------------------------------------------------------------------------------
//! Root of factions.json.
class MrFrost_FactionJson : JsonApiStruct
{
	bool enabled;
	int maxImbalance;
	string balanceMode;
	ref array<string> balancedFactions;
	int queueLimit;
	bool returningPlayersSkipQueue;
	bool adminsSkipQueue;
	ref array<ref MrFrost_FactionJsonEntry> factions;

	//------------------------------------------------------------------------------
	void MrFrost_FactionJson()
	{
		enabled = true;
		maxImbalance = 3;
		balanceMode = "ALL";
		balancedFactions = {};
		queueLimit = 10;
		returningPlayersSkipQueue = true;
		adminsSkipQueue = false;
		factions = {};

		RegV("enabled");
		RegV("maxImbalance");
		RegV("balanceMode");
		RegV("balancedFactions");
		RegV("queueLimit");
		RegV("returningPlayersSkipQueue");
		RegV("adminsSkipQueue");
		RegV("factions");
	}

	//------------------------------------------------------------------------------
	MrFrost_FactionConfig ToConfig()
	{
		MrFrost_FactionConfig config = new MrFrost_FactionConfig();

		config.m_bEnabled                     = enabled;
		config.m_iMaxImbalance                = maxImbalance;
		config.m_eBalanceMode                 = ParseMode(balanceMode);
		config.m_iQueueLimit                  = queueLimit;
		config.m_bReturningPlayersSkipQueue   = returningPlayersSkipQueue;
		config.m_bAdminsSkipQueue             = adminsSkipQueue;
		config.m_aBalancedFactions            = {};
		config.m_aFactions                    = {};

		foreach (string key : balancedFactions)
		{
			if (!key.IsEmpty())
				config.m_aBalancedFactions.Insert(key);
		}

		foreach (MrFrost_FactionJsonEntry entry : factions)
		{
			if (!entry || entry.key.IsEmpty())
				continue;

			MrFrost_FactionSettings settings = new MrFrost_FactionSettings();
			settings.m_sKey         = entry.key;
			settings.m_iPlayerLimit = entry.playerLimit;
			settings.m_aWhitelist   = {};

			foreach (string identity : entry.whitelist)
			{
				if (!identity.IsEmpty())
					settings.m_aWhitelist.Insert(identity);
			}

			config.m_aFactions.Insert(settings);
		}

		return config;
	}

	//------------------------------------------------------------------------------
	//! Anything unrecognised falls back to ALL rather than switching balancing
	//! off: a typo in the mode should not quietly stop the rule a server owner
	//! wrote the file to get.
	protected MrFrost_EBalanceMode ParseMode(string value)
	{
		string mode = value;
		mode.ToUpper();

		if (mode == "INCLUDE")
			return MrFrost_EBalanceMode.INCLUDE;

		if (mode == "EXCLUDE")
			return MrFrost_EBalanceMode.EXCLUDE;

		if (mode != "ALL" && !mode.IsEmpty())
			MrFrost_Log.Warn("balanceMode '" + value + "' is not ALL, INCLUDE or EXCLUDE - using ALL.");

		return MrFrost_EBalanceMode.ALL;
	}
}

//------------------------------------------------------------------------------
//! Ties factions.json into the shared server-content transfer.
class MrFrost_FactionChannel : MrFrost_ServerContentChannel
{
	//------------------------------------------------------------------------------
	override string GetId()
	{
		return "factions";
	}

	//------------------------------------------------------------------------------
	override string GetFileName()
	{
		return "factions.json";
	}

	//------------------------------------------------------------------------------
	//! Runs on the client, and on the server when it hosts its own game.
	//!
	//! Whitelists are dropped on the way: a client needs to know a faction is
	//! gated so the menu can say so, and has no business holding the list of who
	//! is on it.
	override bool Apply(string json)
	{
		MrFrost_FactionJson parsed = new MrFrost_FactionJson();
		parsed.ExpandFromRAW(json);

		MrFrost_FactionConfig config = parsed.ToConfig();

		if (!Replication.IsServer())
		{
			foreach (MrFrost_FactionSettings settings : config.m_aFactions)
			{
				if (settings && settings.m_aWhitelist)
					settings.m_aWhitelist.Clear();
			}
		}

		MrFrost_FactionConfigLoader.SetServerConfig(config);

		MrFrost_Log.Info("Using this server's faction rules.");
		return true;
	}

	//------------------------------------------------------------------------------
	//! Server side. The authority keeps the whole file, whitelists included.
	override bool Validate(string json)
	{
		MrFrost_FactionJson parsed = new MrFrost_FactionJson();
		parsed.ExpandFromRAW(json);

		MrFrost_FactionConfig config = parsed.ToConfig();
		MrFrost_FactionConfigLoader.SetServerConfig(config);

		if (!config.m_bEnabled)
		{
			MrFrost_Log.Info("Faction rules are switched off on this server.");
			return true;
		}

		MrFrost_Log.Info("Faction rules active: max imbalance " + config.GetMaxImbalance()
			+ ", queue limit " + config.m_iQueueLimit
			+ ", " + config.m_aFactions.Count() + " faction(s) configured.");

		return true;
	}
}
