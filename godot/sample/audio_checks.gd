extends SceneTree

# Run without --headless and with -- --capture-audio to test Godot's actual
# mixer. A muted downstream bus keeps the test inaudible. This proves decoded
# samples and routing, not speakers/headphones or physical device latency.
const AudioBank = preload("res://audio_bank.gd")

# AudioStreamPlayer.stop() queues mixer retirement rather than releasing the
# server's playback references immediately. Keep headless policy assertions
# independent of Dummy driver scheduling. Only playing/start/stop are simulated:
# the inherited bank creates real players, binds the preloaded OGG resources,
# and applies the same routing, gain, priority and reuse policy as production.
class PolicyAudioBank extends "res://audio_bank.gd":
    var playing_ids: Dictionary = {}

    func _voice_playing(voice: AudioStreamPlayer) -> bool:
        return playing_ids.has(voice.get_instance_id())

    func _play_voice(voice: AudioStreamPlayer) -> void:
        playing_ids[voice.get_instance_id()] = true

    func _stop_voice(voice: AudioStreamPlayer) -> void:
        playing_ids.erase(voice.get_instance_id())


var bank: Node
var capture: AudioEffectCapture
var receipt := {"resources": 0, "mixed_cues": {}, "checks": []}


func _initialize() -> void:
    run.call_deferred()


func require(condition: bool, detail: String) -> bool:
    if not condition:
        push_error("Audio contract failed: " + detail)
    return condition


func active_voices(cue: int) -> int:
    var active := 0
    for voice in bank.voices[cue]:
        if bank._voice_playing(voice): active += 1
    return active


func policy_checks() -> bool:
    if not require(bank.voices.size() == AudioBank.CUES.size() and bank.get_child_count() == 0 and
            not bank.highest_priority_playing(), "preloaded recordings start with no mixer players"):
        return false
    var recordings: Array[AudioStream] = bank.streams.duplicate()
    bank.set_enabled(false)
    bank.play(0)
    bank.play(7)
    bank.update_engine(true, true)
    bank.refresh_engine()
    bank.set_volume(0.0)
    if not require(bank.get_child_count() == 0, "disabled effects and engine updates allocate no voices"):
        return false
    bank.stop_all()
    bank.set_volume(1.0)
    bank.set_enabled(true)
    bank.play(-1)
    bank.play(AudioBank.CUES.size())
    if not require(bank.get_child_count() == 0, "invalid cue ordinals allocate no voices"):
        return false
    bank.play(7)
    if not require(bank.voices[7].size() == 1 and active_voices(7) == 1 and
            bank.voices[7][0].stream == recordings[7] and bank.voices[7][0].bus == bank.bus,
            "first effect creates one correctly routed voice from the preloaded recording"):
        return false
    var first: int = bank.voices[7][0].get_instance_id()
    bank.play(0)
    bank.play(12)
    bank.update_engine(true, true)
    if not require(active_voices(0) == 1 and active_voices(7) == 0 and
            bank.voices[12].is_empty() and bank.voices[19].is_empty(),
            "priority interrupts effects and suppresses creation of new effects and engines"):
        return false
    bank.play(20)
    if not require(active_voices(20) == 1 and active_voices(0) == 0,
            "a new priority recording replaces the existing jingle"):
        return false
    bank.stop_all()
    bank.update_engine(true, false)
    if not require(bank.voices[17].size() == 1 and active_voices(17) == 1 and bank.voices[19].is_empty(),
            "idle engine creates only its requested voice"):
        return false
    bank.update_engine(true, true)
    var drive: int = bank.voices[19][0].get_instance_id()
    var engine_nodes: int = bank.get_child_count()
    bank.update_engine(true, true)
    if not require(bank.voices[19].size() == 1 and active_voices(19) == 1 and active_voices(17) == 0 and
            bank.voices[19][0].get_instance_id() == drive and bank.get_child_count() == engine_nodes,
            "moving engine replaces idle and reuses its exclusive player"):
        return false
    bank.update_engine(false, false)
    if not require(active_voices(17) == 0 and active_voices(19) == 0, "inactive engine stops both recordings"):
        return false
    for index in range(20): bank.play(7)
    if not require(bank.voices[7].size() == AudioBank.VOICE_LIMIT and active_voices(7) == AudioBank.VOICE_LIMIT and
            bank.voices[7][-1].volume_db < bank.voices[7][0].volume_db,
            "overlapping effects create at most twelve attenuated players"):
        return false
    var saturated_nodes: int = bank.get_child_count()
    bank._stop_voice(bank.voices[7][3])
    bank._stop_voice(bank.voices[7][5])
    bank.play(7)
    if not require(bank._voice_playing(bank.voices[7][3]) and not bank._voice_playing(bank.voices[7][5]),
            "playback first reuses the earliest available player"):
        return false
    bank.play(7)
    if not require(bank._voice_playing(bank.voices[7][5]) and bank.get_child_count() == saturated_nodes,
            "reusing a bounded pool creates no additional players"):
        return false
    bank.stop_all()
    bank.play(7)
    if not require(active_voices(7) == 1 and bank.voices[7][0].get_instance_id() == first and
            bank.get_child_count() == saturated_nodes, "stopped pools retain and reuse their first player"):
        return false
    for cue in AudioBank.SINGLE:
        bank.stop_all()
        bank.play(cue)
        bank.play(cue)
        if not require(bank.voices[cue].size() == 1 and active_voices(cue) == 1,
                "single-voice bound for " + AudioBank.CUES[cue]):
            return false
    bank.stop_all()
    bank.set_volume(0.0)
    bank.play(8)
    if not require(bank.voices[8].size() == 1 and db_to_linear(bank.voices[8][0].volume_db) == 0.0,
            "a player created at zero volume has zero linear gain"):
        return false
    for cue in range(AudioBank.CUES.size()):
        for voice in bank.voices[cue]:
            if not require(voice.stream == recordings[cue] and voice.bus == bank.bus and
                    db_to_linear(voice.volume_db) == 0.0,
                    "created players reuse preloaded recordings, routing and zero gain"):
                return false
    bank.set_volume(1.0)
    for group in bank.voices:
        for voice in group:
            if not require(is_equal_approx(db_to_linear(voice.volume_db), float(voice.get_meta("cue_gain"))),
                    "volume restore preserves each player's attenuated cue gain"):
                return false
    bank.set_enabled(false)
    var disabled_nodes: int = bank.get_child_count()
    bank.play(6)
    bank.update_engine(true, false)
    bank.refresh_engine()
    if not require(bank.get_child_count() == disabled_nodes and bank.voices[6].is_empty(),
            "disabled playback never expands an existing pool"):
        return false
    for cue in range(AudioBank.CUES.size()):
        if not require(active_voices(cue) == 0, "disabled playback leaves every existing player stopped"):
            return false
    bank.stop_all()
    if bank is PolicyAudioBank:
        for group in bank.voices:
            for voice in group:
                if not require(not voice.playing and not voice.has_stream_playback(),
                        "headless policy never queues native mixer playback"):
                    return false
        receipt.checks.append("simulated-playback")
    receipt.checks.append_array(["lazy-pool", "reuse", "voice-limit", "single", "priority",
        "engine-exclusive", "disabled", "zero-gain", "stream-reuse"])
    return true


func peak() -> float:
    var frames := capture.get_frames_available()
    if frames == 0:
        return 0.0
    var samples := capture.get_buffer(frames)
    var maximum := 0.0
    for sample in samples:
        maximum = maxf(maximum, maxf(absf(sample.x), absf(sample.y)))
    return maximum


func wait_for_silence() -> float:
    # Drain already mixed/ramped samples before testing the new state.
    await create_timer(0.08).timeout
    capture.clear_buffer()
    await create_timer(0.16).timeout
    return peak()


func mixer_checks() -> bool:
    for cue in range(AudioBank.CUES.size()):
        bank.stop_all()
        await create_timer(0.04).timeout
        capture.clear_buffer()
        var existing: int = bank.voices[cue].size()
        if cue in [17, 19]:
            bank.update_engine(true, cue == 19)
        else:
            bank.play(cue)
        if not require(bank.voices[cue].size() == maxi(1, existing) and active_voices(cue) == 1 and
                bank.voices[cue][0].stream == bank.streams[cue],
                "mixer playback creates or reuses a preloaded voice for " + AudioBank.CUES[cue]):
            return false
        var first: int = bank.voices[cue][0].get_instance_id()
        var maximum := 0.0
        for attempt in range(5):
            await create_timer(0.12).timeout
            maximum = maxf(maximum, peak())
            if maximum > 0.00001:
                break
        if not require(maximum > 0.00001, "decoded nonzero samples for " + AudioBank.CUES[cue]):
            return false
        receipt.mixed_cues[AudioBank.CUES[cue]] = maximum
        bank.stop_all()
        if cue in [17, 19]:
            bank.update_engine(true, cue == 19)
        else:
            bank.play(cue)
        if not require(bank.voices[cue].size() == maxi(1, existing) and active_voices(cue) == 1 and
                bank.voices[cue][0].get_instance_id() == first and bank.voices[cue][0].stream == bank.streams[cue],
                "repeated mixer playback reuses its player and preloaded recording"):
            return false
    bank.stop_all()
    bank.play(15)
    bank.play(0)
    if not require(bank.voices[0][0].playing and not bank.voices[15][0].playing,
            "stage-start jingle interrupts existing effects"):
        return false
    bank.play(12)
    bank.update_engine(true, true)
    if not require(not bank.voices[12][0].playing and not bank.voices[19][0].playing,
            "highest-priority jingle suppresses effects and engines"):
        return false
    bank.play(20)
    if not require(bank.voices[20][0].playing and not bank.voices[0][0].playing,
            "respawn jingle replaces an existing priority jingle"):
        return false
    bank.stop_all()
    bank.update_engine(true, false)
    if not require(bank.voices[17][0].playing and not bank.voices[19][0].playing,
            "idle engine is exclusive"):
        return false
    bank.update_engine(true, true)
    if not require(bank.voices[19][0].playing and not bank.voices[17][0].playing,
            "moving engine replaces idle"):
        return false
    bank.update_engine(false, false)
    if not require(not bank.voices[17][0].playing and not bank.voices[19][0].playing,
            "inactive/creating/terminal request stops engine"):
        return false
    bank.stop_all()
    for index in range(20): bank.play(7)
    var active := 0
    for voice in bank.voices[7]:
        if voice.playing: active += 1
    if not require(active == 12 and bank.voices[7][11].volume_db < bank.voices[7][0].volume_db,
            "overlapping impact voices are bounded and attenuated"):
        return false
    bank.stop_all()
    bank.update_engine(true, true)
    bank.set_volume(0.0)
    if not require(await wait_for_silence() <= 0.000001, "zero volume mutes mixed samples"):
        return false
    bank.set_volume(1.0)
    capture.clear_buffer()
    await create_timer(0.16).timeout
    if not require(peak() > 0.00001, "volume restores mixed samples"):
        return false
    bank.stop_all()
    if not require(await wait_for_silence() <= 0.000001, "stop-all leaves no stale engine or effect samples"):
        return false
    bank.set_enabled(false)
    bank.play(0)
    bank.update_engine(true, true)
    if not require(await wait_for_silence() <= 0.000001, "disabled demo/self-test output is silent"):
        return false
    receipt.checks.append_array(["decoded-mixer", "routing", "overlap-limit", "zero-mute", "volume-restore", "stop"])
    return true


func run() -> void:
    var mix := OS.get_cmdline_user_args().has("--capture-audio")
    if OS.get_cmdline_user_args().has("--require-import-only"):
        for cue in AudioBank.CUES:
            if not require(not FileAccess.file_exists("res://resources/sounds/%s.ogg" % cue),
                    "packaged audio has no raw source or source-project fallback: " + cue):
                quit(1)
                return
        receipt.checks.append("import-only")
    if mix and DisplayServer.get_name() == "headless":
        push_error("The mixer acceptance check requires a real audio driver; omit --headless.")
        quit(2)
        return
    bank = AudioBank.new() if mix else PolicyAudioBank.new()
    bank.enabled = mix
    bank.volume = 1.0
    if mix:
        AudioServer.add_bus()
        var sink := AudioServer.bus_count - 1
        AudioServer.set_bus_name(sink, &"TanksTestSilentOutput")
        AudioServer.set_bus_mute(sink, true)
        AudioServer.add_bus()
        var test_bus := AudioServer.bus_count - 1
        AudioServer.set_bus_name(test_bus, &"TanksTestCapture")
        AudioServer.set_bus_send(test_bus, &"TanksTestSilentOutput")
        capture = AudioEffectCapture.new()
        capture.buffer_length = 1.0
        AudioServer.add_bus_effect(test_bus, capture)
        bank.bus = &"TanksTestCapture"
    root.add_child(bank)
    for index in range(AudioBank.CUES.size()):
        var stream: AudioStream = bank.streams[index]
        if not require(stream != null and stream.get_length() > 0.0 and is_finite(stream.get_length()),
                "valid recording " + AudioBank.CUES[index]):
            quit(1)
            return
        receipt.resources += 1
    receipt.checks.append("resources")
    if not policy_checks():
        quit(1)
        return
    bank.set_enabled(mix)
    if mix and not await mixer_checks():
        quit(1)
        return
    bank.stop_all()
    if mix:
        # The real mixer consumes stop requests asynchronously; allow its
        # bounded fade-out retirement before deleting the player's nodes.
        await create_timer(0.12).timeout
    bank.queue_free()
    bank = null
    # process_frame resumes before the rest of that frame is complete. Wait
    # through a full frame after deferred player deletion before quitting.
    await process_frame
    await process_frame
    print("TANKS_AUDIO_CHECKS_PASSED " + JSON.stringify(receipt))
    quit(0)
