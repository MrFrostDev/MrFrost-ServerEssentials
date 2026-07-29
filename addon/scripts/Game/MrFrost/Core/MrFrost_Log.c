//------------------------------------------------------------------------------
//! Logging for every MrFrost feature.
//!
//! One prefix across the whole addon, so a server owner greps `MrFrost` once and
//! has the story of every feature in one place instead of learning a prefix per
//! menu.
//!
//! Verbose logging is meant for bring-up. It must be off in a release build.
//------------------------------------------------------------------------------
class MrFrost_Log
{
	static const string PREFIX = "[MrFrost] ";

	//! Diagnostics switch. Enabled during bring-up, disabled for release.
	static bool s_bVerbose = true;

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
