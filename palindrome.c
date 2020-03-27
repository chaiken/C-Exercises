/******************************************************************************
 *									      *
 *	File:     palindrome.c						      *
 *	Author:   Alison Chaiken <alison@bonnet>			      *
 *	Created:  Thu 26 Mar 2020 02:37:26 PM PDT			      *
 *	Contents: Determine if a given sequence of single-character           *
 *		  values stored in a singly-linked list is a palindrome.      *
 *		  Problem and stack approach to solution suggested by         *
 *		  Rizavan Sipai.  "man string" is original work.              *
 *									      *
 *	Copyright (c) 2020 Alison Chaiken.				      *
 *       License: GPLv2 or later.                                             *
 *									      *
 ******************************************************************************/

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define SIZE 5
#define MAXLEN 256

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
  char data[MAXLEN];
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
  assert((MAXLEN - 1) >= charstack.HEAD);
  return ((MAXLEN - 1) == charstack.HEAD);
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

/* Return a pointer to the middle element of a singly linked list.
 * In the event of an odd number of list elements, the middle pointer ALWAYS
 * contains the same character as the stack top. In the case of an even number,
 * the middle pointer character and the stack may or may not be the same.  const
 * nature of list avoids memory leaks.
 */
struct node *list_to_stack(const struct node *list) {
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

bool is_palindrome_stack(struct node *middle) {
  while (NULL != middle) {
    if (middle->data != pop()) {
      return false;
    }
    middle = middle->next;
  }
  return true;
}

/* Place the contents of a singly linked list into in an array again (because
 * the problem statement has the chars in a list). */
void list_to_array(const struct node *list, char input[]) {
  struct node *copy;
  int i = 0;

  if ((!list) || (!list->data)) {
#ifdef DEBUG
    printf("Input list is empty.\n");
#endif
    return;
  }
  copy = make_node(list->data);

  /* Use the contents of list rather than the list pointer so as to avoid
   * modifying the list. */
  copy->next = list->next;
  /* Cannot free copy after we modify it. */
  const struct node *save = copy;
  while (copy->next) {
    if (i > (MAXLEN - 1)) {
      fprintf(stderr, "Input too long.\n");
      exit(EXIT_FAILURE);
    }
    /* strncat() doesn't work properly if the src string is not null-terminated.
     * Thus it is not possible to strncat() a non-null-terminated byte sequence
     * followed by a '\0'. Therefore create a disposable 1-char string. */
    const char data[2] = {copy->data, '\0'};
    strncat(input, data, strlen(data));
    copy = copy->next;
    i++;
  }
  /* Get final element. */
  assert(NULL != copy);
  const char data[2] = {copy->data, '\0'};
  strncat(input, data, strlen(data));
  free((struct node *)save);
}

/* If A, the length of an array, is an even number, then the halves of the array
 are of length strlen(input)/2 for both A and A-1 cases.
 *
 * Consider even-length string 6 chars long.  first half needs 1st 3 chars;
 second the last 3. first therefore gets chars 0-2 in positions 0-2, while
 second gets characters 3-5 in positions 0-2.
 *
 * Consider odd string 5 chars long.  first needs 1st 3 chars, and get 0-2 in
 * positions 0-2, same as in even case.  second needs last 3, and gets 2-4 in
 * positions 0-2.
 *
 * len is the length of each half.  start is the index in the starting array
 input[] of the first char of the 2nd half. The last char in the 2nd half is at
 index (strlen(input)-1) in input and (len - 1) in output.
 */
void calculate_output_parameters(const char input[], int *len, int *start) {
  *len = strlen(input);
  /* Odd number of elements. */
  if (*len % 2) {
    *len = (*len / 2) + 1;
    /* The two halves overlap. */
    *start = *len - 1;
  } else {
    /* The two halves do not overlap. */
    *len /= 2;
    *start = *len;
  }
}

/* Split the contents of a list of chars into two arrays at the midway point.
 * Load the charaters into the first-half array in the provided order, but into
 * the second-half array in reversed order to facilitate the palindrome test. */
void split_and_partially_reverse_list(const struct node *list, char first[],
                                      char second[]) {
  int len = 0, start = 0, inputlen = 0;

  char input[MAXLEN] = {'\0'};
  list_to_array(list, input);
  calculate_output_parameters(input, &len, &start);

  /* strncpy() doesn't work properly if the src string is not null-terminated.
   * Thus it is not possible to strncpy() a non-null-terminated byte sequence,
   * then a '\0'. Therefore create a disposable string len chars long. */
  char data2[MAXLEN] = {'\0'};
  int j = 0;
  for (j = 0; j < len; j++) {
    data2[j] = input[j];
  }
  data2[len] = '\0';
  strncpy(first, data2, len + 1);

  strncpy(data2, "", 1);
  inputlen = (int)strlen(input);
  /* Put the elements in the second-half array in the reverse order that they
   * appear in the input array in order to faciliate the palindrome test. */
  for (j = start; j < inputlen; j++) {
    /* Load into second-half array in input order: data2[j - start] = input[j];
     */
    data2[inputlen - (j + 1)] = input[j];
  }
  data2[len] = '\0';
  strncpy(second, data2, len + 1);
}

bool is_palindrome_arrays(const char first[], const char second[]) {
  return (!strcmp(first, second));
}

#ifndef TESTING

long get_timestamp() {
  struct timespec ts;
  if (-1 == clock_gettime(CLOCK_MONOTONIC, &ts)) {
    perror("clock_gettime");
    exit(EXIT_FAILURE);
  }
  return (((1e9 * ts.tv_sec) + ts.tv_nsec));
}

void stack_method() {
  for (int i = 0; i < SIZE; i++) {
    struct node *this_list = make_list(teststring[i]);
    struct node *middle = list_to_stack(this_list);
    bool ans = is_palindrome_stack(middle);

    delete_list(this_list);
    printf("teststring %s %s a palindrome.\n", teststring[i],
           (ans == 1 ? "is" : "is not"));
  }
}

/* TODO: dedup with stack_method() using a function pointer. */
void array_method() {
  for (int i = 0; i < SIZE; i++) {
    struct node *this_list = make_list(teststring[i]);
    char first[MAXLEN], second[MAXLEN];

    split_and_partially_reverse_list(this_list, first, second);
    bool ans = is_palindrome_arrays(first, second);

    delete_list(this_list);
    printf("teststring %s %s a palindrome.\n", teststring[i],
           (ans == 1 ? "is" : "is not"));
  }
}

int main() {
  long end, start = get_timestamp();

  reset_stack();
  stack_method();
  end = get_timestamp();
  printf("Stack processing took %lu nanoseconds.\n", (end - start));

  printf("\n");

  start = get_timestamp();
  array_method();
  end = get_timestamp();
  printf("Array processing took %lu nanoseconds.\n", (end - start));

  exit(EXIT_SUCCESS);
}
#endif
