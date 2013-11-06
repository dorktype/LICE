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

    struct {
        int a;
        struct { char b[5]; }; // todo fix
    } data = {
        100 { 'h', 'e', 'l', 'p', 0 }
    };

    //strcpy(data.b, "help");

    expecti(d.a[0], 1);
    expecti(d.a[1], 2);
    expecti(d.a[2], 3);
    expecti(d.a[3], 4);

    expecti(data.a, 100);
    expects(data.b, "help");
}

int main() {
    init("initializer lists");
    test();
    return ok();
}
