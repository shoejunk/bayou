import math;

#include <fmt/core.h>

int main()
{
    fmt::println("5 + 3 = {}", add(5, 3));
    fmt::println("4 * 7 = {}", multiply(4, 7));
    fmt::println("Circle area (r=5): {}", math::circle_area(5.0));

    return 0;
}
