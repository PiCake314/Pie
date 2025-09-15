#define CATCH_CONFIG_MAIN
#include "catch.hpp"
#include "TestSuite.hxx"


TEST_CASE("Named Arguments") {

    const auto src =
R"(
print = __builtin_print;
add = (x: Int, y, a: Int, b, c, d: Int, e: String, f, g, z: Double): Int => 0;

print(add(g = 7, 1, c = 3, 2, 4, 5, f = 6, 1, "", 1.));
)";


    REQUIRE(run(src) == "0");
}




