#include "core/trust.h"

#include <chrono>
#include <cmath>

namespace newsdesk {

double Decay(double weight, TimePoint anchor, TimePoint now, Duration half_life) {
    using SecondsD = std::chrono::duration<double>;

    double elapsed_seconds = std::chrono::duration_cast<SecondsD>(now - anchor).count();
    if (elapsed_seconds < 0.0) {
        elapsed_seconds = 0.0;
    }

    double half_life_seconds = std::chrono::duration_cast<SecondsD>(half_life).count();
    return weight * std::pow(2.0, -elapsed_seconds / half_life_seconds);
}

}  // namespace newsdesk
