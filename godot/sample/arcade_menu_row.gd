extends Button
# Code-drawn row matching the established raylib setup navigation.
var caption := ""
var value_text := ""
var adjustable := true
var value_tint := Color("d3d8d6")

func _ready() -> void:
    custom_minimum_size.y = 42
    for kind in ["normal","hover","pressed","focus","disabled"]:
        add_theme_stylebox_override(kind,StyleBoxEmpty.new())
    focus_entered.connect(queue_redraw)
    focus_exited.connect(queue_redraw)
    mouse_entered.connect(queue_redraw)
    mouse_exited.connect(queue_redraw)
    resized.connect(queue_redraw)

func set_value(value: String) -> void:
    if value_text != value:
        value_text = value
        queue_redraw()

func _draw() -> void:
    var selected := has_focus()
    if selected or is_hovered():
        draw_rect(Rect2(0,1,size.x,size.y-3),Color("2a484c") if selected else Color("1a333c"))
    var font := get_theme_font("font")
    var caption_size := 20
    var value_size := 22
    while value_size > 12 and font.get_string_size(value_text,HORIZONTAL_ALIGNMENT_LEFT,-1,value_size).x > 266:
        value_size -= 1
    var baseline := (size.y-font.get_height(caption_size))*.5+font.get_ascent(caption_size)
    draw_string(font,Vector2(18,baseline),("> " if selected else "  ")+caption,HORIZONTAL_ALIGNMENT_LEFT,-1,caption_size,Color("f2f2e8") if selected else Color("aab9bc"))
    var value_width := font.get_string_size(value_text,HORIZONTAL_ALIGNMENT_LEFT,-1,value_size).x
    var value_x := size.x-171-value_width*.5
    draw_string(font,Vector2(value_x,(size.y-font.get_height(value_size))*.5+font.get_ascent(value_size)),value_text,HORIZONTAL_ALIGNMENT_LEFT,-1,value_size,Color("ffdf62") if selected else value_tint)
    if selected and adjustable:
        draw_string(font,Vector2(size.x-321,baseline),"<",HORIZONTAL_ALIGNMENT_LEFT,-1,21,Color("ffdf62"))
        draw_string(font,Vector2(size.x-35,baseline),">",HORIZONTAL_ALIGNMENT_LEFT,-1,21,Color("ffdf62"))
