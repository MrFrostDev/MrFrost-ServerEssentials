# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

- Faction rules and a queue for a full faction, on a branch and not yet released.
  Player limits, whitelists and a balance rule as one system, with a queue that
  catches everyone they turn away. Design notes live on that branch.

## [1.0.3] - 2026-07-31

### Security

- `report.json` was sent to every client exactly as it sat on disk, `delivery`
  block included. Any player on a server with a Discord webhook received that
  webhook's URL, which is enough to post into the channel. The file is now
  repacked without that block before it is transferred, and the result is
  checked for the block before it goes out — a repack that cannot be verified
  sends nothing at all rather than risk it. Any server that ran an earlier build
  with a webhook configured should treat that URL as exposed and regenerate it.

### Added

- `verboseLogging` in `report.json` turns the diagnostic log lines back on for a
  server that needs them.

### Changed

- Server files are read when the server starts rather than when the first player
  asks for them. Until now a dedicated server ran on the addon's bundled
  settings until somebody connected, and a broken file was only reported at that
  same late moment instead of at startup where an owner is watching.
- `report.json` and `infomenu.json` are accepted as long as they are JSON
  objects. Every key in both is optional, so requiring particular ones rejected
  ordinary files — and rejection is not soft: it costs a server its entire
  configuration, webhook included, for the life of the process.
- A server may now send nothing but string overrides, a title or a link, and
  keep the bundled info menu content around them. That was previously reported
  as a broken file.
- Server files are now checked for structural soundness before they are used,
  and a file that fails is named on the server console with what to look for.

### Removed

- The bundled `Configs/MrFrost/Presets/` directory. Its one preset duplicated
  the shipped info menu config and never had a second entry to switch between.

### Fixed

- A stray comma anywhere in a server file came back as every setting at its
  default, because the JSON layer reports a parse failure by returning a struct
  full of them. A server that had switched reporting off had it switched back
  on, on both sides, and no console said a word.
- A player who left one server and joined another without restarting the game
  carried the first server's rules, title, colours and Discord invite with them,
  because a server that ships no file of its own sends nothing rather than an
  instruction to go back to the bundled content.
- An `infomenu.json` with no categories discarded its own title, accent colour
  and all three footer links along with the content it did not have.
- The check that keeps the `delivery` block out of a transfer was defeated by a
  space before a colon, which is how a hand-formatted file is usually written.
- A report could be lost to Discord over the combined size of the embed rather
  than any one part of it. The description now takes what the labels around it
  leave, so the report arrives shortened instead of not at all.
- Reports reached the Discord channel out of the order they were made when
  several landed inside the same rate-limit window, so the channel and
  `reports.log` disagreed about what happened first.
- A `webhookUsername` longer than Discord accepts cost every report, with a
  rejection message that named three settings and not the length of any of them.
- A dedicated server enforced the addon's bundled report settings while every
  client honoured the server's file, because the server-side path never
  installed them. A server that had switched reporting off still accepted
  reports from a modified client.
- Server files larger than roughly 9 KB lost everything past that point in
  transfer. `Substring()` caps its result at 8191 characters and the splitter
  took the tail of the remainder, so the loss was silent — the server logged
  the full byte count it had read.
- Reports could name a player who had nothing to do with them. Kill and damage
  records outlived the players in them, so a report against "whoever last shot
  me" could resolve to whoever inherited that player id after they left.
- A modified client could hold the server in a permanent resend by repeating its
  content request, and a player leaving mid-transfer left repeating timers
  firing against a freed controller.
- Verbose logging shipped switched on, burying the lines that matter in every
  server log.
- `maxDescription` was applied by byte count, so a limit landing inside a
  multi-byte character left half of one in the Discord payload and in the log
  file. Text is now cut on a character boundary, and at a space where one is
  near.
- Embed field names went into the JSON unescaped. They stopped being constants
  when the labels became overridable, so a quote in a `report.embed.*` override
  broke the whole embed.
- Embed titles, field names and the footer were sent to Discord unbounded, so a
  server that renamed a label past Discord's limit for that slot lost every
  report to a rejected message.
- A `webhookAvatarUrl` that is not a URL made Discord reject the entire message,
  costing every report. It is dropped with a warning instead, and the rejection
  message names the settings to check.
- `logFile` accepted a path, placing the log outside the `MrFrost` folder. It
  falls back to `reports.log` and says so.
- String overrides from one server file discarded those from another, so
  whichever arrived second won and the other file's wording vanished.
- The info menu stopped offering itself after a mission restart, and never
  appeared at all for a player who joined a second server in the same session.
- The info menu's key opened an empty menu on a server that had switched the
  feature off, although its pause entry was correctly gone.
- Content that failed to parse on arrival was replaced by the bundled version
  without saying so, leaving a server owner to work out why players saw
  something other than what they had written.

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
