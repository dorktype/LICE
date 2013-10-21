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

int compile(ast_t *ast) {
    FILE *as = popen(GMCC_ASSEMBLER, "w");
    if (!as) {
        compile_error("failed to open pipe to assembler");
        return 0;
    }

    if (ast->type == ast_type_data_str)
        gen_emit_string(as, ast);
    else {
        // emit the function label
        fprintf(as,".text\n.global %s\n%s:\n", GMCC_ENTRY_INT, GMCC_ENTRY_INT);

        // now do the integer expression
        gen_emit_expression(as, ast);

        // return
        fprintf(as, "ret\n");
    }

    // assembled? now link
    pclose(as);
    system(GMCC_LINKER);

    return 1;
}

int main(int argc, char **argv) {
    ast_t *ast = parse();

    argc--;
    argv++;

    if (argc && !strcmp(*argv, "--dump-ast")) {
        ast_dump(ast);
        printf("\n");
    } else {
        if (!compile(ast))
            compile_error("Compilation error");
        else
            printf("Success!\nrun ./program now\n");
    }

    return EXIT_SUCCESS;
}
