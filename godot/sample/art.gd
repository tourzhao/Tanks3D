class_name SampleArt
extends RefCounted
## Original Godot arcade art, PolyForm Noncommercial 1.0.0.
## Authored silhouettes and surfaces; legacy models inform identity/mounts only.
## Local forward is -Z. Production yaw must be negated by the scene adapter.
## All geometry is visual only; collisions and attachment rules remain in C++.

static var _meshes: Dictionary = {}
static var _materials: Dictionary = {}
static var _mesh_groups: Dictionary = {}
static var _gear_cache: Dictionary = {}
const GEAR_FRAME_COUNT := 16
const GEAR_SHOE_COUNT := 10
const GEAR_SPOKE_COUNT := 3
# User-requested breadth is authored into armor and track profiles. Circular
# fittings and suspension keep rigid transforms instead of stretching the root.
const VEHICLE_WIDTH_SCALE := 1.4784
# Broaden the stance more than the turret; retain the individual cast/welded
# shapes and keep circular gun sections independent of chassis proportions.
const TURRET_WIDTH_SCALE := 1.272
const GUN_RADIUS_SCALE := 1.781
# Add chassis mass around the existing turret seat, not by lifting the turret
# or gun mount. A small deck rise and deeper lower armor retain ground clearance.
const HULL_DECK_SCALE := 1.04
const HULL_DEPTH_SCALE := 1.10
const HULL_MIN_CLEARANCE := .066
# Army woodland olive, Soviet winter whitewash and matte German Panzer gray.
# Lighting, mechanical materials and player markings remain independent.
const NATIONAL_PAINT := [Color("55613d"),Color("b8bcaf"),Color("45494b")]
static var _visual_time: float = 0.0

static func set_visual_time(seconds: float) -> void:
    _visual_time = seconds
    if _materials.has("water"):
        _materials["water"].set_shader_parameter("visual_time",seconds)

class Geometry:
    var surfaces: Dictionary = {}

    func tri(a: Vector3, b: Vector3, c: Vector3, color: Color, group: String = "paint", normal := Vector3.ZERO) -> void:
        if not surfaces.has(group):
            var stream := SurfaceTool.new()
            stream.begin(Mesh.PRIMITIVE_TRIANGLES)
            surfaces[group] = stream
        var n := normal if normal != Vector3.ZERO else (b - a).cross(c - a).normalized()
        var stream: SurfaceTool = surfaces[group]
        # Author panels counterclockwise with an outward normal, then reverse
        # only index order for Godot's clockwise front-face convention.
        var tint := color
        if group == "paint" or group == "armor":
            var light := maxf(n.y, 0.0)
            var shade := maxf(-n.y, 0.0)
            tint = color.lerp(Color(0.84, 0.79, 0.56), light * 0.13)
            tint = tint.lerp(Color(0.13, 0.21, 0.22), shade * 0.25)
        for point in [a, c, b]:
            stream.set_normal(n)
            stream.set_color(tint)
            stream.add_vertex(point)

    func quad(a: Vector3, b: Vector3, c: Vector3, d: Vector3, color: Color, group: String = "paint") -> void:
        var n := (b - a).cross(d - a).normalized()
        tri(a, b, c, color, group, n)
        tri(a, c, d, color, group, n)

    func box(center: Vector3, size: Vector3, color: Color, group: String = "paint") -> void:
        var a := center - size * 0.5
        var b := center + size * 0.5
        quad(Vector3(a.x,b.y,a.z),Vector3(a.x,b.y,b.z),Vector3(b.x,b.y,b.z),Vector3(b.x,b.y,a.z),color,group)
        quad(Vector3(a.x,a.y,b.z),Vector3(a.x,a.y,a.z),Vector3(b.x,a.y,a.z),Vector3(b.x,a.y,b.z),color,group)
        quad(Vector3(a.x,a.y,a.z),Vector3(a.x,b.y,a.z),Vector3(b.x,b.y,a.z),Vector3(b.x,a.y,a.z),color,group)
        quad(Vector3(b.x,a.y,b.z),Vector3(b.x,b.y,b.z),Vector3(a.x,b.y,b.z),Vector3(a.x,a.y,b.z),color,group)
        quad(Vector3(b.x,a.y,a.z),Vector3(b.x,b.y,a.z),Vector3(b.x,b.y,b.z),Vector3(b.x,a.y,b.z),color,group)
        quad(Vector3(a.x,a.y,b.z),Vector3(a.x,b.y,b.z),Vector3(a.x,b.y,a.z),Vector3(a.x,a.y,a.z),color,group)

    func loft(sections: Array, color: Color, offset := Vector3.ZERO, group: String = "paint") -> void:
        var rings: Array = []
        for s in sections:
            var y: float = s[0]
            var x: float = s[1]
            var f: float = s[2]
            var r: float = s[3]
            var c: float = s[4]
            var ring: Array[Vector3] = [Vector3(-x+c,y,f),Vector3(x-c,y,f),
                Vector3(x-c*.28,y,f+c*.28),Vector3(x,y,f+c),Vector3(x,y,r-c),
                Vector3(x-c*.28,y,r-c*.28),Vector3(x-c,y,r),Vector3(-x+c,y,r),
                Vector3(-x+c*.28,y,r-c*.28),Vector3(-x,y,r-c),Vector3(-x,y,f+c),
                Vector3(-x+c*.28,y,f+c*.28)]
            for i in ring.size():
                ring[i] += offset
            rings.append(ring)
        for level in range(rings.size() - 1):
            for i in 12:
                var j := (i + 1) % 12
                quad(rings[level][j],rings[level][i],rings[level+1][i],rings[level+1][j],color,group)
        var bottom: Array = rings.front()
        var top: Array = rings.back()
        for i in range(1, 11):
            tri(top[0],top[i+1],top[i],color,group)
            tri(bottom[0],bottom[i],bottom[i+1],color.darkened(.2),group)

    func tube(a: Vector3, b: Vector3, radius_a: float, radius_b: float, color: Color, group: String = "metal", count: int = 10, phase: float = 0.0) -> void:
        var axis := (b-a).normalized()
        var u := axis.cross(Vector3.RIGHT if absf(axis.x) < .8 else Vector3.UP).normalized()
        var v := axis.cross(u)
        for i in count:
            var t := TAU * i / count + phase
            var q := TAU * (i+1) / count + phase
            var d0 := u*cos(t)+v*sin(t)
            var d1 := u*cos(q)+v*sin(q)
            quad(a+d0*radius_a,a+d1*radius_a,b+d1*radius_b,b+d0*radius_b,color,group)
            tri(a,a+d1*radius_a,a+d0*radius_a,color,group)
            tri(b,b+d0*radius_b,b+d1*radius_b,color,group)

    static func armor_ring(s: Array, plan: Array) -> Array[Vector3]:
        # Separate fore/aft shoulder stations and breadth: a long welded cheek
        # is not the same construction as a small rear corner chamfer.
        var y: float = s[0]
        var w: float = s[1]
        var f: float = s[2]
        var r: float = s[3]
        var fx: float = w*plan[0]
        var fz: float = (r-f)*plan[1]
        var rx: float = w*plan[2]
        var rz: float = (r-f)*plan[3]
        var qf: float = plan[4]
        var qr: float = plan[5]
        var rw: float = w*plan[6]
        return [Vector3(-w+fx,y,f),Vector3(w-fx,y,f),
            Vector3(w-fx*qf,y,f+fz*qf),Vector3(w,y,f+fz),
            Vector3(rw,y,r-rz),Vector3(rw-rx*qr,y,r-rz*qr),
            Vector3(rw-rx,y,r),Vector3(-rw+rx,y,r),
            Vector3(-rw+rx*qr,y,r-rz*qr),Vector3(-rw,y,r-rz),
            Vector3(-w,y,f+fz),Vector3(-w+fx*qf,y,f+fz*qf)]

    static func armor_rings(sections: Array, plan: Array) -> Array:
        var rings: Array = []
        for section in sections: rings.append(armor_ring(section,plan))
        if plan[4] != .5 or plan[5] != .5: return rings
        # A welded plate must be planar. Independently scaling X/Z corners
        # rotates its edges between height rings and creates twisted quads.
        # Define parallel XZ boundary lines from the main shoulder, then solve
        # adjacent lines at each height. The front/rear and widest shoulders
        # remain anchored; the real vertices, not only normals, are corrected.
        const CORNERS := [0,1,3,4,6,7,9,10]
        const ANCHORS := [0,2,2,3,4,6,7,7]
        var normals: Array[Vector2] = []
        for i in 8:
            var edge: Vector3 = rings[1][CORNERS[(i+1)%8]]-rings[1][CORNERS[i]]
            normals.append(Vector2(edge.z,-edge.x).normalized())
        for level in rings.size():
            var raw: Array = rings[level]
            var distances: Array[float] = []
            for i in 8:
                var anchor: Vector3 = raw[CORNERS[ANCHORS[i]]]
                distances.append(normals[i].dot(Vector2(anchor.x,anchor.z)))
            var corners: Array[Vector3] = []
            for i in 8:
                var previous := (i+7)%8
                var a: Vector2 = normals[previous]
                var b: Vector2 = normals[i]
                var determinant := a.cross(b)
                corners.append(Vector3((distances[previous]*b.y-a.y*distances[i])/determinant,
                    raw[0].y,(a.x*distances[i]-distances[previous]*b.x)/determinant))
            rings[level] = [corners[0],corners[1],corners[1].lerp(corners[2],.5),
                corners[2],corners[3],corners[3].lerp(corners[4],.5),corners[4],corners[5],
                corners[5].lerp(corners[6],.5),corners[6],corners[7],corners[7].lerp(corners[0],.5)]
        return rings

    static func armor_slice(rings: Array, height: float) -> Array:
        for i in rings.size()-1:
            if height <= rings[i+1][0].y:
                var fraction := clampf((height-rings[i][0].y)/(rings[i+1][0].y-rings[i][0].y),0.0,1.0)
                var result: Array[Vector3] = []
                for point in 12: result.append(rings[i][point].lerp(rings[i+1][point],fraction))
                return result
        return rings.back()

    static func cast_surface_rings(sections: Array, plan: Array) -> Array:
        # Refine only the casting skin. The twelve structural landmarks stay
        # unchanged for fixture placement, width, front/rear and roof support.
        # A rational arc passes through each authored corner midpoint, so the
        # fore/aft shoulder differences do not become one generic round tower.
        var result: Array = []
        for ring in armor_rings(sections,plan):
            var refined: Array[Vector3] = [ring[0],ring[1]]
            for corner in 4:
                var start: int = 1+corner*3
                var a: Vector3 = ring[start]
                var b: Vector3 = ring[(start+1)%12]
                var c: Vector3 = ring[(start+2)%12]
                var q: float = plan[4] if corner in [0,3] else plan[5]
                var weight := .5/q-1.0
                var control := ((2.0+2.0*weight)*b-a-c)/(2.0*weight)
                for t in [.25,.5,.75]:
                    if t == .5:
                        refined.append(b)
                    else:
                        var u: float = 1.0-t
                        refined.append((a*u*u+control*2.0*weight*t*u+c*t*t)/(u*u+2.0*weight*t*u+t*t))
                if corner < 3:
                    refined.append(c)
                    refined.append(ring[(start+3)%12])
            result.append(refined)
        return result

    static func cast_shoulder_ring(sections: Array) -> int:
        # The lower neck ends at the first widest shoulder. T62 reaches that
        # station higher than the five other castings; it is not a shared band.
        var widest := 1
        for level in range(2,sections.size()-1):
            if sections[level][1] > sections[widest][1]: widest = level
        return widest

    func cast_loft(sections: Array, color: Color, offset := Vector3.ZERO, softness: float = .58, cheek_bounce: bool = false, planar_corners: bool = false, front_corner_factor: float = 1.0, plan: Array = [], refined_cast: bool = false) -> void:
        # Vehicle castings only. Adjacent cheek/shoulder faces share an authored
        # normal, but the roof, underside and attached plates keep hard seams.
        # The shared world loft and its material response remain unchanged.
        var points: Array[Vector3] = []
        var normals: Array[Vector3] = []
        var neck_normals: Array[Vector3] = []
        var faces: Array[Vector3i] = []
        var authored: Array = [] if plan.is_empty() else armor_rings(sections,plan)
        if refined_cast: authored = cast_surface_rings(sections,plan)
        var ring_size := 20 if refined_cast else 12
        var neck_end := cast_shoulder_ring(sections)*ring_size if refined_cast else 0
        for s in sections:
            var y: float = s[0]
            var x: float = s[1]
            var f: float = s[2]
            var r: float = s[3]
            var c: float = s[4]
            # Welded Abrams cheeks use one flat diagonal per corner. M60's
            # nose tapers more strongly than its broad rear casting.
            var q := .5 if planar_corners else .28
            var fc := minf(c*front_corner_factor,minf(x*.94,(r-f)*.48))
            var ring: Array[Vector3] = [Vector3(-x+fc,y,f),Vector3(x-fc,y,f),
                Vector3(x-fc*q,y,f+fc*q),Vector3(x,y,f+fc),Vector3(x,y,r-c),
                Vector3(x-c*q,y,r-c*q),Vector3(x-c,y,r),Vector3(-x+c,y,r),
                Vector3(-x+c*q,y,r-c*q),Vector3(-x,y,r-c),Vector3(-x,y,f+fc),
                Vector3(-x+fc*q,y,f+fc*q)]
            if not plan.is_empty(): ring.assign(authored[points.size()/ring_size])
            for point in ring:
                points.append(point+offset)
                normals.append(Vector3.ZERO)
                if refined_cast: neck_normals.append(Vector3.ZERO)
        for level in sections.size()-1:
            for i in ring_size:
                var j := (i+1)%ring_size
                faces.append(Vector3i(level*ring_size+j,level*ring_size+i,(level+1)*ring_size+i))
                faces.append(Vector3i(level*ring_size+j,(level+1)*ring_size+i,(level+1)*ring_size+j))
        for face in faces:
            var n := (points[face.y]-points[face.x]).cross(points[face.z]-points[face.x]).normalized()
            var corners := [face.x,face.y,face.z]
            for i in 3:
                var at: int = corners[i]
                var u := (points[corners[(i+1)%3]]-points[at]).normalized()
                var v := (points[corners[(i+2)%3]]-points[at]).normalized()
                var contribution := n*acos(clampf(u.dot(v),-1.0,1.0))
                if refined_cast and face.x < neck_end:
                    # The underside neck joins the upper casting at a real
                    # change of slope. Keep this structural seam; only the
                    # curved shoulder above it has continuous side normals.
                    neck_normals[at] += contribution
                else:
                    normals[at] += contribution
        if not surfaces.has("armor"):
            var created := SurfaceTool.new()
            created.begin(Mesh.PRIMITIVE_TRIANGLES)
            surfaces.armor = created
        var stream: SurfaceTool = surfaces.armor
        var bottom: float = sections.front()[0]+offset.y
        var height: float = sections.back()[0]-sections.front()[0]
        for face in faces:
            var n := (points[face.y]-points[face.x]).cross(points[face.z]-points[face.x]).normalized()
            for at in [face.x,face.z,face.y]:
                var shared_normal: Vector3 = neck_normals[at] if refined_cast and face.x < neck_end else normals[at]
                var normal := shared_normal.normalized() if refined_cast else n.lerp(shared_normal.normalized(),softness).normalized()
                if not refined_cast and n.dot(normal) < .84:
                    normal = n.lerp(normal,.35).normalized()
                var level := clampf((points[at].y-bottom)/height,0.0,1.0)
                var shade := (1.0-smoothstep(0.0,.12,level))*.06
                var warm := maxf(normal.y,0.0)*.16+level*level*.045+(1.0-level)*.055
                var tint := color.lerp(Color("263c40"),shade).lerp(Color("d4c99e"),warm)
                if cheek_bounce:
                    # Broad painted bounce on the crew pod, not emission or a
                    # change to scene lighting. Keep the bearing/gaskets dark.
                    var bounce := (1.0-smoothstep(.08,.56,level))*.24
                    tint = tint.lerp(color.lerp(Color("a6b6a5"),.66),bounce)
                stream.set_normal(normal)
                stream.set_color(tint)
                stream.add_vertex(points[at])
        var last := (sections.size()-1)*ring_size
        # Flat chamfers have collinear midpoints. Exclude those from the cap
        # fan rather than emitting zero-area triangles at its anchor corner.
        var flat_corners: bool = planar_corners if plan.is_empty() else plan[4] == .5 and plan[5] == .5
        var cap: Array = [0,1,3,4,6,7,9,10] if flat_corners else range(ring_size)
        for i in range(1,cap.size()-1):
            tri(points[last+cap[0]],points[last+cap[i+1]],points[last+cap[i]],color,"armor")
            tri(points[cap[0]],points[cap[i]],points[cap[i+1]],color.darkened(.24),"armor")

    func armor_panel(outline: Array[Vector3], outward: Vector3, depth: float, color: Color, rim: Color) -> void:
        # A real raised lip, broad bevel and inset lid; no coplanar decal or
        # tiny noise. The outline follows its supporting cast/sloped surface.
        var center := Vector3.ZERO
        for point in outline: center += point
        center /= outline.size()
        var order: Array[Vector3] = outline.duplicate()
        if (order[1]-order[0]).cross(order[2]-order[0]).dot(outward) < 0.0:
            order.reverse()
        for i in order.size():
            var j := (i+1)%order.size()
            var a: Vector3 = order[i]
            var b: Vector3 = order[j]
            var ai := a.lerp(center,.14)+outward*depth
            var bi := b.lerp(center,.14)+outward*depth
            quad(a,b,bi,ai,rim,"armor")
            tri(center+outward*depth,ai,bi,color,"armor")

    func crown(center: Vector3, size: Vector3, color: Color, group: String = "paint", phase: float = 0.0) -> void:
        var rings: Array = []
        for level in 5:
            var heights := [-.48,-.25,.12,.37,.50]
            var widths := [.34,.88,1.0,.69,.15]
            var ring: Array[Vector3] = []
            for i in 8:
                var a := TAU*i/8.0 + phase
                ring.append(center+Vector3(cos(a)*size.x*widths[level],size.y*heights[level],sin(a)*size.z*widths[level]))
            rings.append(ring)
        for level in 4:
            for i in 8:
                var j := (i+1)%8
                quad(rings[level][j],rings[level][i],rings[level+1][i],rings[level+1][j],color,group)
        for i in range(1,7):
            tri(rings[4][0],rings[4][i+1],rings[4][i],color,group)
            tri(rings[0][0],rings[0][i],rings[0][i+1],color,group)

    func plume(center: Vector3, size: Vector3, color: Color, group: String, lean: Vector2, phase: float) -> void:
        # Shared vertex normals soften the billow without textures or extra
        # material surfaces. Broad continuous painted values avoid latitude-band seams.
        var segments := 12
        var points: Array[Vector3] = []
        var normals: Array[Vector3] = []
        var triangles: Array[Vector3i] = []
        var heights := [-.45,-.22,.08,.34,.53]
        var widths := [.32,.84,1.0,.76,.24]
        for level in 5:
            var drift := float(level)*.25
            for i in segments:
                var a := TAU*i/float(segments)+phase
                points.append(center+Vector3(cos(a)*size.x*widths[level]+lean.x*drift,
                    heights[level]*size.y,sin(a)*size.z*widths[level]+lean.y*drift))
                normals.append(Vector3.ZERO)
        for level in 4:
            for i in segments:
                var j := (i+1)%segments
                triangles.append(Vector3i(level*segments+j,level*segments+i,(level+1)*segments+i))
                triangles.append(Vector3i(level*segments+j,(level+1)*segments+i,(level+1)*segments+j))
        for i in range(1,segments-1):
            triangles.append(Vector3i(4*segments,4*segments+i+1,4*segments+i))
            triangles.append(Vector3i(0,i,i+1))
        for face in triangles:
            var n := (points[face.y]-points[face.x]).cross(points[face.z]-points[face.x]).normalized()
            var corners := [face.x,face.y,face.z]
            for corner in 3:
                var index: int = corners[corner]
                var u := (points[corners[(corner+1)%3]]-points[index]).normalized()
                var v := (points[corners[(corner+2)%3]]-points[index]).normalized()
                normals[index] += n*acos(clampf(u.dot(v),-1.0,1.0))
        if not surfaces.has(group):
            var created := SurfaceTool.new()
            created.begin(Mesh.PRIMITIVE_TRIANGLES)
            surfaces[group] = created
        var stream: SurfaceTool = surfaces[group]
        for face in triangles:
            for index in [face.x,face.z,face.y]:
                var height := clampf((points[index].y-center.y)/size.y+.45,0.0,1.0)
                var tint := color.lerp(Color(.58,.64,.64,color.a),smoothstep(.08,.95,height)*.48)
                stream.set_normal(normals[index].normalized())
                stream.set_color(tint)
                stream.add_vertex(points[index])

    func leaf_cluster(center: Vector3, size: Vector3, color: Color, phase: float, shape: int = 0, footprint := Rect2(), branch: int = 0) -> void:
        # An asymmetric branch fan: a broad angular underside and a canted
        # crown, rather than a latitude-ring ball. All clusters share one
        # foliage surface; broad painted planes do the work of small leaves.
        var outline := [Vector2(-.90,-.35),Vector2(-.43,-.82),Vector2(.43,-.72),
            Vector2(1.0,-.06),Vector2(.56,.76),Vector2(-.30,.91),Vector2(-.86,.31)]
        var rings: Array = []
        # Recover the original upper crown height: visibility uses the whole
        # tree's rendered AABB to suppress hints behind foreground forest.
        # Crown masses can move below that ceiling without changing it.
        var original_levels := [[-.27,.48,.03],[-.08,1.0,0.0],[.14,.85+.025*(shape%3),-.055],
            [.33+.045*(shape%2),.40+.075*((shape>>1)%3),-.13]]
        var tilt := Vector2(.025+.012*(shape%3),-.016 if shape%2 == 0 else .022)
        var original_high := -INF
        for level in original_levels:
            for p in outline:
                # Match the original Vector3 float32 rounding before adding
                # the center, not a newly combined double-precision sum.
                var original_point := center+Vector3(0,level[0]*size.y+(p.x*level[1]+level[2])*tilt.x+p.y*level[1]*tilt.y,0)
                var y: float = original_point.y
                original_high = maxf(original_high,y)
        # Distribute the same three crowns as four distinct tree habits. The
        # thin umbrella, tapered deep crown, leaning fan and broad low shoulder
        # need visibly different mass distribution at normal gameplay scale.
        var family := ((shape-branch)>>2)&3
        var habits := [
            [[0.0,.16,.04],[.40,1.0,0.0],[.80,.97,-.04],[1.0,.74,-.10]],
            [[0.0,.18,0.0],[.12,1.0,0.0],[.43,.68,.02],[1.0,.035,.04]],
            [[0.0,.18,-.24],[.20,.82,-.10],[.67,1.0,.05],[1.0,.30,.44]],
            [[0.0,.30,.12],[.30,.96,0.0],[.55,1.0,-.05],[1.0,.55,-.24]],
        ]
        var levels: Array = habits[family]
        if family == 2:
            outline = [Vector2(-1.0,-.18),Vector2(-.52,-.85),Vector2(.30,-.52),
                Vector2(1.0,-.18),Vector2(.62,.89),Vector2(-.11,.85),Vector2(-.78,.50)]
            tilt = Vector2(.18,-.055)
        var target_high := original_high
        var crown_depth: float = [.22,.78,.56,.48][family]
        if branch == 1:
            target_high += [.06,-.10,.08,-.08][family]
            crown_depth = [.20,.42,.28,.38][family]
        elif branch == 2:
            target_high += [.14,-.05,.18,.09][family]
            crown_depth = [.16,.30,.26,.30][family]
        var target_low := target_high-crown_depth
        var shaped_low := INF
        var shaped_high := -INF
        for level in levels:
            var ring: Array[Vector3] = []
            for p in outline:
                var x: float = p.x*level[1]+level[2]
                var z: float = p.y*level[1]
                var point := center+Vector3((x*cos(phase)-z*sin(phase))*size.x,
                    level[0]*size.y+x*tilt.x+z*tilt.y,(x*sin(phase)+z*cos(phase))*size.z)
                ring.append(point)
                shaped_low = minf(shaped_low,point.y)
                shaped_high = maxf(shaped_high,point.y)
            rings.append(ring)
        var attachment_drop := maxf(0.0,target_low-(center.y-.18))
        for ring_index in rings.size():
            var ring: Array = rings[ring_index]
            for index in ring.size():
                ring[index].y = lerpf(target_low,target_high,(ring[index].y-shaped_low)/(shaped_high-shaped_low))
                # Keep thin/upward leaf fans seated on the original branch.
                # Only the small underside neck moves; the three upper rings
                # retain the reviewed canopy, including its exact ceiling.
                if ring_index == 0: ring[index].y -= attachment_drop
        if footprint.has_area():
            # Fit authored crown contours before normals are built. Different
            # edge-reaching footprints interlock without crossing cover cells.
            var minimum := Vector2(INF,INF)
            var maximum := Vector2(-INF,-INF)
            for ring in rings:
                for point in ring:
                    minimum = minimum.min(Vector2(point.x,point.z))
                    maximum = maximum.max(Vector2(point.x,point.z))
            for ring in rings:
                for index in ring.size():
                    var unit := (Vector2(ring[index].x,ring[index].z)-minimum)/(maximum-minimum)
                    var fitted := footprint.position+unit*footprint.size
                    ring[index].x = fitted.x
                    ring[index].z = fitted.y
        # Broad warm upper planes and cool lower folds separate leaf masses
        # without changing the canopy's alpha, coverage or surface normals.
        var cool_under := color.darkened(.16).lerp(Color(.10,.22,.20,color.a),.16)
        var warm_shoulder := color.lightened(.10).lerp(Color(.38,.43,.20,color.a),.18)
        var warm_top := color.lightened(.15).lerp(Color(.42,.47,.21,color.a),.24)
        for level in 3:
            var shade := cool_under if level == 0 else color if level == 1 else warm_shoulder
            for i in 7:
                var j := (i+1)%7
                quad(rings[level][j],rings[level][i],rings[level+1][i],rings[level+1][j],shade,"foliage")
        for i in range(1,6):
            tri(rings[3][0],rings[3][i+1],rings[3][i],warm_top,"foliage")
            tri(rings[0][0],rings[0][i],rings[0][i+1],cool_under.darkened(.05),"foliage")

    func polygon(points: Array, depth: float, color: Color, group: String = "paint") -> void:
        # Extruded X/Y silhouette. Polygon points must wind counterclockwise.
        var center := Vector3.ZERO
        for p in points:
            center += Vector3(p.x,p.y,0)
        center /= points.size()
        for i in points.size():
            var j := (i+1)%points.size()
            var a := Vector3(points[i].x,points[i].y,-depth*.5)
            var b := Vector3(points[j].x,points[j].y,-depth*.5)
            var c := Vector3(points[j].x,points[j].y,depth*.5)
            var d := Vector3(points[i].x,points[i].y,depth*.5)
            quad(a,b,c,d,color,group)
            tri(center+Vector3(0,0,-depth*.5),b,a,color,group)
            tri(center+Vector3(0,0,depth*.5),d,c,color,group)

    func ring(center: Vector3, axis: Vector3, outer: float, inner: float, depth: float, color: Color, group: String = "paint", accent := Color.TRANSPARENT) -> void:
        var u := axis.cross(Vector3.RIGHT if absf(axis.x) < .8 else Vector3.UP).normalized()
        var v := axis.cross(u)
        for i in 16:
            var a := u*cos(TAU*i/16.0)+v*sin(TAU*i/16.0)
            var b := u*cos(TAU*(i+1)/16.0)+v*sin(TAU*(i+1)/16.0)
            var front := center+axis*depth*.5
            var back := center-axis*depth*.5
            var tint := accent if accent.a > 0 and (i/2)%2 == 0 else color
            quad(front+a*outer,front+b*outer,front+b*inner,front+a*inner,tint,group)
            quad(back+b*outer,back+a*outer,back+a*inner,back+b*inner,tint,group)
            quad(back+a*outer,back+b*outer,front+b*outer,front+a*outer,tint,group)
            quad(front+a*inner,front+b*inner,back+b*inner,back+a*inner,tint,group)

    func finish(materials: Dictionary) -> ArrayMesh:
        var result := ArrayMesh.new()
        for group in surfaces:
            var stream: SurfaceTool = surfaces[group]
            stream.index()
            stream.commit(result)
            result.surface_set_material(result.get_surface_count()-1, materials[group])
        return result

static func _material(group: String) -> Material:
    if _materials.has(group):
        return _materials[group]
    if group == "water":
        var shader := Shader.new()
        shader.code = """shader_type spatial;
uniform vec4 deep_color : source_color = vec4(0.13, 0.31, 0.36, 1.0);
uniform vec4 shallow_color : source_color = vec4(0.27, 0.49, 0.49, 1.0);
uniform vec4 crest_color : source_color = vec4(0.58, 0.72, 0.65, 1.0);
uniform float visual_time = 0.0;
varying vec2 water_position;
void vertex() { water_position = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xz; }
void fragment() {
    vec2 p = floor(water_position * 24.0) / 24.0;
    float broad = 0.5 + 0.5 * sin(p.y * 2.8 + sin(p.x * 1.2) * 0.8);
    float wave = sin(p.y * 8.0 + sin(p.x * 1.8) * 1.1 - visual_time * 0.9);
    float broken = step(0.15, sin(p.x * 3.5 - p.y * 0.8));
    float crest = step(0.97, wave) * broken;
    ALBEDO = mix(mix(deep_color.rgb, shallow_color.rgb, broad * 0.36), crest_color.rgb, crest * 0.52);
    ROUGHNESS = 0.44; SPECULAR = 0.28;
}
"""
        var water := ShaderMaterial.new()
        water.shader = shader
        water.set_shader_parameter("visual_time",_visual_time)
        _materials[group] = water
        return water
    var mat := StandardMaterial3D.new()
    mat.vertex_color_use_as_albedo = true
    # Source paint swatches are authored sRGB, as in the raylib version.
    # Treating them as linear washes the whole casting toward pale plastic.
    mat.vertex_color_is_srgb = true
    mat.roughness = .88
    mat.metallic_specular = .22
    if group == "armor":
        # Broad, matte armor keeps a readable middle tone on curved cheeks.
        # This material group belongs only to vehicles; world paint and the
        # separate exposed metal/rubber surfaces keep their existing response.
        mat.diffuse_mode = BaseMaterial3D.DIFFUSE_LAMBERT_WRAP
        mat.roughness = .72
        mat.metallic_specular = .10
    elif group == "rubber":
        mat.roughness = 1.0
        mat.metallic_specular = .05
    elif group == "metal":
        mat.metallic = .30
        mat.roughness = .67
    elif group == "optic":
        mat.roughness = .3
        mat.emission_enabled = true
        mat.emission = Color(.10,.23,.19)
        mat.emission_energy_multiplier = .3
    elif group == "hot":
        mat.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
        mat.emission_enabled = true
        mat.emission = Color(1,.40,.025)
        mat.emission_energy_multiplier = .45
    elif group == "foliage":
        mat.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA_DEPTH_PRE_PASS
        mat.roughness = 1.0
    elif group == "water":
        mat.roughness = .38
        mat.metallic_specular = .36
    elif group == "smoke":
        mat.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA_DEPTH_PRE_PASS
        mat.roughness = 1.0
    elif group == "status":
        mat.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
    _materials[group] = mat
    return mat

static func _camouflage_armor_material(nation: int) -> StandardMaterial3D:
    var key := "woodland_armor" if nation == 0 else "winter_armor"
    if _materials.has(key):
        return _materials[key]
    # Original, editable blotches baked once. No noise/RNG, external image,
    # animated coordinates or extra geometry. Whitewash remains the majority.
    var patches := [
        PackedVector2Array([Vector2(4,10),Vector2(15,6),Vector2(21,10),Vector2(20,17),Vector2(27,22),Vector2(23,29),Vector2(15,28),Vector2(11,21),Vector2(3,18)]),
        PackedVector2Array([Vector2(39,1),Vector2(52,3),Vector2(56,14),Vector2(51,20),Vector2(55,27),Vector2(46,32),Vector2(38,27),Vector2(40,19),Vector2(33,12)]),
        PackedVector2Array([Vector2(5,45),Vector2(16,40),Vector2(27,46),Vector2(33,43),Vector2(39,50),Vector2(31,60),Vector2(19,57),Vector2(11,62),Vector2(4,55)]),
        PackedVector2Array([Vector2(47,47),Vector2(53,42),Vector2(60,45),Vector2(59,53),Vector2(54,55),Vector2(48,52)]),
    ]
    if nation in [0,1]:
        for index in patches.size():
            var center := Vector2.ZERO
            for point in patches[index]: center += point
            center /= patches[index].size()
            for point in patches[index].size():
                patches[index][point] = center+(patches[index][point]-center)*(.78 if nation == 0 else .82)
    var image := Image.create(256,256,false,Image.FORMAT_RGB8)
    # Leave light-response headroom in the broad winter field, including on
    # horizontal roofs. The neutral modulation survives per-instance HP tint.
    image.fill(Color("e4e4e4") if nation == 1 else Color.WHITE)
    for y in 256:
        for x in 256:
            for index in patches.size():
                if Geometry2D.is_point_in_polygon(Vector2((x+.5)*.25,(y+.5)*.25),patches[index]):
                    # Modulate the authored pale paint without washing out its
                    # normal-based highlights or tinting metal/rubber/identity.
                    image.set_pixel(x,y,Color("53664f") if nation == 1 else Color("74776b") if index >= 2 else Color("d8b8a0"))
                    break
    image.generate_mipmaps()
    var mat := _material("armor").duplicate() as StandardMaterial3D
    if nation == 1:
        # Winter whitewash needs an unwrapped dark side and highlight headroom.
        # This is a vehicle-only material; world light/exposure are untouched.
        mat.diffuse_mode = BaseMaterial3D.DIFFUSE_LAMBERT
        mat.roughness = .90
        mat.metallic_specular = .04
    mat.albedo_texture = ImageTexture.create_from_image(image)
    mat.texture_filter = BaseMaterial3D.TEXTURE_FILTER_LINEAR_WITH_MIPMAPS
    mat.uv1_triplanar = true
    mat.uv1_world_triplanar = false
    mat.uv1_triplanar_sharpness = 32.0
    mat.uv1_scale = Vector3(.72,.72,.72)
    mat.uv1_offset = Vector3(.47,.19,.27)
    _materials[key] = mat
    return mat

static func _instance(key: String, geometry: Geometry) -> Node3D:
    if not _meshes.has(key):
        var materials: Dictionary = {}
        for group in geometry.surfaces:
            materials[group] = _material(group)
        _mesh_groups[key] = geometry.surfaces.keys()
        _meshes[key] = geometry.finish(materials)
    var root := Node3D.new()
    var body := MeshInstance3D.new()
    body.name = "Body"
    body.mesh = _meshes[key]
    root.add_child(body)
    root.set_meta("surface_groups",_mesh_groups[key])
    return root

static func _cached(key: String) -> Node3D:
    return _instance(key, null) if _meshes.has(key) else null

static func _vehicle_profile(nation: int, enemy: bool, role: int, level: int) -> Dictionary:
    nation = clampi(nation,0,2)
    var tier: int = [1,0,2,3][clampi(role,0,3)] if enemy else clampi(level,0,3)
    var chassis: float = [.82,.88,.94,1.0][tier]
    var turret: float = [.70,.80,.90,1.0][tier]
    # Source-informed chassis silhouettes, with equal plan area at each tier.
    # Track length is not hull length; see TANK_MODELING_STANDARD.md and the
    # proportion study. Wheel radii, heights and gun mounts remain independent.
    var breadth: float = [[.91,1.0,1.0,.96],[.97,.91,.97,1.0],[.96,.97,.925,.98]][nation][tier]
    var chassis_x: float = chassis*breadth*VEHICLE_WIDTH_SCALE
    var chassis_z: float = chassis/breadth
    var names := [["M4A3(75) SHERMAN","M26 PERSHING","M60A3","M1A1 ABRAMS"],
        ["T-34/76","IS-2","T-62","T-90A"],["PANTHER AUSF. A","TIGER II","LEOPARD 1","LEOPARD 2A4"]]
    var shapes := [["sherman","pershing","m60","abrams"],["t34","is2","t62","t90"],
        ["panther","tiger2","leopard1","leopard2"]]
    # Recognizable historical silhouettes with modest arcade volume. Gun tips
    # stay at the prior visual mounts; native spawning and collisions are separate.
    var hull_tops := [[.48,.365,.43,.34],[.385,.39,.345,.345],[.44,.465,.36,.39]]
    var heights := [[.205,.215,.255,.190],[.185,.235,.165,.180],[.210,.250,.205,.225]]
    var widths := [[.56,.64,.69,.80],[.55,.66,.72,.78],[.55,.67,.71,.81]]
    var lengths := [[.61,.76,.91,1.02],[.58,.75,.80,.89],[.63,.80,.87,1.01]]
    var centers := [[-.055,-.03,-.015,.02],[-.22,-.10,-.065,-.075],[-.08,-.055,-.02,.015]]
    var tips := [[.87,1.36,1.44,1.38],[.82,1.49,1.40,1.52],[1.19,1.43,1.48,1.43]]
    var base: float = hull_tops[nation][tier]+.006
    var height: float = heights[nation][tier]
    var gun_heights := [[.5974,.4872,.5714,.455],[.4952,.5218,.4456,.4504],[.5622,.6064,.4774,.5194]]
    var muzzle := Vector3(0,gun_heights[nation][tier],-tips[nation][tier])
    return {"nation":nation,"tier":tier,"enemy":enemy,"role":role,"name":names[nation][tier],
        "shape":shapes[nation][tier],"chassis":chassis,"chassis_x":chassis_x,"chassis_z":chassis_z,"turret":turret,
        "w":widths[nation][tier]*.52*TURRET_WIDTH_SCALE,"length":lengths[nation][tier],
        "center":centers[nation][tier]/breadth,"h":height,
        "base":base,"hull_top":hull_tops[nation][tier]*HULL_DECK_SCALE,"hull_width":.35*chassis_x,
        "track_length":1.90*chassis_z,"track_height":[[.40,.36,.37,.34],[.34,.36,.34,.34],[.39,.40,.35,.35]][nation][tier]*chassis,"track_center":.38*chassis_x,
        "track_width":.24*chassis_x,"offset":0.0,"muzzle":muzzle,
        "gun_radius":[.032,.035,.038,.042][tier]*GUN_RADIUS_SCALE,"wheeled":false,"casemate":false,
        "skirts":tier == 3,"auxiliary":false,"coaxial":false}

# Wheel locations are authored in a normalized chassis, not scaled copies of
# one wheel count. See docs/TANK_CHASSIS_REFERENCES.md for variant sources.
static func _chassis_running_gear(d: Dictionary) -> Dictionary:
    var profiles := {
        "sherman":[[-.45,-.29,-.08,.08,.29,.45],.060,[-.37,0,.37],true,false],
        "pershing":[[-.47,-.282,-.094,.094,.282,.47],.065,[-.40,-.20,0,.20,.40],false,false],
        "m60":[[-.475,-.285,-.095,.095,.285,.475],.066,[-.34,0,.34],false,false],
        "abrams":[[-.49,-.327,-.164,0,.164,.327,.49],.055,[-.29,.29],false,false],
        "t34":[[-.445,-.2225,0,.2225,.445],.077,[],false,false],
        "is2":[[-.475,-.285,-.095,.095,.285,.475],.065,[-.34,0,.34],false,false],
        "t62":[[-.46,-.245,-.03,.225,.485],.074,[],false,false],
        "t90":[[-.48,-.288,-.096,.096,.288,.48],.067,[-.34,0,.34],false,false],
        "panther":[[-.48,-.343,-.206,-.069,.069,.206,.343,.48],.101,[],true,true],
        "tiger2":[[-.49,-.3675,-.245,-.1225,0,.1225,.245,.3675,.49],.091,[],true,true],
        "leopard1":[[-.49,-.327,-.164,0,.164,.327,.49],.058,[-.39,-.13,.13,.39],false,false],
        "leopard2":[[-.49,-.327,-.164,0,.164,.327,.49],.059,[-.39,-.13,.13,.39],false,false]}
    var profile: Array = profiles[d.shape]
    return {"positions":profile[0],"radius":profile[1]*1.70*d.chassis,
        "rollers":profile[2],"front_drive":profile[3],"overlap":profile[4]}

static func _track_point(travel: float, d: Dictionary, inset: float = 0.0) -> Vector3:
    var p := _belt_point(travel,d.track_length,d.track_height,inset)
    # Raised end wheels give the lower run a climbing nose instead of a
    # capsule-shaped rubber pod. Unsupported upper runs sag slightly.
    var lift: float = (.10 if d.shape in ["sherman","panther","tiger2"] else .065)*d.chassis
    p.y += lift*smoothstep(d.track_length*.25,d.track_length*.49,absf(p.z))*maxf(0,1.0-p.y/d.track_height)
    if d.shape in ["t34","t62","panther","tiger2"] and p.y > d.track_height*.72:
        p.y -= .025*d.chassis*maxf(0,1.0-pow(p.z/(d.track_length*.38),2))
    return p

static func _belt_point(travel: float, length: float, height: float, inset: float = 0.0) -> Vector3:
    var radius := height*.5
    var straight := length-height
    var arc := PI*radius
    var p := fposmod(travel,2.0*(straight+arc))
    var r := radius-inset
    var y := radius+.006
    if p < straight:
        return Vector3(0,y+r,-straight*.5+p)
    p -= straight
    if p < arc:
        return Vector3(0,y+r*cos(p/radius),straight*.5+r*sin(p/radius))
    p -= arc
    if p < straight:
        return Vector3(0,y-r,straight*.5-p)
    p -= straight
    return Vector3(0,y-r*cos(p/radius),-straight*.5-r*sin(p/radius))

static func _at_x(p: Vector3, x: float) -> Vector3:
    return Vector3(x,p.y,p.z)

static func _belt(g: Geometry, side: float, d: Dictionary, center_offset: float = 0.0, width_scale: float = 1.0, phase: float = 0.0) -> void:
    var length: float = d.track_length
    var height: float = d.track_height
    var outside: float = side*(d.track_center+center_offset+d.track_width*width_scale*.5)
    var inside: float = side*(d.track_center+center_offset-d.track_width*width_scale*.5)
    var perimeter := 2.0*(length-height+PI*height*.5)
    var rubber := Color("263335")
    var shoe := Color("697575")
    for i in 36:
        var a := _track_point(perimeter*i/36.0,d)
        var b := _track_point(perimeter*(i+1)/36.0,d)
        var c := _track_point(perimeter*i/36.0,d,.067)
        var e := _track_point(perimeter*(i+1)/36.0,d,.067)
        if side > 0:
            g.quad(_at_x(a,outside),_at_x(b,outside),_at_x(e,outside),_at_x(c,outside),shoe,"metal")
            g.quad(_at_x(a,inside),_at_x(b,inside),_at_x(b,outside),_at_x(a,outside),rubber,"rubber")
        else:
            g.quad(_at_x(b,outside),_at_x(a,outside),_at_x(c,outside),_at_x(e,outside),shoe,"metal")
            g.quad(_at_x(a,outside),_at_x(b,outside),_at_x(b,inside),_at_x(a,inside),rubber,"rubber")
    for i in GEAR_SHOE_COUNT:
        # Broad raised shoes retain their end lips. Three shallow top panels
        # follow the end arcs, keeping the shoe above the continuous belt.
        # Fewer repeated forms remain readable at native gameplay pixel size.
        var start := perimeter*(float(i)-phase)/GEAR_SHOE_COUNT
        var finish := perimeter*(float(i)+.75-phase)/GEAR_SHOE_COUNT
        var a := _track_point(start,d,-.006)
        var b := _track_point(finish,d,-.006)
        var foot_a := _track_point(start,d,.016)
        var foot_b := _track_point(finish,d,.016)
        for segment in 3:
            var u := _track_point(lerpf(start,finish,float(segment)/3.0),d,-.006)
            var v := _track_point(lerpf(start,finish,float(segment+1)/3.0),d,-.006)
            if side > 0:
                g.quad(_at_x(u,inside),_at_x(v,inside),_at_x(v,outside),_at_x(u,outside),shoe,"metal")
            else:
                g.quad(_at_x(u,outside),_at_x(v,outside),_at_x(v,inside),_at_x(u,inside),shoe,"metal")
        if side > 0:
            g.quad(_at_x(foot_a,inside),_at_x(a,inside),_at_x(a,outside),_at_x(foot_a,outside),shoe.darkened(.12),"metal")
            g.quad(_at_x(b,inside),_at_x(foot_b,inside),_at_x(foot_b,outside),_at_x(b,outside),shoe.lightened(.08),"metal")
        else:
            g.quad(_at_x(foot_a,outside),_at_x(a,outside),_at_x(a,inside),_at_x(foot_a,inside),shoe.darkened(.12),"metal")
            g.quad(_at_x(b,outside),_at_x(foot_b,outside),_at_x(foot_b,inside),_at_x(b,inside),shoe.lightened(.08),"metal")

static func _wheel_spokes(g: Geometry, center: Vector3, side: float, radius: float, phase: float) -> void:
    # Broad, flush spokes retain similar total painted area with fewer repeated
    # slivers. The spoke count divides the wheel's 12-sided rotating profile.
    var half_width := .78/GEAR_SPOKE_COUNT
    for spoke in GEAR_SPOKE_COUNT:
        var angle := TAU*(float(spoke)-phase)/GEAR_SPOKE_COUNT
        var a := Vector3(0,cos(angle-half_width),sin(angle-half_width))
        var b := Vector3(0,cos(angle+half_width),sin(angle+half_width))
        var p := center+a*radius*.30
        var q := center+a*radius*.72
        var r := center+b*radius*.72
        var s := center+b*radius*.30
        if side > 0.0: g.quad(p,q,r,s,Color("93a391"),"metal")
        else: g.quad(s,r,q,p,Color("93a391"),"metal")

static func _chassis_wheel(g: Geometry, center: Vector3, side: float, radius: float,
        paint: Color, phase: float, rubber_tire: bool = true, teeth: bool = false) -> void:
    # Continuous tire wall, bevelled rim and recessed dish. Hidden backs are
    # omitted; 12 segments keep nine overlapping wheels within the mesh budget.
    var normal := Vector3(side,0,0)
    var outer := center+normal*.020
    var hub := center+normal*.052
    var rim := Color("293639") if teeth else paint
    for i in 12:
        var angle := TAU*float(i)/12.0-phase*TAU/GEAR_SPOKE_COUNT
        var next := angle+TAU/12.0
        var a := Vector3(0,cos(angle),sin(angle))
        var b := Vector3(0,cos(next),sin(next))
        var tooth_a := 1.08 if teeth and i%2 == 0 else 1.0
        var tooth_b := 1.08 if teeth and (i+1)%2 == 0 else 1.0
        var r0 := radius*tooth_a
        var r1 := radius*tooth_b
        var points: Array[Vector3] = [center+a*r0,center+b*r1,outer+b*r1,outer+a*r0]
        if side < 0: points.reverse()
        g.quad(points[0],points[1],points[2],points[3],Color("263335") if rubber_tire else Color("66716b"),"rubber" if rubber_tire else "metal")
        points = [outer+a*r0,outer+b*r1,hub+b*radius*.75,hub+a*radius*.75]
        if side < 0: points.reverse()
        g.quad(points[0],points[1],points[2],points[3],rim,"metal")
        var aa := hub+a*radius*.75
        var bb := hub+b*radius*.75
        if side > 0: g.tri(hub,aa,bb,rim.darkened(.15),"metal",normal)
        else: g.tri(hub,bb,aa,rim.darkened(.15),"metal",normal)
    # Flush hub cap and broad recessed spoke openings turn with the wheel.
    for cap in 6:
        var a := TAU*cap/6.0
        var b := TAU*(cap+1)/6.0
        var center_cap := hub+normal*.002
        var aa := center_cap+Vector3(0,cos(a),sin(a))*radius*.24
        var bb := center_cap+Vector3(0,cos(b),sin(b))*radius*.24
        if side > 0: g.tri(center_cap,aa,bb,paint.lightened(.08),"metal",normal)
        else: g.tri(center_cap,bb,aa,paint.lightened(.08),"metal",normal)
    for spoke in 3:
        var angle := TAU*(float(spoke)-phase)/3.0
        var a := hub+normal*.001+Vector3(0,cos(angle-.18),sin(angle-.18))*radius*.38
        var b := hub+normal*.001+Vector3(0,cos(angle+.18),sin(angle+.18))*radius*.67
        var c := hub+normal*.001+Vector3(0,cos(angle-.18),sin(angle-.18))*radius*.67
        if side > 0: g.tri(a,c,b,Color("263335"),"metal",normal)
        else: g.tri(a,b,c,Color("263335"),"metal",normal)

static func _running_gear(g: Geometry, d: Dictionary, selected_side: float, phase: float = 0.0) -> void:
    var side := selected_side
    _belt(g,side,d,0.0,1.0,phase)
    var profile := _chassis_running_gear(d)
    var s: float = d.chassis
    var paint: Color = NATIONAL_PAINT[d.nation].lerp(Color("a5ada0"),.23)
    var face: float = d.track_center+d.track_width*.5-.057
    var radius: float = profile.radius
    for i in profile.positions.size():
        var z: float = profile.positions[i]*(d.track_length-d.track_height)
        var layer: float = -.052*s if profile.overlap and i%2 == 1 else 0.0
        var center := Vector3(side*(face+layer),radius+.041*s,z)
        _chassis_wheel(g,center,side,radius,paint,phase,d.shape not in ["tiger2","is2"])
        # Arms anchor the wheels to the hull. Sherman instead has distinct
        # paired-wheel VVSS spring bogies, constructed below.
        if d.shape != "sherman":
            var pivot := center+Vector3(-side*.05,.075*s,-.05*s)
            g.quad(pivot+Vector3(0,.012,0),center+Vector3(0,.012,0),center-Vector3(0,.012,0),pivot-Vector3(0,.012,0),Color("3b4641"),"metal")
    var end_z: float = (d.track_length-d.track_height)*.5
    for end in [-1.0,1.0]:
        var drive: bool = (end < 0) == bool(profile.front_drive)
        var height_factor: float = .62 if d.shape == "pershing" and end < 0 else .54
        var center := Vector3(side*(face-.012*s),d.track_height*height_factor+.006,end*end_z)
        _chassis_wheel(g,center,side,d.track_height*(.32 if drive else .29),paint,phase,false,drive)
    for z in profile.rollers:
        var at := Vector3(side*(face-.040*s),d.track_height-.056*s,float(z)*(d.track_length-d.track_height))
        g.tube(at-Vector3(side*.028*s,0,0),at,.046*s,.046*s,Color("3b4843"),"metal",8)
    if d.shape == "sherman":
        for bogie in 3:
            var first: float = profile.positions[bogie*2]
            var second: float = profile.positions[bogie*2+1]
            var half_spacing: float = (second-first)*.5*(d.track_length-d.track_height)
            var at := Vector3(side*(face+.002),.29*s,(first+second)*.5*(d.track_length-d.track_height))
            g.box(at,Vector3(.055*s,.10*s,.13*s),paint,"metal")
            for fore in [-1.0,1.0]:
                g.tube(at+Vector3(0,-.035*s,fore*.085*s),at+Vector3(0,-.115*s,fore*half_spacing),.024*s,.028*s,paint,"metal",6)

static func _gear_frames(d: Dictionary, side: float) -> Array[ArrayMesh]:
    # Twelve historical running-gear layouts; shared by player/enemy instances
    # of a model, but never by unrelated same-tier national vehicles.
    var key := "%s_%d" % [d.shape,int(side)]
    if _gear_cache.has(key): return _gear_cache[key]
    var frames: Array[ArrayMesh] = []
    var materials := {"metal":_material("metal"),"rubber":_material("rubber")}
    for index in GEAR_FRAME_COUNT:
        var geometry := Geometry.new()
        _running_gear(geometry,d,side,float(index)/GEAR_FRAME_COUNT)
        frames.append(geometry.finish(materials))
    _gear_cache[key] = frames
    return frames

static func warm_running_gear() -> void:
    # Call during frontend setup, before battle input is enabled. Even an
    # unseen enemy chassis then selects existing meshes during combat.
    for nation in 3:
        for tier in 4:
            var profile := _vehicle_profile(nation,false,0,tier)
            _gear_frames(profile,-1.0)
            _gear_frames(profile,1.0)

static func _cannon(g: Geometry, muzzle: Vector3, root: float, radius: float, paint: Color, brake: bool, x: float = 0.0) -> void:
    var distance := root-muzzle.z
    # A short seated collar followed by a continuous tube. Most of the length
    # belongs to the barrel, rather than three inflated nozzle-like steps.
    var zs := [root,root-distance*.13,root-distance*.20,muzzle.z+distance*.18,muzzle.z+distance*.14,muzzle.z]
    # A thicker shaft carries the stylization. Keep the brake proportionate
    # instead of making the terminal nozzle as exaggerated as the main tube.
    var sizes := [radius*1.48,radius*1.42,radius,radius*.88,radius*(1.50 if brake else .98),radius*(1.44 if brake else .96)]
    var rings: Array = []
    for j in 6:
        var ring: Array[Vector3] = []
        for i in 10:
            var a := TAU*i/10.0
            # A broad oval mantlet leaves room for the crew's sight line.
            # The barrel, circular muzzle lip and attachment remain exact.
            var vertical := .82 if j < 2 else 1.0
            ring.append(Vector3(x+cos(a)*sizes[j],muzzle.y+sin(a)*sizes[j]*vertical,zs[j]))
        rings.append(ring)
    var steel := Color("687671")
    for j in 5:
        for i in 10:
            var k := (i+1)%10
            g.quad(rings[j][i],rings[j+1][i],rings[j+1][k],rings[j][k],paint if j < 3 else steel,"armor" if j < 3 else "metal")
    for i in 10:
        var a := TAU*i/10.0
        var b := TAU*(i+1)/10.0
        var p := Vector3(x+cos(a)*radius*.69,muzzle.y+sin(a)*radius*.69,muzzle.z)
        var q := Vector3(x+cos(b)*radius*.69,muzzle.y+sin(b)*radius*.69,muzzle.z)
        var p2 := p+Vector3(0,0,.075)
        var q2 := q+Vector3(0,0,.075)
        g.quad(rings[5][i],p,q,rings[5][(i+1)%10],steel,"metal")
        g.quad(p,p2,q2,q,Color("1b292d"),"rubber")
        g.tri(Vector3(x,muzzle.y,muzzle.z+.075),q2,p2,Color("142226"),"rubber")
    if brake:
        # Two gas ports on each side distinguish a real brake from a thick lip.
        # Dark recessed planes follow its flanks; no extra material or noise.
        for side in [-1.0,1.0]:
            for port in 2:
                var z: float = muzzle.z+distance*(.035+port*.065)
                var x_side: float = x+side*radius*1.47
                var points: Array[Vector3] = [Vector3(x_side,muzzle.y-radius*.57,z),
                    Vector3(x_side,muzzle.y+radius*.57,z),
                    Vector3(x_side,muzzle.y+radius*.57,z+distance*.034),
                    Vector3(x_side,muzzle.y-radius*.57,z+distance*.034)]
                if side < 0: points.reverse()
                g.quad(points[0],points[1],points[2],points[3],Color("182321"),"rubber")

static func _seated_mantlet(g: Geometry, d: Dictionary, gun_root: float, paint: Color) -> void:
    # Authored gun shields, not measurements: broad convex M26, narrow long-nose
    # M60, small Sherman shield, and separate angular M1/A4 aperture surrounds.
    # An actual open center accepts the existing cannon; the rear seats inside
    # the turret casting. No disconnected plate or painted-on circular hole.
    # [half-width / turret half-width, lower/upper reach, front bulge, corner]
    var profiles := {
        "sherman":[.48,.059,.064,.018,.32],
        "pershing":[.67,.084,.080,.095,.48],
        "m60":[.40,.090,.070,.100,.46],
        "abrams":[.35,.059,.064,.055,.12],
        "leopard2":[.57,.068,.068,.020,.08]}
    var p: Array = profiles[d.shape]
    var width: float = d.w*p[0]
    var reach: float = .100+float(p[3])
    # Match the tube at the actual front plane: the rear collar is wider than
    # the exposed shaft. A tapered internal socket clears both sections.
    var fraction: float = reach/(gun_root-d.muzzle.z)
    var collar_at_opening := lerpf(1.42,1.0,clampf((fraction-.13)/.07,0,1))
    var bore: float = d.gun_radius*(collar_at_opening+.10)
    # The flat modern surrounds need a wider rim to remain visible around the
    # thicker tube; cast shields retain their broader sculpted front surface.
    var rim: float = .018 if d.shape in ["abrams","leopard2"] else .012
    var rim_height: float = (bore+rim)/.95
    var low: float = maxf(p[1],rim_height)
    var high: float = maxf(p[2],rim_height)
    var cut: float = p[4]
    var outline: Array[Vector2] = [Vector2(-1+cut,1),Vector2(1-cut,1),
        Vector2(1,1-cut),Vector2(1,-1+cut),Vector2(1-cut,-1),
        Vector2(-1+cut,-1),Vector2(-1,-1+cut),Vector2(-1,1-cut)]
    if d.shape in ["pershing","m60"]:
        outline.clear()
        for i in 12:
            var a := PI*.5-TAU*float(i)/12.0
            outline.append(Vector2(signf(cos(a))*pow(absf(cos(a)),.72),
                signf(sin(a))*pow(absf(sin(a)),.85)))
    var rear: Array[Vector3] = []
    var shoulder: Array[Vector3] = []
    var opening: Array[Vector3] = []
    var socket: Array[Vector3] = []
    for point in outline:
        var xy := Vector2(point.x*width,point.y*(high if point.y > 0 else low))
        var unit := point.normalized()
        rear.append(Vector3(xy.x,d.muzzle.y+xy.y,gun_root+.10))
        shoulder.append(Vector3(xy.x*.95,d.muzzle.y+xy.y*.95,gun_root-.100))
        opening.append(Vector3(unit.x*bore,d.muzzle.y+unit.y*bore,gun_root-reach))
        socket.append(Vector3(unit.x*d.gun_radius*1.54,d.muzzle.y+unit.y*d.gun_radius*1.54,gun_root+.10))
    var front_paint := paint.darkened(.08)
    var edge_paint := paint.darkened(.28)
    var count := outline.size()
    var points: Array[Vector3] = shoulder.duplicate()
    points.append_array(opening)
    var faces: Array[Vector3i] = []
    for i in outline.size():
        var j := (i+1)%outline.size()
        g.quad(rear[i],rear[j],shoulder[j],shoulder[i],edge_paint,"armor")
        faces.append(Vector3i(i,j,count+j))
        faces.append(Vector3i(i,count+j,count+i))
        g.quad(opening[i],opening[j],socket[j],socket[i],Color("24302f"),"rubber")
    var curved: bool = d.shape in ["sherman","pershing","m60"]
    var normals: Array[Vector3] = []
    normals.resize(points.size())
    normals.fill(Vector3.ZERO)
    if curved:
        # Share angle-weighted normals only within the convex front casting.
        # The flange and inner socket keep their structural hard edges.
        for face in faces:
            var n := (points[face.y]-points[face.x]).cross(points[face.z]-points[face.x]).normalized()
            var indices := [face.x,face.y,face.z]
            for corner in 3:
                var at: int = indices[corner]
                var a := (points[indices[(corner+1)%3]]-points[at]).normalized()
                var b := (points[indices[(corner+2)%3]]-points[at]).normalized()
                normals[at] += n*acos(clampf(a.dot(b),-1,1))
    for face in faces:
        if not curved:
            g.tri(points[face.x],points[face.y],points[face.z],front_paint,"armor")
            continue
        var stream: SurfaceTool = g.surfaces.armor
        for at in [face.x,face.z,face.y]:
            var n := normals[at].normalized()
            var tint := front_paint.lerp(Color(.84,.79,.56),maxf(n.y,0)*.13)
            tint = tint.lerp(Color(.13,.21,.22),maxf(-n.y,0)*.25)
            stream.set_normal(n)
            stream.set_color(tint)
            stream.add_vertex(points[at])
    # Hidden back is embedded in the original turret and deliberately omitted.

static func _casting_section(sections: Array, height: float) -> Array:
    for i in sections.size()-1:
        if height <= float(sections[i+1][0]):
            var fraction := clampf((height-float(sections[i][0]))/(float(sections[i+1][0])-float(sections[i][0])),0.0,1.0)
            var section: Array = []
            for axis in 5: section.append(lerpf(sections[i][axis],sections[i+1][axis],fraction))
            return section
    return sections.back().duplicate()

static func _roof_half_width(section: Array, z: float, plan: Array = []) -> float:
    if not plan.is_empty():
        var ring := Geometry.armor_ring(section,plan)
        for i in range(1,6):
            var a: Vector3 = ring[i]
            var b: Vector3 = ring[i+1]
            if z >= a.z and z <= b.z and b.z-a.z > .000001:
                return lerpf(a.x,b.x,(z-a.z)/(b.z-a.z))
        return 0.0
    # The actual twelve-corner casting, including both clipped roof corners.
    # A rectangular marking must fit at both its front and rear edges.
    var edge := minf(z-section[2],section[3]-z)
    var width: float = section[1]
    var corner: float = section[4]
    if edge < corner*.28:
        return width-corner+maxf(edge,0.0)*(.72/.28)
    if edge < corner:
        return width-corner*.28+(edge-corner*.28)*(.28/.72)
    return width

static func _make_boat_support() -> Node3D:
    var key := "boat_support"
    var cached := _cached(key)
    if cached:
        return cached
    var g := Geometry.new()
    for side in [-1.0,1.0]:
        g.loft([[.09,.075,-.59,.59,.07],[.19,.11,-.73,.73,.095],
            [.30,.105,-.69,.69,.09],[.34,.065,-.57,.57,.05]],Color("718c89"),Vector3(side*.73,0,0),"rubber")
        g.box(Vector3(side*.73,.30,.23),Vector3(.23,.035,.06),Color("c1c09b"),"metal")
        g.box(Vector3(side*.73,.30,-.28),Vector3(.23,.035,.06),Color("c1c09b"),"metal")
    g.box(Vector3(0,.25,.38),Vector3(1.48,.045,.06),Color("354a4e"),"metal")
    return _instance(key,g)

static func _finish_vehicle(key: String, d: Dictionary, id: int) -> Node3D:
    var root := _instance(key,null)
    var body: MeshInstance3D = root.get_node("Body")
    var parts: Array[MeshInstance3D] = [body]
    var bounds := body.mesh.get_aabb()
    for item in [["Hull","_hull"],["TrackLeft","_left"],["TrackRight","_right"]]:
        var part_root := _instance(key+item[1],null)
        var part: MeshInstance3D = part_root.get_node("Body")
        part_root.remove_child(part)
        part.name = item[0]
        part.set_meta("surface_groups",part_root.get_meta("surface_groups"))
        root.add_child(part)
        part_root.free()
        parts.append(part)
        bounds = bounds.merge(part.mesh.get_aabb())
    body.set_meta("surface_groups",root.get_meta("surface_groups"))
    if d.nation in [0,1]:
        for part in parts:
            var armor_surface: int = part.get_meta("surface_groups").find("armor")
            if armor_surface >= 0:
                # Each enemy owns its brightness state; the small pattern
                # texture is shared. P1/P2 marks use the separate paint surface.
                part.set_surface_override_material(armor_surface,
                    _camouflage_armor_material(d.nation).duplicate() if d.enemy else _camouflage_armor_material(d.nation))
    var crown := _cartoon_crown(d)
    var turret_bounds := AABB(Vector3(0,crown[0][0],crown[0][2]),Vector3.ZERO)
    for section in crown:
        turret_bounds = turret_bounds.expand(Vector3(-section[1],section[0],section[2]))
        turret_bounds = turret_bounds.expand(Vector3(section[1],section[0],section[3]))
    root.set_meta("turret_bounds",turret_bounds)
    root.set_meta("chassis_size",Vector2(d.track_center*2.0+d.track_width,d.track_length))
    root.set_meta("turret_size",Vector2(turret_bounds.size.x,turret_bounds.size.z))
    root.set_meta("vehicle_parts",parts)
    root.set_meta("visual_bounds",bounds)
    root.set_meta("motion_pivot",Vector3(d.offset,d.base,.04))
    root.set_meta("track_half_length",float(d.track_length)*.5)
    root.set_meta("wheeled",d.wheeled)
    root.set_meta("motion",{"time":0.0,"drive":0.0,"spring":0.0,"velocity":0.0,
        "roll":0.0,"roll_velocity":0.0,"moving":false,"yaw":0.0,"initialized":false,
        "turret_y":0.0,"turret_pitch":0.0,"turret_roll":0.0})
    root.set_meta("gear_frames_left",_gear_frames(d,-1.0))
    root.set_meta("gear_frames_right",_gear_frames(d,1.0))
    root.set_meta("gear_cycle",TAU*.217/GEAR_SPOKE_COUNT if d.wheeled else
        2.0*(float(d.track_length)-float(d.track_height)+PI*float(d.track_height)*.5)/GEAR_SHOE_COUNT)
    root.set_meta("gear_half_width",float(d.track_center))
    reset_running_gear(root)
    root.set_meta("neutral_muzzle",d.muzzle)
    root.set_meta("vehicle_name",d.name)
    root.set_meta("enemy",d.enemy)
    root.set_meta("phase",float(posmod(id,97))*.71)
    var boat := _make_boat_support()
    boat.name = "Boat"
    boat.scale.x = VEHICLE_WIDTH_SCALE
    boat.visible = false
    root.add_child(boat)
    if d.enemy:
        var frozen := _make_frozen_cue()
        frozen.name = "Frozen"
        frozen.visible = false
        root.add_child(frozen)
    return root

static func _make_frozen_cue() -> Node3D:
    var cached := _cached("frozen_cue")
    if cached:
        cached.get_node("Body").cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
        return cached
    var g := Geometry.new()
    var color := Color("92e6ed")
    for i in 24:
        var a := TAU*i/24.0
        var b := TAU*(i+1)/24.0
        var u := Vector3(cos(a),0,sin(a))
        var v := Vector3(cos(b),0,sin(b))
        var height := Vector3(0,.075,0)
        g.quad(height+u*.76,height+v*.76,height+v*.82,height+u*.82,color,"status")
    for i in 4:
        var a := PI*.25+TAU*i/4.0
        var point := Vector3(cos(a)*.79,.17,sin(a)*.79)
        g.crown(point,Vector3(.045,.28,.045),color,"status",a)
    var result := _instance("frozen_cue",g)
    result.get_node("Body").cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
    return result

static func _armor_plan(shape: String, hull: bool = false) -> Array:
    # Original game-space interpretations of the selected variants. X cuts use
    # half-width; Z cuts use section length. Last entry is rear shoulder width.
    # See TANK_ARMOR_SURFACES.md for sources, construction and uncertainty.
    var turret_plans := {
        "sherman":[0.68,0.24,0.49,0.19,0.28,0.28,0.87],
        "pershing":[0.72,0.24,0.52,0.19,0.28,0.28,0.96],
        "m60":[0.88,0.39,0.48,0.14,0.28,0.28,0.88],
        "abrams":[0.55,0.3,0.05,0.035,0.5,0.5,0.9],
        "t34":[0.3,0.13,0.2,0.08,0.5,0.5,0.84],
        "is2":[0.78,0.25,0.62,0.22,0.28,0.28,0.88],
        "t62":[0.9,0.36,0.74,0.32,0.28,0.28,0.9],
        "t90":[0.57,0.29,0.26,0.11,0.5,0.5,0.8],
        "panther":[0.18,0.08,0.15,0.055,0.5,0.5,0.91],
        "tiger2":[0.12,0.045,0.1,0.055,0.5,0.5,0.85],
        "leopard1":[0.81,0.31,0.58,0.21,0.28,0.28,0.9],
        "leopard2":[0.08,0.035,0.08,0.035,0.5,0.5,1.0],
    }
    var hull_plans := {
        "sherman":[0.1,0.027,0.055,0.018,0.5,0.5,1.0],
        "pershing":[0.35,0.092,0.13,0.038,0.28,0.28,0.97],
        "m60":[0.1,0.025,0.13,0.038,0.5,0.5,0.96],
        "abrams":[0.1,0.05,0.06,0.015,0.5,0.5,1.0],
        "t34":[0.04,0.015,0.06,0.022,0.5,0.5,1.0],
        "is2":[0.4,0.08,0.14,0.035,0.28,0.28,0.96],
        "t62":[0.045,0.016,0.08,0.024,0.5,0.5,1.0],
        "t90":[0.08,0.025,0.06,0.02,0.5,0.5,1.0],
        "panther":[0.045,0.012,0.04,0.015,0.5,0.5,1.0],
        "tiger2":[0.035,0.012,0.04,0.015,0.5,0.5,1.0],
        "leopard1":[0.095,0.025,0.08,0.02,0.5,0.5,1.0],
        "leopard2":[0.065,0.025,0.05,0.015,0.5,0.5,1.0],
    }
    return hull_plans[shape] if hull else turret_plans[shape]

static func _cartoon_crown(d: Dictionary) -> Array:
    # Model-specific armor sections: cast shoulders, welded trapezoids and
    # modern wedges. Broad roofs replace the shared inflated/conical crew pod.
    # Entries are height, half-width, front/rear offsets and corner radius.
    var profiles := {
        "sherman":[[0,.72,.09,-.10,.88],[.16,1,0,0,1.20],[.45,1,.015,0,1.22],[.80,.91,.06,-.02,1.07],[.96,.80,.10,-.045,.88],[1,.78,.11,-.05,.86]],
        "pershing":[[0,.76,.07,-.06,.78],[.16,1,0,0,1.05],[.42,1,.018,0,1.10],[.74,.94,.075,-.025,1.0],[.96,.83,.14,-.065,.82],[1,.81,.15,-.075,.80]],
        "m60":[[0,.70,.10,-.08,.85],[.15,1,0,0,1.22],[.35,1,.025,0,1.24],[.72,.83,.13,-.015,1.02],[.95,.71,.16,-.05,.82],[1,.69,.17,-.055,.80]],
        "abrams":[[0,.88,.05,-.04,.64],[.10,1,0,0,.78],[.28,1,.02,0,.78],[.85,.99,.23,-.005,.64],[.98,.97,.25,-.01,.59],[1,.97,.25,-.012,.59]],
        "t34":[[0,.78,.055,-.035,.46],[.13,1,0,0,.52],[.35,.97,.045,-.018,.52],[.85,.79,.13,-.05,.47],[.97,.76,.14,-.06,.45],[1,.75,.145,-.065,.44]],
        "is2":[[0,.72,.065,-.05,1.02],[.15,1,0,0,1.28],[.38,1,.02,0,1.32],[.72,.92,.065,-.015,1.12],[.96,.79,.12,-.055,.96],[1,.76,.13,-.065,.91]],
        "t62":[[0,.70,.09,-.11,.95],[.12,.94,.015,-.018,1.28],[.27,1,0,0,1.38],[.56,.88,.065,-.075,1.15],[.85,.68,.165,-.19,.88],[1,.52,.225,-.245,.66]],
        "t90":[[0,.79,.05,-.03,.32],[.12,1,0,0,.44],[.32,1,.035,-.005,.44],[.76,.90,.15,-.035,.38],[.97,.85,.20,-.055,.34],[1,.84,.205,-.06,.33]],
        "panther":[[0,.87,.025,-.025,.19],[.11,1,0,0,.22],[.29,.96,.035,-.008,.21],[.85,.72,.155,-.045,.17],[.97,.70,.17,-.05,.16],[1,.69,.175,-.055,.15]],
        "tiger2":[[0,.88,.025,-.012,.25],[.12,1,0,0,.29],[.45,.99,.07,0,.28],[.86,.92,.18,-.018,.26],[.98,.91,.195,-.023,.25],[1,.90,.20,-.025,.24]],
        "leopard1":[[0,.75,.07,-.05,1.03],[.16,1,0,0,1.36],[.37,1,.025,0,1.36],[.72,.89,.12,-.03,1.18],[.96,.76,.18,-.08,.94],[1,.74,.19,-.09,.92]],
        "leopard2":[[0,.91,.015,-.012,.13],[.10,1,0,0,.15],[.34,1,.015,0,.15],[.89,.98,.035,-.004,.14],[.98,.975,.045,-.015,.14],[1,.97,.05,-.018,.13]],
    }
    # Welded towers use three structural bands: undercut, continuous main
    # plates, and a narrow roof shoulder. Intermediate rings are coplanar;
    # they must not create an accidental six-step rounded-box highlight.
    var welded := {
        "abrams":[[0,0.88,0.035,-0.04,0.64],[0.12,1,0,0,0.78],[0.86,0.99,0.18,-0.004,0.64],[1,0.94,0.205,-0.02,0.59]],
        "t34":[[0,0.78,0.055,-0.035,0.46],[0.13,1,0,0,0.52],[0.94,0.76,0.145,-0.062,0.45],[1,0.74,0.15,-0.068,0.44]],
        "t90":[[0,0.79,0.05,-0.03,0.32],[0.12,1,0,0,0.44],[0.92,0.84,0.145,-0.055,0.34],[1,0.82,0.16,-0.065,0.33]],
        "panther":[[0,0.87,0.025,-0.025,0.19],[0.11,1,0,0,0.22],[0.94,0.73,0.15,-0.075,0.16],[1,0.71,0.165,-0.09,0.15]],
        "tiger2":[[0,0.88,0.025,-0.012,0.25],[0.12,1,0,0,0.29],[0.94,0.85,0.11,-0.06,0.25],[1,0.83,0.12,-0.066,0.24]],
        "leopard2":[[0,0.91,0.015,-0.012,0.13],[0.1,1,0,0,0.15],[0.94,1,0.025,0,0.14],[1,0.97,0.045,-0.02,0.13]],
    }
    if welded.has(d.shape):
        var p: Array = welded[d.shape]
        var main_mid: Array = []
        var roof_mid: Array = []
        for axis in 5:
            main_mid.append(lerpf(p[1][axis],p[2][axis],.25))
            roof_mid.append(lerpf(p[2][axis],p[3][axis],.80))
        profiles[d.shape] = [p[0],p[1],main_mid,p[2],roof_mid,p[3]]
    var result: Array = []
    # Individual fighting compartments leave room for their rear engine decks;
    # T-34 keeps its stronger forward bias. Mounts follow the sections, not a
    # shared rearward offset. The existing gun centerline stays fixed.
    var front: float = d.center-d.length*.5
    for ring in profiles[d.shape]:
        result.append([d.base+.025+ring[0]*d.h,d.w*ring[1],front+ring[2]*d.length,
            front+d.length+ring[3]*d.length,minf(d.w*.56,d.length*.25)*ring[4]])
    return result

static func _cartoon_fender(g: Geometry, side: float, d: Dictionary, paint: Color) -> void:
    # One curved sheet with a flared nose and turned-down end, not stacked boxes.
    var profiles := {
        "sherman":[[-.505,.35,.75],[-.44,.51,.96],[-.28,.55,1.02],[.22,.55,1.0],[.43,.50,.90],[.49,.37,.72]],
        "pershing":[[-.50,.32,.50],[-.43,.46,.87],[-.28,.54,1.06],[.16,.54,1.04],[.43,.49,.92],[.49,.38,.66]],
        "m60":[[-.50,.36,.26],[-.43,.48,.70],[-.29,.55,1.08],[.19,.56,1.05],[.42,.53,.87],[.49,.43,.40]],
        "abrams":[[-.49,.43,.15],[-.40,.55,.52],[-.27,.57,1.03],[.27,.57,1.03],[.44,.53,.95],[.49,.41,.70]],
        "t34":[[-.50,.40,.70],[-.40,.53,.77],[-.23,.55,.80],[.24,.55,.80],[.42,.50,.75],[.49,.38,.65]],
        "is2":[[-.50,.39,.77],[-.43,.53,.80],[-.27,.56,.88],[.27,.56,.90],[.44,.53,.88],[.49,.41,.76]],
        "t62":[[-.50,.43,.72],[-.41,.54,.80],[-.25,.55,.80],[.25,.55,.84],[.44,.54,.84],[.49,.43,.72]],
        "t90":[[-.50,.41,.78],[-.41,.54,.85],[-.25,.55,.87],[.25,.55,.87],[.44,.54,.86],[.49,.41,.77]],
        "panther":[[-.50,.34,.76],[-.44,.54,.95],[-.27,.56,1.01],[.26,.56,1.01],[.45,.55,.97],[.49,.38,.84]],
        "tiger2":[[-.50,.37,1.03],[-.46,.55,1.03],[-.30,.56,1.03],[.22,.56,1.03],[.46,.56,1.03],[.49,.39,1.03]],
        "leopard1":[[-.50,.37,1.02],[-.45,.54,1.02],[-.27,.56,1.02],[.24,.56,1.02],[.45,.55,1.02],[.49,.37,.97]],
        "leopard2":[[-.50,.37,1.04],[-.45,.55,1.04],[-.27,.56,1.04],[.24,.56,1.04],[.45,.56,1.04],[.49,.40,1.00]]}
    var profile: Array = profiles[d.shape]
    var inner: Array[Vector3] = []
    var outer: Array[Vector3] = []
    for p in profile:
        inner.append(Vector3(side*(d.track_center-d.track_width*.5),d.track_height+.027+(p[1]-.55)*d.chassis*.70,p[0]*d.track_length))
        outer.append(Vector3(side*(d.track_center+d.track_width*.52*p[2]),d.track_height+.027+(p[1]-.55)*d.chassis*.70,p[0]*d.track_length))
    var down := Vector3(0,-.015,0)
    for i in profile.size()-1:
        var a := inner[i]; var b := inner[i+1]; var c := outer[i+1]; var e := outer[i]
        if side < 0: g.quad(e,c,b,a,paint,"armor")
        else: g.quad(a,b,c,e,paint,"armor")
        if side < 0: g.quad(e+down,c+down,c,e,Color("2a3331"),"rubber")
        else: g.quad(e,c,c+down,e+down,Color("2a3331"),"rubber")
        if side < 0: g.quad(a+down,b+down,c+down,e+down,paint.darkened(.3),"armor")
        else: g.quad(e+down,c+down,b+down,a+down,paint.darkened(.3),"armor")
    for index in [0,profile.size()-1]:
        var a: Vector3 = inner[index]; var b: Vector3 = outer[index]
        if (index == 0) == (side > 0): g.quad(a,b,b+down,a+down,paint,"armor")
        else: g.quad(b,a,a+down,b+down,paint,"armor")

static func _chassis_hull_sections(d: Dictionary) -> Array:
    # [height fraction, half width, front offset, rear offset, corner radius].
    # Separate welded/cast hulls, deck shoulders and lower transmission noses.
    var profiles := {
        "sherman":[[.23,.72,.13,-.05,.10],[.49,.95,0,0,.12],[.66,1,.055,0,.025],[1,.94,.25,-.045,.025]],
        "pershing":[[.22,.71,.16,-.045,.13],[.43,.97,0,0,.15],[.69,1,.09,-.01,.13],[1,.84,.30,-.055,.12]],
        "m60":[[.20,.64,.19,-.06,.19],[.43,.94,0,-.01,.17],[.70,1,.075,0,.16],[1,.91,.275,-.055,.15]],
        "abrams":[[.22,.79,.14,-.045,.035],[.40,1,0,0,.025],[.70,1,.23,0,.025],[1,.91,.41,-.035,.025]],
        "t34":[[.24,.62,.20,-.12,.04],[.44,1,0,0,.035],[.71,.84,.16,-.04,.03],[1,.69,.36,-.11,.025]],
        "is2":[[.22,.66,.18,-.075,.11],[.43,1,0,0,.09],[.70,.89,.115,-.025,.10],[1,.73,.29,-.075,.08]],
        "t62":[[.22,.66,.17,-.06,.035],[.42,1,0,0,.035],[.68,.90,.18,-.02,.035],[1,.72,.355,-.04,.03]],
        "t90":[[.22,.70,.14,-.055,.035],[.42,1,0,0,.025],[.74,.91,.23,-.025,.035],[1,.72,.37,-.055,.03]],
        "panther":[[.24,.70,.16,-.065,.025],[.43,1,0,0,.025],[.67,.97,.115,-.035,.025],[1,.78,.36,-.075,.022]],
        "tiger2":[[.22,.74,.15,-.055,.025],[.42,1,0,0,.025],[.72,.98,.15,-.025,.025],[1,.85,.37,-.06,.022]],
        "leopard1":[[.20,.65,.15,-.055,.045],[.40,.97,0,0,.045],[.63,1,.18,-.03,.045],[1,.84,.38,-.06,.035]],
        "leopard2":[[.22,.77,.14,-.045,.025],[.42,1,0,0,.025],[.70,1,.20,-.01,.025],[1,.92,.37,-.035,.025]]}
    var profile: Array = profiles[d.shape]
    # Low modern hulls need less downward growth during the existing suspension
    # travel. Limit the whole section profile, not individual belly vertices,
    # so welded slopes stay continuous and the authored wheel heights survive.
    var depth: float = minf(HULL_DEPTH_SCALE,
        (1.0-HULL_MIN_CLEARANCE/d.hull_top)/(1.0-float(profile[0][0])))
    var result: Array = []
    for ring in profile:
        var y: float = d.hull_top*(1.0-(1.0-ring[0])*depth)
        result.append([y,ring[1]*d.hull_width,
            -d.track_length*.46+ring[2]*d.chassis_z,d.track_length*.43+ring[3]*d.chassis_z,ring[4]*d.chassis_x])
    return result

static func _deck_grille(g: Geometry, center: Vector3, width: float, length: float, paint: Color, bars: int = 5) -> void:
    g.box(center,Vector3(width,.014,length),Color("202c2b"),"rubber")
    for i in bars:
        g.box(center+Vector3(0,.010,(float(i)+.5)/bars*length-length*.5),
            Vector3(width*.94,.012,length/float(bars)*.40),paint,"armor")

static func _chassis_details(g: Geometry, d: Dictionary, sections: Array, paint: Color) -> void:
    var scale: float = d.chassis
    var across: float = d.chassis_x
    var top: float = d.hull_top
    var rear: float = sections.back()[3]
    var front: float = sections.back()[2]
    var dark := paint.darkened(.22)
    # Deck hatches and small periscopes replace the same oversized driver's
    # window on every tank. Each is seated on its actual supporting plate.
    var hatches: Array = [-.16,.16] if d.shape in ["sherman","pershing","panther","tiger2"] else [.13] if d.shape in ["leopard1","leopard2"] else [-.13] if d.shape == "t62" else [0.0]
    if d.shape == "t34": hatches = []
    if d.shape == "is2":
        g.loft([[top*.72,.14*scale,front-.12*scale,front+.09*scale,.025*scale],
            [top+.022,.13*scale,front-.035*scale,front+.09*scale,.025*scale]],dark,Vector3.ZERO,"armor")
    for x in hatches:
        var center := Vector3(x*across,top+.012,front+.092*scale)
        g.loft([[center.y,.079*scale,-.075*scale,.075*scale,.04*scale],
            [center.y+.018,.070*scale,-.066*scale,.066*scale,.033*scale]],dark,Vector3(center.x,0,center.z),"armor")
        g.box(center+Vector3(0,.029,-.042*scale),Vector3(.077*scale,.022,.025*scale),Color("263f3f"),"metal")
    if d.shape == "t34":
        var lower: Array = sections[2]
        var upper: Array = sections[3]
        var a := Vector3(-.22*across,lower[0]+.012,lower[2]-.004)
        var b := Vector3(-.02*across,lower[0]+.012,lower[2]-.004)
        var c := Vector3(-.02*across,upper[0]-.012,upper[2]-.025)
        var e := Vector3(-.22*across,upper[0]-.012,upper[2]-.025)
        g.armor_panel([a,b,c,e],Vector3(0,.55,-.84),.009,dark,paint)
    if d.shape in ["sherman","pershing","t34","is2","panther","tiger2"]:
        # Bow machine-gun ball mount on the glacis, separate from the driver's hatch.
        var face := _casting_section(sections,top*.78)
        var at := Vector3(.22*across,face[0],face[2]-.018)
        g.tube(at+Vector3(0,0,.025),at-Vector3(0,0,.015),.042*scale,.030*scale,dark,"armor",8)
        g.tube(at-Vector3(0,0,.01),at-Vector3(0,0,.065),.013,.010,Color("303936"),"metal",6)
    if d.shape in ["m60","t62"]:
        for side in ([-1.0,1.0] if d.shape == "m60" else [1.0]):
            var at := Vector3(side*.31*across,top*.83,rear-.24*scale)
            g.loft([[at.y,.040*scale,-.15*scale,.15*scale,.01*scale],
                [at.y+.075*scale,.037*scale,-.145*scale,.145*scale,.012*scale]],paint,Vector3(at.x,0,at.z),"armor")
    if d.shape == "m60":
        g.loft([[top-.035,.29*across,rear-.33*scale,rear,.04*scale],
            [top+.026,.27*across,rear-.30*scale,rear-.015,.04*scale]],paint,Vector3.ZERO,"armor")
    # Distinct rear cooling arrangements remain legible from the game camera.
    if d.shape in ["panther","tiger2"]:
        for side in [-1.0,1.0]:
            var at := Vector3(side*.158*across,top+.018,rear-.17*scale)
            g.tube(at,at+Vector3(0,.018,0),.116*scale,.112*scale,dark,"armor",12)
            g.tube(at+Vector3(0,.019,0),at+Vector3(0,.021,0),.089*scale,.089*scale,Color("253330"),"rubber",12)
            for line in [-1.0,0.0,1.0]:
                g.box(at+Vector3(0,.025,line*.043*scale),Vector3(.16*scale,.010,.013*scale),paint,"armor")
            var exhaust := Vector3(side*.17*across,top*.58,rear+.025)
            g.tube(exhaust,exhaust+Vector3(0,.21*scale,0),.039*scale,.035*scale,dark,"armor",8)
            g.tube(exhaust+Vector3(0,.21*scale,0),exhaust+Vector3(0,.225*scale,0),.025*scale,.025*scale,Color("202a28"),"metal",8)
    elif d.shape == "abrams":
        _deck_grille(g,Vector3(0,top+.010,rear-.14*scale),.48*across,.19*scale,dark,5)
        g.box(Vector3(0,top*.62,rear+.015),Vector3(.52*across,.17*scale,.028),Color("1d2b29"),"rubber")
        for i in 4:
            g.box(Vector3(0,top*.49+i*.034*scale,rear+.033),Vector3(.50*across,.016,.026),dark,"armor")
    elif d.shape in ["leopard1","leopard2"]:
        for side in [-1.0,1.0]:
            _deck_grille(g,Vector3(side*.15*across,top+.012,rear-.16*scale),.22*across,.22*scale,dark,4)
            # Leopard 1 exhausts vent laterally; Leopard 2 uses rear grilles.
            var at := Vector3(side*.22*across,top*.73,rear-.11*scale) if d.shape == "leopard1" else Vector3(side*.18*across,top*.64,rear+.02)
            g.box(at,Vector3(.025,.10*scale,.23*scale) if d.shape == "leopard1" else Vector3(.24*across,.12*scale,.025),Color("24302e"),"rubber")
            for i in 3:
                g.box(at+Vector3(side*.012,i*.027*scale-.025*scale,.012),Vector3(.03,.011,.21*scale) if d.shape == "leopard1" else Vector3(.22*across,.012,.03),dark,"armor")
    else:
        var two_grilles: bool = d.shape in ["sherman","pershing","m60"]
        for side in ([-1.0,1.0] if two_grilles else [0.0]):
            _deck_grille(g,Vector3(side*.14*across,top+(.038 if d.shape == "m60" else .012),rear-.16*scale),(.20 if two_grilles else .38)*across,.24*scale,dark,4)
        if d.shape in ["t34","is2"]:
            for side in [-1.0,1.0]:
                var at := Vector3(side*.14*across,top*.60,rear+.013)
                g.tube(at,at+Vector3(0,-.055*scale,.055*scale),.034*scale,.032*scale,dark,"armor",8)
        elif d.shape in ["t62","t90"]:
            g.box(Vector3(-.40*across,top*.78,rear-.24*scale),Vector3(.04,.075*scale,.15*scale),Color("27332f"),"rubber")
        else:
            g.box(Vector3(0,top*.55,rear+.015),Vector3(.43*across,.10*scale,.035),dark,"armor")
    # Real glacis ERA rows and flexible side skirts distinguish T90 from T62.
    if d.shape == "t90":
        var lower: Array = sections[2]
        var upper: Array = sections[3]
        for side in [-1.0,1.0]:
            for row in 2:
                var lo := lerpf(lower[0],upper[0],row*.42+.02)
                var hi := lerpf(lower[0],upper[0],row*.42+.37)
                var a := _casting_section(sections,lo)
                var b := _casting_section(sections,hi)
                var x0: float = side*.035*across
                var x1: float = side*.22*across
                g.armor_panel([Vector3(x0,lo,a[2]-.004),Vector3(x1,lo,a[2]-.004),Vector3(x1,hi,b[2]-.004),Vector3(x0,hi,b[2]-.004)],Vector3(0,.65,-.76),.012,paint,dark)
    for side in [-1.0,1.0]:
        # Paired tow points on the rear are structural cues, not decorative rivets.
        g.box(Vector3(side*.29*across,top*.42,rear+.025),Vector3(.065*scale,.045*scale,.06*scale),dark,"armor")

static func make_tank(nation: int, enemy: bool = false, role: int = 0, player_id: int = 0, level: int = 0) -> Node3D:
    var d := _vehicle_profile(nation,enemy,role,level)
    var key := "tank_%s_%s_%s_%s_%s" % [clampi(nation,0,2),enemy,clampi(role,0,3),player_id%2 if not enemy else 0,clampi(level,0,3) if not enemy else 0]
    if _meshes.has(key): return _finish_vehicle(key,d,player_id)
    var hull := Geometry.new()
    var g := Geometry.new()
    var paint: Color = NATIONAL_PAINT[d.nation]
    var deep := paint.darkened(.24).lerp(Color("253839"),.20)
    var light := paint.lerp(Color("dbd6a5"),.25)
    var ink := Color("1c292b")
    var metal := Color("8d9890")
    var identity := Color.WHITE if enemy else Color("ebc553") if player_id%2 == 0 else Color("55dca2")
    var s: float = d.chassis
    var sx: float = d.chassis_x
    var sz: float = d.chassis_z
    var front: float = -d.track_length*.46
    var rear: float = d.track_length*.43
    var top: float = d.hull_top
    var width: float = d.hull_width
    var soft: float = .58 if d.shape in ["sherman","pershing","m60","is2","t62","leopard1"] else 0.0
    for side in [-1.0,1.0]:
        var track_key := key+("_left" if side < 0 else "_right")
        _meshes[track_key] = _gear_frames(d,side)[0]
        _mesh_groups[track_key] = ["metal","rubber"]
    var hull_sections := _chassis_hull_sections(d)
    var nose: float = hull_sections.back()[2]-front
    hull.cast_loft(hull_sections,paint,Vector3.ZERO,.32 if d.shape in ["pershing","is2"] else 0.0,false,false,1.0,_armor_plan(d.shape,true))
    for side in [-1.0,1.0]:
        _cartoon_fender(hull,side,d,paint)
        var lamp := Vector3(side*.225*sx,top+.006,front+nose+.03)
        if d.nation == 2 or d.shape == "abrams":
            hull.loft([[lamp.y-.036,.047,lamp.z-.042,lamp.z+.02,.010],
                [lamp.y+.033,.043,lamp.z-.035,lamp.z+.020,.010]],deep,Vector3(lamp.x,0,0),"armor")
            hull.box(lamp+Vector3(0,0,-.043),Vector3(.06,.031,.012),Color("f5e5a6"),"optic")
        elif d.nation == 0 or side < 0:
            var radius: float = (.045 if d.shape == "sherman" else .036)*s
            hull.tube(lamp+Vector3(0,-.095,.025),lamp+Vector3(0,0,.025),.032,.032,ink,"metal",8)
            hull.tube(lamp+Vector3(0,0,.045),lamp-Vector3(0,0,.055),radius,radius,ink,"rubber",10)
            hull.ring(lamp-Vector3(0,0,.056),Vector3(0,0,-1),radius,radius*.77,.020,light,"armor")
            hull.tube(lamp-Vector3(0,0,.06),lamp-Vector3(0,0,.076),radius*.76,radius*.70,Color("f5e5a6"),"optic",12)
        # A raised side badge retains the existing status/player information.
        hull.box(Vector3(side*(d.track_center+d.track_width*.55),minf(top-.025,d.track_height+.014),.14*sz),Vector3(.016,.058,.18*sz),identity)
        if d.nation == 2 and d.shape != "leopard1":
            # Three substantial plate sections leave the lower wheels and
            # curved track ends visible. The identity badge stays outside.
            for panel in 3:
                var z: float = (-.44+panel*.34)*sz
                var lower_edge: float = (.17 if panel == 0 else .23) if d.shape == "leopard2" else .32
                var thickness: float = (.027 if panel == 0 else .012) if d.shape == "leopard2" else .018
                var panel_paint: Color = paint.darkened(.16) if d.shape == "leopard2" and panel > 0 else paint
                hull.loft([[lower_edge*s,thickness*s,z,z+.29*sz,.006*s],
                    [maxf(d.track_height+.025,top-.04),(thickness+.004)*s,z-.009*sz,z+.299*sz,.008*s],
                    [maxf(d.track_height+.043,top-.022),thickness*s,z,z+.29*sz,.006*s]],panel_paint,
                    Vector3(side*(d.track_center+d.track_width*.52),0,0),"armor")
        elif d.skirts:
            for panel in 5:
                # Abrams has a long rigid flank around its turbine hull;
                # T90's shorter flexible skirt leaves its end wheels exposed.
                var pitch: float = .27 if d.shape == "abrams" else .22
                var z: float = ((-.66 if d.shape == "abrams" else -.52)+panel*pitch)*sz
                var length: float = (pitch-.015)*sz
                var low: float = .17 if d.shape == "abrams" else .15
                if d.shape == "abrams" and panel == 0:
                    # A swept leading skirt corner clears the raised idler,
                    # instead of presenting Leopard 2's full-width square bow.
                    var leading_x: float = side*(d.track_center-.020*sx)
                    var trailing_x: float = side*(d.track_center+d.track_width*.54)
                    hull.armor_panel([Vector3(leading_x,.29*s,z),Vector3(trailing_x,low*s,z+length),
                        Vector3(trailing_x,d.track_height+.028,z+length),Vector3(leading_x,d.track_height+.028,z)],
                        Vector3(side,.0,-.4).normalized(),.012,paint,paint.darkened(.18))
                    continue
                hull.loft([[low*s,.014*s,z,z+length,.008*s],
                    [d.track_height+.028,.022*s,z,z+length,.008*s]],
                    paint.darkened(.10) if d.shape == "t90" else paint,
                    Vector3(side*(d.track_center+d.track_width*.53),0,0),"armor")
    _chassis_details(hull,d,hull_sections,paint)
    if d.shape == "sherman":
        # A cast transmission chin under the welded upper glacis distinguishes
        # the high early chassis from the later long sloping hulls.
        hull.loft([[.19,width*.81,front-.018,front+.15,.11*s],
            [.27,width*.90,front-.025,front+.16,.12*s],
            [.34,width*.82,front+.025,front+.17,.10*s]],deep,Vector3.ZERO,"armor")
    var crown := _cartoon_crown(d)
    var crown_rings := Geometry.armor_rings(crown,_armor_plan(d.shape))
    g.loft([[d.hull_top-.045,d.w*.58,crown[0][2]+.10,crown[0][3]-.10,.10*d.turret],
        [d.base+.015,d.w*.70,crown[0][2]+.065,crown[0][3]-.065,.14*d.turret]],
        paint.darkened(.10),Vector3.ZERO,"armor")
    g.loft([[d.base+.013,d.w*.71,crown[0][2]+.06,crown[0][3]-.06,.14*d.turret],
        [d.base+.031,d.w*.72,crown[0][2]+.055,crown[0][3]-.055,.14*d.turret]],ink,Vector3.ZERO,"rubber")
    g.cast_loft(crown,paint,Vector3.ZERO,soft,false,false,1.0,_armor_plan(d.shape),
        d.shape in ["sherman","pershing","m60","is2","t62","leopard1"])
    var gun_root: float = _casting_section(crown,d.muzzle.y)[2]+.065
    if d.shape in ["panther","leopard1","is2"]:
        # Rounded mantlet on the trapezoid Panther / cast Leopard 1, unlike
        # Tiger II's broad plate or the rectangular modern A4 gun opening.
        g.tube(Vector3(-d.w*.53,d.muzzle.y,gun_root+.035),Vector3(d.w*.53,d.muzzle.y,gun_root+.035),
            .105,.105,deep,"armor",10)
    elif d.shape == "tiger2":
        # Production turret: flat front plate around a compact rounded collar.
        g.tube(Vector3(0,d.muzzle.y,gun_root+.045),Vector3(0,d.muzzle.y,gun_root-.045),
            .125,.084,deep,"armor",10)
    elif d.shape in ["sherman","pershing","m60","abrams","leopard2"]:
        _seated_mantlet(g,d,gun_root,paint)
    _cannon(g,d.muzzle,gun_root,d.gun_radius,paint,d.shape in ["pershing","is2","panther","tiger2"])
    var roof: Array = crown.back()
    var roof_y: float = roof[0]
    var commander_side := 1.0 if d.nation == 0 or d.shape in ["leopard1","leopard2","t90"] else -1.0
    var hatch := Vector3(commander_side*d.w*.30,roof_y,(roof[2]+roof[3])*.5+.045)
    if d.shape == "t34": hatch = Vector3(-d.w*.27,roof_y,(roof[2]+roof[3])*.5+.02)
    if d.shape == "t62": hatch = Vector3(commander_side*d.w*.22,roof_y,(roof[2]+roof[3])*.5+.025)
    var hatch_radius: float = (.072+.005*d.tier)*(.90 if d.nation == 1 else 1.0)
    if d.shape in ["sherman","pershing","m60"]:
        # A proper raised commander's drum, seated on the casting rather than
        # a tall antenna or an isolated decoration supplying the silhouette.
        var drum: float = .068 if d.shape == "m60" else .035
        g.tube(hatch,hatch+Vector3(0,drum-.002,0),hatch_radius*1.12,hatch_radius,paint,"armor",10)
        g.tube(hatch+Vector3(0,drum-.024,0),hatch+Vector3(0,drum+.002,0),hatch_radius*1.04,hatch_radius*1.04,ink,"rubber",10)
        hatch.y += drum
    if d.shape == "t34":
        # Early L-11 two-man turret: one broad low hatch, no later cupola.
        hatch = Vector3(0,roof_y,(roof[2]+roof[3])*.5+.02)
        g.loft([[roof_y+.003,.151,-.107,.107,.046],
            [roof_y+.024,.140,-.097,.097,.044]],deep,Vector3(0,0,hatch.z),"armor")
        g.box(hatch+Vector3(0,.029,.10),Vector3(.14,.014,.022),metal,"metal")
    else:
        g.tube(hatch,hatch+Vector3(0,.022,0),hatch_radius*1.10,hatch_radius*1.10,ink,"rubber",10)
        g.tube(hatch+Vector3(0,.018,0),hatch+Vector3(0,.050,0),hatch_radius,hatch_radius*.88,light,"armor",10)
        g.box(hatch+Vector3(0,.066,0),Vector3(.052,.014,.018),deep,"armor")
    # A seated rim gives the round crew hatch weight; one / two roof dashes
    # use identical triangles for the two players, with a dark center gap for P2.
    var mark_x: float = -commander_side*d.w*.32
    if d.shape == "t62": mark_x = -commander_side*d.w*.22
    var mark_z: float = (roof[2]+roof[3])*.5
    var mark_w: float = .083+.008*d.tier
    var mark_y: float = roof_y+(.025 if d.shape == "t34" else 0.0)
    g.box(Vector3(mark_x,mark_y+.009,mark_z),Vector3(mark_w+.020,.018,.185),ink,"rubber")
    for stripe in 3:
        var tint := identity if enemy or player_id%2 == 0 or stripe != 1 else ink
        var z: float = mark_z-.075+stripe*.05
        g.quad(Vector3(mark_x-mark_w*.5,mark_y+.019,z),Vector3(mark_x-mark_w*.5,mark_y+.019,z+.05),
            Vector3(mark_x+mark_w*.5,mark_y+.019,z+.05),Vector3(mark_x+mark_w*.5,mark_y+.019,z),tint)
    # Functional roof fittings: a small separate loader hatch and hinge;
    # the early two-man T34 keeps its single hatch. Marks remain unobstructed.
    if d.shape != "t34":
        var loader := Vector3(-commander_side*d.w*.38,roof_y+.006,roof[3]-.078)
        if d.shape == "t62": loader = Vector3(0,roof_y+.006,roof[3]-.073)
        if d.shape in ["panther","tiger2","leopard2"]:
            g.box(loader+Vector3(0,.008,0),Vector3(.088,.016,.094),deep,"armor")
        else:
            g.tube(loader,loader+Vector3(0,.016,0),.044 if d.shape == "t62" else .049,.041 if d.shape == "t62" else .046,deep,"armor",8)
        g.box(loader+Vector3(0,.020,.046),Vector3(.048,.012,.017),metal,"metal")
    # The commander's sight and hatch handle read as separate seats and fittings.
    var periscope := hatch+Vector3(-.080 if d.shape == "t34" else 0.0,.046,-hatch_radius*.76)
    g.box(periscope,Vector3(.047,.027,.022),deep,"armor")
    g.quad(periscope+Vector3(-.018,.010,-.012),periscope+Vector3(-.018,-.006,-.012),
        periscope+Vector3(.018,-.006,-.012),periscope+Vector3(.018,.010,-.012),Color("203631"),"metal")
    # National/model landmarks use large structural parts, not noisy microdetail.
    if d.shape == "leopard1":
        var searchlight := Vector3(0,d.muzzle.y+.095,gun_root+.035)
        g.box(searchlight,Vector3(.13,.09,.09),deep,"armor")
        g.box(searchlight-Vector3(0,0,.047),Vector3(.105,.065,.012),Color("d1cfad"),"metal")
    if d.shape == "m60":
        # A3 laser/rangefinder port on the right; not the A1 mantlet searchlight.
        var optic_y: float = d.muzzle.y+.015
        var ring := Geometry.armor_slice(crown_rings,optic_y)
        var optic: Vector3 = ring[3].lerp(ring[4],.08)+Vector3(-.004,0,0)
        g.box(optic,Vector3(.075,.075,.12),deep,"armor")
        g.box(optic+Vector3(.040,0,0),Vector3(.010,.043,.068),Color("314c49"),"metal")
        g.tube(hatch+Vector3(0,.047,0),hatch+Vector3(0,.095,0),.056,.048,paint,"armor",10)
    if d.shape in ["panther","tiger2"]:
        # Raise the existing hatch into a low commander's cupola.
        g.tube(hatch-Vector3(0,.008,0),hatch+Vector3(0,.030,0),hatch_radius*1.17,hatch_radius*1.13,paint,"armor",8)
    if d.shape in ["m60","abrams","t62","t90","leopard1","leopard2"]:
        var span: float = gun_root-d.muzzle.z
        var rear_z: float = d.muzzle.z+span*(.46 if d.shape == "t62" else .56)
        var front_z: float = d.muzzle.z+span*(.25 if d.shape == "t62" else .35)
        # Bore evacuator stays on the existing barrel; the firing mount is fixed.
        g.tube(Vector3(0,d.muzzle.y,rear_z),Vector3(0,d.muzzle.y,front_z),
            d.gun_radius*1.31,d.gun_radius*1.24,paint.darkened(.10),"armor",10)
    if d.shape == "leopard2":
        var sight := Vector3(d.w*.58,roof_y+.052,roof[2]+.115)
        g.box(sight,Vector3(.10,.067,.11),deep,"armor")
        g.box(sight-Vector3(0,0,.057),Vector3(.075,.039,.012),Color("668c87"),"metal")
    if d.shape == "is2":
        # Small side tanks belong to this reference configuration. Optional
        # rear barrels are not copied onto every Soviet chassis.
        for side in [-1.0,1.0]:
            var drum := Vector3(side*.29*sx,top+.056,rear-.115)
            var drum_radius: float = .045
            # Saddles bridge the rounded tank to its narrower deck shoulder.
            # Their feet enter the actual deck, rather than leaving a bright gap.
            for offset in [-.085,.085]:
                hull.box(Vector3(drum.x,top+.012,drum.z+offset),
                    Vector3(.082,.028,.026),deep,"armor")
            hull.tube(drum-Vector3(0,0,.13),drum+Vector3(0,0,.13),drum_radius,drum_radius,deep,"armor",10)
            hull.ring(drum,Vector3(0,0,1),drum_radius+.003,drum_radius-.014,.020,metal,"metal")
    if d.shape == "t90":
        # Three broad cheek tiles follow the casting's actual slope. Seating
        # their four corners on both rings avoids floating teeth on the dome.
        var lower := Geometry.armor_slice(crown_rings,d.base+.025+d.h*.30)
        var upper := Geometry.armor_slice(crown_rings,d.base+.025+d.h*.64)
        for side in [-1.0,1.0]:
            var rows: Array = []
            for ring in [lower,upper]:
                var row: Array[Vector3] = [ring[1],ring[2],ring[3],ring[3].lerp(ring[4],.40)]
                if side < 0:
                    for i in row.size(): row[i].x = -row[i].x
                rows.append(row)
            for panel in 3:
                var a: Vector3 = rows[0][panel].lerp(rows[0][panel+1],.08)
                var b: Vector3 = rows[0][panel].lerp(rows[0][panel+1],.92)
                var c: Vector3 = rows[1][panel].lerp(rows[1][panel+1],.92)
                var e: Vector3 = rows[1][panel].lerp(rows[1][panel+1],.08)
                var normal: Vector3 = (b-a).cross(e-a).normalized()*side
                g.armor_panel([a,b,c,e],normal,.050,paint.darkened(.06),deep)
    if d.shape == "abrams":
        # Wide low bustle and open stowage rails, not a fake rear radiator.
        var z: float = crown[2][3]
        var y: float = d.base+d.h*.45
        g.box(Vector3(0,y,z+.025),Vector3(d.w*1.48,.055,.13),deep,"armor")
        g.box(Vector3(0,y+.10,z+.085),Vector3(d.w*1.53,.018,.018),metal,"metal")
        for side in [-1.0,1.0]:
            g.box(Vector3(side*d.w*.74,y+.047,z+.085),Vector3(.018,.11,.018),metal,"metal")
    if d.shape == "t90":
        var section := _casting_section(crown,d.muzzle.y-.015)
        for side in [-1.0,1.0]:
            var at := Vector3(side*d.w*.53,d.muzzle.y-.005,section[2]+.045)
            g.box(at,Vector3(.085,.068,.065),deep,"armor")
            # Dark optical lenses; they are not glowing team/status markers.
            g.box(at-Vector3(0,0,.035),Vector3(.055,.043,.010),Color("554135"),"metal")
    _instance(key+"_hull",hull).free()
    _instance(key,g).free()
    return _finish_vehicle(key,d,player_id)

static func _part_pose(part: Node3D, pivot: Vector3, rise: float, pitch: float, roll: float) -> void:
    # Rigid armor and running gear: rotate about their suspension seats, never
    # scale a casting or move the simulation root. Shadow geometry follows too.
    var basis := Basis.from_euler(Vector3(pitch,0,roll))
    part.transform = Transform3D(basis,pivot-basis*pivot+Vector3(0,rise,0))

static func reset_running_gear(vehicle: Node3D) -> void:
    # Explicit new-game/stage/respawn boundary. Pause uses a zero-dt rebase,
    # so it does not reset a visible wheel or disturb the established springs.
    vehicle.set_meta("gear_state",{"initialized":false,"x":0.0,"z":0.0,"yaw":0.0,
        "left_travel":0.0,"right_travel":0.0,"left_frame":0,"right_frame":0,"creating":false})
    var left_frames: Array = vehicle.get_meta("gear_frames_left")
    var right_frames: Array = vehicle.get_meta("gear_frames_right")
    vehicle.get_node("TrackLeft").mesh = left_frames[0]
    vehicle.get_node("TrackRight").mesh = right_frames[0]

static func _update_running_gear(vehicle: Node3D, data: Dictionary, dt: float) -> void:
    var gear: Dictionary = vehicle.get_meta("gear_state")
    var x := float(data.get("x",vehicle.position.x))
    var z := float(data.get("z",vehicle.position.z))
    var yaw := float(data.get("yaw",0.0))
    if not is_finite(x) or not is_finite(z) or not is_finite(yaw): return
    var creating := float(data.get("creating",0.0)) > 0.0
    var delta := Vector2(x-float(gear.x),z-float(gear.z))
    var step := clampf(dt,0.0,.1)
    var frozen := float(data.get("frozen",0.0)) > 0.0
    # Normal LAN snapshots can batch many native ticks. Lifecycle resets are
    # explicit; this generous guard only rejects implausible cross-map jumps.
    var teleported := bool(gear.initialized) and delta.length_squared() > 64.0
    if step > 0.0 and not frozen and (teleported or creating and not bool(gear.creating)):
        reset_running_gear(vehicle)
        gear = vehicle.get_meta("gear_state")
    if bool(gear.initialized) and step > 0.0 and not creating and not frozen:
        # Native yaw is the inverse of Godot root yaw. Sideways ice drift and
        # lane snapping do not become forward tread travel; blocked moving
        # input also leaves the gear still because no displacement occurred.
        var distance := delta.dot(Vector2(sin(yaw),-cos(yaw)))
        var turn := wrapf(yaw-float(gear.yaw),-PI,PI)*float(vehicle.get_meta("gear_half_width"))
        if absf(distance) < 0.000001: distance = 0.0
        if absf(turn) < 0.000001: turn = 0.0
        gear.left_travel = float(gear.left_travel)+distance+turn
        gear.right_travel = float(gear.right_travel)+distance-turn
        var cycle := float(vehicle.get_meta("gear_cycle"))
        var left_frame := int(floor(fposmod(float(gear.left_travel)/cycle,1.0)*GEAR_FRAME_COUNT))
        var right_frame := int(floor(fposmod(float(gear.right_travel)/cycle,1.0)*GEAR_FRAME_COUNT))
        if left_frame != int(gear.left_frame):
            var frames: Array = vehicle.get_meta("gear_frames_left")
            vehicle.get_node("TrackLeft").mesh = frames[left_frame]
            gear.left_frame = left_frame
        if right_frame != int(gear.right_frame):
            var frames: Array = vehicle.get_meta("gear_frames_right")
            vehicle.get_node("TrackRight").mesh = frames[right_frame]
            gear.right_frame = right_frame
    gear.x = x
    gear.z = z
    gear.yaw = yaw
    gear.creating = creating
    gear.initialized = true

static func set_vehicle_state(vehicle: Node3D, data: Dictionary, dt: float) -> void:
    _update_running_gear(vehicle,data,dt)
    var body: MeshInstance3D = vehicle.get_node("Body")
    var hull: MeshInstance3D = vehicle.get_node("Hull")
    var left: MeshInstance3D = vehicle.get_node("TrackLeft")
    var right: MeshInstance3D = vehicle.get_node("TrackRight")
    var motion: Dictionary = vehicle.get_meta("motion")
    var moving := bool(data.get("moving",false))
    var frozen := float(data.get("frozen",0.0)) > 0.0
    var step := clampf(dt,0.0,.1) if not frozen else 0.0
    var yaw := float(data.get("yaw",0.0))
    if not bool(motion.initialized):
        motion.moving = moving
        motion.yaw = yaw
        motion.initialized = true
    if step > 0.0:
        if moving != bool(motion.moving):
            # Start compresses the suspension; stopping sends one soft recoil
            # through it. Bounded springs settle rather than random jitter.
            motion.velocity += -.68 if moving else .46
        var turn := wrapf(yaw-float(motion.yaw),-PI,PI)
        motion.roll_velocity = clampf(float(motion.roll_velocity)+turn*.24,-.50,.50)
        motion.moving = moving
        motion.yaw = yaw
        motion.time = float(motion.time)+step
        var substeps := maxi(1,ceili(step*120.0))
        var h := step/float(substeps)
        for _substep in substeps:
            motion.drive = lerpf(float(motion.drive),1.0 if moving else 0.0,1.0-exp(-h*9.0))
            motion.velocity += (-float(motion.spring)*105.0-float(motion.velocity)*9.5)*h
            motion.spring = clampf(float(motion.spring)+float(motion.velocity)*h,-.055,.055)
            motion.roll_velocity += (-float(motion.roll)*115.0-float(motion.roll_velocity)*11.0)*h
            motion.roll = clampf(float(motion.roll)+float(motion.roll_velocity)*h,-.043,.043)
        var time: float = motion.time
        var phase := time*11.6+sin(time*1.3)*.16+float(vehicle.get_meta("phase",0.0))
        var drive: float = motion.drive
        var suspension := float(motion.spring)
        var breath := sin(time*2.15+float(vehicle.get_meta("phase",0.0)))
        var rise := (breath*.015)*(1.0-drive)+drive*(.011+sin(phase)*.027)+suspension
        var pitch := drive*sin(phase+.52)*.038-suspension*.43
        var roll := drive*sin(phase*.5)*.013+float(motion.roll)
        _part_pose(hull,Vector3(0,.30,0),rise,pitch,roll)
        # The pod follows the hull with a short vertical/pitch delay. Its yaw
        # remains exact, so the gun keeps the native driving/fire direction.
        var follow := 1.0-exp(-step*13.0)
        motion.turret_y = lerpf(float(motion.turret_y),rise+drive*sin(phase-.62)*.014+breath*.006*(1.0-drive),follow)
        motion.turret_pitch = lerpf(float(motion.turret_pitch),pitch*.73,follow)
        motion.turret_roll = lerpf(float(motion.turret_roll),roll*.73,follow)
        _part_pose(body,vehicle.get_meta("motion_pivot"),motion.turret_y,motion.turret_pitch,motion.turret_roll)
        var strength := .52 if bool(vehicle.get_meta("wheeled")) else 1.0
        var half_length: float = vehicle.get_meta("track_half_length")
        for index in 2:
            var track: MeshInstance3D = left if index == 0 else right
            var track_phase := phase+float(index)*.95
            var track_pitch := sin(track_phase)*.027*drive*strength
            # Compensate the tilted belt's lowest end: tracks do not sink into
            # the ground. Fore/aft rock and opposite phase suggest traction.
            var track_rise := absf(track_pitch)*half_length+(.5+.5*sin(track_phase+.75))*.014*drive*strength
            _part_pose(track,Vector3(0,.22,0),track_rise,track_pitch,0.0)
    vehicle.set_meta("motion",motion)
    vehicle.get_node("Boat").visible = bool(data.get("boat",false))
    var parts: Array = vehicle.get_meta("vehicle_parts")
    var bounds := body.transform*body.mesh.get_aabb()
    for part in parts:
        bounds = bounds.merge(part.transform*part.mesh.get_aabb())
    if bool(data.get("boat",false)):
        var boat_body: MeshInstance3D = vehicle.get_node("Boat/Body")
        bounds = bounds.merge(vehicle.get_node("Boat").transform*boat_body.mesh.get_aabb())
    vehicle.set_meta("visual_bounds",bounds)
    if not bool(vehicle.get_meta("enemy",false)):
        return
    vehicle.get_node("Frozen").visible = frozen
    var armor := clampi(int(data.get("armor",1)),1,4)
    var status: Color = [Color("a1c2cf"),Color("718b40"),Color("cfa465"),Color("279770")][armor-1]
    var carrier_flash := bool(data.get("carries_bonus",data.get("bonus_carrier",false))) and sin(float(motion.time)*8.1+float(vehicle.get_meta("phase",0.0))*.75) > .2
    if carrier_flash: status = Color("df7259")
    # Neutral brightness steps preserve national hue when armor is damaged.
    # The original health/carrier colors occupy only roof and shoulder patches.
    var brightness: float = [1.0,.96,.92,.88][armor-1]
    if carrier_flash: brightness *= 1.18
    for part in parts:
        var groups: Array = part.get_meta("surface_groups",[])
        for group in ["armor","paint"]:
            var surface := groups.find(group)
            if surface < 0: continue
            var material := part.get_surface_override_material(surface) as StandardMaterial3D
            if material == null:
                material = _material(group).duplicate() as StandardMaterial3D
                part.set_surface_override_material(surface,material)
            material.albedo_color = Color(brightness,brightness,brightness) if group == "armor" else status.lightened(.18)

static func _roof_height(x: float, z: float, kind: int, parity_x: int, parity_z: int, variant: int) -> float:
    var eave := .73+variant*.06
    var lot_x := x+float(parity_x)-.5
    var lot_z := z+float(parity_z)-.5
    if kind == 0:
        # A steep lower pitch and shallow crown make a readable barn-like
        # shoulder. The ridge remains at its original height and lot seam.
        var distance := absf(lot_x)
        var rise := lerpf(.70,0.0,(distance-.52)/.48) if distance > .52 else lerpf(1.0,.70,distance/.52)
        return eave+rise*(.30+variant*.06)
    if kind == 1:
        return eave+.015
    if kind == 2:
        return eave+minf(1.0-absf(lot_x),1.0-absf(lot_z))*.37
    # The shop roof has an off-center ridge and a short rear pitch. This
    # breaks the old four equally flat-looking roof blocks at game scale.
    return eave+(.26*(lot_z+1.0)/1.35 if lot_z <= .35 else lerpf(.26,.11,(lot_z-.35)/.65))

static func _roof_cuts(first: float, last: float, kind: int, parity: int, x_axis: bool) -> Array[float]:
    var cuts: Array[float] = [first]
    var crease := 0.0
    if kind == 0 and x_axis:
        # Local x of the gambrel shoulder on this half of the lot.
        crease = -.02 if parity == 0 else .02
        if crease > first+.00001 and crease < last-.00001: cuts.append(crease)
    elif kind == 3 and not x_axis:
        crease = .85-float(parity)
        if crease > first+.00001 and crease < last-.00001: cuts.append(crease)
    cuts.append(last)
    return cuts

static func _roof_top(g: Geometry, area: Rect2, kind: int, px: int, pz: int, variant: int, lift: float, color: Color) -> void:
    var xs := _roof_cuts(area.position.x,area.end.x,kind,px,true)
    var zs := _roof_cuts(area.position.y,area.end.y,kind,pz,false)
    for xi in xs.size()-1:
        for zi in zs.size()-1:
            var a := Vector3(xs[xi],_roof_height(xs[xi],zs[zi],kind,px,pz,variant)+lift,zs[zi])
            var b := Vector3(xs[xi],_roof_height(xs[xi],zs[zi+1],kind,px,pz,variant)+lift,zs[zi+1])
            var c := Vector3(xs[xi+1],_roof_height(xs[xi+1],zs[zi+1],kind,px,pz,variant)+lift,zs[zi+1])
            var d := Vector3(xs[xi+1],_roof_height(xs[xi+1],zs[zi],kind,px,pz,variant)+lift,zs[zi])
            if kind == 2 and px != pz:
                g.tri(a,b,d,color)
                g.tri(b,c,d,color)
            else:
                g.tri(a,b,c,color)
                g.tri(a,c,d,color)

static func _roof_piece(g: Geometry, x0: float, x1: float, z0: float, z1: float, kind: int, px: int, pz: int, variant: int, color: Color) -> void:
    _roof_top(g,Rect2(x0,z0,x1-x0,z1-z0),kind,px,pz,variant,0.0,color)
    var xs := _roof_cuts(x0,x1,kind,px,true)
    var zs := _roof_cuts(z0,z1,kind,pz,false)
    var wall_top := .66+variant*.06
    for xi in xs.size()-1:
        for z in [z0,z1]:
            var first := Vector3(xs[xi],wall_top,z)
            var last := Vector3(xs[xi+1],wall_top,z)
            var high_first := Vector3(first.x,_roof_height(first.x,z,kind,px,pz,variant),z)
            var high_last := Vector3(last.x,_roof_height(last.x,z,kind,px,pz,variant),z)
            if z == z0: g.quad(first,high_first,high_last,last,color.darkened(.14))
            else: g.quad(last,high_last,high_first,first,color.darkened(.14))
    for zi in zs.size()-1:
        for x in [x0,x1]:
            var first := Vector3(x,wall_top,zs[zi])
            var last := Vector3(x,wall_top,zs[zi+1])
            var high_first := Vector3(x,_roof_height(x,first.z,kind,px,pz,variant),first.z)
            var high_last := Vector3(x,_roof_height(x,last.z,kind,px,pz,variant),last.z)
            if x == x1: g.quad(first,high_first,high_last,last,color.darkened(.14))
            else: g.quad(last,high_last,high_first,first,color.darkened(.14))

static func _roof_inlay(g: Geometry, area: Rect2, kind: int, px: int, pz: int, variant: int, lift: float, color: Color) -> void:
    _roof_top(g,area,kind,px,pz,variant,lift,color)

static func _building_details(g: Geometry, kind: int, px: int, pz: int, variant: int, plaster: Color, roof: Color) -> void:
    # Broad authored roof courses, reveals and coping: no texture noise or RNG.
    # One shared paint surface, bounded by the intact tile's existing footprint.
    var seam := roof.darkened(.28)
    var trim := plaster.lightened(.24)
    var sill_shadow := Color("655e50")
    if kind in [0,3]:
        for course in 2:
            var at := -.30+course*.50
            var area := Rect2(at,-.495,.021,.99) if kind == 0 else Rect2(-.495,at,.99,.021)
            _roof_inlay(g,area,kind,px,pz,variant,.006,seam)
        if kind == 0:
            for course in 2:
                for joint in 2:
                    var x := -.29+course*.50
                    var z := -.29+joint*.53+(.10 if course%2 else 0.0)
                    _roof_inlay(g,Rect2(x,z,minf(.475,.495-x),.015),kind,px,pz,variant,.007,seam.lightened(.07))
            var ridge_x := .455 if px == 0 else -.495
            _roof_inlay(g,Rect2(ridge_x,-.495,.04,.99),kind,px,pz,variant,.018,roof.lightened(.18))
    elif kind == 1:
        for course in 2:
            _roof_inlay(g,Rect2(-.48,-.25+course*.50,.96,.016),kind,px,pz,variant,.006,seam)
    elif kind == 2:
        var outer_x := -.47 if px == 0 else .47
        var outer_z := -.47 if pz == 0 else .47
        var ridge_start := Vector3(outer_x,_roof_height(outer_x,outer_z,kind,px,pz,variant)+.012,outer_z)
        var ridge_end := Vector3(-outer_x,_roof_height(-outer_x,-outer_z,kind,px,pz,variant)+.012,-outer_z)
        g.tube(ridge_start,ridge_end,.012,.012,roof.darkened(.08),"paint",5)
    # Eaves keep a distinct dark underside and a narrow warm lip at exposed lot
    # edges. Adjoining cells meet exactly; no geometry leaks into nearby roads.
    var edge_z := -1.0 if pz == 0 else 1.0
    var edge_x := -1.0 if px == 0 else 1.0
    var eave_y := .665+variant*.06
    g.box(Vector3(0,eave_y,edge_z*.482),Vector3(.988,.034,.034),sill_shadow)
    g.box(Vector3(edge_x*.482,eave_y,0),Vector3(.034,.034,.988),sill_shadow)
    var frontage := posmod(px+variant,2)
    for side in [-1.0,1.0]:
        var windows: Array = []
        if kind == 1:
            windows = [[0.0,.435,.65,.235]] if frontage == 0 else [[0.0,.35,.44,.42]]
        elif kind == 2:
            windows = [[-.17 if frontage == 0 else .17,.39,.37,.34]]
        elif frontage == 0:
            windows = [[-.23,.425,.21,.25],[.23,.425,.21,.25]]
        else:
            windows = [[-.195,.343,.28,.40],[.235,.43,.22,.23]]
        for opening in windows:
            var x := float(opening[0])
            var y := float(opening[1])
            var width := float(opening[2])
            var height := float(opening[3])
            for jamb in [-1.0,1.0]:
                g.box(Vector3(x+jamb*(width*.5+.011),y,side*.490),Vector3(.026,height+.040,.018),trim)
            # Window glazing occupies only the upper portion of the deep reveal;
            # retaining its dark lower band keeps the opening visibly recessed.
            if height < .38:
                var glass := [Vector3(x-width*.36,y-height*.01,side*.496),Vector3(x+width*.36,y-height*.01,side*.496),Vector3(x+width*.36,y+height*.27,side*.496),Vector3(x-width*.36,y+height*.27,side*.496)]
                if side > 0:
                    g.quad(glass[0],glass[1],glass[2],glass[3],Color("6b9890"))
                else:
                    g.quad(glass[3],glass[2],glass[1],glass[0],Color("6b9890"))
    if kind == 0 and px == 1 and pz == 0:
        var chimney_x := .13
        var chimney_z := .15
        var roof_y := _roof_height(chimney_x,chimney_z,kind,px,pz,variant)
        var old_roof_y := .73+variant*.06+(1.0-absf(chimney_x+float(px)-.5))*(.30+variant*.06)
        var chimney_height := .21-(roof_y-old_roof_y)
        g.box(Vector3(chimney_x,roof_y+.005,chimney_z),Vector3(.26,.065,.25),Color("646b60"))
        g.box(Vector3(chimney_x,roof_y+.005+chimney_height*.5,chimney_z),Vector3(.16,chimney_height,.16),Color("a67c59"))
        g.box(Vector3(chimney_x,old_roof_y+.218,chimney_z),Vector3(.21,.035,.21),trim)
        g.quad(Vector3(chimney_x-.057,old_roof_y+.237,chimney_z-.057),Vector3(chimney_x-.057,old_roof_y+.237,chimney_z+.057),Vector3(chimney_x+.057,old_roof_y+.237,chimney_z+.057),Vector3(chimney_x+.057,old_roof_y+.237,chimney_z-.057),Color("35423c"))

static func make_environment_edges(rows: Array, masks: Array) -> Node3D:
    # A single stage-local mesh paints contact shoulders on existing open
    # ground. It neither fills a map cell nor creates a collision/cover node.
    # No coordinate RNG, per-frame rebuilds or new material are required.
    var root := Node3D.new()
    if rows.size() != 26 or masks.size() != 676:
        return root
    var g := Geometry.new()
    var directions: Array[Vector2i] = [Vector2i(0,-1),Vector2i(1,0),Vector2i(0,1),Vector2i(-1,0)]
    for row in 26:
        if str(rows[row]).length() != 26: return root
    for row in 26:
        for col in 26:
            if str(rows[row])[col] not in ["."," "]: continue
            for direction in directions:
                var neighbor: Vector2i = Vector2i(col,row)+direction
                if neighbor.x < 0 or neighbor.x >= 26 or neighbor.y < 0 or neighbor.y >= 26: continue
                var symbol := str(rows[neighbor.y])[neighbor.x]
                if symbol not in ["#","@","%"]: continue
                if symbol == "#" and int(masks[neighbor.y*26+neighbor.x]) == 0: continue
                var normal := Vector3(direction.x,0,direction.y)
                var along := Vector3(-direction.y,0,direction.x)
                var center := Vector3(col+.5,-.024,row+.5)
                var variation := posmod(row*3+col*7+direction.x*2+direction.y,4)
                # Forest contact is broken leaf litter, not a continuous
                # planted-bed border. Broad map color remains the ground.
                if symbol == "%" and variation == 0: continue
                var widths := [.06,.10,.08,.12] if symbol == "%" else [.16,.18,.17,.20]
                var first_width: float = widths[variation]
                var last_width: float = widths[(variation+1)%4]
                var edge_span: float = [.37,.42,.35,.45][variation] if symbol == "%" else .498
                # Straight road edges run through adjacent cells. Miter only
                # a genuine corner, rather than stamping a bevel at every
                # tile boundary and emphasizing the underlying grid.
                var extents := [edge_span,edge_span]
                for endpoint in 2:
                    var across: Vector2i = Vector2i(col,row)+Vector2i(-direction.y,direction.x)*(-1 if endpoint == 0 else 1)
                    if across.x < 0 or across.x >= 26 or across.y < 0 or across.y >= 26: continue
                    var beside := str(rows[across.y])[across.x]
                    if beside in ["#","@","%"] and (beside != "#" or int(masks[across.y*26+across.x]) != 0):
                        extents[endpoint] = minf(edge_span,.5-(first_width if endpoint == 0 else last_width))
                if symbol == "%":
                    extents[0] = minf(extents[0],edge_span-.025)
                    extents[1] = minf(extents[1],edge_span-.025)
                var shoulder := Color("353f29") if symbol == "%" else Color("454b32")
                g.quad(center+normal*.498-along*edge_span,
                    center+normal*(.5-first_width)-along*extents[0],
                    center+normal*(.5-last_width)+along*extents[1],
                    center+normal*.498+along*edge_span,shoulder)
                if symbol == "%": continue
                center.y = -.018
                for piece in 2:
                    var start := -.42+piece*.43
                    var end := start+.41
                    var inset := .040+.01*posmod(variation+piece,3)
                    var stone := Color("62684c") if symbol == "#" else Color("51634e")
                    g.quad(center+normal*.495+along*start,
                        center+normal*(.5-inset)+along*start,
                        center+normal*(.5-inset)+along*end,
                        center+normal*.495+along*end,stone)
    if not g.surfaces.is_empty():
        var body := MeshInstance3D.new()
        body.name = "Body"
        body.mesh = g.finish({"paint":_material("paint")})
        body.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
        root.add_child(body)
    return root

static func make_tile(tile: String, brick_mask: int, row: int, col: int) -> Node3D:
    # Coordinate-derived variation never samples the simulation RNG. Buildings
    # are designed in two-cell lots, and roofs meet at their actual cell edges.
    var lot_row := floori(row*.5)
    var lot_col := floori(col*.5)
    var kind := posmod(lot_row*5+lot_col*7,4)
    var variant := posmod(lot_row+floori(lot_col*.5),2)
    var parity_x := posmod(col,2)
    var parity_z := posmod(row,2)
    var key := "tile_%s_%s_%s_%s_%s_%s" % [tile,brick_mask,kind,variant,parity_x,parity_z]
    var tree_code := row*7381+col*1933+9277
    tree_code = (tree_code^(tree_code>>13))*1274126177
    var tree_variant := (tree_code^(tree_code>>16))&15
    if tile == "%":
        key = "forest_%s" % tree_variant
    elif tile == "~":
        # Water shape is independent of lot, parity and brick damage. Sharing
        # the startup instance's mesh also retains its prepared material.
        key = "water"
    var cached := _cached(key)
    if cached:
        return cached
    var g := Geometry.new()
    if tile == "#":
        var plaster: Color = [Color("ae9b78"),Color("829382"),Color("b5a78b"),Color("afa386")][kind]
        # Keep broad roof planes behind vehicles in the battlefield palette;
        # ridge highlights, facade detail and steel retain their own contrast.
        var roof: Color = [Color("735747"),Color("405954"),Color("4e6070"),Color("4d6053")][kind]
        var ink := Color("2c4145")
        if brick_mask == 15:
            var wall_top := .66+variant*.06
            g.loft([[.015,.49,-.49,.49,.025],[.12,.49,-.49,.49,.025],
                [.15,.474,-.474,.474,.022],[wall_top,.474,-.474,.474,.022]],plaster)
            g.box(Vector3(0,.135,0),Vector3(.98,.04,.98),Color("675f4c"))
            _roof_piece(g,-.5,.5,-.5,.5,kind,parity_x,parity_z,variant,roof)
            var frontage := posmod(parity_x+variant,2)
            # Two-cell facade rhythms: the entrance belongs to one bay, broad
            # paired windows to its neighbor. Side walls use a quieter rhythm.
            for side in [-1.0,1.0]:
                if kind == 1:
                    var opening := .65 if frontage == 0 else .44
                    var height := .235 if frontage == 0 else .42
                    var center_y := .435 if frontage == 0 else .35
                    g.box(Vector3(0,center_y,side*.480),Vector3(opening,height,.015),ink)
                    g.box(Vector3(0,center_y,side*.491),Vector3(.035,height,.015),Color("acb5a0"))
                    g.box(Vector3(0,.445,side*.491),Vector3(opening,.033,.015),Color("acb5a0"))
                elif kind == 2:
                    var window_x := -.17 if frontage == 0 else .17
                    g.box(Vector3(window_x,.39,side*.480),Vector3(.37,.34,.016),ink)
                    g.box(Vector3(window_x,.565,side*.486),Vector3(.47,.052,.026),plaster.lightened(.16))
                    g.box(Vector3(window_x,.205,side*.484),Vector3(.46,.038,.031),Color("8b7960"))
                    g.box(Vector3(-window_x*1.9,.40,side*.486),Vector3(.075,.32,.025),Color("65817b"))
                elif frontage == 0:
                    for window in [-1.0,1.0]:
                        g.box(Vector3(window*.23,.425,side*.480),Vector3(.21,.25,.016),ink)
                        g.box(Vector3(window*.23,.283,side*.484),Vector3(.27,.038,.031),plaster.lightened(.15))
                    if kind == 0:
                        g.box(Vector3(-.385,.425,side*.487),Vector3(.065,.265,.024),Color("687f70"))
                        g.box(Vector3(.385,.425,side*.487),Vector3(.065,.265,.024),Color("687f70"))
                else:
                    g.box(Vector3(-.195,.343,side*.480),Vector3(.28,.40,.016),ink)
                    g.box(Vector3(.235,.43,side*.480),Vector3(.22,.23,.016),ink)
                    g.box(Vector3(-.195,.563,side*.485),Vector3(.37,.045,.028),plaster.lightened(.16))
                    g.box(Vector3(.235,.295,side*.484),Vector3(.28,.038,.031),plaster.lightened(.15))
                var side_z := -.11 if parity_z == 0 else .11
                g.box(Vector3(side*.480,.425,side_z),Vector3(.014,.25,.34),ink)
                g.box(Vector3(side*.481,.285,side_z),Vector3(.036,.03,.40),plaster.lightened(.16))
            if kind == 1:
                var flat_top := _roof_height(0,0,kind,parity_x,parity_z,variant)
                var edge_x := -1.0 if parity_x == 0 else 1.0
                var edge_z := -1.0 if parity_z == 0 else 1.0
                g.box(Vector3(edge_x*.47,flat_top+.048,0),Vector3(.06,.10,.88),roof.lightened(.18))
                g.box(Vector3(0,flat_top+.048,edge_z*.47),Vector3(1,.10,.06),roof.lightened(.18))
                if parity_x == 1 and parity_z == 0:
                    g.loft([[flat_top,.14,-.26,.26,.04],[flat_top+.16,.11,-.21,.21,.035]],Color("96aba3"))
                    g.box(Vector3(0,flat_top+.172,0),Vector3(.15,.025,.29),Color("5b8992"))
            elif kind == 3:
                # Folded canvas shop awnings stay entirely inside the cell.
                for side in [-1.0,1.0]:
                    g.box(Vector3(0,.565,side*.47),Vector3(.86,.06,.058),Color("c4b17e"))
                    for stripe in 3:
                        g.box(Vector3(-.29+stripe*.29,.565,side*.495),Vector3(.14,.061,.010),Color("547b76"))
            _building_details(g,kind,parity_x,parity_z,variant,plaster,roof)
        else:
            for quadrant in 4:
                if (brick_mask & (1 << quadrant)) == 0:
                    continue
                var x := -.25+.5*(quadrant & 1)
                var z := -.25+.5*((quadrant >> 1) & 1)
                var offset := Vector3(x,0,z)
                var height := .47+.055*posmod(quadrant+kind,3)
                var sx := -1.0 if (quadrant&1) == 0 else 1.0
                var sz := -1.0 if (quadrant&2) == 0 else 1.0
                # A floor and two broken perimeter walls expose an actual
                # room corner. The collider still owns the complete surviving
                # half-cell; no rubble reaches a destroyed quadrant.
                g.box(offset+Vector3(0,.055,0),Vector3(.492,.10,.492),Color("776c53"))
                g.box(offset+Vector3(sx*.195,height*.5,0),Vector3(.10,height,.49),plaster)
                g.box(offset+Vector3(0,height*.46,sz*.195),Vector3(.49,height*.92,.10),plaster)
                g.box(offset+Vector3(sx*.185,height+.042,-.08),Vector3(.12,.084,.15),Color("967255"))
                g.box(offset+Vector3(.10,height*.92+.065,sz*.184),Vector3(.15,.13,.12),Color("a17b57"))
                g.box(offset+Vector3(sx*.133,.26,0),Vector3(.013,.033,.35),Color("957354"))
                g.box(offset+Vector3(0,.22,sz*.134),Vector3(.36,.034,.013),Color("957354"))
                g.loft([[.10,.09,-.065,.065,.03],[.20,.055,-.05,.05,.025]],Color("aa8562"),offset+Vector3(-sx*.055,0,-sz*.025))
    elif tile == "@":
        g.loft([[.005,.492,-.492,.492,.055],[.10,.492,-.492,.492,.055],
            [.60,.46,-.46,.46,.065],[.67,.40,-.40,.40,.055]],Color("466d7c"),Vector3.ZERO,"metal")
        g.loft([[.665,.415,-.415,.415,.04],[.715,.395,-.395,.395,.04]],Color("b0c0b6"),Vector3.ZERO,"metal")
        g.box(Vector3(0,.747,0),Vector3(.65,.034,.65),Color("466a78"),"metal")
        # Bright raised X ribs read as steel even in a small overhead tile.
        var y := .772
        g.quad(Vector3(-.29,y,-.25),Vector3(.25,y,.29),Vector3(.29,y,.25),Vector3(-.25,y,-.29),Color("b6c7bb"),"metal")
        g.quad(Vector3(-.29,y,.25),Vector3(-.25,y,.29),Vector3(.29,y,-.25),Vector3(.25,y,-.29),Color("b6c7bb"),"metal")
        for side in [-1.0,1.0]:
            g.box(Vector3(side*.466,.33,0),Vector3(.025,.085,.78),Color("b7bfac"),"metal")
            g.box(Vector3(0,.33,side*.466),Vector3(.78,.085,.025),Color("b7bfac"),"metal")
    elif tile == "%":
        # Three foliage fans share one branching trunk. The high, narrower
        # crown and separated side tiers leave the warm wood visible below;
        # their cover-cell footprint and translucency remain unchanged.
        var layout := tree_variant%4
        var regions := [Rect2(-.498,-.498,.756,.736),
            Rect2(-.028,-.068,.526,.566),Rect2(-.498,.052,.416,.446)]
        for index in regions.size():
            var region: Rect2 = regions[index]
            for turn in layout:
                region = Rect2(Vector2(-region.end.y,region.position.x),Vector2(region.size.y,region.size.x))
            regions[index] = region
        var center: Vector2 = regions[0].get_center()
        var side: Vector2 = regions[1].get_center()
        var low: Vector2 = regions[2].get_center()
        var origin := Vector3(center.x,0,center.y)
        var phase := .19+tree_variant*2.399963
        var height: float = [1.04,1.08,1.02,1.10][(tree_variant>>2)&3]
        var bark := Color("795e3d")
        g.tube(origin+Vector3(0,.012,0),origin+Vector3(.025,height-.09,.025),.058,.025,bark,"paint",6)
        g.tube(origin+Vector3(.013,.46,.013),Vector3(side.x,height-.24,side.y),.031,.010,bark.lightened(.06),"paint",3)
        g.tube(origin+Vector3(.007,.33,.007),Vector3(low.x,height-.38,low.y),.027,.009,bark,"paint",3)
        var leaves: Color = [Color(.25,.38,.24,.70),Color(.22,.36,.245,.70),
            Color(.29,.40,.23,.70),Color(.20,.34,.23,.70)][(tree_variant>>2)&3]
        g.leaf_cluster(Vector3(0,height,0),Vector3(1,.65,1),leaves,phase,tree_variant,regions[0],0)
        g.leaf_cluster(Vector3(0,height-.23,0),Vector3(1,.60,1),leaves.darkened(.08),phase+.9,tree_variant+1,regions[1],1)
        g.leaf_cluster(Vector3(0,height-.39,0),Vector3(1,.48,1),leaves.darkened(.16),phase-.7,tree_variant+2,regions[2],2)
    elif tile == "~":
        g.box(Vector3(0,.005,0),Vector3(1,.020,1),Color.WHITE,"water")
    elif tile == "-":
        g.box(Vector3(0,.012,0),Vector3(1,.02,1),Color("83b3bd"))
        g.quad(Vector3(-.43,.024,-.31),Vector3(-.38,.024,-.20),Vector3(.28,.024,.22),Vector3(.37,.024,.20),Color("bcdbd4"))
        g.quad(Vector3(-.28,.025,.33),Vector3(-.21,.025,.35),Vector3(.32,.025,-.27),Vector3(.26,.025,-.30),Color("9fc9cf"))
    if g.surfaces.is_empty():
        return Node3D.new()
    return _instance(key,g)

static func make_base_wall(health: int, steel: bool) -> Node3D:
    health = clampi(health,0,4)
    var key := "base_wall_%s_%s" % [health,steel]
    var cached := _cached(key)
    if cached:
        return cached
    var g := Geometry.new()
    var brick := Color("897054")
    if health == 0:
        # Broken segments must not imply an intact collider: rubble stays low.
        g.loft([[0,.44,-.40,.40,.10],[.045,.41,-.37,.37,.12]],brick.darkened(.16))
        g.loft([[.035,.15,-.13,.13,.045],[.095,.12,-.11,.11,.04]],brick,Vector3(-.20,0,.16))
        g.loft([[.035,.17,-.11,.11,.04],[.075,.14,-.09,.09,.04]],brick.lightened(.10),Vector3(.18,0,-.16))
        return _instance(key,g)
    var height := .20 + health*.065
    var body := Color("526f7c") if steel else brick
    g.loft([[.005,.49,-.49,.49,.04],[.07,.49,-.49,.49,.04],
        [height-.045,.47,-.47,.47,.035],[height,.44,-.44,.44,.035]],body)
    if steel:
        g.box(Vector3(0,height+.015,0),Vector3(.90,.05,.90),Color("b6c5bd"),"metal")
        for side in [-1.0,1.0]:
            g.box(Vector3(side*.471,height*.52,0),Vector3(.025,.09,.85),Color("bcc6b2"),"metal")
            g.box(Vector3(0,height*.52,side*.471),Vector3(.85,.09,.025),Color("bcc6b2"),"metal")
    else:
        # Large mortar bands survive at the ordinary gameplay pixel size.
        var mortar := Color("514e44")
        for side in [-1.0,1.0]:
            g.box(Vector3(0,.145,side*.476),Vector3(.94,.018,.009),mortar)
            g.box(Vector3(side*.476,.145,0),Vector3(.009,.018,.94),mortar)
        if health == 4:
            g.loft([[height,.475,-.475,.475,.035],
                [height+.055,.465,-.465,.465,.035]],Color("b0a081"))
        else:
            # Asymmetric cap fragments reveal increasing damage without
            # opening a false horizontal passage through a live wall cell.
            g.loft([[height,.19,-.18,.18,.035],
                [height+.045,.15,-.14,.14,.035]],brick.lightened(.18),Vector3(-.24,0,.22))
            if health >= 2:
                g.loft([[height,.16,-.19,.19,.025],
                    [height+.075,.12,-.17,.17,.025]],brick.lightened(.10),Vector3(.25,0,-.22))
            if health == 3:
                g.box(Vector3(-.13,height+.025,-.30),Vector3(.34,.05,.21),Color("b0a081"))
            for crack in range(4-health):
                g.box(Vector3(-.24+crack*.19,height*.67,.473),Vector3(.025,height*.49,.008),mortar)
    return _instance(key,g)

static func make_base(nation: int = 0) -> Node3D:
    nation = clampi(nation,0,2)
    var key := "base_%s" % nation
    var cached := _cached(key)
    if cached:
        return cached
    var g := Geometry.new()
    var wall: Color = [Color("829378"),Color("9a9977"),Color("a49a7f")][nation]
    var roof: Color = [Color("516f68"),Color("6c7963"),Color("627d76")][nation]
    g.loft([[0,.92,-.92,.92,.11],[.09,.92,-.92,.92,.11],
        [.14,.84,-.84,.84,.10]],Color("555f52"))
    g.loft([[.14,.69,-.59,.66,.12],[.39,.71,-.60,.68,.12],
        [.61,.65,-.52,.61,.13],[.68,.56,-.45,.53,.12]],wall)
    g.loft([[.68,.58,-.47,.55,.10],[.77,.55,-.44,.52,.12]],roof)
    for side in [-1.0,1.0]:
        g.box(Vector3(0,.34,side*.599),Vector3(.31,.34,.014),Color("263d41"))
        g.box(Vector3(.39,.43,side*.584),Vector3(.22,.12,.022),Color("5f9396"),"optic")
        g.box(Vector3(-.39,.43,side*.584),Vector3(.22,.12,.022),Color("5f9396"),"optic")
        g.box(Vector3(0,.53,side*.59),Vector3(.40,.065,.044),wall.lightened(.14))
    g.box(Vector3(.28,.786,-.06),Vector3(.17,.019,.28),Color("e3bf63"))
    if nation == 0:
        g.loft([[.77,.19,-.15,.15,.06],[.91,.17,-.13,.13,.05]],wall,Vector3(-.21,0,.18))
        g.tube(Vector3(-.34,.91,.18),Vector3(-.34,1.37,.18),.019,.012,Color("a9b8a7"))
        g.tube(Vector3(-.56,1.23,.18),Vector3(-.12,1.23,.18),.013,.013,Color("a9b8a7"))
        g.tube(Vector3(-.48,1.08,.18),Vector3(-.20,1.08,.18),.013,.013,Color("a9b8a7"))
    elif nation == 1:
        g.loft([[.77,.24,-.22,.22,.065],[1.00,.22,-.20,.20,.065],
            [1.10,.16,-.17,.17,.06]],wall,Vector3(-.17,0,.17))
        g.loft([[1.10,.235,-.24,.24,.055],[1.16,.18,-.19,.19,.06]],Color("8d6550"),Vector3(-.17,0,.17))
        g.box(Vector3(-.17,.97,-.035),Vector3(.24,.085,.02),Color("314c4e"))
    else:
        g.loft([[.77,.23,-.22,.22,.13],[.90,.26,-.24,.24,.14],
            [1.04,.18,-.17,.17,.105],[1.10,.08,-.08,.08,.05]],Color("598378"),Vector3(-.17,0,.14))
        g.tube(Vector3(-.17,1.10,.14),Vector3(-.17,1.23,.14),.022,.01,Color("b8b08b"))
    return _instance(key,g)

static func make_shell() -> Node3D:
    var cached := _cached("shell")
    if cached:
        return cached
    var g := Geometry.new()
    # The simulation root and original front tip remain fixed. A broad cream
    # nose separates the projectile from terrain; the short amber tail points
    # behind it rather than extending the apparent hit position forward.
    g.tube(Vector3(0,0,.055),Vector3(0,0,-.035),.044,.061,Color("fff0b4"),"hot",8)
    g.tube(Vector3(0,0,-.035),Vector3(0,0,-.14),.061,.005,Color("fff7d2"),"hot",8)
    g.tube(Vector3(0,0,.30),Vector3(0,0,.055),.007,.044,Color("f7ac43"),"hot",6)
    return _instance("shell",g)

static func _fire_triangle(g: Geometry, points: Array[Vector3], colors: Array[Color]) -> void:
    if not g.surfaces.has("hot"):
        var created := SurfaceTool.new()
        created.begin(Mesh.PRIMITIVE_TRIANGLES)
        g.surfaces["hot"] = created
    var stream: SurfaceTool = g.surfaces["hot"]
    var normal := (points[1]-points[0]).cross(points[2]-points[0]).normalized()
    for index in [0,2,1]:
        stream.set_normal(normal)
        stream.set_color(colors[index])
        stream.add_vertex(points[index])

static func _muzzle_flash_mesh() -> ArrayMesh:
    if _meshes.has("muzzle_flash"): return _meshes["muzzle_flash"]
    var g := Geometry.new()
    var rings: Array = []
    var tints: Array = []
    var lengths := [0.0,-.28,-.60,-1.26]
    var radii := [.12,.53,.24,.022]
    for level in 4:
        var ring: Array[Vector3] = []
        var colors: Array[Color] = []
        for index in 8:
            var angle := TAU*index/8.0
            var radius: float = radii[level]*(.58 if level == 1 and index%2 == 1 else 1.0)
            ring.append(Vector3(cos(angle)*radius,sin(angle)*radius*.84,lengths[level]))
            var tint := Color("fff2bd")
            if level == 1:
                tint = Color("cf5918") if index%2 == 0 else Color("ffc16a")
            elif level == 2:
                tint = Color("f5ae3f") if index%2 == 0 else Color("ffe6a0")
            colors.append(tint)
        rings.append(ring)
        tints.append(colors)
    for level in 3:
        for index in 8:
            var next := (index+1)%8
            _fire_triangle(g,[rings[level][index],rings[level+1][index],rings[level+1][next]],
                [tints[level][index],tints[level+1][index],tints[level+1][next]])
            _fire_triangle(g,[rings[level][index],rings[level+1][next],rings[level][next]],
                [tints[level][index],tints[level+1][next],tints[level][next]])
    for index in 8:
        var next := (index+1)%8
        g.tri(Vector3.ZERO,rings[0][index],rings[0][next],Color("fff2bd"),"hot")
        g.tri(Vector3(0,0,lengths[3]),rings[3][next],rings[3][index],Color("fff2bd"),"hot")
    _meshes["muzzle_flash"] = g.finish({"hot":_material("hot")})
    _mesh_groups["muzzle_flash"] = ["hot"]
    return _meshes["muzzle_flash"]

static func set_muzzle_flash(effect: Node3D, enabled: bool) -> void:
    # Reuse the existing small-effect slot and its independent mutable fade
    # material. Switching back restores the exact impact mesh, not a variant.
    effect.set_meta("muzzle_flash",enabled)
    var body: MeshInstance3D = effect.get_node("Body")
    body.mesh = _muzzle_flash_mesh() if enabled else _meshes["explosion"]

static func make_pickup(type: int) -> Node3D:
    type = clampi(type,0,8)
    var key := "pickup_%s" % type
    var cached := _cached(key)
    if cached:
        return cached
    var g := Geometry.new()
    var rim: Color = [Color("bd7550"),Color("d0b467"),Color("64a3a3"),Color("8eb573"),Color("8ba06f"),Color("e3c36f"),Color("a2b4be"),Color("79bcd1"),Color("d17973")][type]
    var cream := Color("d6cfa6")
    var ink := Color("263f45")
    g.loft([[0,.40,-.38,.38,.105],[.045,.45,-.43,.43,.12],
        [.105,.41,-.39,.39,.11]],rim)
    g.loft([[.106,.35,-.33,.33,.095],[.12,.35,-.33,.33,.095]],ink)
    if type == 0:
        # Grenade: pinched neck, pineapple body, bent safety lever and pin ring.
        g.loft([[.14,.125,-.115,.115,.065],[.24,.215,-.19,.19,.10],
            [.44,.205,-.18,.18,.10],[.56,.12,-.11,.11,.07],
            [.60,.09,-.08,.08,.05]],Color("74865b"))
        for y in [.29,.42]:
            g.loft([[y,.219,-.191,.191,.10],[y+.027,.219,-.191,.191,.10]],Color("3a5347"))
        g.tube(Vector3(-.03,.62,0),Vector3(.19,.61,0),.035,.035,Color("b7b89b"))
        g.tube(Vector3(.19,.61,0),Vector3(.24,.37,0),.035,.030,Color("b7b89b"))
        g.ring(Vector3(-.08,.64,0),Vector3.FORWARD,.072,.045,.028,Color("b7b89b"),"metal")
    elif type == 1:
        g.loft([[.18,.32,-.28,.28,.12],[.22,.34,-.29,.29,.13],
            [.25,.27,-.25,.25,.12],[.40,.255,-.23,.23,.13],
            [.52,.17,-.17,.17,.105],[.56,.09,-.10,.10,.06]],Color("7e946d"))
        g.loft([[.215,.34,-.295,.295,.13],[.255,.325,-.28,.28,.13]],cream)
        g.box(Vector3(.02,.545,-.02),Vector3(.085,.015,.18),rim)
    elif type == 2:
        # Clock: sculpted alarm housing, face on both sides and a real gap
        # around the handle, so the icon remains legible as it rotates.
        g.tube(Vector3(0,.41,-.075),Vector3(0,.41,.075),.27,.27,Color("547e81"),"paint",16)
        for side in [-1.0,1.0]:
            g.tube(Vector3(0,.41,side*.076),Vector3(0,.41,side*.084),.225,.225,cream,"paint",16)
            g.tube(Vector3(0,.41,side*.089),Vector3(-.12,.49,side*.089),.024,.020,ink,"paint",6)
            g.tube(Vector3(0,.41,side*.091),Vector3(0,.58,side*.091),.023,.015,ink,"paint",6)
        g.tube(Vector3(-.15,.18,0),Vector3(-.18,.12,0),.035,.035,ink)
        g.tube(Vector3(.15,.18,0),Vector3(.18,.12,0),.035,.035,ink)
        g.ring(Vector3(0,.72,0),Vector3.FORWARD,.077,.045,.036,cream,"metal")
    elif type == 3:
        # Shovel has a distinct broad spade and open D-shaped grip.
        g.polygon([Vector2(0,.12),Vector2(.15,.18),Vector2(.21,.36),
            Vector2(.13,.43),Vector2(-.13,.43),Vector2(-.21,.36),Vector2(-.15,.18)],.10,Color("a9b8a7"),"metal")
        g.tube(Vector3(0,.35,0),Vector3(0,.75,0),.038,.031,Color("a28255"),"paint",8)
        g.ring(Vector3(0,.79,0),Vector3.FORWARD,.098,.059,.055,Color("738c78"),"paint")
    elif type == 4:
        for side in [-1.0,1.0]:
            g.loft([[.14,.095,-.30,.30,.07],[.22,.11,-.34,.34,.09],
                [.31,.082,-.29,.29,.065]],ink,Vector3(side*.22,0,0),"rubber")
        g.loft([[.22,.21,-.25,.25,.07],[.36,.23,-.22,.25,.08],
            [.41,.17,-.17,.20,.07]],Color("809774"))
        g.loft([[.36,.16,-.15,.16,.07],[.49,.20,-.15,.19,.085],
            [.60,.13,-.09,.13,.065]],Color("a0b28a"))
        g.tube(Vector3(0,.47,-.11),Vector3(0,.47,-.39),.065,.047,cream,"metal",10)
    elif type == 5:
        var points: Array[Vector2] = []
        for i in 10:
            var a := PI*.5+TAU*i/10.0
            var radius := .335 if i%2 == 0 else .157
            points.append(Vector2(cos(a)*radius,.43+sin(a)*radius))
        g.polygon(points,.12,Color("e5c264"))
        g.ring(Vector3(0,.43,.072),Vector3.BACK,.070,.046,.016,Color("9a793e"))
    elif type == 6:
        g.loft([[.17,.16,-.18,.22,.06],[.28,.15,-.19,.20,.06]],Color("748d86"))
        g.loft([[.28,.20,-.22,.20,.07],[.44,.175,-.20,.18,.065]],Color("a7b9ad"))
        g.tube(Vector3(0,.37,-.16),Vector3(0,.37,-.43),.095,.068,ink,"metal",10)
        g.ring(Vector3(0,.37,-.441),Vector3.FORWARD,.069,.043,.022,cream,"metal")
        g.box(Vector3(-.075,.465,.10),Vector3(.12,.05,.13),rim)
    elif type == 7:
        # River tug: true keel-to-shoulder hull, sharpened bow, leaning bridge,
        # inset glazing and one open cream/orange life ring. No rail-box pile.
        g.loft([[.145,.12,-.35,.29,.10],[.23,.245,-.43,.34,.21],
            [.36,.275,-.43,.35,.22],[.405,.255,-.395,.325,.19]],Color("315763"))
        g.loft([[.405,.248,-.385,.315,.19],[.435,.238,-.37,.305,.18]],cream)
        g.loft([[.435,.168,-.14,.21,.06],[.625,.158,-.11,.20,.05],
            [.705,.138,-.065,.175,.045]],Color("c9c8a4"))
        g.loft([[.705,.18,-.115,.23,.065],[.747,.17,-.105,.22,.06]],Color("607f79"))
        g.quad(Vector3(-.12,.59,-.132),Vector3(-.11,.666,-.102),
            Vector3(.11,.666,-.102),Vector3(.12,.59,-.132),Color("355f6a"),"optic")
        for side in [-1.0,1.0]:
            g.box(Vector3(side*.155,.625,.068),Vector3(.012,.090,.15),Color("355f6a"),"optic")
        g.box(Vector3(0,.624,.203),Vector3(.22,.105,.014),Color("355f6a"),"optic")
        g.tube(Vector3(.085,.66,.16),Vector3(.085,.87,.16),.057,.049,Color("be734d"),"paint",9)
        g.tube(Vector3(.085,.862,.16),Vector3(.085,.895,.16),.062,.062,ink,"metal",9)
        g.ring(Vector3(.265,.49,.07),Vector3.RIGHT,.113,.064,.038,cream,"paint",Color("cb774c"))
        g.tube(Vector3(0,.434,-.29),Vector3(0,.49,-.29),.025,.025,Color("a3b5ad"),"metal",8)
    else:
        g.loft([[.14,.29,-.23,.23,.055],[.38,.29,-.23,.23,.055],
            [.44,.25,-.20,.20,.05]],cream)
        for side in [-1.0,1.0]:
            g.box(Vector3(0,.29,side*.235),Vector3(.082,.235,.013),Color("af5045"))
            g.box(Vector3(0,.29,side*.237),Vector3(.22,.082,.013),Color("af5045"))
        g.box(Vector3(0,.45,0),Vector3(.082,.015,.25),Color("af5045"))
        g.box(Vector3(0,.45,0),Vector3(.22,.017,.08),Color("af5045"))
    return _instance(key,g)

static func make_explosion(with_smoke: bool = true) -> Node3D:
    if not _meshes.has("explosion"):
        var g := Geometry.new()
        # One connected irregular burst avoids a necklace of separate glowing
        # balls. Its broad waist, folded orange rim and staggered cream crown
        # stay readable at the same native pixel scale as a tank.
        var rings: Array = []
        var radii := [1.25,.52,1.05,.70,1.17,.55,1.31,.62,1.07,.58,1.16,.64]
        var tips := [.08,.01,.14,.04,.06,.0,.12,.03,.09,.02,.16,.035]
        var heights := [-.13,.035,.225,.405]
        var widths := [.13,.245,.390,.17]
        for level in 4:
            var ring: Array[Vector3] = []
            for i in 12:
                var a := TAU*i/12.0+.17
                var radius: float = widths[level]*lerpf(1.0,radii[i],1.0 if level == 2 else .45)
                ring.append(Vector3(cos(a)*radius+level*.012,
                    heights[level]+tips[i]*float(level)/3.0,sin(a)*radius*.91-level*.010))
            rings.append(ring)
        var colors := [Color("d66b27"),Color("f4a43f"),Color("ffd779")]
        for level in 3:
            for i in 12:
                var j := (i+1)%12
                var color: Color = colors[level].lerp(Color("fff0b2"),.23 if i%2 == 0 and level > 0 else 0.0)
                g.tri(rings[level][j],rings[level][i],rings[level+1][i],color,"hot")
                g.tri(rings[level][j],rings[level+1][i],rings[level+1][j],color,"hot")
        for i in 12:
            var j := (i+1)%12
            g.tri(Vector3(.036,.54,-.030),rings[3][j],rings[3][i],Color("fff0b2"),"hot")
            g.tri(Vector3(0,-.13,0),rings[0][i],rings[0][j],Color("c15c25"),"hot")
        _instance("explosion",g).free()
    if with_smoke and not _meshes.has("explosion_smoke"):
        var smoke := Geometry.new()
        smoke.plume(Vector3(-.14,.25,-.005),Vector3(.31,.41,.32),Color(.26,.32,.34,.78),"smoke",Vector2(-.075,.03),.20)
        smoke.plume(Vector3(.12,.31,.025),Vector3(.30,.49,.29),Color(.25,.31,.34,.78),"smoke",Vector2(.075,-.04),.65)
        smoke.plume(Vector3(.015,.49,-.04),Vector3(.29,.44,.295),Color(.30,.36,.39,.78),"smoke",Vector2(.045,-.08),.02)
        _instance("explosion_smoke",smoke).free()
    var root := _instance("explosion",null)
    var flame_body: MeshInstance3D = root.get_node("Body")
    flame_body.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
    var flame_material := _material("hot").duplicate() as StandardMaterial3D
    flame_material.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
    flame_body.material_override = flame_material
    if not with_smoke:
        return root
    var smoke := _instance("explosion_smoke",null)
    smoke.name = "Smoke"
    smoke.visible = false
    var smoke_body: MeshInstance3D = smoke.get_node("Body")
    smoke_body.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
    var smoke_material := _material("smoke").duplicate() as StandardMaterial3D
    smoke_material.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
    smoke_body.material_override = smoke_material
    root.add_child(smoke)
    return root

static func set_effect_state(effect: Node3D, age_fraction: float, size: float) -> void:
    # One cached flame mesh plus one cached smoke mesh, no particle allocation
    # or gameplay RNG. The caller continues to own lifetime/pool/position.
    var t := clampf(age_fraction,0.0,1.0)
    var large := size > .5
    var flame: MeshInstance3D = effect.get_node("Body")
    effect.scale = Vector3.ONE*size
    if bool(effect.get_meta("muzzle_flash",false)):
        # Direction belongs to the caller's gun basis. Keep this brief burst
        # anchored at the muzzle, with no explosion-style spin or upward drift.
        var open := 1.0-pow(1.0-clampf(t/.20,0.0,1.0),2.0)
        var close := smoothstep(.35,1.0,t)
        flame.position = Vector3.ZERO
        flame.rotation = Vector3.ZERO
        flame.scale = Vector3.ONE*(.68+.32*open)*lerpf(1.0,.55,close)
        flame.scale.z = (.84+.36*open)*lerpf(1.0,.38,close)
        flame.visible = t < .96
        var material := flame.material_override as StandardMaterial3D
        material.albedo_color = Color(1,1,1,maxf(.025,1.0-smoothstep(.15,.96,t)))
        material.emission_energy_multiplier = .04+.18*(1.0-smoothstep(.10,.75,t))
        var hidden_smoke: Node3D = effect.get_node_or_null("Smoke")
        if hidden_smoke != null: hidden_smoke.visible = false
        return
    var burst := 1.0-pow(1.0-clampf(t/.26,0.0,1.0),2.0)
    flame.scale = Vector3(.24+1.38*burst,.33+1.02*burst-.44*clampf(t/.58,0.0,1.0),.24+1.38*burst)
    flame.position.y = .04+t*.30
    flame.rotation.y = burst*.14
    flame.visible = t < .58 if large else t < .96
    # GeometryInstance3D.transparency is ignored by Mobile. Material opacity
    # works on Mobile and Forward+, with independent per-effect materials.
    var flame_material := flame.material_override as StandardMaterial3D
    var flame_color := Color.WHITE.lerp(Color(1,.48,.20),smoothstep(.08,.53,t))
    flame_color.a = maxf(.025,1.0-smoothstep(.10 if large else .0,.58 if large else .96,t))
    flame_material.albedo_color = flame_color
    flame_material.emission_energy_multiplier = .08+.50*(1.0-smoothstep(.16,.55,t))
    var smoke: Node3D = effect.get_node_or_null("Smoke")
    if smoke == null:
        return
    smoke.visible = large and t > .23
    if not large:
        return
    var smoke_age := clampf((t-.20)/.80,0.0,1.0)
    smoke.scale = Vector3(.35+1.05*smoke_age,.38+.95*smoke_age,.35+1.05*smoke_age)
    smoke.position = Vector3(.025+t*.11,.08+t*.45,-t*.06)
    smoke.rotation.y = -.10+t*.17
    var smoke_body: MeshInstance3D = smoke.get_node("Body")
    var smoke_material := smoke_body.material_override as StandardMaterial3D
    smoke_material.albedo_color.a = maxf(.025,smoothstep(.23,.39,t)*(1.0-smoothstep(.76,1.0,t)))
