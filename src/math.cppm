export module math;

export int add(int a, int b)
{
    return a + b;
}

export int multiply(int a, int b)
{
    return a * b;
}

export namespace math
{
    double pi = 3.14159;

    double circle_area(double radius)
    {
        return pi * radius * radius;
    }
}
