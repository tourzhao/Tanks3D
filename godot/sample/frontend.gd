extends CanvasLayer

signal selection_changed
signal start_requested(configuration: Dictionary)
signal resume_requested
signal restart_requested
signal menu_requested
signal confirm_requested
signal quit_requested
signal pixel_changed(enabled: bool)
signal volume_changed(value: float)
signal network_requested(host: bool, address: String, port: int, configuration: Dictionary)
signal network_cancel_requested

const Radar = preload("res://radar.gd")
const ArcadePlate = preload("res://arcade_plate.gd")
const MenuRow = preload("res://arcade_menu_row.gd")
const Backdrop = preload("res://arcade_backdrop.gd")
const ReportPreview = preload("res://report_preview.gd")
const ArcadeFont = preload("res://resources/fonts/arcade.fnt")
const NATIONS := ["USA","USSR","GERMANY"]
const TIERS := ["LIGHT","MEDIUM","HEAVY","SUPER HEAVY"]
const TECH := [["M4A3(75) SHERMAN","M26 PERSHING","M60A3","M1A1 ABRAMS"],
    ["T-34/76","IS-2","T-62","T-90A"],
    ["PANTHER AUSF. A","TIGER II","LEOPARD 1","LEOPARD 2A4"]]
const DEFAULTS := {"stage":1,"players":1,"ai_p2":false,"lives":10,"nation_p1":0,"nation_p2":1,
    "max_hp":3,"enemy_speed":0,"enemy_fire":0,"enemy_spawn":0,"camera_yaw":0,"camera_elevation":50}

# Code-drawn HUD artwork stays in the existing resource. The bounded plate
# exposes hierarchy through team paint, a stepped edge and health segments.
class CombatPlate extends PanelContainer:
    var accent := Color("ec9118")
    var hp_color := Color("62e881")
    var hp_ratio := 1.0
    var hp_maximum := 3
    var pulse := 0.0
    var pulse_color := Color.WHITE

    func _ready() -> void:
        add_theme_stylebox_override("panel",StyleBoxEmpty.new())
        resized.connect(queue_redraw)

    func _draw() -> void:
        var w := size.x
        var h := size.y
        var points := PackedVector2Array([Vector2(7,0),Vector2(w,0),Vector2(w,h-7),
            Vector2(w-7,h),Vector2(0,h),Vector2(0,7)])
        draw_colored_polygon(points,Color(.035,.055,.061,.82))
        draw_rect(Rect2(7,2,w-9,22),Color(accent,.09))
        draw_line(Vector2(8,0),Vector2(124,0),accent,2)
        draw_line(Vector2(0,8),Vector2(0,31),accent,3)
        draw_line(Vector2(7,26),Vector2(w-7,26),Color("48554c"),1)
        draw_line(Vector2(w-55,h-1),Vector2(w-9,h-1),accent.darkened(.3),1)
        for index in 4:
            var x := w-18-index*6
            draw_line(Vector2(x,h-5),Vector2(x+2,h-7),Color("677165"),1)
        var count := clampi(hp_maximum,1,6)
        var width := 108.0/count
        for index in count:
            var on := float(index)/count < hp_ratio
            draw_rect(Rect2(201+index*width,49,width-3,3),hp_color if on else Color("35403c"))
        if pulse>0:
            var edge := points.duplicate()
            edge.append(points[0])
            draw_polyline(edge,Color(pulse_color,pulse*.88),2,false)
            draw_rect(Rect2(7,27,w-14,24),Color(pulse_color,pulse*.13))

class CombatTag extends Control:
    var accent := Color.WHITE

    func _ready() -> void:
        mouse_filter = Control.MOUSE_FILTER_IGNORE
        resized.connect(queue_redraw)

    func _draw() -> void:
        draw_rect(Rect2(Vector2.ZERO,size),Color(.027,.043,.047,.92))
        draw_line(Vector2(0,0),Vector2(size.x,0),accent,1)

const PICKUP_NAMES := ["GRENADE", "SHIELD", "TIME STOP", "BASE STEEL", "+1 LIFE", "STAR", "POWER GUN", "BOAT", "REPAIR"]
var input_device := "keyboard"
var human_p2_input := false
var input_axes: Dictionary = {}
var input_active: Dictionary = {}
var feedback_tick := -1
var feedback_stage := -1
var feedback_intro := false
var feedback_players: Array = []
var feedback: Array[Dictionary] = [{},{}]
var feedback_labels: Array[Label] = []
var feedback_plates: Array[Control] = []
var pause_controls: Label
var setup_button: Button

var menu_open := false
var menu_page := "setup"
var root: Control
var menu: Control
var menu_subtitle: Label
var menu_help: Label
var menu_confirm: Label
var crew_help: Label
var device_help: Label
var menu_panel: PanelContainer
var setup_options: VBoxContainer
var advanced: VBoxContainer
var network_box: VBoxContainer
var network_ip: LineEdit
var network_port: SpinBox
var network_wait: Label
var network_pending := false
var network_hosting := false
var network_editing := false
var tech_labels: Array[Label] = []
var rows: Dictionary = {}
var row_keys: Dictionary = {}
var pad_axes: Dictionary = {}
var pad_directions: Dictionary = {}
var controls: Dictionary = {}
var status_label: Label
var pause_panel: PanelContainer
var pause_style: Label
var resume_button: Button
var restart_button: Button
var report_panel: Control
var report_title: Label
var report_text: Label
var report_continue: Button
var report_column: VBoxContainer
var report_preview: Control
var report_record: Label
var report_stage: Label
var report_cards: Array[Control] = []
var report_card_labels: Array[Dictionary] = []
var record_page: Control
var record_score: Label
var modal_message: Label
var bonus_message: Label
var bonus_label: Label
var help: Label
var hud_root: Control
var p1_card: PanelContainer
var p2_card: PanelContainer
var mission_card: PanelContainer
var p1_label: Label
var p2_label: Label
var p1_stats: Label
var p2_stats: Label
var p1_status: Label
var p2_status: Label
var player_hud: Array[Dictionary] = []
var stage_label: Label
var radar: Control
var settings: Dictionary = DEFAULTS.duplicate()
var pixel := false
var volume := .75

func _ready() -> void:
    layer = 5
    root = Control.new()
    root.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
    root.mouse_filter = Control.MOUSE_FILTER_IGNORE
    root.texture_filter = CanvasItem.TEXTURE_FILTER_NEAREST
    root.theme = make_theme()
    add_child(root)
    build_hud()
    build_menu()
    build_pause()
    build_report()
    menu.hide()
    pause_panel.hide()
    report_panel.hide()
    Input.joy_connection_changed.connect(pad_connection_changed)

func pad_connection_changed(device: int, _connected: bool) -> void:
    # Bluetooth discovery can finish after the deployment menu is built.
    # Device IDs may also be reused: a held direction from the previous
    # connection must not swallow the new controller's first menu movement.
    pad_axes.erase(device)
    pad_directions.erase(device)
    input_axes.erase(device)
    input_active.erase(device)
    input_active.erase("trigger_%d" % device)
    refresh_menu()

func make_theme() -> Theme:
    var theme := Theme.new()
    theme.default_font = ArcadeFont
    theme.default_font_size = 18
    theme.set_color("font_color","Label",Color("e1e6e6"))
    for kind in ["normal","hover","pressed","focus"]:
        var style := StyleBoxFlat.new()
        style.bg_color = Color("162b34") if kind == "normal" else Color("2a484c")
        style.border_color = Color("63979f") if kind == "normal" else Color("ffdf62")
        style.set_border_width_all(1 if kind == "normal" else 2)
        style.content_margin_left = 12
        style.content_margin_right = 12
        style.content_margin_top = 8
        style.content_margin_bottom = 8
        theme.set_stylebox(kind,"Button",style)
        theme.set_stylebox(kind,"LineEdit",style)
    return theme

func label(text: String, parent: Node, font_size: int = 18) -> Label:
    var item := Label.new()
    item.text = text
    item.add_theme_font_size_override("font_size",font_size)
    item.mouse_filter = Control.MOUSE_FILTER_IGNORE
    parent.add_child(item)
    return item

func button(text: String, parent: Node, action: Callable) -> Button:
    var item := Button.new()
    item.text = text
    item.custom_minimum_size.y = 38
    item.pressed.connect(func(): selection_changed.emit(); action.call())
    parent.add_child(item)
    return item

func panel(parent: Node, width: float, height: float) -> PanelContainer:
    var item := PanelContainer.new()
    item.set_anchors_and_offsets_preset(Control.PRESET_CENTER)
    item.offset_left = -width*.5
    item.offset_right = width*.5
    item.offset_top = -height*.5
    item.offset_bottom = height*.5
    var style := StyleBoxFlat.new()
    style.bg_color = Color(.016,.031,.043,.94)
    style.border_color = Color("63979f")
    style.set_border_width_all(2)
    style.set_corner_radius_all(12)
    style.content_margin_left = 20
    style.content_margin_right = 20
    style.content_margin_top = 14
    style.content_margin_bottom = 14
    item.add_theme_stylebox_override("panel",style)
    parent.add_child(item)
    return item

func build_models() -> void:
    var models := Control.new()
    models.hide()
    menu.add_child(models)
    for key in ["mode","nation_p1","nation_p2"]:
        var item := OptionButton.new()
        for value in (["1 PLAYER","2 PLAYERS","AI AS P2"] if key == "mode" else NATIONS): item.add_item(value)
        models.add_child(item)
        controls[key] = item
    var limits := {"stage":[1,35,1],"lives":[1,99,1],"max_hp":[1,6,1],
        "enemy_speed":[-30,30,5],"enemy_fire":[-30,30,5],"enemy_spawn":[-30,30,5],
        "camera_yaw":[-45,45,5],"camera_elevation":[40,70,5],"network_port":[1,65535,1]}
    for key in limits:
        var item := SpinBox.new()
        item.min_value = limits[key][0]
        item.max_value = limits[key][1]
        item.step = limits[key][2]
        models.add_child(item)
        controls[key] = item
    network_port = controls.network_port
    network_port.value = 41987
    var toggle := CheckButton.new()
    models.add_child(toggle)
    controls.pixel = toggle
    toggle.toggled.connect(func(value: bool): pixel=value; pixel_changed.emit(value); selection_changed.emit(); refresh_menu())
    var slider := HSlider.new()
    slider.min_value = 0
    slider.max_value = 1
    slider.step = .05
    models.add_child(slider)
    controls.volume = slider
    slider.value_changed.connect(func(value: float): volume=value; volume_changed.emit(value); refresh_menu())

func add_row(page: String, key: String, caption: String, parent: Node, adjustable: bool = true) -> Button:
    var item := MenuRow.new()
    item.caption = caption
    item.adjustable = adjustable
    parent.add_child(item)
    controls["row_"+key] = item
    row_keys[item] = key
    if not rows.has(page): rows[page] = []
    rows[page].append(item)
    item.pressed.connect(func(): activate_row(key))
    return item

func build_menu() -> void:
    menu = Control.new()
    menu.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
    root.add_child(menu)
    menu.add_child(Backdrop.new())
    var title := label("TANKS 3D",menu,92)
    title.set_anchors_and_offsets_preset(Control.PRESET_TOP_WIDE)
    title.offset_top = 54
    title.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
    title.add_theme_color_override("font_color",Color("ffcb00"))
    menu_subtitle = label("",menu,22)
    menu_subtitle.set_anchors_and_offsets_preset(Control.PRESET_TOP_WIDE)
    menu_subtitle.offset_top = 138
    menu_subtitle.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
    menu_subtitle.add_theme_color_override("font_color",Color("b4dae1"))
    menu_panel = panel(menu,900,400)
    menu_panel.set_anchors_and_offsets_preset(Control.PRESET_CENTER_TOP)
    menu_panel.offset_left = -450
    menu_panel.offset_right = 450
    menu_panel.offset_top = 178
    menu_panel.offset_bottom = 570
    var pages := VBoxContainer.new()
    pages.add_theme_constant_override("separation",0)
    menu_panel.add_child(pages)
    setup_options = VBoxContainer.new()
    setup_options.add_theme_constant_override("separation",0)
    pages.add_child(setup_options)
    advanced = VBoxContainer.new()
    advanced.add_theme_constant_override("separation",0)
    pages.add_child(advanced)
    network_box = VBoxContainer.new()
    network_box.add_theme_constant_override("separation",0)
    pages.add_child(network_box)
    build_models()
    controls.deploy = add_row("setup","mode","PLAYERS",setup_options)
    add_row("setup","stage","STAGE",setup_options)
    add_row("setup","lives","LIVES EACH",setup_options)
    add_row("setup","nation_p1","P1 NATION",setup_options)
    add_row("setup","nation_p2","P2 NATION",setup_options)
    controls.network_toggle = add_row("setup","network","LOCAL NETWORK",setup_options,false)
    controls.advanced_toggle = add_row("setup","advanced","ADVANCED SETTINGS",setup_options,false)
    for index in 2:
        var line := label("",setup_options,13)
        line.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
        line.add_theme_color_override("font_color",Color("f0b441") if index == 0 else Color("58dd8e"))
        tech_labels.append(line)
    var captions := {"max_hp":"PLAYER HP","enemy_speed":"ENEMY SPEED","enemy_fire":"FIRE FREQUENCY","enemy_spawn":"SPAWN PACE","camera_yaw":"VIEW HORIZONTAL","camera_elevation":"VIEW ELEVATION","pixel":"PIXEL STYLE","volume":"SOUND VOLUME"}
    for key in captions:
        var item := add_row("advanced",key,captions[key],advanced)
        item.custom_minimum_size.y = 36
    controls.advanced_back = add_row("advanced","back","BACK TO SETUP",advanced,false)
    controls.advanced_back.custom_minimum_size.y = 36
    controls.network_host = add_row("network","network_host","CREATE ROOM",network_box,false)
    controls.network_join = add_row("network","network_join","JOIN ROOM",network_box,false)
    add_row("network","network_ip","HOST IP",network_box,false)
    add_row("network","network_port","PORT",network_box)
    controls.network_back = add_row("network","network_back","BACK",network_box,false)
    network_ip = LineEdit.new()
    network_ip.placeholder_text = "IPv4[:port]"
    network_ip.text = "127.0.0.1"
    network_ip.custom_minimum_size.y = 40
    network_box.add_child(network_ip)
    network_ip.hide()
    network_ip.text_changed.connect(func(_text: String): refresh_menu())
    network_wait = label("",pages,26)
    network_wait.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
    network_wait.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
    network_wait.hide()
    menu_help = label("",menu,16)
    menu_help.set_anchors_and_offsets_preset(Control.PRESET_BOTTOM_WIDE)
    menu_help.offset_top = -124
    menu_help.offset_bottom = -51
    menu_help.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
    menu_help.add_theme_color_override("font_color",Color("ffe05e"))
    menu_confirm = label("START / +: PLAY   A / B / ENTER: SELECT",menu,25)
    menu_confirm.set_anchors_and_offsets_preset(Control.PRESET_BOTTOM_WIDE)
    menu_confirm.offset_top = -94
    menu_confirm.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
    menu_confirm.add_theme_color_override("font_color",Color("ffe05e"))
    crew_help = label("",menu,16)
    crew_help.set_anchors_and_offsets_preset(Control.PRESET_BOTTOM_WIDE)
    crew_help.offset_top = -61
    crew_help.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
    crew_help.add_theme_color_override("font_color",Color("bdd2d6"))
    device_help = label("",menu,13)
    device_help.set_anchors_and_offsets_preset(Control.PRESET_BOTTOM_WIDE)
    device_help.offset_top = -47
    device_help.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
    device_help.add_theme_color_override("font_color",Color("91afb5"))
    status_label = label("",menu,15)
    status_label.set_anchors_and_offsets_preset(Control.PRESET_BOTTOM_WIDE)
    status_label.offset_top = -24
    status_label.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
    status_label.add_theme_color_override("font_color",Color("ff896f"))
    set_menu_page("setup",false)

func visible_rows() -> Array:
    var result: Array = []
    for item in rows.get(menu_page,[]):
        if item.visible: result.append(item)
    return result

func set_menu_page(page: String, focus: bool = true) -> void:
    menu_page = page
    network_editing = false
    network_ip.hide()
    setup_options.visible = page == "setup"
    advanced.visible = page == "advanced"
    network_box.visible = page == "network" and not network_pending
    network_wait.visible = page == "network" and network_pending
    refresh_menu()
    if focus and not network_pending:
        var available := visible_rows()
        if not available.is_empty(): available[0].grab_focus()

func refresh_menu() -> void:
    if setup_options == null: return
    var mode: int = controls.mode.selected
    controls.row_nation_p2.visible = mode != 0
    controls.nation_p2.disabled = mode == 0
    for key in ["mode","stage","lives","nation_p1","nation_p2","max_hp","enemy_speed","enemy_fire","enemy_spawn","camera_yaw","camera_elevation","pixel","volume","network_port"]:
        controls["row_"+key].set_value(row_value(key))
    for key in ["advanced","network"]: controls["row_"+key].set_value("OPEN")
    controls.row_back.set_value("RETURN")
    controls.row_network_back.set_value("RETURN")
    controls.row_network_host.set_value("HOST")
    controls.row_network_join.set_value("JOIN")
    controls.row_network_ip.set_value(network_ip.text if not network_ip.text.is_empty() else "Enter address")
    for index in 2:
        tech_labels[index].visible = index == 0 or mode != 0
        var nation: int = controls["nation_p%d"%(index+1)].selected
        tech_labels[index].text = "P%d TECH  %s"%[index+1," > ".join(TECH[nation])]
    menu_subtitle.text = {"setup":"TILTED TOP-DOWN ARMORED COMBAT","advanced":"ADVANCED SETTINGS","network":"LOCAL NETWORK CO-OP"}[menu_page]
    menu_panel.offset_bottom = 570 if menu_page == "setup" else (548 if menu_page == "advanced" else 480)
    menu_confirm.visible = menu_page in ["setup", "advanced"]
    menu_confirm.offset_top = -76 if menu_page == "advanced" else -94
    crew_help.visible = menu_page == "setup"
    if menu_page == "setup":
        menu_help.text = "D-PAD / STICK OR KEYS: SELECT / CHANGE   SHOULDER / SHIFT: x10"
        crew_help.text = "P1 YOU: PAD / ARROWS   P2 AI TEAMMATE" if mode == 2 else "PLAYERS: 1 PLAYER / 2 PLAYERS / AI AS P2"
        device_help.text = "GAMEPADS %d   %s   F11 FULLSCREEN   MINUS / ESC QUIT"%[Input.get_connected_joypads().size(),"1P/AI: EITHER PAD" if mode != 1 else "FIRST=P1 SECOND=P2"]
    elif menu_page == "advanced":
        menu_help.text = "HP 1-6   RATES -30% TO +30% (STEP 5%)\nVIEW LEFT 45 TO RIGHT 45 / ELEVATION 40 TO 70 (STEP 5 DEG)\nMINUS / ESC: RETURN   TOP FACE / R: RESET"
        device_help.text = "F11 FULLSCREEN"
    else:
        menu_help.text = "YOUR NATION: %s   HOST STAGE: %d   LIVES: %d\nChoose your nation and host rules in the main setup menu.\n%s"%[NATIONS[controls.nation_p1.selected],int(controls.stage.value),int(controls.lives.value),"ESC / MINUS: CANCEL" if network_pending else "ARROWS / PAD: SELECT   ENTER: OPEN   ESC: BACK"]
        device_help.text = "Use the same app build on both computers, on the same network."
        network_wait.text = ("YOU ARE P1 / HOST\n\nWaiting for another player..." if network_hosting else "YOU ARE P2 / JOINING\n\nConnecting and preparing the battle...")+"\n\n"+status_label.text

func row_value(key: String) -> String:
    if key == "mode": return ["1 PLAYER","2 PLAYERS","AI AS P2"][controls.mode.selected]
    if key.begins_with("nation_"): return NATIONS[controls[key].selected]
    if key == "pixel": return "ON" if controls.pixel.button_pressed else "OFF"
    if key == "volume": return "%d%%"%roundi(controls.volume.value*100)
    var value: int = int(controls[key].value)
    if key == "max_hp": return "1  BANDAGE OFF" if value == 1 else str(value)
    if key in ["enemy_speed","enemy_fire","enemy_spawn"]: return "0% DEFAULT" if value == 0 else "%+d%%"%value
    if key == "camera_yaw": return "0 DEG STRAIGHT" if value == 0 else "%s %d DEG"%["LEFT" if value<0 else "RIGHT",absi(value)]
    if key == "camera_elevation": return "%d DEG%s"%[value," DEFAULT" if value == 50 else ""]
    return str(value)

func activate_row(key: String) -> void:
    selection_changed.emit()
    if key == "advanced": set_menu_page("advanced")
    elif key == "network": set_menu_page("network")
    elif key in ["back","network_back"]: set_menu_page("setup")
    elif key == "network_host": request_network(true)
    elif key == "network_join":
        if network_ip.text.is_empty(): activate_row("network_ip")
        else: request_network(false)
    elif key == "network_ip":
        network_editing = true
        network_ip.show()
        network_ip.grab_focus()
        network_ip.caret_column = network_ip.text.length()
    elif key == "pixel": controls.pixel.button_pressed = not controls.pixel.button_pressed
    elif menu_page == "setup":
        settings = read_settings()
        start_requested.emit(settings.duplicate())

func adjust_row(key: String, direction: int, coarse: bool) -> void:
    var step := 10 if coarse else 1
    if key == "mode": controls.mode.select(posmod(controls.mode.selected+direction,3))
    elif key.begins_with("nation_"): controls[key].select(posmod(controls[key].selected+direction,3))
    elif key == "stage": controls.stage.value = posmod(int(controls.stage.value)-1+direction*step,35)+1
    elif key == "lives": controls.lives.value += direction*step
    elif key == "pixel": controls.pixel.button_pressed = not controls.pixel.button_pressed
    elif key == "volume": controls.volume.value += direction*.05
    elif controls.has(key) and controls[key] is SpinBox: controls[key].value += direction*controls[key].step
    else: return
    selection_changed.emit()
    refresh_menu()

func menu_action(action: String, coarse: bool = false) -> void:
    if action == "cancel":
        if network_pending: network_cancel_requested.emit()
        elif network_editing:
            network_editing = false
            network_ip.hide()
            controls.row_network_ip.grab_focus()
        elif menu_page != "setup": set_menu_page("setup")
        else: quit_requested.emit()
        return
    if network_pending: return
    if action == "start" and menu_page in ["setup", "advanced"]:
        settings = read_settings()
        start_requested.emit(settings.duplicate())
        return
    if action == "reset" and menu_page == "advanced": reset_advanced(); return
    if action in ["one","two"] and menu_page == "setup":
        controls.mode.select(0 if action == "one" else 1)
        selection_changed.emit()
        refresh_menu()
        return
    var available := visible_rows()
    if available.is_empty(): return
    var focused := get_viewport().gui_get_focus_owner()
    var index := available.find(focused)
    if index < 0: index = 0
    if action in ["up","down"]:
        available[posmod(index+(-1 if action == "up" else 1),available.size())].grab_focus()
        selection_changed.emit()
    elif action in ["left","right"]: adjust_row(row_keys[available[index]],-1 if action == "left" else 1,coarse)
    elif action == "accept": available[index].pressed.emit()

func _input(event: InputEvent) -> void:
    if not menu_open or get_viewport().gui_disable_input: return
    # Menu input may be consumed here before the app input observer runs.
    note_input(event)
    var action := ""
    var coarse := Input.is_key_pressed(KEY_SHIFT)
    if event is InputEventKey and event.pressed and not event.echo:
        if network_editing:
            if event.keycode == KEY_ENTER:
                network_editing = false
                network_ip.hide()
                request_network(false)
                get_viewport().set_input_as_handled()
            elif event.keycode == KEY_ESCAPE:
                menu_action("cancel")
                get_viewport().set_input_as_handled()
            return
        match event.keycode:
            KEY_UP, KEY_W: action = "up"
            KEY_DOWN, KEY_S: action = "down"
            KEY_LEFT, KEY_A: action = "left"
            KEY_RIGHT, KEY_D: action = "right"
            KEY_ENTER, KEY_SPACE: action = "accept"
            KEY_ESCAPE, KEY_Q: action = "cancel"
            KEY_R: action = "reset"
            KEY_1: action = "one"
            KEY_2: action = "two"
        coarse = event.shift_pressed
    elif event is InputEventJoypadButton and event.pressed:
        match event.button_index:
            JOY_BUTTON_DPAD_UP: action = "up"
            JOY_BUTTON_DPAD_DOWN: action = "down"
            JOY_BUTTON_DPAD_LEFT: action = "left"
            JOY_BUTTON_DPAD_RIGHT: action = "right"
            JOY_BUTTON_A, JOY_BUTTON_B: action = "accept"
            JOY_BUTTON_START: action = "start" if menu_page in ["setup", "advanced"] else "accept"
            JOY_BUTTON_BACK: action = "cancel"
            JOY_BUTTON_Y: action = "reset"
        coarse = Input.is_joy_button_pressed(event.device,JOY_BUTTON_LEFT_SHOULDER) or Input.is_joy_button_pressed(event.device,JOY_BUTTON_RIGHT_SHOULDER)
    elif event is InputEventJoypadMotion and event.axis in [JOY_AXIS_LEFT_X,JOY_AXIS_LEFT_Y]:
        var axes: Vector2 = pad_axes.get(event.device,Vector2.ZERO)
        if event.axis == JOY_AXIS_LEFT_X: axes.x = event.axis_value
        else: axes.y = event.axis_value
        pad_axes[event.device] = axes
        var direction := ""
        if axes.length() > .5:
            direction = ("right" if axes.x>0 else "left") if absf(axes.x)>absf(axes.y) else ("down" if axes.y>0 else "up")
        elif axes.length() > .25: return
        if direction != str(pad_directions.get(event.device,"")):
            pad_directions[event.device] = direction
            action = direction
        coarse = Input.is_joy_button_pressed(event.device,JOY_BUTTON_LEFT_SHOULDER) or Input.is_joy_button_pressed(event.device,JOY_BUTTON_RIGHT_SHOULDER)
    if not action.is_empty():
        menu_action(action,coarse)
        get_viewport().set_input_as_handled()

func set_network_pending(value: bool, host: bool = false) -> void:
    network_pending = value
    network_hosting = host
    if value: set_menu_page("network",false)
    elif menu_open: set_menu_page("setup")

func configure(value: Dictionary, pixel_value: bool, volume_value: float) -> void:
    settings = value.duplicate()
    controls.mode.select(2 if settings.ai_p2 else (1 if int(settings.players)==2 else 0))
    for key in ["nation_p1","nation_p2"]: controls[key].select(int(settings[key]))
    for key in ["stage","lives","max_hp","enemy_speed","enemy_fire","enemy_spawn","camera_yaw","camera_elevation"]: controls[key].value = settings[key]
    controls.pixel.set_pressed_no_signal(pixel_value)
    controls.volume.set_value_no_signal(volume_value)
    pixel = pixel_value
    volume = volume_value
    refresh_menu()

func read_settings() -> Dictionary:
    var result := DEFAULTS.duplicate()
    result.players = 1 if controls.mode.selected == 0 else 2
    result.ai_p2 = controls.mode.selected == 2
    for key in ["nation_p1","nation_p2"]: result[key] = controls[key].selected
    for key in ["stage","lives","max_hp","enemy_speed","enemy_fire","enemy_spawn","camera_yaw","camera_elevation"]: result[key] = int(controls[key].value)
    return result

func reset_advanced() -> void:
    for key in ["max_hp","enemy_speed","enemy_fire","enemy_spawn","camera_yaw","camera_elevation"]: controls[key].value = DEFAULTS[key]
    controls.pixel.button_pressed = false
    selection_changed.emit()
    refresh_menu()

func request_network(host: bool) -> void:
    var value := read_settings()
    value.players = 2
    value.ai_p2 = false
    var address := network_ip.text.strip_edges()
    if not host and address.count(":") == 1:
        var parts := address.split(":")
        if parts[1].is_valid_int():
            var port := int(parts[1])
            if port < 1 or port > 65535:
                set_status("PORT MUST BE BETWEEN 1 AND 65535")
                return
            address = parts[0]
            network_port.value = port
    network_requested.emit(host,address,int(network_port.value),value)

func show_menu(status: String = "") -> void:
    reset_combat_feedback()
    menu_open = true
    menu.show()
    hud_root.hide()
    radar.hide()
    mission_card.hide()
    help.hide()
    bonus_message.hide()
    pause_panel.hide()
    report_panel.hide()
    modal_message.hide()
    status_label.text = status
    set_menu_page("setup")

func show_battle() -> void:
    reset_combat_feedback()
    menu_open = false
    menu.hide()
    hud_root.show()
    radar.show()
    mission_card.show()
    help.show()
    bonus_message.show()
    modal_message.show()
    var focused := root.get_viewport().gui_get_focus_owner()
    if focused != null: focused.release_focus()

func set_status(text: String) -> void:
    status_label.text = text
    if menu_page == "network": refresh_menu()
func hud_card(parent: Control, right_side: bool) -> PanelContainer:
    # Only these compact corner panels are opaque. The existing transparent
    # 74px root remains the camera contract and the upper center stays open.
    var card := CombatPlate.new()
    card.accent = Color("24cd5e") if right_side else Color("ec9118")
    card.set_anchors_and_offsets_preset(Control.PRESET_TOP_RIGHT if right_side else Control.PRESET_TOP_LEFT)
    card.offset_left = -327 if right_side else 7
    card.offset_right = -7 if right_side else 327
    card.offset_top = 7
    card.offset_bottom = 87
    card.mouse_filter = Control.MOUSE_FILTER_IGNORE
    parent.add_child(card)
    return card


func hud_field(parent: Control, top: int, preferred: int, color: Color, left: int, width: int, right_aligned := false) -> Label:
    var item := label("",parent,preferred)
    item.set_anchors_and_offsets_preset(Control.PRESET_TOP_LEFT)
    item.offset_left = 7+left
    item.offset_right = item.offset_left+width
    item.offset_top = 5+top
    item.offset_bottom = item.offset_top+preferred+2
    item.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT if right_aligned else HORIZONTAL_ALIGNMENT_LEFT
    item.mouse_filter = Control.MOUSE_FILTER_IGNORE
    item.add_theme_color_override("font_color",color)
    item.add_theme_color_override("font_shadow_color",Color(0,0,0,190.0/255))
    item.add_theme_constant_override("shadow_offset_x",2)
    item.add_theme_constant_override("shadow_offset_y",2)
    item.add_theme_constant_override("line_spacing",0)
    item.set_meta("column_width",width)
    return item


func fit_hud_text(item: Label, text: String, preferred: int, minimum: int) -> void:
    item.text = text
    var font_size := preferred
    var width := float(item.get_meta("column_width"))
    while font_size > minimum and ArcadeFont.get_string_size(text,HORIZONTAL_ALIGNMENT_LEFT,-1,font_size).x > width:
        font_size -= 1
    item.add_theme_font_size_override("font_size",font_size)


func refresh_player_hud(slot: int, player: Dictionary, value: Dictionary) -> void:
    var fields: Dictionary = player_hud[slot]
    var tier := clampi(int(player.get("level",0)),0,3)
    var accent := Color("ec9118") if slot == 0 else Color("24cd5e")
    fit_hud_text(fields.identity,"P%d%s  %s"%[int(player.id)+1,
        " AI" if slot == 1 and value.get("ai_p2",false) else "",NATIONS[clampi(int(player.get("nation",0)),0,2)]],18,16)
    fit_hud_text(fields.lives,"LIVES %d"%maxi(0,int(player.lives)),14,14)
    fit_hud_text(fields.vehicle,str(player.get("vehicle_name","TANK")),14,14)
    fields.vehicle.add_theme_color_override("font_color",Color("ffcb00") if tier >= 3 else Color("ffa100"))
    fit_hud_text(fields.tier,"TIER %d/4  %s"%[tier+1,TIERS[tier]],13,12)
    fit_hud_text(fields.score,"SCORE %d"%int(player.score),13,12)
    var hp := maxi(0,int(player.hp))
    var maximum := maxi(1,int(player.get("max_hp",3)))
    fit_hud_text(fields.hp,"HP  %d / %d"%[hp,maximum],16,16)
    fields.hp.add_theme_color_override("font_color",Color("878e94") if hp <= 0 else
        (Color("62e881") if hp >= maximum else (Color("ffcf53") if hp*2 >= maximum else Color("ff634b"))))
    fields.streak.visible = int(player.get("streak",0)) > 0
    if fields.streak.visible:
        fit_hud_text(fields.streak,"STREAK  x%d"%int(player.streak),14,14)
        fields.streak.add_theme_color_override("font_color",accent.lerp(Color.WHITE,.30))
    var statuses: Array[String] = []
    if not player.get("active",true): statuses.append("RESPAWNING" if int(player.lives)>0 else "OUT")
    else:
        if float(player.get("shield",0))>0: statuses.append("SHIELD")
        if player.get("boat",false): statuses.append("BOAT")
    fields.state.visible = not statuses.is_empty()
    if fields.state.visible: fit_hud_text(fields.state,"  ".join(statuses),14,14)
    var height := 102 if fields.streak.visible or fields.state.visible else 80
    fields.content.custom_minimum_size.y = height
    var card: PanelContainer = p1_card if slot == 0 else p2_card
    card.offset_bottom = 7+height
    card.hp_color = fields.hp.get_theme_color("font_color")
    card.hp_ratio = clampf(float(hp)/maximum,0,1)
    card.hp_maximum = maximum
    card.queue_redraw()
    feedback_labels[slot].offset_top = 7+height+5
    feedback_labels[slot].offset_bottom = 7+height+25
    feedback_plates[slot].offset_top = 7+height+3
    feedback_plates[slot].offset_bottom = 7+height+25


func hud_occluder_rects() -> Array[Rect2]:
    var rectangles: Array[Rect2] = []
    for control in [p1_card,p2_card,mission_card,radar]:
        if control != null and control.is_visible_in_tree(): rectangles.append(control.get_global_rect())
    return rectangles


func build_hud() -> void:
    hud_root = Control.new()
    hud_root.set_anchors_and_offsets_preset(Control.PRESET_TOP_WIDE)
    hud_root.offset_bottom = 74
    hud_root.mouse_filter = Control.MOUSE_FILTER_IGNORE
    root.add_child(hud_root)
    p1_card = hud_card(hud_root,false)
    p2_card = hud_card(hud_root,true)
    for slot in 2:
        var card := p1_card if slot == 0 else p2_card
        var content := Control.new()
        content.custom_minimum_size.y = 80
        content.mouse_filter = Control.MOUSE_FILTER_IGNORE
        card.add_child(content)
        var accent := Color("ec9118") if slot == 0 else Color("24cd5e")
        player_hud.append({"content":content,
            "identity":hud_field(content,0,18,accent,0,200),
            "lives":hud_field(content,2,14,Color("f5f5f5"),208,98,true),
            "vehicle":hud_field(content,26,14,Color("ffa100"),0,186),
            "hp":hud_field(content,24,16,Color("62e881"),194,112,true),
            "tier":hud_field(content,51,13,Color("c8c8c8"),0,168),
            "score":hud_field(content,51,13,Color("f5f5f5"),174,132,true),
            "streak":hud_field(content,75,14,accent,0,174),
            "state":hud_field(content,75,14,Color("c8c8c8"),182,124,true)})
        var tag := CombatTag.new()
        tag.set_anchors_and_offsets_preset(Control.PRESET_TOP_RIGHT if slot == 1 else Control.PRESET_TOP_LEFT)
        tag.hide()
        hud_root.add_child(tag)
        feedback_plates.append(tag)
        var flash := label("",hud_root,16)
        flash.set_anchors_and_offsets_preset(Control.PRESET_TOP_RIGHT if slot == 1 else Control.PRESET_TOP_LEFT)
        flash.offset_left = -321 if slot == 1 else 13
        flash.offset_right = -13 if slot == 1 else 321
        flash.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT if slot == 1 else HORIZONTAL_ALIGNMENT_LEFT
        flash.offset_top = 92
        flash.offset_bottom = 112
        flash.add_theme_color_override("font_shadow_color",Color("101c1a"))
        flash.add_theme_constant_override("shadow_offset_x",2)
        flash.add_theme_constant_override("shadow_offset_y",2)
        flash.hide()
        feedback_labels.append(flash)
    p1_label = player_hud[0].identity
    p2_label = player_hud[1].identity
    p1_stats = player_hud[0].hp
    p2_stats = player_hud[1].hp
    p1_status = player_hud[0].state
    p2_status = player_hud[1].state
    radar = Radar.new()
    radar.set_anchors_and_offsets_preset(Control.PRESET_BOTTOM_RIGHT)
    radar.offset_left = -184
    radar.offset_top = -208
    radar.offset_right = -16
    radar.offset_bottom = -12
    radar.mouse_filter = Control.MOUSE_FILTER_IGNORE
    root.add_child(radar)
    mission_card = ArcadePlate.new()
    mission_card.accent = Color("d2ab61")
    mission_card.padding_y = 4
    mission_card.set_anchors_and_offsets_preset(Control.PRESET_BOTTOM_RIGHT)
    mission_card.offset_left = -184
    mission_card.offset_top = -208
    mission_card.offset_right = -16
    mission_card.offset_bottom = -164
    mission_card.mouse_filter = Control.MOUSE_FILTER_IGNORE
    root.add_child(mission_card)
    stage_label = label("",mission_card,12)
    stage_label.mouse_filter = Control.MOUSE_FILTER_IGNORE
    stage_label.add_theme_constant_override("line_spacing",0)
    help = label("",root,12)
    help.set_anchors_and_offsets_preset(Control.PRESET_BOTTOM_WIDE)
    help.offset_left = 20
    help.offset_right = -200
    help.offset_top = -25
    help.mouse_filter = Control.MOUSE_FILTER_IGNORE
    help.add_theme_color_override("font_shadow_color",Color("162720"))
    help.add_theme_constant_override("shadow_offset_y",1)
    bonus_message = label("",root,20)
    bonus_label = bonus_message
    bonus_message.set_anchors_and_offsets_preset(Control.PRESET_BOTTOM_WIDE)
    bonus_message.offset_top = -66
    bonus_message.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
    bonus_message.add_theme_color_override("font_color",Color("ffe05e"))
    modal_message = label("",root,34)
    modal_message.set_anchors_and_offsets_preset(Control.PRESET_CENTER)
    modal_message.offset_left = -310
    modal_message.offset_right = 310
    modal_message.offset_top = -48
    modal_message.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
    modal_message.mouse_filter = Control.MOUSE_FILTER_IGNORE
    modal_message.add_theme_color_override("font_color",Color("fff0b4"))
    modal_message.add_theme_color_override("font_shadow_color",Color("162720"))
    modal_message.add_theme_constant_override("shadow_offset_y",3)

func build_pause() -> void:
    pause_panel = ArcadePlate.new()
    pause_panel.accent = Color("d2ab61")
    pause_panel.padding_y = 14
    pause_panel.set_anchors_and_offsets_preset(Control.PRESET_CENTER)
    pause_panel.offset_left = -325
    pause_panel.offset_right = 325
    pause_panel.offset_top = -185
    pause_panel.offset_bottom = 185
    root.add_child(pause_panel)
    var column := VBoxContainer.new()
    column.add_theme_constant_override("separation",9)
    pause_panel.add_child(column)
    var title := label("PAUSED",column,40)
    title.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
    title.add_theme_color_override("font_color",Color("ffe077"))
    pause_style = label("",column,14)
    pause_style.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
    pause_controls = label("",column,14)
    pause_controls.add_theme_constant_override("line_spacing",8)
    resume_button = button("ENTER: RESUME",column,func(): resume_requested.emit())
    resume_button.action_mode = BaseButton.ACTION_MODE_BUTTON_PRESS
    restart_button = button("R: RESTART STAGE",column,func(): restart_requested.emit())
    setup_button = button("ESC: SETUP",column,func(): menu_requested.emit())
    for action in [resume_button,restart_button,setup_button]:
        action.alignment = HORIZONTAL_ALIGNMENT_LEFT
        action.add_theme_color_override("font_color",Color("e1e6d5"))
        action.add_theme_color_override("font_focus_color",Color("ffe077"))
        for state in ["normal","hover","pressed","focus"]:
            var ink := StyleBoxFlat.new()
            ink.bg_color = Color(0,0,0,0) if state=="normal" else Color(.83,.67,.33,.08)
            ink.border_color = Color("d2ab61")
            ink.border_width_left = 3 if state=="focus" else 0
            ink.border_width_bottom = 1 if state=="focus" else 0
            ink.content_margin_left = 12
            ink.content_margin_right = 12
            ink.content_margin_top = 6
            ink.content_margin_bottom = 6
            action.add_theme_stylebox_override(state,ink)


# The input owner forwards only focused, accepted devices. Releasing an old
# held control and analog noise must not steal the prompt from the new device.
func note_input(event: InputEvent, allow_pad: bool = true) -> void:
    if event is InputEventKey and event.pressed and not event.echo:
        var key: int = event.physical_keycode if event.physical_keycode != KEY_NONE else event.keycode
        var accepted := [KEY_UP,KEY_DOWN,KEY_LEFT,KEY_RIGHT,KEY_SPACE,KEY_ALT,KEY_CTRL,
            KEY_ENTER,KEY_ESCAPE,KEY_TAB,KEY_R,KEY_F11]
        if not menu_open and key in [KEY_ALT,KEY_CTRL] and not (event.location==KEY_LOCATION_RIGHT or (event.location==KEY_LOCATION_LEFT and human_p2_input)): return
        if menu_open or human_p2_input:
            accepted.append_array([KEY_W,KEY_A,KEY_S,KEY_D,KEY_F])
        if menu_open: accepted.append_array([KEY_1,KEY_2,KEY_Q])
        if key in accepted: input_device = "keyboard"
    elif allow_pad and event is InputEventJoypadButton:
        if event.pressed and event.button_index in [JOY_BUTTON_DPAD_UP,JOY_BUTTON_DPAD_DOWN,
            JOY_BUTTON_DPAD_LEFT,JOY_BUTTON_DPAD_RIGHT,JOY_BUTTON_A,JOY_BUTTON_B,JOY_BUTTON_X,
            JOY_BUTTON_RIGHT_SHOULDER,JOY_BUTTON_START,JOY_BUTTON_BACK]:
            input_device = "gamepad"
        elif menu_open and event.pressed and event.button_index==JOY_BUTTON_Y:
            input_device = "gamepad"
    elif allow_pad and event is InputEventJoypadMotion:
        if event.axis in [JOY_AXIS_LEFT_X,JOY_AXIS_LEFT_Y]:
            var axes: Vector2 = input_axes.get(event.device,Vector2.ZERO)
            var before := axes
            if event.axis == JOY_AXIS_LEFT_X: axes.x = event.axis_value
            else: axes.y = event.axis_value
            input_axes[event.device] = axes
            var engaged: bool = input_active.get(event.device,false)
            if axes.length()>.20:
                # A materially moving stick is an input; tiny reports from a
                # held pad do not continually replace keyboard help.
                if not engaged or axes.distance_to(before)>.08: input_device = "gamepad"
                input_active[event.device] = true
            elif axes.length()<=.12: input_active[event.device] = false
        elif event.axis == JOY_AXIS_TRIGGER_RIGHT:
            var trigger_key := "trigger_%d"%event.device
            var previous: float = input_active.get(trigger_key,0.0)
            if event.axis_value>.5 and (previous<=.5 or absf(event.axis_value-previous)>.08):
                input_device = "gamepad"
            input_active[trigger_key] = event.axis_value


func refresh_control_hints(value: Dictionary, network: Dictionary) -> void:
    var pad := input_device == "gamepad"
    var count := int(value.get("player_count",1))
    var coop: bool = network.is_empty() and count>1 and not value.get("ai_p2",false)
    human_p2_input = coop
    help.text = "STICK MOVE   FACE FIRE   PLUS PAUSE / HELP" if pad else "ARROWS MOVE   SPACE FIRE   ENTER PAUSE / HELP"
    if coop: help.text += "   P2: PAD 2" if pad else "   P2: WASD / F"
    elif count>1 and network.is_empty(): help.text += "   P2 AI TEAMMATE"
    if not network.is_empty():
        var owner := "LAN P%d %s"%[int(network.get("local_player",0))+1,str(network.get("role","")).to_upper()]
        if network.get("waiting",false): owner += " WAIT"
        help.text = owner+"   "+("STICK MOVE   FACE FIRE   PLUS PAUSE / HELP" if pad else "ARROWS MOVE   SPACE FIRE   ENTER PAUSE / HELP")
    pause_controls.text = ("STICK / D-PAD  MOVE     BOTTOM / LEFT FACE  FIRE\nR1 / RT  FIRE     PLUS  RESUME     MINUS  SETUP" if pad else
        "P%d  ARROWS  MOVE     SPACE / RIGHT ALT / CTRL  FIRE"%[int(network.get("local_player",0))+1])
    if not pad and coop: pause_controls.text += "\nP2  WASD  MOVE       F / LEFT ALT / CTRL  FIRE"
    elif network.is_empty() and value.get("ai_p2",false): pause_controls.text += "\nP2  AI TEAMMATE"
    elif pad and coop: pause_controls.text += "\nP1 / P2  USE YOUR ASSIGNED CONTROLLER"
    pause_controls.text += "\nTAB  PIXEL STYLE     F11  FULLSCREEN\nR  RESTART STAGE (KEYBOARD)"
    if not network.is_empty():
        pause_controls.text += "\nLAN "+str(network.get("status",""))
        if network.get("role","")=="guest": pause_controls.text += "  /  HOST RESTARTS"
    # Keep a compact centered plate while allowing the complete LAN/controller
    # guide to grow by its actual line count instead of leaving a fixed gap.
    var height := maxf(340.0,260.0+22.0*pause_controls.text.split("\n").size())
    pause_panel.offset_top = -height*.5
    pause_panel.offset_bottom = height*.5
    resume_button.text = ">  PLUS / A / B: RESUME" if pad else ">  ENTER: RESUME"
    restart_button.text = ">  R: RESTART STAGE"
    setup_button.text = ">  MINUS: SETUP" if pad else ">  ESC: SETUP"


func reset_combat_feedback() -> void:
    feedback_tick = -1
    feedback_stage = -1
    feedback_intro = false
    feedback_players.clear()
    feedback = [{},{}]
    for item in feedback_labels: item.hide()
    for item in feedback_plates: item.hide()
    for card in [p1_card,p2_card]:
        if card != null:
            card.pulse = 0.0
            card.queue_redraw()


func trigger_feedback(slot: int, kind: String, caption: String, duration: float) -> void:
    if slot<0 or slot>=2: return
    var priority := {"pickup":1,"upgrade":2,"damage":3}
    var old: Dictionary = feedback[slot]
    if not old.is_empty() and int(priority[old.kind])>int(priority[kind]) and float(old.age)<.18: return
    feedback[slot] = {"kind":kind,"caption":caption,"age":0.0,"duration":duration}


func refresh_combat_feedback(value: Dictionary, dt: float, reset: bool) -> void:
    var tick := int(value.get("tick",-1))
    var stage_number := int(value.get("stage",1))
    var intro := bool(value.get("intro",false))
    var players: Array = value.get("players",[])
    var boundary := reset or feedback_tick<0 or tick<feedback_tick or stage_number!=feedback_stage or (intro and not feedback_intro)
    if boundary: reset_combat_feedback()
    var paused := bool(value.get("paused",false))
    var terminal := intro or bool(value.get("settling",false)) or bool(value.get("high_score",false)) or bool(value.get("game_over",false))
    var count := mini(2,mini(players.size(),int(value.get("player_count",players.size()))))
    if not paused and not terminal and not boundary:
        for slot in 2:
            if not feedback[slot].is_empty():
                feedback[slot].age += clampf(dt,0,.05)
                if float(feedback[slot].age)>=float(feedback[slot].duration): feedback[slot] = {}
        if tick!=feedback_tick:
            for event in value.get("events",[]):
                if event.get("type","")=="TankDamaged":
                    var slot := int(event.get("target_player",-1))
                    if slot>=0 and slot<count: trigger_feedback(slot,"damage","HIT!",.52)
                elif event.get("type","")=="BonusCollected":
                    var slot := int(event.get("source_player",-1))
                    var bonus := int(event.get("bonus_type",-1))
                    if slot<0 or slot>=count or bonus<0 or bonus>=PICKUP_NAMES.size(): continue
                    var upgraded := slot<feedback_players.size() and int(players[slot].get("level",0))>int(feedback_players[slot].get("level",0)) and bonus in [5,6]
                    trigger_feedback(slot,"upgrade" if upgraded else "pickup",
                        "UPGRADE!  TIER %d"%(int(players[slot].get("level",0))+1) if upgraded else PICKUP_NAMES[bonus],.95 if upgraded else .75)
    if terminal: feedback = [{},{}]
    for slot in 2:
        var item: Label = feedback_labels[slot]
        var card := p1_card if slot==0 else p2_card
        # Respawn/new roster boundaries erase old flashes; a death never
        # synthesizes a hit from a changed HP number.
        if slot>=count or (slot<feedback_players.size() and (players[slot].get("active",false)!=feedback_players[slot].get("active",false) or players[slot].get("nation",0)!=feedback_players[slot].get("nation",0))):
            feedback[slot] = {}
        var current: Dictionary = feedback[slot]
        item.visible = not current.is_empty() and slot<count
        var tag: Control = feedback_plates[slot]
        tag.visible = item.visible
        card.pulse = 0.0
        if not current.is_empty():
            var color := Color("ff785e") if current.kind=="damage" else (Color("ffe077") if current.kind=="upgrade" else Color("8debaf"))
            var age := float(current.age)
            var duration := float(current.duration)
            item.text = str(current.caption)
            item.add_theme_color_override("font_color",color)
            var width := minf(320.0,ArcadeFont.get_string_size(item.text,HORIZONTAL_ALIGNMENT_LEFT,-1,16).x+12.0)
            tag.offset_left = -7-width if slot==1 else 7.0
            tag.offset_right = -7.0 if slot==1 else 7+width
            tag.accent = color
            tag.modulate.a = minf(1.0,(duration-age)/.18)
            tag.queue_redraw()
            item.modulate.a = tag.modulate.a
            card.pulse_color = color
            card.pulse = maxf(0.0,1.0-age/.36)
        card.queue_redraw()
    feedback_tick = tick
    feedback_stage = stage_number
    feedback_intro = intro
    # Retain only event-relevant scalar fields, never the mutable supplied snapshot.
    feedback_players = []
    for player in players.slice(0,count):
        feedback_players.append({"level":int(player.get("level",0)),"active":bool(player.get("active",false)),"nation":int(player.get("nation",0))})


func build_report() -> void:
    report_panel = Control.new()
    report_panel.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
    root.add_child(report_panel)
    report_panel.add_child(Backdrop.new())
    report_column = VBoxContainer.new()
    report_column.set_anchors_and_offsets_preset(Control.PRESET_TOP_WIDE)
    report_column.offset_left = 72
    report_column.offset_right = -72
    report_column.offset_top = 22
    report_column.add_theme_constant_override("separation",8)
    report_panel.add_child(report_column)
    report_record = label("",report_column,22)
    report_record.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
    report_stage = label("",report_column,42)
    report_stage.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
    report_stage.add_theme_color_override("font_color",Color("fff7c2"))
    report_title = label("STAGE BATTLE REPORT",report_column,20)
    report_title.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
    report_preview = ReportPreview.new()
    report_preview.custom_minimum_size.y = 160
    report_column.add_child(report_preview)
    var cards := HBoxContainer.new()
    cards.alignment = BoxContainer.ALIGNMENT_CENTER
    cards.add_theme_constant_override("separation",20)
    report_column.add_child(cards)
    for slot in 2:
        var card := ArcadePlate.new()
        card.accent = Color("ec9118") if slot == 0 else Color("24cd5e")
        card.size_flags_horizontal = Control.SIZE_EXPAND_FILL
        cards.add_child(card)
        report_cards.append(card)
        var column := VBoxContainer.new()
        column.add_theme_constant_override("separation",5)
        card.add_child(column)
        var fields: Dictionary = {}
        fields.header = label("",column,20)
        fields.header.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
        fields.header.add_theme_color_override("font_color",card.accent)
        fields.vehicle = label("",column,14)
        fields.vehicle.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
        var header := report_line(column,12)
        header.name.text = "TYPE"
        header.kills.text = "K.O."
        header.points.text = "POINTS"
        fields.rows = []
        var names := ["BASIC","FAST","POWER","ARMOR"]
        var colors := [Color("ecd578"),Color("66d3e8"),Color("f6914a"),Color("e15b59")]
        for type in 4:
            var item := report_line(column,18)
            item.name.text = names[type]
            item.name.add_theme_color_override("font_color",colors[type])
            fields.rows.append(item)
        fields.total = label("",column,18)
        fields.total.add_theme_color_override("font_color",Color("ffcb00"))
        var bonus_row := HBoxContainer.new()
        column.add_child(bonus_row)
        fields.bonus = label("",bonus_row,14)
        fields.bonus.size_flags_horizontal = Control.SIZE_EXPAND_FILL
        fields.bonus.add_theme_color_override("font_color",Color("8ed69a"))
        fields.stage = label("",bonus_row,14)
        fields.stage.add_theme_color_override("font_color",Color("ffe077"))
        var score_row := HBoxContainer.new()
        column.add_child(score_row)
        fields.score = label("",score_row,16)
        fields.score.size_flags_horizontal = Control.SIZE_EXPAND_FILL
        fields.lives = label("",score_row,16)
        report_card_labels.append(fields)
    report_text = label("",report_panel,16)
    report_text.hide()
    record_page = Control.new()
    record_page.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
    report_panel.add_child(record_page)
    var record_caption := label("NEW RECORD",record_page,28)
    record_caption.set_anchors_and_offsets_preset(Control.PRESET_CENTER)
    record_caption.offset_left = -400
    record_caption.offset_right = 400
    record_caption.offset_top = -164
    record_caption.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
    record_caption.add_theme_color_override("font_color",Color("ffcb00"))
    var hiscore := label("HISCORE",record_page,76)
    hiscore.set_anchors_and_offsets_preset(Control.PRESET_CENTER)
    hiscore.offset_left = -400
    hiscore.offset_right = 400
    hiscore.offset_top = -105
    hiscore.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
    hiscore.add_theme_color_override("font_color",Color("ffdf62"))
    record_score = label("",record_page,58)
    record_score.set_anchors_and_offsets_preset(Control.PRESET_CENTER)
    record_score.offset_left = -400
    record_score.offset_right = 400
    record_score.offset_top = 5
    record_score.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
    record_score.add_theme_color_override("font_color",Color("ffdf62"))
    report_continue = button("BOTTOM FACE / ENTER: CONTINUE",report_panel,func(): confirm_requested.emit())
    report_continue.action_mode = BaseButton.ACTION_MODE_BUTTON_PRESS
    report_continue.set_anchors_and_offsets_preset(Control.PRESET_BOTTOM_WIDE)
    report_continue.offset_left = 180
    report_continue.offset_right = -180
    report_continue.offset_top = -56
    report_continue.offset_bottom = -12

func report_line(parent: Node, font_size: int) -> Dictionary:
    var line := HBoxContainer.new()
    parent.add_child(line)
    var name_label := label("",line,font_size)
    name_label.size_flags_horizontal = Control.SIZE_EXPAND_FILL
    var kills := label("",line,font_size)
    kills.custom_minimum_size.x = 75
    kills.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
    var points := label("",line,font_size)
    points.custom_minimum_size.x = 112
    points.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT
    return {"name":name_label,"kills":kills,"points":points}

func refresh(value: Dictionary, pixel_value: bool, dt: float, network: Dictionary = {}, reset_feedback: bool = false) -> void:
    if menu_open: return
    var players: Array = value.get("players",[])
    var count := clampi(int(value.get("player_count",players.size())),0,mini(2,players.size()))
    for slot in count:
        refresh_player_hud(slot,players[slot],value)
    p2_card.visible = players.size()>1 and int(value.get("player_count",players.size()))>1
    var base_status := "HEADQUARTERS" if value.get("base_alive",true) else "BASE LOST"
    if float(value.get("base_steel_remaining",0))>0: base_status = "BASE STEEL %.1fs"%float(value.base_steel_remaining)
    stage_label.text = "STAGE %02d  ENEMY %02d\n%s"%[int(value.get("stage",1)),int(value.get("enemies_left",20)),base_status]
    pause_style.text = "PIXEL STYLE %s"%["ON" if pixel_value else "OFF"]
    refresh_control_hints(value,network)
    refresh_combat_feedback(value,dt,reset_feedback)
    bonus_message.text = str(value.get("bonus_message","")) if float(value.get("bonus_message_remaining",0))>0 else ""
    radar.update_state(value,dt)
    var paused: bool = value.get("paused",false)
    if paused and not pause_panel.visible:
        pause_panel.show()
        resume_button.grab_focus()
    elif not paused and pause_panel.visible:
        pause_panel.hide()
        var focus := root.get_viewport().gui_get_focus_owner()
        if focus != null: focus.release_focus()
    restart_button.disabled = not network.is_empty() and network.get("role","")=="guest"
    var reporting: bool = value.get("settling",false) or value.get("high_score",false)
    if reporting and not report_panel.visible:
        report_panel.show()
        report_continue.grab_focus()
    elif not reporting: report_panel.hide()
    if reporting: update_report(value)
    modal_message.text = ""
    if value.get("intro",false): modal_message.text = "STAGE %02d\nGET READY"%int(value.stage)
    elif value.get("game_over",false) and not reporting: modal_message.text = "GAME OVER\nBATTLE REPORT SOON"
    elif value.get("stage_transition",false) and not reporting: modal_message.text = "STAGE CLEAR"

func update_report(value: Dictionary) -> void:
    var report: Dictionary = value.get("report",{})
    var high_score: bool = value.get("high_score",false)
    report_column.visible = not high_score
    record_page.visible = high_score
    report_preview.visible = not high_score
    report_title.text = "NEW RECORD" if high_score else ("FINAL BATTLE REPORT" if report.get("game_over",false) else "STAGE BATTLE REPORT")
    report_record.text = "HI- %d"%int(report.get("high_score",0))
    report_stage.text = "STAGE %d"%int(report.get("stage",value.get("stage",1)))
    record_score.text = str(int(report.get("high_score",0)))
    if not high_score:
        var roster: Array = value.get("players",[])
        report_preview.set_players(roster.slice(0,clampi(int(value.get("player_count",roster.size())),0,roster.size())))
    var native_players: Array = value.get("players",[])
    var tally: Array = report.get("players",[])
    var combined: Array[String] = [report_stage.text,report_record.text]
    for slot in 2:
        var shown := slot<tally.size() and not high_score
        report_cards[slot].visible = shown
        if not shown: continue
        report_cards[slot].custom_minimum_size.x = 720 if tally.size()==1 else 0
        report_cards[slot].size_flags_horizontal = Control.SIZE_SHRINK_CENTER if tally.size()==1 else Control.SIZE_EXPAND_FILL
        var data: Dictionary = tally[slot]
        var player: Dictionary = native_players[slot] if slot<native_players.size() else {}
        var fields: Dictionary = report_card_labels[slot]
        fields.header.text = "P%d PLAYER"%(slot+1)
        fields.vehicle.text = str(player.get("vehicle_name","TANK"))
        var kills: Array = data.get("displayed_kills",[0,0,0,0])
        var points: Array = data.get("points_by_type",[0,0,0,0])
        var total := 0
        for type in 4:
            fields.rows[type].kills.text = "x %d"%int(kills[type])
            fields.rows[type].points.text = str(int(points[type]))
            total += int(kills[type])
        fields.total.text = "TOTAL   x %d      %d"%[total,int(data.get("enemy_points",0))]
        fields.bonus.text = "BONUS %d"%int(data.get("bonus_points",0))
        fields.stage.text = "STAGE +%d"%int(data.get("stage_points",0))
        fields.score.text = "SCORE %d"%mini(int(data.get("score",0)),int(report.get("score_counter",0)))
        fields.lives.text = "LIVES x%d"%maxi(0,int(player.get("lives",0)))
        for key in ["header","vehicle","total","bonus","stage","score","lives"]: combined.append(fields[key].text)
    report_text.text = "\n".join(combined)
    report_continue.text = "RETURN TO DEPLOYMENT" if high_score else ("BOTTOM FACE / ENTER: COUNT NOW" if value.get("settlement_counting",false) else "BOTTOM FACE / ENTER: CONTINUE")
