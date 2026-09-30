// Project-owned GDExtension adapter. Gameplay remains in sample_core.cpp.
#include "../../src/godot/sample_core.h"

#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/godot.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/dictionary.hpp>

#include <climits>

namespace godot
{
class TanksSampleCore : public RefCounted
{
    GDCLASS(TanksSampleCore, RefCounted)

    void *core_ = nullptr;
    String wrapperError_;

    bool readConfig(const Dictionary &settings, TanksSampleConfig &config)
    {
        struct Setting { const char *name; int *value; };
        const Setting fields[]{
            {"stage", &config.stage}, {"players", &config.players}, {"ai_p2", &config.ai_p2},
            {"lives", &config.lives}, {"nation_p1", &config.nation_p1}, {"nation_p2", &config.nation_p2},
            {"max_hp", &config.max_hp}, {"enemy_speed", &config.enemy_speed},
            {"enemy_fire", &config.enemy_fire}, {"enemy_spawn", &config.enemy_spawn},
            {"camera_yaw", &config.camera_yaw}, {"camera_elevation", &config.camera_elevation}};
        if (settings.size() != 12)
        {
            wrapperError_ = "Expected exactly the 12 native setup keys";
            return false;
        }
        for (const auto &field : fields)
        {
            if (!settings.has(field.name))
            {
                wrapperError_ = String("Missing setting: ") + field.name;
                return false;
            }
            const Variant value = settings[field.name];
            if (field.value == &config.ai_p2)
            {
                if (value.get_type() != Variant::BOOL)
                {
                    wrapperError_ = "ai_p2 must be a boolean";
                    return false;
                }
                *field.value = static_cast<bool>(value) ? 1 : 0;
            }
            else
            {
                if (value.get_type() != Variant::INT || static_cast<int64_t>(value) < INT_MIN ||
                    static_cast<int64_t>(value) > INT_MAX)
                {
                    wrapperError_ = String("Setting must be a signed 32-bit integer: ") + field.name;
                    return false;
                }
                *field.value = static_cast<int>(static_cast<int64_t>(value));
            }
        }
        return true;
    }

protected:
    static void _bind_methods()
    {
        ClassDB::bind_method(D_METHOD("open", "resources"), &TanksSampleCore::open);
        ClassDB::bind_method(D_METHOD("reset", "seed", "stage", "players", "ai_p2"),
                            &TanksSampleCore::reset);
        ClassDB::bind_method(D_METHOD("reset_config", "seed", "settings"), &TanksSampleCore::reset_config);
        ClassDB::bind_method(D_METHOD("start_config", "settings"), &TanksSampleCore::start_config);
        ClassDB::bind_method(D_METHOD("restart"), &TanksSampleCore::restart);
        ClassDB::bind_method(D_METHOD("lan_host", "seed", "settings", "port", "now_seconds"), &TanksSampleCore::lan_host);
        ClassDB::bind_method(D_METHOD("lan_join", "address", "nation", "camera_yaw", "camera_elevation", "now_seconds"), &TanksSampleCore::lan_join);
        ClassDB::bind_method(D_METHOD("lan_poll", "now_seconds", "local_bits"), &TanksSampleCore::lan_poll);
        ClassDB::bind_method(D_METHOD("lan_stop"), &TanksSampleCore::lan_stop);
        ClassDB::bind_method(D_METHOD("lan_status"), &TanksSampleCore::lan_status);
        ClassDB::bind_method(D_METHOD("map_pad", "slot", "x", "y", "buttons", "yaw"), &TanksSampleCore::map_pad);
        ClassDB::bind_method(D_METHOD("reset_pad", "slot", "suppress_stick"), &TanksSampleCore::reset_pad);
        ClassDB::bind_method(D_METHOD("step", "dt", "p1", "p2"), &TanksSampleCore::step);
        ClassDB::bind_method(D_METHOD("snapshot"), &TanksSampleCore::snapshot);
        ClassDB::bind_method(D_METHOD("drain_audio"), &TanksSampleCore::drain_audio);
        ClassDB::bind_method(D_METHOD("error"), &TanksSampleCore::error);
    }

public:
    ~TanksSampleCore() override { tanks_sample_destroy(core_); }

    bool open(const String &resources)
    {
        wrapperError_ = String();
        tanks_sample_destroy(core_);
        core_ = tanks_sample_create(resources.utf8().get_data());
        return core_ != nullptr;
    }

    bool reset(int seed, int stage, int players, bool aiP2)
    {
        wrapperError_ = String();
        return tanks_sample_reset(core_, seed, stage, players, aiP2 ? 1 : 0) == 0;
    }

    bool reset_config(int seed, const Dictionary &settings)
    {
        wrapperError_ = String();
        TanksSampleConfig config{};
        return readConfig(settings, config) && tanks_sample_reset_config(core_, seed, &config) == 0;
    }

    bool start_config(const Dictionary &settings)
    {
        wrapperError_ = String();
        TanksSampleConfig config{};
        return readConfig(settings, config) && tanks_sample_start_config(core_, &config) == 0;
    }

    bool step(double dt, int p1, int p2)
    {
        wrapperError_ = String();
        return tanks_sample_step(core_, dt, static_cast<unsigned>(p1),
                                 static_cast<unsigned>(p2)) == 0;
    }

    bool restart()
    {
        wrapperError_ = String();
        return tanks_sample_restart(core_) == 0;
    }

    bool lan_host(int seed, const Dictionary &settings, int port, double nowSeconds)
    {
        wrapperError_ = String();
        TanksSampleConfig config{};
        return readConfig(settings, config) && tanks_sample_lan_host(core_, seed, &config, port, nowSeconds) == 0;
    }

    bool lan_join(const String &address, int nation, int yaw, int elevation, double nowSeconds)
    {
        wrapperError_ = String();
        return tanks_sample_lan_join(core_, address.utf8().get_data(), nation, yaw, elevation, nowSeconds) == 0;
    }

    bool lan_poll(double nowSeconds, int localBits)
    {
        wrapperError_ = String();
        return tanks_sample_lan_poll(core_, nowSeconds, static_cast<unsigned>(localBits)) == 0;
    }

    bool lan_stop()
    {
        wrapperError_ = String();
        return tanks_sample_lan_stop(core_) == 0;
    }

    String lan_status() const
    {
        const char *value = tanks_sample_lan_status(core_);
        return value ? String::utf8(value) : String();
    }

    int map_pad(int slot, double x, double y, int buttons, int yaw)
    {
        wrapperError_ = String();
        return tanks_sample_map_pad(core_, slot, static_cast<float>(x), static_cast<float>(y),
                                    static_cast<unsigned>(buttons), yaw);
    }

    bool reset_pad(int slot, bool suppressStick)
    {
        wrapperError_ = String();
        return tanks_sample_reset_pad(core_, slot, suppressStick ? 1 : 0) == 0;
    }

    String snapshot() const
    {
        const char *value = tanks_sample_snapshot(core_);
        return value ? String::utf8(value) : String();
    }
    String drain_audio() const
    {
        const char *value = tanks_sample_drain_audio(core_);
        return value ? String::utf8(value) : String();
    }
    String error() const
    {
        return wrapperError_.is_empty() ? String::utf8(tanks_sample_error(core_)) : wrapperError_;
    }
};

void initializeSample(ModuleInitializationLevel level)
{
    if (level == MODULE_INITIALIZATION_LEVEL_SCENE)
        ClassDB::register_class<TanksSampleCore>();
}

void terminateSample(ModuleInitializationLevel) {}
} // namespace godot

extern "C" GDExtensionBool GDE_EXPORT tanks_sample_library_init(
    GDExtensionInterfaceGetProcAddress getProcAddress,
    GDExtensionClassLibraryPtr library, GDExtensionInitialization *initialization)
{
    godot::GDExtensionBinding::InitObject init(getProcAddress, library, initialization);
    init.register_initializer(godot::initializeSample);
    init.register_terminator(godot::terminateSample);
    init.set_minimum_library_initialization_level(godot::MODULE_INITIALIZATION_LEVEL_SCENE);
    return init.init();
}
