#ifndef TANKS3D_BATTLE_FX_H
#define TANKS3D_BATTLE_FX_H

#include <raylib.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

// Lightweight, texture-free battle effects for the Y-up Tanks 3D world.
// Keep one BattleFx instance in the game, call update() once per frame, and
// call draw() while a 3D camera is active. All spawn methods are allocation-free.
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
        for (int index = 0; index < 7; ++index)
        {
            const float spread = index == 0 ? 0.0f : 0.21f;
            const Vector3 axis = safeNormalize(
                add(direction, randomVector(spread)), direction);
            Particle &flash = emit(Kind::MuzzleFlash, position, axis,
                                   index == 0 ? 0.092f : random(0.095f, 0.145f),
                                   index == 0 ? 0.165f : random(0.115f, 0.155f));
            flash.endSize = flash.startSize * 0.22f;
            flash.startColor = index == 0 ? Color{255, 253, 218, 255}
                                           : (index & 1) == 0
                                                 ? Color{255, 196, 73, 245}
                                                 : color;
            flash.endColor = Color{255, 73, 10, 0};
        }

        // Hot wisps detach from the main pressure cone and fade quickly.
        for (int index = 0; index < 3; ++index)
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
        for (int index = 0; index < 5; ++index)
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
        for (int index = 0; index < 5; ++index)
        {
            Particle &smoke = emit(Kind::Smoke,
                                   add(position, add(scale(direction, random(0.02f, 0.20f)),
                                                     randomVector(0.045f))),
                                   add(scale(direction, random(0.20f, 0.62f)),
                                       Vector3{random(-0.08f, 0.08f), random(0.20f, 0.45f),
                                               random(-0.08f, 0.08f)}),
                                   random(0.48f, 0.82f), random(0.055f, 0.105f));
            smoke.endSize = smoke.startSize * random(2.7f, 4.1f);
            smoke.startColor = index < 2 ? Color{116, 94, 72, 105}
                                          : Color{79, 80, 79, 88};
            smoke.endColor = Color{42, 45, 47, 0};
            smoke.drag = 0.955f;
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
        for (int index = 0; index < 5; ++index)
        {
            Particle &flash = emit(Kind::Fireball,
                                   add(position, randomVector(0.045f)),
                                   add(scale(normal, random(0.08f, 0.45f)),
                                       randomVector(0.22f)),
                                   random(0.075f, 0.145f),
                                   index == 0 ? 0.115f : random(0.055f, 0.095f));
            flash.endSize = flash.startSize * random(2.0f, 3.1f);
            flash.startColor = index == 0 ? Color{255, 255, 226, 245}
                                          : Color{255, 174, 48, 220};
            flash.endColor = Color{255, 58, 8, 0};
            flash.drag = 0.89f;
        }

        // Angular grey and heat-stained chips read as pieces of armor rather
        // than the red-brown fragments emitted by brick walls.
        for (int index = 0; index < 12; ++index)
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

        for (int index = 0; index < 7; ++index)
        {
            Particle &smoke = emit(Kind::Smoke,
                                   add(position, randomVector(0.085f)),
                                   add(scale(normal, random(0.15f, 0.62f)),
                                       Vector3{random(-0.18f, 0.18f),
                                               random(0.18f, 0.52f),
                                               random(-0.18f, 0.18f)}),
                                   random(0.48f, 0.88f), random(0.055f, 0.11f));
            smoke.endSize = smoke.startSize * random(2.4f, 3.8f);
            smoke.startColor = index < 2 ? Color{117, 83, 55, 125}
                                         : Color{66, 69, 73, 112};
            smoke.endColor = Color{37, 40, 43, 0};
            smoke.drag = 0.957f;
        }

        spawnShockwave(position, 0.055f, 0.66f, 0.18f,
                       Color{255, 224, 142, 205});
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
        const int fragmentCount = powerShell ? 30 : destroyed ? 26 : 13;
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
                                      random(0.48f, collapse ? 1.15f : 0.82f),
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
            // A final breach gets a broad rolling masonry cloud and a very
            // short warm pressure flash. Partial half/quarter damage remains
            // compact, so the player can read when the whole wall vanished.
            for (int index = 0; index < 14; ++index)
            {
                Particle &dust = emit(
                    Kind::Dust,
                    add(position, add(randomHorizontal(0.34f),
                                      Vector3{0.0f, random(-0.18f, 0.30f), 0.0f})),
                    add(scale(normal, random(0.18f, 0.78f)),
                        Vector3{random(-0.62f, 0.62f), random(0.28f, 1.15f),
                                random(-0.62f, 0.62f)}),
                    random(0.78f, 1.42f), random(0.10f, 0.19f));
                dust.endSize = dust.startSize * random(3.4f, 5.4f);
                dust.startColor = index % 3 == 0 ? Color{184, 119, 78, 175}
                                                  : Color{112, 82, 66, 145};
                dust.endColor = Color{59, 53, 49, 0};
                dust.drag = 0.92f;
                delayParticle(dust, random(0.0f, 0.16f));
            }

            for (int index = 0; index < 4; ++index)
            {
                Particle &flash = emit(
                    Kind::Fireball, add(position, randomVector(0.08f)),
                    add(scale(normal, random(0.08f, 0.35f)), randomVector(0.18f)),
                    random(0.10f, 0.18f), random(0.075f, 0.14f));
                flash.endSize = flash.startSize * random(1.8f, 2.8f);
                flash.startColor = index == 0 ? Color{255, 246, 193, 235}
                                              : Color{255, 141, 46, 205};
                flash.endColor = Color{204, 52, 12, 0};
                flash.drag = 0.90f;
            }

            Vector3 groundWave = position;
            groundWave.y = 0.055f;
            spawnShockwave(groundWave, 0.09f,
                           powerShell ? 1.12f : 0.92f,
                           powerShell ? 0.30f : 0.25f,
                           Color{216, 144, 82, 165});
        }
    }

    void spawnTankExplosion(Vector3 position,
                            Color fireColor = Color{255, 116, 24, 255})
    {
        position.y = std::max(position.y, 0.12f);

        // An instantaneous nested core gives the detonation a white-hot
        // center before the larger lobes separate. Each sphere has a
        // different growth rate, so their silhouettes do not collapse into a
        // single uniformly colored blob.
        for (int index = 0; index < 4; ++index)
        {
            Particle &core = emit(Kind::Fireball,
                                  add(position, randomVector(0.055f)),
                                  randomVector(0.18f),
                                  0.10f + static_cast<float>(index) * 0.045f,
                                  0.22f + static_cast<float>(index) * 0.035f);
            core.endSize = core.startSize * (2.9f - static_cast<float>(index) * 0.28f);
            core.startColor = index == 0 ? Color{255, 255, 236, 255}
                                         : index == 1 ? Color{255, 240, 139, 250}
                                                      : index == 2 ? Color{255, 151, 34, 238}
                                                                   : fireColor;
            core.endColor = index < 2 ? Color{255, 119, 16, 0}
                                      : Color{146, 24, 5, 0};
            core.drag = 0.90f;
        }

        // The main fireball is a staggered blossom of hot gas. Delays are
        // short enough to feel like one blast while still exposing the pale
        // core, orange crown, and darker outer roll in sequence.
        for (int index = 0; index < 32; ++index)
        {
            Vector3 velocity = randomVector(random(0.72f, 3.25f));
            velocity.y = std::fabs(velocity.y) + random(0.35f, 1.75f);
            Particle &fireball = emit(Kind::Fireball,
                                      add(position, randomVector(0.24f)), velocity,
                                      random(0.30f, 0.72f), random(0.13f, 0.31f));
            fireball.endSize = fireball.startSize * random(1.45f, 2.8f);
            fireball.startColor = index % 7 == 0 ? Color{255, 250, 184, 255}
                                                  : index % 3 == 0
                                                        ? fireColor
                                                        : Color{255, 104, 19, 248};
            fireball.endColor = index % 4 == 0 ? Color{109, 20, 8, 0}
                                                : Color{194, 35, 5, 0};
            fireball.gravity = random(-1.05f, 0.10f);
            fireball.drag = random(0.915f, 0.955f);
            delayParticle(fireball, random(0.0f, 0.16f));
        }

        // Low flames keep licking through the wreck after the pressure flash
        // has disappeared, which makes the transition into smoke less abrupt.
        for (int index = 0; index < 18; ++index)
        {
            const Vector3 flameBase = add(
                position, add(randomHorizontal(0.34f),
                              Vector3{0.0f, random(-0.10f, 0.18f), 0.0f}));
            Particle &fire = emit(Kind::Fire, flameBase,
                                  Vector3{random(-0.32f, 0.32f), random(0.62f, 1.65f),
                                          random(-0.32f, 0.32f)},
                                  random(0.42f, 0.92f), random(0.075f, 0.18f));
            fire.endSize = fire.startSize * random(0.42f, 0.95f);
            fire.startColor = index % 4 == 0 ? Color{255, 242, 145, 245}
                                              : fireColor;
            fire.endColor = Color{142, 22, 4, 0};
            fire.gravity = random(-0.48f, -0.12f);
            fire.drag = 0.945f;
            delayParticle(fire, random(0.12f, 0.88f));
        }

        // Staggered, slow-rising lobes build a persistent smoke column rather
        // than one expanding grey ball. Warm early smoke gives way to dense
        // charcoal higher in the plume.
        for (int index = 0; index < 40; ++index)
        {
            Vector3 velocity{random(-0.52f, 0.52f), random(0.48f, 1.55f),
                             random(-0.52f, 0.52f)};
            Particle &smoke = emit(Kind::Smoke, add(position, randomVector(0.28f)), velocity,
                                   random(1.55f, 3.35f), random(0.12f, 0.29f));
            smoke.endSize = smoke.startSize * random(3.6f, 6.2f);
            smoke.startColor = index < 8 ? Color{111, 70, 45, 205}
                                          : index % 5 == 0 ? Color{71, 64, 60, 192}
                                                           : Color{43, 47, 52, 182};
            smoke.endColor = Color{22, 25, 29, 0};
            smoke.drag = random(0.973f, 0.988f);
            delayParticle(smoke, random(0.05f, 0.92f));
        }

        // Bright ballistic streaks sell the initial violence at a distance.
        for (int index = 0; index < 58; ++index)
        {
            Vector3 velocity = randomVector(random(2.2f, 7.2f));
            velocity.y = std::fabs(velocity.y) + random(0.30f, 2.85f);
            Particle &spark = emit(Kind::Spark, position, velocity,
                                   random(0.32f, 0.98f), random(0.010f, 0.032f));
            spark.startColor = index % 6 == 0 ? Color{255, 255, 220, 255}
                                               : index % 3 == 0
                                                     ? Color{255, 207, 78, 255}
                                                     : Color{255, 111, 20, 255};
            spark.endColor = Color{224, 30, 4, 0};
            spark.gravity = -7.6f;
            spark.drag = 0.978f;
            delayParticle(spark, random(0.0f, 0.07f));
        }

        // Larger fragments remain visibly incandescent well after the sparks
        // fade. BurningDebris renders a dark metal body in the alpha pass and
        // a hot ember/trail over it in the additive pass.
        for (int index = 0; index < 20; ++index)
        {
            Vector3 velocity = randomVector(random(1.45f, 4.9f));
            velocity.y = std::fabs(velocity.y) + random(0.75f, 3.25f);
            Particle &fragment = emit(Kind::BurningDebris,
                                      add(position, randomVector(0.17f)), velocity,
                                      random(0.86f, 1.72f), random(0.026f, 0.086f));
            fragment.endSize = fragment.startSize * random(0.48f, 0.82f);
            fragment.startColor = index % 4 == 0 ? Color{132, 92, 55, 255}
                                                  : Color{69, 72, 74, 255};
            fragment.endColor = Color{29, 29, 28, 0};
            fragment.gravity = -7.4f;
            fragment.drag = 0.982f;
        }

        for (int index = 0; index < 18; ++index)
        {
            Particle &dust = emit(Kind::Dust, add(position, randomHorizontal(0.20f)),
                                  Vector3{random(-1.75f, 1.75f), random(0.12f, 0.62f),
                                          random(-1.75f, 1.75f)},
                                  random(0.72f, 1.34f), random(0.11f, 0.24f));
            dust.endSize = dust.startSize * random(3.0f, 4.8f);
            dust.startColor = Color{135, 103, 70, 155};
            dust.endColor = Color{65, 59, 54, 0};
            dust.drag = 0.905f;
        }

        Vector3 groundWave = position;
        groundWave.y = 0.055f;
        spawnShockwave(groundWave, 0.12f, 2.15f, 0.46f,
                       Color{255, 159, 50, 210});
        spawnShockwave(groundWave, 0.055f, 1.12f, 0.22f,
                       Color{255, 247, 190, 245});
        spawnShockwave(groundWave, 0.34f, 2.85f, 0.68f,
                       Color{198, 69, 20, 120});
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

    void draw() const
    {
        // Soft, alpha-blended particles first; emissive particles then bloom
        // visually over them using additive blending.
        BeginBlendMode(BLEND_ALPHA);
        for (const Particle &particle : particles_)
        {
            if (!particle.active || particle.age < 0.0f)
                continue;
            if (particle.kind == Kind::Smoke || particle.kind == Kind::Dust)
                drawSoft(particle);
            else if (particle.kind == Kind::Debris ||
                     particle.kind == Kind::BurningDebris)
                drawDebris(particle);
        }
        EndBlendMode();

        BeginBlendMode(BLEND_ADDITIVE);
        for (const Particle &particle : particles_)
        {
            if (!particle.active || particle.age < 0.0f ||
                particle.kind == Kind::Smoke ||
                particle.kind == Kind::Dust || particle.kind == Kind::Debris)
                continue;
            drawEmissive(particle);
        }
        EndBlendMode();
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
        const int sparkCount = strong ? 28 : 15;
        for (int index = 0; index < sparkCount; ++index)
        {
            Vector3 velocity = add(scale(normal, random(1.0f, strong ? 4.8f : 3.3f)),
                                   randomVector(strong ? 2.4f : 1.55f));
            velocity.y += random(0.15f, 1.2f);
            Particle &spark = emit(Kind::Spark, add(position, scale(normal, 0.025f)), velocity,
                                   random(0.18f, strong ? 0.52f : 0.38f),
                                   random(0.008f, strong ? 0.026f : 0.019f));
            spark.startColor = index % 3 == 0 ? Color{255, 250, 194, 255}
                                               : masonry ? Color{255, 155, 69, 255}
                                                         : Color{255, 142, 35, 255};
            spark.endColor = Color{232, 38, 5, 0};
            spark.gravity = -6.5f;
            spark.drag = 0.965f;
        }

        const int dustCount = strong ? 12 : 7;
        for (int index = 0; index < dustCount; ++index)
        {
            Particle &dust = emit(Kind::Dust, add(position, randomHorizontal(0.13f)),
                                  add(scale(normal, random(0.15f, 0.65f)), randomVector(0.34f)),
                                  random(0.34f, 0.72f), random(0.055f, 0.12f));
            dust.endSize = dust.startSize * random(2.0f, 3.5f);
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

    static void drawSoft(const Particle &particle)
    {
        const float progress = particle.age / particle.life;
        const float size = particle.startSize + (particle.endSize - particle.startSize) * progress;
        const Color color = interpolate(particle.startColor, particle.endColor, progress);
        DrawSphere(particle.position, size, color);

        // A second translucent lobe breaks up the perfectly spherical outline.
        Color lobeColor = color;
        lobeColor.a = static_cast<unsigned char>(lobeColor.a / 2U);
        DrawSphere(add(particle.position, Vector3{size * 0.38f, size * 0.16f, -size * 0.24f}),
                   size * 0.72f, lobeColor);
    }

    static void drawDebris(const Particle &particle)
    {
        const float progress = particle.age / particle.life;
        const float size = particle.startSize +
                           (particle.endSize - particle.startSize) * progress;
        const Color color = interpolate(particle.startColor, particle.endColor, progress);
        DrawCube(particle.position, size * 1.35f, size * 0.72f, size, color);
    }

    static void drawEmissive(const Particle &particle)
    {
        const float progress = particle.age / particle.life;
        const float size = particle.startSize + (particle.endSize - particle.startSize) * progress;
        const Color color = interpolate(particle.startColor, particle.endColor, progress);

        if (particle.kind == Kind::MuzzleFlash)
        {
            const Vector3 axis = safeNormalize(particle.velocity, Vector3{0.0f, 0.0f, -1.0f});
            const float fade = 1.0f - progress;
            const float flameLength = size * (3.8f + 1.8f * fade);
            const Vector3 shoulder = add(particle.position, scale(axis, size * 0.10f));
            const Vector3 tip = add(particle.position, scale(axis, flameLength));
            DrawCylinderEx(shoulder, tip, size * 0.72f, size * 0.025f,
                           10, color);

            const Vector3 coreTip = add(particle.position, scale(axis, flameLength * 0.62f));
            const Color core{255, 252, 217,
                             static_cast<unsigned char>(color.a * 4U / 5U)};
            DrawCylinderEx(particle.position, coreTip, size * 0.35f,
                           size * 0.018f, 9, core);
        }
        else if (particle.kind == Kind::Fireball)
        {
            DrawSphere(particle.position, size, color);
            Color halo = color;
            halo.a = static_cast<unsigned char>(halo.a / 3U);
            DrawSphere(particle.position, size * 1.34f, halo);
            Color core{255, 247, 192,
                       static_cast<unsigned char>(color.a * 2U / 3U)};
            DrawSphere(particle.position, size * 0.52f, core);
        }
        else if (particle.kind == Kind::Spark)
        {
            const Vector3 tail = add(particle.position,
                                     scale(safeNormalize(particle.velocity, Vector3{0.0f, 1.0f, 0.0f}),
                                           -std::clamp(length(particle.velocity) * 0.045f, 0.045f, 0.26f)));
            DrawCylinderEx(tail, particle.position, size * 0.42f, size, 5, color);
            DrawSphere(particle.position, size * 1.15f, color);
        }
        else if (particle.kind == Kind::BurningDebris)
        {
            const Vector3 axis = safeNormalize(particle.velocity,
                                               Vector3{0.0f, 1.0f, 0.0f});
            const float trailLength = std::clamp(length(particle.velocity) * 0.055f,
                                                 0.06f, 0.34f);
            const Vector3 tail = add(particle.position, scale(axis, -trailLength));
            const Color ember = interpolate(Color{255, 225, 112, 255},
                                             Color{255, 49, 6, 0}, progress);
            DrawCylinderEx(tail, particle.position, size * 0.20f,
                           size * 0.72f, 6, ember);
            DrawSphere(particle.position, size * 0.88f, ember);
        }
        else if (particle.kind == Kind::Shockwave)
        {
            DrawCircle3D(particle.position, size, Vector3{1.0f, 0.0f, 0.0f}, 90.0f, color);
            Color inner = color;
            inner.a = static_cast<unsigned char>(inner.a / 2U);
            DrawCircle3D(particle.position, size * 0.92f,
                         Vector3{1.0f, 0.0f, 0.0f}, 90.0f, inner);
        }
        else
        {
            // Advected fire is a tapered tongue aligned to its velocity. This
            // also improves explosions, whose flames now stretch with motion.
            const Vector3 axis = safeNormalize(particle.velocity, Vector3{0.0f, 1.0f, 0.0f});
            const float flameLength = size * (1.9f + (1.0f - progress) * 1.8f);
            const Vector3 tail = add(particle.position, scale(axis, -flameLength));
            DrawCylinderEx(tail, particle.position, size * 0.04f,
                           size * 0.68f, 8, color);
            Color core{255, 250, 205, static_cast<unsigned char>(color.a * 3U / 4U)};
            const Vector3 coreTail = add(particle.position, scale(axis, -flameLength * 0.48f));
            DrawCylinderEx(coreTail, particle.position, size * 0.02f,
                           size * 0.31f, 7, core);
        }
    }

    std::array<Particle, kMaxParticles> particles_{};
    std::size_t cursor_ = 0;
    std::uint32_t randomState_ = 0x91e10da5U;
};

#endif
