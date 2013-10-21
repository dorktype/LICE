#include <string.h>
#include <stdlib.h>

#include "gmcc.h"

ast_t *var_find(const char *name) {
    ast_t *var = ast_variables();

    for (; var; var = var->variable.next)
        if (!strcmp(name, var->variable.name))
            return var;

    return NULL;
}
