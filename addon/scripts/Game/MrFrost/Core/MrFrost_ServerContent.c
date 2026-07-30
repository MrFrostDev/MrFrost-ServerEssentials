//------------------------------------------------------------------------------
//! Content that lives on the server, outside the addon.
//!
//! Every feature that lets a server owner define its own content uses this. The
//! files sit together in one folder next to the server config:
//!
//!     <server profile directory>/MrFrost/
//!         infomenu.json
//!         report.json
//!
//! One file per feature rather than one file with sections, so a syntax error in
//! one does not take the others down with it.
//!
//! The folder is named after the addon author, not the addon, so renaming the
//! mod never invalidates a server owner's setup.
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
//! One transferable file. A feature subclasses this to say what its file is
//! called and what to do with the text once it arrives on a client.
class MrFrost_ServerContentChannel
{
	//------------------------------------------------------------------------------
	//! Short identifier, used in log lines.
	string GetId();

	//------------------------------------------------------------------------------
	//! File name inside the MrFrost folder, e.g. "infomenu.json".
	string GetFileName();

	//------------------------------------------------------------------------------
	//! Client side: hand the received text to the feature. Returning false means
	//! the text did not parse and the feature keeps whatever it had.
	bool Apply(string json);

	//------------------------------------------------------------------------------
	//! Server side: does this text look usable? Checked before it is sent, so a
	//! broken file is reported on the server console — where the owner will see
	//! it — instead of silently on every client.
	bool Validate(string json);
}

//------------------------------------------------------------------------------
//! Registry of channels and the reader behind them.
class MrFrost_ServerContent
{
	//! Folder holding every MrFrost server file.
	static const string FOLDER = "$profile:MrFrost/";

	//! Bytes per transfer packet.
	//!
	//! Well under any plausible RPC payload limit. Content travels once per
	//! player, so there is nothing to gain from crowding the ceiling and a
	//! silently dropped packet to lose.
	static const int CHUNK_SIZE = 900;

	protected static ref array<ref MrFrost_ServerContentChannel> s_aChannels;

	//! Server side: file text by channel id, read once and kept.
	protected static ref map<string, string> s_mFiles;

	//------------------------------------------------------------------------------
	//! Adds a channel. Registering the same id twice is a no-op, so callers do not
	//! have to coordinate who registers first.
	static void Register(notnull MrFrost_ServerContentChannel channel)
	{
		if (!s_aChannels)
			s_aChannels = {};

		if (Find(channel.GetId()))
			return;

		s_aChannels.Insert(channel);
	}

	//------------------------------------------------------------------------------
	//! Every registered channel, in registration order.
	//!
	//! That order is the wire format: a transfer packet names its channel by index
	//! into this array, so both sides have to build it identically. They do,
	//! because both run MrFrost_Features.Init().
	static array<ref MrFrost_ServerContentChannel> GetChannels()
	{
		if (!s_aChannels)
			s_aChannels = {};

		return s_aChannels;
	}

	//------------------------------------------------------------------------------
	static MrFrost_ServerContentChannel Find(string id)
	{
		if (!s_aChannels)
			return null;

		foreach (MrFrost_ServerContentChannel channel : s_aChannels)
		{
			if (channel.GetId() == id)
				return channel;
		}

		return null;
	}

	//------------------------------------------------------------------------------
	//! Server side: this channel's file as text, or an empty string when the
	//! server does not have one.
	//!
	//! Read once and kept: the content goes to every joining player, and going
	//! back to disk per player would be pointless work. A server owner editing a
	//! file therefore has to restart the server, which is the rule the rest of the
	//! server configuration follows too.
	static string Read(notnull MrFrost_ServerContentChannel channel)
	{
		if (!s_mFiles)
			s_mFiles = new map<string, string>();

		string id = channel.GetId();

		string cached;
		if (s_mFiles.Find(id, cached))
			return cached;

		string raw = ReadFile(channel);
		s_mFiles.Set(id, raw);
		return raw;
	}

	//------------------------------------------------------------------------------
	protected static string ReadFile(notnull MrFrost_ServerContentChannel channel)
	{
		string path = FOLDER + channel.GetFileName();

		if (!FileIO.FileExists(path))
		{
			MrFrost_Log.Info("No " + path + " on this server - " + channel.GetId() + " uses the content bundled with the addon.");
			return string.Empty;
		}

		FileHandle file = FileIO.OpenFile(path, FileMode.READ);
		if (!file)
		{
			MrFrost_Log.Error("Could not open " + path + " - falling back to the bundled content.");
			return string.Empty;
		}

		// Line by line rather than in one read: JSON strings cannot contain a raw
		// newline, so dropping the line breaks is safe and saves carrying them
		// across the network.
		string raw;
		string line;
		while (file.ReadLine(line) >= 0)
		{
			raw += line;
		}

		file.Close();

		if (raw.IsEmpty())
		{
			MrFrost_Log.Error(path + " is empty - falling back to the bundled content.");
			return string.Empty;
		}

		if (!channel.Validate(raw))
		{
			MrFrost_Log.Error(path + " did not parse into anything usable - falling back to the bundled content.");
			return string.Empty;
		}

		MrFrost_Log.Info("Loaded " + path + " (" + raw.Length() + " bytes).");
		return raw;
	}

	//------------------------------------------------------------------------------
	//! Shortens text to a byte limit without slicing a character in half.
	//!
	//! Lives beside Split() because it is the same hazard: Substring() counts
	//! bytes, so a limit landing inside a multi-byte character leaves half of one
	//! behind. That half breaks the JSON it is written into and the log line it
	//! is appended to, and a German or Japanese report reaches a limit sooner
	//! than an English one of the same apparent length.
	//!
	//! The cut moves back to the nearest space when one is close, so a sentence
	//! ends on a word rather than mid-syllable.
	static string Truncate(string value, int limit)
	{
		if (limit <= 0 || value.Length() <= limit)
			return value;

		string cut = value.Substring(0, limit);

		int lastSpace = cut.LastIndexOf(" ");
		if (lastSpace > limit - 64)
			cut = cut.Substring(0, lastSpace);

		return cut;
	}

	//------------------------------------------------------------------------------
	//! Splits a file for transfer, cutting only at spaces.
	//!
	//! Substring() counts bytes, and rule texts are full of multi-byte characters
	//! — cutting at a fixed offset would eventually slice one in half and corrupt
	//! the two chunks around it. A space is always a single byte, so a cut there
	//! is always on a character boundary.
	static void Split(string raw, int chunkSize, notnull out array<string> chunks)
	{
		string rest = raw;

		while (!rest.IsEmpty())
		{
			int length = rest.Length();
			if (length <= chunkSize)
			{
				chunks.Insert(rest);
				break;
			}

			int cut = chunkSize;

			string head = rest.Substring(0, cut);
			int lastSpace = head.LastIndexOf(" ");

			// No space in a whole chunk means a very long unbroken token. Cutting
			// hard is the only option left; it is also the only case where a
			// character could be split, and JSON structure itself is pure ASCII.
			if (lastSpace > 0)
				cut = lastSpace;

			chunks.Insert(rest.Substring(0, cut));
			rest = rest.Substring(cut, length - cut);
		}
	}
}
