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
				if (!raw.IsEmpty())
					channel.Apply(raw);
			}

			return;
		}

		Rpc(MrFrost_RpcAsk_ServerContent);
	}

	//------------------------------------------------------------------------------
	//! Server: queue this client's copy of every file that exists.
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void MrFrost_RpcAsk_ServerContent()
	{
		MrFrost_Features.Init();

		m_aOutgoingChannel = {};
		m_aOutgoingIndex = {};
		m_aOutgoingTotal = {};
		m_aOutgoingData = {};
		m_iOutgoingSent = 0;

		array<ref MrFrost_ServerContentChannel> channels = MrFrost_ServerContent.GetChannels();

		for (int c = 0, count = channels.Count(); c < count; c++)
		{
			string raw = MrFrost_ServerContent.Read(channels[c]);
			if (raw.IsEmpty())
				continue;	// No file for this feature. The client keeps its bundled content.

			array<string> chunks = {};
			MrFrost_ServerContent.Split(raw, MrFrost_ServerContent.CHUNK_SIZE, chunks);

			for (int i = 0, chunkCount = chunks.Count(); i < chunkCount; i++)
			{
				m_aOutgoingChannel.Insert(c);
				m_aOutgoingIndex.Insert(i);
				m_aOutgoingTotal.Insert(chunkCount);
				m_aOutgoingData.Insert(chunks[i]);
			}
		}

		if (m_aOutgoingData.IsEmpty())
			return;

		GetGame().GetCallqueue().Remove(MrFrost_SendChunks);
		GetGame().GetCallqueue().CallLater(MrFrost_SendChunks, MRFROST_CHUNK_TICK_MS, true);
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

		for (int i = 0; i < MRFROST_CHUNKS_PER_TICK && m_iOutgoingSent < total; i++)
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
		m_aOutgoingChannel = null;
		m_aOutgoingIndex = null;
		m_aOutgoingTotal = null;
		m_aOutgoingData = null;
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

		if (total <= 0 || index < 0 || index >= total)
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
		MrFrost_Log.Debug("Received this server's " + channel.GetId() + " content (" + json.Length() + " bytes).");
		channel.Apply(json);
	}

	//------------------------------------------------------------------------------
	//! True while at least one feature's content is still on its way.
	protected bool MrFrost_IsTransferPending()
	{
		return m_aPendingChannels && !m_aPendingChannels.IsEmpty();
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
		if (MrFrost_IsTransferPending() || menuManager.IsAnyMenuOpen())
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
