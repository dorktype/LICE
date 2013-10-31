void test() {
    int array[5];
    array[0] = 1;
    array[1] = 2;
    array[2] = 3;
    array[3] = 4;
    array[4] = 5;

    expecti(array[0], 1);
    expecti(array[1], 2);
    expecti(array[2], 3);
    expecti(array[3], 4);
    expecti(array[4], 5);

    int a1 = array[4];
    int a2 = array[3];
    int a3 = array[2];
    int a4 = array[1];
    int a5 = array[0];

    array[0] = a1;
    array[1] = a2;
    array[2] = a3;
    array[3] = a4;
    array[4] = a5;

    expecti(array[0], 5);
    expecti(array[1], 4);
    expecti(array[2], 3);
    expecti(array[3], 2);
    expecti(array[4], 1);
}

int main() {
    init("array");
    test();
    return ok();
}
