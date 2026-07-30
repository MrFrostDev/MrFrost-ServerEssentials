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

	//------------------------------------------------------------------------------
	//! Compiled in always, gated so a release build pays only a bool check.
	static void Debug(string msg)
	{
		if (s_bVerbose)
			Print(PREFIX + msg, LogLevel.NORMAL);
	}
}
