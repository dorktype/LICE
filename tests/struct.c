void test() {
    struct s {
        int a;
        int b;

        int array[3];
    } d;

    d.a = 100;
    d.b = 200;
    d.array[0] = d.a;
    d.array[1] = d.b;
    d.array[2] = d.array[1];

    expecti(d.a, 100);
    expecti(d.b, 200);
    expecti(d.array[0], 100);
    expecti(d.array[1], 200);
    expecti(d.array[2], 200);

    struct s *p = &d;
    (*p).a = 101;
    (*p).b = 202;
    p->array[0] = p->a;
    p->array[1] = p->b;
    *(p->array + 2) = 104;

    expecti(p->a, 101);
    expecti(p->b, 202);
    expecti(p->array[0], 101);
    expecti(p->array[1], 202);
    expecti(p->array[2], 104);
}

int main() {
    init("struct");
    test();
    printf(" [OK]\n");

    return 0;
}
