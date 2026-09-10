#ifndef TANKS3D_BATTLE_FX_H
#define TANKS3D_BATTLE_FX_H

#include <raylib.h>
#include <rlgl.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

// Lightweight, texture-free battle effects for the Y-up Tanks 3D world.
// Keep one BattleFx instance in the game, call update() once per frame, and
// call draw(camera) while that 3D camera is active. Spawns are allocation-free
// and use a private visual random stream, never the simulation's random source.
class BattleFx
{
public:
    static constexpr std::size_t kMaxParticles = 960;

    void clear()
    {
        for (Particle &particle : particles_)
            particle.active = false;
        cursor_ = 0;
    }

    void update(float dt)
    {
        dt = std::clamp(dt, 0.0f, 0.05f);
        for (Particle &particle : particles_)
        {
            if (!particle.active)
                continue;

            particle.age += dt;
            // Delayed particles let a single allocation-free burst unfold in
            // layers instead of presenting every fireball and smoke lobe on
            // the same frame.
            if (particle.age < 0.0f)
                continue;
            if (particle.age >= particle.life)
            {
                particle.active = false;
                continue;
            }

            // Muzzle flashes keep their base locked to the barrel. Their
            // velocity stores the cone axis rather than physical motion.
            if (particle.kind != Kind::Shockwave && particle.kind != Kind::MuzzleFlash)
            {
                particle.velocity.y += particle.gravity * dt;
                particle.position = add(particle.position, scale(particle.velocity, dt));
                const float damping = std::pow(particle.drag, dt * 60.0f);
                particle.velocity = scale(particle.velocity, damping);
            }
        }
    }

    // direction points from the barrel toward the shell's travel direction.
    void spawnMuzzleFlash(Vector3 position, Vector3 direction,
                          Color color = Color{255, 174, 58, 255})
    {
        direction = safeNormalize(direction, Vector3{0.0f, 0.0f, -1.0f});

        // A tank gun produces a very short, directional blossom: a white-hot
        // center and several orange petals.  Dedicated cone particles read as
        // expanding gas instead of the former cluster of glowing spheres.
        for (int index = 0; index < 4; ++index)
        {
            const float spread = index == 0 ? 0.0f : 0.17f;
            const Vector3 axis = safeNormalize(
                add(direction, randomVector(spread)), direction);
            Particle &flash = emit(Kind::MuzzleFlash, position, axis,
                                   index == 0 ? 0.065f : random(0.07f, 0.11f),
                                   index == 0 ? 0.105f : random(0.075f, 0.11f));
            flash.endSize = flash.startSize * 0.22f;
            flash.startColor = index == 0 ? Color{255, 253, 218, 255}
                                           : (index & 1) == 0
                                                 ? Color{255, 196, 73, 245}
                                                 : color;
            flash.endColor = Color{255, 73, 10, 0};
        }

        // Hot wisps detach from the main pressure cone and fade quickly.
        for (int index = 0; index < 2; ++index)
        {
            Particle &fire = emit(Kind::Fire,
                                  add(position, scale(direction, random(0.10f, 0.24f))),
                                  add(scale(direction, random(1.0f, 1.8f)), randomVector(0.20f)),
                                  random(0.12f, 0.19f), random(0.070f, 0.105f));
            fire.startColor = index == 0 ? Color{255, 245, 181, 235} : color;
            fire.endColor = Color{255, 62, 8, 0};
            fire.endSize = fire.startSize * 0.30f;
            fire.drag = 0.88f;
        }

        // Only a few incandescent flecks: a cannon muzzle is not a sparkler.
        for (int index = 0; index < 3; ++index)
        {
            Particle &spark = emit(Kind::Spark, position,
                                   add(scale(direction, random(1.8f, 3.4f)), randomVector(0.70f)),
                                   random(0.10f, 0.18f), random(0.008f, 0.014f));
            spark.startColor = Color{255, 231, 132, 255};
            spark.endColor = Color{255, 68, 8, 0};
            spark.gravity = -3.0f;
            spark.drag = 0.95f;
        }

        // Smoke begins compact and warm, then rolls upward into irregular
        // grey-brown lobes after the flame itself has vanished.
        for (int index = 0; index < 3; ++index)
        {
            Particle &smoke = emit(Kind::Smoke,
                                   add(position, add(scale(direction, random(0.02f, 0.20f)),
                                                     randomVector(0.045f))),
                                   add(scale(direction, random(0.20f, 0.62f)),
                                       Vector3{random(-0.08f, 0.08f), random(0.20f, 0.45f),
                                               random(-0.08f, 0.08f)}),
                                   random(0.40f, 0.65f), random(0.045f, 0.070f));
            smoke.endSize = random(0.13f, 0.22f);
            smoke.startColor = index < 2 ? Color{145, 127, 102, 98}
                                          : Color{110, 112, 104, 82};
            smoke.endColor = Color{89, 92, 84, 0};
            smoke.drag = 0.955f;
            delayParticle(smoke, 0.045f + index * 0.025f);
        }
    }

    // Creates a compact ricochet on ordinary surfaces. Heavy impacts add the
    // white-hot flash, armor splinters, and pressure ring used when a shell
    // survives long enough to strike a tank. Brick impacts deliberately use
    // the masonry-only path below so a breached wall never reads as a tank.
    void spawnImpact(Vector3 position, Vector3 normal = Vector3{0.0f, 1.0f, 0.0f},
                     bool heavy = false)
    {
        normal = safeNormalize(normal, Vector3{0.0f, 1.0f, 0.0f});
        spawnSurfaceImpact(position, normal, heavy, false);
        if (!heavy)
            return;

        // The flash is intentionally much smaller and shorter than a kill
        // explosion: it identifies armor contact without obscuring the tank.
        for (int index = 0; index < 3; ++index)
        {
            Particle &flash = emit(Kind::Fireball,
                                   add(position, randomVector(0.045f)),
                                   add(scale(normal, random(0.08f, 0.45f)),
                                       randomVector(0.22f)),
                                   random(0.065f, 0.115f),
                                   index == 0 ? 0.10f : random(0.045f, 0.070f));
            flash.endSize = flash.startSize * random(1.5f, 2.0f);
            flash.startColor = index == 0 ? Color{255, 255, 226, 245}
                                          : Color{255, 174, 48, 220};
            flash.endColor = Color{255, 58, 8, 0};
            flash.drag = 0.89f;
        }

        // Angular grey and heat-stained chips read as pieces of armor rather
        // than the red-brown fragments emitted by brick walls.
        for (int index = 0; index < 6; ++index)
        {
            Vector3 velocity = add(scale(normal, random(0.55f, 2.25f)),
                                   randomVector(random(0.85f, 2.6f)));
            velocity.y = std::fabs(velocity.y) + random(0.18f, 1.4f);
            Particle &fragment = emit(Kind::Debris,
                                      add(position, randomVector(0.075f)), velocity,
                                      random(0.34f, 0.72f), random(0.018f, 0.052f));
            fragment.endSize = fragment.startSize * random(0.45f, 0.78f);
            fragment.startColor = index % 3 == 0 ? Color{235, 190, 104, 255}
                                                  : Color{103, 111, 119, 255};
            fragment.endColor = Color{48, 50, 51, 0};
            fragment.gravity = -7.8f;
            fragment.drag = 0.978f;
        }

        for (int index = 0; index < 3; ++index)
        {
            Particle &smoke = emit(Kind::Smoke,
                                   add(position, randomVector(0.085f)),
                                   add(scale(normal, random(0.15f, 0.62f)),
                                       Vector3{random(-0.18f, 0.18f),
                                               random(0.18f, 0.52f),
                                               random(-0.18f, 0.18f)}),
                                   random(0.38f, 0.64f), random(0.045f, 0.08f));
            smoke.endSize = random(0.15f, 0.26f);
            smoke.startColor = index < 2 ? Color{127, 108, 84, 115}
                                         : Color{98, 103, 102, 98};
            smoke.endColor = Color{77, 83, 80, 0};
            smoke.drag = 0.957f;
            delayParticle(smoke, 0.06f);
        }

        spawnShockwave(position, 0.055f, 0.40f, 0.14f,
                       Color{237, 207, 139, 132});
    }

    // A directional masonry burst makes the classic half/full-tile breach
    // readable against the taller 3D building facade. Collision and damage
    // remain governed by the exact 8 px projectile footprint in StageMap.
    void spawnBrickImpact(Vector3 position, Vector3 normal, bool powerShell,
                          bool destroyed)
    {
        normal = safeNormalize(normal, Vector3{0.0f, 0.25f, 1.0f});
        const bool collapse = destroyed || powerShell;
        spawnSurfaceImpact(position, normal, collapse, true);
        const int fragmentCount = powerShell ? 26 : destroyed ? 22 : 10;
        for (int index = 0; index < fragmentCount; ++index)
        {
            Vector3 velocity = add(
                scale(normal, random(0.45f, collapse ? 3.35f : 2.1f)),
                randomVector(collapse ? 2.55f : 1.55f));
            velocity.y = std::fabs(velocity.y) +
                         random(0.45f, collapse ? 2.85f : 1.8f);
            Particle &fragment = emit(Kind::Debris,
                                      add(position, randomVector(collapse ? 0.28f : 0.13f)),
                                      velocity,
                                      random(0.38f, collapse ? 0.90f : 0.68f),
                                      random(0.035f, collapse ? 0.10f : 0.068f));
            fragment.endSize = fragment.startSize * random(0.62f, 0.88f);
            fragment.startColor = index % 4 == 0 ? Color{151, 139, 119, 255}
                                                 : index % 3 == 0
                                                       ? Color{104, 62, 47, 255}
                                                       : Color{178, 76, 52, 255};
            fragment.endColor = Color{70, 55, 47, 0};
            fragment.gravity = -7.8f;
            fragment.drag = 0.975f;
        }

        if (collapse)
        {
            // A breach throws red masonry and a short ochre dust roll. It has
            // no sustained orange fire, which belongs to a burning vehicle.
            for (int index = 0; index < 10; ++index)
            {
                Particle &dust = emit(
                    Kind::Dust,
                    add(position, add(randomHorizontal(0.34f),
                                      Vector3{0.0f, random(-0.18f, 0.30f), 0.0f})),
                    add(scale(normal, random(0.18f, 0.78f)),
                        Vector3{random(-0.62f, 0.62f), random(0.28f, 1.15f),
                                random(-0.62f, 0.62f)}),
                    random(0.58f, 1.02f), random(0.08f, 0.13f));
                dust.endSize = random(0.23f, 0.40f);
                dust.startColor = index % 3 == 0 ? Color{179, 132, 94, 125}
                                                  : Color{140, 111, 85, 106};
                dust.endColor = Color{115, 103, 86, 0};
                dust.drag = 0.92f;
                delayParticle(dust, random(0.0f, 0.16f));
            }

            for (int index = 0; index < 1; ++index)
            {
                Particle &flash = emit(
                    Kind::Fireball, add(position, randomVector(0.08f)),
                    add(scale(normal, random(0.08f, 0.35f)), randomVector(0.18f)),
                    0.075f, 0.075f);
                flash.endSize = 0.12f;
                flash.startColor = Color{248, 225, 170, 200};
                flash.endColor = Color{161, 106, 62, 0};
                flash.drag = 0.90f;
            }

            Vector3 groundWave = position;
            groundWave.y = 0.055f;
            spawnShockwave(groundWave, 0.09f,
                           powerShell ? 0.76f : 0.62f,
                           powerShell ? 0.24f : 0.20f,
                           Color{195, 154, 107, 112});
        }
    }

    void spawnTankExplosion(Vector3 position,
                            Color fireColor = Color{255, 116, 24, 255})
    {
        position.y = std::max(position.y, 0.12f);

        // A few animation beats: pale ignition, an orange broken silhouette,
        // then separate warm-grey smoke rolls. No enclosing luminous sphere.
        for (int index = 0; index < 3; ++index)
        {
            Particle &core = emit(Kind::Fireball,
                                  add(position, randomVector(0.045f)),
                                  randomVector(0.12f),
                                  0.065f + index * 0.040f, 0.15f + index * 0.025f);
            core.endSize = 0.25f + index * 0.055f;
            core.startColor = index == 0 ? Color{255, 252, 217, 255}
                                         : index == 1 ? Color{255, 211, 100, 250}
                                                      : fireColor;
            core.endColor = Color{188, 60, 24, 0};
            core.drag = 0.90f;
        }

        for (int index = 0; index < 14; ++index)
        {
            Vector3 velocity = randomVector(random(0.55f, 1.85f));
            velocity.y = std::fabs(velocity.y) + random(0.25f, 0.90f);
            Particle &fireball = emit(Kind::Fireball,
                                      add(position, randomVector(0.17f)), velocity,
                                      random(0.22f, 0.46f), random(0.10f, 0.19f));
            fireball.endSize = random(0.24f, 0.38f);
            fireball.startColor = index % 4 == 0 ? Color{255, 191, 71, 245}
                                                 : fireColor;
            fireball.endColor = Color{140, 42, 25, 0};
            fireball.gravity = -0.48f;
            fireball.drag = random(0.925f, 0.951f);
            delayParticle(fireball, random(0.025f, 0.13f));
        }

        for (int index = 0; index < 6; ++index)
        {
            Particle &fire = emit(Kind::Fire,
                                  add(position, randomHorizontal(0.26f)),
                                  Vector3{random(-0.16f, 0.16f), random(0.42f, 0.88f),
                                          random(-0.16f, 0.16f)},
                                  random(0.26f, 0.48f), random(0.075f, 0.12f));
            fire.endSize = fire.startSize * 0.45f;
            fire.startColor = fireColor;
            fire.endColor = Color{126, 44, 25, 0};
            fire.gravity = -0.25f;
            fire.drag = 0.945f;
            delayParticle(fire, random(0.16f, 0.40f));
        }

        for (int index = 0; index < 18; ++index)
        {
            Particle &smoke = emit(Kind::Smoke,
                                   add(position, randomVector(0.23f)),
                                   Vector3{random(-0.29f, 0.29f), random(0.42f, 0.88f),
                                           random(-0.29f, 0.29f)},
                                   random(0.88f, 1.65f), random(0.085f, 0.15f));
            smoke.endSize = random(0.28f, 0.47f);
            smoke.startColor = index < 5 ? Color{139, 109, 80, 142}
                                          : index % 3 == 0 ? Color{112, 111, 99, 126}
                                                           : Color{87, 93, 90, 116};
            smoke.endColor = Color{91, 97, 88, 0};
            smoke.drag = random(0.964f, 0.978f);
            delayParticle(smoke, random(0.14f, 0.48f));
        }

        // Short, sparse streaks leave room to read the lobe silhouette.
        for (int index = 0; index < 14; ++index)
        {
            Vector3 velocity = randomVector(random(1.8f, 4.8f));
            velocity.y = std::fabs(velocity.y) + random(0.25f, 1.5f);
            Particle &spark = emit(Kind::Spark, position, velocity,
                                   random(0.18f, 0.48f), random(0.009f, 0.019f));
            spark.startColor = index % 4 == 0 ? Color{255, 245, 184, 255}
                                               : Color{255, 170, 53, 245};
            spark.endColor = Color{214, 65, 25, 0};
            spark.gravity = -7.6f;
            spark.drag = 0.974f;
        }

        for (int index = 0; index < 10; ++index)
        {
            Vector3 velocity = randomVector(random(1.25f, 3.2f));
            velocity.y = std::fabs(velocity.y) + random(0.55f, 1.85f);
            Particle &fragment = emit(Kind::BurningDebris,
                                      add(position, randomVector(0.14f)), velocity,
                                      random(0.55f, 0.94f), random(0.035f, 0.065f));
            fragment.endSize = fragment.startSize * 0.70f;
            fragment.startColor = index % 3 == 0 ? Color{130, 102, 71, 255}
                                                  : Color{77, 83, 78, 255};
            fragment.endColor = Color{53, 56, 50, 0};
            fragment.gravity = -7.4f;
            fragment.drag = 0.978f;
        }

        for (int index = 0; index < 8; ++index)
        {
            Particle &dust = emit(Kind::Dust, add(position, randomHorizontal(0.17f)),
                                  Vector3{random(-0.95f, 0.95f), random(0.10f, 0.36f),
                                          random(-0.95f, 0.95f)},
                                  random(0.44f, 0.78f), random(0.08f, 0.14f));
            dust.endSize = random(0.22f, 0.35f);
            dust.startColor = Color{159, 130, 93, 107};
            dust.endColor = Color{117, 109, 90, 0};
            dust.drag = 0.91f;
        }

        Vector3 groundWave = position;
        groundWave.y = 0.055f;
        spawnShockwave(groundWave, 0.12f, 1.08f, 0.24f,
                       Color{233, 180, 91, 134});
        spawnShockwave(groundWave, 0.055f, 0.52f, 0.105f,
                       Color{255, 234, 173, 178});
    }

    // Call sparingly while a tank moves; a spacing/timer check belongs in gameplay.
    void spawnTrackDust(Vector3 position, Vector3 tankVelocity = Vector3{})
    {
        Particle &dust = emit(Kind::Dust, add(position, randomHorizontal(0.10f)),
                              add(scale(tankVelocity, -0.16f),
                                  Vector3{random(-0.18f, 0.18f), random(0.18f, 0.38f),
                                          random(-0.18f, 0.18f)}),
                              random(0.42f, 0.72f), random(0.045f, 0.085f));
        dust.endSize = dust.startSize * random(2.5f, 3.8f);
        dust.startColor = Color{135, 112, 80, 88};
        dust.endColor = Color{67, 65, 62, 0};
        dust.drag = 0.91f;
    }

    void draw(const Camera3D &camera) const
    {
        const Vector3 forward = safeNormalize(
            add(camera.target, scale(camera.position, -1.0f)), {0.0f, 0.0f, -1.0f});
        const Vector3 right = safeNormalize(cross(forward, camera.up), {1.0f, 0.0f, 0.0f});
        const Vector3 up = safeNormalize(cross(right, forward), {0.0f, 1.0f, 0.0f});
        struct DrawItem
        {
            const Particle *particle;
            float depth;
        };
        std::array<DrawItem, kMaxParticles> order{};
        std::size_t count = 0;
        for (const Particle &particle : particles_)
        {
            if (!particle.active || particle.age < 0.0f)
                continue;
            const Vector3 offset = add(particle.position, scale(camera.position, -1.0f));
            order[count++] = {&particle, offset.x * forward.x +
                                            offset.y * forward.y + offset.z * forward.z};
        }
        std::sort(order.begin(), order.begin() + count,
                  [](const DrawItem &first, const DrawItem &second) {
                      return first.depth != second.depth ? first.depth > second.depth
                                                          : first.particle < second.particle;
                  });

        // Keep opaque world depth testing, but never let a translucent lobe
        // stamp a solid silhouette into the depth buffer. Sorting uses view
        // depth, so an orbiting camera does not reverse the smoke layers.
        rlDrawRenderBatchActive();
        rlDisableDepthMask();
        // Preserve opaque scene alpha. The post-process composites this render
        // texture later; multiplying alpha twice would turn grey smoke black.
        rlSetBlendFactorsSeparate(RL_SRC_ALPHA, RL_ONE_MINUS_SRC_ALPHA,
                                  RL_ONE, RL_ONE_MINUS_SRC_ALPHA, RL_FUNC_ADD, RL_FUNC_ADD);
        BeginBlendMode(BLEND_CUSTOM_SEPARATE);
        for (std::size_t index = 0; index < count; ++index)
        {
            const Particle &particle = *order[index].particle;
            if (particle.kind == Kind::Smoke || particle.kind == Kind::Dust)
                drawSoft(particle, right, up);
            else if (particle.kind == Kind::Debris ||
                     particle.kind == Kind::BurningDebris)
                drawDebris(particle);
            else if (particle.kind != Kind::Spark)
                drawFlame(particle, right, up);
        }
        EndBlendMode();

        BeginBlendMode(BLEND_ADDITIVE);
        for (std::size_t index = 0; index < count; ++index)
        {
            const Particle &particle = *order[index].particle;
            drawEmissive(particle, right, up);
        }
        EndBlendMode();
        rlDrawRenderBatchActive();
        rlEnableDepthMask();
    }

    std::size_t activeCount() const
    {
        std::size_t count = 0;
        for (const Particle &particle : particles_)
            count += particle.active ? 1U : 0U;
        return count;
    }

private:
    enum class Kind : unsigned char
    {
        MuzzleFlash,
        Fire,
        Fireball,
        Spark,
        Smoke,
        Dust,
        Debris,
        BurningDebris,
        Shockwave
    };

    struct Particle
    {
        Kind kind = Kind::Dust;
        Vector3 position{};
        Vector3 velocity{};
        Color startColor{255, 255, 255, 255};
        Color endColor{255, 255, 255, 0};
        float age = 0.0f;
        float life = 1.0f;
        float startSize = 0.1f;
        float endSize = 0.1f;
        float gravity = 0.0f;
        float drag = 1.0f;
        float phase = 0.0f;
        bool active = false;
    };

    static Vector3 add(Vector3 first, Vector3 second)
    {
        return {first.x + second.x, first.y + second.y, first.z + second.z};
    }

    static Vector3 scale(Vector3 vector, float amount)
    {
        return {vector.x * amount, vector.y * amount, vector.z * amount};
    }

    static Vector3 cross(Vector3 first, Vector3 second)
    {
        return {first.y * second.z - first.z * second.y,
                first.z * second.x - first.x * second.z,
                first.x * second.y - first.y * second.x};
    }

    static float length(Vector3 vector)
    {
        return std::sqrt(vector.x * vector.x + vector.y * vector.y + vector.z * vector.z);
    }

    static Vector3 safeNormalize(Vector3 vector, Vector3 fallback)
    {
        const float magnitude = length(vector);
        return magnitude > 0.0001f ? scale(vector, 1.0f / magnitude) : fallback;
    }

    static unsigned char channel(float value)
    {
        return static_cast<unsigned char>(std::clamp(value, 0.0f, 255.0f));
    }

    static Color interpolate(Color first, Color second, float amount)
    {
        amount = std::clamp(amount, 0.0f, 1.0f);
        return Color{channel(first.r + (second.r - first.r) * amount),
                     channel(first.g + (second.g - first.g) * amount),
                     channel(first.b + (second.b - first.b) * amount),
                     channel(first.a + (second.a - first.a) * amount)};
    }

    float nextUnit()
    {
        // Xorshift32 is deterministic, fast, and avoids per-frame allocations.
        randomState_ ^= randomState_ << 13U;
        randomState_ ^= randomState_ >> 17U;
        randomState_ ^= randomState_ << 5U;
        return static_cast<float>(randomState_ & 0x00ffffffU) / 16777215.0f;
    }

    float random(float minimum, float maximum)
    {
        return minimum + (maximum - minimum) * nextUnit();
    }

    Vector3 randomVector(float radius)
    {
        Vector3 vector{random(-1.0f, 1.0f), random(-1.0f, 1.0f), random(-1.0f, 1.0f)};
        return scale(safeNormalize(vector, Vector3{0.0f, 1.0f, 0.0f}), radius * random(0.25f, 1.0f));
    }

    Vector3 randomHorizontal(float radius)
    {
        Vector3 vector{random(-1.0f, 1.0f), 0.0f, random(-1.0f, 1.0f)};
        return scale(safeNormalize(vector, Vector3{1.0f, 0.0f, 0.0f}), radius * random(0.1f, 1.0f));
    }

    Particle &emit(Kind kind, Vector3 position, Vector3 velocity, float life, float size)
    {
        Particle &particle = particles_[cursor_];
        cursor_ = (cursor_ + 1U) % particles_.size();
        particle = Particle{};
        particle.kind = kind;
        particle.position = position;
        particle.velocity = velocity;
        particle.life = std::max(life, 0.001f);
        particle.startSize = size;
        particle.endSize = size;
        particle.phase = static_cast<float>(cursor_ % 37U) * 0.618034f;
        particle.active = true;
        return particle;
    }

    static void delayParticle(Particle &particle, float delay)
    {
        particle.age = -std::max(delay, 0.0f);
    }

    void spawnSurfaceImpact(Vector3 position, Vector3 normal, bool strong,
                            bool masonry)
    {
        const int sparkCount = masonry ? (strong ? 4 : 3) : (strong ? 11 : 7);
        for (int index = 0; index < sparkCount; ++index)
        {
            Vector3 velocity = add(scale(normal, random(1.0f, strong ? 3.6f : 2.8f)),
                                   randomVector(strong ? 1.8f : 1.25f));
            velocity.y += random(0.15f, 1.2f);
            Particle &spark = emit(Kind::Spark, add(position, scale(normal, 0.025f)), velocity,
                                   random(0.12f, strong ? 0.32f : 0.25f),
                                   random(0.008f, strong ? 0.020f : 0.015f));
            spark.startColor = index % 3 == 0 ? Color{255, 250, 194, 255}
                                               : masonry ? Color{255, 155, 69, 255}
                                                         : Color{255, 213, 125, 255};
            spark.endColor = Color{232, 38, 5, 0};
            spark.gravity = -6.5f;
            spark.drag = 0.965f;
        }

        const int dustCount = strong ? 7 : 4;
        for (int index = 0; index < dustCount; ++index)
        {
            Particle &dust = emit(Kind::Dust, add(position, randomHorizontal(0.13f)),
                                  add(scale(normal, random(0.15f, 0.65f)), randomVector(0.34f)),
                                  random(0.30f, 0.56f), random(0.04f, 0.085f));
            dust.endSize = random(0.12f, 0.22f);
            dust.startColor = masonry ? Color{165, 105, 73, 125}
                                      : Color{121, 119, 111, 110};
            dust.endColor = masonry ? Color{81, 62, 52, 0}
                                    : Color{62, 64, 65, 0};
            dust.drag = 0.90f;
        }
    }

    void spawnShockwave(Vector3 position, float startRadius, float endRadius,
                        float life, Color color)
    {
        position.y = std::max(position.y, 0.045f);
        Particle &wave = emit(Kind::Shockwave, position, Vector3{}, life, startRadius);
        wave.endSize = endRadius;
        wave.startColor = color;
        wave.endColor = Color{color.r, color.g, color.b, 0};
    }

    static Color tint(Color color, float value)
    {
        return {channel(color.r * value), channel(color.g * value),
                channel(color.b * value), color.a};
    }

    static float particleSize(const Particle &particle)
    {
        const float progress = std::clamp(particle.age / particle.life, 0.0f, 1.0f);
        return particle.startSize + (particle.endSize - particle.startSize) * progress;
    }

    static Color particleColor(const Particle &particle)
    {
        const float progress = std::clamp(particle.age / particle.life, 0.0f, 1.0f);
        // Four painted colour steps, with continuous opacity at their edges.
        // The global display filter supplies pixels; geometry keeps its curves.
        const float colourStep = std::floor(progress * 4.0f) * 0.25f;
        Color color = interpolate(particle.startColor, particle.endColor, colourStep);
        color.a = channel(particle.startColor.a * (1.0f - progress));
        return color;
    }

    static void drawLobe(Vector3 center, float size, Vector3 right, Vector3 up,
                         Color color, float phase, float stretch = 1.0f)
    {
        constexpr int sides = 12;
        constexpr std::array<float, sides> contour{{
            0.84f, 0.98f, 0.88f, 1.00f, 0.92f, 0.97f,
            0.81f, 0.96f, 1.00f, 0.87f, 0.98f, 0.90f}};
        const float cosine = std::cos(phase), sine = std::sin(phase);
        const Vector3 horizontal = add(scale(right, cosine), scale(up, sine));
        const Vector3 vertical = add(scale(up, cosine), scale(right, -sine));
        std::array<Vector3, sides> points{};
        for (int index = 0; index < sides; ++index)
        {
            const float angle = index * 2.0f * PI / sides;
            points[index] = add(center,
                add(scale(horizontal, std::cos(angle) * size * contour[index]),
                    scale(vertical, std::sin(angle) * size * contour[index] * stretch)));
        }
        const Vector3 heart = add(center, add(scale(right, -size * 0.16f),
                                              scale(up, size * 0.12f)));
        const Vector3 normal = safeNormalize(cross(right, up), {0.0f, 0.0f, 1.0f});
        rlSetTexture(0);
        rlBegin(RL_TRIANGLES);
        rlNormal3f(normal.x, normal.y, normal.z);
        for (int index = 0; index < sides; ++index)
        {
            // Three broad facets keep the cloud painted rather than glassy.
            const Color face = tint(color, index < 4 ? 1.10f : index < 8 ? 0.88f : 0.98f);
            rlColor4ub(face.r, face.g, face.b, face.a);
            rlVertex3f(heart.x, heart.y, heart.z);
            const Vector3 first = points[index], second = points[(index + 1) % sides];
            rlVertex3f(first.x, first.y, first.z);
            rlVertex3f(second.x, second.y, second.z);
        }
        rlEnd();
    }

    static void drawSoft(const Particle &particle, Vector3 right, Vector3 up)
    {
        const float progress = particle.age / particle.life;
        const float size = particleSize(particle);
        Color color = particleColor(particle);
        color.a = channel(color.a * std::min(1.0f, progress * 9.0f));
        const float roll = particle.phase + progress * 0.40f;
        const float squash = particle.kind == Kind::Dust ? 0.70f : 1.0f;
        Color small = tint(color, 0.90f);
        small.a = channel(color.a * 0.72f);
        drawLobe(add(particle.position, add(scale(right, size * 0.47f),
                                            scale(up, size * 0.22f))),
                 size * 0.66f, right, up, small, roll + 1.3f, squash);
        drawLobe(particle.position, size, right, up, color, roll, squash);
        Color crown = tint(color, 1.05f);
        crown.a = channel(color.a * 0.56f);
        drawLobe(add(particle.position, add(scale(right, -size * 0.30f),
                                            scale(up, size * 0.42f))),
                 size * 0.48f, right, up, crown, roll - 0.8f, squash);
    }

    static void drawDebris(const Particle &particle)
    {
        const float size = particleSize(particle);
        const Color color = particleColor(particle);
        rlPushMatrix();
        rlTranslatef(particle.position.x, particle.position.y, particle.position.z);
        rlRotatef(particle.phase * 57.29578f + particle.age * 380.0f, 0.6f, 1.0f, 0.35f);
        DrawCube({}, size * 1.55f, size * 0.65f, size, color);
        DrawCube({0.0f, size * 0.34f, 0.0f}, size * 1.18f, size * 0.06f,
                 size * 0.76f, tint(color, 1.20f));
        rlPopMatrix();
    }

    static void drawFlame(const Particle &particle, Vector3 right, Vector3 up)
    {
        const float progress = particle.age / particle.life;
        const float size = particleSize(particle);
        const Color color = particleColor(particle);
        if (particle.kind == Kind::MuzzleFlash)
        {
            const Vector3 axis = safeNormalize(particle.velocity, {0.0f, 0.0f, -1.0f});
            const float flameLength = size * (3.1f + 0.8f * (1.0f - progress));
            DrawCylinderEx(particle.position, add(particle.position, scale(axis, flameLength)),
                           size * 0.64f, size * 0.025f, 7, color);
            return;
        }
        if (particle.kind == Kind::Shockwave)
        {
            // Broken, low-contrast arcs suggest a pressure kick, not a halo.
            for (int segment = 0; segment < 12; ++segment)
            {
                if (segment % 3 == 0)
                    continue;
                const float angle = segment * 2.0f * PI / 12.0f + particle.phase;
                const float next = angle + 2.0f * PI / 16.0f;
                DrawLine3D(add(particle.position, {std::cos(angle) * size, 0.0f, std::sin(angle) * size}),
                           add(particle.position, {std::cos(next) * size, 0.0f, std::sin(next) * size}), color);
            }
            return;
        }
        if (particle.kind == Kind::Fireball)
        {
            const float phase = particle.phase + progress * 0.28f;
            drawLobe(add(particle.position, add(scale(right, size * 0.43f), scale(up, size * 0.15f))),
                     size * 0.68f, right, up, tint(color, 0.83f), phase + 1.1f);
            drawLobe(particle.position, size, right, up, color, phase, 1.10f);
            if (progress < 0.65f)
            {
                Color hot = interpolate(color, Color{255, 210, 85, color.a}, 0.45f);
                hot.a = channel(color.a * 0.80f);
                drawLobe(add(particle.position, scale(up, -size * 0.14f)),
                         size * 0.48f, right, up, hot, phase + 0.6f, 1.12f);
            }
            return;
        }
        // Small rising tongues fill the last gaps between orange gas and smoke.
        drawLobe(particle.position, size, right, up, color,
                 particle.phase * 0.14f, 1.7f);
    }

    static void drawEmissive(const Particle &particle, Vector3 right, Vector3 up)
    {
        const float progress = particle.age / particle.life;
        const float size = particleSize(particle);
        const Color color = particleColor(particle);
        if (particle.kind == Kind::Spark || particle.kind == Kind::BurningDebris)
        {
            const Vector3 axis = safeNormalize(particle.velocity, {0.0f, 1.0f, 0.0f});
            const bool fragment = particle.kind == Kind::BurningDebris;
            const float trail = std::clamp(length(particle.velocity) * 0.035f, 0.025f, 0.18f);
            const Vector3 tail = add(particle.position, scale(axis, -trail));
            const Color ember = fragment
                ? interpolate(Color{255, 190, 77, 170}, Color{183, 55, 24, 0}, progress)
                : color;
            DrawCylinderEx(tail, particle.position, size * 0.16f,
                           size * (fragment ? 0.40f : 0.75f), 5, ember);
        }
        else if ((particle.kind == Kind::Fireball || particle.kind == Kind::MuzzleFlash) &&
                 particle.startColor.g >= 205 && progress < 0.28f)
        {
            const Color core{255, 246, 198, channel(132.0f * (1.0f - progress / 0.28f))};
            drawLobe(particle.position, size * 0.32f, right, up, core, particle.phase);
        }
    }

    std::array<Particle, kMaxParticles> particles_{};
    std::size_t cursor_ = 0;
    std::uint32_t randomState_ = 0x91e10da5U;
};

#endif
