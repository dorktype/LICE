void test() {
    char a = 'a';
    unsigned char b = 'b';
    signed char c = 'c';

    int d = 'd';
    unsigned int e = 'e';
    signed int f = 'f';

    short g = 'g';
    short int h = 'h';
    unsigned short i = 'i';
    unsigned short int j = 'j';
    signed short k = 'k';
    signed short int l = 'l';

    long m = 'm';
    long int n = 'n';
    unsigned long o = 'o';
    unsigned long int p = 'p';
    signed long q = 'q';
    signed long int r = 'r';

    long long s = 's';
    long long int t = 't';
    unsigned long long u = 'u';
    unsigned long long int v = 'v';
    signed long long w = 'w';
    signed long long int x = 'x';

    expecti(a, 'a');
    expecti(b, 'b');
    expecti(c, 'c');
    expecti(d, 'd');
    expecti(e, 'e');
    expecti(f, 'f');
    expecti(g, 'g');
    expecti(h, 'h');
    expecti(i, 'i');
    expecti(j, 'j');
    expecti(k, 'k');
    expecti(l, 'l');
    expecti(m, 'm');
    expecti(n, 'n');
    expecti(o, 'o');
    expecti(p, 'p');
    expecti(q, 'q');
    expecti(r, 'r');
    expecti(s, 's');
    expecti(t, 't');
    expecti(u, 'u');
    expecti(v, 'v');
    expecti(w, 'w');
    expecti(x, 'x');
}

int main() {
    init("type specifiers");
    test();
    printf(" [OK]\n");

    return 0;
}
