# Icons

[← Back to index](Home.md)

Icons appear in four places: next to each row of the info menu's list, next to a menu title, on a pause menu entry, and inline in text.

The addon ships none of its own — it uses the game's shared icon set.

## The shared set

```
{3262679C50EF4F01}UI/Textures/Icons/icons_wrapperUI.imageset
```

These sprite names are referenced by the game's own code, so they are safe to use:

| | | |
|---|---|---|
| `field-manual` | `players` | `characters` |
| `scenarios` | `compass` | `veh_trunk` |
| `settings` | `feedback` | `camera` |
| `respawn` | `restart` | `continue` |
| `gameMaster` | `armavision` | `recruitCommandAlt` |
| `blocked-users-list` | `sound-off` | `ingameInteraction` |
| `okCircle` | `cancelCircle` | `check` |
| `warning` | `not-available` | `disable` |

To see the rest, open the imageset in the Workbench resource browser.

## On a row

```
m_IconImageset "{3262679C50EF4F01}UI/Textures/Icons/icons_wrapperUI.imageset"
m_sIconName "warning"
```

Rows without an icon close the gap automatically, so a mixed list still lines up.

## Next to the title

Set once at the root of each feature's config. The same icon is used for that feature's pause menu entry, so the two stay in sync:

```
m_MenuIconImageset "{3262679C50EF4F01}UI/Textures/Icons/icons_wrapperUI.imageset"
m_sMenuIconName "field-manual"
```

Leave `m_sMenuIconName` empty to hide it in both places.

## Inline in text

```
<image set='{3262679C50EF4F01}UI/Textures/Icons/icons_wrapperUI.imageset' name='warning' scale='1'/>
```

See [Text formatting](Formatting.md#inline-images).

## Your own artwork

1. Put the `.edds` somewhere in the mod, for example `UI/Textures/`.
2. In the Workbench, create an imageset or open an existing one and drag the texture in.
3. Reference that imageset and the sprite name you gave it.

Alternatively point `m_Icon` straight at an `.edds`:

```
m_Icon "{YOURGUID}UI/Textures/MyIcon.edds"
```

The imageset wins if both are set. Imagesets are the sharper option and the one the vanilla UI uses throughout.

> [!NOTE]
> Adding new files means the project has to be opened in the Workbench once so they land in the resource database. See [Installation](Installation.md#when-you-need-the-workbench).

## If an icon does not show

The row simply has no icon and the gap closes — there is no error. Check that:

- the imageset path including the GUID is exactly right
- the sprite name matches, including capitalisation
- the file was picked up by a Workbench rescan, if you added it

---

**Next:** [Presets](Presets.md)
