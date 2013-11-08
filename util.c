#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "util.h"

// a memory pool
#define MEMORY 0x800000

static unsigned char *memory_pool = NULL;
static size_t         memory_next = 0;

static void memory_cleanup(void) {
    free(memory_pool);
}

void *memory_allocate(size_t bytes) {
    void *value;

    if (!memory_pool) {
        memory_pool = malloc(MEMORY);
        atexit(memory_cleanup);
    }

    value = &memory_pool[memory_next];
    memory_next += bytes;

    return value;
}

// an efficent strechy buffer string
struct string_s {
    char *buffer;
    int   allocated;
    int   length;
};

static void string_reallocate(string_t *string) {
    int   size   = string->allocated * 2;
    char *buffer = memory_allocate(size);

    strcpy(buffer, string->buffer);
    string->buffer    = buffer;
    string->allocated = size;
}

void string_catf(string_t *string, const char *fmt, ...) {
    va_list  va;
    for (;;) {
        int left  = string->allocated - string->length;
        int write;

        va_start(va, fmt);
        write = vsnprintf(string->buffer + string->length, left, fmt, va);
        va_end(va);

        if (left < write) {
            string_reallocate(string);
            continue;
        }
        string->length += write;
        return;
    }
}

string_t *string_create(void) {
    string_t *string  = memory_allocate(sizeof(string_t));
    string->buffer    = memory_allocate(1024);
    string->allocated = 1024;
    string->length    = 0;
    string->buffer[0] = '\0';
    return string;
}

char *string_buffer(string_t *string) {
    return string->buffer;
}

void string_cat(string_t *string, char ch) {
    if (string->allocated == (string->length + 1))
        string_reallocate(string);
    string->buffer[string->length++] = ch;
    string->buffer[string->length]   = '\0';
}

char *string_quote(char *p) {
    string_t *string = string_create();
    while (*p) {
        if (*p == '\"' || *p == '\\')
            string_catf(string, "\\%c", *p);
        else if (*p == '\n')
            string_catf(string, "\\n");
        else
            string_cat(string, *p);
        p++;
    }
    return string->buffer;
}

// a standard double linked list
struct list_node_s {
    void        *element;
    list_node_t *next;
    list_node_t *prev;
};

struct list_iterator_s {
    list_node_t *pointer;
};

list_t *list_create(void) {
    list_t *list = memory_allocate(sizeof(list_t));
    list->length = 0;
    list->head   = NULL;
    list->tail   = NULL;

    return list;
}

void *list_node_create(void *element) {
    list_node_t *node = memory_allocate(sizeof(list_node_t));
    node->element     = element;
    node->next        = NULL;
    node->prev        = NULL;
    return node;
}

void list_push(list_t *list, void *element) {
    list_node_t *node = list_node_create(element);
    if (!list->head)
        list->head = node;
    else {
        list->tail->next = node;
        node->prev       = list->tail;
    }
    list->tail = node;
    list->length++;
}

void *list_pop(list_t *list) {
    if (!list->head)
        return NULL;
    void *element = list->tail->element;
    list->tail = list->tail->prev;
    if (list->tail)
        list->tail->next = NULL;
    else
        list->head = NULL;
    list->length--;
    return element;
}

void *list_shift(list_t *list) {
    if (!list->head)
        return NULL;
    void *element = list->head->element;
    list->head = list->head->next;
    if (list->head)
        list->head->prev = NULL;
    else
        list->tail = NULL;
    list->length--;
    return element;
}

int list_length(list_t *list) {
    return list->length;
}

list_iterator_t *list_iterator(list_t *list) {
    list_iterator_t *iter = memory_allocate(sizeof(list_iterator_t));
    iter->pointer         = list->head;
    return iter;
}

void *list_iterator_next(list_iterator_t *iter) {
    void *ret;

    if (!iter->pointer)
        return NULL;

    ret           = iter->pointer->element;
    iter->pointer = iter->pointer->next;

    return ret;
}

bool list_iterator_end(list_iterator_t *iter) {
    return !iter->pointer;
}

static void list_shiftify(list_t *list, void *element) {
    list_node_t *node = list_node_create(element);
    node->next = list->head;
    if (list->head)
        list->head->prev = node;
    list->head = node;
    if (!list->tail)
        list->tail = node;
    list->length++;
}

list_t *list_reverse(list_t *list) {
    list_t *ret = list_create();
    for (list_iterator_t *it = list_iterator(list); !list_iterator_end(it); )
        list_shiftify(ret, list_iterator_next(it));
    return ret;
}

void *list_tail(list_t *list) {
    if (!list->head)
        return NULL;

    list_node_t *node = list->head;
    while (node->next)
        node = node->next;

    return node->element;
}

// a key value associative table
typedef struct {
    char *key;
    void *value;
} table_entry_t;

void *table_create(void *parent) {
    table_t *table = memory_allocate(sizeof(table_t));
    table->list    = list_create();
    table->parent  = parent;

    return table;
}

void *table_find(table_t *table, const char *key) {
    for (; table; table = table->parent) {
        for (list_iterator_t *it = list_iterator(table->list); !list_iterator_end(it); ) {
            table_entry_t *entry = list_iterator_next(it);
            if (!strcmp(key, entry->key))
                return entry->value;
        }
    }
    return NULL;
}

void table_insert(table_t *table, char *key, void *value) {
    table_entry_t *entry = memory_allocate(sizeof(table_entry_t));
    entry->key           = key;
    entry->value         = value;

    list_push(table->list, entry);
}

void *table_parent(table_t *table) {
    return table->parent;
}

list_t *table_values(table_t *table) {
    list_t *list = list_create();
    for (; table; table = table->parent)
        for (list_iterator_t *it = list_iterator(table->list); !list_iterator_end(it); )
            list_push(list, ((table_entry_t*)list_iterator_next(it))->value);
    return list;
}

list_t *table_keys(table_t *table) {
    list_t *list = list_create();
    for (; table; table = table->parent)
        for (list_iterator_t *it = list_iterator(table->list); !list_iterator_end(it); )
            list_push(list, ((table_entry_t*)list_iterator_next(it))->key);
    return list;
}
