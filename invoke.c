/*
 * A simple invocation test application that uses external reference
 * to invoke a compiled binary with gmcc.
 */
#include <stdio.h>
#include <stdlib.h>

extern int gmcc_main(void);

int main() {
    printf("%d\n", gmcc_main());
    return EXIT_SUCCESS;
}
