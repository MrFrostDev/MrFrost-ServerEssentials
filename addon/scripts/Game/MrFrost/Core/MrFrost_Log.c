//------------------------------------------------------------------------------
//! Logging for every MrFrost feature.
//!
//! One prefix across the whole addon, so a server owner greps `MrFrost` once and
//! has the story of every feature in one place instead of learning a prefix per
//! menu.
//!
//! Verbose logging is off unless a server asks for it. A shipped build that
//! logs every menu open and every content chunk buries the lines that matter in
//! a server log somebody has to read.
//------------------------------------------------------------------------------
class MrFrost_Log
{
	static const string PREFIX = "[MrFrost] ";

	//! Diagnostics switch. Off by default; a server turns it on with
	//! "verboseLogging": true in its report.json, which is where the rest of its
	//! operational settings live.
	static bool s_bVerbose = false;

	//------------------------------------------------------------------------------
	static void SetVerbose(bool enabled)
	{
		if (s_bVerbose == enabled)
			return;

		s_bVerbose = enabled;

		if (enabled)
			Info("Verbose logging is on.");
	}

	//------------------------------------------------------------------------------
	static void Info(string msg)
	{
		Print(PREFIX + msg, LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------
	static void Warn(string msg)
	{
		Print(PREFIX + msg, LogLevel.WARNING);
	}

	//------------------------------------------------------------------------------
	static void Error(string msg)
	{
		Print(PREFIX + msg, LogLevel.ERROR);
	}

	//! Warnings already said, by their key.
	protected static ref set<string> s_aSaidOnce;

	//------------------------------------------------------------------------------
	//! Says something once per process, however often it is asked.
	//!
	//! For a complaint about configuration. A server file is read at startup and
	//! cannot change while the server runs, so a warning about one has nothing new
	//! to say on the second report, or the two-thousandth - but the paths that
	//! notice run per report, and the line was written every time. That is a slow
	//! way to fill a disk using the diagnostic switch an owner turned on to find
	//! the problem.
	//!
	//! The key is what makes two warnings the same warning, so it names the
	//! setting rather than carrying the message text: a line quoting the offending
	//! value would otherwise be a new key each time the value differed.
	static void WarnOnce(string key, string msg)
	{
		if (!s_aSaidOnce)
			s_aSaidOnce = new set<string>();

		if (s_aSaidOnce.Contains(key))
			return;

		s_aSaidOnce.Insert(key);
		Warn(msg);
	}

	//------------------------------------------------------------------------------
	//! Compiled in always, gated so a release build pays only a bool check.
	static void Debug(string msg)
	{
		if (s_bVerbose)
			Print(PREFIX + msg, LogLevel.NORMAL);
	}
}
