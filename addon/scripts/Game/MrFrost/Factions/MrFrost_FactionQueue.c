//------------------------------------------------------------------------------
//! The queue a player joins when the faction they wanted has no room.
//!
//! Authority only. The queue is state the server owns; clients are told their
//! own position and nothing else, because the list of who else is waiting is
//! not theirs to read.
//!
//! One queue per faction, one place per player. Picking a different faction
//! leaves the previous queue: a player waiting in two lines would be counted
//! twice against both, and the position they read would mean nothing.
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
//! What happened when a player asked to be queued.
enum MrFrost_EQueueResult
{
	//! Now waiting. Position follows.
	QUEUED,

	//! Already waiting for this faction; position unchanged.
	ALREADY_QUEUED,

	//! Every place is taken.
	QUEUE_FULL,

	//! The server switched queueing off.
	QUEUE_DISABLED,

	//! Nothing to wait for — a whitelist or a closed faction.
	NOT_WORTH_QUEUEING,
}

//------------------------------------------------------------------------------
class MrFrost_FactionQueue
{
	//! Faction key to the player ids waiting for it, in the order they arrived.
	protected static ref map<string, ref array<int>> s_mQueues;

	//------------------------------------------------------------------------------
	//! Puts a player in line for a faction, leaving whatever line they were in.
	static MrFrost_EQueueResult Enqueue(int playerId, string factionKey, MrFrost_EFactionVerdict verdict)
	{
		if (!MrFrost_FactionRules.IsWorthQueueing(verdict))
			return MrFrost_EQueueResult.NOT_WORTH_QUEUEING;

		MrFrost_FactionConfig config = MrFrost_FactionConfigLoader.Get();
		if (!config || config.m_iQueueLimit <= 0)
			return MrFrost_EQueueResult.QUEUE_DISABLED;

		array<int> queue = GetOrCreate(factionKey);
		if (queue.Contains(playerId))
			return MrFrost_EQueueResult.ALREADY_QUEUED;

		if (queue.Count() >= config.m_iQueueLimit)
			return MrFrost_EQueueResult.QUEUE_FULL;

		// Only after the place is known to exist: a player refused for a full
		// queue keeps the place they already had somewhere else.
		Leave(playerId);

		queue.Insert(playerId);
		MrFrost_Log.Debug("Player " + playerId + " queued for '" + factionKey + "' at position " + queue.Count() + ".");

		return MrFrost_EQueueResult.QUEUED;
	}

	//------------------------------------------------------------------------------
	//! How many players are ahead of this one. Zero means their turn has come,
	//! -1 that they are not waiting for anything.
	static int GetPosition(int playerId)
	{
		if (!s_mQueues)
			return -1;

		foreach (string key, array<int> queue : s_mQueues)
		{
			int index = queue.Find(playerId);
			if (index >= 0)
				return index;
		}

		return -1;
	}

	//------------------------------------------------------------------------------
	//! The faction a player is waiting for, or an empty string.
	static string GetQueuedFaction(int playerId)
	{
		if (!s_mQueues)
			return string.Empty;

		foreach (string key, array<int> queue : s_mQueues)
		{
			if (queue.Contains(playerId))
				return key;
		}

		return string.Empty;
	}

	//------------------------------------------------------------------------------
	static int GetLength(string factionKey)
	{
		if (!s_mQueues)
			return 0;

		array<int> queue;
		if (!s_mQueues.Find(factionKey, queue))
			return 0;

		return queue.Count();
	}

	//------------------------------------------------------------------------------
	//! Takes a player out of whichever queue holds them. Called on assignment, on
	//! disconnect, and when they pick something else.
	static void Leave(int playerId)
	{
		if (!s_mQueues)
			return;

		foreach (string key, array<int> queue : s_mQueues)
		{
			int index = queue.Find(playerId);
			if (index >= 0)
				queue.Remove(index);
		}
	}

	//------------------------------------------------------------------------------
	//! Empties a faction's queue and hands back everyone who was in it, so the
	//! caller can tell them why. Used when a faction stops being joinable.
	static void Clear(string factionKey, out notnull array<int> outWaiting)
	{
		outWaiting.Clear();

		if (!s_mQueues)
			return;

		array<int> queue;
		if (!s_mQueues.Find(factionKey, queue))
			return;

		foreach (int playerId : queue)
		{
			outWaiting.Insert(playerId);
		}

		queue.Clear();
	}

	//------------------------------------------------------------------------------
	//! The players at the front of a faction's queue, in order, who may now join.
	//!
	//! The rule is asked once per candidate rather than once for the faction: each
	//! player admitted changes the counts for the next, so admitting the whole
	//! queue at once would overshoot the limit it was waiting on.
	static void TakeAdmitted(string factionKey, notnull Faction faction, out notnull array<int> outAdmitted)
	{
		outAdmitted.Clear();

		if (!s_mQueues)
			return;

		array<int> queue;
		if (!s_mQueues.Find(factionKey, queue) || queue.IsEmpty())
			return;

		// Walked front to back, stopping at the first player who still cannot get
		// in: letting a later one past would take the place the person ahead of
		// them has been waiting for.
		while (!queue.IsEmpty())
		{
			int playerId = queue[0];

			if (MrFrost_FactionRules.Evaluate(playerId, faction) != MrFrost_EFactionVerdict.ALLOWED)
				break;

			queue.RemoveOrdered(0);
			outAdmitted.Insert(playerId);
		}
	}

	//------------------------------------------------------------------------------
	//! Every player waiting for anything, for a broadcast of positions.
	static void GetWaiting(out notnull array<int> outPlayers)
	{
		outPlayers.Clear();

		if (!s_mQueues)
			return;

		foreach (string key, array<int> queue : s_mQueues)
		{
			foreach (int playerId : queue)
			{
				outPlayers.Insert(playerId);
			}
		}
	}

	//------------------------------------------------------------------------------
	//! Drops every queue. The player counts a queue was waiting on reset with the
	//! mission, so the places it held would mean nothing afterwards.
	static void Reset()
	{
		s_mQueues = null;
	}

	//------------------------------------------------------------------------------
	protected static array<int> GetOrCreate(string factionKey)
	{
		if (!s_mQueues)
			s_mQueues = new map<string, ref array<int>>();

		array<int> queue;
		if (s_mQueues.Find(factionKey, queue))
			return queue;

		queue = {};
		s_mQueues.Set(factionKey, queue);

		return queue;
	}
}
