extends Control

# Presentation mapping mirrors src/main.cpp::drawMiniMap. Coordinates remain
# native map units; no camera or simulation state is adjusted here.
const WALL_CELLS := [Vector2(11,23),Vector2(12,23),Vector2(13,23),Vector2(14,23),
    Vector2(11,24),Vector2(14,24),Vector2(11,25),Vector2(14,25)]
const PICKUP_COLORS := [Color("ff6837"),Color("55cdff"),Color("ffda4a"),Color("aecee0"),
    Color("57eb7e"),Color("ffdd3e"),Color("ff912f"),Color("4abfff"),Color("2adcbe")]
const CREATION_SCALES := [.25,.45,.70,1.0,.70,.45,.25,.45,.70,1.0]
var snapshot: Dictionary = {}
var refresh_clock := 0.0
var frame_style: StyleBoxFlat


func update_state(value: Dictionary, dt: float) -> void:
    snapshot = value
    refresh_clock += dt
    if refresh_clock >= 0.10 or dt == 0.0:
        refresh_clock = 0.0
        queue_redraw()


func field_rect() -> Rect2:
    # Whole pixels keep the radar legible and match the native five-pixel grid.
    var cell_size := maxf(1.0,floorf(minf((size.x-24.0)/26.0,(size.y-60.0)/26.0)))
    return Rect2(Vector2(floorf((size.x-cell_size*26.0)*.5),50),Vector2.ONE*cell_size*26.0)


static func brick_pixel_rect(cell: Rect2, quadrant: Rect2) -> Rect2:
    # Odd-width tiles split 2+3 pixels, as in raylib; no half-pixel slivers.
    var first := (cell.position+quadrant.position*cell.size).floor()
    var last := (cell.position+quadrant.end*cell.size).floor()
    return Rect2(first,last-first)


func _draw() -> void:
    draw_style_box(panel_style(), Rect2(Vector2.ZERO, size))
    var field := field_rect()
    var scale_value := field.size.x/26.0
    var origin := field.position
    var rows: Array = snapshot.get("map", [])
    var masks: Array = snapshot.get("brick_masks", [])
    for row in range(rows.size()):
        var cells: String = rows[row]
        for column in range(cells.length()):
            var cell := Rect2(origin+Vector2(column,row)*scale_value,Vector2.ONE*scale_value)
            var tile := cells[column]
            if tile == "#":
                draw_rect(cell,tile_color("."))
                var index := row*26+column
                var mask := int(masks[index]) if index < masks.size() else 15
                for quadrant in brick_quadrants(mask):
                    draw_rect(brick_pixel_rect(cell,quadrant),tile_color("#"))
            else:
                draw_rect(cell,tile_color(tile))
    for marker in markers():
        var point: Vector2 = origin+marker.position*scale_value
        match marker.kind:
            "wall": draw_line(point,origin+marker.end*scale_value,marker.color,maxf(2,scale_value))
            "base": draw_rect(Rect2(point,Vector2.ONE*scale_value*2),marker.color)
            "player", "enemy": draw_circle(point,maxf(2,scale_value*marker.radius),marker.color)
            "pickup", "creating":
                var radius := maxf(marker.minimum,scale_value*marker.radius)
                draw_arc(point,radius,0,TAU,24,marker.color,1.0)
                draw_star(point,radius*marker.star_scale,Color("ffd536"))
                if marker.kind == "pickup": draw_star(point,radius*.58,Color("fffcd3"))
    draw_rect(field.grow(2),Color("647776"),false,1.0)
    for edge in [Vector2(0,0),Vector2(1,0),Vector2(0,1),Vector2(1,1)]:
        var point: Vector2 = field.position+field.size*edge
        var direction := Vector2(1 if edge.x == 0 else -1,1 if edge.y == 0 else -1)
        draw_line(point,point+Vector2(direction.x*8,0),Color("d2ab61"),2.0)
        draw_line(point,point+Vector2(0,direction.y*8),Color("d2ab61"),2.0)


func panel_style() -> StyleBoxFlat:
    if frame_style == null:
        frame_style = StyleBoxFlat.new()
        frame_style.bg_color = Color(.055,.085,.092,.92)
        frame_style.border_color = Color("536367")
        frame_style.set_border_width_all(1)
    return frame_style


static func tile_color(tile: String) -> Color:
    match tile:
        "#": return Color("953f2d")
        "@": return Color("8497a2")
        "%": return Color("247432")
        "~": return Color("2276be")
        "-": return Color("9fd9e3")
        _: return Color("373d36")


static func brick_quadrants(mask: int) -> Array[Rect2]:
    var result: Array[Rect2] = []
    for quadrant in 4:
        if mask & (1 << quadrant):
            result.append(Rect2(Vector2(quadrant&1,(quadrant>>1)&1)*.5,Vector2(.5,.5)))
    return result


func markers() -> Array[Dictionary]:
    # This is the same draw list consumed by _draw, allowing contracts to check
    # distinctions that otherwise disappear in a headless screenshot.
    var result: Array[Dictionary] = []
    var walls: Array = snapshot.get("base_walls",[])
    for index in mini(walls.size(),WALL_CELLS.size()):
        var health := int(walls[index])
        if health <= 0: continue
        var cell: Vector2 = WALL_CELLS[index]
        var along := Vector2.RIGHT if cell.y == 23 else Vector2.DOWN
        var center := cell+Vector2(.5,.5)
        var color := Color("8bdaeb") if snapshot.get("base_steel",false) else \
            Color("ae5836").lerp(Color("e4dbb8"),clampf(health/4.0,0,1))
        result.append({"kind":"wall","position":center-along*.5,"end":center+along*.5,"color":color})
    var base_color := Color("505050")
    if snapshot.get("base_alive",false):
        base_color = [Color("ffcb00"),Color("de4136"),Color("b4a487")][clampi(int(snapshot.get("base_nation",0)),0,2)]
    result.append({"kind":"base","position":Vector2(12,24),"color":base_color})
    for pickup in snapshot.get("pickups",[]):
        var age := float(pickup.age)
        var interval := .35 if age < 9.375 else .175
        if int(age/interval)%2 != 0: continue
        result.append({"kind":"pickup","position":Vector2(pickup.x,pickup.z),
            "color":PICKUP_COLORS[clampi(int(pickup.type),0,8)],
            "radius":1.08*(.82+.18*sin(age*9)),"minimum":4.5,"star_scale":1.0})
    for enemy in snapshot.get("enemies",[]):
        if enemy.get("destroyed",false): continue
        var point := Vector2(enemy.x,enemy.z)
        if float(enemy.get("creating",0)) > 0:
            var frame := clampi(int(clampf(1.0-float(enemy.creating),0,.9999)/.1),0,9)
            result.append({"kind":"creating","position":point,"radius":.45+CREATION_SCALES[frame]*.28,
                "minimum":2.5,"star_scale":.72,"color":Color("ff55cd") if frame%2 == 0 else Color("ffe04a")})
        else:
            result.append({"kind":"enemy","position":point,"radius":.72,
                "color":Color("ffcb00") if enemy.get("carries_bonus",false) else Color("e62937")})
    for player in snapshot.get("players",[]):
        if player.active:
            result.append({"kind":"player","position":Vector2(player.x,player.z),"radius":.78,
                "color":Color("ec9118") if int(player.id) == 0 else Color("24cd5e")})
    return result


func draw_star(center: Vector2, radius: float, color: Color) -> void:
    var points := PackedVector2Array()
    for point in 10:
        var angle := -PI*.5+point*TAU/10.0
        points.append(center+Vector2(cos(angle),sin(angle))*radius*(1.0 if point%2 == 0 else .38))
    draw_colored_polygon(points,color)
