//------------------------------------------------------------------------------
//! Shared chrome for every MrFrost menu.
//!
//! All MrFrost menus are the same panel: blurred dark surface, title with an icon,
//! an accent hairline, a body, and the game's own footer prompt along the bottom.
//! Only the body differs. So the panel lives in one layout — MrFrostMenuFrame —
//! and a menu supplies just its body by overriding GetContentLayout().
//!
//! That split is why the frame is worth having: the colours, sizes, blur and
//! footer were taken from the vanilla group menu one value at a time. Copying
//! that per menu would mean every future menu drifting away from vanilla on its
//! own schedule.
//!
//! A subclass overrides GetContentLayout() to name its body, and OnMenuBuilt() to
//! fill it. Everything above happens before OnMenuBuilt() runs.
//------------------------------------------------------------------------------
class MrFrost_MenuBase : ChimeraMenuBase
{
	//! The exact button layout SCR_SuperMenuComponent hands to every stock menu's
	//! footer, so ours is the same widget the group menu and the settings menu
	//! use — same size, same spacing, same controller glyphs.
	protected static const ResourceName NAVIGATION_BUTTON_LAYOUT = "{08CF3B69CB1ACBC4}UI/layouts/WidgetLibrary/WLib_NavigationButton.layout";

	//! The stock back action, used plainly: the player's own Escape and gamepad
	//! bindings apply, and the prompt behaves like every other menu's.
	protected static const string ACTION_BACK = "MenuBack";

	// Widget names inside MrFrostMenuFrame.layout.
	protected static const string WIDGET_TITLE       = "MainTitle";
	protected static const string WIDGET_ICON        = "HeaderIcon";
	protected static const string WIDGET_ICON_BOX    = "HeaderIconSize";
	protected static const string WIDGET_SEPARATOR   = "FooterSeparator";
	protected static const string WIDGET_HEADER_LINE = "HeaderSeparator";
	protected static const string WIDGET_CONTENT     = "ContentRoot";

	protected Widget m_wRoot;
	protected Widget m_wContent;
	protected SCR_DynamicFooterComponent m_Footer;

	//------------------------------------------------------------------------------
	override void OnMenuOpen()
	{
		super.OnMenuOpen();

		// No SetActionContext() and no context pumping here on purpose.
		//
		// Not one vanilla menu calls SetActionContext. Menu actions live in
		// MenuContext, which the engine raises for whichever menu is open, and
		// that is all a footer prompt needs. Handing the menu a context of our own
		// invention replaced the one the engine wires up, and took every menu
		// action down with it — the prompts drew, the mouse worked, and no key
		// ever arrived.
		m_wRoot = GetRootWidget();
		if (!m_wRoot)
		{
			MrFrost_Log.Error("Menu opened without a root widget - layout missing or failed to load.");
			return;
		}

		if (!BuildContent())
			return;

		ApplyChrome();
		BuildFooter();

		OnMenuBuilt();

		FadeIn();
	}

	//------------------------------------------------------------------------------
	//! Instances this menu's body into the frame.
	protected bool BuildContent()
	{
		m_wContent = m_wRoot.FindAnyWidget(WIDGET_CONTENT);
		if (!m_wContent)
		{
			MrFrost_Log.Error("Frame layout has no '" + WIDGET_CONTENT + "' - cannot place the menu body.");
			return false;
		}

		ResourceName contentLayout = GetContentLayout();
		if (contentLayout.IsEmpty())
			return true;	// A menu is allowed to be frame-only.

		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (!workspace)
			return false;

		if (!workspace.CreateWidgets(contentLayout, m_wContent))
		{
			MrFrost_Log.Error("Could not create the menu body from " + contentLayout);
			return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------
	//! Paints the frame from the game's own palette.
	//!
	//! Done here rather than in the layout so every colour traces back to a named
	//! UIColors constant. The layout only carries placeholders, so the Workbench
	//! preview looks right — the preview renders layouts without running script.
	//! If Bohemia retunes the palette, the menus follow without an edit.
	protected void ApplyChrome()
	{
		ImageWidget separator = ImageWidget.Cast(m_wRoot.FindAnyWidget(WIDGET_SEPARATOR));
		if (separator)
			separator.SetColor(UIColors.WHITE_DEFAULT);

		TextWidget title = TextWidget.Cast(m_wRoot.FindAnyWidget(WIDGET_TITLE));
		if (title)
			title.SetColor(UIColors.NEUTRAL_INFORMATION);
	}

	//------------------------------------------------------------------------------
	//! Title and the icon in front of it.
	protected void SetHeader(string title, ResourceName iconImageset, string iconSprite)
	{
		TextWidget titleWidget = TextWidget.Cast(m_wRoot.FindAnyWidget(WIDGET_TITLE));
		if (titleWidget && !title.IsEmpty())
			titleWidget.SetText(title);

		ImageWidget icon = ImageWidget.Cast(m_wRoot.FindAnyWidget(WIDGET_ICON));
		if (!icon)
			return;

		bool hasIcon = !iconImageset.IsEmpty() && !iconSprite.IsEmpty();
		if (hasIcon)
			hasIcon = icon.LoadImageFromSet(0, iconImageset, iconSprite);

		icon.SetVisible(hasIcon);

		// Collapse the icon slot when there is nothing in it, so the title does
		// not sit behind an empty gap.
		Widget iconBox = m_wRoot.FindAnyWidget(WIDGET_ICON_BOX);
		if (iconBox)
			iconBox.SetVisible(hasIcon);
	}

	//------------------------------------------------------------------------------
	//! Hides the accent line under the title.
	//!
	//! A menu with tabs draws its own line below them instead, which is where
	//! vanilla puts it — two lines would be one too many.
	protected void ShowHeaderSeparator(bool show)
	{
		Widget separator = m_wRoot.FindAnyWidget(WIDGET_HEADER_LINE);
		if (separator)
			separator.SetVisible(show);
	}

	//------------------------------------------------------------------------------
	//! Adds the standard back prompt along the bottom edge.
	//!
	//! Built through the game's own footer component rather than by placing a
	//! button by hand. That component owns the button size, the spacing and the
	//! left/right grouping, and it resolves the prompt from the player's active
	//! input device — so the result is identical to every other menu instead of
	//! merely similar, controller glyphs on console included.
	protected void BuildFooter()
	{
		m_Footer = SCR_DynamicFooterComponent.FindComponentInHierarchy(m_wRoot);
		if (!m_Footer)
		{
			MrFrost_Log.Warn("Frame layout has no SCR_DynamicFooterComponent - no back prompt.");
			return;
		}

		// Localised so the prompt follows the player's game language instead of
		// being English everywhere. This is the game's own "Back" string.
		SCR_InputButtonComponent back = AddFooterButton("MrFrost_Close", "#AR-Menu_Back", ACTION_BACK, SCR_EDynamicFooterButtonAlignment.LEFT);
		if (back)
			back.m_OnActivated.Insert(OnCloseClicked);
	}

	//------------------------------------------------------------------------------
	//! Adds a prompt to the footer. Returns null when the footer is missing, so
	//! callers can chain without checking twice.
	protected SCR_InputButtonComponent AddFooterButton(string tag, string label, string action, SCR_EDynamicFooterButtonAlignment alignment)
	{
		if (!m_Footer)
			return null;

		SCR_InputButtonComponent button = m_Footer.CreateButton(NAVIGATION_BUTTON_LAYOUT, tag, label, action, alignment);
		if (!button)
		{
			MrFrost_Log.Warn("Could not create the footer button '" + tag + "'.");
			return null;
		}

		return button;
	}

	//------------------------------------------------------------------------------
	//! Fades the menu in instead of having it appear in one frame.
	//!
	//! Uses the game's own animation system and its FADE_RATE_FAST rate, so a
	//! MrFrost menu arrives at the same pace as every other menu rather than at a
	//! speed invented here.
	protected void FadeIn()
	{
		m_wRoot.SetOpacity(0);
		AnimateWidget.Opacity(m_wRoot, 1, UIConstants.FADE_RATE_FAST);
	}

	//------------------------------------------------------------------------------
	protected void OnCloseClicked()
	{
		Close();
	}

	//------------------------------------------------------------------------------
	//! Layout of this menu's body. Empty means the frame alone.
	protected ResourceName GetContentLayout()
	{
		return string.Empty;
	}

	//------------------------------------------------------------------------------
	//! Called once the frame and the body exist. Fill the body here.
	protected void OnMenuBuilt()
	{
	}
}
