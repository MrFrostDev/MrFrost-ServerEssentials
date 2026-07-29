# Reports

[← Back to index](Home.md)

Players report a bug or another player without leaving the game. Every report goes to the server's log file and, if you configure one, to a Discord channel.

Opened with <kbd>F10</kbd> or from the pause menu. Sent by **holding <kbd>Enter</kbd>** — or the right trigger on a controller.

## What a player sees

A form, not a browsable tree — the info menu is a reference work, a report is a task with three answers:

```
┌──────────────────┬──────────────────┐
│  Report a player │  Report a bug    │      ← tabs
└──────────────────┴──────────────────┘

Who are you reporting?      [ The player who last killed me   ▾ ]
Pick a player               [ Schulz                          ▾ ]
──────────────────────────────────────────────────────────────
What happened?
Describe it in your own words. The more precise, the better.
  ┌──────────────────────────────────────────────────────────┐
  └──────────────────────────────────────────────────────────┘
  status line

  [ESC] Back                              [⏎] Send report ▓▓▓░░░
```

The kind is a tab, the way every vanilla menu splits its sections. A kind the server has switched off gets no tab at all, so a server taking only bug reports shows one tab rather than one live and one dead.

Rows that do not apply are hidden, not greyed out — a bug report has no "who", and the player dropdown only appears for "pick a player".

Controls are the game's own widget library — `WLib_TabViewHorizontal`, `WLib_ComboBox`, `WLib_EditBox` — so the form reads like the settings menu and works on a controller for free.

**Sending is a hold, not a press.** A report cannot be recalled once it reaches a moderator, so it does not sit one stray click away from a text field. There is no send button in the form; the only way to send is holding the footer prompt for a second.

The hold is declared on the input action itself, not in the menu, which is why it behaves identically on a keyboard and on a controller: the prompt reads the filter off the action, draws the fill, and fires only when it completes. Nothing in the UI is device-specific.

The prompt stays greyed out until something is written, the same way the group menu greys its create button out when there is nothing to create. A disabled prompt refuses its own action, so it is the guard as well as the hint.

Once a report is in, the menu says so and closes itself a moment later.

**Report a bug** — free text. Where they were standing is attached automatically.

**Report a player** — free text plus one of four ways to say who:

| Option | Who it means |
|---|---|
| Pick a player | One player the reporter chose |
| The player who last killed me | Whoever the **server** recorded as the killer |
| The player who last injured me | Whoever the **server** recorded as the last attacker |
| Everyone within 300 m | Every other player in range, radius configurable |

> [!IMPORTANT]
> The client never says *who* is being reported for the last three options — it says *which option*, and the server answers from what it saw itself. A modified client cannot pin a report on someone who was not there.

## Setting it up

```
<server profile directory>/MrFrost/report.json
```

```json
{
  "enabled": true,
  "allowBugReports": true,
  "allowPlayerReports": true,
  "nearbyRadius": 300,
  "revealNobodyNearby": false,
  "cooldownSeconds": 10,
  "maxDescription": 1000,
  "menuIcon": "feedback",
  "delivery": {
    "writeLog": true,
    "logFile": "reports.log",
    "webhookUrl": "https://discord.com/api/webhooks/...",
    "serverName": "My Server"
  }
}
```

| Key | Meaning |
|---|---|
| `enabled` | `false` hides the menu, its pause entry and its key |
| `allowBugReports` | Offer "Report a bug" |
| `allowPlayerReports` | Offer "Report a player" |
| `nearbyRadius` | Metres for the "everyone nearby" option |
| `revealNobodyNearby` | Tell the reporter when nobody was in range. Off by default — see below |
| `cooldownSeconds` | How long a player waits between two reports |
| `maxDescription` | Longest description accepted, in characters |
| `menuIcon` | Sprite for the menu and pause entry |

### delivery

| Key | Meaning |
|---|---|
| `writeLog` | Append every report to a file |
| `logFile` | File name inside the `MrFrost` folder |
| `webhookUrl` | Discord webhook. Empty means log only |
| `serverName` | Shown in the Discord message, for people running several servers |
| `webhookUsername` | The name the message is signed with |
| `webhookAvatarUrl` | Picture beside that name |
| `colorPlayer` | Stripe down a player report, as `"r,g,b"` |
| `colorBug` | Stripe down a bug report |

> [!WARNING]
> **The webhook URL is a secret.** Anyone holding it can post into that channel. It lives under `delivery`, which is the one part of the file that is read on the server and never sent to a client. Do not put it in the addon config, and do not paste it into a public support thread.

## Where reports end up

### The log file

```
<server profile directory>/MrFrost/reports.log
```

One line per report, appended, so it can be tailed while the server runs:

```
2026-07-29 21:24:07 | PLAYER | by Meier (12) | against Schulz (7) | at 4821 / 9134 | shot me in the back at the arsenal
2026-07-29 21:31:55 | BUG | by Meier (12) | at 4102 / 8890 | the vehicle spawner at Levie eats supplies and gives nothing
```

Times are the server machine’s wall clock, taken on the server, so every report carries one clock no matter where the player is. That is the clock a ban list, a recording and the server’s own log are dated by.

### Discord

An embed, with a coloured stripe down the side — red for a player report, amber for a bug — so a moderator scanning a busy channel sees which is which before reading a word.

```
┌ Player report
│ shot me in the back at the arsenal
│
│ Reported by
│ Meier (12)
│ Against
│ Schulz (7)
│ Position
│ 4821 / 9134
│ Time
│ 2026-07-29 21:24:07
│
│ My Server • Today at 21:24
└
```

Every field sits on a row of its own, so a report naming thirty players in "Against" does not squeeze the column beside it. The description sits in the body, which takes 4000 characters against a field's 1000 — a long report stays readable rather than being cut to fit. Anything past those limits is trimmed with `...` rather than dropped, because Discord rejects an oversized embed whole. The log file never truncates, so the full text is always somewhere.

`serverName` becomes the footer. Leave it empty and no footer is drawn.

`webhookUsername` and `webhookAvatarUrl` decide how the message signs itself. Left empty, Discord uses the webhook's own name and picture — whatever it was called when you created it in the channel settings.

The stripe colours are `colorPlayer` and `colorBug`, written the same way as `accentColor` in `infomenu.json`. The wording of the embed comes from the `report.embed.*` keys and is resolved on the server, so it follows your language rather than the reporter's — see [Server content](ServerContent.md#text-and-languages).

The time appears twice on purpose. The **Time** field is the server’s clock, so a line in Discord and a line in `reports.log` can be matched character for character. The footer next to `serverName` is Discord’s own timestamp, which every reader sees in *their* timezone — a moderator abroad reads their own clock without converting anything.

Sending is queued at roughly one message every one and a half seconds, which stays clear of Discord's rate limit even when several players report at once.

`@everyone` typed into a description cannot ping anyone — mentions are switched off on every message.

> [!NOTE]
> A failed webhook is not a lost report. The log file already has it, and the reason appears in the server log.

## Why "nobody nearby" is not reported by default

"Nobody is within 300 m" is a useful answer to someone who wanted to report a player. It is an equally useful answer to someone who just wanted to know whether anyone is out there — which would turn the report menu into a radar.

With `revealNobodyNearby` off, the report is still filed; it simply names nobody, and the reporter gets the same confirmation as always. Turn it on only if you would rather have the honest error message than the closed hole.

## Rate limiting

One report per player per `cooldownSeconds`. The check is on the server, so it holds regardless of what the client does. A player who tries earlier is told to wait, in their own language.

## Turning it off

```json
{ "enabled": false }
```

Menu, pause entry and key all disappear. The same works for the info menu in `infomenu.json`.

## Log lines

| Message | Where | Meaning |
|---|---|---|
| `Report accepted from ... (PLAYER)` | Server | A report passed every check |
| `Report written to ...` | Server | It reached the log file |
| `Report delivered to Discord` | Server | The webhook accepted it |
| `Discord rejected a report (HTTP ...)` | Server | Wrong URL, or Discord said no. It is still in the log |
| `No Discord webhook configured` | Server | Log-only, as configured |
| `...has neither a log file nor a webhook` | Server | Nothing is set up — reports go nowhere |

---

**Next:** [Text and languages](ServerContent.md#text-and-languages)
