# Source package only until installed as modules/mod-game-context in a chosen build checkout.
# Playerbots is an explicit dependency. This adapter has no gameplay writes.
ModuleNameToVariable(mod-game-context CONTEXT_MODULE_MODE)
if(${CONTEXT_MODULE_MODE} STREQUAL "disabled")
  return()
endif()
ModuleNameToVariable(mod-playerbots CONTEXT_PLAYERBOTS_MODE)
if(NOT ${CONTEXT_MODULE_MODE} STREQUAL "static" OR NOT ${CONTEXT_PLAYERBOTS_MODE} STREQUAL "static")
  message(FATAL_ERROR "This context adapter currently requires static context and Playerbots modules")
endif()
if(NOT EXISTS "${CMAKE_SOURCE_DIR}/modules/mod-playerbots/src/Bot/PlayerbotAI.h")
  message(FATAL_ERROR "mod-game-context requires the matching native Playerbots module")
endif()
set(GAME_CONTEXT_PLAYERBOTS_REVISION "unavailable")
if(EXISTS "${CMAKE_SOURCE_DIR}/modules/mod-playerbots/.git")
  execute_process(COMMAND "${GIT_EXECUTABLE}" -C "${CMAKE_SOURCE_DIR}/modules/mod-playerbots" describe --always --dirty
    OUTPUT_VARIABLE GAME_CONTEXT_PLAYERBOTS_REVISION OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET
    RESULT_VARIABLE GAME_CONTEXT_GIT_RESULT)
  if(NOT GAME_CONTEXT_GIT_RESULT EQUAL 0 OR NOT GAME_CONTEXT_PLAYERBOTS_REVISION)
    set(GAME_CONTEXT_PLAYERBOTS_REVISION "unavailable")
  endif()
endif()
# Fingerprint the actual adapter sources; a module/core HEAD cannot identify dirty adapter code.
file(SHA256 "${CMAKE_CURRENT_LIST_DIR}/src/ContextModule.cpp" CONTEXT_MODULE_HASH)
file(SHA256 "${CMAKE_CURRENT_LIST_DIR}/src/ContextCache.h" CONTEXT_CACHE_HASH)
file(SHA256 "${CMAKE_CURRENT_LIST_DIR}/src/ContextJson.h" CONTEXT_JSON_HASH)
file(SHA256 "${CMAKE_CURRENT_LIST_DIR}/src/ContextNative.h" CONTEXT_NATIVE_HASH)
string(SHA256 CONTEXT_JSON_HASH "${CONTEXT_JSON_HASH}:${CONTEXT_NATIVE_HASH}")
set(GAME_CONTEXT_SOURCE_IDENTITY "${CONTEXT_MODULE_HASH}:${CONTEXT_CACHE_HASH}:${CONTEXT_JSON_HASH}")
configure_file("${CMAKE_CURRENT_LIST_DIR}/GameContextBuild.h.in"
  "${CMAKE_CURRENT_BINARY_DIR}/game-context-build/GameContextBuild.h" @ONLY)
# Keep adapter-only headers private; adding this optional module must not change
# every existing module's compiler include search path or expose its internals.
get_target_property(CONTEXT_GLOBAL_INCLUDES modules INCLUDE_DIRECTORIES)
list(REMOVE_ITEM CONTEXT_GLOBAL_INCLUDES "${CMAKE_CURRENT_LIST_DIR}/src")
set_property(TARGET modules PROPERTY INCLUDE_DIRECTORIES "${CONTEXT_GLOBAL_INCLUDES}")
set_property(SOURCE "${CMAKE_CURRENT_LIST_DIR}/src/ContextModule.cpp" APPEND PROPERTY
  INCLUDE_DIRECTORIES "${CMAKE_CURRENT_BINARY_DIR}/game-context-build")
