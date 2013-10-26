void test() {
    // add sub mul div
    int a = 100;
    int b = 100;
    int c = a + b;

    expecti(c, 200);

    a = 10;
    b = 10;
    c = a * b;
    expecti(c, 100);

    c = a - b;
    expecti(c, 0);

    a = 100;
    c = a / b;
    expecti(c, 10);

    // shifts
    a = 1;
    b = 4;
    c = a << b;
    expecti(c, 16);

    c = b >> a;
    expecti(c, 2);

    // and
    a = 32;
    b = 96;
    c = (a & b);
    expecti(c, 32);

    // or
    a = 32;
    b = 64;
    c = a | b;
    expecti(c, 96);

    // unary
    a = ~0;
    b = 0-1; // don't want to use unary -1
    expecti(a, b);

    a = -1;
    b = 0-1;
    expecti(a, b);

    a = 1024;
    b = 4096;
    c = a ^ b;
    expecti(c, 5120);

    // increment/decrement
    a = 100;
    b = 200;
    a--;
    b++;
    expecti(a, 99);
    expecti(b, 201);
}

int main() {
    init("operators");
    test();
    printf(" [OK]\n");

    return 0;
}
