#include <string.h>
#include <stdlib.h>

#include "gmcc.h"

// symbol / variable management
static var_t *var_list = NULL;

var_t *var_find(const char *name) {
    var_t *var = var_list;

    for (; var; var = var->next)
        if (!strcmp(name, var->name))
            return var;

    return NULL;
}

var_t *var_create(char *name) {
    var_t *var = (var_t*)malloc(sizeof(var_t));

    var->name      = name;
    var->placement = (var_list)
                        ? var_list->placement + 1
                        : 1;

    // append
    var->next      = var_list;
    var_list       = var;

    return var;
}
