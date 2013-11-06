void test() {
    int a = 0;
    switch (1+2) {
        case 0: expecti(0, 1);
        case 3: a = 3; break;
        case 1: expecti(0, 1);
    }
    expecti(a, 3);

    a = 0;
    switch (1) {
        case 0: a++;
        case 1: a++;
        case 2: a++;
        case 3: a++;
    }
    expecti(a, 3);

    a = 0;
    switch (100) {
        case 0:
            break;
        default:
            a++;
    }

    expecti(a, 1);

    // yes duffs device works
    a = 0;
    int accumulate = 38;
    switch (accumulate % 8) {
        case 0: do { a++;
        case 7:      a++;
        case 6:      a++;
        case 5:      a++;
        case 4:      a++;
        case 3:      a++;
        case 2:      a++;
        case 1:      a++;
                } while ((accumulate -= 8) > 0);
    }

    expecti(a, 38);
}

int main() {
    init("switch statement");
    test();

    return ok();
}

