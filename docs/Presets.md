# Presets

[← Back to index](Home.md)

A preset is simply a complete config kept under a different name.

```
addon/Configs/MrFrost/Presets/
```

Files in this folder are **never loaded automatically**. They sit there until you copy one over the active config.

## Shipped presets

| File | Content |
|---|---|
| `InfoMenu_Default.conf` | Sample content with a built-in formatting and server-setup reference |

Only the info menu ships a preset. `Report.conf` and `Language.conf` are single files — there is nothing to swap between.

## Switching

Copy the one you want over the active config:

```
addon/addon/Configs/MrFrost/Presets/MyServer-Live.conf  →  addon/Configs/MrFrost/InfoMenu.conf
```

Restart the game.

> [!TIP]
> Keep your live config in `Presets/` as well, under its own name. Then switching back is a copy rather than a rewrite.

## Your own

Drop any config in the folder. Naming is free.

```
addon/Configs/MrFrost/Presets/
├── InfoMenu_Default.conf
├── MyServer-Live.conf
└── MyServer-Testing.conf
```

Each `.conf` needs a matching `.conf.meta` with its own GUID. Copy an existing meta file and change the GUID and path inside it:

```
MetaFileClass {
 Name "{YOUR_UNIQUE_GUID}Configs/MrFrost/Presets/MyServer-Live.conf"
 Configurations {
  CONFResourceClass PC {
  }
 }
}
```

Adding files means a Workbench rescan — see [Installation](Installation.md#when-you-need-the-workbench).

## Several servers, one mod

Presets are **not** the answer to that, and they no longer need to be.

A server's own content goes in JSON files on the server itself, outside the addon entirely — see [Server content](ServerContent.md). One published mod, different content everywhere it runs, nothing extra for players.

What presets are still good for:

- keeping the fallback content that ships with the addon under version control
- swapping between sample content and your own while developing
- storing a config you might want back

> [!TIP]
> If you are setting up a server, you probably want [Server content](ServerContent.md), not this page.

---

**Next:** [Troubleshooting](Troubleshooting.md)
