#include <catch2/catch_test_macros.hpp>

TEST_CASE("NullTest", "[NullTest]") {
    SECTION("Null") {
        REQUIRE(1==1);
    }
}
