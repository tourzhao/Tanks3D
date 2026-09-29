extends RefCounted
## Presentation contracts against the existing native minimap and report data.
## Synthetic fixtures here never enter the gameplay core.
const Radar = preload("res://radar.gd")
const Art = preload("res://art.gd")
var failures: Array[String] = []


func expect(condition: bool, detail: String) -> bool:
    if not condition:
        failures.append(detail)
        push_error("Raylib UI parity: "+detail)
    return condition


func of_kind(markers: Array[Dictionary], kind: String) -> Array[Dictionary]:
    return markers.filter(func(marker: Dictionary): return marker.kind == kind)


func radar_contracts() -> bool:
    var radar := Radar.new()
    radar.size = Vector2(168,196)
    expect(radar.field_rect() == Rect2(19,50,130,130),
        "normal radar grid differs from raylib's five-pixel field or origin")
    # Every pixel belongs to exactly its surviving native quadrant, including
    # odd cell widths. This detects half-pixel seams, overlap and lost damage.
    for cell_size in [4,5,6]:
        var tile := Rect2(19,50,cell_size,cell_size)
        var split := int(cell_size/2.0)
        for mask in 16:
            var pixel_rects: Array[Rect2] = []
            for quadrant in Radar.brick_quadrants(mask):
                var rect := Radar.brick_pixel_rect(tile,quadrant)
                pixel_rects.append(rect)
                expect(tile.encloses(rect) and rect.position == rect.position.floor() and
                    rect.end == rect.end.floor(),"damaged brick has a fractional or out-of-tile edge")
            for y in cell_size:
                for x in cell_size:
                    var point := tile.position+Vector2(x+.5,y+.5)
                    var hits := 0
                    for rect in pixel_rects:
                        if rect.has_point(point): hits += 1
                    var bit := (1 if x >= split else 0)+(2 if y >= split else 0)
                    expect(hits == (1 if mask&(1<<bit) else 0),
                        "radar brick pixels disagree with native surviving quadrant")
    # A partially destroyed tile must retain precisely its surviving footprint,
    # not just change its tint or remain an apparently impassable solid square.
    for mask in 16:
        var quadrants := Radar.brick_quadrants(mask)
        for y in 2:
            for x in 2:
                var hits := 0
                var center := Vector2(.25+x*.5,.25+y*.5)
                for rect in quadrants:
                    expect(Rect2(0,0,1,1).encloses(rect),"brick marker escapes its tile")
                    if rect.has_point(center): hits += 1
                expect(hits == (1 if mask&(1<<(y*2+x)) else 0),"brick quadrant damage is not represented")
    var value := {"base_alive":true,"base_nation":0,"base_steel":false,
        "base_walls":[4,3,2,1,0,4,4,4],"players":[
            {"id":0,"x":8.0,"z":25.0,"active":true},
            {"id":1,"x":16.0,"z":25.0,"active":true}],
        "enemies":[{"x":1.0,"z":1.0,"creating":.95,"destroyed":false},
            {"x":13.0,"z":1.0,"creating":0.0,"destroyed":false,"carries_bonus":true},
            {"x":25.0,"z":1.0,"creating":0.0,"destroyed":false,"carries_bonus":false},
            {"x":20.0,"z":1.0,"creating":0.0,"destroyed":true}],"pickups":[]}
    radar.update_state(value,0)
    var markers := radar.markers()
    var walls := of_kind(markers,"wall")
    expect(walls.size() == 7,"destroyed base wall remains on radar")
    expect(walls[0].position == Vector2(11,23.5) and walls[0].end == Vector2(12,23.5) and
        walls[4].position == Vector2(14.5,24) and walls[4].end == Vector2(14.5,25),
        "base enclosure orientation or segment position changed")
    expect(walls[0].color != walls[1].color and walls[1].color != walls[2].color and
        walls[2].color != walls[3].color,"base wall HP is not distinguishable")
    var actors := of_kind(markers,"player")
    expect(actors.size() == 2 and actors[0].color == Color("ec9118") and actors[1].color == Color("24cd5e"),
        "P1 gold/P2 green identity differs from original minimap")
    var enemies := of_kind(markers,"enemy")
    expect(enemies.size() == 2 and enemies[0].color != enemies[1].color and
        of_kind(markers,"creating").size() == 1,"spawn warning/carrier/destroyed enemy distinctions missing")
    var original_star: Dictionary = of_kind(markers,"creating")[0]
    value.enemies[0].creating = .65
    radar.update_state(value,0)
    var pulse: Dictionary = of_kind(radar.markers(),"creating")[0]
    expect(pulse.radius != original_star.radius and pulse.color != original_star.color,
        "enemy warning no longer pulses across native creation frames")
    value.base_steel = true
    radar.update_state(value,0)
    walls = of_kind(radar.markers(),"wall")
    expect(walls.all(func(wall: Dictionary): return wall.color == Color("8bdaeb")),"steel enclosure does not override brick HP tint")
    var bases: Array[Color] = []
    for nation in 3:
        value.base_nation = nation
        radar.update_state(value,0)
        bases.append(of_kind(radar.markers(),"base")[0].color)
    expect(bases[0] != bases[1] and bases[1] != bases[2] and bases[0] != bases[2],"national headquarters colors collapse")
    value.base_alive = false
    value.players[1].active = false
    radar.update_state(value,0)
    expect(of_kind(radar.markers(),"base")[0].color == Color("505050") and
        of_kind(radar.markers(),"player").size() == 1,"destroyed base/inactive player state missing")
    for kind in 9:
        value.pickups = [{"type":kind,"x":10.0,"z":12.0,"age":0.0}]
        radar.update_state(value,0)
        var pickup := of_kind(radar.markers(),"pickup")
        expect(pickup.size() == 1 and pickup[0].star_scale == 1.0,"pickup star missing")
        for sample in [[.36,false],[.71,true],[9.4,false],[9.5,true]]:
            value.pickups[0].age = sample[0]
            radar.update_state(value,0)
            expect((of_kind(radar.markers(),"pickup").size() == 1) == sample[1],"pickup slow/fast blink differs from native presentation")
    var before := JSON.stringify(value)
    radar.markers()
    expect(JSON.stringify(value) == before,"radar mutates native presentation snapshot")
    radar.free()
    return failures.is_empty()


func visible_number(label: Label, value: int, detail: String) -> bool:
    var expression := RegEx.new()
    expression.compile("(^|[^0-9])0*"+str(value)+"([^0-9]|$)")
    return expect(label.is_visible_in_tree() and expression.search(label.text) != null,detail)


func report_contracts(frontend: CanvasLayer, value: Dictionary) -> bool:
    if not expect(frontend.report_panel.is_visible_in_tree(),"actual native report is not visible"): return false
    var tallies: Array = value.report.players
    var count := int(value.player_count)
    var models: Array[Node] = frontend.report_preview.lineup.get_children()
    expect(models.size() == count and tallies.size() == count,
        "report preview includes inactive fixed native player slots")
    for slot in mini(models.size(),count):
        var player: Dictionary = value.players[slot]
        # Art uses short display names (e.g. T-34/85) while native HUD names
        # retain historical designations. Compare the actual cached geometry,
        # selected from the native nation/id/level, rather than those labels.
        var expected: Node3D = Art.make_tank(int(player.nation),false,0,int(player.id),int(player.level))
        expect(not bool(models[slot].get_meta("enemy",true)),"report preview substitutes an enemy model")
        for part in ["Body","Hull"]:
            var actual_mesh := models[slot].get_node_or_null(part) as MeshInstance3D
            var expected_mesh := expected.get_node_or_null(part) as MeshInstance3D
            expect(actual_mesh != null and expected_mesh != null and actual_mesh.mesh == expected_mesh.mesh,
                "report preview selects wrong native nation/id/level geometry: "+part)
        expected.free()
    expect(frontend.report_card_labels.size() >= tallies.size(),"report player cards missing")
    for slot in tallies.size():
        var tally: Dictionary = tallies[slot]
        var labels: Dictionary = frontend.report_card_labels[slot]
        var player: Dictionary = value.players[slot]
        expect(frontend.report_cards[slot].is_visible_in_tree(),"report player card hidden")
        expect(labels.header.is_visible_in_tree() and labels.header.text.contains("P%d"%(slot+1)) and
            labels.vehicle.is_visible_in_tree() and labels.vehicle.text.contains(player.vehicle_name),
            "report drops player/model identity")
        var total := 0
        for type in 4:
            var row: Dictionary = labels.rows[type]
            expect(row.name.is_visible_in_tree() and row.name.text.contains(["BASIC","FAST","POWER","ARMOR"][type]),
                "report enemy class label hidden")
            visible_number(row.kills,int(tally.displayed_kills[type]),"report kill row ignores native counting animation")
            visible_number(row.points,int(tally.points_by_type[type]),"report points row does not use native class tally")
            total += int(tally.displayed_kills[type])
        visible_number(labels.total,total,"report total ignores animated kills")
        visible_number(labels.total,int(tally.enemy_points),"report combat point total missing")
        visible_number(labels.bonus,int(tally.bonus_points),"report pickup awards missing")
        visible_number(labels.stage,int(tally.stage_points),"report stage points missing")
        visible_number(labels.score,mini(int(tally.score),int(value.report.score_counter)),"report score ignores native score counter")
        visible_number(labels.lives,maxi(0,int(player.lives)),"report remaining lives missing")
    if tallies.size() == 1 and frontend.report_cards.size() > 1:
        expect(not frontend.report_cards[1].is_visible_in_tree(),"solo report displays nonexistent P2 tally")
    return failures.is_empty()
