int main(int argc, char **argv) {
    typedef int integer;
    typedef char** stringarray;

    init("typedef");

    integer     a = argc;
    stringarray b = argv;

    typedef struct {
        int a;
    } foo_t;

    typedef union {
        foo_t *data;
    } bar_t;

    foo_t foo;
    bar_t bar;
    foo.a = argc;
    bar.data = &foo;

    expecti(a, argc);
    expecti(b, argv);

    expecti(foo.a, a);
    expecti(bar.data->a, a);

    return ok();
}
