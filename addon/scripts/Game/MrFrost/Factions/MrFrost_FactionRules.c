//------------------------------------------------------------------------------
//! The one rule that decides whether a player may join a faction.
//!
//! Applied to every request the same way, whether it is a player's first pick of
//! the session or a later change. One rule means no second code path to forget
//! when the first one changes.
//!
//! The answer carries a reason, because what happens next depends on it: a full
//! faction is worth queueing for, a whitelist is not.
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
//! Why a faction was refused, or that it was not.
enum MrFrost_EFactionVerdict
{
	//! The player may join.
	ALLOWED,

	//! No such faction, or the mission does not let players have it.
	NOT_PLAYABLE,

	//! At its player limit. Frees up on its own.
	FULL,

	//! Joining would push the factions further apart than allowed. Frees up on
	//! its own, and also when another faction gains a player.
	IMBALANCED,

	//! The faction has a whitelist and the player is not on it. Never frees up.
	NOT_WHITELISTED,
}

//------------------------------------------------------------------------------
class MrFrost_FactionRules
{
	//------------------------------------------------------------------------------
	//! Whether a refusal is worth waiting for.
	//!
	//! FULL and IMBALANCED change as players come and go. NOT_WHITELISTED and
	//! NOT_PLAYABLE do not, and offering a queue for them would leave a player
	//! waiting on something that cannot happen.
	static bool IsWorthQueueing(MrFrost_EFactionVerdict verdict)
	{
		return verdict == MrFrost_EFactionVerdict.FULL || verdict == MrFrost_EFactionVerdict.IMBALANCED;
	}

	//------------------------------------------------------------------------------
	//! Authority: may this player join this faction right now?
	//!
	//! \param playerId  Who is asking
	//! \param faction   What they asked for
	//! \param skipSoftRules  True for a player the config lets past balance — a
	//!        returning player or an administrator. The player limit still applies:
	//!        no bypass may put more people on a faction than it has room for.
	static MrFrost_EFactionVerdict Evaluate(int playerId, Faction faction, bool skipSoftRules = false)
	{
		SCR_Faction scripted = SCR_Faction.Cast(faction);
		if (!scripted || !scripted.IsPlayable())
			return MrFrost_EFactionVerdict.NOT_PLAYABLE;

		MrFrost_FactionConfig config = MrFrost_FactionConfigLoader.Get();
		if (!config || !config.m_bEnabled)
			return MrFrost_EFactionVerdict.ALLOWED;

		string key = scripted.GetFactionKey();
		MrFrost_FactionSettings settings = config.Find(key);

		// A limit of 0 is a faction the server closed rather than one that filled
		// up, so it reads as not playable and never offers a queue.
		if (settings && settings.m_iPlayerLimit == 0)
			return MrFrost_EFactionVerdict.NOT_PLAYABLE;

		// Checked before any bypass: a player taken off the list must not walk
		// back in through the returning-player door.
		if (settings && !settings.IsWhitelisted(GetIdentity(playerId)))
			return MrFrost_EFactionVerdict.NOT_WHITELISTED;

		if (settings && settings.m_iPlayerLimit > 0 && CountPlayers(scripted) >= settings.m_iPlayerLimit)
			return MrFrost_EFactionVerdict.FULL;

		if (skipSoftRules)
			return MrFrost_EFactionVerdict.ALLOWED;

		if (!config.IsBalanced(key))
			return MrFrost_EFactionVerdict.ALLOWED;

		if (!IsBalanceSatisfied(config, scripted, SCR_FactionManager.SGetPlayerFaction(playerId)))
			return MrFrost_EFactionVerdict.IMBALANCED;

		return MrFrost_EFactionVerdict.ALLOWED;
	}

	//------------------------------------------------------------------------------
	//! Would one more player on this faction leave another one too far behind?
	//!
	//! The player is counted onto the faction they asked for and off the one they
	//! are leaving. Without that subtraction, someone moving from the larger side
	//! to the smaller one is refused for creating the imbalance they are about to
	//! reduce.
	protected static bool IsBalanceSatisfied(notnull MrFrost_FactionConfig config, notnull SCR_Faction target, Faction currentFaction)
	{
		FactionManager factionManager = GetGame().GetFactionManager();
		if (!factionManager)
			return true;

		int projected = CountPlayers(target) + 1;
		int allowed = config.GetMaxImbalance();

		array<Faction> factions = {};
		factionManager.GetFactionsList(factions);

		foreach (Faction other : factions)
		{
			SCR_Faction compared = SCR_Faction.Cast(other);
			if (!compared || compared == target || !compared.IsPlayable())
				continue;

			if (!config.IsBalanced(compared.GetFactionKey()))
				continue;

			int count = CountPlayers(compared);
			if (currentFaction == compared)
				count--;

			if (projected - count > allowed)
				return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------
	//! Players currently on a faction.
	//!
	//! Read from the faction manager rather than counted here: it maintains the
	//! authoritative list and replicates it, so the menu and the authority agree
	//! without a second tally that could drift.
	static int CountPlayers(notnull SCR_Faction faction)
	{
		int count = faction.GetPlayerCount();
		if (count < 0)
			return 0;

		return count;
	}

	//------------------------------------------------------------------------------
	//! Stable identity of a player, the string a whitelist is written in. Empty
	//! where the platform has not told us yet, which reads as "not whitelisted"
	//! rather than as "everyone".
	static string GetIdentity(int playerId)
	{
		return SCR_PlayerIdentityUtils.GetPlayerIdentityId(playerId);
	}
}
