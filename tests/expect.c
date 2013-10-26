void expecti(int a, int b) {
    if (a != b) {
        printf(" [ERROR]\n");
        printf("    Expected: %d\n", b);
        printf("    Result:   %d\n", a);

        exit(1);
    }
}

void expectf(float a, float b) {
    if (a != b) {
        printf(" [ERROR]\n");
        printf("    Expected: %f\n", b);
        printf("    Result:   %f\n", a);

        exit(1);
    }
}

void expectd(double a, double b) {
    if (a != b) {
        printf(" [ERROR]\n");
        printf("    Expected: %f\n", b);
        printf("    Result:   %f\n", a);

        exit(1);
    }
}

void init(char *message) {
    int size = strlen(message);
    printf("Testing %s ...", message);
    int fill = 40 - size;
    for (int i = 0; i < fill; i++)
        printf(" ");
}
