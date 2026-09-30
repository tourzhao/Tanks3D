extends Control
var background: GradientTexture2D
func _ready() -> void:
    mouse_filter = Control.MOUSE_FILTER_IGNORE
    set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
    var gradient := Gradient.new()
    gradient.colors = PackedColorArray([Color("1c343f"),Color("070b0f")])
    background = GradientTexture2D.new()
    background.gradient = gradient
    background.width = 2
    background.height = 512
    background.fill_from = Vector2(.5,0)
    background.fill_to = Vector2(.5,1)
    resized.connect(queue_redraw)
func _draw() -> void:
    if background != null: draw_texture_rect(background,Rect2(Vector2.ZERO,size),false)
    for i in 16:
        var y := size.y-40-i*25
        draw_line(Vector2(0,y),Vector2(size.x,y-size.x/5),Color(.32,.49,.52,.12),1)
