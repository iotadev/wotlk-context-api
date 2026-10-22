# WOTLK native context adapter

The native exporter for WOTLK build 12340 / AzerothCore. It copies scoped player
state and observed Playerbots AI/master relationships for the `wotlk_context`
query/MCP package. It defaults off and exposes no gameplay controls.

Install this directory as `modules/mod-game-context` in a compatible core source
tree with statically linked Playerbots. Preserve the directory name: the loader is
`Addmod_game_contextScripts`. Follow [installation and usage](../USAGE.md) for
configuration and Python setup, and [qualification](../QUALIFICATION.md) for the
historical build inputs and remaining runtime limits.

The generated build header records adapter source hashes and the available
Playerbots revision. Keep those alongside the actual core and binary identity
when recording a native build or test result.

## Optional in-process reader

src/ContextNative.h supplies a narrow read-only interface for statically linked
consumers such as Chatter. The adapter registers on world startup and clears its
reader at shutdown. Callers use the world thread, resolve players for each call
and never retain native game-object pointers. Disabled/absent providers supply no
facts. The reader checks enabled scope, boot/login generation and current native
fields through the same capture helper as periodic publication. It does not
publish, initialize AI or change gameplay.

Chatter optionally includes this sibling header without changing global include
paths. A missing/disabled provider rejects fact-dependent output; unknown-context
replies still use native audience eligibility. Consumers own requester/speaker
scope and actual delivery. No permission is granted by an observation.
