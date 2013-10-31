int main(int argc, char **argv) {
    typedef int integer;
    typedef char** stringarray;

    init("typedef");

    integer     a = argc;
    stringarray b = argv;

    expecti(a, argc);
    expecti(b, argv);

    return ok();
}
