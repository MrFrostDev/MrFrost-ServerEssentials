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

		MrFrost_Log.Warn("A page is longer than " + MAX_PAGE_TEXT + " characters and was left empty. Split it into entries: " + MrFrost_InfoMenuChannel.PageLabel(name));
		return string.Empty;
	}

	//------------------------------------------------------------------------------
	//! Translates the parsed file into the config object the menu already knows
	//! how to render, so nothing downstream has to care where the content came
	//! from.
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
		bool dropped = false;

		// Null rather than empty is what "categories": null parses into, and the
		// server-side warning pass already guards it. This did not.
		if (!categories)
			return config;

		foreach (MrFrost_InfoMenuJsonCategory source : categories)
		{
			if (!source)
				continue;

			// Budget asked only of rows that would be drawn, and in that order. Asking
			// first meant a file trailed by switched-off sections reported itself as
			// cut when nothing a player could see had been - and said so only on the
			// client, while the server reading the same file stayed quiet.
			if (source.enabled)
			{
				if (rows >= MAX_ROWS)
				{
					dropped = true;
					break;
				}

				rows++;
			}

			MrFrost_InfoMenuCategory category = new MrFrost_InfoMenuCategory();
			category.m_bEnabled           = source.enabled;
			category.m_bExpandedByDefault = source.expanded;
			category.m_sName              = source.name;
			category.m_sTitle             = source.title;
			category.m_sText              = PageText(source.text, source.name);
			category.m_sIconName          = source.icon;
			category.m_IconImageset       = ResolveImageset(source.iconImageset);
			category.m_aEntries           = {};

			// Null, not empty, is what "entries": null parses into - the same trap
			// categories and strings were guarded against, one level down.
			if (source.entries)
			{
				foreach (MrFrost_InfoMenuJsonEntry sourceEntry : source.entries)
				{
					if (!sourceEntry)
						continue;

					if (source.enabled && sourceEntry.enabled)
					{
						if (rows >= MAX_ROWS)
						{
							dropped = true;
							break;
						}

						rows++;
					}

					MrFrost_InfoMenuEntry entry = new MrFrost_InfoMenuEntry();
					entry.m_bEnabled     = sourceEntry.enabled;
					entry.m_sName        = sourceEntry.name;
					entry.m_sTitle       = sourceEntry.title;
					entry.m_sText        = PageText(sourceEntry.text, sourceEntry.name);
					entry.m_sIconName    = sourceEntry.icon;
					entry.m_IconImageset = ResolveImageset(sourceEntry.iconImageset);

					category.m_aEntries.Insert(entry);
				}
			}

			config.m_aCategories.Insert(category);
		}

		// Said once, after the loops, not inside them. Reporting it from the outer
		// loop meant a file whose budget ran out inside its last category - or its
		// only one - was cut without a word.
		// Only when something was actually left behind. Testing the counter meant
		// a file landing on exactly the limit was reported as cut when nothing was,
		// and disagreed with the server, which tests the same file with >.
		if (dropped)
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
	//! Takes the array as it came out of the JSON layer, which may be null.
	//!
	//! A file writing "strings": null parses into one, and the constructor's
	//! empty array does not survive that. The report channel has always guarded
	//! this; here it reached a notnull parameter from both the server and the
	//! client, so a single null key cost the server the rest of its startup read
	//! and cost every client the content of a channel it had already marked as
	//! delivered.
	static void ApplyStrings(array<ref MrFrost_JsonString> strings)
	{
		if (!strings || strings.IsEmpty())
			return;

		map<string, string> overrides = new map<string, string>();

		foreach (MrFrost_JsonString entry : strings)
		{
			if (!entry || entry.key.IsEmpty())
				continue;

			// An override with no text is not an override. The key travels
			// separately from the value, so a "text" left out arrives as an
			// empty string - and Get() answers from the override map before it
			// reaches the table, so one forgotten field drew a button with no
			// label on it. That is the outcome the unknown-key path returns the
			// key itself to avoid, reached by the one road that skipped it.
			if (entry.text.IsEmpty())
			{
				MrFrost_Log.Warn("The strings override for '" + entry.key + "' has no text and was ignored.");
				continue;
			}

			overrides.Set(entry.key, entry.text);
		}

		MrFrost_Text.SetOverrides(overrides);
	}

	//------------------------------------------------------------------------------
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

			if (!category.entries)
				continue;

			foreach (MrFrost_InfoMenuJsonEntry entry : category.entries)
			{
				if (entry && entry.enabled)
					rows++;
			}
		}

		if (rows > MrFrost_InfoMenuJson.MAX_ROWS)
			MrFrost_Log.Warn("infomenu.json asks for " + rows + " rows; only the first " + MrFrost_InfoMenuJson.MAX_ROWS + " will be shown.");

		foreach (MrFrost_InfoMenuJsonCategory category : probe.categories)
		{
			if (!category)
				continue;

			if (!category.enabled)
				continue;

			WarnOversizedPage(category.text, category.name);

			if (!category.entries)
				continue;

			foreach (MrFrost_InfoMenuJsonEntry entry : category.entries)
			{
				if (entry && entry.enabled)
					WarnOversizedPage(entry.text, entry.name);
			}
		}
	}

	//------------------------------------------------------------------------------
	//! Names a page the client will leave empty.
	protected void WarnOversizedPage(string text, string name)
	{
		if (text.Length() <= MrFrost_InfoMenuJson.MAX_PAGE_TEXT)
			return;

		MrFrost_Log.Warn("A page in infomenu.json is longer than " + MrFrost_InfoMenuJson.MAX_PAGE_TEXT + " characters and will be left empty: " + PageLabel(name));
	}

	//------------------------------------------------------------------------------
	//! A page name a reader can search their file for.
	static string PageLabel(string name)
	{
		if (name.IsEmpty())
			return "(a page with no name set)";

		return "\"" + name + "\"";
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
	//! Reports a colour the client will not be able to read, on the console of
	//! the server whose file carries it.
	//!
	//! Deliberately duplicates ParseColor's own test rather than calling it:
	//! ParseColor returns a Color and warns as a side effect, and the side effect
	//! is the only part wanted here.
	protected void WarnUnreadableColor(string value)
	{
		if (value.IsEmpty())
			return;

		array<string> parts = {};
		value.Split(",", parts, true);

		if (parts.Count() >= 3)
			return;

		MrFrost_Log.Warn("accentColor '" + value + "' in infomenu.json is not 'r,g,b' - players will see the default accent.");
	}

	//------------------------------------------------------------------------------
	//! Parsed on the server purely so a broken file is reported on the server
	//! console, where the owner will see it.
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
			MrFrost_Log.Error("infomenu.json is not sound JSON - falling back to the bundled content. Check it for a stray or missing comma, a capitalised True/False/Null, a number written .5 instead of 0.5, or a backslash in a text value.");
			return false;
		}

		// Said on the server console, where the owner who wrote the file is
		// looking. A link the client will not open draws no button at all, and a
		// footer slot that silently fails to appear is hard to explain from the
		// other end. Not a rejection - the rest of the file is fine.
		// ToConfig() runs on the client, so its warnings land in player logs. The
		// owner who wrote the file needs to hear this on their own console.
		// Applied on the server too, not only when a client parses the file. The
		// Discord embed is built here, so its labels are resolved here - a server
		// overriding report.embed.* from infomenu.json had its wording honoured by
		// every player and ignored in its own moderation channel. report.json has
		// done this since it was written; this file had not.
		ApplyStrings(probe.strings);

		WarnOversizedMenu(probe);

		// Same reason as the links above. ParseColor also says this, but it runs
		// inside ToConfig() on the client, so a mistyped colour was reported to
		// every player and to nobody who could fix it.
		WarnUnreadableColor(probe.accentColor);

		WarnUnopenableLink(probe.discordUrl, "discordUrl");
		WarnUnopenableLink(probe.websiteUrl, "websiteUrl");
		WarnUnopenableLink(probe.customUrl, "customUrl");

		return true;
	}
}
