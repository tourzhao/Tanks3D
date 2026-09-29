extends PanelContainer
# Original code-drawn military arcade plates. No external font or texture.
var accent := Color("d2ab61")
var emblem_only := false
var padding_y := 8.0
var health_color := Color(0,0,0,0)

func _ready() -> void:
    mouse_filter = Control.MOUSE_FILTER_IGNORE
    var space := StyleBoxEmpty.new()
    space.content_margin_left = 14
    space.content_margin_right = 14
    space.content_margin_top = padding_y
    space.content_margin_bottom = padding_y
    add_theme_stylebox_override("panel",space)
    resized.connect(queue_redraw)
    queue_redraw()

func _draw() -> void:
    if emblem_only:
        draw_emblem()
        return
    var w := size.x
    var h := size.y
    var cut := minf(11.0,h*.18)
    var points := PackedVector2Array([Vector2(cut,0),Vector2(w,0),Vector2(w,h-cut),
        Vector2(w-cut,h),Vector2(0,h),Vector2(0,cut)])
    draw_colored_polygon(points,Color(.055,.085,.092,.94))
    var edge := points.duplicate()
    edge.append(points[0])
    draw_polyline(edge,Color("536367"),1.0,true)
    draw_line(Vector2(cut,1),Vector2(w*.38,1),accent,2.0)
    draw_line(Vector2(1,cut),Vector2(1,h-10),accent,3.0)
    draw_line(Vector2(w-22,h-2),Vector2(w-cut,h-2),accent,2.0)
    draw_line(Vector2(12,h-4),Vector2(33,h-4),Color("263a3c"),2.0)
    if health_color.a > 0:
        draw_rect(Rect2(7,26,3,20),health_color)

func draw_emblem() -> void:
    var center := size*.5
    var s := minf(size.x/250.0,size.y/150.0)
    var ink := Color("34494a")
    var metal := Color("78877a")
    var light := Color("d9bb75")
    draw_set_transform(center,0,Vector2.ONE*s)
    for side in [-1.0,1.0]:
        var x: float = side*58
        draw_colored_polygon(PackedVector2Array([Vector2(x-16,-21),Vector2(x+16,-21),Vector2(x+19,44),Vector2(x+9,53),Vector2(x-14,53),Vector2(x-20,42)]),ink)
        for y in [-9,7,23,39]:
            draw_line(Vector2(x-11,y),Vector2(x+11,y),metal,4.0)
    draw_colored_polygon(PackedVector2Array([Vector2(-47,-24),Vector2(44,-24),Vector2(58,22),Vector2(45,39),Vector2(-49,39),Vector2(-60,20)]),metal)
    draw_colored_polygon(PackedVector2Array([Vector2(-35,-30),Vector2(-22,-48),Vector2(27,-43),Vector2(39,-19),Vector2(28,8),Vector2(-30,8)]),light)
    draw_rect(Rect2(-10,-70,20,47),ink)
    draw_rect(Rect2(-8,-72,16,8),light)
    draw_rect(Rect2(-22,-21,44,9),ink)
    draw_line(Vector2(-31,22),Vector2(31,22),ink,4)
    draw_set_transform(Vector2.ZERO)
