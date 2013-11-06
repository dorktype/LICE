void *ignore() {
    return 1024;
}

void test() {
    float  a = (float)1;
    double b = (double)1;
    int    c = (int)1.0;
    short  d = (short)1.0;
    long   e = (long)1.0;

    expectf(a, 1.0);
    expectd(b, 1.0);
    expecti(c, 1);
    expecti(d, 1);
    expecti(e, 1);

    expecti((int*)(ignore()), 1024);

    int n[10];
    *(int*)(n + c) = (int*)(ignore());
    expecti(n[c], 1024);
}

int main() {
    init("type casting");
    test();

    return ok();
}
