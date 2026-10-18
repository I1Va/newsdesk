#pragma once

#include "core/clock.h"

namespace newsdesk {

// функция подсчета доверия репортера
double Decay(double weight, TimePoint anchor, TimePoint now, Duration half_life);

}  // namespace newsdesk
