void test() {
    struct incomplete;

    struct s {
        int a;
        int b;

        int array[3];
    } d;

    d.a = 100;
    d.b = 200;
    d.array[0] = d.a;
    d.array[1] = d.b;
    d.array[2] = d.array[1];

    expecti(d.a, 100);
    expecti(d.b, 200);
    expecti(d.array[0], 100);
    expecti(d.array[1], 200);
    expecti(d.array[2], 200);
}

int main() {
    init("struct");
    test();
    return ok();
}
