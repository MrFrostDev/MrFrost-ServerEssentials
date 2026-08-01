# Troubleshooting

[← Back to index](Home.md)

> [!IMPORTANT]
> Several lines below are diagnostics and are **off by default**. Set
> `"verboseLogging": true` in your `report.json` and restart the server before
> looking for them — without it the log carries only what a server owner needs
> to see day to day.


## Reading the log

Every message the addon prints carries the prefix `[MrFrost]`:

```
%USERPROFILE%\Documents\My Games\ArmaReforger\logs\<newest folder>\script.log
```

Search for `MrFrost` and you have its whole story for that session. On a server, the same lines go to the server console.

## Messages

### Input

| Message | Meaning |
|---|---|
| `Action '...' is bound to: N key(s)` | The input system knows the action — the key should work |
| `Action '...' is NOT known to the input system` | The action was never registered. The key does nothing |
| `Input context '...' is active` | The context carrying the actions is up — keys can fire |
| `Input context '...' is NOT active` | The context never came up. A bound key still does nothing |

> [!NOTE]
> Both lines have to be positive. An action can be bound, rebindable, and still never fire because nothing raised the context that carries it.

### Server content

| Message | Where | Meaning |
|---|---|---|
| `Loaded $profile:MrFrost/<file> (N bytes)` | Server | The server's own file was read |
| `No $profile:MrFrost/<file> on this server` | Server | No server file — the bundled content is used |
| `... is not sound JSON` | Server | A stray or missing comma, an unclosed brace or quote |
| `Could not open ...` | Server | Permissions, or the file is locked |
| `Server content complete` | Client | Every feature's content arrived. Needs `verboseLogging` |
| `Using this server's info menu (N categories)` | Client | The server's content is now what players see |
| `Using this server's report settings` | Client | Same, for the report menu |
| `The info menu is switched off on this server` | Client | `"enabled": false` in `infomenu.json` |
| `This server overrides N UI string(s)` | Both | A `strings` block was applied. The server prints it when it reads the file, each client when it receives it |

### Reports

| Message | Where | Meaning |
|---|---|---|
| `Report accepted from ... (BUG\|PLAYER)` | Server | A report passed every check and reached a log file or Discord |
| `Report written to ...` | Server | It reached the log file |
| `Report delivered to Discord` | Server | The webhook accepted it |
| `Discord rejected a report (HTTP ...)` | Server | Wrong URL, or Discord said no. It is still in the log |
| `The Discord webhook timed out` | Server | No reply in time. It is still in the log |
| `webhookUrl in report.json is not an https:// address` | Server | Missing or wrong scheme. The webhook is ignored entirely |
| `<key> in infomenu.json is not an https:// address` | Server | That footer button will not be drawn |
| `infomenu.json asks for N rows` | Server | More than 512 categories and entries together. The rest is left out |
| `A page in infomenu.json is longer than 20000 characters` | Server | That page is left empty. Split it into entries |
| `accentColor '...' is not 'r,g,b'` | Both | Colour must be three numbers, e.g. `226,167,79`. Players see the default accent |
| `Could not repack report.json without its delivery block` | Server | The safety check refused to send. Reports still work; players see the bundled menu settings |
| `webhookAvatarUrl is not a URL and was ignored` | Server | Must be an `http` address, or left empty |
| `The Discord queue is full` | Server | More reports arriving than Discord accepts. The newest are dropped |
| `logFile must be a file name, not a path` | Server | `logFile` may not contain `/`, `\`, `:` or `..` |
| `Could not write to ...` | Server | The `MrFrost` folder is not writable |
| `Could not get a REST context for the webhook.` | Server | The engine would not hand out an HTTP client for that host. Nothing was sent, so this is not a network failure — an unreachable host shows up as a timeout instead |
| `The embed labels on this server leave no room for a description` | Server | `report.embed.*` overrides are too long |
| `colorPlayer '...' is not 'r,g,b'` | Server | Colour must be three numbers, e.g. `249,67,67` |
| `... is empty - falling back to the bundled content` | Server | The file exists but has nothing in it |
| `report.json still carried its delivery block after repacking` | Server | The safety check that keeps the webhook URL off the wire refused to send. Reports still work; players see the bundled menu settings |
| `No Discord webhook configured` | Server | Log-only, as configured |
| `A report reached neither the log file nor Discord.` | Server | The report was accepted and then went nowhere. Nothing configured, or the log file could not be written, or the Discord queue was full |

### Menus

| Message | Meaning |
|---|---|
| `Info menu opened with N row(s)` | The menu is running and built N rows |
| `Could not load the info menu config from ...` | Bundled config missing or malformed |
| `Frame layout has no '...'` | The shared menu frame is damaged or was overridden |
| `Text table loaded for language 'xx_xx' (N strings)` | Localisation is up |
| `No text for key '...'` | A string is missing from `Language.conf`; the key itself is shown |
| `Opening the info menu for the freshly joined player` | Auto-open fired |
| `Gave up on the welcome info menu` | Another menu stayed open, or the transfer never finished |
| `Pause menu entry '...' added` | A pause menu entry was inserted |
| `Pause menu has no 'FieldManual' button` | Different pause menu layout; the entries were skipped |

## Symptoms

### The menu does not open at all, and the log is empty

The addon was not loaded, or a file was added without a Workbench rescan. See [Installation](Installation.md#when-you-need-the-workbench).

### A key does nothing

Check the log lines above first: an action that is not known, or a context that is not active, cannot work.

If both are positive and the key still does nothing, the key itself is claimed by something else. Try rebinding under *Settings → Controls → MrFrost*.

> [!TIP]
> **F12 is unusable.** Steam captures it for screenshots and the game never sees it.

### Holding the send prompt does nothing

The prompt is greyed out until something is written in the description — that is deliberate, and it also blocks the key.

If there is text and it still does nothing, the mouse still works: clicking the prompt sends immediately. Report it with the `[MrFrost]` lines attached.

### The footer prompt shows a crossed-out circle

That is the game's own "unbound" glyph. The action behind the prompt did not register.

### Accented characters are broken

The file was not saved as UTF-8. Re-save it with the right encoding.

### Entries are missing from the list

Either `enabled` is `false`, or — in the addon config only — two blocks share a GUID. Duplicate GUIDs are silent: nothing appears in the log, one entry simply never shows up.

Search the config for the GUID and confirm it appears exactly once.

### Text is cut off at the bottom

Long entries scroll. If a text ends abruptly and will not scroll, try the mouse wheel with the cursor over the text area.

### Icons do not show

No error is produced; the row just has no icon. Check the imageset path including its GUID, the sprite name and its capitalisation. See [Icons](Icons.md#if-an-icon-does-not-show).

### The info menu does not open on join

It waits while another menu is up — the deploy screen counts — and while the server's content is still arriving. It retries for about two and a half minutes and then gives up, which shows as `Gave up on the welcome info menu`.

It also asks your `infomenu.json` again once that has arrived. A server whose categories are all switched off, or which set `openOnJoin` to false, gets no welcome menu and logs `This server does not want a welcome info menu` instead.

It also opens **once per mission** by design, so it will not reappear after a respawn — but it does after a mission restart.

### Changes do nothing

Content is read at startup. Restart the game — or the server, if you edited a file under `MrFrost/`.

### Players see the bundled sample content, not mine

The server's file never made it. The **server** console says which of the three it was:

| Line | Cause |
|---|---|
| `No $profile:MrFrost/... on this server` | Wrong folder, or wrong file name |
| `Could not open ...` | Permissions, or the file is locked |
| `report.json is not sound JSON` | A stray or missing comma, an unclosed brace or quote. The line names the file |
| `infomenu.json is not sound JSON` | Same, for that file |
| `This server sent no info menu categories` | Client. The file carried settings but no content, so the bundled categories are kept |
| `This server's ... content did not parse` | Client. What arrived was not usable; the bundled content is used |

The folder belongs in the directory passed to the server with `-profile`, not anywhere inside the addon.

If the server logged the file as loaded but the **client**, with `verboseLogging` on, never logs `Server content complete`, the transfer did not complete. That is worth reporting with both logs attached.

## Still stuck

Collect the `[MrFrost]` lines from `script.log` plus the section of your file around the affected entry. Those two together explain nearly every case.

---

**Next:** [Architecture](Architecture.md)
