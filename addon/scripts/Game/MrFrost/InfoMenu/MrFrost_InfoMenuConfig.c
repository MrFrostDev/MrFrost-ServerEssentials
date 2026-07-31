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

	[Attribute(desc: "Website URL. Leave empty to hide the website button in the footer.")]
	string m_sWebsiteUrl;

	[Attribute(defvalue: "Website", uiwidget: UIWidgets.LocaleEditBox, desc: "Label of the website button in the footer")]
	string m_sWebsiteLabel;

	[Attribute(desc: "A third link of your choosing - a ruleset, a ban appeal form, a Teamspeak address. Leave empty to hide the button.")]
	string m_sCustomUrl;

	[Attribute(uiwidget: UIWidgets.LocaleEditBox, desc: "Label of that third button. Leave empty and the button stays hidden even with a URL set, because an unlabelled prompt tells a player nothing.")]
	string m_sCustomLabel;

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
	//! Whether this config has anything a player could actually read.
	//!
	//! Not the same question as "are there categories". A file may carry a dozen
	//! and switch every one of them off, and BuildRows honours that - so the two
	//! gates that gave the menu its pause entry and its key were answering a
	//! question nobody had asked, and opening an empty menu on the strength of
	//! it.
	bool HasVisibleContent()
	{
		if (!m_aCategories)
			return false;

		foreach (MrFrost_InfoMenuCategory category : m_aCategories)
		{
			if (category && category.m_bEnabled)
				return true;
		}

		// The footer counts too. A server may switch every category off and still
		// have working links, and those live nowhere else - closing the menu on
		// them would take away the only way to reach them.
		//
		// Asked exactly as BuildLinkButton asks it. Testing the URL for emptiness
		// was looser than the thing it was standing in for, so a URL with the wrong
		// scheme, or a custom slot with no label, opened a menu with nothing in it
		// at all - no rows and no buttons.
		return DrawsLink(m_sDiscordUrl, m_sDiscordLabel, "Discord")
			|| DrawsLink(m_sWebsiteUrl, m_sWebsiteLabel, "Website")
			|| DrawsLink(m_sCustomUrl, m_sCustomLabel, string.Empty);
	}

	//------------------------------------------------------------------------------
	//! Whether a footer slot would actually produce a button.
	//!
	//! Same three tests BuildLinkButton applies, in the same order: a URL, an
	//! https:// one, and a label to put on it. The custom slot has no fallback
	//! label by design, so an unnamed one draws nothing.
	protected bool DrawsLink(string url, string label, string fallbackLabel)
	{
		if (url.IsEmpty() || !url.StartsWith("https://"))
			return false;

		return !label.IsEmpty() || !fallbackLabel.IsEmpty();
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
	//! Client side: forget whatever the last server sent.
	static void ClearServerConfig()
	{
		s_ServerConfig = null;
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

		// Not an error, and not a failure: a server may send nothing but a title,
		// a link or string overrides, and mean it. What such a file does not
		// carry is content, so the bundled categories stay, while everything it
		// does carry is honoured - which keeping the bundled config wholesale
		// would have ignored.
		//
		// Field by field rather than wholesale the other way, too. ToConfig()
		// builds a plain instance, and the [Attribute] defaults on this class are
		// filled in by the config loader, not by the constructor - so every key
		// the server left out arrives blank rather than at its default, and
		// taking the result as-is would have cost the bundled accent colour and
		// the menu icon on a file that never mentioned either.
		if (parsed.categories.IsEmpty())
		{
			s_ServerConfig = null;
			MrFrost_InfoMenuConfig bundled = Get();

			MrFrost_InfoMenuConfig merged = parsed.ToConfig();
			if (bundled)
			{
				merged.m_aCategories = bundled.m_aCategories;

				if (!merged.m_AccentColor)
					merged.m_AccentColor = bundled.m_AccentColor;

				if (merged.m_sTitle.IsEmpty())
					merged.m_sTitle = bundled.m_sTitle;

				if (merged.m_sPauseMenuEntry.IsEmpty())
					merged.m_sPauseMenuEntry = bundled.m_sPauseMenuEntry;

				// The imageset travels with the icon: an icon named without one
				// resolves to the shared set, which is not where a bundled sprite
				// necessarily lives.
				if (merged.m_sMenuIconName.IsEmpty())
				{
					merged.m_sMenuIconName = bundled.m_sMenuIconName;
					merged.m_MenuIconImageset = bundled.m_MenuIconImageset;
				}

				// m_bOpenOnJoin is the one setting that cannot join this list.
				// ExpandFromRAW gives an absent key the constructor's value, so
				// "not mentioned" and "set to true" arrive identical and there is
				// nothing here to test. It defaults to true on both sides, so the
				// two only diverge for a build that ships it switched off.
				//
				// The footer belongs to the menu being kept. A file that says
				// nothing about the links is keeping the bundled menu whole, not
				// asking for it with its buttons taken off.
				if (merged.m_sDiscordUrl.IsEmpty())
				{
					merged.m_sDiscordUrl = bundled.m_sDiscordUrl;
					merged.m_sDiscordLabel = bundled.m_sDiscordLabel;
				}

				if (merged.m_sWebsiteUrl.IsEmpty())
				{
					merged.m_sWebsiteUrl = bundled.m_sWebsiteUrl;
					merged.m_sWebsiteLabel = bundled.m_sWebsiteLabel;
				}

				if (merged.m_sCustomUrl.IsEmpty())
				{
					merged.m_sCustomUrl = bundled.m_sCustomUrl;
					merged.m_sCustomLabel = bundled.m_sCustomLabel;
				}
			}

			s_ServerConfig = merged;

			MrFrost_Log.Info("This server sent no info menu categories - keeping the content bundled with the addon, and its own settings.");
			return true;
		}

		s_ServerConfig = parsed.ToConfig();
		MrFrost_Log.Info("Using this server's info menu (" + s_ServerConfig.m_aCategories.Count() + " categories).");
		return true;
	}
}
