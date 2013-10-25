#ifndef GMCC_UTIL_HDR
#define GMCC_UTIL_HDR
#include <stdbool.h>

/*
 * Type: string_t
 *  A type capable of representing a self-resizing string with
 *  O(1) strlen.
 */
typedef struct string_s string_t;

/*
 * Function: string_create
 *  Create a string object
 */
string_t *string_create(void);

/*
 * Function: string_buffer
 *  Return the raw buffer of a string object
 */
char *string_buffer(string_t *string);

/*
 * Function: string_cat
 *  Append a character to a string object
 */
void string_cat(string_t *string, char ch);

/*
 * Function: string_catf
 *  Append a formatted string to a string object
 */
void string_catf(string_t *string, const char *fmt, ...);

/*
 * Function: string_quote
 *  Escape a string's quotes
 */
char *string_quote(char *p);

/*
 * Macro: SENTINEL_LIST
 *  Initialize an empty list in place
 */
#define SENTINEL_LIST ((list_t) { \
        .length    = 0,           \
        .head      = NULL,        \
        .tail      = NULL         \
})

/*
 * Type: list_iterator_t
 *  A type capable of representing an itrator for a <list>
 */
typedef struct list_iterator_s list_iterator_t;

/*
 * Type: list_t
 *  A standard double-linked list
 */
typedef struct list_s      list_t;

/*
 * Function: list_create
 *  Creates a new list
 */
list_t *list_create(void);

/*
 * Function: list_push
 *  Push an element onto a list
 */
void list_push(list_t *list, void *element);


/*
 * Function: list_length
 *  Used to retrieve length of a list object
 */
int list_length(list_t *list);

/*
 * Function: list_reverse
 *  Reverse the contents of a list
 */
list_t *list_reverse(list_t *list);

/*
 * Function: list_iterator
 *  Create an iterator for a given list object
 */
list_iterator_t *list_iterator(list_t *list);

/*
 * Function: list_iterator_next
 *  Increment the list iterator while returning the given element
 */
void *list_iterator_next(list_iterator_t *iter);

/*
 * Function: list_iterator_end
 *  Test if the iterator is at the end of the list
 */
bool list_iterator_end(list_iterator_t *iter);

/*
 * Function: list_tail
 *  Get the last element in a list
 */
void *list_tail(list_t *list);

typedef struct list_node_s list_node_t;

struct list_s {
    int          length;
    list_node_t *head;
    list_node_t *tail;
};

/*
 * Type: table_t
 *  A key value associative table
 */
typedef struct table_s table_t;

struct table_s {
    list_t  *list;
    table_t *parent;
};

/*
 * Function: table_create
 *  Creates a table_t object
 */
void *table_create(void *parent);

/*
 * Funciton: table_find
 *  Searches for a given value in the table based on the
 *  key associated with it.
 */
void *table_find(table_t *table, const char *key);

/*
 * Function: table_insert
 *  Inserts a value for the given key as an entry in the
 *  table.
 */
void  table_insert(table_t *table, char *key, void *value);

/*
 * Function: table_parent
 *  Returns the parent opaque object for the given table to
 *  be used as the argument to a new table.
 */
void *table_parent(table_t *table);

/*
 * Function: table_values
 *  Generates a list of all the values in the table, useful for
 *  iterating over the values.
 */
list_t *table_values(table_t *table);

/*
 * Macro: SENTINEL_TABLE
 *  Initialize an empty table in place
 */
#define SENTINEL_TABLE ((table_t) { \
    .list   = &SENTINEL_LIST,       \
    .parent = NULL                  \
})

#endif
