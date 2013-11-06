void test() {
    int i = 0;

    i +=  5;   expecti(i, 5);
    i -=  4;   expecti(i, 1);
    i *=  100; expecti(i, 100);
    i /=  2;   expecti(i, 50);
    i %=  6;   expecti(i, 2);
    i &=  1;   expecti(i, 0);
    i |=  8;   expecti(i, 8);
    i ^=  2;   expecti(i, 10);
    i <<= 2;   expecti(i, 40);
    i >>= 2;   expecti(i, 10);
}

int main() {
    init("compound assignment operators");
    test();

    return ok();
}
