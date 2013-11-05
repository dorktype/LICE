void test() {
    int a = 0;
    int i = 0;

    while (i <= 100)
        a = a + i++;

    expecti(a, 5050);

    a = 5;
    i = 0;

    while (i <= 100) {
        a = a + i++;
    }

    expecti(a, 5055);
}

int main() {
    init("while loop");
    test();

    return ok();
}
