//------------------------------------------------------------------------------
//! Data model for the info menu's content.
//!
//! Server owners describe their categories and entries as data and never touch
//! script.
//!
//! Two levels, deliberately: a flat list of categories, each holding a list of
//! entries. Deeper nesting was a non-goal — a wiki that needs three levels needs
//! a search box more than it needs a third level, and the tree widget is built
//! around exactly two.
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
//! Everything a selectable page has in common, whether it is a category or one
//! of its entries. Kept in one place so an icon or a title behaves identically
//! at both levels.
class MrFrost_InfoMenuPage
{
	[Attribute(defvalue: "1", desc: "Uncheck to hide this without deleting it")]
	bool m_bEnabled;

	[Attribute(uiwidget: UIWidgets.LocaleEditBox, desc: "Name shown in the list on the left")]
	string m_sName;

	[Attribute(uiwidget: UIWidgets.LocaleEditBox, desc: "Headline shown above the text on the right. Falls back to the name when empty.")]
	string m_sTitle;

	[Attribute(uiwidget: UIWidgets.EditBoxMultiline, desc: "Body text. Supports rich text: <br/> for a line break, <b>bold</b>, <color rgba='218,75,0,255'>colour</color> and <image set='{GUID}path.imageset' name='Sprite' scale='1'/> for inline images.")]
	string m_sText;

	[Attribute(uiwidget: UIWidgets.ResourcePickerThumbnail, params: "edds", desc: "Icon in front of the row, as a texture. Leave empty for none. Ignored when an imageset is set below.")]
	ResourceName m_Icon;

	[Attribute(uiwidget: UIWidgets.ResourceNamePicker, params: "imageset", desc: "Icon in front of the row, taken from an imageset. Takes precedence over the texture above.")]
	ResourceName m_IconImageset;

	[Attribute(desc: "Name of the sprite inside the imageset above")]
	string m_sIconName;
}

//------------------------------------------------------------------------------
//! One selectable entry below a category.
[BaseContainerProps(), SCR_BaseContainerLocalizedTitleField("m_sName", "Entry: %1")]
class MrFrost_InfoMenuEntry : MrFrost_InfoMenuPage
{
}

//------------------------------------------------------------------------------
//! A top level category. Selecting it shows its own text; expanding it reveals
//! its entries.
[BaseContainerProps(), SCR_BaseContainerLocalizedTitleField("m_sName", "Category: %1")]
class MrFrost_InfoMenuCategory : MrFrost_InfoMenuPage
{
	[Attribute(defvalue: "0", desc: "Start expanded when the menu opens")]
	bool m_bExpandedByDefault;

	[Attribute(desc: "Entries below this category")]
	ref array<ref MrFrost_InfoMenuEntry> m_aEntries;

	//------------------------------------------------------------------------------
	void MrFrost_InfoMenuCategory()
	{
		// The array is config-provided, but a category authored without any
		// entries leaves it null. Everything downstream iterates it, so it must
		// never be null.
		if (!m_aEntries)
			m_aEntries = {};
	}
}

//------------------------------------------------------------------------------
//! Root of the info menu config.
[BaseContainerProps(configRoot: true)]
class MrFrost_InfoMenuConfig
{
	//! The game's shared icon set. Used as the default whenever a server's JSON
	//! names a sprite without naming an imageset, which is the common case.
	static const ResourceName DEFAULT_IMAGESET = "{3262679C50EF4F01}UI/Textures/Icons/icons_wrapperUI.imageset";

	[Attribute(defvalue: "MrFrost", uiwidget: UIWidgets.LocaleEditBox, desc: "Title shown above the category list")]
	string m_sTitle;

	[Attribute(defvalue: "Info", uiwidget: UIWidgets.LocaleEditBox, desc: "Label of the entry added to the in-game pause menu")]
	string m_sPauseMenuEntry;

	[Attribute(defvalue: "0.76052 0.38643 0.07819 1", uiwidget: UIWidgets.ColorPicker, desc: "Accent colour: selected row and the line under the header. Defaults to the Reforger gold.")]
	ref Color m_AccentColor;

	[Attribute(defvalue: "1", desc: "Open the info menu automatically once, the first time the player spawns into a mission")]
	bool m_bOpenOnJoin;

	[Attribute(desc: "Discord invite URL. Leave empty to hide the Discord button in the footer.")]
	string m_sDiscordUrl;

	[Attribute(defvalue: "Discord", uiwidget: UIWidgets.LocaleEditBox, desc: "Label of the Discord button in the footer")]
	string m_sDiscordLabel;

	[Attribute(defvalue: "{3262679C50EF4F01}UI/Textures/Icons/icons_wrapperUI.imageset", uiwidget: UIWidgets.ResourceNamePicker, params: "imageset", desc: "Imageset for the menu icon, shown next to the title and on the pause menu entry")]
	ResourceName m_MenuIconImageset;

	[Attribute(defvalue: "field-manual", desc: "Sprite name for the menu icon. Leave empty to hide it.")]
	string m_sMenuIconName;

	[Attribute(desc: "Categories, in the order they should appear")]
	ref array<ref MrFrost_InfoMenuCategory> m_aCategories;

	//------------------------------------------------------------------------------
	void MrFrost_InfoMenuConfig()
	{
		if (!m_aCategories)
			m_aCategories = {};
	}

	//------------------------------------------------------------------------------
	//! Accent colour, never null — falls back to white, which is what the flat
	//! grey design is built around.
	Color GetAccentColor()
	{
		if (m_AccentColor)
			return m_AccentColor;

		return new Color(1, 1, 1, 1);
	}
}

//------------------------------------------------------------------------------
//! Single place that knows where the info menu's content comes from.
//!
//! Two sources, in order:
//!
//!   1. the server's own MrFrost/infomenu.json, sent to this client on join
//!   2. the config bundled with the addon
//!
//! The server always wins when it has content. That is what lets one published
//! mod carry a different info menu on every server it runs on.
//!
//! Both the menu and the pause menu entry need this, and the pause menu is built
//! long before the menu is ever opened — so the result is kept here rather than
//! parsed twice per session.
//------------------------------------------------------------------------------
class MrFrost_InfoMenuConfigLoader
{
	//! Fallback content, shipped inside the addon. Used when the server has no
	//! info menu of its own, and in single player.
	static const ResourceName CONFIG_PATH = "{7FA1C3D2E4B50601}Configs/MrFrost/InfoMenu.conf";

	protected static ref MrFrost_InfoMenuConfig s_Config;
	protected static ref MrFrost_InfoMenuConfig s_ServerConfig;

	//------------------------------------------------------------------------------
	//! Config to render, or null when neither source produced one.
	static MrFrost_InfoMenuConfig Get()
	{
		if (s_ServerConfig)
			return s_ServerConfig;

		if (!s_Config)
			s_Config = SCR_ConfigHelperT<MrFrost_InfoMenuConfig>.GetConfigObject(CONFIG_PATH);

		return s_Config;
	}

	//------------------------------------------------------------------------------
	//! Parses a server's JSON and installs it. Returns false and changes nothing
	//! when the text does not parse, so a broken file on the server degrades to
	//! the bundled content instead of an empty menu.
	static bool ApplyServerJson(string json)
	{
		if (json.IsEmpty())
			return false;

		MrFrost_InfoMenuJson parsed = new MrFrost_InfoMenuJson();
		parsed.ExpandFromRAW(json);

		MrFrost_InfoMenuChannel.ApplyStrings(parsed.strings);

		if (!parsed.enabled)
		{
			// Switched off deliberately: an empty category list is what hides the
			// menu, its pause entry and its key.
			s_ServerConfig = new MrFrost_InfoMenuConfig();
			MrFrost_Log.Info("The info menu is switched off on this server.");
			return true;
		}

		if (parsed.categories.IsEmpty())
		{
			MrFrost_Log.Error("The info menu content sent by this server has no categories - keeping the bundled content.");
			return false;
		}

		s_ServerConfig = parsed.ToConfig();
		MrFrost_Log.Info("Using this server's info menu (" + s_ServerConfig.m_aCategories.Count() + " categories).");
		return true;
	}
}
