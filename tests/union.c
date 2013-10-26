void test() {
    union {
        int  value;
        char byte[4];
    } data;
    data.value   = 0;
    data.byte[0] = 255;

    expecti(data.value, 255);
}

int main() {
    init("union");
    test();
    printf(" [OK]\n");

    return 0;
}
