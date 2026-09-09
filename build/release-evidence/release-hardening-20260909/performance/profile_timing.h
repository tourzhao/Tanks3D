#ifndef NATIVE_PROFILE_TIMING_H
#define NATIVE_PROFILE_TIMING_H
#include <chrono>
namespace nativeProfile
{
using Clock = std::chrono::steady_clock;
struct Sample
{
    double render = 0, shadow = 0, world = 0, forest = 0, post = 0;
    double endDrawing = 0, bricks = 0, steel = 0, water = 0, trunks = 0;
    bool shadowRefreshed = false;
};
inline Sample sample;
inline double milliseconds(Clock::duration duration)
{
    return std::chrono::duration<double, std::milli>(duration).count();
}
class Scope
{
public:
    explicit Scope(double &total) : total_(total), start_(Clock::now()) {}
    ~Scope() { total_ += milliseconds(Clock::now() - start_); }
private:
    double &total_;
    Clock::time_point start_;
};
}
#endif
