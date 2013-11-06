void test() {
    int i = 0;

    goto a;
    i = 500;

a:
    expecti(i, 0);
    i++;

b:
    if (i >= 500) goto c;
    expecti(i, 1);
    i++;

c:  if (i >= 500) goto d;
    expecti(i, 2);
    i++;

d:  if (i == 3) goto e;
    expecti(0, 1);

e:
}

int main() {
    init("goto");
    test();

    return ok();
}
