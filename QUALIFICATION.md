# Compatibility and qualification

This is a source alpha, not a prebuilt or universally compatible core module.
Target: AzerothCore build 12340; native module directory `mod-game-context`.

Earlier isolated native build evidence used core base `f24b3b3ea34ada3c5038bb387ae60ef16a5e24d8` and
Playerbots base `29485c926bdc1f99b63ef9dfd3e45aafd15f861c`, with additional recorded development inputs.
These base SHAs are not a claim that every release file was tested against an
immutable clean pair. Retain exact source/binary identity for your deployment.
The source hash manifest identifies this candidate's shared Python inputs.

The October 21 development workspace passed 105 Python tests with optional MCP.
This independent candidate has its own tests; the local release check and hosted
CI must be evaluated separately. No fresh full native core build or live realm
replay is claimed merely by exporting or packaging this release.

Prior native records qualify only their dated inputs. They include copied-state
lifecycle and observer operation. Broader controller transitions, native overhead,
client outcomes and complete dungeon behavior remain unqualified. Cata completed
far transfers are not qualified. Companion model quality is not established.

WOTLK requires matching AzerothCore and mod-playerbots native APIs, static linkage and the Addmod_game_contextScripts loader. Cata hooks cannot be substituted. The provider and optional consumers remain disabled by default. AI attachment alone does not establish character origin or control authority.
