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

	//------------------------------------------------------------------------------
	//! Server side: the version of this file a client may hold.
	//!
	//! The transfer sends a file's text, so anything a channel keeps out of a
	//! client's hands has to be removed here rather than merely ignored when the
	//! client parses it. Returning the text unchanged is the right default; a
	//! channel carrying a secret overrides this and re-emits without it.
	string ForClient(string json)
	{
		return json;
	}
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

	//! The engine's ceiling on a single Substring() result, documented in the
	//! vanilla string type. Everything here that cuts text has to stay under it.
	static const int MAX_SUBSTRING = 8191;

	protected static ref array<ref MrFrost_ServerContentChannel> s_aChannels;

	//! Server side: file text by channel id, read once and kept.
	protected static ref map<string, string> s_mFiles;

	//! Server side: the split, client-safe form of each file, built once.
	protected static ref map<string, ref array<string>> s_mChunks;

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
	//! Server side: this channel's file, made client-safe and already split.
	//!
	//! Cached like the file itself. Both steps produce the same bytes for every
	//! player - the file does not change while the server runs - but they ran
	//! per joining player, so a server with a large strings block paid a full
	//! JSON parse, repack and re-split sixty-four times for one identical
	//! answer.
	static array<string> ChunksForClient(notnull MrFrost_ServerContentChannel channel)
	{
		if (!s_mChunks)
			s_mChunks = new map<string, ref array<string>>();

		string id = channel.GetId();

		array<string> cached;
		if (s_mChunks.Find(id, cached))
			return cached;

		array<string> chunks = {};

		string raw = Read(channel);
		if (!raw.IsEmpty())
		{
			// What leaves the server is the channel's client-safe version, never
			// the file as it sits on disk.
			raw = channel.ForClient(raw);
			if (!raw.IsEmpty())
				Split(raw, CHUNK_SIZE, chunks);
		}

		s_mChunks.Set(id, chunks);
		return chunks;
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
		// newline, so every line break is outside a string and dropping it is
		// safe. The same reasoning covers the indentation around it, which is
		// structural whitespace and nothing else - and it is not free, since
		// every byte kept here is sent to every player who joins. A pretty-
		// printed config is easily a tenth indentation.
		string raw;
		string line;
		while (file.ReadLine(line) >= 0)
		{
			line.TrimInPlace();
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


	// Parser states for IsJsonObject().
	protected static const int JSON_VALUE = 0;
	protected static const int JSON_KEY   = 1;
	protected static const int JSON_COLON = 2;
	protected static const int JSON_NEXT  = 3;

	//------------------------------------------------------------------------------
	//! Whether text is a structurally sound JSON object.
	//!
	//! ExpandFromRAW() returns void, so a file the JSON layer could not read is
	//! indistinguishable from one deliberately full of defaults. A single stray
	//! comma in a server's file therefore came back as every setting at its
	//! default - inverting whatever the owner had switched off - and no console
	//! anywhere said a word. Testing the braces at each end was not enough,
	//! because the damage is almost always in the middle.
	//!
	//! This is a shape check, not a parser and not a substitute for one. It walks
	//! the text once and rejects what cannot be JSON whatever the values mean:
	//! unbalanced or crossed brackets, an unterminated string, a missing or
	//! doubled comma, a trailing one, a key with no colon. Text that passes is
	//! not guaranteed to say anything useful - it is guaranteed to have the shape
	//! the layer behind it expects, which is the part that was failing silently.
	static bool IsJsonObject(string text)
	{
		int len = text.Length();

		// An array or a bare number is sound JSON and still not a settings file.
		// Both callers want an object, so the top level is settled here rather
		// than tested again at each of them.
		string head = text;
		head.TrimInPlace();
		if (!head.StartsWith("{"))
			return false;

		// Container types, innermost last. True for an object, false for an array.
		array<bool> stack = {};

		int state = JSON_VALUE;

		// Whether the container we are in is still empty, which is the only place
		// a closing bracket may follow an opening one. Guards against "[1,]".
		bool empty = false;

		int i = 0;
		while (i < len)
		{
			int c = text.ToAscii(i);

			// Whitespace between tokens.
			if (c == 32 || c == 9 || c == 10 || c == 13)
			{
				i++;
				continue;
			}

			if (c == 123 || c == 91)	// { or [
			{
				if (state != JSON_VALUE)
					return false;

				stack.Insert(c == 123);
				empty = true;

				if (c == 123)
					state = JSON_KEY;
				else
					state = JSON_VALUE;

				i++;
				continue;
			}

			if (c == 125 || c == 93)	// } or ]
			{
				if (stack.IsEmpty())
					return false;

				bool isObject = stack[stack.Count() - 1];
				if (isObject != (c == 125))
					return false;

				// Either the container never held anything, or it just finished a
				// value. Anything else means a dangling comma or colon.
				bool afterOpen = empty && ((isObject && state == JSON_KEY) || (!isObject && state == JSON_VALUE));
				if (!afterOpen && state != JSON_NEXT)
					return false;

				stack.Remove(stack.Count() - 1);
				state = JSON_NEXT;
				empty = false;
				i++;
				continue;
			}

			if (c == 34)	// "
			{
				if (state != JSON_VALUE && state != JSON_KEY)
					return false;

				// Walk to the closing quote, stepping over an escaped one. Every
				// byte of a multi-byte character is above 127 and none of them can
				// be mistaken for a quote or a backslash, so this is byte-safe.
				i++;
				bool closed = false;
				while (i < len)
				{
					int s = text.ToAscii(i);
					if (s == 92)	// backslash
					{
						i += 2;
						continue;
					}

					if (s == 34)
					{
						closed = true;
						i++;
						break;
					}

					i++;
				}

				if (!closed)
					return false;

				if (state == JSON_KEY)
					state = JSON_COLON;
				else
					state = JSON_NEXT;

				empty = false;
				continue;
			}

			if (c == 58)	// :
			{
				if (state != JSON_COLON)
					return false;

				state = JSON_VALUE;
				i++;
				continue;
			}

			if (c == 44)	// ,
			{
				if (state != JSON_NEXT || stack.IsEmpty())
					return false;

				if (stack[stack.Count() - 1])
					state = JSON_KEY;
				else
					state = JSON_VALUE;

				empty = false;
				i++;
				continue;
			}

			// Anything else starts a bare literal: a number, true, false or null.
			// Its contents are the JSON layer's business; all that matters here is
			// that one is allowed at this point and that it ends where it should.
			if (state != JSON_VALUE)
				return false;

			// A literal ends at any structural byte, not merely at a comma or a
			// closing bracket. Stopping only at those let a literal swallow the
			// token after it, so a file missing one comma between a number and
			// the next key read as a single clean value - and the reader joins
			// lines without a separator, which is exactly how that file arrives.
			while (i < len)
			{
				int l = text.ToAscii(i);
				if (l == 44 || l == 125 || l == 93 || l == 32 || l == 9 || l == 10 || l == 13)
					break;

				if (l == 34 || l == 58 || l == 123 || l == 91)
					break;

				i++;
			}

			state = JSON_NEXT;
			empty = false;
		}

		// A document that ended mid-container, or never opened one at all.
		return stack.IsEmpty() && state == JSON_NEXT;
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
		// Substring() will not return more than this however much is asked of it,
		// and its own cut is not character-aware. A server setting a limit above
		// it therefore got the engine's blind cut instead of this function's, so
		// the limit is brought down to what can actually be honoured.
		if (limit > MAX_SUBSTRING)
			limit = MAX_SUBSTRING;

		if (limit <= 0 || value.Length() <= limit)
			return value;

		string cut = value.Substring(0, limit);

		// A space is always a single byte, so cutting there is always on a
		// character boundary. LastIndexOf answers -1 with no space present, and a
		// limit below 63 would let that -1 through into Substring as a negative
		// length, so both conditions have to hold.
		int lastSpace = cut.LastIndexOf(" ");
		if (lastSpace > 0 && lastSpace > limit - 64)
			return value.Substring(0, lastSpace);

		// No usable space. Japanese and Chinese carry none at all, which is the
		// very case the limit was written for, so the raw cut has to be walked
		// back off any continuation byte by hand: 10xxxxxx marks the middle of a
		// UTF-8 sequence, and stopping there leaves half a character behind.
		int end = limit;
		while (end > 0 && IsContinuationByte(value.ToAscii(end)))
		{
			end--;
		}

		return value.Substring(0, end);
	}

	//------------------------------------------------------------------------------
	//! Whether this byte is the middle of a UTF-8 sequence rather than its start.
	protected static bool IsContinuationByte(int value)
	{
		return (value & 0xC0) == 0x80;
	}

	//------------------------------------------------------------------------------
	//! Splits a file for transfer, cutting only at spaces.
	//!
	//! Substring() counts bytes, and rule texts are full of multi-byte characters
	//! — cutting at a fixed offset would eventually slice one in half and corrupt
	//! the two chunks around it. A space is always a single byte, so a cut there
	//! is always on a character boundary.
	//!
	//! Walked with an offset rather than by re-slicing the remainder. Substring()
	//! caps its output at 8191 characters, so taking the tail of a longer file
	//! returned a truncated tail and everything past it was lost — silently, with
	//! the server still logging the full byte count it had read. Reading forward
	//! from an offset means every call asks for at most a chunk.
	static void Split(string raw, int chunkSize, notnull out array<string> chunks)
	{
		int total = raw.Length();
		int offset = 0;

		while (offset < total)
		{
			int take = total - offset;
			if (take > chunkSize)
				take = chunkSize;

			// The last chunk keeps whatever is left, so it needs no boundary work.
			//
			// Walked back off UTF-8 continuation bytes rather than to the last
			// space. Both keep characters whole, but a space is wherever the file
			// happens to have one - so the chunk size, and with it the number of
			// packets sent to every joining player, depended on how the server
			// owner had formatted their JSON. A minified config whose structure is
			// long unbroken runs - GUIDs, paths, URLs - could multiply the packet
			// count many times over for a file of the same size. This gives back
			// at most three bytes.
			if (offset + take < total)
			{
				while (take > 1 && IsContinuationByte(raw.ToAscii(offset + take)))
				{
					take--;
				}
			}

			chunks.Insert(raw.Substring(offset, take));
			offset += take;
		}
	}
}
