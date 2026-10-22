/* GPL v2 or later. Read-only context; no native game object pointers. */
#ifndef WOTLK_CONTEXT_CACHE_H
#define WOTLK_CONTEXT_CACHE_H

#include <cstdint>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace WotlkContext
{
struct Sample
{
    uint64_t Guid = 0, Generation = 0, SampledMs = 0, WallMs = 0;
    uint64_t Group = 0, Target = 0, Master = 0;
    uint32_t Map = 0, Instance = 0, Health = 0, MaxHealth = 0;
    uint32_t Power = 0, MaxPower = 0, PowerType = 0, MovementFlags = 0;
    float X = 0, Y = 0, Z = 0, Orientation = 0;
    float RunSpeed = 0, SwimSpeed = 0, FlightSpeed = 0;
    bool HasMaster = false;
    bool Alive = false, Combat = false, Bot = false, Falling = false, Movement = false;
    std::string Name, State = "unobserved";
};

class Cache
{
public:
    Cache(std::vector<uint64_t> const& guids, uint32_t interval) : _interval(interval)
    {
        for (uint64_t guid : guids)
        {
            Sample row;
            row.Guid = guid;
            _rows.emplace(guid, std::move(row));
        }
    }

    void Transition(uint64_t guid, uint64_t now, uint64_t wall, bool login)
    {
        std::lock_guard<std::mutex> guard(_mutex);
        auto found = _rows.find(guid);
        if (found == _rows.end())
            return;
        uint64_t generation = found->second.Generation + 1;
        found->second = Sample{};
        found->second.Guid = guid;
        found->second.Generation = generation;
        found->second.State = login ? "awaiting_sample" : "offline";
        found->second.SampledMs = now;
        found->second.WallMs = wall;
    }

    std::optional<uint64_t> Due(uint64_t guid, uint64_t now) const
    {
        std::lock_guard<std::mutex> guard(_mutex);
        auto found = _rows.find(guid);
        if (found == _rows.end())
            return {};
        auto const& row = found->second;
        if (row.State == "unobserved" || row.State == "offline" || now < row.SampledMs)
            return {};
        if (row.State != "awaiting_sample" && now - row.SampledMs < _interval)
            return {};
        return row.Generation;
    }

    bool Publish(Sample row)
    {
        std::lock_guard<std::mutex> guard(_mutex);
        auto found = _rows.find(row.Guid);
        if (found == _rows.end() || found->second.Generation != row.Generation ||
            found->second.State == "offline" || row.SampledMs < found->second.SampledMs)
            return false;
        found->second = std::move(row);
        return true;
    }

    std::vector<Sample> Read() const
    {
        std::lock_guard<std::mutex> guard(_mutex);
        std::vector<Sample> rows;
        for (auto const& entry : _rows)
            rows.push_back(entry.second);
        return rows;
    }

private:
    uint32_t _interval;
    mutable std::mutex _mutex;
    std::map<uint64_t, Sample> _rows;
};
}
#endif
