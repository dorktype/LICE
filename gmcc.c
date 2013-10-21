#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include "gmcc.h"

void compile_error(const char *fmt, ...) {
    va_list  a;
    va_start(a, fmt);
    vfprintf(stderr, fmt, a);
    fprintf(stderr, "\n");
    va_end(a);

    exit(EXIT_FAILURE);
}

int compile(int dump) {
    FILE *as;

    if (!dump && !(as = popen(GMCC_ASSEMBLER, "w"))) {
        compile_error("failed to open pipe to assembler");
        return 0;
    }

    parse_compile(as, dump);

    if (!dump) {
        if (pclose(as) != EXIT_SUCCESS)
            return 0;
        if (system(GMCC_LINKER) != EXIT_SUCCESS)
            return 0;
    } else {
        // new line the ast dump
        printf("\n");
    }

    return 1;
}

int main(int argc, char **argv) {
    int dump = 0;
    argc--;
    argv++;

    if (argc && !strcmp(*argv, "--dump-ast"))
        dump = 1;

    return compile(dump)
            ? EXIT_SUCCESS
            : EXIT_FAILURE;
}
