//------------------------------------------------------------------------------
//! Client plumbing for every MrFrost feature: keys, input contexts, and pulling
//! this server's content down on join.
//!
//! UpdateLocalPlayerController() is the point at which vanilla registers all of
//! the local player's own action listeners (inventory, focus toggle, ping, ...).
//! Hooking the same method means everything below happens exactly once, on the
//! client only, and only for the controller that actually belongs to this player
//! — the checks for all of that already happened in super.
//------------------------------------------------------------------------------
modded class SCR_PlayerController
{
	//! Must match the action names in chimeraInputCommon.conf.
	protected static const string MRFROST_INFOMENU_ACTION = "MrFrost_OpenInfoMenu";
	protected static const string MRFROST_REPORT_ACTION   = "MrFrost_OpenReportMenu";

	//! Our own gameplay input context, declared in chimeraInputCommon.conf.
	protected static const string MRFROST_CONTEXT = "MrFrostContext";

	//! How often the context is renewed, and for how long each renewal holds.
	//! The hold is longer than the interval so the context never lapses between
	//! two ticks — a key would silently stop working for a frame or two.
	protected static const int MRFROST_CONTEXT_TICK_MS = 250;
	protected static const int MRFROST_CONTEXT_HOLD_MS = 1000;

	//! Let the spawn settle before taking over the screen. The player has just
	//! been placed in the world and the HUD is still coming up; opening in the
	//! same frame fights with that.
	protected static const int MRFROST_AUTO_OPEN_DELAY_MS = 1500;

	//! Retry cadence while the deploy screen or a dialog still owns the display.
	protected static const int MRFROST_AUTO_OPEN_RETRY_MS = 2000;
	protected static const int MRFROST_AUTO_OPEN_MAX_TRIES = 30;

	//! Packets per tick, and the tick. Spreading the transfer keeps a joining
	//! player from spending their whole reliable budget on our content while the
	//! rest of the join is still being replicated.
	protected static const int MRFROST_CHUNKS_PER_TICK = 4;
	protected static const int MRFROST_CHUNK_TICK_MS = 100;

	//! Ceiling on how many chunks one channel may claim to have. At 900 bytes a
	//! chunk this allows a file of about 3.6 MB, far past anything a server has
	//! reason to send, and it stops a hostile one from having every client
	//! allocate an array sized by a number it made up.
	protected static const int MRFROST_MAX_CHUNKS = 4096;

	//! Server side: whether this client has already been sent the server files.
	protected bool m_bContentServed;

	//! Client: the server has said it sent everything.
	protected bool m_bContentComplete;

	//! How many controllers are sending content right now.
	//!
	//! The per-tick budget below is divided by this. Without it the pacing was
	//! per player, so it held for a trickle join and not at all for the case it
	//! exists for: sixty-four clients asking within seconds of a restart ran
	//! sixty-four independent 10 Hz senders at once, which is the burst the
	//! pacing was meant to prevent.
	protected static int s_iActiveSenders;

	//! Whether this controller currently holds a share of that budget.
	protected bool m_bSending;

	protected int m_iAutoOpenAttempts;

	//! Static, so the info menu greets the player once per session rather than on
	//! every respawn.
	//! Whether the info menu has already opened itself this session, and the
	//! world time at which that happened.
	//!
	//! Static, so it survives a mission restart while the world clock does not.
	//! A stamp ahead of the clock therefore means the mission has restarted and
	//! the menu should be offered again - the same hazard the report cooldown
	//! has, and the same test for it.
	protected static bool s_bAutoOpened;
	protected static float s_fAutoOpenedAt;

	//! Server side: what is left to send to this one client, flattened across all
	//! channels. Each item carries its own channel, so one queue serves them all.
	protected ref array<int> m_aOutgoingChannel;
	protected ref array<int> m_aOutgoingIndex;
	protected ref array<int> m_aOutgoingTotal;
	protected ref array<string> m_aOutgoingData;
	protected int m_iOutgoingSent;

	//! Client side: packets received per channel, by channel index.
	protected ref map<int, ref array<string>> m_mIncoming;

	//! Channels still expected to arrive. Empty means the transfer is done.
	protected ref set<int> m_aPendingChannels;

	//------------------------------------------------------------------------------
	protected override void UpdateLocalPlayerController()
	{
		super.UpdateLocalPlayerController();

		// super returns early for every controller that is not ours; repeating
		// the test keeps us off the remote ones too.
		if (this != GetGame().GetPlayerController())
			return;

		MrFrost_Features.Init();

		InputManager inputManager = GetGame().GetInputManager();
		if (!inputManager)
			return;

		inputManager.AddActionListener(MRFROST_INFOMENU_ACTION, EActionTrigger.DOWN, MrFrost_OnInfoMenuAction);
		inputManager.AddActionListener(MRFROST_REPORT_ACTION, EActionTrigger.DOWN, MrFrost_OnReportAction);

		// Keep our own context alive for as long as this client is playing.
		//
		// The input system only delivers an action while some active context
		// carries it, and nothing in vanilla activates a context it has never
		// heard of. Renewing ours on a timer is what turns a registered,
		// rebindable-but-dead key into one that actually fires.
		GetGame().GetCallqueue().Remove(MrFrost_KeepContextAlive);
		GetGame().GetCallqueue().CallLater(MrFrost_KeepContextAlive, MRFROST_CONTEXT_TICK_MS, true);
		MrFrost_KeepContextAlive();

		// Attaching a listener always succeeds, even for an action the engine has
		// never heard of — which is exactly how a key could look wired up and
		// still do nothing. This asks the input system whether the action really
		// carries a binding, so the log says which of the two it is.
		MrFrost_ReportBinding(inputManager, MRFROST_INFOMENU_ACTION);

		// The other half of the same question: a bound action still does nothing
		// while its context is down. Asked once the timer has had a chance to run.
		GetGame().GetCallqueue().CallLater(MrFrost_ReportContext, 3000, false);

		MrFrost_RequestServerContent();
	}

	//------------------------------------------------------------------------------
	protected void MrFrost_KeepContextAlive()
	{
		InputManager inputManager = GetGame().GetInputManager();
		if (!inputManager)
			return;

		inputManager.ActivateContext(MRFROST_CONTEXT, MRFROST_CONTEXT_HOLD_MS);
	}

	//------------------------------------------------------------------------------
	protected void MrFrost_ReportBinding(notnull InputManager inputManager, string actionName)
	{
		array<string> keys = {};
		array<BaseContainer> filters = {};

		if (inputManager.GetActionKeybinding(actionName, keys, filters))
			MrFrost_Log.Info("Action '" + actionName + "' is bound to: " + keys.Count() + " key(s).");
		else
			MrFrost_Log.Error("Action '" + actionName + "' is NOT known to the input system.");
	}

	//------------------------------------------------------------------------------
	protected void MrFrost_ReportContext()
	{
		InputManager inputManager = GetGame().GetInputManager();
		if (!inputManager)
			return;

		if (inputManager.IsContextActive(MRFROST_CONTEXT))
			MrFrost_Log.Info("Input context '" + MRFROST_CONTEXT + "' is active.");
		else
			MrFrost_Log.Error("Input context '" + MRFROST_CONTEXT + "' is NOT active - keys cannot fire.");
	}

	//------------------------------------------------------------------------------
	protected void MrFrost_OnInfoMenuAction()
	{
		MrFrost_InfoMenuUI.Toggle();
	}

	//------------------------------------------------------------------------------
	//! Toggle() checks whether this server offers reporting at all, so a disabled
	//! feature keeps its key bound and simply does nothing.
	protected void MrFrost_OnReportAction()
	{
		MrFrost_ReportUI.Toggle();
	}

	//------------------------------------------------------------------------------
	//! Client: hands a filled-in report to the server.
	//!
	//! Note what is *not* sent: who the accused are. The client names the option
	//! it picked, and the server resolves it from what the server itself saw —
	//! otherwise a modified client could pin a report on anybody.
	void MrFrost_SendReport(MrFrost_EReportKind kind, MrFrost_EReportTarget target, int selectedId, string description)
	{
		if (Replication.IsServer())
		{
			// Hosting our own game: no round trip, but the menu still needs the
			// verdict - otherwise a report vanishes without a word, which is what
			// it looked like locally.
			MrFrost_RpcDo_ReportAnswer(MrFrost_ReportSubmit.Accept(GetPlayerId(), kind, target, selectedId, description));
			return;
		}

		Rpc(MrFrost_RpcAsk_Report, kind, target, selectedId, description);
	}

	//------------------------------------------------------------------------------
	//! Server: rules on a report and tells the reporter what happened.
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void MrFrost_RpcAsk_Report(MrFrost_EReportKind kind, MrFrost_EReportTarget target, int selectedId, string description)
	{
		// The reporter is whoever owns this controller, never whoever the message
		// claims to be.
		string answer = MrFrost_ReportSubmit.Accept(GetPlayerId(), kind, target, selectedId, description);

		Rpc(MrFrost_RpcDo_ReportAnswer, answer);
	}

	//------------------------------------------------------------------------------
	//! Client: the server's verdict, as a text key so it is worded in the
	//! player's own language rather than the server's.
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void MrFrost_RpcDo_ReportAnswer(string textKey)
	{
		MenuManager menuManager = GetGame().GetMenuManager();
		if (!menuManager)
			return;

		MrFrost_ReportUI menu = MrFrost_ReportUI.Cast(menuManager.FindMenuByPreset(ChimeraMenuPreset.MrFrost_ReportMenu));
		if (menu)
			menu.OnServerAnswer(textKey);
	}

	//------------------------------------------------------------------------------
	//! Asks this server for the content of every feature.
	//!
	//! A request rather than an unprompted push from the server: the moment a
	//! client is actually ready to receive is a moment only the client can know,
	//! and getting it wrong means content that arrives before anything can hold it.
	protected void MrFrost_RequestServerContent()
	{
		// Hosting our own game means the files are already on this machine, so the
		// whole transfer is pointless — read them directly.
		if (Replication.IsServer())
		{
			foreach (MrFrost_ServerContentChannel channel : MrFrost_ServerContent.GetChannels())
			{
				string raw = MrFrost_ServerContent.Read(channel);
				if (!raw.IsEmpty() && !channel.Apply(raw))
					MrFrost_Log.Error("This server's " + channel.GetId() + " content did not parse - using the content bundled with the addon.");
			}

			return;
		}

		// Cleared before asking, not as each answer arrives: a server that ships
		// no file for a feature sends nothing at all for it, so there is no
		// arrival to hang the reset on.
		m_bContentComplete = false;
		MrFrost_Features.ForgetServerContent();

		Rpc(MrFrost_RpcAsk_ServerContent);
	}

	//------------------------------------------------------------------------------
	//! Server: queue this client's copy of every file that exists.
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void MrFrost_RpcAsk_ServerContent()
	{
		// A client asks once, and this enforces it. Testing whether a transfer is
		// still running only stopped overlapping ones: the queue is cleared the
		// moment it drains, so a modified client could wait for that and ask
		// again, forever. Each round cost the server a full parse, repack and
		// split of every server file - unbounded work and unbounded bandwidth
		// out of one empty packet in.
		//
		// It is also the answer for a server whose files produce no chunks at
		// all. That path returned without arming the sender, leaving the old
		// test false and the whole rebuild reachable at packet rate.
		if (m_bContentServed)
		{
			MrFrost_Log.Debug("Ignoring a repeated content request.");
			return;
		}

		m_bContentServed = true;

		MrFrost_Features.Init();

		m_aOutgoingChannel = {};
		m_aOutgoingIndex = {};
		m_aOutgoingTotal = {};
		m_aOutgoingData = {};
		m_iOutgoingSent = 0;

		array<ref MrFrost_ServerContentChannel> channels = MrFrost_ServerContent.GetChannels();

		for (int c = 0, count = channels.Count(); c < count; c++)
		{
			// Built once for the whole server, not once per player. An empty list
			// means this feature has no file, or the channel refused to hand its
			// file out - either way the client keeps its bundled content.
			array<string> chunks = MrFrost_ServerContent.ChunksForClient(channels[c]);
			if (chunks.IsEmpty())
				continue;

			for (int i = 0, chunkCount = chunks.Count(); i < chunkCount; i++)
			{
				m_aOutgoingChannel.Insert(c);
				m_aOutgoingIndex.Insert(i);
				m_aOutgoingTotal.Insert(chunkCount);
				m_aOutgoingData.Insert(chunks[i]);
			}
		}

		// Nothing to send is still an answer. Without it a client waiting on a
		// server that ships no files never hears anything at all.
		if (m_aOutgoingData.IsEmpty())
		{
			Rpc(MrFrost_RpcDo_ServerContentDone);
			return;
		}

		GetGame().GetCallqueue().Remove(MrFrost_SendChunks);
		GetGame().GetCallqueue().CallLater(MrFrost_SendChunks, MRFROST_CHUNK_TICK_MS, true);
		s_iActiveSenders++;
		m_bSending = true;
	}

	//------------------------------------------------------------------------------
	//! Server: hand out the next few packets.
	protected void MrFrost_SendChunks()
	{
		if (!m_aOutgoingData)
		{
			GetGame().GetCallqueue().Remove(MrFrost_SendChunks);
			return;
		}

		int total = m_aOutgoingData.Count();

		// Shared budget: the more transfers are running, the fewer packets each
		// gets per tick. One is the floor, so a busy restart takes longer rather
		// than stalling.
		int budget = MRFROST_CHUNKS_PER_TICK;
		if (s_iActiveSenders > 1)
			budget = MRFROST_CHUNKS_PER_TICK / s_iActiveSenders;

		if (budget < 1)
			budget = 1;

		for (int i = 0; i < budget && m_iOutgoingSent < total; i++)
		{
			Rpc(MrFrost_RpcDo_ServerContentChunk,
				m_aOutgoingChannel[m_iOutgoingSent],
				m_aOutgoingIndex[m_iOutgoingSent],
				m_aOutgoingTotal[m_iOutgoingSent],
				m_aOutgoingData[m_iOutgoingSent]);

			m_iOutgoingSent++;
		}

		if (m_iOutgoingSent < total)
			return;

		GetGame().GetCallqueue().Remove(MrFrost_SendChunks);
		ReleaseSendSlot();
		Rpc(MrFrost_RpcDo_ServerContentDone);
		m_aOutgoingChannel = null;
		m_aOutgoingIndex = null;
		m_aOutgoingTotal = null;
		m_aOutgoingData = null;
	}

	//------------------------------------------------------------------------------
	//! Gives this controller's share of the send budget back.
	protected void ReleaseSendSlot()
	{
		// Flag rather than a look at the queue. The queue is non-null but empty
		// on the path that never took a slot, so testing it would have released
		// one that was never held.
		if (!m_bSending)
			return;

		m_bSending = false;

		if (s_iActiveSenders > 0)
			s_iActiveSenders--;
	}

	//------------------------------------------------------------------------------
	//! Client: one packet of one feature's content.
	//!
	//! Reassembled by index rather than by arrival order — reliable delivery
	//! guarantees everything arrives, not that it arrives in the order it was
	//! sent, and content stitched back together in the wrong order would be
	//! nonsense that still parses.
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void MrFrost_RpcDo_ServerContentChunk(int channelIndex, int index, int total, string data)
	{
		MrFrost_Features.Init();

		array<ref MrFrost_ServerContentChannel> channels = MrFrost_ServerContent.GetChannels();
		if (channelIndex < 0 || channelIndex >= channels.Count())
			return;

		// Bounded above as well as below. A hostile server sending total = 2e9
		// would have the client allocate a two-billion-entry array on the main
		// thread before a single byte of content arrived.
		if (total <= 0 || total > MRFROST_MAX_CHUNKS || index < 0 || index >= total)
			return;

		if (!m_mIncoming)
		{
			m_mIncoming = new map<int, ref array<string>>();
			m_aPendingChannels = new set<int>();
		}

		array<string> parts = m_mIncoming.Get(channelIndex);
		if (!parts || parts.Count() != total)
		{
			parts = {};
			for (int i = 0; i < total; i++)
			{
				parts.Insert(string.Empty);
			}

			m_mIncoming.Set(channelIndex, parts);
			m_aPendingChannels.Insert(channelIndex);
		}

		parts[index] = data;

		foreach (string part : parts)
		{
			if (part.IsEmpty())
				return;	// Still waiting on at least one packet.
		}

		string json;
		foreach (string piece : parts)
		{
			json += piece;
		}

		m_mIncoming.Remove(channelIndex);
		m_aPendingChannels.RemoveItem(channelIndex);

		MrFrost_ServerContentChannel channel = channels[channelIndex];

		// Answered rather than discarded. A channel returns false when the text
		// it was handed did not parse, and that is the one case where a player
		// sees the bundled content while the server believes it sent its own.
		if (!channel.Apply(json))
			MrFrost_Log.Error("This server's " + channel.GetId() + " content did not parse - using the content bundled with the addon.");

	}

	//------------------------------------------------------------------------------
	//! Client: the server has sent everything it means to send.
	//!
	//! Said by the server, because only the server knows how many features have
	//! a file. The client tracks which channels have started arriving, which
	//! answers "is one half-finished", not "is there another one coming" - and
	//! since the queue is drained channel by channel, that looked complete every
	//! time a channel finished.
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void MrFrost_RpcDo_ServerContentDone()
	{
		m_bContentComplete = true;
		MrFrost_Log.Debug("Server content complete.");
	}

	//------------------------------------------------------------------------------
	//! True while at least one feature's content is still on its way.
	protected bool MrFrost_IsTransferPending()
	{
		return m_aPendingChannels && !m_aPendingChannels.IsEmpty();
	}

	//------------------------------------------------------------------------------
	//! Repeating timers outlive the controller that armed them.
	//!
	//! MrFrost_SendChunks in particular runs at 10 Hz and touches this object's
	//! own arrays; a client disconnecting mid-transfer left it firing against a
	//! controller that was already gone, once per player for the rest of the
	//! session. The context pump is the same shape at 4 Hz.
	//!
	//! Removing a timer that was never armed is harmless, so all four come off
	//! regardless of how far this controller got.
	void ~SCR_PlayerController()
	{
		ReleaseSendSlot();

		ScriptCallQueue queue = GetGame().GetCallqueue();
		if (!queue)
			return;

		queue.Remove(MrFrost_KeepContextAlive);
		queue.Remove(MrFrost_SendChunks);
		queue.Remove(MrFrost_ReportContext);
		queue.Remove(MrFrost_AutoOpenInfoMenu);
	}

	//------------------------------------------------------------------------------
	//! First time this client actually takes control of a character, show the
	//! info menu once — that is the moment the player has finished joining.
	override void OnControlledEntityChanged(IEntity from, IEntity to)
	{
		super.OnControlledEntityChanged(from, to);

		if (!to)
			return;

		if (s_bAutoOpened && !MrFrost_HasMissionRestarted())
			return;

		if (this != GetGame().GetPlayerController())
			return;

		MrFrost_InfoMenuConfig config = MrFrost_InfoMenuConfigLoader.Get();
		if (!config || !config.m_bOpenOnJoin)
			return;

		s_bAutoOpened = true;
		s_fAutoOpenedAt = GetGame().GetWorld().GetWorldTime();
		GetGame().GetCallqueue().CallLater(MrFrost_AutoOpenInfoMenu, MRFROST_AUTO_OPEN_DELAY_MS, false);
	}

	//------------------------------------------------------------------------------
	//! True when the world clock has gone backwards since the menu last opened,
	//! which only happens when the mission restarted underneath these statics.
	protected static bool MrFrost_HasMissionRestarted()
	{
		return GetGame().GetWorld().GetWorldTime() < s_fAutoOpenedAt;
	}

	//------------------------------------------------------------------------------
	protected void MrFrost_AutoOpenInfoMenu()
	{
		MenuManager menuManager = GetGame().GetMenuManager();
		if (!menuManager)
			return;

		// Opening before the server's own content has finished arriving would
		// greet the player with the fallback and then have nothing to correct it.
		//
		// Joining also lands the player in the deploy screen, so the very first
		// attempt almost always finds a menu open. Rather than give up — which is
		// what swallowed the welcome screen entirely — keep looking until the
		// display is free, and stop after a while so this never becomes a
		// permanent timer.
		// Both halves. "Nothing half-finished" is not "nothing more coming" - the
		// queue drains channel by channel, so the client looked done every time a
		// channel completed, and the menu could open on bundled content that the
		// next channel was still on its way to replace. The server says when it is
		// finished; the pending set catches the case where that word overtakes the
		// last chunk, which reliable delivery does not rule out.
		if (!m_bContentComplete || MrFrost_IsTransferPending() || menuManager.IsAnyMenuOpen())
		{
			m_iAutoOpenAttempts++;

			if (m_iAutoOpenAttempts > MRFROST_AUTO_OPEN_MAX_TRIES)
			{
				MrFrost_Log.Debug("Gave up on the welcome info menu.");
				return;
			}

			GetGame().GetCallqueue().CallLater(MrFrost_AutoOpenInfoMenu, MRFROST_AUTO_OPEN_RETRY_MS, false);
			return;
		}

		MrFrost_Log.Debug("Opening the info menu for the freshly joined player.");
		menuManager.OpenMenu(ChimeraMenuPreset.MrFrost_InfoMenu);
	}
}
