# Contributing

Thanks for taking the time. By taking part you agree to the
[Code of Conduct](CODE_OF_CONDUCT.md). This page covers what you need to change the addon
and get the change accepted.

## Layout

```
addon/      the addon, and the only thing the game loads
docs/       documentation
.github/    the workflow that builds the release archive from addon/ alone
```

Everything the engine reads lives under `addon/`, which is the folder
`-addonsDir` points at. Documentation and repository files sit beside it, so
nothing built or published from the addon carries them.

## Running it

```
ArmaReforgerSteam.exe -addonsDir "<folder containing addon/>" -addons 69F71634BA1052A0
```

The working directory has to be the game folder. `-addonsDir` adds a search path
without making the engine's own `./addons` absolute, so starting anywhere else
aborts on missing base data. The flag looks one level down for an `addon.gproj`
and may be repeated to load several mods from different places.

## Verifying a change

The engine is the only thing that reads scripts and configs. The Workbench's
`-validate` does not load the addon and reports success on code that does not
compile, so verification means booting the game far enough to compile the Game
module and parse the system configs:

```
ArmaReforgerSteam.exe -profile <temp> -addonsDir "<mods>" -addons 69F71634BA1052A0 -noSound -maxFPS 10
```

Diagnostics land in `<temp>/logs/logs_*/console.log`. Three things fail
independently and all three matter:

| Pattern | Meaning |
|---|---|
| `SCRIPT (E)` | a script does not compile |
| `DEFAULT (E)` | a `.conf` does not parse — this one can take the engine down |
| `ENGINE (F)` | the engine died |

Grepping only for `SCRIPT (E)` will call a build clean while the engine is
crashing. A clean run still logs resource-leak and GUID complaints at shutdown,
so a bare `(E)` grep calls every green run red.

**What this cannot check: input bindings.** `chimeraInputCommon.conf` is not
loaded at the main menu, so a key name that does not exist passes every check
and fails only once a mission starts. Read the names that do exist off the
vanilla configs rather than guessing them.

## Things that have cost hours

**A `.conf` must not contain `//` comments.** They are not ignored — they
desynchronise the parser, which then rejects valid lines further down and can
take the engine with it. The reported offset is where the parser noticed, not
where the comment was.

**New `.conf` and `.layout` resources need the Workbench once.** The engine only
reads `resourceDatabase.rdb`; only the Workbench rebuilds it, and only when a
project is opened through its interface. Until that happens the resource is
invisible and lookups fail.

**Do not switch branches while the Workbench has the project open.** Resource
files disappearing under it produce a null GUID and crash it. Nothing is
damaged; restarting the Workbench is the whole repair.

**EnforceScript has no ternary operator**, and a script method cannot take a
function reference — `func arguments are not supported in script methods`.

## Commits

[Conventional Commits 1.0.0](https://www.conventionalcommits.org/en/v1.0.0/):

```
<type>[optional scope]: <description>

[optional body]

[optional footer(s)]
```

Subject in the imperative, lowercase after the colon, no trailing period, at
most 50 characters. Blank line, then a body wrapped at 72 saying what and why —
the diff already says how. Types follow
[this cheat sheet](https://gist.github.com/qoomon/5dfcdf8eec66a051ecd85625518cfd13);
`feat` and `fix` are the ones that move the version.

One concern per commit. A subject needing "and" usually wants to be two commits.

## Changelog

[Keep a Changelog 1.1.0](https://keepachangelog.com/en/1.1.0/), maintained in
`CHANGELOG.md`. Add your entry under `Unreleased`.

## Content, not code

Most changes to what players read are not code changes at all. The text lives in
JSON on the server, documented in [docs/ServerContent.md](docs/ServerContent.md).
The files under `addon/server/MrFrost/` are the examples that ship; keep them
working and free of anything server-specific.

Save every file as **UTF-8**, and give each category and entry in the addon
config its own unique GUID — copying a block while keeping its GUID silently
drops one of the two.
