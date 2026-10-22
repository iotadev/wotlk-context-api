# WOTLK native context adapter

This directory is the canonical WOTLK adapter of the shared context project.
Cata uses server-module; both feed the same wotlk_context reader/query/capture/MCP
interfaces. The WOTLK profile targets AzerothCore build 12340 and matching native
Playerbots. The distributed runtime configuration remains disabled by default.
No SQL, gameplay control or model dependency is required by the observer.

## Installation and current qualification

Install this source as modules/mod-game-context in the chosen AzerothCore source
checkout. Preserve the name: the static loader is Addmod_game_contextScripts.
The module requires static linkage with Playerbots. Its generated header records
actual adapter source hashes and the available Playerbots revision; also retain
core dirty-input and executable identities with a qualified build.

Install the module with the core's ordinary source/build workflow, then copy
its distributed configuration to the configured runtime directory. Choose the
character scope and snapshot destination explicitly; defaults keep the observer
off. No particular Docker layout, image or pilot configuration is required.
A source installation alone does not qualify a later changed binary.

See [qualification history](../QUALIFICATION.md) and
[shared handoff](../QUALIFICATION.md) for dated paired-build, self/other-master and
separate-companion packet evidence. The historical pilot is not a current
deployment claim. Cata completed far transfer, other controller transitions, production
overhead and collision/physics remain outside this evidence. Companion completions
were controlled fixtures, not real model qualification.

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
