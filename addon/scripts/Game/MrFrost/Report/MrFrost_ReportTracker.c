//------------------------------------------------------------------------------
//! Remembers who killed and who injured whom, so a player can report them
//! without having to catch a name mid-firefight.
//!
//! All of it is server side. A client never says *who* it is reporting for the
//! killer, attacker or nearby options — it says *which option*, and the server
//! resolves it from what it saw. A client that lied could otherwise pin a report
//! on anyone it liked.
//------------------------------------------------------------------------------
modded class SCR_BaseGameMode
{
	//! Victim player id -> the player who killed them last.
	protected ref map<int, int> m_mMrFrostKillers;

	//! Victim player id -> the player who damaged them last.
	protected ref map<int, int> m_mMrFrostAttackers;

	//------------------------------------------------------------------------------
	override protected void OnPlayerKilled(int playerId, IEntity playerEntity, IEntity killerEntity, notnull Instigator killer)
	{
		super.OnPlayerKilled(playerId, playerEntity, killerEntity, killer);

		int killerId = killer.GetInstigatorPlayerID();

		// Suicides, bleeding out and anything the world did are not reportable
		// against a player, so they are simply not remembered.
		if (killerId <= 0 || killerId == playerId)
			return;

		if (!m_mMrFrostKillers)
			m_mMrFrostKillers = new map<int, int>();

		m_mMrFrostKillers.Set(playerId, killerId);
	}

	//------------------------------------------------------------------------------
	override protected void OnControllableSpawned(IEntity entity)
	{
		super.OnControllableSpawned(entity);

		// Only the server watches damage; a client seeing its own hits would add
		// nothing the server cannot see itself.
		if (!Replication.IsServer() || !entity)
			return;

		SCR_DamageManagerComponent damageManager = SCR_DamageManagerComponent.Cast(entity.FindComponent(SCR_DamageManagerComponent));
		if (!damageManager)
			return;

		damageManager.GetOnDamage().Insert(MrFrost_OnDamage);
	}

	//------------------------------------------------------------------------------
	protected void MrFrost_OnDamage(BaseDamageContext damageContext)
	{
		if (!damageContext || !damageContext.instigator || !damageContext.hitEntity)
			return;

		PlayerManager playerManager = GetGame().GetPlayerManager();
		if (!playerManager)
			return;

		int victimId = playerManager.GetPlayerIdFromControlledEntity(damageContext.hitEntity);
		if (victimId <= 0)
			return;

		int attackerId = damageContext.instigator.GetInstigatorPlayerID();
		if (attackerId <= 0 || attackerId == victimId)
			return;

		if (!m_mMrFrostAttackers)
			m_mMrFrostAttackers = new map<int, int>();

		m_mMrFrostAttackers.Set(victimId, attackerId);
	}

	//------------------------------------------------------------------------------
	//! Forgets a player the moment they leave.
	//!
	//! The server reuses player ids. Without this, "report whoever killed me"
	//! resolved through a stale id to whoever holds it now, and a moderator
	//! received an embed naming somebody who was never there. The cooldown map
	//! has the same hazard from the other side: a fresh joiner inherited the
	//! previous holder's cooldown and was refused their first report.
	override void OnPlayerDisconnected(int playerId, KickCauseCode cause, int timeout)
	{
		super.OnPlayerDisconnected(playerId, cause, timeout);

		if (m_mMrFrostKillers)
		{
			m_mMrFrostKillers.Remove(playerId);

			// Also everywhere they appear as the killer, not only as the victim.
			array<int> victims = {};
			foreach (int victim, int killer : m_mMrFrostKillers)
			{
				if (killer == playerId)
					victims.Insert(victim);
			}

			foreach (int victim : victims)
			{
				m_mMrFrostKillers.Remove(victim);
			}
		}

		if (m_mMrFrostAttackers)
		{
			m_mMrFrostAttackers.Remove(playerId);

			array<int> hurt = {};
			foreach (int victim, int attacker : m_mMrFrostAttackers)
			{
				if (attacker == playerId)
					hurt.Insert(victim);
			}

			foreach (int victim : hurt)
			{
				m_mMrFrostAttackers.Remove(victim);
			}
		}

		MrFrost_ReportSubmit.Forget(playerId);
	}

	//------------------------------------------------------------------------------
	//! Who killed this player last, or 0.
	int MrFrost_GetLastKiller(int playerId)
	{
		if (!m_mMrFrostKillers)
			return 0;

		int killerId;
		if (!m_mMrFrostKillers.Find(playerId, killerId))
			return 0;

		return killerId;
	}

	//------------------------------------------------------------------------------
	//! Who injured this player last, or 0.
	int MrFrost_GetLastAttacker(int playerId)
	{
		if (!m_mMrFrostAttackers)
			return 0;

		int attackerId;
		if (!m_mMrFrostAttackers.Find(playerId, attackerId))
			return 0;

		return attackerId;
	}
}

//------------------------------------------------------------------------------
//! Server-side helpers for turning a reporter's choice into actual players.
class MrFrost_ReportTargets
{
	//------------------------------------------------------------------------------
	//! Resolves what the reporter picked into a list of player ids.
	//!
	//! Empty means the option produced nobody — the reporter died to the world, or
	//! is standing alone in a field. The caller turns that into a message rather
	//! than sending a report accusing no one.
	static void Resolve(int reporterId, MrFrost_EReportTarget mode, int selectedId, notnull out array<int> targets)
	{
		SCR_BaseGameMode gameMode = SCR_BaseGameMode.Cast(GetGame().GetGameMode());

		switch (mode)
		{
			case MrFrost_EReportTarget.SELECTED:
			{
				// Still checked against the live player list: an id from a menu the
				// player opened a minute ago may have left in the meantime.
				if (selectedId > 0 && selectedId != reporterId && IsConnected(selectedId))
					targets.Insert(selectedId);

				break;
			}

			case MrFrost_EReportTarget.KILLER:
			{
				if (!gameMode)
					break;

				// Checked against the live list for the same reason SELECTED is:
				// an id belongs to whoever holds it now, not to who held it then.
				int killerId = gameMode.MrFrost_GetLastKiller(reporterId);
				if (killerId > 0 && IsConnected(killerId))
					targets.Insert(killerId);

				break;
			}

			case MrFrost_EReportTarget.ATTACKER:
			{
				if (!gameMode)
					break;

				int attackerId = gameMode.MrFrost_GetLastAttacker(reporterId);
				if (attackerId > 0 && IsConnected(attackerId))
					targets.Insert(attackerId);

				break;
			}

			case MrFrost_EReportTarget.NEARBY:
			{
				CollectNearby(reporterId, targets);
				break;
			}
		}
	}

	//------------------------------------------------------------------------------
	protected static void CollectNearby(int reporterId, notnull out array<int> targets)
	{
		PlayerManager playerManager = GetGame().GetPlayerManager();
		if (!playerManager)
			return;

		IEntity reporter = playerManager.GetPlayerControlledEntity(reporterId);
		if (!reporter)
			return;

		MrFrost_ReportConfig config = MrFrost_ReportConfigLoader.Get();
		float radius = config.m_fNearbyRadius;
		float radiusSq = radius * radius;

		vector origin = reporter.GetOrigin();

		array<int> players = {};
		playerManager.GetPlayers(players);

		foreach (int playerId : players)
		{
			if (playerId == reporterId)
				continue;

			IEntity entity = playerManager.GetPlayerControlledEntity(playerId);
			if (!entity)
				continue;

			// Squared distance so the whole sweep stays multiplications; the
			// radius is a setting, not a measurement, and never needs the root.
			if (vector.DistanceSq(origin, entity.GetOrigin()) <= radiusSq)
				targets.Insert(playerId);
		}
	}

	//------------------------------------------------------------------------------
	protected static bool IsConnected(int playerId)
	{
		PlayerManager playerManager = GetGame().GetPlayerManager();
		if (!playerManager)
			return false;

		return playerManager.IsPlayerConnected(playerId);
	}

	//------------------------------------------------------------------------------
	//! "Name (id)" for a player, or a placeholder when they have since left.
	static string Describe(int playerId)
	{
		PlayerManager playerManager = GetGame().GetPlayerManager();
		if (!playerManager)
			return "player " + playerId;

		string name = playerManager.GetPlayerName(playerId);
		if (name.IsEmpty())
			name = "unknown";

		return name + " (" + playerId + ")";
	}
}
