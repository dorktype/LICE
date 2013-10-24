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
#define SENTINEL_LIST ((list_t) {      \
        .length    = 0,                \
        .head      = NULL,             \
        .tail      = NULL              \
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
#endif
