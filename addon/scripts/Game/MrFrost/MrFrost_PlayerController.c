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
	//! Raised when the transfer became part of what this waits for. At thirty the
	//! window was about a minute, shared with the deploy screen; a large set of
	//! server files arriving under a mass join can outlast that on its own.
	protected static const int MRFROST_AUTO_OPEN_MAX_TRIES = 75;

	//! Packets per tick, and the tick. Spreading the transfer keeps a joining
	//! player from spending their whole reliable budget on our content while the
	//! rest of the join is still being replicated.
	protected static const int MRFROST_CHUNKS_PER_TICK = 4;
	protected static const int MRFROST_CHUNK_TICK_MS = 100;

	//! Server side: whether this client has already been sent the server files.
	protected bool m_bMrFrostContentServed;

	//! Server side: whether a repeated request from this client has been logged.
	//! One line per player, however many packets they send.
	protected bool m_bMrFrostRepeatLogged;

	//! Client: the server has said it sent everything.
	protected bool m_bMrFrostContentComplete;

	//! How many controllers are sending content right now.
	//!
	//! The per-tick budget below is divided by this. Without it the pacing was
	//! per player, so it held for a trickle join and not at all for the case it
	//! exists for: sixty-four clients asking within seconds of a restart ran
	//! sixty-four independent 10 Hz senders at once, which is the burst the
	//! pacing was meant to prevent.
	protected static int s_iMrFrostActiveSenders;

	//! Whether this controller currently holds a share of that budget.
	protected bool m_bMrFrostSending;

	protected int m_iMrFrostAutoOpenAttempts;

	//! Whether the info menu has already opened itself this session, and the
	//! world time at which that happened.
	//!
	//! Static, so it survives a mission restart while the world clock does not.
	//! A stamp ahead of the clock therefore means the mission has restarted and
	//! the menu should be offered again - the same hazard the report cooldown
	//! has, and the same test for it.
	protected static bool s_bMrFrostAutoOpened;
	protected static float s_fMrFrostAutoOpenedAt;

	//! Whether the player has had the info menu in front of them this session
	//! by any road, including opening it themselves.
	//!
	//! The retry chain runs for up to 150 seconds waiting for a free screen. A
	//! player who reads the rules on their own in that window, closes them, and
	//! walks off had the same menu opened over their gameplay when the timer
	//! finally found the display empty. The welcome exists to make sure they see
	//! it once - once is what this records.
	protected static bool s_bMrFrostInfoMenuSeen;

	//! Server side: what is left to send to this one client, flattened across all
	//! channels. Each item carries its own channel, so one queue serves them all.
	protected ref array<int> m_aMrFrostOutgoingChannel;
	protected ref array<int> m_aMrFrostOutgoingIndex;
	protected ref array<int> m_aMrFrostOutgoingTotal;
	protected ref array<string> m_aMrFrostOutgoingData;
	protected int m_iMrFrostOutgoingSent;

	//! Client side: packets received per channel, by channel index.
	protected ref map<int, ref array<string>> m_mMrFrostIncoming;

	//! Channels still expected to arrive. Empty means the transfer is done.
	protected ref set<int> m_aMrFrostPendingChannels;

	//! Channels already reassembled and applied, so a late or repeated packet for
	//! one of them cannot put it back into the set above.
	protected ref set<int> m_aMrFrostDoneChannels;

	//------------------------------------------------------------------------------
	protected override void UpdateLocalPlayerController()
	{
		super.UpdateLocalPlayerController();

		// super returns early for every controller that is not ours; repeating
		// the test keeps us off the remote ones too.
		if (this != GetGame().GetPlayerController())
			return;

		// Released here, and not in a constructor. One controller is built per
		// connection - but on a machine hosting its own game that means one per
		// *connected player*, in the same process whose statics the host's own
		// welcome reads, so every join wiped the host's latch and the menu came
		// back over their screen on the next respawn. Vanilla calls this once, for
		// the controller that is actually ours, which is the granularity wanted.
		s_bMrFrostAutoOpened = false;
		s_fMrFrostAutoOpenedAt = 0;
		s_bMrFrostInfoMenuSeen = false;

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
	//! Whether a key that opens a menu should be ignored right now.
	//!
	//! These two are raw action listeners, not footer prompts. A prompt is an
	//! SCR_InputButtonComponent, which checks on its own whether it is visible,
	//! enabled, and whether its menu has focus; a listener checks nothing and
	//! fires wherever the context is up. Ours is renewed on a timer so the keys
	//! work during gameplay, which also left them live everywhere else.
	//!
	//! Two things that costs, both reachable today. Both actions are rebindable,
	//! so a player who puts Report on a letter loses a half-written report the
	//! first time they type that letter into the description box. And the pause
	//! menu, a dialog, or the other MrFrost menu can all be buried under a second
	//! one, because each Toggle() only looks for its own preset.
	protected bool MrFrost_MenuKeysBlocked()
	{
		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (workspace && EditBoxWidget.Cast(workspace.GetFocusedWidget()))
			return true;

		MenuManager menuManager = GetGame().GetMenuManager();
		if (!menuManager)
			return false;

		MenuBase top = menuManager.GetTopMenu();
		if (!top)
			return false;

		// One of ours is allowed through, so the key that opened a menu still
		// closes it. Anything else owns the screen and we stay out of it.
		return !MrFrost_MenuBase.Cast(top);
	}

	//------------------------------------------------------------------------------
	protected void MrFrost_OnInfoMenuAction()
	{
		if (MrFrost_MenuKeysBlocked())
			return;

		MrFrost_InfoMenuUI.Toggle();
	}

	//------------------------------------------------------------------------------
	//! Toggle() checks whether this server offers reporting at all, so a disabled
	//! feature keeps its key bound and simply does nothing.
	protected void MrFrost_OnReportAction()
	{
		if (MrFrost_MenuKeysBlocked())
			return;

		MrFrost_ReportUI.Toggle();
	}

	//------------------------------------------------------------------------------
	//! Client: hands a filled-in report to the server.
	//!
	//! Note what is *not* sent: who the accused are. The client names the option
	//! it picked, and the server resolves it from what the server itself saw —
	//! otherwise a modified client could pin a report on anybody.
	void MrFrost_SendReport(MrFrost_EReportKind kind, MrFrost_EReportTarget target, int selectedId, string selectedName, string description)
	{
		if (Replication.IsServer())
		{
			// Hosting our own game: no round trip, but the menu still needs the
			// verdict - otherwise a report vanishes without a word, which is what
			// it looked like locally.
			MrFrost_RpcDo_ReportAnswer(MrFrost_ReportSubmit.Accept(GetPlayerId(), kind, target, selectedId, selectedName, description));
			return;
		}

		Rpc(MrFrost_RpcAsk_Report, kind, target, selectedId, selectedName, description);
	}

	//------------------------------------------------------------------------------
	//! Server: rules on a report and tells the reporter what happened.
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void MrFrost_RpcAsk_Report(MrFrost_EReportKind kind, MrFrost_EReportTarget target, int selectedId, string selectedName, string description)
	{
		// The reporter is whoever owns this controller, never whoever the message
		// claims to be.
		string answer = MrFrost_ReportSubmit.Accept(GetPlayerId(), kind, target, selectedId, selectedName, description);

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
			// Forgotten here too, not only on the client path. Someone who played on
			// a server and then hosted their own kept that server's menu, wording and
			// verbose flag - and since this branch now marks itself complete, that is
			// no longer merely stale state, it is what gets shown.
			MrFrost_Features.ForgetServerContent();

			foreach (MrFrost_ServerContentChannel channel : MrFrost_ServerContent.GetChannels())
			{
				string raw = MrFrost_ServerContent.Read(channel);
				if (!raw.IsEmpty() && !channel.Apply(raw))
					MrFrost_Log.Error("This server's " + channel.GetId() + " content did not parse - using the content bundled with the addon.");
			}

			// Nothing is in flight here, so this machine is already finished. Without
			// it the flag stayed false forever - a host never sends the request and
			// so never receives the answer that sets it - and the welcome menu never
			// opened in single player or on a listen host at all.
			m_bMrFrostContentComplete = true;
			return;
		}

		// Cleared before asking, not as each answer arrives: a server that ships
		// no file for a feature sends nothing at all for it, so there is no
		// arrival to hang the reset on.
		m_bMrFrostContentComplete = false;
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
		if (m_bMrFrostContentServed)
		{
			// Said once per player, not once per packet. Debug is a real write to
			// script.log on any server running with verboseLogging on, so the
			// cheap path out was still a line on disk a modified client could ask
			// for at packet rate - a slow way to fill a disk with the diagnostic
			// switch the owner turned on to find the problem.
			if (!m_bMrFrostRepeatLogged)
			{
				m_bMrFrostRepeatLogged = true;
				MrFrost_Log.Debug("Ignoring a repeated content request. Further ones from this player are not logged.");
			}

			return;
		}

		m_bMrFrostContentServed = true;

		MrFrost_Features.Init();

		m_aMrFrostOutgoingChannel = {};
		m_aMrFrostOutgoingIndex = {};
		m_aMrFrostOutgoingTotal = {};
		m_aMrFrostOutgoingData = {};
		m_iMrFrostOutgoingSent = 0;

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
				m_aMrFrostOutgoingChannel.Insert(c);
				m_aMrFrostOutgoingIndex.Insert(i);
				m_aMrFrostOutgoingTotal.Insert(chunkCount);
				m_aMrFrostOutgoingData.Insert(chunks[i]);
			}
		}

		// Nothing to send is still an answer. Without it a client waiting on a
		// server that ships no files never hears anything at all.
		if (m_aMrFrostOutgoingData.IsEmpty())
		{
			Rpc(MrFrost_RpcDo_ServerContentDone);
			return;
		}

		GetGame().GetCallqueue().Remove(MrFrost_SendChunks);
		GetGame().GetCallqueue().CallLater(MrFrost_SendChunks, MRFROST_CHUNK_TICK_MS, true);
		s_iMrFrostActiveSenders++;
		m_bMrFrostSending = true;
	}

	//------------------------------------------------------------------------------
	//! Server: hand out the next few packets.
	protected void MrFrost_SendChunks()
	{
		if (!m_aMrFrostOutgoingData)
		{
			// Releases too. This is unreachable while the queue is armed, but it is
			// the one exit that would otherwise leave the counter inflated for the
			// life of the server, throttling every later transfer to the floor.
			GetGame().GetCallqueue().Remove(MrFrost_SendChunks);
			MrFrost_ReleaseSendSlot();
			return;
		}

		int total = m_aMrFrostOutgoingData.Count();

		// Shared budget: the more transfers are running, the fewer packets each
		// gets per tick. One is the floor, so a busy restart takes longer rather
		// than stalling - which also means the division stops dividing past four
		// senders, so this is a fourfold reduction of the burst rather than a cap
		// on it.
		int budget = MRFROST_CHUNKS_PER_TICK;
		if (s_iMrFrostActiveSenders > 1)
			budget = MRFROST_CHUNKS_PER_TICK / s_iMrFrostActiveSenders;

		if (budget < 1)
			budget = 1;

		for (int i = 0; i < budget && m_iMrFrostOutgoingSent < total; i++)
		{
			Rpc(MrFrost_RpcDo_ServerContentChunk,
				m_aMrFrostOutgoingChannel[m_iMrFrostOutgoingSent],
				m_aMrFrostOutgoingIndex[m_iMrFrostOutgoingSent],
				m_aMrFrostOutgoingTotal[m_iMrFrostOutgoingSent],
				m_aMrFrostOutgoingData[m_iMrFrostOutgoingSent]);

			m_iMrFrostOutgoingSent++;
		}

		if (m_iMrFrostOutgoingSent < total)
			return;

		GetGame().GetCallqueue().Remove(MrFrost_SendChunks);
		MrFrost_ReleaseSendSlot();
		Rpc(MrFrost_RpcDo_ServerContentDone);
		m_aMrFrostOutgoingChannel = null;
		m_aMrFrostOutgoingIndex = null;
		m_aMrFrostOutgoingTotal = null;
		m_aMrFrostOutgoingData = null;
	}

	//------------------------------------------------------------------------------
	//! Gives this controller's share of the send budget back.
	protected void MrFrost_ReleaseSendSlot()
	{
		// Flag rather than a look at the queue. The queue is non-null but empty
		// on the path that never took a slot, so testing it would have released
		// one that was never held.
		if (!m_bMrFrostSending)
			return;

		m_bMrFrostSending = false;

		if (s_iMrFrostActiveSenders > 0)
			s_iMrFrostActiveSenders--;
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
		if (total <= 0 || total > MrFrost_ServerContent.MAX_CHUNKS || index < 0 || index >= total)
			return;

		if (!m_mMrFrostIncoming)
		{
			m_mMrFrostIncoming = new map<int, ref array<string>>();
			m_aMrFrostPendingChannels = new set<int>();
			m_aMrFrostDoneChannels = new set<int>();
		}

		// A channel is finished once. Without this, a chunk arriving for one that
		// already completed found no partial array, built a fresh one of empty
		// slots and put the channel back into the pending set - where nothing
		// would ever fill it again. MrFrost_IsTransferPending() then stayed true
		// for the rest of the session and the welcome menu never opened.
		if (m_aMrFrostDoneChannels.Contains(channelIndex))
			return;

		array<string> parts = m_mMrFrostIncoming.Get(channelIndex);
		if (!parts || parts.Count() != total)
		{
			parts = {};
			for (int i = 0; i < total; i++)
			{
				parts.Insert(string.Empty);
			}

			m_mMrFrostIncoming.Set(channelIndex, parts);
			m_aMrFrostPendingChannels.Insert(channelIndex);
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

		m_mMrFrostIncoming.Remove(channelIndex);
		m_aMrFrostPendingChannels.RemoveItem(channelIndex);
		m_aMrFrostDoneChannels.Insert(channelIndex);

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
		m_bMrFrostContentComplete = true;
		MrFrost_Log.Debug("Server content complete.");
	}

	//------------------------------------------------------------------------------
	//! True while at least one feature's content is still on its way.
	protected bool MrFrost_IsTransferPending()
	{
		return m_aMrFrostPendingChannels && !m_aMrFrostPendingChannels.IsEmpty();
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
		MrFrost_ReleaseSendSlot();

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

		if (s_bMrFrostAutoOpened && !MrFrost_HasMissionRestarted())
			return;

		if (this != GetGame().GetPlayerController())
			return;

		MrFrost_InfoMenuConfig config = MrFrost_InfoMenuConfigLoader.Get();
		if (!config || !config.m_bOpenOnJoin)
			return;

		s_bMrFrostAutoOpened = true;
		s_fMrFrostAutoOpenedAt = GetGame().GetWorld().GetWorldTime();
		GetGame().GetCallqueue().CallLater(MrFrost_AutoOpenInfoMenu, MRFROST_AUTO_OPEN_DELAY_MS, false);
	}

	//------------------------------------------------------------------------------
	//! Called by the info menu whenever it opens, however it was opened.
	static void MrFrost_MarkInfoMenuSeen()
	{
		s_bMrFrostInfoMenuSeen = true;
	}

	//------------------------------------------------------------------------------
	//! True when the world clock has gone backwards since the menu last opened,
	//! which only happens when the mission restarted underneath these statics.
	protected static bool MrFrost_HasMissionRestarted()
	{
		// A world time below the stamp means the clock restarted under it. It does
		// not catch a *second* connection inside one session, where the new world
		// starts at zero and climbs past the old stamp before anyone spawns - so
		// the latch is also released in UpdateLocalPlayerController, which vanilla
		// calls once for the controller that is actually ours.
		// MrFrost_ReportSubmit guards its own stamps the same way.
		return GetGame().GetWorld().GetWorldTime() < s_fMrFrostAutoOpenedAt;
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
		// finished; the pending set catches a channel that is half-arrived. It
		// cannot catch one that has not started, since it only learns of a channel
		// from its first chunk - so the two together are a strong check, not a
		// proof.
		// Nothing left to welcome them to. Checked before the retry rather than
		// after it, because the player opening the menu themselves is exactly what
		// keeps the screen busy and sends this down the retry road in the first
		// place.
		if (s_bMrFrostInfoMenuSeen)
		{
			MrFrost_Log.Debug("The player has already seen the info menu - not opening it again.");
			return;
		}

		if (!m_bMrFrostContentComplete || MrFrost_IsTransferPending() || menuManager.IsAnyMenuOpen())
		{
			m_iMrFrostAutoOpenAttempts++;

			if (m_iMrFrostAutoOpenAttempts > MRFROST_AUTO_OPEN_MAX_TRIES)
			{
				MrFrost_Log.Debug("Gave up on the welcome info menu.");
				return;
			}

			GetGame().GetCallqueue().CallLater(MrFrost_AutoOpenInfoMenu, MRFROST_AUTO_OPEN_RETRY_MS, false);
			return;
		}

		// Asked again, now that the server's own content has arrived. The first
		// ask happened before the transfer, against whatever this client was
		// carrying - so a server switching the menu off, or switching the welcome
		// off, was decided against the wrong file and could be overridden by a
		// race the longer retry window makes more likely.
		MrFrost_InfoMenuConfig config = MrFrost_InfoMenuConfigLoader.Get();
		if (!config || !config.m_bOpenOnJoin || !config.HasVisibleContent())
		{
			MrFrost_Log.Debug("This server does not want a welcome info menu.");
			return;
		}

		MrFrost_Log.Debug("Opening the info menu for the freshly joined player.");
		menuManager.OpenMenu(ChimeraMenuPreset.MrFrost_InfoMenu);
	}
}
