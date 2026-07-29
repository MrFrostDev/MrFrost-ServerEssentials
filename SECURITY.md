# Security policy

## Supported versions

The latest release is supported. Fixes go into a new release rather than into
older ones.

| Version | Supported |
|---|---|
| latest | yes |
| everything else | no |

## Reporting a vulnerability

Use GitHub's **[Report a vulnerability](https://github.com/MrFrostDev/MrFrost-ServerEssentials/security/advisories/new)**
form. It opens a private advisory that only you and the maintainer can read.

Please do not open a public issue for anything that lets a player reach data or
actions they should not have — a public issue tells every server running the
addon at the same time it tells the maintainer.

Include the addon version, what a player can do that they should not, and the
shortest reproduction you have. `[MrFrost]` lines from the server log usually
say more than a description does.

Expect an acknowledgement within a few days. There is no bounty; this is a mod
maintained in spare time.

## What matters here

The addon takes text from players and settings from server owners, and both
cross a trust boundary.

**Player input reaches Discord and a log file.** Report descriptions are typed
by players and end up in a webhook payload and an appended log line. Anything
that lets a player break out of the JSON body, forge fields, ping a whole
Discord, or write a crafted line into the log file is in scope.

**Faction and target resolution happens on the authority.** The client says
*which option* was chosen, not *who* — the server answers from the kills and
hits it recorded. A path that lets a modified client name its own target, or
report a player who was never there, is in scope.

**Rate limits and length caps are enforced on the server.** A way around the
cooldown or the description limit is in scope.

Out of scope: anything requiring server-owner access to the profile directory,
and anything that only affects the person doing it.

## The webhook URL is a secret

`delivery.webhookUrl` in `report.json` lets anyone holding it post into your
channel. It belongs in your server's profile directory and nowhere else.

The `delivery` block is read on the server and **never sent to a client** —
that split is deliberate and is the one part of the file a client cannot see.
Never put the URL in the addon config: that ships to every subscriber.

If it leaks, regenerate the webhook in the Discord channel settings; the old URL
stops working immediately.

## Whitelists and admin lists

Where these exist in server configuration, they carry player identity IDs and
stay on the authority. They are stripped before the configuration reaches a
client. A build that sends them to clients is a vulnerability, not a feature.
