//------------------------------------------------------------------------------
//! The info menu as a server owner writes it: MrFrost/infomenu.json.
//!
//! JSON rather than the .conf format the addon uses, because a .conf only
//! resolves through the resource database, which is built when the addon is
//! packaged. A file dropped onto a running server is not in it and never can be.
//! JSON is parsed from plain text, which is exactly what an unmanaged file is.
//!
//! Field names are the JSON keys — RegV() registers a script member under its own
//! name — so renaming a member here changes the file format for every server.
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
//! One entry below a category.
class MrFrost_InfoMenuJsonEntry : JsonApiStruct
{
	bool enabled;
	string name;
	string title;
	string text;
	string icon;
	string iconImageset;

	//------------------------------------------------------------------------------
	void MrFrost_InfoMenuJsonEntry()
	{
		// Absent in the file means "on". A server owner who never writes the key
		// gets a visible entry, which is the expectation.
		enabled = true;

		RegV("enabled");
		RegV("name");
		RegV("title");
		RegV("text");
		RegV("icon");
		RegV("iconImageset");
	}
}

//------------------------------------------------------------------------------
//! A collapsible category, optionally with entries below it.
//!
//! Deliberately not derived from the entry class: the JSON layer stays a flat
//! mirror of the file so a reader can match class against file one to one.
class MrFrost_InfoMenuJsonCategory : JsonApiStruct
{
	bool enabled;
	bool expanded;
	string name;
	string title;
	string text;
	string icon;
	string iconImageset;
	ref array<ref MrFrost_InfoMenuJsonEntry> entries;

	//------------------------------------------------------------------------------
	void MrFrost_InfoMenuJsonCategory()
	{
		enabled = true;
		entries = {};

		RegV("enabled");
		RegV("expanded");
		RegV("name");
		RegV("title");
		RegV("text");
		RegV("icon");
		RegV("iconImageset");
		RegV("entries");
	}
}

//------------------------------------------------------------------------------
//! Root of infomenu.json.
class MrFrost_InfoMenuJson : JsonApiStruct
{
	string title;
	string pauseMenuEntry;
	string discordUrl;
	string discordLabel;
	string websiteUrl;
	string websiteLabel;
	string customUrl;
	string customLabel;
	string accentColor;
	string menuIcon;
	string menuIconImageset;
	bool openOnJoin;
	bool enabled;
	ref array<ref MrFrost_InfoMenuJsonCategory> categories;
	ref array<ref MrFrost_JsonString> strings;

	//------------------------------------------------------------------------------
	void MrFrost_InfoMenuJson()
	{
		openOnJoin = true;
		enabled = true;
		categories = {};
		strings = {};

		RegV("enabled");
		RegV("strings");
		RegV("title");
		RegV("pauseMenuEntry");
		RegV("discordUrl");
		RegV("discordLabel");
		RegV("websiteUrl");
		RegV("websiteLabel");
		RegV("customUrl");
		RegV("customLabel");
		RegV("accentColor");
		RegV("menuIcon");
		RegV("menuIconImageset");
		RegV("openOnJoin");
		RegV("categories");
	}

	//------------------------------------------------------------------------------
	//! Translates the parsed file into the config object the menu already knows
	//! how to render, so nothing downstream has to care where the content came
	//! from.
	//! Ceiling on how many rows one menu may ask a client to build. Each row is
	//! fifteen widgets, created up front, and nothing else bounded this - a
	//! server could hand every joining player tens of thousands of them, on the
	//! main thread, in a menu that opens by itself.
	static const int MAX_ROWS = 512;

	//! Ceiling on one page of text. Far past any rule set anyone writes.
	static const int MAX_PAGE_TEXT = 20000;

	//------------------------------------------------------------------------------
	//! One page of body text, or nothing when it is absurdly long.
	//!
	//! Dropped rather than cut. This text is markup - the bundled pages are full
	//! of <br/>, <b> and <color rgba=...> - and a cut cannot leave it whole. A
	//! generic truncator knows about spaces and character boundaries, not about
	//! tags, and the space rule actively prefers the space inside <color rgba=,
	//! which ends the page on a bare "<color". Even a lucky cut leaves whatever
	//! was opened before it unclosed. The limit is only here to stop a server
	//! handing a client something unreasonable, so refusing the page and saying
	//! so is both safe and honest.
	protected string PageText(string text, string name)
	{
		if (text.Length() <= MAX_PAGE_TEXT)
			return text;

		MrFrost_Log.Warn("The page '" + name + "'" + " is longer than " + MAX_PAGE_TEXT + " characters and was left empty. Split it into entries.");
		return string.Empty;
	}

	//------------------------------------------------------------------------------
	MrFrost_InfoMenuConfig ToConfig()
	{
		MrFrost_InfoMenuConfig config = new MrFrost_InfoMenuConfig();

		config.m_sTitle          = title;
		config.m_sPauseMenuEntry = pauseMenuEntry;
		config.m_sDiscordUrl     = discordUrl;
		config.m_sDiscordLabel   = discordLabel;
		config.m_sWebsiteUrl     = websiteUrl;
		config.m_sWebsiteLabel   = websiteLabel;
		config.m_sCustomUrl      = customUrl;
		config.m_sCustomLabel    = customLabel;
		config.m_sMenuIconName   = menuIcon;
		config.m_bOpenOnJoin     = openOnJoin;
		config.m_aCategories     = {};

		config.m_MenuIconImageset = ResolveImageset(menuIconImageset);

		Color accent = ParseColor(accentColor);
		if (accent)
			config.m_AccentColor = accent;

		int rows = 0;

		foreach (MrFrost_InfoMenuJsonCategory source : categories)
		{
			if (!source)
				continue;

			if (rows >= MAX_ROWS)
				break;

			// Only what will actually be built. A file that keeps archived sections
			// switched off should not spend its budget on rows nobody draws.
			if (source.enabled)
				rows++;

			MrFrost_InfoMenuCategory category = new MrFrost_InfoMenuCategory();
			category.m_bEnabled           = source.enabled;
			category.m_bExpandedByDefault = source.expanded;
			category.m_sName              = source.name;
			category.m_sTitle             = source.title;
			category.m_sText              = PageText(source.text, source.name);
			category.m_sIconName          = source.icon;
			category.m_IconImageset       = ResolveImageset(source.iconImageset);
			category.m_aEntries           = {};

			foreach (MrFrost_InfoMenuJsonEntry sourceEntry : source.entries)
			{
				if (!sourceEntry)
					continue;

				if (rows >= MAX_ROWS)
					break;

				if (source.enabled && sourceEntry.enabled)
					rows++;

				MrFrost_InfoMenuEntry entry = new MrFrost_InfoMenuEntry();
				entry.m_bEnabled     = sourceEntry.enabled;
				entry.m_sName        = sourceEntry.name;
				entry.m_sTitle       = sourceEntry.title;
				entry.m_sText        = PageText(sourceEntry.text, sourceEntry.name);
				entry.m_sIconName    = sourceEntry.icon;
				entry.m_IconImageset = ResolveImageset(sourceEntry.iconImageset);

				category.m_aEntries.Insert(entry);
			}

			config.m_aCategories.Insert(category);
		}

		// Said once, after the loops, not inside them. Reporting it from the outer
		// loop meant a file whose budget ran out inside its last category - or its
		// only one - was cut without a word.
		if (rows >= MAX_ROWS)
			MrFrost_Log.Warn("This info menu has more than " + MAX_ROWS + " rows. The rest was left out.");

		return config;
	}

	//------------------------------------------------------------------------------
	//! Falls back to the game's shared icon set, which is what almost every server
	//! wants — so the per-row "iconImageset" key only has to be written by someone
	//! shipping their own artwork.
	protected ResourceName ResolveImageset(string imageset)
	{
		if (imageset.IsEmpty())
			return MrFrost_InfoMenuConfig.DEFAULT_IMAGESET;

		return imageset;
	}

	//------------------------------------------------------------------------------
	//! Accepts "226,167,79" or "226,167,79,255" in ordinary sRGB, the numbers a
	//! colour picker shows. Layout colours are linear, which nobody can be
	//! expected to type by hand, so the conversion happens here.
	protected Color ParseColor(string value)
	{
		if (value.IsEmpty())
			return null;

		array<string> parts = {};
		value.Split(",", parts, true);

		if (parts.Count() < 3)
		{
			MrFrost_Log.Warn("accentColor '" + value + "' is not 'r,g,b' - ignoring it.");
			return null;
		}

		int alpha = 255;
		if (parts.Count() > 3)
			alpha = parts[3].Trim().ToInt();

		return Color.FromSRGBA(parts[0].Trim().ToInt(), parts[1].Trim().ToInt(), parts[2].Trim().ToInt(), alpha);
	}
}

//------------------------------------------------------------------------------
//! Ties infomenu.json into the shared server-content transfer.
class MrFrost_InfoMenuChannel : MrFrost_ServerContentChannel
{
	//------------------------------------------------------------------------------
	override string GetId()
	{
		return "infomenu";
	}

	//------------------------------------------------------------------------------
	override string GetFileName()
	{
		return "infomenu.json";
	}

	//------------------------------------------------------------------------------
	override bool Apply(string json)
	{
		return MrFrost_InfoMenuConfigLoader.ApplyServerJson(json);
	}

	//------------------------------------------------------------------------------
	//! A server's own wording for the addon's own strings. Shared across features:
	//! whichever file carries a key, that is the wording every menu uses.
	static void ApplyStrings(notnull array<ref MrFrost_JsonString> strings)
	{
		if (strings.IsEmpty())
			return;

		map<string, string> overrides = new map<string, string>();

		foreach (MrFrost_JsonString entry : strings)
		{
			if (entry && !entry.key.IsEmpty())
				overrides.Set(entry.key, entry.text);
		}

		MrFrost_Text.SetOverrides(overrides);
	}

	//------------------------------------------------------------------------------
	//! Parsed on the server purely so a broken file is reported on the server
	//! console, where the owner will see it.
	//! Names a menu the client will have to cut down.
	protected void WarnOversizedMenu(notnull MrFrost_InfoMenuJson probe)
	{
		if (!probe.categories)
			return;

		int rows = 0;
		foreach (MrFrost_InfoMenuJsonCategory category : probe.categories)
		{
			if (!category || !category.enabled)
				continue;

			rows++;

			foreach (MrFrost_InfoMenuJsonEntry entry : category.entries)
			{
				if (entry && entry.enabled)
					rows++;
			}
		}

		if (rows > MrFrost_InfoMenuJson.MAX_ROWS)
			MrFrost_Log.Warn("infomenu.json asks for " + rows + " rows; only the first " + MrFrost_InfoMenuJson.MAX_ROWS + " will be shown.");
	}

	//------------------------------------------------------------------------------
	//! Names a footer link the client will refuse to open.
	protected void WarnUnopenableLink(string url, string keyName)
	{
		if (url.IsEmpty() || url.StartsWith("https://"))
			return;

		MrFrost_Log.Warn(keyName + " in infomenu.json is not an https:// address, so no button will be drawn for it: " + url);
	}

	//------------------------------------------------------------------------------
	override bool Validate(string json)
	{
		MrFrost_InfoMenuJson probe = new MrFrost_InfoMenuJson();
		probe.ExpandFromRAW(json);

		// Structure, not content. Requiring a category rejected the one file a
		// server writes to switch the menu off - "{"enabled": false}" has none -
		// and the documentation promises that file works. It also rejected a
		// file carrying only a title or only string overrides.
		if (!MrFrost_ServerContent.IsJsonObject(json))
		{
			MrFrost_Log.Error("infomenu.json is not sound JSON - falling back to the bundled content. Check it for a stray or missing comma.");
			return false;
		}

		// Said on the server console, where the owner who wrote the file is
		// looking. A link the client will not open draws no button at all, and a
		// footer slot that silently fails to appear is hard to explain from the
		// other end. Not a rejection - the rest of the file is fine.
		// ToConfig() runs on the client, so its warnings land in player logs. The
		// owner who wrote the file needs to hear this on their own console.
		WarnOversizedMenu(probe);

		WarnUnopenableLink(probe.discordUrl, "discordUrl");
		WarnUnopenableLink(probe.websiteUrl, "websiteUrl");
		WarnUnopenableLink(probe.customUrl, "customUrl");

		return true;
	}
}
