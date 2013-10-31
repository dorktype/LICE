int call(int value) {
    return value + value;
}

void test() {
    int a = 100;
    int b = call(a);
    expecti(400, call(b));
    expecti(1600, call(call(400)));
}

int main() {
    init("function calls");
    test();
    return ok();
}
