#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#define GMCC_ENTRY_INT "gmcc_entry_int"
#define GMCC_ENTRY_STR "gmcc_entry_str"
#define GMCC_ASSEMBLER "as -o blob.o"
#define GMCC_LINKER    "gcc blob.o invoke.c -o program"

static void compile_error(const char *fmt, ...) {
    va_list  a;
    va_start(a, fmt);
    vfprintf(stderr, fmt, a);
    fprintf(stderr, "\n");
    va_end(a);

    exit(EXIT_FAILURE);
}

static void compile_integer(FILE *as, int value) {
    int c;
    while ((c = getc(stdin)) != EOF) {
        if (isspace(c))
            break;

        if (!isdigit(c))
            compile_error("Expected numeric, got `%c` instead", c);

        value = value * 10 + (c - '0');
    }

    fprintf(
        as,
        ".text\n\
        .global %s\n\
        %s:\n\
            mov $%d, %%rax\n\
            ret\n",

        GMCC_ENTRY_INT,
        GMCC_ENTRY_INT,
        value
    );
}

static void compile_string(FILE *as) {
    char buffer[1024];
    int  i = 0;
    int  c = EOF;

    for (;;) {
        if ((c = getc(stdin)) == EOF)
            compile_error("Unterminated string");
        if (c == '"')
            break;

        if (c == '\\') {
            if ((c = getc(stdin)) == EOF)
                compile_error("Unterminated line continuation `\\` in string");
        }

        buffer[i++] = c;
        if (i == sizeof(buffer) - 1)
            compile_error("Internal error: OOM");

    }

    buffer[i] = '\0';

    fprintf(
        as,
        ".data\n\
        .gmcc_data:\n\
        .string \"%s\"\n\
        .text\n\
        .global %s\n\
        %s:\n\
            lea .gmcc_data(%%rip), %%rax\n\
            ret\n",
        buffer,
        GMCC_ENTRY_STR,
        GMCC_ENTRY_STR
    );
}

static void compile_go(void) {
    FILE *as = popen(GMCC_ASSEMBLER, "w");
    int   c  = getc(stdin);

    if (!as)
        compile_error("failed to invoke assembler");

    if (isdigit(c))
        compile_integer(as, c - '0');
    else if (c == '"')
        compile_string(as);
    else
        compile_error("Syntax error");

    fclose(as);
    system(GMCC_LINKER);

    printf("compiled, you may run with ./program\n");
}

int main() {
    compile_go();
}
