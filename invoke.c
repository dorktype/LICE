/*
 * A simple invocation test application that uses external reference
 * to invoke a compiled binary with gmcc.
 */
#include <stdio.h>
#include <stdlib.h>

extern int gmcc_entry(void);

int main(int argc, char **argv) {
    printf("%d\n", gmcc_entry());
    return EXIT_SUCCESS;
}
