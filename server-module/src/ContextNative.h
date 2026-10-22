/* GPL v2 or later. Optional, read-only native observation interface. */
#ifndef WOTLK_CONTEXT_NATIVE_H
#define WOTLK_CONTEXT_NATIVE_H

#include <cstdint>
#include <string>

class Player;

namespace WotlkContextNative
{
struct Observation
{
    std::string Boot, Name;
    std::uint64_t Guid = 0, Generation = 0, Group = 0, Master = 0;
    std::uint32_t Realm = 0, Map = 0, Instance = 0, Health = 0, MaxHealth = 0;
    std::uint32_t PowerType = 0, Power = 0, MaxPower = 0;
    bool Alive = false, Combat = false, Bot = false, HasMaster = false;
};

// Registration/startup/shutdown and reads belong to the world thread. This
// interface stores only a static function pointer, never a game object pointer.
// Static module linkage gives both translation units the same inline registry.
using Reader = bool (*)(Player*, Observation&);
inline Reader& RegisteredReader()
{
    static Reader reader = nullptr;
    return reader;
}

// One optional module provider. Called only by the world thread for configured
// subjects. Return copied, bounded JSON objects produced by native serialization;
// empty means unavailable. No player pointers or observer locks cross this call.
using ModuleReader = std::string (*)(std::uint64_t);
inline ModuleReader& RegisteredModuleReader()
{
    static ModuleReader reader = nullptr;
    return reader;
}

inline bool Read(Player* player, Observation& out)
{
    out = Observation{};
    Reader reader = RegisteredReader();
    return reader && reader(player, out);
}
}
#endif
