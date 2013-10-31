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

}

int main() {
    init("numbers");
    test();
    return ok();
}
