void test() {
    int a = 0xFF;
    int b = 0777;

    long c = 0xDEADBEEFUL;
    long long d = 0xFUL;
    long e = 0xDEADBEEFlu;
    long f = 3735928559LU;

    expecti(a, 255);
    expecti(b, 511);
    expecti(c, 3735928559);
    expecti(d, 15);
    expecti(e, 3735928559);
    expecti(f, 3735928559);

    expecti(3L,   3);
    expecti(3LL,  3);
    expecti(3UL,  3);
    expecti(3LU,  3);
    expecti(3ULL, 3);
    expecti(3LU,  3);
    expecti(3LLU, 3);

    expectd(55.3, 55.3);
    expectd(200, 2e2);
    expectd(0x0.DE488631p8, 0xDE.488631);

    expecti(sizeof(5),    4); // int
    expecti(sizeof(5L),   8); // long
    expecti(sizeof(3.0f), 4); // float
    expecti(sizeof(8.0),  8); // double
}

int main() {
    init("numbers");
    test();

    return ok();
}
