/* GPL v2 or later. AzerothCore adapter for the shared read-only context contract. */
#include "ContextCache.h"
#include "ContextNative.h"
#include "Config.h"
#include "GitRevision.h"
#include "Group.h"
#include "Log.h"
#include "Map.h"
#include "ObjectGuid.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "PlayerbotMgr.h"
#include "Random.h"
#include "Realm.h"
#include "ScriptMgr.h"
#include "GameContextBuild.h"
#include "ContextJson.h"
#include <algorithm>
#include <atomic>
#include <charconv>
#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <thread>
#ifdef _WIN32
#include <windows.h>
#endif

namespace WotlkContext
{
using Clock = std::chrono::steady_clock;

uint64_t WallMs()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

class Exporter
{
public:
    Exporter(std::vector<uint64_t> const& guids, uint32_t interval, std::string const& file, bool movement)
        : Rows(guids, interval), _interval(interval), _file(file), _movement(movement),
          _origin(Clock::now()), _started(WallMs()), _realm(realm.Id.Realm)
    {
        // Native random helpers; boot identity is scoped to realm/adapter, not a permission token.
        _boot = std::to_string(_started) + "-" + std::to_string(urand(0, 0xFFFFFFFF));
    }

    ~Exporter() { Stop(); }

    uint64_t Now() const
    {
        return std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - _origin).count();
    }

    std::string const& Boot() const { return _boot; }
    uint32_t Realm() const { return static_cast<uint32_t>(_realm); }
    bool Enabled() const { return _enabled.load(); }
    bool Movement() const { return _movement; }

    void Start()
    {
        _writer = std::thread([this] { Run(); });
        _enabled.store(true);
    }

    void Stop()
    {
        _enabled.store(false);
        {
            std::lock_guard<std::mutex> guard(_waitMutex);
            _stop = true;
        }
        _wake.notify_all();
        if (_writer.joinable())
            _writer.join();
    }

    void CollectModules()
    {
        uint64_t now = Now();
        if (_moduleCaptured && now - _moduleCaptured < _interval)
            return;
        ModuleSnapshot snapshot;
        snapshot.CapturedMs = now;
        snapshot.WallMs = WallMs();
        auto reader = WotlkContextNative::RegisteredModuleReader();
        snapshot.Supported = reader != nullptr;
        // Cache lock is released by Read before calling the optional provider.
        if (reader)
            for (auto const& row : Rows.Read())
            {
                try
                {
                    auto payload = reader(row.Guid);
                    if (!payload.empty() && payload.size() <= 49152)
                        snapshot.Payloads.emplace(row.Guid, std::move(payload));
                }
                catch (std::exception const&)
                {
                    // A failing optional provider must not interrupt the world update.
                    // Missing scoped payload is exposed as module_unavailable.
                }
            }
        _moduleCaptured = now;
        std::lock_guard<std::mutex> guard(_moduleMutex);
        _modules = std::move(snapshot);
    }

    Cache Rows;

private:
    struct ModuleSnapshot
    {
        bool Supported = false;
        uint64_t CapturedMs = 0, WallMs = 0;
        std::map<uint64_t, std::string> Payloads;
    };
    void Write(bool stopped)
    {
        auto rows = Rows.Read();
        ModuleSnapshot modules;
        {
            std::lock_guard<std::mutex> guard(_moduleMutex);
            modules = _modules;
        }
        JsonWriter json;
        auto text = [&](char const* key, std::string const& value)
        {
            json.Key(key);
            json.String(value.c_str(), uint32_t(value.size()));
        };
        auto number = [&](char const* key, uint64_t value) { json.Key(key); json.Uint64(value); };
        auto boolean = [&](char const* key, bool value) { json.Key(key); json.Bool(value); };
        auto real = [&](char const* key, float value) { json.Key(key); json.Double(value); };
        auto guid = [&](char const* key, uint64_t value)
        {
            json.Key(key);
            if (value)
                json.String(std::to_string(value).c_str());
            else
                json.Null();
        };
        json.StartObject();
        number("schema_version", 1); text("source", "server");
        json.Key("server"); json.StartObject();
        text("expansion", "wotlk"); text("core", "AzerothCore"); number("client_build", 12340);
        text("boot_id", _boot); text("status", stopped ? "stopped" : "running"); number("realm_id", _realm);
        text("core_revision", GitRevision::GetHash()); text("playerbots_revision", GAME_CONTEXT_PLAYERBOTS_REVISION);
        text("adapter_source_identity", GAME_CONTEXT_SOURCE_IDENTITY);
        text("adapter_version", "0.4.0"); text("sampling_phase", "player_update_before_after_update_hooks");
        number("started_at_ms", _started); number("exported_at_ms", WallMs()); number("uptime_ms", Now());
        number("sequence", ++_sequence); number("sample_interval_ms", _interval); json.EndObject();
        json.Key("scope"); json.StartObject(); boolean("roster_complete", false);
        json.Key("characters"); json.StartArray();
        for (auto const& row : rows)
            json.String(std::to_string(row.Guid).c_str());
        json.EndArray(); json.EndObject();
        json.Key("capabilities"); json.StartObject();
        boolean("control", false); boolean("history", false); boolean("movement", _movement);
        json.Key("module_observations"); json.StartArray();
        if (modules.Supported)
            json.String("skate_pilot_v1");
        json.EndArray();
        json.Key("query_methods"); json.StartArray();
        json.String("context.get_capabilities"); json.String("game.get_party"); json.String("playerbots.get_bot_state");
        if (modules.Supported)
            json.String("modules.get_observations");
        json.EndArray(); json.EndObject();
        json.Key("entities"); json.StartArray();
        for (auto const& row : rows)
        {
            json.StartObject(); guid("guid", row.Guid); number("session_generation", row.Generation);
            text("state", row.State); number("sampled_ms", row.SampledMs); number("sampled_at_ms", row.WallMs);
            if (modules.Supported)
            {
                json.Key("module_observations"); json.StartObject();
                number("captured_ms", modules.CapturedMs); number("captured_at_ms", modules.WallMs);
                json.Key("skate_pilot");
                auto found = modules.Payloads.find(row.Guid);
                if (found == modules.Payloads.end())
                    json.Null();
                else
                    json.NativeObject(found->second);
                json.EndObject();
            }
            json.Key("player");
            if (row.State != "online")
                json.Null();
            else
            {
                json.StartObject(); text("name", row.Name); text("origin", "unknown");
                guid("group_guid", row.Group); guid("selected_target_guid", row.Target);
                boolean("alive", row.Alive); boolean("in_combat", row.Combat);
                number("map_id", row.Map); number("instance_id", row.Instance);
                json.Key("health"); json.StartObject();
                number("current", row.Health); number("max", row.MaxHealth); json.EndObject();
                json.Key("power"); json.StartObject();
                number("type", row.PowerType); number("current", row.Power); number("max", row.MaxPower);
                json.EndObject();
                json.Key("position"); json.StartObject();
                real("x", row.X); real("y", row.Y); real("z", row.Z); real("orientation", row.Orientation);
                json.EndObject();
                if (row.Movement)
                {
                    json.Key("movement"); json.StartObject();
                    text("kind", "native_movement_v1"); text("source", "server_native_state");
                    number("flags_raw", row.MovementFlags); boolean("falling", row.Falling);
                    real("run_speed", row.RunSpeed); real("swim_speed", row.SwimSpeed);
                    real("flight_speed", row.FlightSpeed); json.EndObject();
                }
                json.EndObject();
            }
            json.Key("bot");
            if (row.State != "online" || !row.Bot)
                json.Null();
            else
            {
                json.StartObject(); text("kind", "wotlk_ai_attachment_v1"); boolean("ai_attached", true);
                guid("controller_guid", row.Master);
                text("control_mode", !row.HasMaster ? "no_master" : !row.Master ? "other_master_unresolved" :
                    row.Master == row.Guid ? "self" : "other_master");
                json.EndObject();
            }
            json.EndObject();
        }
        json.EndArray(); json.EndObject();
        std::string buffer = json.Finish();
        auto temporary = _file;
        temporary += ".tmp";
        if (!_file.parent_path().empty())
            std::filesystem::create_directories(_file.parent_path());
        {
            std::ofstream stream(temporary, std::ios::binary | std::ios::trunc);
            stream.write(buffer.data(), std::streamsize(buffer.size()));
            stream.flush();
            if (!stream)
                throw std::runtime_error("snapshot write failed");
        }
#ifdef _WIN32
        if (!ReplaceFileW(_file.c_str(), temporary.c_str(), nullptr, 0, nullptr, nullptr))
        {
            DWORD error = GetLastError();
            if (error != ERROR_FILE_NOT_FOUND ||
                !MoveFileExW(temporary.c_str(), _file.c_str(), MOVEFILE_WRITE_THROUGH))
                throw std::runtime_error("snapshot replacement failed");
        }
#else
        std::filesystem::rename(temporary, _file);
#endif
    }

    void Run()
    {
        for (;;)
        {
            bool stopped;
            {
                std::lock_guard<std::mutex> guard(_waitMutex);
                stopped = _stop;
            }
            try
            {
                Write(stopped);
            }
            catch (std::exception const& error)
            {
                if (++_errors == 1 || _errors % 60 == 0)
                    LOG_ERROR("module.context", "Snapshot export failed: {}", error.what());
            }
            if (stopped)
                return;
            std::unique_lock<std::mutex> lock(_waitMutex);
            _wake.wait_for(lock, std::chrono::milliseconds(_interval), [this] { return _stop; });
        }
    }

    std::mutex _moduleMutex;
    ModuleSnapshot _modules;
    uint64_t _moduleCaptured = 0;
    uint32_t _interval;
    std::filesystem::path _file;
    bool _movement;
    Clock::time_point _origin;
    uint64_t _started, _realm, _sequence = 0, _errors = 0;
    std::string _boot;
    std::atomic<bool> _enabled{false};
    std::thread _writer;
    std::mutex _waitMutex;
    std::condition_variable _wake;
    bool _stop = false;
};

// Created before login; retained through map-thread shutdown. No runtime reload/destruction.
std::unique_ptr<Exporter> Service;

void CapturePlayer(Player* player, Sample& row, bool movement)
{
    uint64_t guid = player->GetGUID().GetRawValue();
    row.Name = player->GetName(); row.Alive = player->IsAlive(); row.Combat = player->IsInCombat();
    row.Map = player->GetMapId(); row.Instance = player->GetInstanceId();
    row.Target = player->GetTarget().GetRawValue();
    if (Group* group = player->GetGroup())
        row.Group = group->GetGUID().GetRawValue();
    row.Health = player->GetHealth(); row.MaxHealth = player->GetMaxHealth();
    auto power = player->getPowerType();
    row.PowerType = uint32_t(power); row.Power = player->GetPower(power); row.MaxPower = player->GetMaxPower(power);
    row.X = player->GetPositionX(); row.Y = player->GetPositionY(); row.Z = player->GetPositionZ();
    row.Orientation = player->GetOrientation();
    // Lookup only; do not initialize an engine or evaluate strategy values.
    if (PlayerbotAI* ai = PlayerbotsMgr::instance().GetPlayerbotAI(player))
    {
        row.Bot = true;
        if (Player* master = ai->GetMaster())
        {
            row.HasMaster = true;
            if (master == player)
                row.Master = guid;
            else
            {
                // Compare the raw identity only; never dereference a master
                // owned by another map. Resolution is bounded and map-local.
                uint32_t inspected = 0;
                for (auto const& reference : player->GetMap()->GetPlayers())
                {
                    if (++inspected > 64)
                        break;
                    Player* local = reference.GetSource();
                    if (local == master)
                    {
                        row.Master = local->GetGUID().GetRawValue();
                        break;
                    }
                }
            }
        }
    }
    if (movement)
    {
        row.Movement = true; row.MovementFlags = player->GetUnitMovementFlags();
        row.Falling = player->HasUnitMovementFlag(MOVEMENTFLAG_FALLING | MOVEMENTFLAG_FALLING_FAR);
        row.RunSpeed = player->GetSpeed(MOVE_RUN); row.SwimSpeed = player->GetSpeed(MOVE_SWIM);
        row.FlightSpeed = player->GetSpeed(MOVE_FLIGHT);
    }
}

bool ReadCurrent(Player* player, WotlkContextNative::Observation& out)
{
    if (!Service || !Service->Enabled() || !player || !player->IsInWorld() || player->IsBeingTeleported())
        return false;
    uint64_t guid = player->GetGUID().GetRawValue();
    auto rows = Service->Rows.Read();
    auto found = std::find_if(rows.begin(), rows.end(), [guid](Sample const& row) { return row.Guid == guid; });
    if (found == rows.end() || found->State != "online" || !found->Generation)
        return false;
    Sample live;
    CapturePlayer(player, live, false);
    out.Boot = Service->Boot(); out.Realm = Service->Realm();
    out.Guid = guid; out.Generation = found->Generation; out.Name = live.Name;
    out.Group = live.Group; out.Map = live.Map; out.Instance = live.Instance;
    out.Alive = live.Alive; out.Combat = live.Combat;
    out.Health = live.Health; out.MaxHealth = live.MaxHealth;
    out.PowerType = live.PowerType; out.Power = live.Power; out.MaxPower = live.MaxPower;
    out.Bot = live.Bot; out.HasMaster = live.HasMaster; out.Master = live.Master;
    return true;
}

class ContextWorld final : public WorldScript
{
public:
    ContextWorld() : WorldScript("GameContextWorld") { }

    void OnStartup() override
    {
        if (!sConfigMgr->GetOption<bool>("ContextAPI.Enabled", false))
            return;
        std::vector<uint64_t> guids;
        std::string input = sConfigMgr->GetOption<std::string>("ContextAPI.Characters", "");
        size_t offset = 0;
        while (offset < input.size())
        {
            size_t end = input.find(',', offset);
            if (end == std::string::npos)
                end = input.size();
            uint32_t counter = 0;
            auto parsed = std::from_chars(input.data() + offset, input.data() + end, counter);
            uint64_t guid = ObjectGuid::Create<HighGuid::Player>(counter).GetRawValue();
            if (!counter || parsed.ec != std::errc{} || parsed.ptr != input.data() + end ||
                std::find(guids.begin(), guids.end(), guid) != guids.end() || guids.size() >= 8)
            {
                guids.clear();
                break;
            }
            guids.push_back(guid);
            offset = end + 1;
            if (end < input.size() && offset == input.size())
            {
                guids.clear();
                break;
            }
        }
        uint32_t interval = sConfigMgr->GetOption<uint32_t>("ContextAPI.IntervalMs", 1000);
        auto file = sConfigMgr->GetOption<std::string>("ContextAPI.SnapshotFile", "context-api/snapshot.json");
        if (guids.empty() || interval < 250 || interval > 10000 || file.empty())
        {
            LOG_ERROR("module.context", "Context disabled: invalid scope, interval or snapshot path");
            return;
        }
        try
        {
            Service = std::make_unique<Exporter>(guids, interval, file,
                sConfigMgr->GetOption<bool>("ContextAPI.Movement", false));
            Service->CollectModules();
            Service->Start();
            WotlkContextNative::RegisteredReader() = &ReadCurrent;
        }
        catch (std::exception const& error)
        {
            Service.reset();
            LOG_ERROR("module.context", "Context startup failed: {}", error.what());
        }
    }

    void OnUpdate(uint32 /*diff*/) override
    {
        if (Service && Service->Enabled())
            Service->CollectModules();
    }

    void OnShutdown() override
    {
        WotlkContextNative::RegisteredReader() = nullptr;
        if (Service)
            Service->Stop();
    }
};

class ContextPlayers final : public PlayerScript
{
public:
    ContextPlayers() : PlayerScript("GameContextPlayers",
        {PLAYERHOOK_ON_LOGIN, PLAYERHOOK_ON_BEFORE_LOGOUT, PLAYERHOOK_ON_UPDATE}) { }

    void OnPlayerLogin(Player* player) override
    {
        if (Service && Service->Enabled())
            Service->Rows.Transition(player->GetGUID().GetRawValue(), Service->Now(), WallMs(), true);
    }

    void OnPlayerBeforeLogout(Player* player) override
    {
        if (Service && Service->Enabled())
            Service->Rows.Transition(player->GetGUID().GetRawValue(), Service->Now(), WallMs(), false);
    }

    void OnPlayerUpdate(Player* player, uint32 /*diff*/) override
    {
        if (!Service || !Service->Enabled())
            return;
        uint64_t now = Service->Now(), guid = player->GetGUID().GetRawValue();
        auto generation = Service->Rows.Due(guid, now);
        if (!generation)
            return;
        Sample row;
        row.Guid = guid; row.Generation = *generation; row.SampledMs = now; row.WallMs = WallMs();
        row.State = player->IsBeingTeleported() ? "transferring" : "online";
        if (row.State == "transferring")
        {
            Service->Rows.Publish(std::move(row));
            return;
        }
        CapturePlayer(player, row, Service->Movement());
        Service->Rows.Publish(std::move(row));
    }
};
}

void Addmod_game_contextScripts()
{
    new WotlkContext::ContextWorld();
    new WotlkContext::ContextPlayers();
}
