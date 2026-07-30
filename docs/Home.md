# MrFrost Server Essentials — Documentation

Player-facing tooling for Arma Reforger servers — an info menu and a report system — driven by data rather than script.

Running a server? Read [Installation](Installation.md) and then [Server content](ServerContent.md) — that pair is the whole setup. Everything else is reference material you can come back to.

## Pages

### Setting up

- **[Installation](Installation.md)** — server setup, local testing, and the one situation where you need the Workbench
- **[Server content](ServerContent.md)** — the JSON files on your server, their keys, and how they reach players
- **[Reports](Reports.md)** — the report menu, Discord webhook and log file
- **[Faction queue](FactionQueue.md)** — limits, balance and the queue for a full faction *(design notes, not built yet)*
- **[Configuration](Configuration.md)** — the complete field reference for the addon config

### Writing content

- **[Text formatting](Formatting.md)** — bold, colours, inline images, and how to structure a readable entry
- **[Icons](Icons.md)** — sprite names that are known to work, plus using your own artwork

### When something is wrong

- **[Troubleshooting](Troubleshooting.md)** — log messages, common symptoms, and what actually causes them

### Under the hood

- **[Architecture](Architecture.md)** — how the addon is put together, what is shared between features, and how to add one

## The short version

Content comes from one of two places, and the first one that has something wins:

| Source | Where | Who uses it |
|---|---|---|
| **The server** | `<server profile>/MrFrost/*.json` | Anyone running a server |
| **The addon** | `addon/Configs/MrFrost/*.conf` | Single player, local testing, fallback |

That split is what lets one published mod carry different content on every server. See [Server content](ServerContent.md).

The addon config is a plain text file with two levels: categories, each holding entries.

```
MrFrost_InfoMenuConfig {
 m_sTitle "My Server"
 m_aCategories {
  MrFrost_InfoMenuCategory "{7FA1C3D2E4B51900}" {
   m_sName "Rules"
   m_sText "Pick a rule on the left."
   m_aEntries {
    MrFrost_InfoMenuEntry "{7FA1C3D2E4B51901}" {
     m_sName "Teamkilling"
     m_sText "Don't."
    }
   }
  }
 }
}
```

Edit, restart the game, done.

## Two things that bite everyone

Both belong to the **addon config**. The server JSON has neither problem.

**Every block needs a unique GUID.** Those 16 hex characters in braces are identity, not decoration. Copy a category without changing its GUID and one of the two disappears without any error.

**Colours in the config are linear, not sRGB.** `0.5` is brighter than you expect. [Configuration](Configuration.md#colours) has ready-made values so you never have to convert anything.
