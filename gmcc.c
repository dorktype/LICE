#include <stdio.h>
#include <stdlib.h>

#define GMCC_ENTRY     "gmcc_main"
#define GMCC_ASSEMBLER "as"
#define GMCC_LINKER    "gcc"

static void generate(int input) {
    FILE *as;

    if (!(as = popen("as -o blob.o", "w")))
        abort();

    fprintf(as,

".text\n\
.global %s\n\
%s:\n\
    mov $%d, %%eax\n\
    ret\n",

        GMCC_ENTRY,
        GMCC_ENTRY,
        input
    );

    fclose(as);
}

static void link(void) {
    system("gcc blob.o invoke.c -o output");
}

int main() {
    int read;
    if (scanf("%d", &read) == EOF) {
        fprintf(stderr, "failed to read integer\n");
        return EXIT_FAILURE;
    }

    generate(read);
    link();
}
