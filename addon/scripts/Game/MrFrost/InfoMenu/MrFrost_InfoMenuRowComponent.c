//------------------------------------------------------------------------------
//! One clickable row in the info menu's category list.
//!
//! Rows are instanced from MrFrostInfoMenuRow.layout, one per visible category and
//! entry, and are driven entirely by the menu class. The component only knows
//! how to paint itself and how to report a click; which row is selected and what
//! that means is decided by MrFrost_InfoMenuUI.
//!
//! Why a hand-rolled row instead of an SCR_ list component: the engine has no
//! tree widget, and the list components that do exist are flat. A two-level
//! collapsible tree means modelling the expand/collapse state here regardless —
//! at which point a plain button row is both simpler and fully under our
//! control.
//------------------------------------------------------------------------------
class MrFrost_InfoMenuRowComponent : ScriptedWidgetComponent
{
	// Widget names inside MrFrostInfoMenuRow.layout.
	protected static const string WIDGET_HIGHLIGHT = "RowHighlight";
	protected static const string WIDGET_MARKER    = "RowMarker";
	protected static const string WIDGET_STEM_FULL = "TreeStemFull";
	protected static const string WIDGET_STEM_HALF = "TreeStemHalf";
	protected static const string WIDGET_BRANCH    = "TreeBranch";
	protected static const string WIDGET_ICON      = "RowIcon";
	protected static const string WIDGET_ICON_BOX  = "RowIconSize";
	protected static const string WIDGET_TEXT      = "RowText";
	protected static const string WIDGET_INDENT    = "RowIndent";

	//! Width the icon column takes when a row actually has an icon.
	protected static const float ICON_WIDTH = 22.0;

	//! Indent applied to entry rows so they read as children of their category.
	protected static const float ENTRY_INDENT = 13.0;

	// Row states reuse the game's own button palette rather than hand-picked
	// values, so the list reads exactly like the group menu's list and follows
	// along if Bohemia ever retunes those colours.
	//   hovered  -> CONTRAST_HOVERED   (gold, 40% alpha)
	//   selected -> the configured accent, full strength
	//   idle     -> no fill at all; the sidebar behind it shows through
	//! The selected row is filled with the accent colour, so its label has to flip
	//! to a dark tone — bright text on that fill has almost no contrast left.
	protected static const ref Color COLOR_TEXT_IDLE     = UIColors.NEUTRAL_ACTIVE_STANDBY;
	protected static const ref Color COLOR_TEXT_SELECTED = Color.FromSRGBA(18, 18, 18, 255);

	protected Widget m_wRoot;
	protected ImageWidget m_wHighlight;
	protected TextWidget m_wMarker;
	protected Widget m_wStemFull;
	protected Widget m_wStemHalf;
	protected Widget m_wBranch;
	protected ImageWidget m_wIcon;
	protected SizeLayoutWidget m_wIconBox;
	protected TextWidget m_wText;
	protected Widget m_wIndent;

	//! Index of the category this row belongs to.
	protected int m_iCategory = -1;

	//! Index of the entry within that category, or -1 when this row *is* the
	//! category.
	protected int m_iEntry = -1;

	protected bool m_bSelected;
	protected ref Color m_AccentColor;

	//! (MrFrost_InfoMenuRowComponent row)
	ref ScriptInvoker m_OnRowClicked = new ScriptInvoker();

	//! (MrFrost_InfoMenuRowComponent row) — raised when the row takes focus, which on
	//! a controller happens by simply moving the stick.
	ref ScriptInvoker m_OnRowFocused = new ScriptInvoker();

	//------------------------------------------------------------------------------
	override void HandlerAttached(Widget w)
	{
		m_wRoot = w;

		m_wHighlight = ImageWidget.Cast(w.FindAnyWidget(WIDGET_HIGHLIGHT));
		m_wMarker    = TextWidget.Cast(w.FindAnyWidget(WIDGET_MARKER));
		m_wStemFull  = w.FindAnyWidget(WIDGET_STEM_FULL);
		m_wStemHalf  = w.FindAnyWidget(WIDGET_STEM_HALF);
		m_wBranch    = w.FindAnyWidget(WIDGET_BRANCH);
		m_wIcon      = ImageWidget.Cast(w.FindAnyWidget(WIDGET_ICON));
		m_wIconBox   = SizeLayoutWidget.Cast(w.FindAnyWidget(WIDGET_ICON_BOX));
		m_wText      = TextWidget.Cast(w.FindAnyWidget(WIDGET_TEXT));
		m_wIndent    = w.FindAnyWidget(WIDGET_INDENT);

		PaintHighlight();
	}

	//------------------------------------------------------------------------------
	//! Paints the row. Passing entry = -1 marks it as a category row.
	//!
	//! \param lastChild only meaningful for entries: the last one under a
	//!        category ends the connector with an elbow instead of a tee.
	void SetUp(int category, int entry, notnull MrFrost_InfoMenuPage page, bool expandable, bool lastChild, notnull Color accent)
	{
		m_iCategory = category;
		m_iEntry = entry;
		m_AccentColor = accent;

		if (m_wText)
			m_wText.SetText(page.m_sName);

		bool isEntry = entry >= 0;

		// Categories carry a +/- marker; entries carry a tree connector instead.
		// "+"/"-" over a glyph like a triangle because the shipped UI fonts are
		// not guaranteed to carry geometric shapes, and a missing glyph renders
		// as a blank box.
		if (m_wMarker)
		{
			m_wMarker.SetVisible(expandable && !isEntry);
			if (expandable)
				m_wMarker.SetText("+");
		}

		// The connector is drawn from two thin bars rather than box-drawing
		// characters, for the same font reason. A middle child needs the stem to
		// run the full height so it joins the row below; the last child stops at
		// the branch.
		if (m_wStemFull)
			m_wStemFull.SetVisible(isEntry && !lastChild);

		if (m_wStemHalf)
			m_wStemHalf.SetVisible(isEntry && lastChild);

		if (m_wBranch)
			m_wBranch.SetVisible(isEntry);

		// Tree connectors take the palette's disabled tone: present enough to read
		// as structure, quiet enough not to compete with the labels.
		TintIfPresent(m_wStemFull, UIColors.IDLE_DISABLED);
		TintIfPresent(m_wStemHalf, UIColors.IDLE_DISABLED);
		TintIfPresent(m_wBranch, UIColors.IDLE_DISABLED);

		ApplyIcon(page);

		// Entries sit indented below their category, so the tree reads as a tree
		// rather than as a flat list with icons.
		SizeLayoutWidget indent = SizeLayoutWidget.Cast(m_wIndent);
		if (indent)
		{
			indent.EnableWidthOverride(true);
			if (entry >= 0)
				indent.SetWidthOverride(ENTRY_INDENT);
			else
				indent.SetWidthOverride(0);
		}

		PaintHighlight();
	}

	//------------------------------------------------------------------------------
	protected void TintIfPresent(Widget w, notnull Color color)
	{
		ImageWidget image = ImageWidget.Cast(w);
		if (image)
			image.SetColor(color);
	}

	//------------------------------------------------------------------------------
	//! An imageset sprite wins over a plain texture: it is the sharper option and
	//! the one the vanilla UI uses throughout.
	protected void ApplyIcon(notnull MrFrost_InfoMenuPage page)
	{
		if (!m_wIcon)
			return;

		bool hasIcon = false;

		if (!page.m_IconImageset.IsEmpty() && !page.m_sIconName.IsEmpty())
			hasIcon = m_wIcon.LoadImageFromSet(0, page.m_IconImageset, page.m_sIconName);
		else if (!page.m_Icon.IsEmpty())
			hasIcon = m_wIcon.LoadImageTexture(0, page.m_Icon);

		m_wIcon.SetVisible(hasIcon);

		// Collapse the icon column entirely on rows without one, so their labels
		// line up with the rest instead of hanging behind an empty gap.
		if (m_wIconBox)
		{
			m_wIconBox.EnableWidthOverride(true);
			if (hasIcon)
				m_wIconBox.SetWidthOverride(ICON_WIDTH);
			else
				m_wIconBox.SetWidthOverride(0);
		}
	}

	//------------------------------------------------------------------------------
	void SetExpanded(bool expanded)
	{
		if (!m_wMarker || !m_wMarker.IsVisible())
			return;

		if (expanded)
			m_wMarker.SetText("-");
		else
			m_wMarker.SetText("+");
	}

	//------------------------------------------------------------------------------
	void SetSelected(bool selected)
	{
		m_bSelected = selected;
		PaintHighlight();
	}

	//------------------------------------------------------------------------------
	//! Single place that decides how a row looks: the accent colour fills the
	//! selected row, a fainter wash marks the hovered one, nothing at rest.
	protected void PaintHighlight(bool hovered = false)
	{
		if (m_wText)
		{
			if (m_bSelected)
				m_wText.SetColor(COLOR_TEXT_SELECTED);
			else
				m_wText.SetColor(COLOR_TEXT_IDLE);
		}

		// Icon and connector have to flip too, or they vanish into the fill.
		Color detailColor = COLOR_TEXT_IDLE;
		if (m_bSelected)
			detailColor = COLOR_TEXT_SELECTED;

		if (m_wIcon)
			m_wIcon.SetColor(detailColor);

		if (m_wMarker)
			m_wMarker.SetColor(detailColor);

		if (!m_wHighlight)
			return;

		// The fill is always present and only its colour changes, so the state
		// change can be animated. Toggling visibility instead would make the row
		// pop, which is what the vanilla buttons deliberately avoid.
		m_wHighlight.SetVisible(true);

		if (m_bSelected)
		{
			if (m_AccentColor)
				AnimateWidget.Color(m_wHighlight, m_AccentColor, UIConstants.FADE_RATE_FAST);

			return;
		}

		if (hovered)
		{
			AnimateWidget.Color(m_wHighlight, UIColors.CONTRAST_HOVERED, UIConstants.FADE_RATE_FAST);
			return;
		}

		AnimateWidget.Color(m_wHighlight, UIColors.TRANSPARENT, UIConstants.FADE_RATE_FAST);
	}

	//------------------------------------------------------------------------------
	bool IsCategoryRow()
	{
		return m_iEntry < 0;
	}

	//------------------------------------------------------------------------------
	int GetCategoryIndex()
	{
		return m_iCategory;
	}

	//------------------------------------------------------------------------------
	int GetEntryIndex()
	{
		return m_iEntry;
	}

	//------------------------------------------------------------------------------
	Widget GetRootWidget()
	{
		return m_wRoot;
	}

	//------------------------------------------------------------------------------
	override bool OnClick(Widget w, int x, int y, int button)
	{
		if (button != 0)
			return false;

		m_OnRowClicked.Invoke(this);
		return true;
	}

	//------------------------------------------------------------------------------
	override bool OnFocus(Widget w, int x, int y)
	{
		// Controller navigation moves focus rather than clicking, so reading the
		// page has to follow focus or the right-hand panel would never change.
		m_OnRowFocused.Invoke(this);
		PaintHighlight(true);
		return false;
	}

	//------------------------------------------------------------------------------
	override bool OnFocusLost(Widget w, int x, int y)
	{
		PaintHighlight(false);
		return false;
	}

	//------------------------------------------------------------------------------
	override bool OnMouseEnter(Widget w, int x, int y)
	{
		PaintHighlight(true);
		return false;
	}

	//------------------------------------------------------------------------------
	override bool OnMouseLeave(Widget w, Widget enterW, int x, int y)
	{
		PaintHighlight(false);
		return false;
	}
}
