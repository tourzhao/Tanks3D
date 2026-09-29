#ifndef TANKS3D_APP_SHELL_FLIGHT_PRESENTATION_H
#define TANKS3D_APP_SHELL_FLIGHT_PRESENTATION_H

#include <algorithm>

namespace tanks3d::app
{
struct ShellFlightPresentation
{
    bool visible = false;
    float height = 0.0f;
};

// Native shells start with four seconds of life and retain their physical XZ
// origin. Long visual guns only suppress the part of that flight inside the
// barrel; impacts still occur at their original position, even before emergence.
// Lifetime and speed deliberately avoid a dependency on a turning/moving owner.
inline ShellFlightPresentation shellFlightPresentation(
    float remainingLife, float speed, bool impacting, float spawnDistance,
    float muzzleDistance, float muzzleHeight, float flightHeight)
{
    const float traveled = std::max(0.0f, 4.0f - remainingLife) *
                           std::max(0.0f, speed);
    const float emerged = traveled + spawnDistance - muzzleDistance;
    const float fraction = std::clamp(emerged / 1.5f, 0.0f, 1.0f);
    const float blend = fraction * fraction * (3.0f - 2.0f * fraction);
    return {!impacting && emerged >= 0.0f,
            muzzleHeight + (flightHeight - muzzleHeight) * blend};
}
} // namespace tanks3d::app

#endif
