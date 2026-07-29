//------------------------------------------------------------------------------
//! The info menu: category tree on the left, formatted text on the right.
//!
//! Two columns: what there is to read on the left, what it says on the right.
//!
//! Only the body is built here. Panel, header, footer and fade come from
//! MrFrost_MenuBase and are shared with every other MrFrost menu.
//!
//! Structure note: every row (category and entry alike) is created once when the
//! menu opens and then only shown or hidden. Collapsing a category does not
//! destroy its rows. That costs a handful of widgets for a config of this size and
//! buys back all the bookkeeping that rebuilding the list on every click would
//! need — index remapping, re-resolving the selection, re-attaching invokers.
//------------------------------------------------------------------------------
class MrFrost_InfoMenuUI : MrFrost_MenuBase
{
	//! Two-column body placed into the shared frame.
	protected static const ResourceName CONTENT_LAYOUT = "{7FA1C3D2E4B50604}UI/layouts/MrFrost/InfoMenu/MrFrostInfoMenu.layout";

	//! Layout instanced once per row in the list.
	protected static const ResourceName ROW_LAYOUT = "{7FA1C3D2E4B50603}UI/layouts/MrFrost/InfoMenu/MrFrostInfoMenuRow.layout";

	//! Declared in chimeraInputCommon.conf inside the shared menu context, so it
	//! only fires while a MrFrost menu is up and never steals D from movement.
	protected static const string ACTION_DISCORD = "MrFrost_MenuDiscord";
	protected static const string ACTION_WEBSITE = "MrFrost_MenuWebsite";
	protected static const string ACTION_CUSTOM  = "MrFrost_MenuCustom";

	// Widget names inside MrFrostInfoMenu.layout.
	protected static const string WIDGET_ENTRY_TITLE = "EntryTitle";
	protected static const string WIDGET_TREE_LIST   = "TreeList";
	protected static const string WIDGET_TEXT        = "ContentText";
	protected static const string WIDGET_SCROLL      = "ContentScroll";
	protected static const string WIDGET_SIDEBAR     = "SidebarBackground";

	protected ref MrFrost_InfoMenuConfig m_Config;

	protected VerticalLayoutWidget m_wTreeList;
	protected TextWidget m_wEntryTitle;
	protected RichTextWidget m_wText;
	protected ScrollLayoutWidget m_wScroll;

	//! Every row in display order, categories and entries interleaved.
	protected ref array<ref MrFrost_InfoMenuRowComponent> m_aRows = {};

	//! Entry rows per category, so expanding one only touches its own children.
	protected ref map<int, ref array<MrFrost_InfoMenuRowComponent>> m_mEntryRows = new map<int, ref array<MrFrost_InfoMenuRowComponent>>();

	//! Category rows by category index, for the expand marker.
	protected ref map<int, MrFrost_InfoMenuRowComponent> m_mCategoryRows = new map<int, MrFrost_InfoMenuRowComponent>();

	protected ref set<int> m_aExpanded = new set<int>();

	protected MrFrost_InfoMenuRowComponent m_SelectedRow;

	//------------------------------------------------------------------------------
	override protected ResourceName GetContentLayout()
	{
		return CONTENT_LAYOUT;
	}

	//------------------------------------------------------------------------------
	override protected void OnMenuBuilt()
	{
		m_wTreeList   = VerticalLayoutWidget.Cast(m_wRoot.FindAnyWidget(WIDGET_TREE_LIST));
		m_wEntryTitle = TextWidget.Cast(m_wRoot.FindAnyWidget(WIDGET_ENTRY_TITLE));
		m_wText       = RichTextWidget.Cast(m_wRoot.FindAnyWidget(WIDGET_TEXT));
		m_wScroll     = ScrollLayoutWidget.Cast(m_wRoot.FindAnyWidget(WIDGET_SCROLL));

		if (!m_wTreeList || !m_wText)
		{
			MrFrost_Log.Error("Info menu body is missing '" + WIDGET_TREE_LIST + "' or '" + WIDGET_TEXT + "'.");
			return;
		}

		ApplyPalette();

		m_Config = MrFrost_InfoMenuConfigLoader.Get();
		if (!m_Config)
		{
			MrFrost_Log.Error("Could not load the info menu config from " + MrFrost_InfoMenuConfigLoader.CONFIG_PATH);
			ShowPlaceholder(MrFrost_Text.Get("infomenu.load_failed"));
			return;
		}

		// A server that set no title falls back to a translated word rather than
		// an English one.
		string title = m_Config.m_sTitle;
		if (title.IsEmpty())
			title = MrFrost_Text.Get("infomenu.title");

		SetHeader(title, m_Config.m_MenuIconImageset, m_Config.m_sMenuIconName);

		BuildLinkButtons();
		BuildRows();
		SelectFirstRow();

		MrFrost_Log.Debug("Info menu opened with " + m_aRows.Count() + " row(s).");
	}

	//------------------------------------------------------------------------------
	//! Body colours that the frame does not own.
	protected void ApplyPalette()
	{
		// Column behind the tree. The pause menu puts its button column on black
		// at 39%, which is BACKGROUND_DEFAULT to the decimal — so that is the
		// wash a vanilla side column gets, not the heavier hovered one.
		ImageWidget sidebar = ImageWidget.Cast(m_wRoot.FindAnyWidget(WIDGET_SIDEBAR));
		if (sidebar)
			sidebar.SetColor(UIColors.BACKGROUND_DEFAULT);

		// Body copy sits a step below pure white, as everywhere else in the game.
		if (m_wText)
			m_wText.SetColor(UIColors.NEUTRAL_ACTIVE_STANDBY);

		if (m_wEntryTitle)
			m_wEntryTitle.SetColor(UIColors.NEUTRAL_INFORMATION);
	}

	//------------------------------------------------------------------------------
	//! Adds the link prompts on the right, mirroring how the group menu keeps its
	//! secondary actions there.
	//!
	//! Three slots, each independent: Discord, a website, and one a server owner
	//! names itself. A slot with no URL draws no button, so a server that runs
	//! only a Discord gets one prompt rather than two dead ones beside it.
	//!
	//! They are built in a fixed order so the footer does not reshuffle when a
	//! server fills in a slot it had left empty.
	//! The wiring is spelled out per slot rather than passed to a shared helper:
	//! EnforceScript rejects a function reference as a script method parameter
	//! ("func arguments are not supported in script methods"), so the helper can
	//! only decide *whether* to draw a button and hand it back.
	protected void BuildLinkButtons()
	{
		InputManager inputManager = GetGame().GetInputManager();
		SCR_InputButtonComponent button;

		button = BuildLinkButton(m_Config.m_sDiscordUrl, m_Config.m_sDiscordLabel, "Discord", "MrFrost_Discord", ACTION_DISCORD);
		if (button)
		{
			button.m_OnActivated.Insert(OnDiscordClicked);
			if (inputManager)
				inputManager.AddActionListener(ACTION_DISCORD, EActionTrigger.DOWN, OnDiscordClicked);
		}

		button = BuildLinkButton(m_Config.m_sWebsiteUrl, m_Config.m_sWebsiteLabel, "Website", "MrFrost_Website", ACTION_WEBSITE);
		if (button)
		{
			button.m_OnActivated.Insert(OnWebsiteClicked);
			if (inputManager)
				inputManager.AddActionListener(ACTION_WEBSITE, EActionTrigger.DOWN, OnWebsiteClicked);
		}

		// No fallback label for this one: the other two are named after what they
		// are, while this slot is whatever the server made it. A prompt reading
		// "Custom" tells a player nothing, so an unlabelled slot stays hidden.
		button = BuildLinkButton(m_Config.m_sCustomUrl, m_Config.m_sCustomLabel, string.Empty, "MrFrost_Custom", ACTION_CUSTOM);
		if (button)
		{
			button.m_OnActivated.Insert(OnCustomClicked);
			if (inputManager)
				inputManager.AddActionListener(ACTION_CUSTOM, EActionTrigger.DOWN, OnCustomClicked);
		}
	}

	//------------------------------------------------------------------------------
	//! Draws one footer prompt, or nothing when the slot is unused. Returns the
	//! button so the caller can attach its own handler — see BuildLinkButtons().
	protected SCR_InputButtonComponent BuildLinkButton(string url, string label, string fallbackLabel, string widgetName, string action)
	{
		if (url.IsEmpty())
			return null;

		if (label.IsEmpty())
			label = fallbackLabel;

		if (label.IsEmpty())
			return null;

		// See MrFrost_ReportUI.BuildSubmit(): a runtime-created prompt never gets
		// its keybind flag set, so the key half of it is wired by the caller.
		return AddFooterButton(widgetName, label, action, SCR_EDynamicFooterButtonAlignment.RIGHT);
	}

	//------------------------------------------------------------------------------
	//! Removing a listener that was never added is harmless, so all three come off
	//! regardless of which ones the config put on.
	override void OnMenuClose()
	{
		InputManager inputManager = GetGame().GetInputManager();
		if (inputManager)
		{
			inputManager.RemoveActionListener(ACTION_DISCORD, EActionTrigger.DOWN, OnDiscordClicked);
			inputManager.RemoveActionListener(ACTION_WEBSITE, EActionTrigger.DOWN, OnWebsiteClicked);
			inputManager.RemoveActionListener(ACTION_CUSTOM, EActionTrigger.DOWN, OnCustomClicked);
		}

		super.OnMenuClose();
	}

	//------------------------------------------------------------------------------
	protected void OnDiscordClicked()
	{
		OpenLink(m_Config.m_sDiscordUrl, "Discord");
	}

	//------------------------------------------------------------------------------
	protected void OnWebsiteClicked()
	{
		OpenLink(m_Config.m_sWebsiteUrl, "website");
	}

	//------------------------------------------------------------------------------
	protected void OnCustomClicked()
	{
		OpenLink(m_Config.m_sCustomUrl, "custom");
	}

	//------------------------------------------------------------------------------
	//! Hands the URL to the platform, which is what makes these work on console:
	//! a link in a text widget cannot be clicked there, an overlay opened by the
	//! platform can.
	protected void OpenLink(string url, string what)
	{
		if (!m_Config || url.IsEmpty())
			return;

		PlatformService platformService = GetGame().GetPlatformService();
		if (!platformService)
		{
			MrFrost_Log.Warn("No platform service - cannot open the " + what + " link.");
			return;
		}

		MrFrost_Log.Debug("Opening the " + what + " link.");
		platformService.OpenBrowser(url);
	}

	//------------------------------------------------------------------------------
	//! Creates one row per enabled category and per enabled entry below it.
	protected void BuildRows()
	{
		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (!workspace)
			return;

		Color accent = m_Config.GetAccentColor();

		for (int c = 0, categoryCount = m_Config.m_aCategories.Count(); c < categoryCount; c++)
		{
			MrFrost_InfoMenuCategory category = m_Config.m_aCategories[c];
			if (!category || !category.m_bEnabled)
				continue;

			MrFrost_InfoMenuRowComponent categoryRow = CreateRow(workspace);
			if (!categoryRow)
				continue;

			// Which entries are actually shown has to be known up front: the last
			// visible one closes the tree connector with an elbow, and a disabled
			// entry at the end must not leave a dangling line.
			array<int> visibleEntries = {};
			for (int probe = 0, probeCount = category.m_aEntries.Count(); probe < probeCount; probe++)
			{
				MrFrost_InfoMenuEntry candidate = category.m_aEntries[probe];
				if (candidate && candidate.m_bEnabled)
					visibleEntries.Insert(probe);
			}

			bool hasEntries = !visibleEntries.IsEmpty();
			categoryRow.SetUp(c, -1, category, hasEntries, false, accent);
			m_mCategoryRows.Set(c, categoryRow);

			array<MrFrost_InfoMenuRowComponent> entryRows = {};

			for (int i = 0, visibleCount = visibleEntries.Count(); i < visibleCount; i++)
			{
				int e = visibleEntries[i];

				MrFrost_InfoMenuRowComponent entryRow = CreateRow(workspace);
				if (!entryRow)
					continue;

				bool lastChild = (i == visibleCount - 1);
				entryRow.SetUp(c, e, category.m_aEntries[e], false, lastChild, accent);
				entryRows.Insert(entryRow);
			}

			m_mEntryRows.Set(c, entryRows);

			// Honour the per-category default, then paint the marker and the
			// child rows to match.
			if (category.m_bExpandedByDefault && hasEntries)
				m_aExpanded.Insert(c);

			ApplyExpansion(c);
		}
	}

	//------------------------------------------------------------------------------
	//! Instances one row layout into the list and wires up its click.
	protected MrFrost_InfoMenuRowComponent CreateRow(notnull WorkspaceWidget workspace)
	{
		Widget rowWidget = workspace.CreateWidgets(ROW_LAYOUT, m_wTreeList);
		if (!rowWidget)
		{
			MrFrost_Log.Error("Could not create a row from " + ROW_LAYOUT);
			return null;
		}

		MrFrost_InfoMenuRowComponent row = MrFrost_InfoMenuRowComponent.Cast(rowWidget.FindHandler(MrFrost_InfoMenuRowComponent));
		if (!row)
		{
			MrFrost_Log.Error("Row layout has no MrFrost_InfoMenuRowComponent attached.");
			return null;
		}

		row.m_OnRowClicked.Insert(OnRowClicked);
		row.m_OnRowFocused.Insert(OnRowFocused);
		m_aRows.Insert(row);

		return row;
	}

	//------------------------------------------------------------------------------
	protected void OnRowClicked(MrFrost_InfoMenuRowComponent row)
	{
		if (!row)
			return;

		int categoryIndex = row.GetCategoryIndex();

		// Clicking a category both selects it and toggles its children: a
		// category has its own page, so a click that only expanded would leave
		// that page unreachable.
		if (row.IsCategoryRow())
		{
			if (m_aExpanded.Contains(categoryIndex))
				m_aExpanded.RemoveItem(categoryIndex);
			else
				m_aExpanded.Insert(categoryIndex);

			ApplyExpansion(categoryIndex);
		}

		Select(row);
	}

	//------------------------------------------------------------------------------
	//! Focus follows the controller stick, so reading follows focus. Deliberately
	//! does not expand anything: only a real press should collapse or expand a
	//! category, otherwise merely scrolling past one would reshuffle the list
	//! under the player.
	protected void OnRowFocused(MrFrost_InfoMenuRowComponent row)
	{
		if (row)
			Select(row);
	}

	//------------------------------------------------------------------------------
	//! Shows or hides a category's entry rows and updates its +/- marker.
	protected void ApplyExpansion(int categoryIndex)
	{
		bool expanded = m_aExpanded.Contains(categoryIndex);

		MrFrost_InfoMenuRowComponent categoryRow = m_mCategoryRows.Get(categoryIndex);
		if (categoryRow)
			categoryRow.SetExpanded(expanded);

		array<MrFrost_InfoMenuRowComponent> entryRows = m_mEntryRows.Get(categoryIndex);
		if (!entryRows)
			return;

		foreach (MrFrost_InfoMenuRowComponent entryRow : entryRows)
		{
			Widget entryWidget = entryRow.GetRootWidget();
			if (entryWidget)
				entryWidget.SetVisible(expanded);
		}
	}

	//------------------------------------------------------------------------------
	protected void Select(notnull MrFrost_InfoMenuRowComponent row)
	{
		if (m_SelectedRow == row)
		{
			// Re-selecting the current row still repaints the text: a collapsed
			// category that is clicked again should show its own page.
			ShowContentFor(row);
			return;
		}

		if (m_SelectedRow)
			m_SelectedRow.SetSelected(false);

		m_SelectedRow = row;
		m_SelectedRow.SetSelected(true);

		ShowContentFor(row);
	}

	//------------------------------------------------------------------------------
	//! Paints the right-hand panel from whatever the row points at.
	protected void ShowContentFor(notnull MrFrost_InfoMenuRowComponent row)
	{
		int categoryIndex = row.GetCategoryIndex();
		if (categoryIndex < 0 || categoryIndex >= m_Config.m_aCategories.Count())
			return;

		MrFrost_InfoMenuCategory category = m_Config.m_aCategories[categoryIndex];
		if (!category)
			return;

		string title;
		string text;

		if (row.IsCategoryRow())
		{
			title = category.m_sTitle;
			if (title.IsEmpty())
				title = category.m_sName;

			text = category.m_sText;
		}
		else
		{
			int entryIndex = row.GetEntryIndex();
			if (entryIndex < 0 || entryIndex >= category.m_aEntries.Count())
				return;

			MrFrost_InfoMenuEntry entry = category.m_aEntries[entryIndex];
			if (!entry)
				return;

			title = entry.m_sTitle;
			if (title.IsEmpty())
				title = entry.m_sName;

			text = entry.m_sText;
		}

		if (m_wEntryTitle)
			m_wEntryTitle.SetText(title);

		m_wText.SetText(text);

		// Every page starts at the top; otherwise the reader lands mid-way down
		// the new text after scrolling through a long previous one.
		if (m_wScroll)
			m_wScroll.SetSliderPos(0, 0);
	}

	//------------------------------------------------------------------------------
	protected void SelectFirstRow()
	{
		if (m_aRows.IsEmpty())
		{
			ShowPlaceholder(MrFrost_Text.Get("infomenu.empty"));
			return;
		}

		Select(m_aRows[0]);

		// Hand the first row keyboard/controller focus. Without this the menu is
		// mouse-only: a gamepad has nothing to move from, so the stick and d-pad
		// do nothing at all on console.
		WorkspaceWidget workspace = GetGame().GetWorkspace();
		Widget firstWidget = m_aRows[0].GetRootWidget();
		if (workspace && firstWidget)
			workspace.SetFocusedWidget(firstWidget);
	}

	//------------------------------------------------------------------------------
	//! Puts a message in the text panel when there is nothing to show. Keeps the
	//! menu usable (and diagnosable) instead of opening blank.
	protected void ShowPlaceholder(string message)
	{
		if (m_wEntryTitle)
			m_wEntryTitle.SetText(string.Empty);

		if (m_wText)
			m_wText.SetText(message);
	}

	//------------------------------------------------------------------------------
	//! Opens the info menu, or closes it when it is already up, so the same key
	//! can toggle it.
	static void Toggle()
	{
		MenuManager menuManager = GetGame().GetMenuManager();
		if (!menuManager)
			return;

		MenuBase existing = menuManager.FindMenuByPreset(ChimeraMenuPreset.MrFrost_InfoMenu);
		if (existing)
		{
			existing.Close();
			return;
		}

		menuManager.OpenMenu(ChimeraMenuPreset.MrFrost_InfoMenu);
	}
}
