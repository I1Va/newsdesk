#include <doctest/doctest.h>

#include "core/trust.h"

using namespace newsdesk;
using namespace std::chrono_literals;

namespace {

const TimePoint kBase = TimePoint{} + 1000h;

}  // namespace

TEST_CASE("Decay at zero elapsed time returns the full weight") {
    CHECK(Decay(1.0, kBase, kBase, 6h) == doctest::Approx(1.0));
}

TEST_CASE("Decay after one half life halves the weight") {
    CHECK(Decay(1.0, kBase, kBase + 6h, 6h) == doctest::Approx(0.5));
}

TEST_CASE("Decay after two half lives quarters the weight") {
    CHECK(Decay(1.0, kBase, kBase + 12h, 6h) == doctest::Approx(0.25));
}

TEST_CASE("Decay scales with the input weight") {
    CHECK(Decay(0.4, kBase, kBase + 6h, 6h) == doctest::Approx(0.2));
}

TEST_CASE("Decay clamps negative elapsed time to zero") {
    CHECK(Decay(1.0, kBase, kBase - 1h, 6h) == doctest::Approx(1.0));
}
