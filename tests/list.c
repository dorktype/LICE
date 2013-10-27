void test() {
    int array[2] = { 1, 2 };
    expecti(array[0], 1);
    expecti(array[1], 2);

    int autosize[] = { 1, 2, 3 };
    expecti(autosize[0], 1);
    expecti(autosize[1], 2);
    expecti(autosize[2], 3);

    struct {
        int a[4];
    } d = {
        {1, 2, 3, 4}
    };

    expecti(d.a[0], 1);
    expecti(d.a[1], 2);
    expecti(d.a[2], 3);
    expecti(d.a[3], 4);
}

int main() {
    init("initializer lists");
    test();
    printf(" [OK]\n");

    return 0;
}
