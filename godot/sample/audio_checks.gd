extends SceneTree

# Run without --headless and with -- --capture-audio to test Godot's actual
# mixer. A muted downstream bus keeps the test inaudible. This proves decoded
# samples and routing, not speakers/headphones or physical device latency.
const AudioBank = preload("res://audio_bank.gd")
var bank: Node
var capture: AudioEffectCapture
var receipt := {"resources": 0, "mixed_cues": {}, "checks": []}


func _initialize() -> void:
    run.call_deferred()


func require(condition: bool, detail: String) -> bool:
    if not condition:
        push_error("Audio contract failed: " + detail)
    return condition


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
        if cue in [17, 19]:
            bank.update_engine(true, cue == 19)
        else:
            bank.play(cue)
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
    receipt.checks = ["resources", "decoded-mixer", "routing", "priority", "engine-exclusive",
        "overlap-limit", "zero-mute", "volume-restore", "stop", "disabled"]
    return true


func run() -> void:
    var mix := OS.get_cmdline_user_args().has("--capture-audio")
    if mix and DisplayServer.get_name() == "headless":
        push_error("The mixer acceptance check requires a real audio driver; omit --headless.")
        quit(2)
        return
    bank = AudioBank.new()
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
    receipt.checks = ["resources"]
    bank.set_volume(0.0)
    for group in bank.voices:
        for voice in group:
            if not require(db_to_linear(voice.volume_db) == 0.0, "zero volume has zero linear gain"):
                quit(1)
                return
    bank.set_volume(1.0)
    if mix and not await mixer_checks():
        quit(1)
        return
    bank.stop_all()
    bank.queue_free()
    await process_frame
    print("TANKS_AUDIO_CHECKS_PASSED " + JSON.stringify(receipt))
    quit(0)
