void test() {
    char a;
    short b;
    int c;
    long d;

    expecti(1, sizeof(a));
    expecti(1, sizeof a);
    //expecti(1, sizeof char);
    expecti(1, sizeof(char));

    expecti(2, sizeof(b));
    expecti(2, sizeof b);
    //expecti(2, sizeof short);
    expecti(2, sizeof(short));

    expecti(4, sizeof(c));
    expecti(4, sizeof c);
    //expecti(4, sizeof int);
    expecti(4, sizeof(int));

    expecti(8, sizeof(d));
    expecti(8, sizeof d);
    //expecti(8, sizeof long);
    expecti(8, sizeof(long));

    int array[4];
    expecti(4*4, sizeof(array));

    struct {
        int a;
        int b;
    } data;
    expecti(8, sizeof(data));
}

int main() {
    init("sizeof operator");
    test();

    return ok();
}
