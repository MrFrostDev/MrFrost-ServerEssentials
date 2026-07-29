//------------------------------------------------------------------------------
//! Adds one pause menu entry per MrFrost menu, next to the Field Manual.
//!
//! The buttons are created at runtime and parented to whatever container already
//! holds the vanilla Field Manual button. Deliberately *not* done by shipping our
//! own copy of pauseMenu.layout: overriding that asset would silently drop every
//! change Bohemia makes to it, and would collide with any other mod doing the
//! same. A modded class only adds behaviour, so several mods can each add their
//! own buttons without fighting over one file.
//!
//! If the vanilla button ever disappears or is renamed, nothing breaks — the
//! insert is skipped and the keybinds still open the menus.
//------------------------------------------------------------------------------
modded class PauseMenuUI
{
	//! Vanilla pause menu button. Reused so the entries match their neighbours in
	//! font, size, focus behaviour and gamepad navigation for free.
	protected static const ResourceName MRFROST_BUTTON_LAYOUT = "{9ECCD201BCF07E95}UI/layouts/Menus/PauseMenu/PauseMenuButton.layout";

	//! Anchor: the button we place ourselves next to.
	protected static const string MRFROST_ANCHOR_BUTTON = "FieldManual";

	//! Which menu each created button opens. Parallel to the created widgets,
	//! because the vanilla button component carries no payload of its own.
	protected ref map<SCR_ButtonBaseComponent, ref MrFrost_MenuEntry> m_mMrFrostButtons;

	//------------------------------------------------------------------------------
	override void OnMenuOpen()
	{
		super.OnMenuOpen();
		MrFrost_InsertEntries();
	}

	//------------------------------------------------------------------------------
	protected void MrFrost_InsertEntries()
	{
		if (!m_wRoot)
			return;

		SCR_ButtonTextComponent anchor = SCR_ButtonTextComponent.GetButtonText(MRFROST_ANCHOR_BUTTON, m_wRoot);
		if (!anchor)
		{
			MrFrost_Log.Debug("Pause menu has no '" + MRFROST_ANCHOR_BUTTON + "' button - skipping the MrFrost entries.");
			return;
		}

		Widget anchorWidget = anchor.GetRootWidget();
		if (!anchorWidget)
			return;

		Widget container = anchorWidget.GetParent();
		if (!container)
			return;

		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (!workspace)
			return;

		m_mMrFrostButtons = new map<SCR_ButtonBaseComponent, ref MrFrost_MenuEntry>();

		foreach (MrFrost_MenuEntry entry : MrFrost_Features.GetMenus())
		{
			if (entry && entry.IsEnabled())
				MrFrost_InsertEntry(workspace, container, entry);
		}
	}

	//------------------------------------------------------------------------------
	protected void MrFrost_InsertEntry(notnull WorkspaceWidget workspace, notnull Widget container, notnull MrFrost_MenuEntry entry)
	{
		Widget buttonWidget = workspace.CreateWidgets(MRFROST_BUTTON_LAYOUT, container);
		if (!buttonWidget)
		{
			MrFrost_Log.Warn("Could not create a pause menu button.");
			return;
		}

		SCR_ButtonTextComponent button = SCR_ButtonTextComponent.Cast(buttonWidget.FindHandler(SCR_ButtonTextComponent));
		if (!button)
		{
			MrFrost_Log.Warn("Pause menu button layout carries no SCR_ButtonTextComponent.");
			return;
		}

		button.SetText(entry.GetLabel());
		button.m_OnClicked.Insert(MrFrost_OnEntryClicked);

		m_mMrFrostButtons.Set(button, entry);

		MrFrost_ApplyIcon(buttonWidget, entry);

		MrFrost_Log.Debug("Pause menu entry '" + entry.GetLabel() + "' added.");
	}

	//------------------------------------------------------------------------------
	//! Gives an entry its icon, the way the stock pause menu buttons carry theirs.
	//!
	//! The image widget inside the vanilla button has no Name — it sits unnamed
	//! several levels down — so it has to be found by shape instead. Nothing
	//! breaks if a future layout drops it; the entry just stays text-only.
	protected void MrFrost_ApplyIcon(notnull Widget buttonWidget, notnull MrFrost_MenuEntry entry)
	{
		ResourceName imageset = entry.GetIconImageset();
		string sprite = entry.GetIconName();

		if (imageset.IsEmpty() || sprite.IsEmpty())
			return;

		ImageWidget icon = MrFrost_FindIcon(buttonWidget);
		if (!icon)
		{
			MrFrost_Log.Debug("Pause menu button has no image widget - entry stays text-only.");
			return;
		}

		if (icon.LoadImageFromSet(0, imageset, sprite))
			icon.SetVisible(true);
	}

	//------------------------------------------------------------------------------
	//! Finds a button's icon.
	//!
	//! Taking the first ImageWidget in the subtree picks up the button's own
	//! decoration — an arrow — instead of the icon. In the stock layout the icon
	//! is the image sitting under a ScaleWidget, so that is what is searched for
	//! first, with the plain search kept as a fallback.
	protected ImageWidget MrFrost_FindIcon(notnull Widget w)
	{
		ImageWidget scaled = MrFrost_FindImageUnderScale(w);
		if (scaled)
			return scaled;

		return MrFrost_FindAnyImage(w);
	}

	//------------------------------------------------------------------------------
	protected ImageWidget MrFrost_FindImageUnderScale(notnull Widget w)
	{
		Widget child = w.GetChildren();

		while (child)
		{
			if (ScaleWidget.Cast(child))
			{
				ImageWidget icon = MrFrost_FindAnyImage(child);
				if (icon)
					return icon;
			}

			ImageWidget nested = MrFrost_FindImageUnderScale(child);
			if (nested)
				return nested;

			child = child.GetSibling();
		}

		return null;
	}

	//------------------------------------------------------------------------------
	protected ImageWidget MrFrost_FindAnyImage(notnull Widget w)
	{
		Widget child = w.GetChildren();

		while (child)
		{
			ImageWidget image = ImageWidget.Cast(child);
			if (image)
				return image;

			ImageWidget nested = MrFrost_FindAnyImage(child);
			if (nested)
				return nested;

			child = child.GetSibling();
		}

		return null;
	}

	//------------------------------------------------------------------------------
	protected void MrFrost_OnEntryClicked(SCR_ButtonBaseComponent button)
	{
		if (!m_mMrFrostButtons)
			return;

		MrFrost_MenuEntry entry = m_mMrFrostButtons.Get(button);
		if (!entry)
			return;

		// Close the pause menu first: leaving it open would stack ours on top of
		// it and the player would have to back out twice.
		Close();

		GetGame().GetMenuManager().OpenMenu(entry.m_Preset);
	}
}
