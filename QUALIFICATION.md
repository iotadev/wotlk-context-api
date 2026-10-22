# Compatibility and qualification

This is a source alpha, not a prebuilt or universally compatible core module.
Target: AzerothCore build 12340; native module directory `mod-game-context`.

Earlier isolated native build evidence used core base `f24b3b3ea34ada3c5038bb387ae60ef16a5e24d8` and
Playerbots base `29485c926bdc1f99b63ef9dfd3e45aafd15f861c`, with additional recorded development inputs.
These base SHAs are not a claim that every release file was tested against an
immutable clean pair. Retain exact source/binary identity for your deployment.
The source hash manifest identifies this release's shared Python inputs.

Python/package and hosted CI results are recorded in [VALIDATION.md](VALIDATION.md).
Publication did not include a fresh full native core build or live realm replay.
Earlier native observations include copied-state lifecycle and observer operation;
broader controller transitions, native overhead, client outcomes and complete
dungeon behavior remain unqualified.

WOTLK requires matching AzerothCore and mod-playerbots native APIs, static linkage and the Addmod_game_contextScripts loader. Cata hooks cannot be substituted. The provider and optional consumers remain disabled by default. AI attachment alone does not establish character origin or control authority.
