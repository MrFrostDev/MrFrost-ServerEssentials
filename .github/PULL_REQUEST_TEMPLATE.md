## What this changes

<!-- What the change does, and why. The diff already says how. -->

## Related

<!-- Fixes #123, Refs #456, or "none". -->

## Checklist

- [ ] Commits follow [Conventional Commits](https://www.conventionalcommits.org/en/v1.0.0/) — imperative subject, lowercase after the colon, at most 50 characters
- [ ] `CHANGELOG.md` has an entry under `Unreleased`, if this is user-visible
- [ ] Verified against the engine, not only by reading: `SCRIPT (E)`, `DEFAULT (E)` and `ENGINE (F)` are all clean
- [ ] New `.conf` or `.layout` resources were opened in the Workbench once, so their GUIDs are in `resourceDatabase.rdb`
- [ ] No `//` comments in any `.conf` — they desynchronise the parser
- [ ] Documentation updated where behaviour or configuration changed
- [ ] No secrets: no webhook URLs, no personal paths, no identity IDs

## How it was verified

<!-- What you ran, and what it said. "Compile check clean" plus the counts is enough. -->
