/*
 * A simple invocation test application that uses external reference
 * to invoke a compiled binary with gmcc.
 */
#include <stdio.h>
#include <stdlib.h>

extern int   gmcc_entry_int(void) __attribute__((weak));
extern char *gmcc_entry_str(void) __attribute__((weak));

int main() {
    if (gmcc_entry_int)
        printf("%d\n", gmcc_entry_int());
    else if (gmcc_entry_str)
        printf("%s\n", gmcc_entry_str());
    else
        return EXIT_FAILURE;

    return EXIT_SUCCESS;
}
