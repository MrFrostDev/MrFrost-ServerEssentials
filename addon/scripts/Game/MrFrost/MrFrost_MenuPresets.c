//------------------------------------------------------------------------------
//! Registers every MrFrost menu with the engine's menu manager.
//!
//! The enum value alone is not enough: an entry of the *same name* must exist in
//! Configs/System/chimeraMenus.conf, which maps it to a layout and to the class
//! that drives it. Renaming a value below therefore means renaming it there too,
//! or the engine will not find the menu and OpenMenu() silently fails.
//------------------------------------------------------------------------------
modded enum ChimeraMenuPreset
{
	MrFrost_InfoMenu,
	MrFrost_ReportMenu,
}
