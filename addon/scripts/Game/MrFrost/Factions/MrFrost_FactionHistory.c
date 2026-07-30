//------------------------------------------------------------------------------
//! Which factions a player has held this session.
//!
//! Authority only. Losing a connection is not a reason to lose your side, so a
//! player who comes back may rejoin a faction they already had, past balance and
//! past the queue. The player limit still applies — a bypass decides who waits,
//! not how many fit.
//!
//! Keyed on the identity rather than the player id: a reconnecting player gets a
//! new id, and the whole point is to recognise them across that.
//------------------------------------------------------------------------------
class MrFrost_FactionHistory
{
	//! Identity to the faction keys that identity has held.
	protected static ref map<string, ref array<string>> s_mHistory;

	//------------------------------------------------------------------------------
	//! Records that a player is now on a faction.
	static void Record(int playerId, Faction faction)
	{
		SCR_Faction scripted = SCR_Faction.Cast(faction);
		if (!scripted)
			return;

		string identity = MrFrost_FactionRules.GetIdentity(playerId);
		if (identity.IsEmpty())
			return;

		string key = scripted.GetFactionKey();
		if (key.IsEmpty())
			return;

		if (!s_mHistory)
			s_mHistory = new map<string, ref array<string>>();

		array<string> held;
		if (!s_mHistory.Find(identity, held))
		{
			held = {};
			s_mHistory.Set(identity, held);
		}

		if (!held.Contains(key))
			held.Insert(key);
	}

	//------------------------------------------------------------------------------
	//! Whether this player has held this faction at some point this session.
	static bool WasOn(int playerId, notnull Faction faction)
	{
		if (!s_mHistory)
			return false;

		SCR_Faction scripted = SCR_Faction.Cast(faction);
		if (!scripted)
			return false;

		string identity = MrFrost_FactionRules.GetIdentity(playerId);
		if (identity.IsEmpty())
			return false;

		array<string> held;
		if (!s_mHistory.Find(identity, held))
			return false;

		return held.Contains(scripted.GetFactionKey());
	}

	//------------------------------------------------------------------------------
	//! Drops everything. The counts a bypass was measured against reset with the
	//! mission, and so does the claim to a side from before it.
	static void Reset()
	{
		s_mHistory = null;
	}
}
