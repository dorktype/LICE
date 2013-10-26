void test() {
    int a = 0;
    if (0) a = 1;
    expecti(a, 0);
    if (1) a = 1;
    expecti(a, 1);
    if (0) { a = 0; } else expecti(a, 1);
    if (1) { a = 0; }
    expecti(a, 0);

    if (0) expecti(0, 1); else {
        expecti(1, 1); // force succeed
    }

    if (1) { expecti(1, 1); } else a = 1;
    expecti(a, 0);

    if (1) {
        if (0) { expecti(1, 0); } else expecti(1, 1);
        if (1) {
            if (1) expecti(1, 1); else { expecti(1, 0); }
        } else
            expecti(0, 1);
    } else {
        expecti(1, 1);
    }
}

int main() {
    init("control flow");
    test();
    printf(" [OK]\n");
    return 0;
}
