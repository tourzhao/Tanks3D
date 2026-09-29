extends SceneTree

const Art = preload("res://art.gd")
const Pool = preload("res://effect_pool.gd")
var failures := 0


func expect(condition: bool, detail: String) -> bool:
    if not condition:
        failures += 1
        push_error("Effect pool contract: " + detail)
    return condition


func _initialize() -> void:
    _run.call_deferred()


func same_visual(actual: Node3D, fresh: Node3D) -> bool:
    if not actual.transform.is_equal_approx(fresh.transform):
        return false
    if bool(actual.get_meta("muzzle_flash", false)) != bool(fresh.get_meta("muzzle_flash", false)):
        return false
    for path in ["Body", "Smoke", "Smoke/Body"]:
        var a := actual.get_node_or_null(path) as Node3D
        var b := fresh.get_node_or_null(path) as Node3D
        if (a == null) != (b == null):
            return false
        if a == null:
            continue
        if not a.transform.is_equal_approx(b.transform) or a.visible != b.visible:
            return false
        if a is MeshInstance3D:
            if a.mesh != b.mesh or a.cast_shadow != b.cast_shadow or a.transparency != b.transparency:
                return false
            var ma := a.material_override as StandardMaterial3D
            var mb := b.material_override as StandardMaterial3D
            if ma == null or mb == null or ma == mb:
                return false
            if not ma.albedo_color.is_equal_approx(mb.albedo_color):
                return false
            if not is_equal_approx(ma.emission_energy_multiplier, mb.emission_energy_multiplier):
                return false
    return true


func check_muzzle_reuse(pool: RefCounted) -> void:
    var point := Vector3(9.0, 0.7, 6.0)
    var slot: Node3D = pool.acquire(point, 0.23, true)
    var identity := slot.get_instance_id()
    var fade: Material = slot.get_node("Body").material_override
    pool.release(slot)
    # The same small slot alternates both kinds. A material/transform left by
    # the directional flash must not contaminate the next ordinary impact.
    for muzzle in [true, false, true, false]:
        var reused: Node3D = pool.acquire(point, 0.23, muzzle)
        var fresh := Art.make_explosion(false)
        Art.set_muzzle_flash(fresh, muzzle)
        fresh.position = point
        Art.set_effect_state(fresh, 0.0, 0.23)
        expect(reused.get_instance_id() == identity and reused.get_node("Body").material_override == fade,
            "switching muzzle/impact replaces the pooled node or its fade material")
        expect(bool(reused.get_meta("muzzle_flash", false)) == muzzle,
            "recycled small slot retains stale muzzle metadata")
        expect(same_visual(reused, fresh), "switching small effect kind retains a stale initial visual")
        expect(not reused.has_node("Smoke"), "directional flash changes small-slot membership")
        for phase in [-1.0, 0.0, 0.10, 0.40, 0.85, 0.99, 1.0, 2.0]:
            Art.set_effect_state(reused, phase, 0.23)
            Art.set_effect_state(fresh, phase, 0.23)
            expect(same_visual(reused, fresh), "alternating effect differs from fresh kind at phase %s" % phase)
        # Deliberately dirty both root and body poses before the next lease.
        reused.position += Vector3(.2, .9, -.3)
        reused.rotation = Vector3(.3, -.7, .1)
        reused.get_node("Body").transform = Transform3D(Basis.from_euler(Vector3(.2, .4, -.1)), Vector3(.1, .2, .3))
        fresh.free()
        pool.release(reused)
    # The flag cannot convert an explosion slot or add a third retained kind.
    var large: Node3D = pool.acquire(point, 1.0, true)
    var fresh_large := Art.make_explosion(true)
    fresh_large.position = point
    Art.set_effect_state(fresh_large, 0.0, 1.0)
    expect(same_visual(large, fresh_large), "muzzle flag changes the large generic explosion")
    fresh_large.free()
    pool.release(large)
    pool.clear()


func _run() -> void:
    seed(20260918)
    var first_random := randi()
    var world := Node3D.new()
    root.add_child(world)
    var pool := Pool.new(world)
    expect(world.get_child_count() == 0 and pool.stats().allocated == 0,
        "construction must remain lazy without hidden prewarm nodes")

    # A reused late, drifting effect must match a freshly constructed effect at
    # every visible phase, including the smoke reveal and nearly expired flame.
    for size in [0.20, 0.30, 1.0, 1.2]:
        var point := Vector3(4.0 + size, 0.45, 7.0)
        var old: Node3D = pool.acquire(point, size)
        var old_id := old.get_instance_id()
        Art.set_effect_state(old, 0.97, size)
        old.position.y += 0.9
        old.rotation = Vector3(0.1, 0.7, -0.2)
        old.get_node("Body").rotation.x = 0.4
        if old.has_node("Smoke"):
            old.get_node("Smoke/Body").scale = Vector3.ONE * 1.7
        pool.release(old)
        expect(not old.visible and not old.get_node("Body").is_visible_in_tree(),
            "released effect remains visible or able to cast a ghost")
        var reused: Node3D = pool.acquire(point, size)
        expect(reused.get_instance_id() == old_id, "free slot was replaced instead of reused")
        var fresh := Art.make_explosion(size > 0.5)
        fresh.position = point
        Art.set_effect_state(fresh, 0.0, size)
        expect(same_visual(reused, fresh), "reused initial state differs from fresh art")
        for phase in [0.0, 0.10, 0.35, 0.80, 0.97]:
            Art.set_effect_state(reused, phase, size)
            Art.set_effect_state(fresh, phase, size)
            expect(same_visual(reused, fresh), "reused animation differs from fresh art at phase %s" % phase)
        fresh.free()
        pool.release(reused)

    check_muzzle_reuse(pool)

    var a: Node3D = pool.acquire(Vector3.ZERO, 1.0)
    var b: Node3D = pool.acquire(Vector3.ONE, 1.0)
    for path in ["Body", "Smoke/Body"]:
        var ma := a.get_node(path) as MeshInstance3D
        var mb := b.get_node(path) as MeshInstance3D
        expect(ma.mesh == mb.mesh and ma.material_override != mb.material_override,
            "slots must share geometry but isolate mutable fade materials")
    var peer_flame: Color = b.get_node("Body").material_override.albedo_color
    var peer_smoke: Color = b.get_node("Smoke/Body").material_override.albedo_color
    Art.set_effect_state(a, 0.93, 1.0)
    pool.release(a)
    expect(b.get_node("Body").material_override.albedo_color == peer_flame and
        b.get_node("Smoke/Body").material_override.albedo_color == peer_smoke,
        "fading or releasing one slot modifies another active slot")
    pool.clear()

    # Fill all slots of each kind. Hundreds of further bursts must retain the
    # same node/material identities and never exceed the original live cap.
    var known_nodes: Dictionary = {}
    var known_materials: Dictionary = {}
    for large in [false, true]:
        for index in 48:
            var node: Node3D = pool.acquire(Vector3(index, 0, 0), 1.0 if large else 0.23, not large and index % 2 == 0)
            expect(node != null, "a full legal burst was rejected")
            known_nodes[node.get_instance_id()] = true
            for path in ["Body", "Smoke/Body"]:
                if node.has_node(path):
                    known_materials[node.get_node(path).material_override.get_instance_id()] = true
            expect(node.has_node("Smoke") == large, "small/large variant membership changed")
        expect(pool.acquire(Vector3.ZERO, 1.0) == null and pool.acquire(Vector3.ZERO, 0.23) == null and
            pool.acquire(Vector3.ZERO, 0.23, true) == null,
            "49th live effect must be rejected for either variant")
        expect(pool.stats().active == 48, "live admission cap changed")
        pool.clear()
    expect(known_nodes.size() == 96 and known_materials.size() == 144,
        "variant warmup allocated unexpected node or material counts")
    var created_after_warmup: int = pool.stats().created
    for cycle in 192:
        for index in 48:
            var large := (cycle + index) % 2 == 0
            var muzzle := not large and (cycle + index) % 3 == 0
            var node: Node3D = pool.acquire(Vector3(index, 0.45, cycle), 1.2 if large else 0.20, muzzle)
            expect(node != null and known_nodes.has(node.get_instance_id()), "warmed burst allocates a new slot")
            expect(bool(node.get_meta("muzzle_flash", false)) == muzzle, "warmed burst retains the previous effect kind")
            for path in ["Body", "Smoke/Body"]:
                if node.has_node(path):
                    expect(known_materials.has(node.get_node(path).material_override.get_instance_id()),
                        "warmed burst duplicates a fade material")
            Art.set_effect_state(node, 0.8, 1.2 if large else 0.20)
        pool.clear()
    var stats: Dictionary = pool.stats()
    expect(stats.created == created_after_warmup and stats.allocated == 96 and
        stats.small == 48 and stats.large == 48 and world.get_child_count() == 96,
        "retained pool grows after both variants reach capacity")
    for node in world.get_children():
        expect(not node.visible, "clear leaves a retained slot visible")
        expect(not bool(node.get_meta("muzzle_flash", false)), "session clear retains a muzzle kind")

    # Exercise the real application's spawn/clear seam, not a copied lifetime or
    # admission policy. No scene _ready or native gameplay step is invoked here.
    var app = load("res://main.gd").new()
    app.world = world
    app.effect_pool = pool
    for index in 60:
        app.spawn_effect(Vector3(index, 0.45, 2.0), 1.0 if index % 2 == 0 else 0.23)
    expect(app.effects.size() == 48 and pool.stats().active == 48,
        "application spawn seam does not preserve the original live cap")
    for index in app.effects.size():
        var entry: Dictionary = app.effects[index]
        var large: bool = index % 2 == 0
        expect(entry.node.position == Vector3(index, 0.45, 2.0) and entry.age == 0.0 and
            entry.life == (0.7 if large else 0.10) and entry.size == (1.0 if large else 0.23),
            "pool integration changed event order, placement, initial age or lifetime")
    app.clear_effects()
    expect(app.effects.is_empty() and pool.stats().active == 0,
        "application restart clear leaves stale leases or lifetime records")
    var firing_basis := Basis.from_euler(Vector3(.18, -.8, .12))
    var firing_point := Vector3(8.0, .8, 4.0)
    app.spawn_effect(firing_point, .23, firing_basis, true)
    var flash: Dictionary = app.effects[0]
    var flash_node: Node3D = flash.node
    expect(flash_node.position == firing_point and flash.age == 0.0 and flash.life == .10 and flash.size == .23,
        "directional muzzle changes native placement or small-effect lifetime")
    expect(flash_node.basis.orthonormalized().is_equal_approx(firing_basis) and
        flash_node.scale.is_equal_approx(Vector3.ONE * .23), "muzzle orientation loses size or supplied firing basis")
    for phase in [0.0, .4, .9]:
        Art.set_effect_state(flash_node, phase, .23)
        expect(flash_node.basis.orthonormalized().is_equal_approx(firing_basis),
            "effect animation discards the supplied muzzle orientation")
    var flash_id := flash_node.get_instance_id()
    app.clear_effects()
    app.spawn_effect(firing_point, .23)
    var impact_node: Node3D = app.effects[0].node
    var fresh_impact := Art.make_explosion(false)
    fresh_impact.position = firing_point
    Art.set_effect_state(fresh_impact, 0.0, .23)
    expect(impact_node.get_instance_id() == flash_id and same_visual(impact_node, fresh_impact),
        "application impact inherits a recycled muzzle mesh, orientation, or fade state")
    fresh_impact.free()
    app.clear_effects()
    app.free()

    var retained: Array[WeakRef] = []
    for node in world.get_children():
        retained.append(weakref(node))
    pool.dispose()
    pool.dispose()
    expect(world.get_child_count() == 0 and pool.stats().allocated == 0 and pool.stats().active == 0,
        "dispose leaks nodes or is not idempotent")
    for reference in retained:
        expect(reference.get_ref() == null, "disposed node remains allocated")
    expect(pool.acquire(Vector3.ZERO, 1.0) == null, "disposed pool admits new leases")
    world.free()

    # Parent-owned teardown must also work while a lease is active.
    var parent := Node3D.new()
    root.add_child(parent)
    var owned := Pool.new(parent)
    var owned_node: WeakRef = weakref(owned.acquire(Vector3.ZERO, 1.0))
    parent.free()
    expect(owned_node.get_ref() == null, "world teardown leaks a pooled active node")
    owned.dispose()

    var next_random := randi()
    seed(20260918)
    expect(randi() == first_random and randi() == next_random, "pool or animation consumes gameplay-independent RNG")
    if failures > 0:
        quit(1)
        return
    print("TANKS_EFFECT_POOL_CHECKS_PASSED " + JSON.stringify({
        "status": "passed", "live_limit": 48, "retained_limit": 96,
        "warmed_created": created_after_warmup, "final_created": stats.created,
        "retained_peak": stats.allocated, "cycles": 192, "reused": stats.reused,
        "checks": ["lazy", "admission", "reuse", "reset", "visibility", "material-isolation",
            "shared-mesh", "lifecycle", "integration", "rng"]}))
    quit()
