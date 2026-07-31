//------------------------------------------------------------------------------
//! Every piece of UI text in the addon, in every language it has.
//!
//! Three layers, first hit wins:
//!
//!   1. what the server put in its JSON      — a server owner's own wording
//!   2. the addon's table, in the game's language
//!   3. the addon's table, in English
//!
//! So a player on a German client sees German without anyone configuring
//! anything, and a server that wants its own tone overrides individual strings
//! without having to translate the rest.
//!
//! Deliberately not the engine's own string table. That lives in
//! Language/localization.<locale>.conf, which is what the Workbench localization
//! editor writes — a format this addon would have to reproduce exactly, with no
//! way to tell a wrong guess from a missing translation at runtime. A table we
//! own cannot silently fail to load, and it is the only layer a server owner can
//! override anyway.
//!
//! Engine keys still pass through untouched: anything starting with '#' is
//! handed to the widget as-is, so vanilla strings like #AR-Menu_Back keep
//! working.
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
//! One language's version of a string.
[BaseContainerProps(), SCR_BaseContainerLocalizedTitleField("m_sLanguage")]
class MrFrost_Translation
{
	[Attribute(desc: "Language code as the game reports it, e.g. de_de. A bare 'de' matches every German locale.")]
	string m_sLanguage;

	[Attribute(uiwidget: UIWidgets.EditBoxMultiline, desc: "The text in that language")]
	string m_sText;
}

//------------------------------------------------------------------------------
//! One string, in English plus whatever translations exist.
[BaseContainerProps(), SCR_BaseContainerLocalizedTitleField("m_sKey")]
class MrFrost_TextEntry
{
	[Attribute(desc: "Key the code and the server JSON refer to, e.g. report.submit")]
	string m_sKey;

	[Attribute(uiwidget: UIWidgets.EditBoxMultiline, desc: "English text. Used whenever the player's language has no translation.")]
	string m_sText;

	[Attribute(desc: "Translations")]
	ref array<ref MrFrost_Translation> m_aTranslations;

	//------------------------------------------------------------------------------
	//! Best match for a language code, or the English text.
	//!
	//! Tries the full code first, then the bare language: a table carrying "de"
	//! serves de_de and de_at alike, and one carrying "pt_br" can differ from a
	//! generic "pt" sitting next to it.
	string Resolve(string language, string languageShort)
	{
		if (!m_aTranslations)
			return m_sText;

		foreach (MrFrost_Translation translation : m_aTranslations)
		{
			if (translation && translation.m_sLanguage == language && !translation.m_sText.IsEmpty())
				return translation.m_sText;
		}

		foreach (MrFrost_Translation fallback : m_aTranslations)
		{
			if (fallback && fallback.m_sLanguage == languageShort && !fallback.m_sText.IsEmpty())
				return fallback.m_sText;
		}

		return m_sText;
	}
}

//------------------------------------------------------------------------------
//! Root of the addon's text table.
[BaseContainerProps(configRoot: true)]
class MrFrost_TextConfig
{
	[Attribute(desc: "Every string the addon shows")]
	ref array<ref MrFrost_TextEntry> m_aEntries;
}

//------------------------------------------------------------------------------
//! Lookup. Everything the player reads goes through MrFrost_Text.Get().
class MrFrost_Text
{
	static const ResourceName CONFIG_PATH = "{7FA1C3D2E4B50620}Configs/MrFrost/Language.conf";

	protected static ref map<string, string> s_mResolved;
	protected static ref map<string, string> s_mOverrides;
	protected static string s_sLanguage;
	protected static string s_sLanguageShort;

	//------------------------------------------------------------------------------
	//! Localised text for a key.
	//!
	//! An unknown key comes back as the key itself rather than as an empty string:
	//! a menu with "report.submit" on a button is obviously wrong, a menu with a
	//! blank button just looks broken.
	static string Get(string key)
	{
		if (key.IsEmpty())
			return key;

		// Engine key - let the widget translate it.
		if (key.StartsWith("#"))
			return key;

		if (s_mOverrides)
		{
			string custom;
			if (s_mOverrides.Find(key, custom))
				return custom;
		}

		Load();

		string text;
		if (s_mResolved.Find(key, text))
			return text;

		MrFrost_Log.Warn("No text for key '" + key + "'.");
		return key;
	}

	//------------------------------------------------------------------------------
	//! Installs a server's own wording. Overrides survive a language change, which
	//! is the point: a server writing German rules wants German buttons next to
	//! them whatever the client is set to.
	//! Client side: forget the wording the last server sent.
	static void ClearOverrides()
	{
		if (s_mOverrides)
			s_mOverrides.Clear();
	}

	//------------------------------------------------------------------------------
	static void SetOverrides(notnull map<string, string> overrides)
	{
		// Merged, not replaced. Each feature's file brings its own map, and the
		// last one read would otherwise discard what the others set - a server
		// using "strings" in both files lost the first file's wording, and it
		// presented as a mistyped key.
		if (!s_mOverrides)
			s_mOverrides = new map<string, string>();

		foreach (string key, string text : overrides)
		{
			s_mOverrides.Set(key, text);
		}

		if (!overrides.IsEmpty())
			MrFrost_Log.Info("This server overrides " + overrides.Count() + " UI string(s).");
	}

	//------------------------------------------------------------------------------
	//! Builds the table for the language the game is currently in, and rebuilds it
	//! whenever that changes.
	//!
	//! Two things this has to survive, both learned from how vanilla handles it:
	//!
	//! **The language changes while the game runs.** A player switching language in
	//! the settings does not restart anything, and script statics outlive the
	//! settings menu. Caching the table once means every menu keeps the language it
	//! first saw until the game is restarted. So the current code is compared
	//! against the language the table was built for on every lookup - one proto
	//! call, and a rebuild only when it actually differs.
	//!
	//! **GetLanguage() can answer with nothing.** Early enough in the boot it
	//! returns an empty string; vanilla guards against exactly that. Recording ""
	//! as the loaded language would pin every string to English for the rest of the
	//! session, so an empty answer keeps whatever table exists and simply tries
	//! again on the next call.
	protected static void Load()
	{
		string language;
		WidgetManager.GetLanguage(language);

		// Nothing to do: either the table already matches, or the engine has not
		// settled on a language yet and we keep what we have.
		if (s_mResolved && (language == s_sLanguage || language.IsEmpty()))
			return;

		bool reload = s_mResolved != null;

		s_mResolved = new map<string, string>();
		s_sLanguage = language;

		s_sLanguageShort = s_sLanguage;
		int separator = s_sLanguage.IndexOf("_");
		if (separator > 0)
			s_sLanguageShort = s_sLanguage.Substring(0, separator);

		MrFrost_TextConfig config = SCR_ConfigHelperT<MrFrost_TextConfig>.GetConfigObject(CONFIG_PATH);
		if (!config || !config.m_aEntries)
		{
			MrFrost_Log.Error("Could not load the text table from " + CONFIG_PATH + " - the UI will show raw keys.");
			return;
		}

		foreach (MrFrost_TextEntry entry : config.m_aEntries)
		{
			if (entry && !entry.m_sKey.IsEmpty())
				s_mResolved.Set(entry.m_sKey, entry.Resolve(s_sLanguage, s_sLanguageShort));
		}

		if (reload)
			MrFrost_Log.Info("Language changed to '" + s_sLanguage + "' - text table rebuilt (" + s_mResolved.Count() + " strings).");
		else
			MrFrost_Log.Debug("Text table loaded for language '" + s_sLanguage + "' (" + s_mResolved.Count() + " strings).");
	}
}

//------------------------------------------------------------------------------
//! A server's override of one string, as it appears in a JSON file.
class MrFrost_JsonString : JsonApiStruct
{
	string key;
	string text;

	//------------------------------------------------------------------------------
	void MrFrost_JsonString()
	{
		RegV("key");
		RegV("text");
	}
}
