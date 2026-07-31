# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

- Faction rules and a queue for a full faction, on a branch and not yet released.
  Player limits, whitelists and a balance rule as one system, with a queue that
  catches everyone they turn away. Design notes live on that branch.

## [1.0.3] - 2026-07-30

### Added

- `verboseLogging` in `report.json` turns the diagnostic log lines back on for a
  server that needs them.

### Fixed

- Verbose logging shipped switched on, burying the lines that matter in every
  server log.
- `maxDescription` was applied by byte count, so a limit landing inside a
  multi-byte character left half of one in the Discord payload and in the log
  file. Text is now cut on a character boundary, and at a space where one is
  near.
- Embed field names went into the JSON unescaped. They stopped being constants
  when the labels became overridable, so a quote in a `report.embed.*` override
  broke the whole embed.
- A `webhookAvatarUrl` that is not a URL made Discord reject the entire message,
  costing every report. It is dropped with a warning instead, and the rejection
  message names the settings to check.
- `logFile` accepted a path, placing the log outside the `MrFrost` folder. It
  falls back to `reports.log` and says so.
- The info menu stopped offering itself after a mission restart, and never
  appeared at all for a player who joined a second server in the same session.

## [1.0.2] - 2026-07-30

### Added

- `webhookUsername` and `webhookAvatarUrl` in `report.json`, passed to Discord
  so reports sign themselves with the server's name and badge. Left empty,
  Discord uses whatever the webhook was called in the channel settings.
- `colorPlayer` and `colorBug` set the embed's stripe colours, written as
  `"r,g,b"` in ordinary sRGB — the same notation as `accentColor` in
  `infomenu.json`.
- The six labels the Discord embed prints are now text-table keys
  (`report.embed.*`), translated into all thirteen languages and overridable
  per server.

### Fixed

- The embed's labels are resolved on the server, where the embed is built. A
  server could previously rename them for its players and still get English in
  its own moderation channel.

## [1.0.1] - 2026-07-30

### Added

- Two more link slots in the info menu footer beside Discord: a website on
  <kbd>W</kbd> and one on <kbd>L</kbd> that a server names itself. Both are
  configurable from `infomenu.json` and the addon config, and both open through
  the platform service, which is what makes them work on console.

### Fixed

- The report cooldown compared a stored world time against the current one
  without allowing for a mission restart putting the clock back to zero. A stamp
  from the previous mission then sat in the future and reported every player as
  on cooldown until the clock caught up — minutes, or an hour on a long-running
  server.

## [1.0.0] - 2026-07-29

### Added

- **Info menu** on <kbd>F11</kbd>. Rules, onboarding and SOPs as a browsable
  list of categories and entries, with rich text, icons, a per-server title and
  accent colour, and a Discord button that opens on console as well as on PC.
- **Reports** on <kbd>F10</kbd>. Bug and player reports, sent by holding a key.
  A player is named by picking from a list, by who last killed or injured the
  reporter, or as everyone within a radius; for the last three the client sends
  only which option was chosen and the server resolves it from the kills and
  hits it recorded itself.
- **Delivery** to a Discord embed and a log file on the server, each written
  independently of the other.
- **Localisation** into thirteen languages, taken from the player's game
  language, with every string overridable per server.
- **Server-side content.** Both menus read their content from JSON files in the
  server's profile directory, so one published mod carries different content on
  every server that runs it.

[Unreleased]: https://github.com/MrFrostDev/MrFrost-ServerEssentials/compare/v1.0.3...HEAD
[1.0.3]: https://github.com/MrFrostDev/MrFrost-ServerEssentials/compare/v1.0.2...v1.0.3
[1.0.2]: https://github.com/MrFrostDev/MrFrost-ServerEssentials/compare/v1.0.1...v1.0.2
[1.0.1]: https://github.com/MrFrostDev/MrFrost-ServerEssentials/compare/v1.0.0...v1.0.1
[1.0.0]: https://github.com/MrFrostDev/MrFrost-ServerEssentials/releases/tag/v1.0.0
