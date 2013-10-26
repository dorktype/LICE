void test() {
    int accumulate = 0;
    for (int i = 0; i < 1024; i++)
        accumulate ++;

    expecti(accumulate, 1024);

    for (int i = accumulate; i > 0; i--)
        accumulate--;

    expecti(accumulate, 0);

    for (int i = 1024; i > 0; i--)
        accumulate--;

    expecti(accumulate, -1024);

    for (int i = accumulate; i <= 0; i++)
        accumulate++;

    expecti(accumulate, 1);

    for (int i = 1024; i >= 0; i--)
        accumulate ++;

    expecti(accumulate, 1026);
}

int main() {
    init("for loop");
    test();
    printf(" [OK]\n");

    return 0;
}
