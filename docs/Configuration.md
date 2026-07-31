# Configuration

[← Back to index](Home.md)

The content bundled with the addon:

```
addon/Configs/MrFrost/InfoMenu.conf     the info menu's categories and entries
addon/Configs/MrFrost/Report.conf       the report menu's settings
addon/Configs/MrFrost/Language.conf     every string the addon itself shows
```

Most of this page is about the first one.

Edit it in the Workbench config editor or any text editor. Save as **UTF-8** — anything else mangles accented characters. Changes apply after a game restart.

> [!IMPORTANT]
> This file is the **fallback**. A server that has its own `MrFrost/infomenu.json` overrides it for everyone who joins — see [Server content](ServerContent.md). If you are setting up a server, that is the page you want; this one covers single player, local testing, and the sample content shipped with the mod.

Both formats hold the same thing and the rich text inside them is identical. [Server content](ServerContent.md#converting-an-existing-addon-config) has the field-by-field mapping.

## Structure

Two levels: categories, each holding entries.

```
MrFrost_InfoMenuConfig {
 m_sTitle "My Server"
 m_aCategories {

  MrFrost_InfoMenuCategory "{7FA1C3D2E4B51900}" {
   m_sName "Rules"
   m_sTitle "Server Rules"
   m_sText "Pick a rule on the left."
   m_bExpandedByDefault 1
   m_aEntries {

    MrFrost_InfoMenuEntry "{7FA1C3D2E4B51901}" {
     m_sName "Teamkilling"
     m_sTitle "Teamkilling"
     m_sText "Don't. Repeat offences are a permanent ban."
    }

   }
  }

 }
}
```

> [!WARNING]
> Every category and entry needs its **own GUID** — the 16 hex characters in braces. Copy a block without changing it and one of the two vanishes, with no error in the log.
>
> Any unique value works. Increment the last characters, or generate one in the Workbench.

## Root fields

| Field | Type | Effect |
|---|---|---|
| `m_sTitle` | text | Heading above the category list |
| `m_sPauseMenuEntry` | text | Label of the pause menu entry |
| `m_MenuIconImageset` | imageset | Icon shown next to the title **and** on the pause menu entry |
| `m_sMenuIconName` | text | Sprite name inside that imageset. Empty hides the icon |
| `m_AccentColor` | colour | Accent colour — see [Colours](#colours) |
| `m_bOpenOnJoin` | `0` / `1` | Open the info menu once per mission after joining. Not on respawn; again after a mission restart |
| `m_sDiscordUrl` | text | Discord invite, `https://`. Empty or any other scheme draws no button |
| `m_sDiscordLabel` | text | Label of the Discord button |
| `m_sWebsiteUrl` | text | Your website, `https://`. Empty or any other scheme draws no button |
| `m_sWebsiteLabel` | text | Label of the website button |
| `m_sCustomUrl` | text | A third link of your choosing, `https://`. Empty or any other scheme draws no button |
| `m_sCustomLabel` | text | Label of that button. Required — an unlabelled slot stays hidden |

## Category fields

| Field | Type | Effect |
|---|---|---|
| `m_bEnabled` | `0` / `1` | `0` hides it without deleting |
| `m_sName` | text | Name in the list on the left |
| `m_sTitle` | text | Heading above the text. Empty falls back to `m_sName` |
| `m_sText` | text | Shown when the category itself is selected |
| `m_IconImageset` | imageset | Row icon source |
| `m_sIconName` | text | Sprite name inside that imageset |
| `m_Icon` | `.edds` | Alternative row icon. The imageset wins if both are set |
| `m_bExpandedByDefault` | `0` / `1` | Start expanded |
| `m_aEntries` | list | The entries below it |

## Entry fields

The same as a category, minus `m_bExpandedByDefault` and `m_aEntries`.

An entry with no `m_sText` shows an empty page — useful as a placeholder while you write.

## Colours

Colours here are in **linear space**, not sRGB. `0.5` renders brighter than you would expect from a colour picker.

Rather than converting, take one of these:

| Colour | Value |
|---|---|
| Reforger gold (default) | `0.76052 0.38643 0.07819 1` |
| White | `1 1 1 1` |
| Strong orange | `0.85 0.29 0 1` |
| Red | `0.71569 0.0512 0.0512 1` |

```
m_AccentColor 0.76052 0.38643 0.07819 1
```

The accent colour drives the selected row, the expand markers and the line under the header.

> [!TIP]
> Colours **inside text** are unrelated to this and use ordinary 0–255 values. See [Text formatting](Formatting.md).

## Worked example

A category with two entries, an icon, and coloured text:

```
MrFrost_InfoMenuCategory "{7FA1C3D2E4B51910}" {
 m_bEnabled 1
 m_sName "Getting Started"
 m_IconImageset "{3262679C50EF4F01}UI/Textures/Icons/icons_wrapperUI.imageset"
 m_sIconName "compass"
 m_sTitle "Getting Started"
 m_sText "The things new players ask on their first evening."
 m_bExpandedByDefault 0
 m_aEntries {
  MrFrost_InfoMenuEntry "{7FA1C3D2E4B51911}" {
   m_bEnabled 1
   m_sName "Joining a squad"
   m_sTitle "Joining a squad"
   m_sText "<color rgba='226,167,79,255'>In three steps</color><br/><br/>  •  Open the group menu<br/>  •  Pick a squad with a free slot<br/>  •  Set your radio to its frequency"
  }
  MrFrost_InfoMenuEntry "{7FA1C3D2E4B51912}" {
   m_bEnabled 1
   m_sName "Vehicles"
   m_sTitle "Vehicles"
   m_sText "Ask before you take one that is not yours."
  }
 }
}
```

## Report.conf

The report menu's fallback settings. Same idea: a server's `MrFrost/report.json` overrides it.

| Field | Type | Effect |
|---|---|---|
| `m_bEnabled` | `0` / `1` | `0` hides the menu and its pause entry. The key stays bound and does nothing |
| `m_bAllowBugReports` | `0` / `1` | Offer the "report a bug" tab |
| `m_bAllowPlayerReports` | `0` / `1` | Offer the "report a player" tab |
| `m_fNearbyRadius` | metres | Range for the "everyone nearby" option |
| `m_iCooldownSeconds` | seconds | Wait between two accepted reports from one player. A half-second floor applies to every request on top of this |
| `m_bRevealNobodyNearby` | `0` / `1` | Whether to tell a reporter nobody was in range. Off by default |
| `m_iMaxDescription` | bytes | Longest description accepted, up to 8191. An umlaut counts two, a Japanese character three |
| `m_MenuIconImageset` | imageset | Icon for the menu and its pause entry |
| `m_sMenuIconName` | text | Sprite name inside it |

> [!IMPORTANT]
> There is deliberately **no webhook field here.** Delivery settings exist only in the server's `report.json`, because the webhook URL is a secret and anything in the addon ships to every subscriber. See [Reports](Reports.md).

## Language.conf

Every string the addon shows, in English plus translations. Keys are what a server's `strings` block overrides — see [Server content](ServerContent.md#text-and-languages).

```
MrFrost_TextEntry "{7FA1C3D2E4B52001}" {
 m_sKey "report.submit"
 m_sText "Send report"
 m_aTranslations {
  MrFrost_Translation "{7FA1C3D2E4B52002}" {
   m_sLanguage "de"
   m_sText "Meldung senden"
  }
 }
}
```

`m_sLanguage` takes either a full code (`de_de`) or a bare language (`de`), and the full code wins. A key with no translation for the player's language falls back to `m_sText`.

---

**Next:** [Text formatting](Formatting.md)
