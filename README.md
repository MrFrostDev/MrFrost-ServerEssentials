<div align="center">

<img src="https://github.com/MrFrostDev/MrFrost-ServerEssentials/releases/download/v1.0.0/banner.png" alt="MrFrost Server Essentials — in-game info menu and player reports for Arma Reforger servers" width="100%">

# MrFrost Server Essentials

**An in-game info menu and report system for Arma Reforger servers.**

The content lives on your server — one published addon, a different setup on every server that runs it.

[![Workshop](https://img.shields.io/badge/Workshop-subscribe-E2A74F?style=flat-square)](https://reforger.armaplatform.com/workshop/697F04F543907FAF)
[![Game](https://img.shields.io/badge/Arma_Reforger-1.3+-4B5563?style=flat-square)](https://reforger.armaplatform.com/)
[![Platform](https://img.shields.io/badge/platform-PC_%7C_Xbox_%7C_PlayStation-4B5563?style=flat-square)](#requirements)
[![Script](https://img.shields.io/badge/Enfusion_Script-1F2937?style=flat-square)](addon/scripts/Game/MrFrost)
[![Content](https://img.shields.io/badge/content-server--side_JSON-2563EB?style=flat-square)](#one-mod-many-servers)
[![License](https://img.shields.io/badge/license-MIT-16A34A?style=flat-square)](license.txt)
[![Docs](https://img.shields.io/badge/docs-read-16A34A?style=flat-square)](docs/Home.md)

</div>

---

## What this is

Two menus, added to the game and filled from your server:

- **<kbd>F11</kbd> — an info menu.** Your rules, onboarding and SOPs as a browsable list, in game.
- **<kbd>F10</kbd> — a report menu.** Bug and player reports, delivered to a Discord channel and a log file on your server.

Both are also in the pause menu, both work on a controller, and both follow the player's game language.

The content is a JSON file in your server's profile directory. Edit it, restart the server, and players see the new text when they next join. Every server running this addon serves its own content from the same published mod.

## What a player sees

### The info menu — <kbd>F11</kbd>

![The info menu in game: category tree on the left, formatted text on the right](https://github.com/MrFrostDev/MrFrost-ServerEssentials/releases/download/v1.0.0/infomenu.png)

Two levels of hierarchy: categories, each holding entries. The text supports bold, colours and inline icons. Title, accent colour and icons come from your config.

The footer holds up to three link buttons — Discord on <kbd>D</kbd>, your website on <kbd>W</kbd>, and one more on <kbd>L</kbd> that you name yourself: a ruleset, a ban appeal form, a Teamspeak address. Each appears once its URL is set, and each opens through the platform, which is what makes them work on console.

The menu is built from the game's own widget library, so it matches the vanilla settings and group menus.

### The report menu — <kbd>F10</kbd>

![Reporting a player: a dropdown for who, a dropdown for which player, and a description field](https://github.com/MrFrostDev/MrFrost-ServerEssentials/releases/download/v1.0.0/report-player.png)

A form that asks in order: which kind of report, who it concerns, what happened. Each tab shows the rows that apply to it, so the bug tab asks for a description alone:

![Reporting a bug: the same form with the target rows gone, leaving only the description](https://github.com/MrFrostDev/MrFrost-ServerEssentials/releases/download/v1.0.0/report-bug.png)

A player is named one of four ways: picked from a list, *whoever last killed me*, *whoever last injured me*, or *everyone within 300 m*. For the last three the client sends **which option** was chosen, and the server resolves it from the kills and hits it recorded itself.

Sending is a hold. The footer prompt fills while the key is held down and fires when the fill completes.

### In the pause menu

<img src="https://github.com/MrFrostDev/MrFrost-ServerEssentials/releases/download/v1.0.0/pausemenu.png" alt="The pause menu with Server Info and Report entries added below the vanilla ones" width="360">

Both menus appear in the pause menu. Labels and icons come from your config — a server that calls its info menu *Regelwerk* gets that word here too.

An entry appears when its feature is enabled and has content to show.

### What you get

Every report goes to two places, each written independently of the other.

**A Discord embed**, with a coloured stripe telling the two kinds apart at a glance in a busy channel:

<img src="https://github.com/MrFrostDev/MrFrost-ServerEssentials/releases/download/v1.0.0/discord.png" alt="Two Discord embeds: a player report with a red stripe, listing reporter, target, position and time, and a bug report with an amber one, listing reporter, position and time" width="420">

`serverName` becomes the footer. The time appears as the server's clock in the field and as Discord's own timestamp beside the footer, which every reader sees in their own timezone.

**A log file on your server**, one line per report, appended while the server runs:

```
2026-07-29 21:24:07 | PLAYER | by Meier (12) | against Schulz (7) | at 4821 / 9134 | shot me in the back at the arsenal
2026-07-29 21:31:55 | BUG | by Meier (12) | at 4102 / 8890 | the vehicle spawner at Levie eats supplies and gives nothing
```

## One mod, many servers

The addon reads its content from the server:

```
<server profile directory>/MrFrost/
    infomenu.json
    report.json
```

Drop that folder next to your server config. On join, the server hands its content to the player and the menus render it. One published mod, different content everywhere it runs.

Where those files are absent, the content bundled with the addon is shown, which covers single player and local testing.

## Features in full

| | |
|---|---|
| **Info menu** | Categories and entries, rich text, icons, per-server accent colour, optional open-on-join |
| **Reports** | Bug and player reports, four ways to name a target, server-side resolution, hold-to-send |
| **Delivery** | Discord embed **and** a log file, each written independently of the other |
| **Localisation** | Thirteen languages, picked from the player's game language; every string overridable per server |
| **Per-server config** | Each menu can be switched off; its menu, pause entry and key go together |
| **Console** | Controller support throughout, and link buttons that open on console |
| **Anti-radar** | "Everyone within 300 m" files the report without disclosing whether anyone was in range, until you enable it |

Both menus share one panel, one input category and one folder of server-side content.

## Quick start

### Running a server

1. Add the mod as a dependency in your server config:

   ```json
   { "modId": "697F04F543907FAF", "name": "MrFrost Server Essentials" }
   ```
2. Copy [`addon/server/MrFrost/`](addon/server/MrFrost) into your server's profile directory and write your own text into it.
3. Restart the server.

### Single player, or one server you also build the addon for

1. Edit `addon/Configs/MrFrost/InfoMenu.conf`.
2. Restart the game.

Players open the info menu with <kbd>F11</kbd> and the report menu with <kbd>F10</kbd>, or from the **pause menu**. Both keys are rebindable under *Settings → Controls → MrFrost*. A report is sent by **holding <kbd>Enter</kbd>**, or the right trigger on a controller.

> [!IMPORTANT]
> Save every file as **UTF-8**. In the addon config, give each category and entry its own **unique GUID** — copying a block while keeping its GUID silently drops one of the two.

> [!TIP]
> Server files are read once at startup, like the rest of your server configuration. Edited one? Restart the server.

> [!WARNING]
> The Discord **webhook URL is a secret** — anyone holding it can post into that channel. It belongs in your server's `report.json`. The addon config ships to every subscriber.

## Documentation

| Page | What it covers |
|---|---|
| [Installation](docs/Installation.md) | Server setup, local testing, when the Workbench is required |
| [Server content](docs/ServerContent.md) | The JSON files, their keys, languages, and how they reach players |
| [Reports](docs/Reports.md) | The report menu, Discord webhook and log file |
| [Configuration](docs/Configuration.md) | Every field of the addon config, with examples |
| [Text formatting](docs/Formatting.md) | Rich text markup and layout conventions |
| [Icons](docs/Icons.md) | Verified sprite names and using your own artwork |
| [Presets](docs/Presets.md) | Shipping and switching complete configurations |
| [Troubleshooting](docs/Troubleshooting.md) | Log messages and what they mean |
| [Architecture](docs/Architecture.md) | How the addon is built, for modders |
| [Changelog](CHANGELOG.md) | What changed in each release |

Start at [docs/Home.md](docs/Home.md) for the full index.

## Repository layout

```
addon/                        the addon, and the only thing the game loads
  Configs/MrFrost/            addon content, the fallback for each feature
    InfoMenu.conf             the info menu's categories and entries
    Report.conf               the report menu's settings
    Language.conf             every string the addon itself shows
  Configs/System/             menu registration, input actions, key bindings
  UI/layouts/MrFrost/         the shared menu frame, then one folder per feature
  scripts/Game/MrFrost/Core/  everything shared between features
  scripts/Game/MrFrost/…/     one folder per feature
  server/MrFrost/             example server files - copy this into your profile dir
docs/                         the documentation linked above
.github/workflows/            builds the release archive from addon/ alone
```

Everything the game reads lives under `addon/`, which is the folder you point
`-addonsDir` at. Documentation, the README and the repository's own `.git`
directory sit beside it, so they stay out of anything built or published from
the addon.

## Requirements

- Arma Reforger
- [Arma Reforger Tools](https://store.steampowered.com/app/1874910/) — needed for adding files to the addon and for publishing

## Limitations

- The info menu has two levels of hierarchy: categories and entries.
- Content is read at startup.
- There is no search box.
- Reports carry no screenshot. The script API exposes screenshot pixels as an opaque pointer, and the REST layer sends strings.

> [!NOTE]
> Reports, the log file and the Discord webhook are verified working. The **server-to-client content transfer** is compile-verified and still awaits a run on a live dedicated server — local hosting reads the files directly. The `[MrFrost]` lines in the server and client logs say how far it got. See [Troubleshooting](docs/Troubleshooting.md).

## Contributing

[Contributing guide](CONTRIBUTING.md) · [Code of Conduct](CODE_OF_CONDUCT.md) · [Security policy](SECURITY.md) · [Changelog](CHANGELOG.md)

## Credits

By **MrFrost**. Released under the [MIT licence](license.txt).
