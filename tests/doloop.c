void test() {
    int a = 0;
    int i = 0;

    do {
        a = a + i++;
    } while (i <= 100);

    expecti(a, 5050);

    i = 5;
    do { i = i + a; } while (0);

    expecti(i, 5055);
}

int main() {
    init("do loop");
    test();

    return ok();
}
