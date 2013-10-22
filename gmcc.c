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

static int padding(int n) {
    int remainder = n % 8;
    return (remainder == 0)
                ? n
                : n - remainder + 8;
}

static int compile_type_size(data_type_t *type) {
    switch (type->type) {
        case TYPE_CHAR: return 1;
        case TYPE_INT:  return 4;
        case TYPE_PTR:  return 8;

        case TYPE_ARRAY:
            return compile_type_size(type->pointer) * type->size;

        default:
            compile_error("Internal error");
    }
    return 0;
}

int compile(int dump) {

    ast_t *nodes[1024];
    int    total = 0;

    for (int i = 0; i < 1024; i++, total++) {
        ast_t *next = parse_statement();
        if (!next)
            break;
        nodes[i] = next;
    }

    if (!dump) {
        int offset = 0;
        for (ast_t *item = ast_data_locals(); item; item = item->next) {
            offset         += padding(compile_type_size(item->ctype));
            item->local.off = offset;
        }

        gen_data_section();

        printf(".text\n\t");
        printf(".global entry\n");
        printf("entry:\n\t");
        printf("push %%rbp\n\t");
        printf("mov %%rsp, %%rbp\n\t");

        if (ast_data_locals())
            printf("sub $%d, %%rsp\n\t", offset);
    }

    for (int i = 0; i < total; i++) {
        if (dump)
            printf("%s", ast_dump_string(nodes[i]));
        else
            gen_expression(nodes[i]);
    }

    if (!dump) {
        printf("leave\n\t");
        printf("ret\n");
    } else {
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
