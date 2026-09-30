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
        var group: Array[AudioStreamPlayer] = []
        for voice_index in range(1 if index in SINGLE else VOICE_LIMIT):
            var voice := AudioStreamPlayer.new()
            voice.stream = stream
            voice.bus = bus
            voice.volume_db = linear_to_db(VOLUMES[index] * volume)
            voice.set_meta("cue_gain", VOLUMES[index])
            add_child(voice)
            group.append(voice)
        voices.append(group)


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
        if voices[cue][0].playing:
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
        if voice.playing:
            active += 1
        elif available == null:
            available = voice
    if available == null:
        return
    var gain: float = VOLUMES[cue] * pow(OVERLAP[cue], active)
    available.set_meta("cue_gain", gain)
    available.volume_db = linear_to_db(gain * volume)
    available.play()


func update_engine(active: bool, moving: bool) -> void:
    engine_active = active
    engine_moving = moving
    refresh_engine()


func refresh_engine() -> void:
    var idle: AudioStreamPlayer = voices[17][0]
    var drive: AudioStreamPlayer = voices[19][0]
    if not enabled or not engine_active or highest_priority_playing():
        idle.stop()
        drive.stop()
        return
    var wanted := drive if engine_moving else idle
    var other := idle if engine_moving else drive
    other.stop()
    # The original restarts each complete recording; do not add an OGG loop.
    if not wanted.playing:
        wanted.play()


func stop_all() -> void:
    for group in voices:
        for voice in group:
            voice.stop()
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
