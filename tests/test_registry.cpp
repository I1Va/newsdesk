#include <doctest/doctest.h>

#include "core/registry.h"

using namespace newsdesk;

TEST_CASE("Registry finds a client that was added") {
    Registry registry;
    registry.Add(ClientInfo{"alice", 0.6, Role::kReporter});

    const ClientInfo* found = registry.Find("alice");
    REQUIRE(found != nullptr);
    CHECK(found->id == "alice");
    CHECK(found->trust == doctest::Approx(0.6));
    CHECK(found->role == Role::kReporter);
}

TEST_CASE("Registry returns nullptr for an unknown client") {
    Registry registry;
    CHECK(registry.Find("ghost") == nullptr);
    CHECK_FALSE(registry.Exists("ghost"));
}

TEST_CASE("Registry Exists reflects added clients") {
    Registry registry;
    registry.Add(ClientInfo{"editor1", 1.0, Role::kEditor});
    CHECK(registry.Exists("editor1"));
}
