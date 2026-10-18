#pragma once

#include <chrono>

namespace newsdesk {

using TimePoint = std::chrono::system_clock::time_point;
using Duration = std::chrono::system_clock::duration;

class Clock {
public:
    virtual ~Clock() = default;
    virtual TimePoint Now() const = 0;
};

class SystemClock : public Clock {
public:
    TimePoint Now() const override;
};

}  // namespace newsdesk
