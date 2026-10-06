#ifndef TANKS3D_GODOT_SAMPLE_CORE_H
#define TANKS3D_GODOT_SAMPLE_CORE_H

// Experimental presentation bridge. A handle owns one production Game3D and
// must be used and destroyed on the same thread. No window/audio is opened.
// Inputs are held bit masks: north=1, south=2, west=4, east=8, fire=16,
// confirm report=32, pause=64. Direction and UI rising edges are derived here.
// reset/step return 0 on success, -1 on failure. Negative/non-finite dt and
// unknown input bits reject without advancing; finite dt clamps to 0.05 s.
// reset uses normal 3 lives, default gameplay settings and USA/USSR nations.
// ai_p2 requires players=2. Intro/report phases retain production timing.
// snapshot/error strings belong to the handle and last until its next call.
// A null snapshot signals failure; error(nullptr) describes create failures.
// Valid non-null handles are required (arbitrary/freed pointers are invalid).
#ifdef __cplusplus
extern "C"
{
#endif

typedef struct TanksSampleConfig
{
    int stage;
    int players;
    int ai_p2;
    int lives;
    int nation_p1;
    int nation_p2;
    int max_hp;
    int enemy_speed;
    int enemy_fire;
    int enemy_spawn;
    int camera_yaw;
    int camera_elevation;
} TanksSampleConfig;

void *tanks_sample_create(const char *resource_root);
void tanks_sample_destroy(void *handle);
// Full offline setup. Strictly rejects unknown nation/range/step values before
// replacing a running world. Yaw/elevation and enemy tuning use 5-unit steps.
// The old reset retains its original three-life/default-settings behavior.
void tanks_sample_default_config(TanksSampleConfig *config);
int tanks_sample_reset_config(void *handle, int seed, const TanksSampleConfig *config);
// Start another run in an existing app session, preserving its RNG/highscore
// exactly like the production setup menu. Requires one prior successful reset.
int tanks_sample_start_config(void *handle, const TanksSampleConfig *config);
int tanks_sample_restart(void *handle);
int tanks_sample_reset(void *handle, int seed, int stage, int players, int ai_p2);
int tanks_sample_step(void *handle, double dt, unsigned p1_bits, unsigned p2_bits);
const char *tanks_sample_snapshot(void *handle);
// The same read-only presentation fields as snapshot, with digest omitted.
// Rendering can avoid serializing/hashing the complete deterministic world;
// tests, LAN diagnostics and final reports retain the full snapshot above.
const char *tanks_sample_presentation_snapshot(void *handle);
// Ordered native audio commands, outside the simulation snapshot/RNG/digest.
// Returns a JSON array of play(cue: AudioCue ordinal), engine(active,moving),
// and stop operations, then clears the queue. No world state is advanced.
const char *tanks_sample_drain_audio(void *handle);
const char *tanks_sample_error(void *handle);
// Optional LAN reuses the existing protocol/session/channel. The supplied
// monotonic time is wall time in seconds (not clamped simulation dt).
// Host config must have players=2 and ai_p2=0. Port0 asks the OS for a free port.
// Local input uses the same held bits; bit128 requests host-only restart.
// While a LAN session exists, only lan_poll may advance it. Even after a
// disconnect, explicit reset/reset_config is required to return to offline.
int tanks_sample_lan_host(void *handle, int seed, const TanksSampleConfig *config,
                          int port, double now_seconds);
int tanks_sample_lan_join(void *handle, const char *address, int nation,
                          int camera_yaw, int camera_elevation, double now_seconds);
int tanks_sample_lan_poll(void *handle, double now_seconds, unsigned local_bits);
int tanks_sample_lan_stop(void *handle);
const char *tanks_sample_lan_status(void *handle);
// Reuse the production stick hysteresis, camera rotation and D-pad priority.
// Physical slots0..3 remain stable in the host UI. Input is held D-pad1/2/4/8,
// any fire16, bottom face32, Start64, Back128. Output includes held direction/
// fire and rising confirm32/pause64/cancel128. Start also confirms reports.
// These UI output bits are pulses; strip cancel before calling step/lan_poll.
int tanks_sample_map_pad(void *handle, int slot, float x, float y, unsigned buttons, int yaw);
int tanks_sample_reset_pad(void *handle, int slot, int suppress_stick);

#ifdef __cplusplus
}
#endif

#endif
