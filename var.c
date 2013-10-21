#include <string.h>
#include <stdlib.h>

#include "gmcc.h"

ast_t *var_find(const char *name) {
    ast_t *var = ast_variables();

    for (; var; var = var->value.variable.next)
        if (!strcmp(name, var->value.variable.name))
            return var;

    return NULL;
}
