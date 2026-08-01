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

	//! Set by the server when it strips the whitelist, so a client can still be
	//! told the faction is gated without being told who is on the list.
	bool gated;

	//------------------------------------------------------------------------------
	void MrFrost_FactionJsonEntry()
	{
		playerLimit = -1;
		whitelist = {};

		RegV("key");
		RegV("playerLimit");
		RegV("whitelist");
		RegV("gated");
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
	ref array<string> admins;
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
		admins = {};
		factions = {};

		RegV("enabled");
		RegV("maxImbalance");
		RegV("balanceMode");
		RegV("balancedFactions");
		RegV("queueLimit");
		RegV("returningPlayersSkipQueue");
		RegV("adminsSkipQueue");
		RegV("admins");
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
		config.m_aAdmins                      = {};
		config.m_aFactions                    = {};

		// Null, not empty, is what a missing or malformed key parses into: the
		// constructor's empty array does not survive ExpandFromRAW, and a file
		// writing "admins": null hands one back.
		if (balancedFactions)
		{
			foreach (string key : balancedFactions)
			{
				if (!key.IsEmpty())
					config.m_aBalancedFactions.Insert(key);
			}
		}

		if (admins)
		{
			foreach (string admin : admins)
			{
				if (!admin.IsEmpty())
					config.m_aAdmins.Insert(admin);
			}
		}

		if (!factions)
			return config;

		foreach (MrFrost_FactionJsonEntry entry : factions)
		{
			if (!entry || entry.key.IsEmpty())
				continue;

			MrFrost_FactionSettings settings = new MrFrost_FactionSettings();
			settings.m_sKey         = entry.key;
			settings.m_iPlayerLimit = entry.playerLimit;
			settings.m_aWhitelist   = {};

			if (entry.whitelist)
			{
				foreach (string identity : entry.whitelist)
				{
					if (!identity.IsEmpty())
						settings.m_aWhitelist.Insert(identity);
				}
			}

			// A client is never handed the list itself, so the flag is what tells the
			// menu the faction is gated. The server derives it from the list it has;
			// a client reads the one the server sent in the list's place.
			settings.m_bGated = entry.gated || !settings.m_aWhitelist.IsEmpty();

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
	override bool Apply(string json)
	{
		// Checked here as well as on the server. A payload the client cannot read
		// comes back as a struct full of constructor defaults, not as an error.
		if (!MrFrost_ServerContent.IsJsonObject(json))
			return false;

		MrFrost_FactionJson parsed = new MrFrost_FactionJson();
		parsed.ExpandFromRAW(json);

		MrFrost_FactionConfigLoader.SetServerConfig(parsed.ToConfig());

		MrFrost_Log.Info("Using this server's faction rules.");
		return true;
	}

	//------------------------------------------------------------------------------
	//! Re-emits the file without the whitelists or the admin list.
	//!
	//! Stripping them in Apply() was too late by a whole network hop: the bytes
	//! had already been sent, and a modified client reads them off the wire
	//! whatever this code does with them afterwards. These are account
	//! identifiers of a server's admins and its whitelisted players - the second
	//! secret this file has to keep, after the webhook.
	//!
	//! A parse failure sends nothing rather than the original, for the same
	//! reason the report channel does: a file this code could not read is a file
	//! whose secrets it could not find.
	override string ForClient(string json)
	{
		MrFrost_FactionJson parsed = new MrFrost_FactionJson();
		parsed.ExpandFromRAW(json);

		// The flag survives, the list does not - a menu still has to be able to say
		// that a faction is gated.
		if (parsed.factions)
		{
			foreach (MrFrost_FactionJsonEntry entry : parsed.factions)
			{
				if (!entry)
					continue;

				entry.gated = entry.whitelist && !entry.whitelist.IsEmpty();
				entry.whitelist = null;
				entry.UnregV("whitelist");
			}
		}

		parsed.admins = null;
		parsed.UnregV("admins");
		parsed.Pack();

		string safe = parsed.AsString();
		safe.TrimInPlace();

		if (safe.Length() <= 2)
		{
			MrFrost_Log.Error("Could not repack factions.json without its whitelists - sending nothing rather than risking them.");
			return string.Empty;
		}

		// Checked rather than assumed, the same way the report channel checks its
		// own repack. Whitespace removed first, so a hand-formatted file cannot
		// slip a key past on a space.
		string probe = safe;
		probe.Replace(" ", "");
		probe.Replace("\t", "");
		probe.Replace("\n", "");
		probe.Replace("\r", "");

		if (probe.Contains("\"whitelist\":") || probe.Contains("\"admins\":"))
		{
			MrFrost_Log.Error("factions.json still carried a whitelist after repacking - refusing to send it. The rules still apply on the server.");
			return string.Empty;
		}

		return safe;
	}

	//------------------------------------------------------------------------------
	//! Server side. The authority keeps the whole file, whitelists included.
	override bool Validate(string json)
	{
		// Testing the braces at each end is not enough - the damage in a
		// hand-edited file is almost always in the middle, and ExpandFromRAW
		// answers a stray comma with a struct full of defaults. Without this a
		// server that had switched the rules off had them switched back on, with
		// every cap at its built-in value and a success line on the console.
		if (!MrFrost_ServerContent.IsJsonObject(json))
		{
			MrFrost_Log.Error("factions.json is not sound JSON - falling back to the bundled rules. Check it for a stray or missing comma.");
			return false;
		}

		MrFrost_FactionJson parsed = new MrFrost_FactionJson();
		parsed.ExpandFromRAW(json);

		MrFrost_FactionConfig config = parsed.ToConfig();
		MrFrost_FactionConfigLoader.SetServerConfig(config);

		if (!config.m_bEnabled)
		{
			MrFrost_Log.Info("Faction rules are switched off on this server.");
			return true;
		}

		// Says "read", not "active". Nothing calls the rules or the queue yet, and
		// a line telling an owner their caps are enforced when nothing enforces
		// them is worse than no line at all.
		MrFrost_Log.Info("Faction rules read: max imbalance " + config.GetMaxImbalance()
			+ ", queue limit " + config.m_iQueueLimit
			+ ", " + config.m_aFactions.Count() + " faction(s) configured. Not enforced yet - the feature is not wired up.");

		return true;
	}
}
