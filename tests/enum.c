
void test() {
    enum {
        A,
        B,
        C
    };

    enum {
        D,
        E,
        F
    } value;

    enum {
        I = 100,
        J,
        K
    };

    expecti(A, 0);
    expecti(B, 1);
    expecti(C, 2);
    expecti(D, 0);
    expecti(E, 1);
    expecti(F, 2);

    value = A; expecti(value, D);
    value = B; expecti(value, E);
    value = C; expecti(value, F);

    expecti(I, 100);
    expecti(J, 101);
    expecti(K, 102);
}

int main() {
    init("enumerations");
    test();
    return ok();
}
