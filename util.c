#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "util.h"

// an efficent strechy buffer string
struct string_s {
    char *buffer;
    int   allocated;
    int   length;
};

static void string_reallocate(string_t *string) {
    int   size   = string->allocated * 2;
    char *buffer = malloc(size);

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
    string_t *string  = (string_t*)malloc(sizeof(string_t));
    string->buffer    = malloc(16);
    string->allocated = 16;
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

struct list_node_s {
    void        *element;
    list_node_t *next;
};

struct list_iterator_s {
    list_node_t *pointer;
};

list_t *list_create(void) {
    list_t *list = (list_t*)malloc(sizeof(list_t));
    list->length = 0;
    list->head   = NULL;
    list->tail   = NULL;

    return list;
}

void *list_node_create(void *element) {
    list_node_t *node = (list_node_t*)malloc(sizeof(list_node_t));
    node->element     = element;
    node->next        = NULL;

    return node;
}

void list_push(list_t *list, void *element) {
    list_node_t *node = list_node_create(element);
    if (!list->head)
        list->head = node;
    else
        list->tail->next = node;
    list->tail = node;
    list->length++;
}

int list_length(list_t *list) {
    return list->length;
}

list_iterator_t *list_iterator(list_t *list) {
    list_iterator_t *iter = (list_iterator_t*)malloc(sizeof(list_iterator_t));
    iter->pointer     = list->head;
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
