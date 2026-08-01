# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

- Faction rules and a queue for a full faction, on a branch and not yet released.
  Player limits, whitelists and a balance rule as one system, with a queue that
  catches everyone they turn away. Design notes live on that branch.

## [1.1.0] - 2026-08-01

### Fixed

- The report menu could not be used with a controller. Nothing held focus when
  it opened, so no button responded until a mouse moved the cursor onto one, and
  the info menu's page text could not be scrolled at all. Both menus now take
  focus on open and answer the stick.
- One capital letter in a server file could replace the whole menu with the
  addon's demo content, silently. `"enabled": True`, a misspelt `fasle`, a
  number written `.5`, and a Windows path in a text value all passed the file
  check and then failed the parse — which cannot report failure, so every
  setting fell back to its default while the server console said the file had
  loaded. Those files are now rejected, by name, on the server console.
- A file saved from Notepad or `Out-File` was rejected outright: both write a
  byte order mark, and the message sent the owner looking for a missing comma in
  a file whose punctuation was perfect. The mark is dropped.
- `cooldownSeconds` is held between 0 and 3600. Above 2,147,483 it used to wrap
  to a negative number and remove the cooldown entirely — the setting that asks
  for the strictest throttle gave the weakest. **If your `report.json` sets it
  above 3600, it is now 3600.** The server says so on its console when it caps.
- A description at the documented maximum of 8191 could be cut through the
  middle of a character, which Discord rejects, losing that report.
- A repeated content packet after a channel had finished put that channel back
  in the pending set, where nothing would ever complete it. The welcome menu
  then never opened again for the rest of the session.
- The keys that open the two menus fired everywhere, including into an open
  menu and into the description box. Rebind Report to a letter and the first
  time you typed it the form closed and took the report with it; F11 over an
  open report menu buried it under the info menu. The keys now stay out of a
  menu they did not open, and switch between the two rather than stacking them.
- The welcome menu could open a second time, over gameplay, up to two and a half
  minutes after a player had already read it and closed it themselves.
- A menu whose content failed to load had no way out — the back prompt is built
  with the footer, and the footer came second.
- `logFile` set to a reserved Windows device name (`NUL`, `CON`, `COM1`, …) was
  accepted, and every report written to it was discarded while the console
  reported each one as written.
- Warnings about settings that cannot change while the server runs were printed
  once per report rather than once.
- **A server that shipped an `infomenu.json` without `categories` gave its
  players a Discord button pointing at the addon author's server**, and no value
  it could write would stop it — an absent key and `""` arrive identical, so the
  empty string the documentation offers as "draws no button" was the same input
  as saying nothing. The footer links are no longer inherited: a server that
  ships a file owns its own footer. **If you rely on that inheritance, write
  `discordUrl`, `websiteUrl` and `customUrl` into your file.**
- The same merge discarded a `discordLabel`, `websiteLabel`, `customLabel` or
  `menuIconImageset` written without its paired key.
- The menu opened from the pause screen before the server's content had arrived
  showed the addon's placeholder pages — and counted as the welcome, so the
  server's real one never opened. It no longer counts until the content is in,
  and a mission restart offers it again.
- A `strings` override with no `text` blanked the widget it named instead of
  being ignored. **If you used `{"key": "…", "text": ""}` deliberately to hide a
  label, that entry is now refused** and the built-in wording comes back.
- The title and the pause-menu entry read "Server Info" in English in all thirteen
  languages on a default install, because the shipped value won against the
  translated fallback. Both ship empty now and read "Info" in the player's own
  language; a server that sets either still gets exactly what it wrote.
- The Discord sender stalled under a steady stream of reports: every new report
  pushed the next send further out, so one report went and the rest queued.

### Security

- With `verboseLogging` on, a modified client could write to the server log
  without bound: two paths logged a rejection per packet with no limit and, in
  one case, whatever text the client chose to put in it.
- Control characters a player typed reached `reports.log` unfiltered, so a
  reporter could erase the lines above their own in a terminal. Discord's copy
  has always stripped them, so the two records of one report disagreed.

### Changed

- A mistyped `accentColor` is now reported on the server console rather than in
  every player's log, where the owner who wrote the file could not see it.

## [1.0.7] - 2026-08-01

### Changed

- `report.nobody_nearby` now means the report was filed and nobody was named.
  It used to mean the report was refused. **If you replaced that string through
  a `strings` block, re-read it before upgrading** — your wording will still win
  over the built-in one, and it now describes the opposite of what happened.

### Fixed

- With `revealNobodyNearby` on, a nearby report with nobody in range was thrown
  away rather than filed. The player was told something true about their
  surroundings and nothing about their report, and left able to send it again
  and lose it again. The report goes either way now.
- Fourteen fields and a method on the modded player controller carried no
  prefix. A second mod picking one of those names collides in the same chain,
  and for the method that is a compile error — which takes down every mod on
  the server.
- `nearbyRadius` at zero or below now falls back to 300, the way
  `maxDescription` falls back to 1000. No shipped configuration could reach
  that state; the guard closes the path rather than a symptom.
- Several translations said the wrong thing: German called a bug report an error
  message, Portuguese and Chinese used the verb for denouncing a person on the
  bug tab specifically, and Spanish said players *at* the radius rather than
  within it. Four more drifted from the word their own language uses everywhere
  else, and the German player-picker label was half again as long as any
  sibling in a slot that cannot grow.
- The website button was labelled in English for every player. The example
  `infomenu.json` no longer fills that label in, so copying it as documented
  leaves the translation in place.

## [1.0.6] - 2026-08-01

### Fixed

- A server whose `report.json` could not be repacked without its webhook block
  said so once, in the middle of the first player's join, and every client after
  that quietly used the bundled settings. The client-safe form is now built when
  the server starts, where an owner is watching.
- `"categories": null`, `"strings": null` or `"entries": null` in `infomenu.json`
  passed every check and then broke — on the server it cost the rest of the startup read, on clients
  it cost the content of a channel already marked as delivered.
- A server file too large to send was transmitted in full to every joining player
  and discarded in total on arrival, with nothing said on either machine. The
  limit is now checked before sending, and named.
- A report against a player picked from the list could name whoever inherited
  that player's id if they left while the report was being written. The pick now
  carries the name the reporter was looking at, and the server refuses it when
  the two no longer agree.
- Content arriving over the network was installed without being checked, so a
  payload a client could not read came back as the addon's defaults while the
  console said the server's settings were in use. Both features check it now,
  the same way the server checks its own files.
- A report that reached neither the log file nor Discord was reported to the
  owner as "neither is configured", contradicting the line above it that had
  just named the real reason — a full disk, or a queue that was full.

## [1.0.5] - 2026-07-31

### Fixed

- Each footer link listened for its key twice, so one press opened the browser
  twice - and the second listener ran outside every guard the prompt applies,
  so with the report menu open on top of the info menu, typing a `d` into a
  description opened Discord.
- Nothing bounded how many rows a server could have a client build. Each row is
  fifteen widgets, created up front, in a menu that opens by itself on join. A
  page of text far past any rule set is now left out rather than cut, because a
  cut cannot leave markup whole.
- The accent colour never reached the line under the header, which kept the
  built-in gold whatever a server set.
- A menu whose categories were all switched off still had a pause entry and a
  key, and opened empty — unless it carries footer links, which live nowhere
  else and would otherwise become unreachable.
- A page with nothing on it said "this info menu has no entries yet", beside a
  full tree of categories. It has its own wording now.
- Hosting your own game after playing on a server kept that server's menu,
  wording and log setting.
- Whether to show the welcome menu was decided before the server's own content
  arrived, so a server that had switched it off could still be overridden.
- A second connection in the same session was not recognised as a fresh start,
  and the player was never welcomed.
- String overrides written into `infomenu.json` were honoured by every player
  and ignored in the server's own Discord embed.
- "No other players" was written into the dropdown but never selected, so the
  closed box showed nothing at all.
- The welcome menu gave up after about a minute of waiting, which the server's
  own content can outlast when several players join at once. It waits about
  two and a half minutes now.
- The row icon column was two pixels narrower than the icon it holds.
- The player dropdown was filled without being emptied first, so anything the
  layout shipped with would have shifted every index behind it.
- The transfer confirmation could never appear in a client log, because verbose
  logging is a setting that arrives with the transfer it was meant to confirm.
  The server now says when it has sent everything, which is the only side that
  knows.
- `maxDescription` of zero disabled the length cut on the addon config path,
  which the JSON path already guarded against.

## [1.0.4] - 2026-07-31

### Added

- `report.failed`, told to a player whose report reached neither the log file
  nor Discord. Until now that was reported as sent.

### Changed

- A footer link is only offered if it is an `https://` address. The URL comes
  from the server and is never shown, so a button labelled Discord could lead
  anywhere. A link that does not qualify draws no button, and the server names
  the key on its own console when it reads the file.
- A `webhookUrl` that is not an `https://` address is dropped when the file is
  read rather than when the first report is already on its way.
- The Discord queue is bounded. Past the cap the newest report is dropped, not
  the oldest, because the first reports of an incident are the ones a moderator
  wants.

### Fixed

- Damage was watched on everything the world spawned, not only on players.
  That allocated per-entity data vanilla deliberately avoids, and with it
  present every bullet, fragment and damage tick on the server ran a script
  dispatch that could only ever be discarded. On a Conflict server with a few
  hundred AI alive, hundreds of them a second.
- The report kind was taken from the client and only two values were tested
  for. A third slipped past both permission checks and arrived as a bug report
  on a server that had switched bug reports off.
- Only accepted reports were rate limited. Every rejected one - no description,
  no target, nobody nearby, feature switched off - was free, so a modified
  client could hold the server in a loop of player sweeps at packet rate.
- A client could ask for the server's files repeatedly. The guard only stopped
  overlapping transfers, so waiting for one to finish and asking again rebuilt
  everything, without limit.
- A report could be lost while the player was told it had been sent.
- Server files were sent with their indentation, which on a pretty-printed
  config is easily a tenth of every byte sent to every player who joins.
- The packet count for a transfer depended on how the server owner had
  formatted their JSON, not on how large it was.
- The client-safe form of each file was rebuilt for every joining player,
  though it is the same for all of them.
- Transfer pacing was per player, so sixty-four clients joining after a restart
  ran sixty-four independent senders at once - the burst the pacing exists to
  prevent.
- A control character in a description made Discord reject the whole message,
  with an error naming the webhook settings rather than the cause.
- Embed titles, footers and the webhook username went to Discord unbounded.
- `maxDescription` set to zero disabled the length cut entirely instead of
  meaning what it says.
- Reports left the Discord queue out of order when several arrived close
  together.
- A second answer could overwrite the first, telling a player their filed
  report had bounced.
- The info menu built every row with two colour animations that ramped from
  transparent to transparent, each costing a scan of a global animation list.

### Security

- A malicious client could make the server allocate an array sized by a number
  it chose, and a malicious server could do the same to every client that
  joined it. Both are bounded now.

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
- The check that keeps the `delivery` block out of a transfer could be walked
  past by a line break between a key and its colon.
- Every warning about a malformed `report.json` value appeared twice, because
  the delivery settings were built from the file twice.
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

[Unreleased]: https://github.com/MrFrostDev/MrFrost-ServerEssentials/compare/v1.1.0...HEAD
[1.1.0]: https://github.com/MrFrostDev/MrFrost-ServerEssentials/compare/v1.0.7...v1.1.0
[1.0.7]: https://github.com/MrFrostDev/MrFrost-ServerEssentials/compare/v1.0.6...v1.0.7
[1.0.6]: https://github.com/MrFrostDev/MrFrost-ServerEssentials/compare/v1.0.5...v1.0.6
[1.0.5]: https://github.com/MrFrostDev/MrFrost-ServerEssentials/compare/v1.0.4...v1.0.5
[1.0.4]: https://github.com/MrFrostDev/MrFrost-ServerEssentials/compare/v1.0.3...v1.0.4
[1.0.3]: https://github.com/MrFrostDev/MrFrost-ServerEssentials/compare/v1.0.2...v1.0.3
[1.0.2]: https://github.com/MrFrostDev/MrFrost-ServerEssentials/compare/v1.0.1...v1.0.2
[1.0.1]: https://github.com/MrFrostDev/MrFrost-ServerEssentials/compare/v1.0.0...v1.0.1
[1.0.0]: https://github.com/MrFrostDev/MrFrost-ServerEssentials/releases/tag/v1.0.0
