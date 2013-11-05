void test_standard() {
    char a;
    short b;
    int c;
    long d;
    long long e;
    short int f;
    long int g;
    long long int h;
    long int long i;
    float j;
    double k;
    long double l;
}

void test_signess() {
    signed char a;
    signed short b;
    signed int c;
    signed long  d;
    signed long long e;
    signed short int f;
    signed long int g;
    signed long long int h;

    unsigned char i;
    unsigned short j;
    unsigned int k;
    unsigned long  l;
    unsigned long long m;
    unsigned short int n;
    unsigned long int o;
    unsigned long long int p;
}

void test_storage() {
    static a;
    auto b;
    register c;
    static int d;
    auto int e;
    register int f;
}

void test_odd() {
    int unsigned const const *const a;
    int *const b[5];
    unsigned const *const c[5];
    static const unsigned *const d;
    const static signed const *const e;
    int unsigned auto *const *const f;
    void *restrict g;
    volatile int *const restrict h;

    int *(*A)[3];
    char (*(*B[3])())[5];
    int (*(*C)(void))[3];
    float *(*(*D)(int))(double **, char);
    unsigned **(*(*E)[5])(char const *, int *);
    char *(*F(const char *))(char *);

    char *G(
        char (*a)(int),
        struct {
            char (*a)(
                union {
                    struct {
                        int (a[5])[5];
                        enum { A, B, C } b;
                    };
                    char (*p)(int[]);
                } a
            );
            int b;
        } **b
    );
}

int main() {
    init("type specifiers");
    test_standard();
    test_signess();
    test_storage();
    test_odd();
    return ok();
}
