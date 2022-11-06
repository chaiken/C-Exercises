#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DEBUG

/* The maximum number of characters in a name is MAXNAME-1.*/
#define MAXNAME 32u

#define LISTLEN 8u
const char *namelist[LISTLEN] = {
    "it", "turns", "out", "that", "you", "have", "our", "oil",
};

struct node {
  char *name;
  struct node *next;
};

/* Name must not be empty. */
struct node *alloc_node(const char *name) {
  if (!name) {
    return NULL;
  }
  struct node *newnode = (struct node *)malloc(sizeof(struct node));
  if (!newnode) {
    perror(strerror(-ENOMEM));
    return NULL;
  }

  size_t namelen = (strlen(name) < (MAXNAME - 1)) ? strlen(name) : MAXNAME - 1;
  if (!namelen) {
    perror("Empty name\n");
    return NULL;
  }
  if (namelen < strlen(name)) {
    fprintf(stderr, "Warning: name %s truncated.\n", name);
  }
  newnode->name = strndup(name, namelen);
  newnode->next = NULL;
  return newnode;
}

void delete_node(struct node **oldnode) {
  free((*oldnode)->name);
  free(*oldnode);
  *oldnode = NULL;
}

struct node *prepend_node(struct node *prepended, struct node *headp) {
  if (!prepended) {
    return headp;
  }
  prepended->next = headp;
  return prepended;
}

size_t count_nodes(const struct node *HEAD) {
  if (!HEAD) {
    return 0u;
  }
  /* Work-around constness of HEAD */
  struct node *cursor = alloc_node(HEAD->name);
  /* Initialize. */
  cursor->next = HEAD->next;
  struct node *save = cursor;
  size_t listlen = 0u;
  while (cursor) {
    listlen++;
#ifdef DEBUG
    printf("%s\n", cursor->name);
#endif
    cursor = cursor->next;
  }
  delete_node(&save);
  return listlen;
}

void reverse_list(struct node **headp) {
  if (!headp || (!*headp) || (!(*headp)->next)) {
    return;
  }
  struct node *cursor = (*headp)->next;
  /* Prevent a loop at the end of the list, which is needed since only HEAD is not prepended.*/
  (*headp)->next = NULL;
  while (cursor) {
    /* Save the value of the next pointer of the node to be prepended. */
    struct node *cursor_next = cursor->next;
    /* headp is passed by value, so the assigned is not UB. */
    *headp = prepend_node(cursor, *headp);
    cursor = cursor_next;
  }
}

/* Create nodes from each successive name in the array, prepending them to the HEAD node. */
struct node *create_list() {
  struct node *headp = NULL;
  for (int i = 0; i < (int)LISTLEN; i++) {
    struct node *newnode = alloc_node(namelist[i]);
    if (!newnode) {
      exit(EXIT_FAILURE);
    }
    headp = prepend_node(newnode, headp);
  }
  return headp;
}

/* Delete nodes following HEAD one-by-one, then delete HEAD when end of list is reached. */
void delete_list(struct node **headp) {
  if (!headp) {
    return;
  }
  struct node *cursor = (*headp)->next;
  while (cursor) {
    struct node *cursor_next = cursor->next;
    delete_node(&cursor);
    cursor = cursor_next;
  }
  delete_node(headp);
}

int main(void) {
  struct node *HEAD = create_list();
  assert(NULL != HEAD);
  assert(LISTLEN == count_nodes(HEAD));
  reverse_list(&HEAD);
  assert(LISTLEN == count_nodes(HEAD));
  delete_list(&HEAD);
  assert(NULL == HEAD);
  assert(0U == count_nodes(HEAD));
  exit(EXIT_SUCCESS);
}
