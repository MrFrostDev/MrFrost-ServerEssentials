# Installation

[← Back to index](Home.md)

## On a server

Add the mod as a dependency in your server config. Clients download it automatically when they join.

Then put your content next to your server config:

```
<server profile directory>/MrFrost/
    infomenu.json
    report.json
```

Those files are what your players see. They live on the server, not in the addon, so the mod you depend on is the same one every other server depends on — no per-server build, no second download for players. A ready-made folder to copy is in the repository under `addon/server/MrFrost/`. Full reference in [Server content](ServerContent.md).

Without them, players see the sample content bundled with the addon.

## Testing locally

With the mod unpacked in a folder:

```
ArmaReforgerSteam.exe -addonsDir "<folder containing addon/>" -addons 69F71634BA1052A0
```

`-addonsDir` looks one level down for an `addon.gproj`, so it points at the repository root and finds `addon/`. The flag can be repeated to load several mods from different places.

> [!NOTE]
> The working directory must be the game folder. `-addonsDir` adds a search path but does not make the engine's own `./addons` absolute — start it anywhere else and the engine aborts because it cannot find its base data.

A convenience script:

```powershell
Start-Process "D:\Steam\steamapps\common\Arma Reforger\ArmaReforgerSteam.exe" `
  -ArgumentList '-addonsDir "C:\path\to\MrFrost-ServerEssentials" -addons 69F71634BA1052A0' `
  -WorkingDirectory "D:\Steam\steamapps\common\Arma Reforger"
```

## When you need the Workbench

| What you did | What is needed |
|---|---|
| Edited text inside an existing file | Restart the game |
| Added, deleted or renamed a file | Open the project once in the **Arma Reforger Workbench**, then restart |

Only the Workbench rebuilds `resourceDatabase.rdb`, the index that maps GUIDs to files. The game engine reads that index but never regenerates it — deleting the file does not help either.

Skip this step after adding a file and the engine simply will not know it exists. The symptom is a menu that does not open with nothing in the log.

## Verifying it works

Join a mission and press **F11** for the info menu or **F10** for the report menu — or open the pause menu, where both sit next to the Field Manual.

A report is sent by holding **Enter** (right trigger on a controller). It lands in `MrFrost/reports.log` on the server, and in Discord if you configured a webhook.

If nothing happens, check the log — see [Troubleshooting](Troubleshooting.md).

---

**Next:** [Server content](ServerContent.md)
