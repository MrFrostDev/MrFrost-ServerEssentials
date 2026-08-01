# Server content

[← Back to index](Home.md)

One published mod, different content on every server that runs it. Players subscribe once.

## Where the files go

```
<server profile directory>/MrFrost/
    infomenu.json
    report.json
    reports.log      (written by the addon)
```

The profile directory is the one you pass the server with `-profile`. It is the same folder your server config and logs live in — nothing to do with the addon.

That is the entire setup. There is no key to enable, and no addon rebuild.

> [!NOTE]
> The folder is named after the author, not the addon, so renaming the mod never invalidates your setup. Every feature gets its own file in it — one broken file cannot take the others down.

## What happens on join

1. The client asks the server for its content.
2. The server reads each file once, when it starts, and keeps it.
3. Each file is sent to that client in packets and reassembled.
4. Every feature renders the server's version instead of the one bundled with the addon.

If the server has no file for a feature, has an unreadable one, or sends something that does not parse, that feature keeps its bundled content. A typo in one file never leaves a menu empty.

> [!TIP]
> Hosting your own game locally skips the transfer entirely — the files are already on the machine, so they are read directly. That makes single player the fastest way to test a JSON before putting it on a server.

## Getting started

A ready-made folder is in the repository:

```
addon/server/MrFrost/
```

Copy it into your profile directory and replace the text. The files carry the keys a server usually sets, with real content behind each one rather than placeholders. Optional keys documented here and absent from them keep the defaults listed above.

JSON has no comments, so the files stay clean and everything is documented here instead. A key you leave out keeps its default — you never have to write one you do not care about.

## infomenu.json

### Top level

| Key | Type | Default | Meaning |
|---|---|---|---|
| `enabled` | bool | `true` | `false` hides the menu **and** its pause entry. The key stays bound and does nothing. The same happens when every category is switched off — unless a footer link is still drawable, since those live nowhere else |
| `title` | string | `Info` | Headline above the category list. Usually the server or unit name |
| `pauseMenuEntry` | string | `Info` | Label of the entry added next to the vanilla Field Manual. Left out, it reads "Info" in the player's own language |
| `openOnJoin` | bool | `true` | Show the menu once, the first time a player spawns in. Not on respawn; again after a mission restart |
| `accentColor` | string | white | `"226,167,79"` — ordinary sRGB, the numbers a colour picker shows. Fills the selected row and draws the line under the header |
| `menuIcon` | string | — | Sprite next to the title and on the pause menu entry — see [Icons](Icons.md) |
| `menuIconImageset` | string | the shared set | Only if you ship your own artwork |
| `discordUrl` | string | — | Invite link for the footer button. Must be `https://`; empty or anything else draws no button |
| `discordLabel` | string | `Discord` | Label of that button |
| `websiteUrl` | string | — | Your website, opened by the same mechanism. Must be `https://` |
| `websiteLabel` | string | — | Label of that button. Left out, it reads "Website" in the player's own language |
| `customUrl` | string | — | A third link of your choosing — a ruleset, a ban appeal form, a forum thread. Must be `https://` |
| `customLabel` | string | — | Label of that button. **Required** — an unlabelled third slot stays hidden even with a URL set |
| `strings` | array | — | Override individual UI strings — see [Text and languages](#text-and-languages) |
| `categories` | array | — | The list on the left, up to 512 rows counting categories and entries together. Leave it out to keep the bundled pages and change only the settings around them. The footer links are **not** inherited that way — a file that names no `discordUrl` gets no Discord button |

### A category

| Key | Type | Default | Meaning |
|---|---|---|---|
| `name` | string | — | What the row on the left says. Keep it short; the column is 380 px wide |
| `title` | string | `name` | Headline above the text on the right |
| `text` | string | — | The body, up to 20000 characters. Rich text — see [Text formatting](Formatting.md) |
| `icon` | string | — | Sprite in front of the row. Rows without one close the gap, so a mixed list still lines up |
| `iconImageset` | string | the shared set | Only if you ship your own artwork |
| `expanded` | bool | `false` | Start the category open |
| `enabled` | bool | `true` | `false` hides it without deleting it — useful for seasonal rules |
| `entries` | array | — | The rows below it |

### An entry

The same keys as a category, minus `expanded` and `entries`.

Two levels is the whole hierarchy. If a category feels like it needs a third, it usually wants splitting into two categories instead — a reader scanning short titles finds things faster than one scrolling a long page.

### Switching the menu off

```json
{ "enabled": false }
```

Menu, pause entry and key all disappear. `report.json` takes the same key.

### Skeleton

```json
{
  "title": "My Server",
  "discordUrl": "https://discord.gg/example",
  "discordLabel": "Discord",
  "websiteUrl": "https://example.com",
  "customUrl": "https://example.com/appeal",
  "customLabel": "Ban appeal",
  "accentColor": "226,167,79",
  "menuIcon": "field-manual",
  "openOnJoin": true,
  "categories": [
    {
      "name": "Rules",
      "title": "Server Rules",
      "text": "Pick a rule on the left.",
      "icon": "warning",
      "expanded": true,
      "entries": [
        {
          "name": "Teamkilling",
          "title": "Teamkilling",
          "text": "Don't.",
          "icon": "cancelCircle"
        }
      ]
    }
  ]
}
```

## report.json

### Top level

| Key | Type | Default | Meaning |
|---|---|---|---|
| `enabled` | bool | `true` | `false` hides the report menu and its pause entry. The key stays bound and does nothing |
| `allowBugReports` | bool | `true` | The "report a bug" tab. Free text plus the reporter's position |
| `allowPlayerReports` | bool | `true` | The "report a player" tab, with the four ways of naming a target |
| `nearbyRadius` | number | `300` | Metres for the "everyone within X" option. The menu label is written with a placeholder and follows this number. Zero or less falls back to 300 |
| `revealNobodyNearby` | bool | `false` | Whether to tell a reporter nobody was in range — see below |
| `cooldownSeconds` | number | `10` | Wait between two accepted reports from one player, `0` to `3600`. Enforced on the server; a value outside that range is capped and said on the console. A half-second floor applies to every request on top of this, and cannot be switched off |
| `maxDescription` | number | `1000` | Longest description accepted. Longer text is trimmed, not rejected. Zero or less falls back to 1000 |
| `menuIcon` | string | `feedback` | Sprite for the menu title and its pause entry |
| `menuIconImageset` | string | the shared set | Only if you ship your own artwork |
| `verboseLogging` | bool | `false` | Writes the addon's diagnostic lines to the server log. On only while you are chasing something |
| `delivery` | object | — | Where reports go. **Server-only** — see below |
| `strings` | array | — | Override individual UI strings |

A tab the server switches off is not drawn at all, so a server taking only bug reports shows one tab rather than one live and one dead.

### delivery

| Key | Type | Default | Meaning |
|---|---|---|---|
| `writeLog` | bool | `true` | Append every report to a file in the same `MrFrost` folder |
| `logFile` | string | `reports.log` | One line per report, appended, so it can be tailed while the server runs |
| `webhookUrl` | string | — | Discord: channel settings → Integrations → Webhooks. Must be `https://`; anything else is ignored and named in the server log |
| `serverName` | string | — | Shown in the Discord message. Useful when several servers post to one channel |
| `webhookUsername` | string | — | The name the message is signed with. Empty leaves Discord's own, which is whatever the webhook was called when it was created |
| `webhookAvatarUrl` | string | — | Picture beside that name. Any public image URL |
| `colorPlayer` | string | `249,67,67` | Stripe down a player report. Ordinary sRGB, the same notation as `accentColor` |
| `colorBug` | string | `226,167,79` | Stripe down a bug report |

> [!WARNING]
> **The webhook URL is a secret.** Anyone holding it can post into that channel. `delivery` is the one block that is read on the server and never sent to a client, which is why it belongs here and nowhere else. Never put it in the addon config — that ships to every subscriber — and never paste it into a public thread. If it leaks, regenerate it in the Discord channel settings and the old one stops working.

### Why `revealNobodyNearby` defaults to off

"Nobody is within 300 m" is a useful answer to someone who wanted to report a player. It is an equally useful answer to someone who just wanted to know whether anyone is out there — which would turn the report menu into a radar.

The report is filed either way — it simply names nobody. The setting only decides whether the confirmation mentions that: off, the reporter is thanked as always; on, they are told the report went and that nobody was in range. Leave it off if you would rather not answer the question "is anyone out there".

Full walkthrough of the menu itself: [Reports](Reports.md).

## Text and languages

Every string the addon itself shows — button labels, hints, confirmations — is translated by the game's language. A player on a German client sees German without anyone configuring anything.

Your own content is never touched: categories, titles and rule texts are yours, in whatever language you wrote them.

If you want the addon's own wording to match your server's language regardless of the client, override individual strings in any of the JSON files:

```json
"strings": [
  { "key": "report.submit", "text": "Meldung abschicken" },
  { "key": "report.sent",   "text": "Danke, ist raus." }
]
```

An override wins over both the client's language and the addon's English.

Every key lives in `addon/Configs/MrFrost/Language.conf`. The ones a server owner usually changes:

| Key | English |
|---|---|
| `report.title` | Report |
| `report.pause_entry` | Report |
| `report.submit` | Send report |
| `report.sent` | Report sent. Thank you. |
| `report.description_hint` | Describe it in your own words. The more precise, the better. |
| `report.type_bug` | Report a bug |
| `report.type_player` | Report a player |
| `report.embed.player` | Player report |
| `report.embed.bug` | Bug report |
| `report.embed.reporter` | Reported by |
| `report.embed.against` | Against |
| `report.embed.position` | Position |
| `report.embed.time` | Time |

The six `report.embed.*` keys are the wording of the Discord message itself. They are resolved on the server rather than on a player's machine, so they follow the server's language and its overrides — a German unit gets a German embed while its players still read the menus in whatever language they play in.

> [!TIP]
> Overrides are shared across features. Whichever file carries a key, that wording is what every menu uses — so it does not matter which JSON you put them in.

> [!WARNING]
> **An override replaces the translation, it does not add one.** A key you override is that exact text for every player, whatever language their game is in — a German player on a server that overrode `report.submit` with English sees English.
>
> That is the point when your server has one language and you want your own tone. It is a trap when you only meant to see what the block looks like: override five keys "as an example" and you have switched localisation off for those five strings in all thirteen languages. This is why the shipped `report.json` has no `strings` block at all — leave it out and every string follows the player.
>
> Want your wording *and* other languages? Give the key its own entry per language in the addon's `Language.conf` instead of overriding it here.

### Which languages are covered

English is the base, and every string is translated into all twelve other languages Arma Reforger ships:

`cs_cz` · `de_de` · `es_es` · `fr_fr` · `it_it` · `ja_jp` · `ko_kr` · `pl_pl` · `pt_br` · `ru_ru` · `uk_ua` · `zh_cn`

The addon asks the game which language it is running in and picks the matching text. A language with no translation for a key falls back to English, so nothing is ever blank.

With `verboseLogging` on, the log says which one it settled on:

```
[MrFrost] Text table loaded for language 'de_de' (31 strings).
```

## Rules of the files

**UTF-8, always.** Umlauts and guillemets are common in rule texts and anything else mangles them.

**No unique GUIDs.** That requirement belongs to the addon config format. JSON entries are identified by position, so copy and paste freely.

**A restart applies changes.** Files are read once, like every other server setting.

**Escape double quotes** as `\"`, or use `«` and `»` — they read better in prose anyway.

## Checking it worked

The server console at startup:

```
[MrFrost] Loaded $profile:MrFrost/infomenu.json (21402 bytes).
```

The client, once it has everything (the first line needs `verboseLogging`):

```
[MrFrost] Server content complete.
[MrFrost] Using this server's info menu (7 categories).
[MrFrost] Using this server's report settings.
```

Neither line means the file was never found, was empty, or is not sound JSON — the message right there says which. See [Troubleshooting](Troubleshooting.md).

## Converting an existing addon config

If you already keep your content in a `.conf` and want it as JSON, the mapping is mechanical:

| `.conf` | JSON |
|---|---|
| `m_sName` | `name` |
| `m_sTitle` | `title` |
| `m_sText` | `text` |
| `m_sIconName` | `icon` |
| `m_bEnabled 0` | `"enabled": false` |
| `m_bExpandedByDefault 1` | `"expanded": true` |
| `m_AccentColor` (linear) | `accentColor` (sRGB) |

The rich text inside `text` is identical — nothing in it changes.

---

**Next:** [Configuration](Configuration.md)
