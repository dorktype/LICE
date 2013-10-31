int external_1 = 1337;
int external_2 = 7331;

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
        // TODO fix stack alignment
        //printf("    Expected: %f\n", b);
        //printf("    Result:   %f\n", a);

        exit(1);
    }
}

void expectd(double a, double b) {
    if (a != b) {
        printf(" [ERROR]\n");
        // TODO fix stack alignment
        //printf("    Expected: %f\n", b);
        //printf("    Result:   %f\n", a);

        exit(1);
    }
}

void expects(char *a, char *b) {
    if (strcmp(a, b)) {
        printf(" [ERROR]\n");
        printf("    Expected: %s\n", b);
        printf("    Result:   %s\n", a);

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

int ok() {
    printf("[OK]\n");
    return 0;
}
