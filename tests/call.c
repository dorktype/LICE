int call(int value) {
    return value + value;
}

void test() {
    int a = 100;
    int b = call(a);
    expecti(400, call(b));
    expecti(1600, call(call(400)));
}

int call2(int a, ...);

int main() {
    init("function calls");
    test();
    call2(1);
    return ok();
}

int call2(int a, ...) {
    expecti(a, 1);
}
