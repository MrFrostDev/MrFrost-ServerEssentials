# Text formatting

[← Back to index](Home.md)

Body text understands rich text — `m_sText` in the addon config, `text` in a server's JSON. The two are identical; nothing in the markup changes between them. Long texts scroll. One page holds up to 20000 characters; a longer one is left out entirely rather than cut, because a cut cannot leave markup whole, and both consoles name the page. Split it into entries instead.

## Markup

| Markup | Effect |
|---|---|
| `<br/>` | Line break |
| `<b>text</b>` | Bold |
| `<color rgba='226,167,79,255'>text</color>` | Coloured text, values 0–255 |
| `<image set='{GUID}path.imageset' name='Sprite' scale='1'/>` | Inline image |

That is the whole set. There is no italic, no underline, no headings, no list markup.

## Paragraphs

A single `<br/>` breaks the line. **Two** leave a blank line, which is what separates paragraphs:

```
First paragraph.<br/><br/>Second paragraph.
```

Without the blank line everything runs together into a wall of text. Use it generously — the panel is wide and unbroken text is hard to scan.

## Headings

There is no heading markup, so use a coloured line followed by a blank line:

```
<color rgba='226,167,79,255'>Sanctions</color><br/><br/>Body text follows here.
```

Keeping every heading in the same colour is what makes a long entry readable.

## Lists

No list markup either. A bullet character with leading spaces works:

```
  •  First item<br/>  •  Second item<br/>  •  Third item
```

The two spaces before and after the bullet give it room to breathe. Single `<br/>` between items keeps them tight; for items that wrap onto a second line, `<br/><br/>` reads better.

An icon makes a better bullet than `•`, and carries meaning as well:

```
<image set='{3262679C50EF4F01}UI/Textures/Icons/icons_wrapperUI.imageset' name='check' scale='1'/>  Required
<image set='{3262679C50EF4F01}UI/Textures/Icons/icons_wrapperUI.imageset' name='cancelCircle' scale='1'/>  Forbidden
```

Wrapping the icon inside a `<color>` tag tints it along with the text, so a red prohibition list stays red throughout.

## Inline images

```
<image set='{3262679C50EF4F01}UI/Textures/Icons/icons_wrapperUI.imageset' name='warning' scale='1'/>  Careful with this one
```

`scale` resizes relative to the text. `1` sits at line height, `2` is double.

Two spaces after the tag stop the icon from touching the following word.

See [Icons](Icons.md) for names that are known to work.

## Colour conventions

Nothing enforces these, but a consistent scheme makes a ruleset scannable:

| Purpose | Value |
|---|---|
| Headings, rule numbers | `226,167,79,255` — the Reforger gold |
| Hard prohibitions | `249,67,67,255` — red |
| Warnings, middle severity | `234,203,131,255` — light yellow |
| Confirmations | `67,194,93,255` — green |

These are the game's own UI colours, so they sit naturally against the panel.

## Putting it together

```
m_sText "<color rgba='226,167,79,255'>Reporting a rule break</color><br/><br/>Bring evidence. A clip or a witness.<br/><br/>  •  Open a ticket on Discord<br/>  •  Attach the clip<br/>  •  Name the player and the time<br/><br/><color rgba='249,67,67,255'>Without evidence</color><br/><br/>It comes down to one word against another and usually ends without a sanction."
```

## Structuring long content

If an entry needs more than a screen, consider splitting it into several entries under one category instead. A reader scanning a list of short titles finds things faster than one scrolling through a long page — and each entry gets its own row icon.

---

**Next:** [Icons](Icons.md)
