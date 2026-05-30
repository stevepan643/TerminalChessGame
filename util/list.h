/*
 * .___________.  ______   _______
 * |           | /      | /  _____|
 * `---|  |----`|  ,----'|  |  __
 *     |  |     |  |     |  | |_ |
 *     |  |     |  `----.|  |__| |
 *     |__|      \______| \______|
 * 
 * TCG - Terminal Chess Game
 * Copyright (c) 2026   Steve Pan
 * 
 * File: list.h
 * 
 * Description:
 *      Common header for list data structure used in the project.
 * 
 * This file is part of TCG.
 */

#ifndef TCG_LIST_H
#define TCG_LIST_H

#include <stddef.h>     /* For offsetof */

/* Intrusive linked list (inspired by Linux kernel implementation) */
struct ListHead {
    struct ListHead *prev;
    struct ListHead *next;
};

/* Initialize a list head */
#define LIST_HEAD_INIT(name) { &(name), &(name) }
/* Declare a list head */
#define LIST_HEAD(name) \
    struct ListHead name = LIST_HEAD_INIT(name)

/* Initialize a list */
static inline void INIT_LIST_HEAD(struct ListHead *list) {
    list->next = list;
    list->prev = list;
}

/* Get the struct for this entry */
#define container_of(ptr, type, member) \
    ((type *)((char *)(ptr) - offsetof(type, member)))
/* Get the struct for this entry */
#define list_entry(ptr, type, member) \
    container_of(ptr, type, member)

/* Add a new entry between two known consecutive entries. */
static inline void __list_add(struct ListHead *new_node,
                              struct ListHead *prev,
                              struct ListHead *next) {
    next->prev = new_node;
    new_node->next = next;
    new_node->prev = prev;
    prev->next = new_node;
}

/* Add a new entry after the specified head (at the beginning of the list) */
static inline void list_add(struct ListHead *new_node, struct ListHead *head) {
    __list_add(new_node, head, head->next);
}

/* Add a new entry before the specified head (at the end of the list) */
static inline void list_add_tail(struct ListHead *new_node, struct ListHead *head) {
    __list_add(new_node, head->prev, head);
}

/* Delete a list entry by making the prev/next entries point to each other */
static inline void __list_del(struct ListHead *prev, struct ListHead *next) {
    next->prev = prev;
    prev->next = next;
}

/* Deletes entry from list and reinitializes it */
static inline void list_del(struct ListHead *entry) {
    __list_del(entry->prev, entry->next);
    entry->next = NULL;
    entry->prev = NULL;
}

/* Iterate over a list */
#define list_for_each(pos, head) \
    for (pos = (head)->next; pos != (head); pos = pos->next)

/* Iterate over a list safe against removal of list entry */
#define list_for_each_safe(pos, n, head) \
    for (pos = (head)->next, n = pos->next; pos != (head); \
        pos = n, n = pos->next)

/* Iterate over a list of a specific type */
#define list_for_each_entry(pos, type, head, member) \
    for (pos = list_entry((head)->next, type, member); \
         &pos->member != (head); \
         pos = list_entry(pos->member.next, type, member))

#endif /* TCG_LIST_H */