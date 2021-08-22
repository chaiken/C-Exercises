#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* older container_of() definition. */

/* typeof: http://tigcc.ticalc.org/doc/gnuexts.html#SEC69 */
/*
  grep offsetof /usr/lib/gcc/x86_64-linux-gnu/4.9/include/stddef.h
  #define offsetof(TYPE, MEMBER) __builtin_offsetof (TYPE, MEMBER)
*/
/* include/linux/stddef.h; note include/linux/compiler-gcc4.h also */
/**
 * container_of - cast a member of a structure out to the containing structure
 * @ptr:	the pointer to the member.
 * @type:	the type of the container struct this is embedded in.
 * @member:	the name of the member within the struct.
 *
 */
#define container_of(ptr, type, member)                                        \
  ({                                                                           \
    const typeof(((type *)0)->member) *__mptr = (ptr);                         \
    (type *)((char *)__mptr - offsetof(type, member));                         \
  })

/* from include/linux/types.h in kernel source */
struct list_head {
  struct list_head *next, *prev;
};

/* from include/linux/poison.h */
/*
 * These are non-NULL pointers that will result in page faults
 * under normal circumstances, used to verify that nobody uses
 * non-initialized list entries.
 */
#define POISON_POINTER_DELTA 0
#define LIST_POISON1 ((void *)0x100 + POISON_POINTER_DELTA)
#define LIST_POISON2 ((void *)0x122 + POISON_POINTER_DELTA)

/* from list.h in Linux kernel source. */

/*
 * Simple doubly linked list implementation.
 *
 * Some of the internal functions ("__xxx") are useful when
 * manipulating whole lists rather than single entries, as
 * sometimes we already know the next/prev entries and we can
 * generate better code by using them directly rather than
 * using the generic single-entry routines.
 */

#define LIST_HEAD_INIT(name)                                                   \
  { &(name), &(name) }

#define LIST_HEAD(name) struct list_head name = LIST_HEAD_INIT(name)

/**
 * INIT_LIST_HEAD - Initialize a list_head structure
 * @list: list_head structure to be initialized.
 *
 * Initializes the list_head to point to itself.  If it is a list header,
 * the result is an empty list.
 */
static inline void INIT_LIST_HEAD(struct list_head *list) {
  /*	WRITE_ONCE(list->next, list); */
  list->next = list;
  list->prev = list;
}

/**
 * list_entry - get the struct for this entry
 * @ptr:	the &struct list_head pointer.
 * @type:	the type of the struct this is embedded in.
 * @member:	the name of the list_head within the struct.
 */
#define list_entry(ptr, type, member) container_of(ptr, type, member)

/**
 * list_first_entry - get the first element from a list
 * @ptr:	the list head to take the element from.
 * @type:	the type of the struct this is embedded in.
 * @member:	the name of the list_head within the struct.
 *
 * Note, that list is expected to be not empty.
 */
#define list_first_entry(ptr, type, member)                                    \
  list_entry((ptr)->next, type, member)

/**
 * list_last_entry - get the last element from a list
 * @ptr:	the list head to take the element from.
 * @type:	the type of the struct this is embedded in.
 * @member:	the name of the list_head within the struct.
 *
 * Note, that list is expected to be not empty.
 */
#define list_last_entry(ptr, type, member) list_entry((ptr)->prev, type, member)

/**
 * list_entry_is_head - test if the entry points to the head of the list
 * @pos:	the type * to cursor
 * @head:	the head for your list.
 * @member:	the name of the list_head within the struct.
 */
#define list_entry_is_head(pos, head, member) (&pos->member == (head))

/**
 * list_next_entry - get the next element in list
 * @pos:	the type * to cursor
 * @member:	the name of the list_head within the struct.
 */
#define list_next_entry(pos, member)                                           \
  list_entry((pos)->member.next, typeof(*(pos)), member)

/**
 * list_prev_entry - get the prev element in list
 * @pos:	the type * to cursor
 * @member:	the name of the list_head within the struct.
 */
#define list_prev_entry(pos, member)                                           \
  list_entry((pos)->member.prev, typeof(*(pos)), member)

/**
 * list_for_each_entry	-	iterate over list of given type
 * @pos:	the type * to use as a loop cursor.
 * @head:	the head for your list.
 * @member:	the name of the list_head within the struct.
 */
#define list_for_each_entry(pos, head, member)                                 \
  for (pos = list_first_entry(head, typeof(*pos), member);                     \
       !list_entry_is_head(pos, head, member);                                 \
       pos = list_next_entry(pos, member))

//

/**
 * list_for_each_entry_reverse - iterate backwards over list of given type.
 * @pos:	the type * to use as a loop cursor.
 * @head:	the head for your list.
 * @member:	the name of the list_head within the struct.
 */
#define list_for_each_entry_reverse(pos, head, member)                         \
  for (pos = list_last_entry(head, typeof(*pos), member);                      \
       !list_entry_is_head(pos, head, member);                                 \
       pos = list_prev_entry(pos, member))

// Saves a copy of the next entry in case another thread deletes it before the
// final pos assignment. Doing so prevents the &pos->member access in
// list_entry_is_head() in the next iteration.
/**
 * list_for_each_entry_safe - iterate over list of given type safe against
 * removal of list entry
 * @pos:	the type * to use as a loop cursor.
 * @n:		another type * to use as temporary storage
 * @head:	the head for your list.
 * @member:	the name of the list_head within the struct.
 */
#define list_for_each_entry_safe(pos, n, head, member)                         \
  for (pos = list_first_entry(head, typeof(*pos), member),                     \
      n = list_next_entry(pos, member);                                        \
       !list_entry_is_head(pos, head, member);                                 \
       pos = n, n = list_next_entry(n, member))

/**
 * list_is_first -- tests whether @list is the first entry in list @head
 * @list: the entry to test
 * @head: the head of the list
 */
static inline int list_is_first(const struct list_head *list,
                                const struct list_head *head) {
  return list->prev == head;
}

/**
 * list_is_last - tests whether @list is the last entry in list @head
 * @list: the entry to test
 * @head: the head of the list
 */
static inline int list_is_last(const struct list_head *list,
                               const struct list_head *head) {
  return list->next == head;
}

/*
 * Insert a new entry between two known consecutive entries.
 *
 * This is only for internal list manipulation where we know
 * the prev/next entries already!
 */
static inline void __list_add(struct list_head *new, struct list_head *prev,
                              struct list_head *next) {
  /*	if (!__list_add_valid(new, prev, next))
        return; */
  next->prev = new;
  new->next = next;
  new->prev = prev;
  /*	WRITE_ONCE(prev->next, new); */
  prev->next = new;
}

/**
 * list_add - add a new entry
 * @new: new entry to be added
 * @head: list head to add it after
 *
 * Insert a new entry after the specified head.
 * This is good for implementing stacks.
 */
static inline void list_add(struct list_head *new, struct list_head *head) {
  __list_add(new, head, head->next);
}

/*
 * Delete a list entry by making the prev/next entries
 * point to each other.
 *
 * This is only for internal list manipulation where we know
 * the prev/next entries already!
 */
static inline void __list_del(struct list_head *prev, struct list_head *next) {
  next->prev = prev;
  /*	WRITE_ONCE(prev->next, next); */
  prev->next = next;
}

static inline void __list_del_entry(struct list_head *entry) {
  /*	if (!__list_del_entry_valid(entry))
        return; */

  __list_del(entry->prev, entry->next);
}

/**
 * list_del - deletes entry from list.
 * @entry: the element to delete from the list.
 * Note: list_empty() on entry does not return true after this, the entry is
 * in an undefined state.
 */
static inline void list_del(struct list_head *entry) {
  __list_del_entry(entry);
  entry->next = LIST_POISON1;
  entry->prev = LIST_POISON2;
}

struct fruit_node {
  char *payload;
  struct list_head listp;
};

const char *fruits[] = {"apple",     "banana",     "chamoya",
                        "dandelion", "elderberry", 0};

int build_list(struct list_head *headp) {
  // Construct the list.
  int i = 0;
  while (fruits[i]) {
    // The list pointers get linked into the list, not the full struct.
    struct fruit_node *element =
        (struct fruit_node *)malloc(sizeof(struct fruit_node));
    assert(0 != element);
    size_t len = strlen(fruits[i]) + 1;
    element->payload = (char *)malloc(len);
    assert(0 != element->payload);
    strncpy(element->payload, fruits[i], len);
    // Insert element after HEADP.
    list_add(&element->listp, headp);
    i++;
  }
  return i;
}

void check_list(struct list_head *headp) {
  struct fruit_node *cursor, *temp;
  int ctr = 4;
  list_for_each_entry_safe(cursor, temp, headp, listp) {
    assert(0 == strcmp(fruits[ctr], cursor->payload));
    assert(!list_entry_is_head(cursor, headp, listp));
    ctr--;
  }
  assert(-1 == ctr);
}

void del_list(struct list_head *headp) {
  struct fruit_node *cursor, *temp;
  list_for_each_entry_safe(cursor, temp, headp, listp) {
    struct fruit_node *prev_entry =
        list_entry(cursor->listp.prev, struct fruit_node, listp);
    assert(sizeof(struct list_head) + sizeof(char *) == sizeof(*prev_entry));

    // The list is circular, so a complete traverse encounters HEAD, not NULL.
    if (!list_entry_is_head(prev_entry, headp, listp)) {
      assert(0 != prev_entry->payload);
      // Set prev pointer of current element to HEAD.
      cursor->listp.prev = headp;
      // Set next pointer of HEAD to current element.
      headp->next = cursor->listp.next;
      free(prev_entry->payload);
      free(prev_entry);
    }
    /*  triggers use-after-free
    if (list_is_last(cursor->listp.next, headp)) {
      struct fruit_node *last_entry =
          list_entry(cursor->listp.next, struct fruit_node, listp);
      printf("freeing %p\n", last_entry);
      free(last_entry->payload);
      free(last_entry);
      }
 */
  }
  // Loop exits because cursor points at the list head.
  assert(headp == cursor->listp.next);

  struct fruit_node *last_entry =
      list_entry(cursor->listp.prev, struct fruit_node, listp);
  free(last_entry->payload);
  free(last_entry);
}

int main() {
  LIST_HEAD(anewlist);

  int index = build_list(&anewlist);
  assert(4 == --index);

  // Check the list.
  check_list(&anewlist);

  struct fruit_node *head_element =
      list_entry(&anewlist, struct fruit_node, listp);
  // The payload is empty.
  assert(sizeof(struct list_head) + sizeof(char *) == sizeof(*head_element));

  // Delete the list.
  del_list(&anewlist);

  exit(EXIT_SUCCESS);
}
