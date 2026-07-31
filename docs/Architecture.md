# Architecture

[← Back to index](Home.md)

Notes for anyone modifying the addon, adding a feature to it, or combining it with other mods.

## Layout of the addon

Everything below is relative to `addon/`, the folder the engine mounts. Documentation and repository files sit outside it.

```
Configs/
  MrFrost/
    InfoMenu.conf              fallback content for the info menu
    Report.conf                fallback settings for the report menu
    Language.conf              every string the addon itself shows
  System/
    chimeraMenus.conf          registers every menu preset
    chimeraInputCommon.conf    declares the input actions and contexts
    keyBindingMenu.conf        adds the controls-settings category
UI/layouts/MrFrost/
    MrFrostMenuFrame.layout    the panel every MrFrost menu is drawn in
    MrFrostEmpty.layout        an empty body, used as tab content
    InfoMenu/                  the info menu's body and its row
    Report/                    the report menu's body
scripts/Game/MrFrost/
    Core/                      shared by every feature
    InfoMenu/                  the info menu
    Report/                    the report menu
    MrFrost_Features.c         the list of features
    MrFrost_MenuPresets.c      the menu preset enum
    MrFrost_PauseMenu.c        pause menu entries
    MrFrost_PlayerController.c keys, input contexts, content transfer
server/MrFrost/                example server files, not part of the addon
```

## What is shared, and why

A second feature is meant to cost almost nothing. Everything below is written once and used by every menu:

| | |
|---|---|
| `MrFrost_MenuBase` | Panel, header, footer, fade, input context. A menu overrides `GetContentLayout()` and `OnMenuBuilt()` and gets the rest |
| `MrFrostMenuFrame.layout` | The panel itself, with an empty `ContentRoot` a feature fills |
| `MrFrost_ServerContent` | Reading, validating and chunking the server's files |
| `MrFrost_PlayerController` | One transfer that carries every feature's content, one context pump for every key |
| `MrFrost_PauseMenu` | Builds an entry per feature from one list |
| `MrFrost_Log` | One prefix, so `grep MrFrost` tells the whole story |

The frame is worth its own file for a specific reason: its colours, sizes, blur and footer were taken from the vanilla group menu one value at a time. Copying that per menu would mean every future menu drifting away from vanilla on its own schedule.

`MrFrost_Features.Init()` is the single list of what exists. The pause menu builds its entries from it, and the transfer takes its channel order from it — so adding a feature is a change in one file, and there is no second list to fall out of sync.

## Where content comes from

Two sources, resolved per feature — for the info menu in `MrFrost_InfoMenuConfigLoader.Get()`:

1. **The server's `$profile:MrFrost/infomenu.json`**, sent to this client on join
2. **The addon config**, as a fallback

The server wins whenever it has content. That is the whole reason a single published mod can carry a different info menu on every server.

### Why JSON and not a .conf

A `.conf` only resolves through the resource database, which is built when the addon is packaged. A file dropped onto a running server is not in it and never can be. JSON is parsed from plain text — which is exactly what an unmanaged file is.

### Why the client asks

The transfer is a request from the client, not a push from the server. Only the client knows when it is ready to hold the content; a server guessing that moment gets it wrong on slow joins and the content arrives before anything can receive it.

### Why it is chunked

`Substring()` counts bytes, and rule texts are full of multi-byte characters. Cutting at a fixed offset would eventually slice one in half and corrupt both chunks around it, so cuts land on spaces — always a single byte, therefore always a character boundary.

Packets are reassembled by index, not arrival order: reliable delivery guarantees everything arrives, not the order it arrives in, and an info menu stitched back together wrongly would be nonsense that still parses.

## Vanilla files it overrides

Three, each keeping the original GUID so the engine resolves to our copy:

| File | Why |
|---|---|
| `chimeraMenus.conf` | A menu preset can only be registered here |
| `chimeraInputCommon.conf` | Input actions are declared here |
| `keyBindingMenu.conf` | Adds the category to the controls settings |

> [!WARNING]
> Overriding by GUID means another mod doing the same to the same file can conflict. There is no way around this for menu registration — it is how Bohemia documents it.

Everything else is **additive**. The pause menu entry and the key listener use `modded class` and touch no vanilla assets.

## Scripts

| File | Role |
|---|---|
| `Core/MrFrost_Log.c` | Logging with a single prefix |
| `Core/MrFrost_Text.c` | The text table: language, and a server's own wording |
| `Core/MrFrost_MenuBase.c` | Shared menu: frame, header, footer, fade, input context |
| `Core/MrFrost_ServerContent.c` | Server files: the channel registry, reading, chunking |
| `InfoMenu/MrFrost_InfoMenuConfig.c` | Data model and the loader that picks the source |
| `InfoMenu/MrFrost_InfoMenuJson.c` | The server's JSON and its transfer channel |
| `InfoMenu/MrFrost_InfoMenuUI.c` | The menu body: builds rows, wires the Discord prompt |
| `InfoMenu/MrFrost_InfoMenuRowComponent.c` | One row: icon, tree connector, selection state |
| `MrFrost_Features.c` | The single list of what this addon contributes |
| `MrFrost_MenuPresets.c` | `modded enum ChimeraMenuPreset` |
| `MrFrost_PauseMenu.c` | `modded class PauseMenuUI` — one entry per feature |
| `MrFrost_PlayerController.c` | `modded class SCR_PlayerController` — keys, contexts, transfer, report submit |
| `Report/MrFrost_ReportConfig.c` | Data model, and the split between menu settings and secrets |
| `Report/MrFrost_ReportJson.c` | The server's JSON and its transfer channel |
| `Report/MrFrost_ReportTracker.c` | `modded class SCR_BaseGameMode` — who killed and injured whom |
| `Report/MrFrost_ReportSubmit.c` | Rate limiting, target resolution, building the report |
| `Report/MrFrost_ReportDelivery.c` | Log file and the Discord webhook queue |
| `Report/MrFrost_ReportUI.c` | The report menu: tabs, dropdowns, hold-to-send |

## Adding a feature

A new menu is six small edits and no new plumbing:

1. **`MrFrost_MenuPresets.c`** — add the enum value.
2. **`addon/Configs/System/chimeraMenus.conf`** — map that name to `MrFrostMenuFrame.layout` and your class. Every menu points at the same frame; the class is what differs.
3. **A body layout** under `addon/UI/layouts/MrFrost/<Feature>/`. Its root needs an `OverlayWidgetSlot`, because `ContentRoot` is an overlay.
4. **A class extending `MrFrost_MenuBase`** — override `GetContentLayout()` and `OnMenuBuilt()`. Panel, header, footer, fade and input context are already there.
5. **`MrFrost_Features.c`** — add a `MrFrost_MenuEntry` for the pause menu entry, and a `MrFrost_ServerContentChannel` if the feature has server-side content.
6. **`chimeraInputCommon.conf`** — if it needs a key, add the action to `MrFrostContext` (in-game) or the stock `MenuContext` (menu-only), plus a line in `keyBindingMenu.conf`.

Nothing else has to know the feature exists. The transfer, the pause menu and the controls category all read from the list in step 5.

> [!IMPORTANT]
> Channel order in `MrFrost_Features.Init()` is the wire format — a transfer packet names its channel by index into that list. Add channels at the end, and never reorder them without both sides shipping together.

## Design decisions

### Rows are created once

Every row — category and entry alike — is built when the menu opens and then only shown or hidden. Collapsing does not destroy anything.

That costs a handful of widgets for a config of this size and avoids all the bookkeeping a rebuild-on-click would need: index remapping, re-resolving the selection, re-attaching invokers.

### Tree connectors are drawn, not typed

The lines linking entries to their category are thin image widgets, not box-drawing characters like `├`. The shipped UI fonts are not guaranteed to carry those glyphs, and a missing glyph renders as an empty box.

The same reasoning applies to the `+` / `-` expand markers.

### Colours come from the game

`MrFrost_MenuBase.ApplyChrome()` and each menu's own `ApplyPalette()` set colours from named `UIColors` constants at runtime. The values in the layout are placeholders that get overwritten — they exist so the Workbench preview looks right, since the preview renders layouts without running script.

Panel tone, size and blur are taken verbatim from the vanilla group menu.

> [!IMPORTANT]
> Colours in `.layout` files are **linear**, not sRGB. A value that looks like `#121212` in a colour picker has to be written as roughly `0.005`. Getting this wrong produces a washed-out grey panel.

### The footer is the game's own

The back prompt is built through `SCR_DynamicFooterComponent` using `WLib_NavigationButton.layout` — the exact widget `SCR_SuperMenuComponent` hands to every stock menu.

Going through the component rather than placing a button by hand is what makes the prompt identical to other menus instead of merely similar, and it is what resolves controller glyphs on console.

### The pause menu icon is found by shape, not name

The image widget inside the vanilla pause menu button has **no name** — it sits unnamed several levels down, under a `ScaleWidget`. It cannot be looked up by name, so the code searches the subtree for an image under a scale widget.

Taking simply the first image in the subtree finds the button's own arrow decoration instead.

## Gotchas found the hard way

**A script method cannot take a function reference.** Passing a callback to a helper fails to compile with `func arguments are not supported in script methods`. A helper that would wire up several near-identical buttons therefore has to return the button and let each caller attach its own handler — see `MrFrost_InfoMenuUI.BuildLinkButtons()`, where three link slots are spelled out for exactly this reason.

**A key name in an input action is only checked when a mission starts.** `chimeraInputCommon.conf` is not loaded at the main menu, so the headless compile check never sees it — a binding naming a button that does not exist passes every check and then fails in game:

```
INPUT (E): InputManager: unknown key 'gamepad0:dpad_up'
```

The button is called `pad_up`. The names that exist can be read off the vanilla configs rather than guessed:

```
grep -rhoE '"gamepad0:[a-z_0-9]+"' vanilla-export/ reference/ | sort -u
```

**`WLib_TabViewHorizontal` sizes its accent line by fill weight, not in pixels.** Its root is a vertical layout of three: a 40 px tab row, then `Separator` at weight `0.005` and `ContentOverlay` at `0.95`, sharing whatever is left over. A menu that gives the tab view the whole panel gets a few pixels of line. A menu that gives it only the height of the bar gets half a percent of almost nothing, and the line disappears — while still being drawn, which is why nothing shows up in any log.

The report menu renders its form *below* the tab view rather than inside it, so `ShowTabSeparator()` hands the whole remainder to the separator and none to the unused content overlay. The tab slot is 44 px: 40 for the bar, 4 for the line, which is the thickness vanilla authored.

**The same tab view hangs its paging buttons 48 px outside its own bounds** (see the negative padding in the layout). A container narrower than that overhang cuts the left button off entirely, and any ancestor with `Clipping True` hides it. The form's margin is 56 px for this reason, and `Clipping False` is set along the chain.

**A `.conf` must not contain `//` comments.** They are not ignored — they desynchronise the parser, which then rejects perfectly valid lines further down and can take the engine with it:

```
DEFAULT (E): Unknown keyword/data 'Sources' at offset 2107
DEFAULT (E): Unknown class 'Contexts' at offset 3207
ENGINE  (F): Crashed
```

The reported offset is where the parser noticed, not where the comment was, so the error points at innocent code. Nothing in `script.log` mentions it either; it is a `DEFAULT (E)` line in `console.log`.

This is why the input actions are documented here rather than in the file they describe.

**The input actions, since they cannot carry comments of their own:**

| Action | Key | Notes |
|---|---|---|
| `MrFrost_OpenInfoMenu` | F11 | In `MrFrostContext`, which the player controller renews on a timer, and in `CharacterGeneralContext` |
| `MrFrost_OpenReportMenu` | F10 | Same two contexts |
| `MrFrost_ReportSubmit` | Enter / right trigger | `MenuContext`. Carries `InputFilterHoldOnce` |
| `MrFrost_MenuDiscord` | D / gamepad Y | `MenuContext` |
| `MrFrost_MenuWebsite` | W / gamepad X | `MenuContext` |
| `MrFrost_MenuCustom` | L / gamepad right stick click | `MenuContext` |

The three link actions exist whether or not a server fills the matching slot. An action with no button behind it fires nothing, which costs less than making the keybinding category change shape per server.

`FilterPreset` on its own does nothing — it is the label the controls settings group a binding under. The behaviour comes from the `Filter` object declared next to `Input`.

**A hold prompt needs `InputFilterHoldOnce`, not `InputFilterHold`.** This one cost hours. `InputFilterHold` produces an action that reports no value at all — no listener fires, `GetActionValue()` stays at zero, and the key looks unbound however it is declared. Nothing in the log says so. `InputFilterHoldOnce` is what `MenuAddGroup` carries in the group menu, and it is the class `SCR_InputButtonComponent` draws a fill for and activates at the end of.

Spell out `HoldDuration` too: an empty filter body falls back to the project default, which is short enough to read as a click.

The failure looks exactly like a wrong key or a dead context, which is the trap — the mouse keeps working throughout, because a click reaches `OnInput()` without going through the action at all.

**A registered action still needs an active context.** An action fires only while some *active* input context carries it, and nothing activates a context the game has never heard of. Attach an action to a misspelled or invented context name and it shows up in the controls settings, rebinds cleanly, and never triggers — with no error anywhere.

For gameplay keys the addon declares its own `MrFrostContext` and renews it on a timer from `MrFrost_PlayerController`, because no vanilla context reliably carries them.

Menu keys need none of that: they live in the stock `MenuContext`, which the engine raises for whichever menu is open. An earlier version handed the menu a context of its own through `MenuBase.SetActionContext()` — no vanilla menu does that, and it is not needed.

**The engine's own string table is `StringTableRuntime`**, in `Language/localization.<locale>.conf`, and it is two parallel arrays:

```
StringTableRuntime {
 Ids   { "AR-Menu_Back" ... }
 Texts { "Zurück" ... }
}
```

The addon does not use it. Whether an addon's own file at that path is picked up is unverified, and a server owner could not override engine strings anyway — which is half of what the text layer is for. `MrFrost_Text` is a table we own: it cannot silently fail to load, and the JSON override layer sits on top of it. Migrating later is possible now that the format is known; running both at once is not, because the strings would drift.

**Reserved words bite in unexpected places.** `override` cannot be a variable name. Neither can a member collide with one on `MenuBase` — `SetLabel` is already taken by the engine's own menu class, and the error names the file, not the base.

**EnforceScript has no ternary operator.** `a ? b : c` fails to compile with a misleading "Broken expression" message.

**`ActionManager` cannot be built from a `.conf`.** `SCR_ConfigHelperT<ActionManager>.GetConfigObject()` returns null, and its destructor is private so script cannot own one either. Runtime registration through `RegisterActionManager` is therefore not usable this way.

**A widget's `Color` in a layout is linear.** See above.

**Widgets clamp to their parent unless told otherwise.** A scroll area's child needs `VerticalAlign 0`, or it is clipped to the viewport instead of overflowing — and then nothing scrolls.

**The Workbench "Override" produces an empty stub**, not a copy. It inherits from the original. Shipping such a stub for a vanilla layout replaces that layout with nothing. Use "Duplicate" to get readable content.

## Verifying changes

The engine can be run headless to compile-check scripts without opening the Workbench:

```
ArmaReforgerSteam.exe -profile <temp> -addonsDir "<mods>" -addons 69F71634BA1052A0 -noSound -maxFPS 10
```

Diagnostics land in `<temp>/logs/logs_*/console.log`. Three things fail independently and all three matter: `SCRIPT\s+\(E\)` for a script that does not compile, `DEFAULT\s+\(E\)` for a `.conf` that does not parse, and `ENGINE\s+\(F\)` for a crash. The engine then fails on the missing world, which is expected — script compilation happens first.

This does not validate layouts or configs, only script.
