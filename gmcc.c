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

    list_t *block = parse_function_list();

    if (!dump) {
        gen_data_section();
    }

    for (list_iter_t *it = list_iterator(block); !list_iterator_end(it); ) {
        if (!dump)
            gen_function(list_iterator_next(it));
        else
            printf("%s\n", ast_dump_block_string(list_iterator_next(it)));
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
