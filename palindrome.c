/******************************************************************************
 *									      *
 *	File:     palindrome.c						      *
 *	Author:   Alison Chaiken <alison@bonnet>			      *
 *	Created:  Thu 26 Mar 2020 02:37:26 PM PDT			      *
 *	Contents: Return true if a given sequence of single-character         *
 *		  values stored in a singly linked list is a palindrome.      *
 *		  Problem and basic approach to solution suggested by         *
 *		  Rizavan Sipai.                                              *
 *									      *
 *	Copyright (c) 2020 Alison Chaiken.				      *
 *       License: GPLv2 or later.                                              *
 *									      *
 ******************************************************************************/

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SIZE 5
#define STACKMAX 256

const char *teststring[SIZE] = {
    /* even-parity palindrome */
    "abcdefghijklmnopqrstuvwxyzzyxwvutsrqponmlkjihgfedcba",
    /* odd-parity palindrome */
    "abcdefghijklmnopqrstuvwxyzyxwvutsrqponmlkjihgfedcba",
    /* odd-parity palindrome 2 */
    "abcdefghijklmnopqrstuvwxyzzzyxwvutsrqponmlkjihgfedcba",
    /* odd-parity not-palindrome */
    "abcdefghijklmnopqrstuvwxyzzyxwvutsrqponmlkjihgfedcb",
    /* even-parity not-palindrome */
    "abcdefghijklmnopqrstuvwxyzyxwvutsrqponmlkjihgfedcb",
};

struct node {
  char data;
  struct node *next;
};

struct stack_holder {
  char data[STACKMAX];
  int HEAD;
} charstack;

void reset_stack() {
  charstack.data[0] = 0;
  charstack.HEAD = 0;
}

/* A node constructor that takes a single char as parameter. */
struct node *make_node(char c) {
  struct node *new_node = (struct node *)malloc(sizeof(struct node));
  if (NULL == new_node) {
    fprintf(stderr, "OOM in make_node()");
    exit(EXIT_FAILURE);
  }
  new_node->data = c;
  new_node->next = NULL;
  return new_node;
}

struct node *init_list(const char c) {
  struct node *first = make_node(c);
  first->next = NULL;
  return first;
}

/* Delete the node immediately after HEAD until the end of the list is reached.
 */
void delete_list(struct node *list) {
  if (NULL == list) {
    return;
  }
  while (NULL != list->next) {
    struct node *current = list->next;
    /* Unlink current, but save pointer to current->next. */
    if (NULL != current->next) {
      list->next = current->next;
      free(current);
    } else {
      /* Free the penultimate node. */
      free(list->next);
      /* Without break, hit a use-after-free exception. */
      break;
    }
  }
  /* Free the HEAD node. */
  free(list);
}

/* Take a provided node onto the end of an existing list. */
struct node *find_end_append_node(struct node *list,
                                  const struct node *new_node) {
  assert((NULL != list) && (NULL != new_node));
  struct node *current = list;
  while (NULL != current->next) {
    current = current->next;
  }
  current->next = make_node(new_node->data);
  current->next->next = NULL;
  return list;
}

/* A list constructor that takes a char array as input. */
struct node *make_list(const char *teststring) {
  int teststringlen = (int)strlen(teststring);
  if (!teststringlen) {
    return NULL;
  }
  /* Points to HEAD and consumes first character. */
  struct node *newlist = init_list(*teststring);
  /* Go past already used first character */
  int i = 1;
  struct node *save;
  /* Not >= since i starts at 1. */
  while (--teststringlen > 0) {
    struct node *new_node = make_node(*(teststring + i++));
    save = new_node;
    newlist = find_end_append_node(newlist, new_node);
    free(save);
  }
  return newlist;
}

bool stack_is_empty() {
  assert(0 <= charstack.HEAD);
  return (0 == charstack.HEAD);
}

bool stack_is_full() {
  assert((STACKMAX - 1) >= charstack.HEAD);
  return ((STACKMAX - 1) == charstack.HEAD);
}

char pop() {
  if (stack_is_empty()) {
    fprintf(stderr, "Stack underflow.\n");
#ifdef TESTING
    abort();
#else
    exit(EXIT_FAILURE);
#endif
  }
  return charstack.data[(charstack.HEAD)--];
}

void push(const char c) {
  if (stack_is_full()) {
    fprintf(stderr, "Stack overflow.\n");
#ifdef TESTING
    abort();
    //   assert_perror(EINVAL);
#else
    exit(EXIT_FAILURE);
#endif
  }
  charstack.data[++charstack.HEAD] = c;
  return;
}

/* In the event of an odd number of list elements, the middle pointer ALWAYS
 * contains the same character as the stack top. In the case of an even number,
 * the middle pointer character and the stack may or may not be the same.  const
 * nature of list avoids memory leaks.
 */
struct node *process_list(const struct node *list) {
  reset_stack();
  if (!list) {
    fprintf(stderr, "Provided list is empty.\n");
    return NULL;
  }
  if (!list->next) {
    push(list->data);
    return (struct node *)list;
  }
  /* Initialization below avoids const-correctness problems. */
  char thischar = list->data;
  struct node *middle_next = list->next;
  struct node *middle = middle_next;
  struct node *end = list->next;

  while ((NULL != end) && (NULL != end->next)) {
    /* push() data from the list (*middle). */
    push(thischar);
    middle = middle_next;
    thischar = middle->data;
    middle_next = middle->next;
    end = end->next->next;
  }
  assert(NULL != middle);
  push(middle->data);
  /* end pointer can't advance, but if it is not already NULL, the stack lacks
   * the middle element. */
  if (NULL == end) {
#ifdef DEBUG
    printf("Odd number of elements.\n");
#endif
  } else {
#ifdef DEBUG
    printf("Even number of elements.\n");
#endif
    /* Move middle to the second half of the list, from which palindrome check
     * progresses forward. */
    middle = middle_next;
  }
  return middle;
}

bool is_palindrome(struct node *middle) {
  while (NULL != middle) {
    if (middle->data != pop()) {
      return false;
    }
    middle = middle->next;
  }
  return true;
}

#ifndef TESTING

int main() {
  reset_stack();
  for (int i = 0; i < SIZE; i++) {
    struct node *this_list = make_list(teststring[i]);
    struct node *middle = process_list(this_list);
    bool ans = is_palindrome(middle);

    delete_list(this_list);
    printf("teststring %s %s a palindrome.\n\n", teststring[i],
           (ans == 1 ? "is" : "is not"));
  }
  exit(EXIT_SUCCESS);
}
#endif
