enum {
    A,
    B,
    C
};

enum foo {
    D,
    E,
    F
} value;

void test() {
    expecti(A, 0);
    expecti(B, 1);
    expecti(C, 2);
    expecti(D, 0);
    expecti(E, 1);
    expecti(F, 2);

    value = A; expecti(value, D);
    value = B; expecti(value, E);
    value = C; expecti(value, F);
}

int main() {
    init("enumerations");
    test();
    printf(" [OK]\n");

    return 0;
}
