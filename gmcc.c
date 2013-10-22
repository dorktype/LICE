#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <stdbool.h>

#include "gmcc.h"

void compile_error(const char *fmt, ...) {
    va_list  a;
    va_start(a, fmt);
    vfprintf(stderr, fmt, a);
    fprintf(stderr, "\n");
    va_end(a);

    exit(EXIT_FAILURE);
}

int compile(bool dump) {
    list_t *block = parse_function_list();
    if (!dump) {
        gen_data_section();
        for (list_iter_t *it = list_iterator(block); !list_iterator_end(it); )
            gen_function(list_iterator_next(it));
    } else {
        printf("%s\n", ast_block_string(block));
    }
    return true;
}

int main(int argc, char **argv) {
    argc--;
    argv++;
    return compile(!!(argc && !strcmp(*argv, "--dump-ast")))
            ? EXIT_SUCCESS
            : EXIT_FAILURE;
}
