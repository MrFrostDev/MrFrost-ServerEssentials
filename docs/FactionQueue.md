# Faction queue

[← Back to index](Home.md)

> [!WARNING]
> Part built, none of it wired up. The rules, the config, the JSON schema, the queue and the history exist and are tested by nothing; no code calls them, so a server running this branch enforces no faction limits at all. This page describes the design and marks what is real.

Players pick a faction from the deploy menu. When the faction they want has no room, they take a place in a queue for it, watch their position, and are put in the moment a place opens.

## The problem it solves

A server that balances its factions ends up with locked buttons. A player who wants the side their squad is on finds it greyed out, waits without knowing for how long, and eventually picks the other side to get into the game at all — which is the outcome the balancing was meant to prevent.

The queue turns the locked button into a place in line with a number on it.

## What the addon takes over

The addon owns every rule that decides whether a player may join a faction:

| | |
|---|---|
| **Player limit** | A hard cap per faction |
| **Balance** | How far the factions may drift apart in size |
| **Whitelist** | Factions only named players may join |
| **Queue** | What happens to everyone the three above turned away |

These are one system on purpose. A player refused by any of them is a candidate for the queue, and the queue needs to know which rule refused them: a full faction frees up, a whitelist does not.

## The rule

One rule, applied to every request, whether it is a player's first pick of the session or their fifth switch.

A join is allowed when all of the following hold:

1. The faction is playable and has room below its `playerLimit`
2. The player is on the faction's whitelist, where the faction has one
3. Joining leaves no other balanced faction further behind than `maxImbalance`

For the third, the count the joiner would create is compared against every other balanced faction:

```
projected = target.playerCount + 1
for each other balanced faction:
    other = faction.playerCount
    if the player is currently on that faction:
        other = other - 1        # they are leaving it
    if projected - other > maxImbalance:
        refuse
```

Subtracting the faction a switcher is leaving matters: without it, a player moving from the larger side to the smaller one is refused for making the imbalance they are about to reduce.

**Returning players skip the queue.** A player who was on a faction earlier in the session may rejoin it, within its player limit, without balance or queue applying. Losing a connection is not a reason to lose your side.

## The queue

One queue per faction. One queue per player — picking a different faction leaves the previous queue.

**Position** counts the players ahead. It is recalculated whenever any faction's player count changes, because a place can open two ways: someone leaves the faction, or the other side gains a player and the balance allows one more.

**Reaching zero assigns the player.** No confirmation step: the player is sitting in the deploy menu waiting for exactly this. Assignment goes through `SetFaction_S()` on the authority, which is past the request gate — the queue has already decided.

**A full queue is shown as full.** The faction stays refused, labelled with its queue length, and the player picks another faction or tries again later.

**Leaving the queue** happens on disconnect, on assignment, and on picking another faction. Nothing else removes a player.

## Configuration

`<server profile directory>/MrFrost/factions.json`

```json
{
  "enabled": true,
  "maxImbalance": 3,
  "balanceMode": "INCLUDE",
  "balancedFactions": ["US", "USSR"],
  "queueLimit": 10,
  "returningPlayersSkipQueue": true,
  "factions": [
    {
      "key": "US",
      "playerLimit": 32,
      "whitelist": []
    },
    {
      "key": "USSR",
      "playerLimit": 32,
      "whitelist": []
    }
  ]
}
```

| Key | Type | Default | Meaning |
|---|---|---|---|
| `enabled` | bool | `false` | `false` leaves faction selection entirely to the game |
| `maxImbalance` | number | `3` | How many players one balanced faction may lead by. Clamped to at least 1 |
| `balanceMode` | string | `ALL` | `ALL`, `INCLUDE` or `EXCLUDE` — which factions the balance rule covers |
| `balancedFactions` | array | — | Faction keys for `INCLUDE` and `EXCLUDE` |
| `admins` | array | — | Account identities that may skip the queue, when `adminsSkipQueue` is on. Never sent to a client |
| `adminsSkipQueue` | bool | `true` | Whether the identities above skip the queue |
| `queueLimit` | number | `10` | Places per faction queue. `0` switches queueing off and refuses outright |
| `returningPlayersSkipQueue` | bool | `true` | A player rejoining a faction they held this session goes straight in |
| `factions[].key` | string | — | Faction key as the mission defines it |
| `factions[].playerLimit` | number | `-1` | Hard cap. `-1` is unlimited, `0` makes the faction unjoinable |
| `factions[].whitelist` | array | — | Player identity IDs. An empty list leaves the faction open to everyone |

## Where it plugs in

**The decision** is read from `GetOnCanPlayerFactionResponseInvoker_S()` and `GetOnPlayerFactionResponseInvoker_S()` on `SCR_PlayerFactionAffiliationComponent`. Both fire once on the authority with the outcome of a request.

Reading the outcome rather than overriding `CanRequestFaction_S()` keeps the addon independent of load order. Two mods overriding the same method form a `super` chain whose order follows the server's mod list, which the addon cannot know. The response invoker fires once at the end of that chain however it was assembled.

**The refusal reason** is worked out by the addon, from the same replicated counts the rule already reads. The invoker carries a bool and nothing else, so a refusal that came from somewhere else looks identical to one of ours. Queueing therefore starts from our own rule rather than from the fact that something said no.

**Assignment** calls `SetFaction_S(faction)` on the waiting player's component.

**The menu** gets its queue state from new widgets placed beside the faction rows. The addon does not touch `SetEnabled()` on the vanilla faction buttons: `SCR_FactionButton.UpdatePlayerCount()` re-evaluates the whole lock state on every count refresh, and a second mod writing to the same property wins or loses by load order again.

**Not built.** For a click to reach the server at all, the button has to be live. The addon therefore sets the vanilla `playerLimit` on every faction it manages to `-1` at startup and enforces the real cap itself. Nothing in the game locks a faction button after that, every click produces a request, and the addon answers all of them.

## Compatibility

Another mod that also balances factions has to be switched off, or two systems refuse each other's decisions and the queue never drains.

**WCS_Core** carries a team balancer and a faction whitelist. Both are mission header settings under `WCS_Core_Settings`:

```
m_bIsTeamBalancerEnabled  = false
m_bEnableFactionWhitelist = false
```

With both off, its faction button handling reduces to the game's own rules. Its balancer can also be toggled live by a Game Master through the editor attributes, and the addon flips the same switch at startup through a named call that fails harmlessly where the mod is absent:

```c
bool ignored;
GetGame().GetScriptModule().Call(factionManager, "WCS_SetIsTeamBalancerEnabled", false, ignored, false);
```

## What this costs

**The faction rows lose their "12 / 32" count.** The vanilla limit is set to `-1` so the buttons stay live, and the game draws no ceiling it does not have. The addon draws its own.

**A server running the addon has no faction limits without it.** The caps live in the addon rather than in the mission, so a server that removes it, or a session where the config fails to load, is a server with open factions until it is restarted with the caps back in place. The log says which limits were applied at startup.

**A refusal from a third party is invisible.** The addon queues on its own rules. Where something else refuses a request for its own reasons, the player sees the refusal without a queue behind it.

## Open items

- ~~Whether admins skip the queue, and how the addon recognises one~~ — answered: `adminsSkipQueue` plus an `admins` list of account identities
- What a player sees when the faction they are queued for becomes unplayable
- Whether the queue survives a mission restart, given that player counts reset with it
