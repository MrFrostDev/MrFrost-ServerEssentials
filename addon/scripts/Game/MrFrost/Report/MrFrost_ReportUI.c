//------------------------------------------------------------------------------
//! The report menu: a form, not a tree.
//!
//! The info menu is a reference work, so it gets a tree — you browse it. A report
//! is a task with three or four answers, so it gets a form: two buttons for what
//! you are reporting, dropdowns for who, a box for what happened, and a send
//! button. Nothing to expand, nothing to browse.
//!
//! Every control is a vanilla widget library one, so it looks and behaves like
//! the settings menu: WLib_ButtonText, WLib_ComboBox, WLib_EditBox. The panel
//! around it is the shared MrFrost frame.
//!
//! Rows that do not apply are hidden rather than disabled — a bug report has no
//! "who", and showing a greyed-out dropdown only invites people to click it.
//------------------------------------------------------------------------------
class MrFrost_ReportUI : MrFrost_MenuBase
{
	protected static const ResourceName CONTENT_LAYOUT   = "{7FA1C3D2E4B50605}UI/layouts/MrFrost/Report/MrFrostReport.layout";
	protected static const ResourceName TABVIEW_LAYOUT   = "{A971263DAE3AD8BC}UI/layouts/WidgetLibrary/TabView/WLib_TabViewHorizontal.layout";

	//! The tab view insists on a content layout per tab. The form below the bar is
	//! shared by both tabs and outlives a switch, so the tabs get an empty one and
	//! act purely as the selector.
	protected static const ResourceName EMPTY_LAYOUT     = "{7FA1C3D2E4B50606}UI/layouts/MrFrost/MrFrostEmpty.layout";
	protected static const ResourceName COMBO_LAYOUT     = "{4B5AE6E64037FFB4}UI/layouts/WidgetLibrary/ComboBox/WLib_ComboBox.layout";
	protected static const ResourceName EDIT_BOX_LAYOUT  = "{0022F0B45ADBC5AC}UI/layouts/WidgetLibrary/EditBox/WLib_EditBox.layout";

	//! Long enough to read the confirmation, short enough not to feel stuck.
	protected static const int CLOSE_AFTER_SEND_MS = 1200;

	//! Hold Enter to send.
	//!
	//! Declared in chimeraInputCommon.conf with InputFilterHoldOnce — the same
	//! filter class MenuAddGroup carries in the group menu. The prompt reads it
	//! off the action, draws its own fill, and activates only once the hold
	//! completes, on keyboard and controller alike.
	protected static const string ACTION_SUBMIT = "MrFrost_ReportSubmit";

	// Widget names inside MrFrostReport.layout.
	protected static const string WIDGET_TAB_SLOT     = "TabSlot";
	protected static const string WIDGET_TARGET_ROW   = "TargetRow";
	protected static const string WIDGET_PLAYER_ROW   = "PlayerRow";
	protected static const string WIDGET_DESC_LABEL   = "DescriptionLabel";
	protected static const string WIDGET_DESC_HINT    = "DescriptionHint";
	protected static const string WIDGET_DESC_ROW     = "DescriptionRow";
	protected static const string WIDGET_FORM_SEPARATOR = "FormSeparator";
	protected static const string WIDGET_STATUS       = "StatusLine";

	//! Inside WLib_TabViewHorizontal, not in our own layout.
	protected static const string WIDGET_TABVIEW_SEPARATOR = "Separator";
	protected static const string WIDGET_TABVIEW_CONTENT   = "ContentOverlay";

	protected SCR_TabViewComponent m_Tabs;

	//! Report kinds in tab order, so the selected tab maps back to one.
	protected ref array<MrFrost_EReportKind> m_aTabKinds = {};
	protected SCR_InputButtonComponent m_SubmitPrompt;
	protected SCR_ComboBoxComponent m_TargetCombo;
	protected SCR_ComboBoxComponent m_PlayerCombo;
	protected SCR_EditBoxComponent m_Description;

	protected TextWidget m_wStatus;
	protected Widget m_wTargetRow;
	protected Widget m_wPlayerRow;

	protected MrFrost_EReportKind m_eKind;


	//! Target modes in dropdown order, so the selected index maps back to one.
	protected ref array<MrFrost_EReportTarget> m_aTargetModes = {};

	//! Player ids in dropdown order, same idea.
	protected ref array<int> m_aPlayerIds = {};

	//------------------------------------------------------------------------------
	override protected ResourceName GetContentLayout()
	{
		return CONTENT_LAYOUT;
	}

	//------------------------------------------------------------------------------
	override protected void OnMenuBuilt()
	{
		MrFrost_ReportConfig config = MrFrost_ReportConfigLoader.Get();

		SetHeader(MrFrost_Text.Get("report.title"), config.m_MenuIconImageset, config.m_sMenuIconName);

		m_wStatus    = TextWidget.Cast(m_wRoot.FindAnyWidget(WIDGET_STATUS));
		m_wTargetRow = m_wRoot.FindAnyWidget(WIDGET_TARGET_ROW);
		m_wPlayerRow = m_wRoot.FindAnyWidget(WIDGET_PLAYER_ROW);

		// Vanilla puts the accent line below the tabs, not above them.
		ShowHeaderSeparator(false);

		ApplyPalette();
		ApplyLabels();

		BuildTabs(config);
		BuildTargetCombo(config);
		BuildPlayerCombo();
		BuildDescriptionBox();
		BuildSubmit();

		// Player reports are the common case, so the menu opens on them — unless
		// this server only takes bug reports.
		if (config.m_bAllowPlayerReports)
			SetKind(MrFrost_EReportKind.PLAYER);
		else
			SetKind(MrFrost_EReportKind.BUG);

		CloseDropdowns();

		// And once more after the menu has settled. A combo box builds its list
		// into the workspace rather than into itself, so a list that opened
		// during construction outlives the row it belongs to and hangs over the
		// form until something closes it.
		GetGame().GetCallqueue().CallLater(CloseDropdowns, 1, false);
	}

	//------------------------------------------------------------------------------
	protected void CloseDropdowns()
	{
		if (m_TargetCombo)
			m_TargetCombo.CloseList();

		if (m_PlayerCombo)
			m_PlayerCombo.CloseList();
	}

	//------------------------------------------------------------------------------
	protected void ApplyPalette()
	{
		TintText(WIDGET_DESC_LABEL, UIColors.NEUTRAL_INFORMATION);
		TintText(WIDGET_DESC_HINT, UIColors.NEUTRAL_ACTIVE_STANDBY);

		ImageWidget separator = ImageWidget.Cast(m_wRoot.FindAnyWidget(WIDGET_FORM_SEPARATOR));
		if (separator)
			separator.SetColor(UIColors.WHITE_DEFAULT);

		// The accent line under the tabs belongs to WLib_TabViewHorizontal itself.
		// Drawing our own next to it was what never lined up; making the tab
		// view's own line visible happens in BuildTabs.
	}

	//------------------------------------------------------------------------------
	protected void TintText(string widgetName, notnull Color color)
	{
		TextWidget text = TextWidget.Cast(m_wRoot.FindAnyWidget(widgetName));
		if (text)
			text.SetColor(color);
	}

	//------------------------------------------------------------------------------
	protected void SetTextFor(string widgetName, string key)
	{
		TextWidget text = TextWidget.Cast(m_wRoot.FindAnyWidget(widgetName));
		if (text)
			text.SetText(MrFrost_Text.Get(key));
	}

	//------------------------------------------------------------------------------
	protected void ApplyLabels()
	{
		SetTextFor(WIDGET_DESC_LABEL, "report.step_description");
		SetTextFor(WIDGET_DESC_HINT, "report.description_hint");
	}

	//------------------------------------------------------------------------------
	//! Makes the tab view's own accent line visible.
	//!
	//! WLib_TabViewHorizontal already carries the gold line under the tabs, so
	//! there is nothing to draw — but it sizes the line by fill weight, not in
	//! pixels. Its root is a vertical layout of three: a 40px tab row, then the
	//! Separator at weight 0.005 and the ContentOverlay at 0.95 sharing whatever
	//! is left over.
	//!
	//! In the group menu the tab view owns the entire panel, so half a percent of
	//! the leftover still comes to a few pixels. Here it owns 44 — the bar plus
	//! the line — and half a percent of the 4px remainder rounds to nothing. The
	//! line was being drawn all along, at sub-pixel height.
	//!
	//! This menu renders its form below the tab view rather than inside it, so
	//! the content overlay has nothing to show: the whole remainder goes to the
	//! line, which lands it at the same 4px vanilla authored it as.
	protected void ShowTabSeparator(notnull Widget tabView)
	{
		Widget separator = tabView.FindAnyWidget(WIDGET_TABVIEW_SEPARATOR);
		Widget content = tabView.FindAnyWidget(WIDGET_TABVIEW_CONTENT);

		if (!separator)
		{
			MrFrost_Log.Warn("Tab view carries no '" + WIDGET_TABVIEW_SEPARATOR + "' - no line under the tabs.");
			return;
		}

		LayoutSlot.SetFillWeight(separator, 1);

		if (content)
			LayoutSlot.SetFillWeight(content, 0);
	}

	//------------------------------------------------------------------------------
	//! Tabs for the report kind, the way every vanilla menu splits its sections.
	//!
	//! A kind the server has switched off gets no tab at all, so a server taking
	//! only bug reports shows one tab rather than one live and one dead. That is
	//! also why the tab index is mapped through m_aTabKinds instead of being cast
	//! to the enum: with one kind disabled, index 0 is not kind 0.
	protected void BuildTabs(notnull MrFrost_ReportConfig config)
	{
		Widget slot = m_wRoot.FindAnyWidget(WIDGET_TAB_SLOT);
		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (!slot || !workspace)
			return;

		Widget widget = workspace.CreateWidgets(TABVIEW_LAYOUT, slot);
		if (!widget)
		{
			MrFrost_Log.Warn("Could not create the tab bar.");
			return;
		}

		AlignableSlot.SetHorizontalAlign(widget, LayoutHorizontalAlign.Stretch);
		ShowTabSeparator(widget);

		m_Tabs = SCR_TabViewComponent.Cast(widget.FindHandler(SCR_TabViewComponent));
		if (!m_Tabs)
		{
			MrFrost_Log.Warn("Tab view layout carries no SCR_TabViewComponent.");
			return;
		}

		// Wrap around: with two tabs, either paging key should get you to the
		// other one, and both buttons stay live rather than one greying out.
		//
		// The left button being invisible was never about being disabled — the tab
		// bar hangs its paging buttons 48px *outside* its own bounds (see the
		// negative padding in WLib_TabViewHorizontal). The form's 40px margin was
		// narrower than that overhang, so the left one fell off the panel. The
		// margin is 56 now.
		m_Tabs.m_bCycleMode = true;

		if (config.m_bAllowPlayerReports)
		{
			m_Tabs.AddTab(EMPTY_LAYOUT, MrFrost_Text.Get("report.type_player"));
			m_aTabKinds.Insert(MrFrost_EReportKind.PLAYER);
		}

		if (config.m_bAllowBugReports)
		{
			m_Tabs.AddTab(EMPTY_LAYOUT, MrFrost_Text.Get("report.type_bug"));
			m_aTabKinds.Insert(MrFrost_EReportKind.BUG);
		}

		m_Tabs.GetOnChanged().Insert(OnTabChanged);

		// AddTab does not necessarily leave one selected, and an unselected tab
		// bar reads as broken. Pick the first explicitly.
		if (!m_aTabKinds.IsEmpty())
			m_Tabs.ShowTab(0, false, false);
	}

	//------------------------------------------------------------------------------
	protected void OnTabChanged(SCR_TabViewComponent tabView, Widget widget, int index)
	{
		if (index < 0 || index >= m_aTabKinds.Count())
			return;

		SetKind(m_aTabKinds[index]);
	}

	//------------------------------------------------------------------------------
	protected SCR_ComboBoxComponent CreateCombo(string slotName, string labelKey)
	{
		Widget slot = m_wRoot.FindAnyWidget(slotName);
		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (!slot || !workspace)
			return null;

		Widget widget = workspace.CreateWidgets(COMBO_LAYOUT, slot);
		if (!widget)
		{
			MrFrost_Log.Warn("Could not create the dropdown in '" + slotName + "'.");
			return null;
		}

		AlignableSlot.SetHorizontalAlign(widget, LayoutHorizontalAlign.Stretch);

		SCR_ComboBoxComponent combo = SCR_ComboBoxComponent.Cast(widget.FindHandler(SCR_ComboBoxComponent));
		if (!combo)
		{
			MrFrost_Log.Warn("Combo box layout carries no SCR_ComboBoxComponent.");
			return null;
		}

		// Emptied before anything is added. m_aElementNames is an [Attribute], so
		// whatever the layout was authored with is already in the list - and every
		// index this menu keeps, for target modes and for player ids, is an index
		// into it. One stray entry would shift all of them and quietly point a
		// report at the wrong player.
		combo.ClearAll();

		// The widget library layouts ship with placeholder labels baked in
		// ("Combo Box", "Editbox"). They are part of the row, not decoration, so
		// they are set rather than removed - that is what makes the form read
		// like the settings menu.
		combo.SetLabel(MrFrost_Text.Get(labelKey));
		return combo;
	}

	//------------------------------------------------------------------------------
	protected void BuildTargetCombo(notnull MrFrost_ReportConfig config)
	{
		m_TargetCombo = CreateCombo(WIDGET_TARGET_ROW, "report.step_target");
		if (!m_TargetCombo)
			return;

		AddTargetMode(MrFrost_EReportTarget.SELECTED, "report.target_select");
		AddTargetMode(MrFrost_EReportTarget.KILLER,   "report.target_killer");
		AddTargetMode(MrFrost_EReportTarget.ATTACKER, "report.target_attacker");
		AddTargetMode(MrFrost_EReportTarget.NEARBY,   "report.target_nearby", config.m_fNearbyRadius);

		m_TargetCombo.SetCurrentItem(0);
		m_TargetCombo.m_OnChanged.Insert(OnTargetChanged);
	}

	//------------------------------------------------------------------------------
	//! Adds one way of naming a target.
	//!
	//! "%1" in the label is the nearby radius. It is a placeholder rather than a
	//! number in the text because nearbyRadius is a server setting: a server
	//! running 500 m had every translation telling its players 300.
	protected void AddTargetMode(MrFrost_EReportTarget mode, string key, float radius = 0)
	{
		string label = MrFrost_Text.Get(key);

		if (radius > 0)
			label.Replace("%1", Math.Round(radius).ToString());

		m_TargetCombo.AddItem(label);
		m_aTargetModes.Insert(mode);
	}

	//------------------------------------------------------------------------------
	//! Fills the player dropdown from whoever is online right now.
	//!
	//! Built on open rather than kept up to date: a report is written in under a
	//! minute, and a list that reshuffles under the cursor while someone is
	//! picking is worse than one that is a few seconds stale. The server checks
	//! the pick against the live list anyway.
	protected void BuildPlayerCombo()
	{
		m_PlayerCombo = CreateCombo(WIDGET_PLAYER_ROW, "report.target_select");
		if (!m_PlayerCombo)
			return;

		PlayerManager playerManager = GetGame().GetPlayerManager();
		if (!playerManager)
			return;

		int ownId = 0;
		PlayerController controller = GetGame().GetPlayerController();
		if (controller)
			ownId = controller.GetPlayerId();

		array<int> players = {};
		playerManager.GetPlayers(players);

		foreach (int playerId : players)
		{
			if (playerId == ownId)
				continue;

			string name = playerManager.GetPlayerName(playerId);
			if (name.IsEmpty())
				continue;

			m_PlayerCombo.AddItem(name);
			m_aPlayerIds.Insert(playerId);
		}

		if (m_aPlayerIds.IsEmpty())
		{
			m_PlayerCombo.AddItem(MrFrost_Text.Get("report.no_players"));

			// Selected, or the closed box paints nothing. ClearAll leaves the index at
			// -1 and AddItem does not move it, so the one line written to explain an
			// empty list was only visible to a player who opened the list to look.
			m_PlayerCombo.SetCurrentItem(0);
			return;
		}

		m_PlayerCombo.SetCurrentItem(0);
	}

	//------------------------------------------------------------------------------
	//! Drops the game's own edit box into the form.
	//!
	//! The vanilla widget rather than a bare EditBoxWidget: it brings the focus
	//! behaviour, the caret and the on-screen keyboard console needs, none of
	//! which is worth reimplementing.
	protected void BuildDescriptionBox()
	{
		Widget slot = m_wRoot.FindAnyWidget(WIDGET_DESC_ROW);
		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (!slot || !workspace)
			return;

		Widget box = workspace.CreateWidgets(EDIT_BOX_LAYOUT, slot);
		if (!box)
		{
			MrFrost_Log.Warn("Could not create the description box.");
			return;
		}

		AlignableSlot.SetHorizontalAlign(box, LayoutHorizontalAlign.Stretch);

		m_Description = SCR_EditBoxComponent.Cast(box.FindHandler(SCR_EditBoxComponent));
		if (!m_Description)
		{
			MrFrost_Log.Warn("Edit box layout carries no SCR_EditBoxComponent.");
			return;
		}

		m_Description.m_OnChanged.Insert(OnDescriptionChanged);

		// No label on this one: the heading above it already asks the question,
		// and dropping the label gives the field the full width it needs.
		m_Description.UseLabel(false);
	}

	//------------------------------------------------------------------------------
	//! Send lives only in the footer, and only as a hold.
	//!
	//! A report cannot be recalled once it reaches a moderator, so it does not sit
	//! one stray click away from a text field.
	protected void BuildSubmit()
	{
		m_SubmitPrompt = AddFooterButton(
			"MrFrost_ReportSubmit",
			MrFrost_Text.Get("report.submit"),
			ACTION_SUBMIT,
			SCR_EDynamicFooterButtonAlignment.RIGHT);

		if (!m_SubmitPrompt)
			return;

		// A hold action only activates once the hold completes, so this is the
		// whole wiring - the same line SCR_GroupSubMenuBase uses for its own.
		m_SubmitPrompt.m_OnActivated.Insert(OnSubmit);

		UpdateSubmitEnabled();

	}

	//------------------------------------------------------------------------------
	protected void OnDescriptionChanged(SCR_EditBoxComponent editBox, string text)
	{
		UpdateSubmitEnabled();
	}

	//------------------------------------------------------------------------------
	//! Nothing written, nothing to send.
	//!
	//! The group menu greys its create button out when there is nothing to create;
	//! this is the same idea. A disabled prompt refuses its own action, so this is
	//! the guard as much as it is the hint.
	protected void UpdateSubmitEnabled()
	{
		if (!m_SubmitPrompt)
			return;

		m_SubmitPrompt.SetEnabled(m_Description && !m_Description.GetValue().Trim().IsEmpty());
	}

	//------------------------------------------------------------------------------
	//! Switches between the two kinds and shows only what that kind needs.
	protected void SetKind(MrFrost_EReportKind kind)
	{
		m_eKind = kind;

		bool isPlayerReport = (kind == MrFrost_EReportKind.PLAYER);

		if (m_wTargetRow)
			m_wTargetRow.SetVisible(isPlayerReport);

		UpdatePlayerRow();
		SetStatus(string.Empty);
	}

	//------------------------------------------------------------------------------
	protected void OnTargetChanged(SCR_ComboBoxComponent combo, int index)
	{
		UpdatePlayerRow();
		SetStatus(string.Empty);
	}

	//------------------------------------------------------------------------------
	//! The player dropdown only means anything for "pick a player".
	protected void UpdatePlayerRow()
	{
		if (!m_wPlayerRow)
			return;

		bool visible = (m_eKind == MrFrost_EReportKind.PLAYER) && (GetTargetMode() == MrFrost_EReportTarget.SELECTED);
		m_wPlayerRow.SetVisible(visible);
	}

	//------------------------------------------------------------------------------
	protected MrFrost_EReportTarget GetTargetMode()
	{
		if (m_eKind == MrFrost_EReportKind.BUG)
			return MrFrost_EReportTarget.NONE;

		if (!m_TargetCombo)
			return MrFrost_EReportTarget.SELECTED;

		int index = m_TargetCombo.GetCurrentIndex();
		if (index < 0 || index >= m_aTargetModes.Count())
			return MrFrost_EReportTarget.SELECTED;

		return m_aTargetModes[index];
	}

	//------------------------------------------------------------------------------
	protected int GetSelectedPlayerId()
	{
		if (!m_PlayerCombo)
			return 0;

		int index = m_PlayerCombo.GetCurrentIndex();
		if (index < 0 || index >= m_aPlayerIds.Count())
			return 0;

		return m_aPlayerIds[index];
	}

	//------------------------------------------------------------------------------
	protected void SetStatus(string text)
	{
		if (m_wStatus)
			m_wStatus.SetText(text);
	}

	//------------------------------------------------------------------------------
	protected void OnSubmit()
	{
		string description;
		if (m_Description)
			description = m_Description.GetValue();

		// Checked here as well as on the server, so the common mistake costs a
		// glance rather than a round trip.
		if (description.Trim().IsEmpty())
		{
			SetStatus(MrFrost_Text.Get("report.need_description"));
			return;
		}

		MrFrost_EReportTarget mode = GetTargetMode();

		if (m_eKind == MrFrost_EReportKind.PLAYER && mode == MrFrost_EReportTarget.SELECTED && m_aPlayerIds.IsEmpty())
		{
			SetStatus(MrFrost_Text.Get("report.no_players"));
			return;
		}

		SCR_PlayerController controller = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (!controller)
			return;

		// The target is named by mode, not by identity, for everything except an
		// explicit pick — which the server checks against the live player list.
		controller.MrFrost_SendReport(m_eKind, mode, GetSelectedPlayerId(), description);
	}

	//------------------------------------------------------------------------------
	//! The lists live in the workspace, not in this menu, so closing the menu
	//! would otherwise leave one floating over whatever comes next.
	override void OnMenuClose()
	{
		GetGame().GetCallqueue().Remove(CloseDropdowns);
		GetGame().GetCallqueue().Remove(Close);
		CloseDropdowns();

		super.OnMenuClose();
	}

	//------------------------------------------------------------------------------
	//! Set once a report has been accepted, so a later answer cannot undo it.
	protected bool m_bAnswered;

	//------------------------------------------------------------------------------
	//! Called from the player controller once the server has ruled on a report.
	void OnServerAnswer(string textKey)
	{
		// A report that already went through is final. Two activations inside the
		// server's flood floor produce two answers, and the second - a refusal -
		// would otherwise land on top of the first and leave the menu open,
		// telling a player their filed report had bounced.
		if (m_bAnswered)
			return;

		SetStatus(MrFrost_Text.Get(textKey));

		// A rejected report leaves the text where it is, so the player can fix it
		// and try again. A filed one is done with - clear it and get out of the
		// way, after a beat so the confirmation is actually read.
		if (textKey != "report.sent")
			return;

		m_bAnswered = true;

		if (m_Description)
			m_Description.SetValue(string.Empty);

		UpdateSubmitEnabled();

		GetGame().GetCallqueue().CallLater(Close, CLOSE_AFTER_SEND_MS, false);
	}

	//------------------------------------------------------------------------------
	static void Toggle()
	{
		if (!MrFrost_ReportConfigLoader.IsEnabled())
			return;

		MenuManager menuManager = GetGame().GetMenuManager();
		if (!menuManager)
			return;

		MenuBase existing = menuManager.FindMenuByPreset(ChimeraMenuPreset.MrFrost_ReportMenu);
		if (existing)
		{
			existing.Close();
			return;
		}

		menuManager.OpenMenu(ChimeraMenuPreset.MrFrost_ReportMenu);
	}
}
