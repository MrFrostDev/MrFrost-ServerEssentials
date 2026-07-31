//------------------------------------------------------------------------------
//! The list of features in this addon, in one place.
//!
//! Everything that has to know "which menus exist" reads it from here: the
//! pause menu builds its entries from it, and the server-content transfer takes
//! its channel order from it.
//!
//! Adding a feature is therefore a change in one file. Miss it and the feature
//! simply does not appear — there is no second list to keep in sync.
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
//! One menu, as the pause menu needs to know it.
class MrFrost_MenuEntry
{
	ChimeraMenuPreset m_Preset;

	//------------------------------------------------------------------------------
	//! Label of the pause menu entry. Read from the feature's own config, so a
	//! server owner renaming a menu never touches script.
	string GetLabel();

	//------------------------------------------------------------------------------
	ResourceName GetIconImageset();

	//------------------------------------------------------------------------------
	string GetIconName();

	//------------------------------------------------------------------------------
	//! False hides the entry entirely — a feature a server has switched off must
	//! not leave a dead button behind.
	bool IsEnabled()
	{
		return true;
	}
}

//------------------------------------------------------------------------------
//! The info menu's pause menu entry.
class MrFrost_InfoMenuEntryDescriptor : MrFrost_MenuEntry
{
	//------------------------------------------------------------------------------
	void MrFrost_InfoMenuEntryDescriptor()
	{
		m_Preset = ChimeraMenuPreset.MrFrost_InfoMenu;
	}

	//------------------------------------------------------------------------------
	override string GetLabel()
	{
		MrFrost_InfoMenuConfig config = MrFrost_InfoMenuConfigLoader.Get();
		if (config && !config.m_sPauseMenuEntry.IsEmpty())
			return config.m_sPauseMenuEntry;

		return MrFrost_Text.Get("infomenu.title");
	}

	//------------------------------------------------------------------------------
	override ResourceName GetIconImageset()
	{
		MrFrost_InfoMenuConfig config = MrFrost_InfoMenuConfigLoader.Get();
		if (config)
			return config.m_MenuIconImageset;

		return string.Empty;
	}

	//------------------------------------------------------------------------------
	override string GetIconName()
	{
		MrFrost_InfoMenuConfig config = MrFrost_InfoMenuConfigLoader.Get();
		if (config)
			return config.m_sMenuIconName;

		return string.Empty;
	}

	//------------------------------------------------------------------------------
	//! A server with no categories at all has nothing to show, so the entry goes
	//! rather than opening an empty menu.
	override bool IsEnabled()
	{
		MrFrost_InfoMenuConfig config = MrFrost_InfoMenuConfigLoader.Get();
		return config && !config.m_aCategories.IsEmpty();
	}
}

//------------------------------------------------------------------------------
//! The report menu's pause menu entry.
class MrFrost_ReportEntryDescriptor : MrFrost_MenuEntry
{
	//------------------------------------------------------------------------------
	void MrFrost_ReportEntryDescriptor()
	{
		m_Preset = ChimeraMenuPreset.MrFrost_ReportMenu;
	}

	//------------------------------------------------------------------------------
	override string GetLabel()
	{
		return MrFrost_Text.Get("report.pause_entry");
	}

	//------------------------------------------------------------------------------
	override ResourceName GetIconImageset()
	{
		MrFrost_ReportConfig config = MrFrost_ReportConfigLoader.Get();
		if (!config)
			return string.Empty;

		return config.m_MenuIconImageset;
	}

	//------------------------------------------------------------------------------
	override string GetIconName()
	{
		MrFrost_ReportConfig config = MrFrost_ReportConfigLoader.Get();
		if (!config)
			return string.Empty;

		return config.m_sMenuIconName;
	}

	//------------------------------------------------------------------------------
	override bool IsEnabled()
	{
		return MrFrost_ReportConfigLoader.IsEnabled();
	}
}

//------------------------------------------------------------------------------
//! Registry of everything this addon contributes.
class MrFrost_Features
{
	protected static ref array<ref MrFrost_MenuEntry> s_aMenus;
	protected static bool s_bInitialised;

	//------------------------------------------------------------------------------
	//! Builds both lists. Safe to call as often as anyone likes; the first call
	//! does the work.
	//!
	//! Called from the client and from the server, and both must produce the same
	//! channel order — a transfer packet names its channel by index into that
	//! list. One function, called by both, is what guarantees that.
	static void Init()
	{
		if (s_bInitialised)
			return;

		s_bInitialised = true;

		s_aMenus = {};
		s_aMenus.Insert(new MrFrost_InfoMenuEntryDescriptor());
		s_aMenus.Insert(new MrFrost_ReportEntryDescriptor());

		// Order is the wire format. Append only.
		MrFrost_ServerContent.Register(new MrFrost_InfoMenuChannel());
		MrFrost_ServerContent.Register(new MrFrost_ReportChannel());
	}

	//------------------------------------------------------------------------------
	//! Menus that want an entry in the pause menu, in display order.
	static array<ref MrFrost_MenuEntry> GetMenus()
	{
		Init();
		return s_aMenus;
	}

	//------------------------------------------------------------------------------
	//! Client side: forget everything the last server sent.
	//!
	//! All of it lives in statics that outlive a connection, and a server that
	//! ships no file of its own sends nothing rather than an instruction to go
	//! back to the bundled content. Without this, a player who left one server
	//! and joined another without restarting the game carried the first server's
	//! rules, title, colours and Discord invite with them - and, if the first had
	//! switched reporting off, a report key that did nothing on a server that
	//! wanted reports.
	static void ForgetServerContent()
	{
		MrFrost_InfoMenuConfigLoader.ClearServerConfig();
		MrFrost_ReportConfigLoader.ClearServerConfig();
		MrFrost_Text.ClearOverrides();
		MrFrost_Log.SetVerbose(false);
	}

	//------------------------------------------------------------------------------
	//! Server side: reads every channel's file, which is also where each channel
	//! installs the settings it keeps to itself.
	static void LoadServerContent()
	{
		Init();

		foreach (MrFrost_ServerContentChannel channel : MrFrost_ServerContent.GetChannels())
		{
			MrFrost_ServerContent.Read(channel);
		}
	}
}
