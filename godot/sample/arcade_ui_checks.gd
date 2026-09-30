extends RefCounted
## Real Control layout in independently sized canvases. Status stress fixtures
## exercise presentation only; the caller supplies and preserves native state.
const Frontend = preload("res://frontend.gd")
const DIMENSIONS := [Vector2i(1280,720),Vector2i(1280,960),Vector2i(1920,720)]
var failures: Array[String] = []
var receipts: Array = []
var samples := 0


func expect(condition: bool, detail: String) -> bool:
    if not condition:
        failures.append(detail)
        if failures.size() <= 20: push_error("Arcade UI contract: "+detail)
    return condition


func settle(parent: Node) -> void:
    for frame in 3: await parent.get_tree().process_frame


func player_labels(frontend: CanvasLayer, slot: int) -> Array:
    var fields: Dictionary = frontend.player_hud[slot]
    return [fields.identity,fields.lives,fields.vehicle,fields.tier,fields.score,fields.hp,
        fields.streak,fields.state]


func visible_text(labels: Array) -> String:
    var parts: Array[String] = []
    for item in labels:
        if item != null and item.is_visible_in_tree(): parts.append(item.text)
    return " ".join(parts)


func permanent_controls(node: Node, transient: Node) -> Array[Control]:
    var result: Array[Control] = []
    if node == transient: return result
    if node is Control and node.is_visible_in_tree() and (node is Panel or node is PanelContainer or
            (node is Label and not node.text.is_empty())):
        result.append(node)
    for child in node.get_children(): result.append_array(permanent_controls(child,transient))
    return result


func check_label(item: Label, viewport_rect: Rect2, label: String) -> void:
    if item == null or not item.is_visible_in_tree() or item.text.is_empty(): return
    var rect := item.get_global_rect()
    expect(viewport_rect.grow(.5).encloses(rect),label+": text control leaves its canvas")
    var font := item.get_theme_font("font")
    var font_size := item.get_theme_font_size("font_size")
    # Measure glyphs, not just the Label's rectangle: clipping/ellipsis must
    # not turn an overflowing score or status into a false layout pass.
    for line in item.text.split("\n"):
        expect(font.get_string_size(line,HORIZONTAL_ALIGNMENT_LEFT,-1,font_size).x <= rect.size.x+.5,
            label+": actual text width exceeds available space: "+line)
    var lines := item.text.split("\n").size()
    var height := font.get_height(font_size)*lines+item.get_theme_constant("line_spacing")*maxi(0,lines-1)
    expect(height <= rect.size.y+.5,label+": actual line height is clipped")
    expect(item.visible_characters < 0 and item.lines_skipped == 0 and
        (item.max_lines_visible < 0 or item.max_lines_visible >= lines),label+": information is deliberately truncated")


func check_hud(frontend: CanvasLayer, value: Dictionary, dimensions: Vector2i, label: String) -> void:
    var bounds := Rect2(Vector2.ZERO,Vector2(dimensions))
    var center := Rect2(dimensions.x*.30,0,dimensions.x*.40,minf(180,dimensions.y*.30))
    var occluders: Array[Rect2] = frontend.hud_occluder_rects()
    expect(not occluders.is_empty(),label+": no real HUD occluders reported")
    for rect in occluders:
        expect(bounds.grow(.5).encloses(rect),label+": HUD leaves the canvas")
        expect(not center.intersects(rect),label+": permanent panel covers upper-center battlefield")
    var cards: Array = [frontend.p1_card,frontend.p2_card]
    var text_items: Array = [frontend.stage_label,frontend.help]
    for slot in 2:
        var labels := player_labels(frontend,slot)
        text_items.append_array(labels)
        if slot >= value.players.size() or int(value.get("player_count",value.players.size())) <= slot:
            expect(not cards[slot].is_visible_in_tree() and visible_text(labels).is_empty(),
                label+": solo HUD shows a nonexistent second player")
            continue
        var player: Dictionary = value.players[slot]
        var text := visible_text(labels)
        expect(cards[slot].is_visible_in_tree() and text.contains("P%d"%(slot+1)),label+": player identity missing")
        expect(text.contains(str(player.vehicle_name)) and text.contains("TIER %d/4"%(int(player.level)+1)),
            label+": vehicle identity or upgrade level missing")
        expect(text.replace(" ","").contains("HP%d/%d"%[int(player.hp),int(player.max_hp)]) and
            text.contains("LIVES %d"%int(player.lives)) and text.contains(str(int(player.score))),
            label+": HP, remaining lives or exact score missing")
        expect(text.contains(" AI") == (slot == 1 and bool(value.get("ai_p2",false))),label+": human/AI crew label wrong")
        if not player.active:
            expect(text.contains("RESPAWNING" if int(player.lives) > 0 else "OUT"),label+": inactive player status missing")
        else:
            for status in [["SHIELD",float(player.get("shield",0))>0],["BOAT",bool(player.get("boat",false))]]:
                expect(text.contains(status[0]) == status[1],label+": stale or missing "+status[0])
            if int(player.get("streak",0))>0:
                expect(text.contains("STREAK  x%d"%int(player.streak)),label+": streak indicator missing")
        var card: Rect2 = cards[slot].get_global_rect()
        # Card geometry can shrink without changing the existing camera band.
        expect(is_equal_approx(frontend.hud_root.get_global_rect().size.y,74.0),
            label+": status layout changed the camera's existing HUD reservation")
        var fields: Dictionary = frontend.player_hud[slot]
        var optional: bool = fields.streak.is_visible_in_tree() or fields.state.is_visible_in_tree()
        expect(card.position.y == 7 and card.size.x == 320 and
            is_equal_approx(card.size.y,102.0 if optional else 80.0),
            label+": compact status panel exceeds its bounded corner footprint")
        expect(fields.lives.text == "LIVES %d"%maxi(0,int(player.lives)),
            label+": remaining lives differ from the native value")
        expect(fields.vehicle.text == str(player.vehicle_name) and fields.hp.text == "HP  %d / %d"%[
            maxi(0,int(player.hp)),int(player.max_hp)],label+": full model name or HP changed")
        expect(fields.tier.text == "TIER %d/4  %s"%[int(player.level)+1,Frontend.TIERS[int(player.level)]] and
            fields.score.text == "SCORE %d"%int(player.score),label+": tier name or exact unpadded score missing")
        expect(not text.contains("STAGE") and not text.contains("ENEMY") and not text.contains("BASE STEEL"),
            label+": shared battlefield information is duplicated in player cards")
        for pair in [["identity","lives"],["vehicle","hp"],["tier","score"],["streak","state"]]:
            var first: Label = fields[pair[0]]
            var second: Label = fields[pair[1]]
            if first.is_visible_in_tree() and second.is_visible_in_tree():
                expect(first.get_global_rect().end.x+5.5 <= second.get_global_rect().position.x,
                    label+": paired information columns overlap: "+str(pair))
        for index in labels.size():
            var item: Label = labels[index]
            if not item.is_visible_in_tree(): continue
            for later in range(index+1,labels.size()):
                var other: Label = labels[later]
                if other.is_visible_in_tree():
                    expect(not item.get_global_rect().intersects(other.get_global_rect()),
                        label+": player text rectangles overlap")
            expect(item.get_theme_font_size("font_size") >= (12 if item in [fields.tier,fields.score] else 14),
                label+": compact layout shrank essential text below its legibility floor")
        var hp_color := Color("878e94") if int(player.hp)<=0 else (Color("62e881") if int(player.hp)>=int(player.max_hp)
            else (Color("ffcf53") if int(player.hp)*2>=int(player.max_hp) else Color("ff634b")))
        expect(fields.hp.get_theme_color("font_color").is_equal_approx(hp_color),label+": HP text lacks the native health color")
        expect(cards[slot] is Frontend.CombatPlate and cards[slot].get_theme_stylebox("panel") is StyleBoxEmpty,
            label+": bounded arcade HUD artwork is absent")
        expect(is_equal_approx(cards[slot].hp_ratio,clampf(float(player.hp)/maxi(1,int(player.max_hp)),0,1)),
            label+": health segments disagree with the native HP number")
        for item in labels:
            if item != null and item.is_visible_in_tree() and not item.text.is_empty():
                expect(card.grow(.5).encloses(item.get_global_rect()),label+": player information escapes its card")
        expect(fields.hp.get_theme_font_size("font_size") >= 16,label+": primary HP text too small")
    if int(value.get("player_count",value.players.size())) == 1:
        expect(not frontend.help.text.contains("P2"),label+": solo help advertises an inactive fixed P2 slot")
    elif value.get("ai_p2",false):
        expect(frontend.help.text.contains("P2 AI TEAMMATE") and not frontend.help.text.contains("P2 PAD"),
            label+": AI teammate help incorrectly requests a second human controller")
    else:
        expect(frontend.help.text.contains("P2: WASD / F") or frontend.help.text.contains("P2: PAD 2"),label+": two-human controls missing")
    var mission: String = frontend.stage_label.text
    expect(mission.contains("STAGE %02d"%int(value.stage)) and mission.contains("ENEMY %02d"%int(value.enemies_left)),
        label+": stage/enemy totals missing")
    var base_status := "BASE STEEL %.1fs"%float(value.base_steel_remaining) if float(value.get("base_steel_remaining",0))>0 else \
        ("HEADQUARTERS" if value.get("base_alive",true) else "BASE LOST")
    expect(mission.contains(base_status),label+": base status or protection countdown missing")
    for item in text_items:
        if item == null or not item.is_visible_in_tree() or item.text.is_empty(): continue
        check_label(item,bounds,label)
        expect(not center.intersects(item.get_global_rect()),label+": permanent text covers upper-center battlefield")
    for control in permanent_controls(frontend.root,frontend.modal_message):
        expect(not center.intersects(control.get_global_rect()),
            label+": another permanent panel or label covers upper-center battlefield")
    # Mission plaque deliberately occupies the radar's header, never its map.
    expect(frontend.mission_card.get_global_rect().end.y <= frontend.radar.get_global_rect().position.y+50.5,
        label+": mission text covers radar terrain or actors")
    expect(frontend.help.get_global_rect().end.x+15.5 <= frontend.radar.get_global_rect().position.x,
        label+": control hints enter the radar footprint")
    if frontend.p2_card.is_visible_in_tree():
        expect(not frontend.p1_card.get_global_rect().intersects(frontend.p2_card.get_global_rect()),label+": player cards overlap")
    samples += 1


func reachable(control: Control, viewport: SubViewport, label: String) -> void:
    var target: Control = control.get_line_edit() if control is SpinBox else control
    target.grab_focus()
    # Exercise production follow_focus, without manually scrolling the target
    # into view and thereby hiding a broken keyboard/controller access path.
    await settle(viewport)
    var visible := Rect2(Vector2.ZERO,Vector2(viewport.size))
    var parent := control.get_parent()
    while parent != null:
        if parent is Control and parent.clip_contents: visible = visible.intersection(parent.get_global_rect())
        parent = parent.get_parent()
    expect(control.is_visible_in_tree() and visible.grow(.5).encloses(control.get_global_rect()),label+": menu control cannot scroll fully into view")
    expect(viewport.gui_get_focus_owner() == target,label+": menu control cannot receive keyboard focus")


func activate(button: Button, viewport: SubViewport, label: String) -> void:
    await reachable(button,viewport,label)
    for pressed in [true,false]:
        var event := InputEventAction.new()
        event.action = &"ui_accept"
        event.pressed = pressed
        event.strength = 1.0 if pressed else 0.0
        viewport.push_input(event,true)
        await settle(viewport)


func menu_key(viewport: SubViewport, key: Key, coarse: bool = false) -> void:
    for pressed in [true,false]:
        var event := InputEventKey.new()
        event.keycode = key
        event.physical_keycode = key
        event.shift_pressed = coarse
        event.pressed = pressed
        viewport.push_input(event,true)
        await settle(viewport)


func check_rows(frontend: CanvasLayer, viewport: SubViewport, keys: Array) -> void:
    var previous := Rect2()
    var bounds := Rect2(Vector2.ZERO,Vector2(viewport.size))
    for key in keys:
        var control: Control = frontend.controls[key]
        await reachable(control,viewport,str(key))
        var rect := control.get_global_rect()
        expect(bounds.grow(.5).encloses(rect),"menu row leaves canvas: "+str(key))
        if previous.has_area(): expect(not previous.intersects(rect),"menu rows overlap: "+str(key))
        previous = rect


func check_menu(frontend: CanvasLayer, viewport: SubViewport) -> void:
    expect(int(Frontend.DEFAULTS.players) == 1 and not Frontend.DEFAULTS.ai_p2,
        "a fresh profile no longer matches original solo default")
    frontend.configure(Frontend.DEFAULTS.duplicate(),false,.75)
    frontend.show_menu()
    await settle(viewport)
    var bounds := Rect2(Vector2.ZERO,Vector2(viewport.size))
    expect(bounds.grow(.5).encloses(frontend.menu.get_global_rect()),"setup exceeds canvas "+str(viewport.size))
    expect(frontend.menu_page == "setup" and viewport.gui_get_focus_owner() == frontend.controls.deploy,
        "setup does not focus its PLAYERS start row")
    expect(not frontend.controls.row_nation_p2.is_visible_in_tree(),"solo setup displays a P2 nation row")
    await menu_key(viewport,KEY_UP)
    expect(viewport.gui_get_focus_owner() == frontend.controls.advanced_toggle,"setup up does not wrap to final row")
    await menu_key(viewport,KEY_DOWN)
    expect(viewport.gui_get_focus_owner() == frontend.controls.deploy,"setup down does not wrap to PLAYERS")
    for expected in [[2,false],[2,true],[1,false]]:
        await menu_key(viewport,KEY_RIGHT)
        var configured: Dictionary = frontend.read_settings()
        expect(int(configured.players) == expected[0] and bool(configured.ai_p2) == expected[1],
            "PLAYERS right key does not cycle solo/two-human/AI")
    await menu_key(viewport,KEY_2)
    expect(int(frontend.read_settings().players) == 2 and not frontend.read_settings().ai_p2,
        "2 shortcut does not select two human players")
    await check_rows(frontend,viewport,["row_mode","row_stage","row_lives","row_nation_p1","row_nation_p2",
        "network_toggle","advanced_toggle"])
    frontend.controls.stage.value = 35
    await reachable(frontend.controls.row_stage,viewport,"stage")
    await menu_key(viewport,KEY_RIGHT)
    expect(int(frontend.read_settings().stage) == 1,"stage increment does not wrap35 to1")
    await menu_key(viewport,KEY_RIGHT,true)
    expect(int(frontend.read_settings().stage) == 11,"Shift stage adjustment does not advance10")
    await menu_key(viewport,KEY_LEFT,true)
    expect(int(frontend.read_settings().stage) == 1,"Shift stage decrement is asymmetric")
    frontend.controls.lives.value = 95
    await reachable(frontend.controls.row_lives,viewport,"lives")
    await menu_key(viewport,KEY_RIGHT,true)
    expect(int(frontend.read_settings().lives) == 99,"coarse lives adjustment exceeds99")
    frontend.controls.lives.value = 1
    await menu_key(viewport,KEY_LEFT)
    expect(int(frontend.read_settings().lives) == 1,"lives decreases below1")
    frontend.controls.nation_p1.select(0)
    await reachable(frontend.controls.row_nation_p1,viewport,"P1 nation")
    await menu_key(viewport,KEY_LEFT)
    expect(int(frontend.read_settings().nation_p1) == 2,"nation left key does not wrap")
    await activate(frontend.controls.advanced_toggle,viewport,"Advanced entry")
    expect(frontend.menu_page == "advanced" and frontend.advanced.is_visible_in_tree() and
        not frontend.network_box.is_visible_in_tree(),"Advanced is not an independent settings page")
    await check_rows(frontend,viewport,["row_max_hp","row_enemy_speed","row_enemy_fire","row_enemy_spawn",
        "row_camera_yaw","row_camera_elevation","row_pixel","row_volume","row_back"])
    frontend.controls.enemy_fire.value = 0
    await reachable(frontend.controls.row_enemy_fire,viewport,"enemy fire")
    await menu_key(viewport,KEY_RIGHT)
    expect(int(frontend.read_settings().enemy_fire) == 5,"advanced right key does not use native5percent step")
    await reachable(frontend.controls.row_pixel,viewport,"Pixel Style")
    await menu_key(viewport,KEY_ENTER)
    expect(frontend.controls.pixel.button_pressed,"Pixel Enter does not toggle the presentation option")
    await menu_key(viewport,KEY_R)
    for key in ["max_hp","enemy_speed","enemy_fire","enemy_spawn","camera_yaw","camera_elevation"]:
        expect(frontend.read_settings()[key] == Frontend.DEFAULTS[key],"R does not reset advanced "+key)
    expect(not frontend.controls.pixel.button_pressed and is_equal_approx(frontend.controls.volume.value,.75),
        "advanced reset changes volume or leaves Pixel on")
    await menu_key(viewport,KEY_ESCAPE)
    expect(frontend.menu_page == "setup" and not frontend.advanced.is_visible_in_tree(),"Advanced Esc does not return to setup")
    await activate(frontend.controls.network_toggle,viewport,"LAN entry")
    expect(frontend.menu_page == "network" and frontend.network_box.is_visible_in_tree() and
        not frontend.advanced.is_visible_in_tree(),"LAN is not an independent page")
    await check_rows(frontend,viewport,["network_host","network_join","row_network_ip","row_network_port","network_back"])
    await reachable(frontend.controls.row_network_ip,viewport,"LAN address row")
    await menu_key(viewport,KEY_ENTER)
    expect(viewport.gui_get_focus_owner() == frontend.network_ip,"LAN address cannot enter text edit mode")
    await menu_key(viewport,KEY_ESCAPE)
    expect(frontend.menu_page == "network" and viewport.gui_get_focus_owner() != frontend.network_ip,
        "Esc in LAN text edit exits the page instead of cancelling edit")
    await check_network_requests(frontend,viewport)
    await menu_key(viewport,KEY_ESCAPE)
    expect(frontend.menu_page == "setup","LAN Esc does not return to setup")
    await menu_key(viewport,KEY_1)
    expect(int(frontend.read_settings().players) == 1 and not frontend.controls.row_nation_p2.is_visible_in_tree(),
        "1 shortcut leaves a phantom P2 setup row")
    var starts := [0]
    var record_start := func(_settings: Dictionary): starts[0] += 1
    frontend.start_requested.connect(record_start)
    await reachable(frontend.controls.row_stage,viewport,"start from selected setup row")
    await menu_key(viewport,KEY_ENTER)
    expect(starts[0] == 1,"Enter on an ordinary setup row must start exactly once")
    frontend.start_requested.disconnect(record_start)
    var quits := [0]
    var count_quit := func(): quits[0] += 1
    frontend.quit_requested.connect(count_quit)
    await menu_key(viewport,KEY_ESCAPE)
    expect(quits[0] == 1,"setup Esc does not request exactly one quit")
    frontend.quit_requested.disconnect(count_quit)


func check_network_requests(frontend: CanvasLayer, viewport: SubViewport) -> void:
    # Observe production UI signals only; this isolated canvas never creates a
    # socket. Native invalid-IP/handshake behavior belongs to the bridge tests.
    var requests: Array = []
    var record := func(host: bool,address: String,port: int,configuration: Dictionary):
        requests.append({"host":host,"address":address,"port":port,"settings":configuration.duplicate(true)})
    frontend.network_requested.connect(record)
    frontend.network_ip.text = " 192.168.2.8:43123 "
    await reachable(frontend.controls.network_join,viewport,"join IPv4:port")
    await menu_key(viewport,KEY_ENTER)
    expect(requests.size() == 1 and requests[0].address == "192.168.2.8" and requests[0].port == 43123 and
        not requests[0].host and requests[0].settings.players == 2 and not requests[0].settings.ai_p2 and
        requests[0].settings.nation_p1 == frontend.controls.nation_p1.selected,
        "IPv4:port Join does not preserve selected local nation or human two-player mode")
    for invalid in ["127.0.0.1:0","127.0.0.1:65536","127.0.0.1:-1"]:
        frontend.network_ip.text = invalid
        await menu_key(viewport,KEY_ENTER)
        expect(requests.size() == 1 and not frontend.status_label.text.is_empty(),"invalid explicit LAN port emits a network request")
    frontend.network_ip.text = "127.0.0.1"
    frontend.network_port.value = 1
    await menu_key(viewport,KEY_ENTER)
    expect(requests.size() == 2 and requests[1].port == 1,"valid lowest explicit port rejected")
    frontend.network_port.value = 65535
    await reachable(frontend.controls.network_host,viewport,"host maximum port")
    await menu_key(viewport,KEY_ENTER)
    expect(requests.size() == 3 and requests[2].host and requests[2].port == 65535,"valid highest host port rejected")
    frontend.network_ip.text = ""
    await reachable(frontend.controls.network_join,viewport,"join empty address")
    await menu_key(viewport,KEY_ENTER)
    expect(requests.size() == 3 and viewport.gui_get_focus_owner() == frontend.network_ip,
        "empty Join should enter address edit without opening a connection")
    frontend.network_ip.text = "10.0.0.9:43210"
    await menu_key(viewport,KEY_ENTER)
    expect(requests.size() == 4 and requests[3].address == "10.0.0.9" and requests[3].port == 43210,
        "address edit Enter does not submit exactly one normalized endpoint")
    var cancels := [0]
    var count_cancel := func(): cancels[0] += 1
    frontend.network_cancel_requested.connect(count_cancel)
    frontend.set_network_pending(true,false)
    await settle(viewport)
    expect(not frontend.network_box.is_visible_in_tree() and frontend.network_wait.is_visible_in_tree(),
        "joining still permits editing settings under the waiting status")
    await menu_key(viewport,KEY_ENTER)
    await menu_key(viewport,KEY_DOWN)
    expect(requests.size() == 4,"waiting Enter starts a second LAN attempt")
    await menu_key(viewport,KEY_ESCAPE)
    expect(cancels[0] == 1,"waiting Esc does not cancel the active connection once")
    frontend.network_cancel_requested.disconnect(count_cancel)
    frontend.network_requested.disconnect(record)
    frontend.set_network_pending(false)
    await activate(frontend.controls.network_toggle,viewport,"return to LAN")
    await activate(frontend.controls.network_back,viewport,"LAN back action")
    expect(frontend.menu_page == "setup","LAN Back action does not return to setup")
    await activate(frontend.controls.network_toggle,viewport,"LAN Esc preparation")


func check_first_battle_layout(frontend: CanvasLayer, viewport: SubViewport, fixture: Dictionary) -> void:
    # Production starts in a hidden HUD. First battle refresh happens before
    # its PanelContainer has received a visible-frame layout pass. Setting a
    # Label's whole position at that moment must not freeze an obsolete width.
    frontend.configure(fixture.settings,false,.75)
    frontend.show_battle()
    frontend.refresh(fixture.snapshot,false,0)
    await settle(viewport)
    for slot in int(fixture.snapshot.player_count):
        var fields: Dictionary = frontend.player_hud[slot]
        var card: Control = frontend.p1_card if slot == 0 else frontend.p2_card
        for key in ["streak","state"]:
            var item: Label = fields[key]
            expect(is_equal_approx(item.offset_left,7.0 if key == "streak" else 189.0) and
                is_equal_approx(item.offset_right,181.0 if key == "streak" else 313.0),
                "first battle refresh changes optional-column horizontal anchors: "+key)
            if item.is_visible_in_tree():
                expect(card.get_global_rect().grow(.5).encloses(item.get_global_rect()),
                    "first battle optional row extends beyond its corner panel: "+key)


func check_health_transitions(frontend: CanvasLayer, viewport: SubViewport, fixture: Dictionary) -> void:
    var value: Dictionary = fixture.duplicate(true)
    value.player_count = 1
    value.intro = false
    value.base_steel = false
    value.base_steel_remaining = 0.0
    var player: Dictionary = value.players[0]
    player.max_hp = 6
    player.lives = 3
    player.streak = 0
    player.shield = 0.0
    player.boat = false
    for sample in [[6,"62e881"],[3,"ffcf53"],[1,"ff634b"],[0,"878e94"],[6,"62e881"]]:
        player.hp = sample[0]
        player.active = int(sample[0])>0
        frontend.refresh(value,false,0)
        await settle(viewport)
        var fields: Dictionary = frontend.player_hud[0]
        expect(fields.hp.get_theme_color("font_color").is_equal_approx(Color(sample[1])),
            "HP text fails full/half/critical/dead/recovered transition")
        expect(not fields.streak.is_visible_in_tree() and
            fields.state.is_visible_in_tree() == (not player.active),
            "optional status row retains expired streak or protection")
        expect(is_equal_approx(frontend.p1_card.size.y,80.0 if player.active else 102.0),
            "corner panel does not shrink after optional statuses expire")


func check_vehicle_columns(frontend: CanvasLayer, viewport: SubViewport, fixture: Dictionary) -> void:
    var value: Dictionary = fixture.duplicate(true)
    value.player_count = 1
    value.intro = false
    var player: Dictionary = value.players[0]
    player.lives = 99
    player.score = 2147483647
    player.hp = 6
    player.max_hp = 6
    player.active = true
    player.streak = 2147483647
    player.shield = 5.0
    player.boat = true
    var bounds := Rect2(Vector2.ZERO,Vector2(viewport.size))
    for nation in 3:
        player.nation = nation
        for tier in 4:
            player.level = tier
            player.vehicle_name = Frontend.TECH[nation][tier]
            frontend.refresh(value,false,0)
            await settle(viewport)
            var fields: Dictionary = frontend.player_hud[0]
            for item in player_labels(frontend,0):
                check_label(item,bounds,"complete vehicle roster / maximum score / maximum streak")
                expect(frontend.p1_card.get_global_rect().grow(.5).encloses(item.get_global_rect()),
                    "complete vehicle roster escapes its compact card")
            expect(fields.vehicle.text == player.vehicle_name and
                fields.vehicle.get_global_rect().end.x+5.5 <= fields.hp.get_global_rect().position.x,
                "full vehicle name overlaps primary HP")


func check_feedback_and_hints(frontend: CanvasLayer, viewport: SubViewport, fixture: Dictionary) -> void:
    # Explicit presentation fixtures: these assertions test event delivery,
    # lifecycle and input semantics, not a claim that synthetic combat occurred.
    var value: Dictionary = fixture.duplicate(true)
    value.player_count = 1
    value.intro = false
    value.paused = false
    value.tick = 100
    value.events = []
    value.players[0].active = true
    value.players[0].level = 0
    frontend.show_battle()
    frontend.refresh(value,false,0,{},true)
    value.tick += 1
    value.players[0].hp = 2
    value.events = [{"type":"TankDamaged","target_player":0}]
    var source := JSON.stringify(value)
    frontend.refresh(value,false,.016)
    expect(JSON.stringify(value)==source,"HUD feedback mutates its presentation snapshot")
    expect(frontend.feedback[0].kind=="damage" and frontend.feedback_labels[0].text=="HIT!",
        "native-style damage event has no localized hit feedback")
    frontend.refresh(value,false,.016)
    expect(float(frontend.feedback[0].age)>.0,"same-tick snapshot replays damage feedback")
    value.paused = true
    var age := float(frontend.feedback[0].age)
    for index in 3: frontend.refresh(value,false,.5)
    expect(float(frontend.feedback[0].age)==age,"paused damage feedback clock advances")
    value.paused = false
    value.events = []
    for index in 40:
        value.tick += 1
        frontend.refresh(value,false,.05)
    expect(frontend.feedback[0].is_empty() and not frontend.feedback_labels[0].visible,
        "damage feedback does not expire")
    value.tick += 1
    value.players[0].level = 1
    value.events = [{"type":"BonusCollected","source_player":0,"bonus_type":5}]
    frontend.refresh(value,false,.016)
    expect(frontend.feedback[0].kind=="upgrade" and frontend.feedback_labels[0].text=="UPGRADE!  TIER 2",
        "real upgrade transaction is indistinguishable from an ordinary pickup")
    value.tick += 1
    value.events = [{"type":"BonusCollected","source_player":0,"bonus_type":7}]
    frontend.feedback.assign([{},{}])
    frontend.refresh(value,false,.016)
    expect(frontend.feedback[0].kind=="pickup" and frontend.feedback_labels[0].text=="BOAT",
        "ordinary pickup incorrectly reports an upgrade")
    value.tick += 1
    value.events = [{"type":"BonusCollected","source_player":0,"bonus_type":5}]
    frontend.feedback.assign([{},{}])
    frontend.refresh(value,false,.016)
    expect(frontend.feedback[0].kind=="pickup","capped star without a level change fakes an upgrade")
    frontend.refresh(value,false,0,{},true)
    expect(frontend.feedback[0].is_empty(),"same-tick restart replays retained events")
    value.tick += 1
    value.players[0].active = false
    value.players[0].hp = 0
    value.events = []
    frontend.refresh(value,false,.016)
    expect(frontend.feedback[0].is_empty(),"death without a native damage event fakes feedback")
    value.tick += 1
    value.players[0].active = true
    value.players[0].hp = 3
    frontend.refresh(value,false,.016)
    expect(frontend.feedback[0].is_empty(),"respawn fakes a repair feedback")
    value.tick += 1
    value.stage = int(value.stage)+1
    value.events = [{"type":"BonusCollected","source_player":0,"bonus_type":5}]
    frontend.refresh(value,false,.016)
    expect(frontend.feedback[0].is_empty(),"stage entry replays retained events")
    if value.players.size()<2:
        value.players.append(value.players[0].duplicate(true))
        value.players[1].id = 1
    value.player_count = 2
    value.players[1].active = true
    value.tick += 1
    value.events = []
    frontend.refresh(value,false,0,{},true)
    value.tick += 3
    value.events = [{"type":"BonusCollected","source_player":0,"bonus_type":7},
        {"type":"TankDamaged","target_player":1},
        {"type":"BonusCollected","source_player":1,"bonus_type":0}]
    frontend.refresh(value,false,.016,{"role":"host","status":"CONNECTED"})
    expect(frontend.feedback[0].kind=="pickup" and frontend.feedback[1].kind=="damage",
        "batched LAN events lose player ownership or overwrite a fresh hit with a pickup")
    expect(not frontend.human_p2_input,"LAN local keyboard advertises unused WASD player")
    expect(frontend.help.text.contains("LAN P1 HOST"),"LAN footer hides the local player/host identity")
    frontend.refresh(value,false,0,{"role":"guest","local_player":1,"waiting":true,"status":"P2 GUEST"})
    expect(frontend.help.text.contains("LAN P2 GUEST WAIT") and frontend.restart_button.disabled,
        "LAN guest prompt loses local identity, wait state or host-only restart")
    value.player_count = 1
    value.events = []
    frontend.refresh(value,false,0,{},true)
    var key := InputEventKey.new()
    key.keycode = KEY_SPACE
    key.pressed = true
    frontend.note_input(key)
    var pad := InputEventJoypadButton.new()
    pad.button_index = JOY_BUTTON_A
    pad.pressed = true
    frontend.note_input(pad,false)
    expect(frontend.input_device=="keyboard","unassigned pad steals control hints")
    frontend.note_input(pad)
    frontend.refresh(value,false,0)
    expect(frontend.help.text.contains("STICK") and not frontend.help.text.contains("ARROWS"),
        "valid controller input fails to select concise pad hints")
    key.pressed = false
    frontend.note_input(key)
    expect(frontend.input_device=="gamepad","keyboard release steals controller hint")
    key.pressed = true
    frontend.note_input(key)
    var axis := InputEventJoypadMotion.new()
    axis.axis = JOY_AXIS_LEFT_X
    axis.axis_value = .15
    frontend.note_input(axis)
    expect(frontend.input_device=="keyboard","stick dead zone steals keyboard hint")
    axis.axis_value = .75
    frontend.note_input(axis)
    expect(frontend.input_device=="gamepad","valid stick input fails to select pad hints")
    frontend.note_input(key)
    axis.axis_value = .76
    frontend.note_input(axis)
    expect(frontend.input_device=="keyboard","stationary held-stick noise steals keyboard hints")
    axis.axis = JOY_AXIS_TRIGGER_RIGHT
    axis.axis_value = .75
    frontend.note_input(axis)
    expect(frontend.input_device=="gamepad","pressed trigger fails to select controller hint")
    frontend.note_input(key)
    axis.axis_value = .76
    frontend.note_input(axis)
    expect(frontend.input_device=="keyboard","held-trigger noise steals keyboard hint")
    pad.button_index = JOY_BUTTON_Y
    frontend.note_input(pad)
    expect(frontend.input_device=="keyboard","unused battle face button steals keyboard hint")
    frontend.refresh(value,false,0)
    expect(frontend.help.text.contains("ARROWS") and not frontend.help.text.contains("R1"),
        "keyboard prompt retains unrelated controller instructions")
    value.paused = true
    frontend.refresh(value,false,0)
    await settle(viewport)
    var bounds := Rect2(Vector2.ZERO,Vector2(viewport.size))
    check_label(frontend.pause_controls,bounds,"pause control guide")
    expect(frontend.pause_panel.get_global_rect().encloses(frontend.pause_controls.get_global_rect()) and
        frontend.pause_controls.text.contains("F11") and frontend.pause_controls.text.contains("RIGHT ALT"),
        "full control guide is missing or escapes pause plate")
    value.paused = false
    frontend.refresh(value,false,0,{},true)
    frontend.input_device = "keyboard"


func check_lan_pause_hints(frontend: CanvasLayer, viewport: SubViewport, fixture: Dictionary) -> void:
    # Presentation-only fixtures: LAN still routes each machine's slot-zero
    # controls to its network player; this checks the guide, not networking.
    var value: Dictionary = fixture.duplicate(true)
    value.player_count = 2
    value.ai_p2 = false
    value.intro = false
    value.paused = true
    value.events = []
    if value.players.size()<2:
        value.players.append(value.players[0].duplicate(true))
        value.players[1].id = 1
    var bounds := Rect2(Vector2.ZERO,Vector2(viewport.size))
    frontend.show_battle()
    for device in ["keyboard","gamepad"]:
        frontend.input_device = device
        for local_player in 2:
            var role: String = "host" if local_player==0 else "guest"
            var owner := "P%d %s"%[local_player+1,role.to_upper()]
            var network := {"role":role,"local_player":local_player,"waiting":local_player==1,
                "status":"P%d · %s"%[local_player+1,role.to_upper()]}
            var context := "LAN pause %s/%s"%[owner,device]
            frontend.refresh(value,false,0,network,true)
            await settle(viewport)
            var guide: String = frontend.pause_controls.text
            expect(frontend.pause_panel.visible and not frontend.human_p2_input,
                context+": local two-player inputs remain enabled in the guide")
            for unused in ["WASD","LEFT ALT","PAD 2","ASSIGNED CONTROLLER","AI TEAMMATE"]:
                expect(not guide.contains(unused) and not frontend.help.text.contains(unused),
                    context+": advertises unused local controls: "+unused)
            expect(frontend.help.text.contains("LAN "+owner) and guide.contains("LAN "+str(network.status)),
                context+": local network player identity is missing")
            expect(frontend.help.text.contains(" WAIT")==bool(network.waiting) and
                frontend.restart_button.disabled==(local_player==1) and
                guide.contains("HOST RESTARTS")==bool(local_player==1),
                context+": wait state or host-only restart changed")
            if device=="keyboard":
                expect(guide.begins_with("P%d  ARROWS  MOVE"%[local_player+1]) and
                    guide.contains("SPACE / RIGHT ALT / CTRL  FIRE"),
                    context+": slot-zero keyboard controls use the wrong player identity")
            else:
                expect(guide.begins_with("STICK / D-PAD  MOVE") and
                    guide.contains("BOTTOM / LEFT FACE  FIRE") and guide.contains("R1 / RT  FIRE"),
                    context+": local controller controls are incomplete")
            check_label(frontend.pause_controls,bounds,context)
            expect(frontend.pause_panel.get_global_rect().encloses(frontend.pause_controls.get_global_rect()),
                context+": control guide escapes the pause plate")
        for crew in ["solo","human","ai"]:
            value.player_count = 1 if crew=="solo" else 2
            value.ai_p2 = crew=="ai"
            frontend.refresh(value,false,0,{},true)
            var guide: String = frontend.pause_controls.text
            var context := "Offline pause %s/%s"%[crew,device]
            expect(not guide.contains("LAN ") and not frontend.restart_button.disabled and
                frontend.human_p2_input==(crew=="human"),context+": LAN state leaks into offline controls")
            expect(guide.contains("P2  WASD")==bool(crew=="human" and device=="keyboard") and
                guide.contains("F / LEFT ALT / CTRL")==bool(crew=="human" and device=="keyboard") and
                guide.contains("ASSIGNED CONTROLLER")==bool(crew=="human" and device=="gamepad") and
                guide.contains("P2  AI TEAMMATE")==bool(crew=="ai"),context+": offline crew controls changed")
            expect(frontend.help.text.contains("P2: WASD / F")==bool(crew=="human" and device=="keyboard") and
                frontend.help.text.contains("P2: PAD 2")==bool(crew=="human" and device=="gamepad") and
                frontend.help.text.contains("P2 AI TEAMMATE")==bool(crew=="ai"),context+": offline footer changed")
            if device=="keyboard":
                expect(guide.begins_with("P1  ARROWS  MOVE"),context+": local primary keyboard identity changed")
        value.player_count = 2
        value.ai_p2 = false
    value.paused = false
    frontend.input_device = "keyboard"
    frontend.refresh(value,false,0,{},true)


func run(parent: Node, fixtures: Array) -> Dictionary:
    for dimensions in DIMENSIONS:
        var viewport := SubViewport.new()
        viewport.size = dimensions
        viewport.render_target_update_mode = SubViewport.UPDATE_DISABLED
        parent.add_child(viewport)
        var frontend := Frontend.new()
        viewport.add_child(frontend)
        frontend.show_menu()
        await settle(parent)
        await check_first_battle_layout(frontend,viewport,fixtures[0])
        expect(viewport.get_visible_rect().size.is_equal_approx(Vector2(dimensions)),"requested canvas size did not actually apply")
        for fixture in fixtures:
            frontend.configure(fixture.settings,false,.75)
            frontend.show_battle()
            for kind in ["native","max-status","respawning","out"]:
                var value: Dictionary = fixture.snapshot.duplicate(true)
                if kind != "native":
                    value.intro = false
                    value.stage = 35
                    value.enemies_left = 19
                    value.base_alive = kind != "out"
                    value.base_steel_remaining = 17.2 if kind == "max-status" else 0.0
                    value.base_steel = kind == "max-status"
                    for player in value.players:
                        player.vehicle_name = "M4A3 SHERMAN"
                        player.level = 3
                        player.hp = 6 if kind == "max-status" else 0
                        player.max_hp = 6
                        player.lives = 99 if kind == "max-status" else (2 if kind == "respawning" else 0)
                        player.score = 2147483647-int(player.id)
                        player.streak = 2147483647
                        player.shield = 5.0
                        player.boat = true
                        player.active = kind == "max-status"
                var before := JSON.stringify(value)
                frontend.refresh(value,false,0.0)
                await settle(parent)
                check_hud(frontend,value,dimensions,"%s/%s/%s"%[dimensions,fixture.name,kind])
                expect(JSON.stringify(value) == before,"HUD mutated its supplied presentation snapshot")
            receipts.append({"canvas":[dimensions.x,dimensions.y],"crew":fixture.name,
                "player_cards":[str(frontend.p1_card.get_global_rect()),str(frontend.p2_card.get_global_rect())],
                "mission":str(frontend.mission_card.get_global_rect()),"radar":str(frontend.radar.get_global_rect())})
        await check_health_transitions(frontend,viewport,fixtures[0].snapshot)
        await check_vehicle_columns(frontend,viewport,fixtures[0].snapshot)
        await check_feedback_and_hints(frontend,viewport,fixtures[0].snapshot)
        await check_lan_pause_hints(frontend,viewport,fixtures[0].snapshot)
        await check_menu(frontend,viewport)
        viewport.free()
    return {"passed":failures.is_empty(),"samples":samples,"layouts":receipts,"failures":failures.slice(0,20)}
