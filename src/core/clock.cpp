#include "core/clock.h"

namespace newsdesk {

TimePoint SystemClock::Now() const { return std::chrono::system_clock::now(); }

}  // namespace newsdesk
