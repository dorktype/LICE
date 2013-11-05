void test() {
    union incomplete;

    union {
        int  value;
        char byte[4];
    } data;
    data.value   = 0;
    data.byte[0] = 255;

    union complete {
        union incomplete *i;
    } completed;

    union incomplete {
        int a;
    } incompleted = { 255 };

    completed.i = &incompleted;

    expecti(data.value, 255);
    expecti(completed.i->a, 255);
}

int main() {
    init("union");
    test();
    return ok();
}
