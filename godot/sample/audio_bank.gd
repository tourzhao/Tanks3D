extends Node

# AudioCue order and voice policy match the original raylib AudioBank. The
# native AudioOutput supplies requests; visual events never guess which cue won.
const CUES := ["stage_start_up", "pause", "game_over", "highscore_beaten",
    "menu_item_selected", "bonus_appeared", "bonus_obtained", "bullet_hit_brick",
    "bullet_hit_map_boundaries", "bullet_hit_stone", "bullet_hit_bullet",
    "eagle_destroyed", "enemy_destroyed", "enemy_hit", "player_destroyed",
    "player_fired", "player_hit", "player_idle", "player_life_up", "player_moving",
    "player_respawn", "score_point_counted"]
const VOLUMES := [1.0, 1.0, 1.0, 1.0, 0.7, 0.9, 0.9, 0.4, 0.4, 0.4, 0.4,
    0.7, 1.0, 0.7, 0.6, 0.6, 1.0, 0.5, 1.0, 0.5, 1.0, 0.8]
const OVERLAP := [1.0, 1.0, 1.0, 1.0, 1.0, 0.9, 0.9, 0.7, 1.0, 1.0, 1.0,
    1.0, 0.7, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0]
const SINGLE := [0, 1, 2, 3, 17, 19, 20]
const PRIORITY := [0, 3, 20]
const VOICE_LIMIT := 12
var streams: Array[AudioStream] = []
var voices: Array = []
var enabled := true
var volume := 0.75
var bus := &"Master"
var engine_active := false
var engine_moving := false


func _ready() -> void:
    for index in range(CUES.size()):
        var stream: AudioStream = load("res://resources/sounds/%s.ogg" % CUES[index])
        streams.append(stream)
        # Keep recordings resident before any playback; allocate mixer players
        # only when requested, then reuse them for this bank's lifetime.
        var group: Array[AudioStreamPlayer] = []
        voices.append(group)


func _create_voice(cue: int) -> AudioStreamPlayer:
    var group: Array[AudioStreamPlayer] = voices[cue]
    if group.size() >= (1 if cue in SINGLE else VOICE_LIMIT):
        return null
    var voice := AudioStreamPlayer.new()
    voice.stream = streams[cue]
    voice.bus = bus
    voice.volume_db = linear_to_db(VOLUMES[cue] * volume)
    voice.set_meta("cue_gain", VOLUMES[cue])
    add_child(voice)
    group.append(voice)
    return voice


# Keep the driver's playback lifecycle separate from allocation and policy so
# headless checks can control completion without queuing Dummy mixer streams.
func _voice_playing(voice: AudioStreamPlayer) -> bool:
    return voice.playing


func _play_voice(voice: AudioStreamPlayer) -> void:
    voice.play()


func _stop_voice(voice: AudioStreamPlayer) -> void:
    voice.stop()


func set_volume(value: float) -> void:
    volume = clampf(value, 0.0, 1.0)
    for group in voices:
        for voice in group:
            # -INF is intentional: the zero slider must really mute.
            voice.volume_db = linear_to_db(float(voice.get_meta("cue_gain")) * volume)


func set_enabled(value: bool) -> void:
    enabled = value
    if not value:
        stop_all()


func highest_priority_playing() -> bool:
    for cue in PRIORITY:
        if not voices[cue].is_empty() and _voice_playing(voices[cue][0]):
            return true
    return false


func play(cue: int) -> void:
    if not enabled or cue < 0 or cue >= CUES.size():
        return
    if cue in PRIORITY:
        stop_all()
    elif highest_priority_playing():
        return
    var available: AudioStreamPlayer
    var active := 0
    for voice in voices[cue]:
        if _voice_playing(voice):
            active += 1
        elif available == null:
            available = voice
    if available == null:
        available = _create_voice(cue)
        if available == null:
            return
    var gain: float = VOLUMES[cue] * pow(OVERLAP[cue], active)
    available.set_meta("cue_gain", gain)
    available.volume_db = linear_to_db(gain * volume)
    _play_voice(available)


func update_engine(active: bool, moving: bool) -> void:
    engine_active = active
    engine_moving = moving
    refresh_engine()


func refresh_engine() -> void:
    if not enabled or not engine_active or highest_priority_playing():
        for cue in [17, 19]:
            for voice in voices[cue]: _stop_voice(voice)
        return
    var wanted_cue := 19 if engine_moving else 17
    var other_cue := 17 if engine_moving else 19
    for voice in voices[other_cue]: _stop_voice(voice)
    var wanted: AudioStreamPlayer = (
        _create_voice(wanted_cue) if voices[wanted_cue].is_empty() else voices[wanted_cue][0])
    # The original restarts each complete recording; do not add an OGG loop.
    if not _voice_playing(wanted):
        _play_voice(wanted)


func stop_all() -> void:
    for group in voices:
        for voice in group:
            _stop_voice(voice)
    engine_active = false


func consume(commands: Array) -> void:
    for command in commands:
        match command.get("op", ""):
            "play": play(int(command.cue))
            "engine": update_engine(bool(command.active), bool(command.moving))
            "stop": stop_all()


func _process(_delta: float) -> void:
    if not voices.is_empty():
        refresh_engine()
