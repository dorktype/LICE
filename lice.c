#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>

#include "lice.h"

void compile_error(const char *fmt, ...) {
    va_list  a;
    va_start(a, fmt);
    vfprintf(stderr, fmt, a);
    fprintf(stderr, "\n");
    va_end(a);

    exit(EXIT_FAILURE);
}

int compile_begin(bool dump) {
    list_t *block = parse_run();
    if (!dump) {
        gen_data_section();
    }
    for (list_iterator_t *it = list_iterator(block); !list_iterator_end(it); ) {
        if (!dump) {
            gen_function(list_iterator_next(it));
        } else {
            printf("%s", ast_string(list_iterator_next(it)));
        }
    }
    return true;
}

int main(int argc, char **argv) {
    argc--;
    argv++;
    return compile_begin(!!(argc && !strcmp(*argv, "--dump-ast")))
            ? EXIT_SUCCESS
            : EXIT_FAILURE;
}
