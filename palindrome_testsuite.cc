/*****************************************************************************
 *	                                                                     *
 *	File:     palindrome_testsuite.cc                                    *
 *	Author:   Alison Chaiken <alison@bonnet>                             *
 *	Created:  Thu 26 Mar 2020 02:29:54 PM PDT                            *
 *	Contents: Exercise the functions in the palindrome program with a    *
 *		  variety of provided input strings which are fed into singly*
 *		  linked lists before testing.  I was not successful in      *
 *		  testing failure of the stack push() function as the        *
 *		  googletest runtime did not trap the error.  That's odd as  *
 *		  I've written C++ before where googletest trapped a failure *
 *		  in similar functions.  Also, I'm not sure why the test for *
 *		  pop() works and that for push() does not.                  *
 *	                                                                     *
 *	Copyright (c) 2020 Alison Chaiken.                                   *
 *       License: GPLv2 or later.                                             *
 *	                                                                     *
 ******************************************************************************/

#include "gtest/gtest.h"
// For EXPECT_EXIT and EXPECT_FATAL_FAILURE, as suggested in
// googletest/googletest/docs/advanced.md.
#include "gtest/gtest-si.h"
#include <sys/time.h>

using namespace std;

// Hide main.
#define TESTING

#include "palindrome.c"

constexpr char even_palindrome[] =
    "abcdefghijklmnopqrstuvwxyzzyxwvutsrqponmlkjihgfedcba";

constexpr char odd_palindrome[] =
    "abcdefghijklmnopqrstuvwxyzyxwvutsrqponmlkjihgfedcba";

constexpr char even_not_palindrome[] = "abcdef";

constexpr char odd_not_palindrome[] = "abcdefg";

constexpr char single[] = "a";

constexpr char empty[] = "";

int count_list_nodes(const struct node *list) {
  if (!list) {
    return 0;
  }
  int i = 1;
  while (list->next) {
    list = list->next;
    i++;
  }
  return i;
}

void print_list(struct node *list) {
  if (!list) {
    return;
  }
  while (list) {
    printf("%c", list->data);
    list = list->next;
  }
  printf("\n");
}

/* Opaque so that it can change. */
int stack_depth() { return charstack.HEAD; }

void print_stack() {
  int top = charstack.HEAD;
  while (0 <= top) {
    printf("%c ", charstack.data[top--]);
  }
  printf("\n");
}

class ListTest : public ::testing::Test {
public:
  ListTest() {
    SetSeed();
    string randstring = to_string(rand());
    first_node = init_list(randstring.front());
    last_node = init_list(randstring.back());
    testlist1 = init_list(*even_palindrome);
    made = make_list(even_palindrome);
  }
  ~ListTest() {
    delete_list(first_node);
    delete_list(last_node);
    delete_list(testlist1);
    delete_list(made);
  }
  struct node *first_node, *last_node, *testlist1, *made;

private:
  static void SetSeed() { srand(static_cast<unsigned>(time(nullptr))); }
};

TEST_F(ListTest, init_list_test) {
  EXPECT_NE(nullptr, first_node);
  EXPECT_GT(129u, first_node->data);
  // No leading zero.
  EXPECT_LT(0u, first_node->data);
  cout << first_node->data << endl;

  EXPECT_NE(nullptr, last_node);
  EXPECT_GT(129u, last_node->data);
  EXPECT_LE(0u, last_node->data);
  cout << last_node->data << endl;

  EXPECT_EQ(1, count_list_nodes(first_node));
  print_list(first_node);
  EXPECT_EQ(1, count_list_nodes(last_node));
  print_list(first_node);
  EXPECT_EQ(1, count_list_nodes(testlist1));
  print_list(first_node);
}

TEST_F(ListTest, find_end_append_node_test) {
  struct node *longer_list = find_end_append_node(testlist1, first_node);
  print_list(longer_list);
  EXPECT_EQ(2, count_list_nodes(longer_list));
  EXPECT_EQ(longer_list->data, testlist1->data);
}

TEST_F(ListTest, make_list) {
  EXPECT_EQ(strlen(even_palindrome), count_list_nodes(made));
  print_list(made);
  struct node *another = make_list(single);
  print_list(another);
  EXPECT_EQ(1, count_list_nodes(another));
  delete_list(another);
  /* Allocates nothing, so needs no deletion. */
  struct node *made = make_list(empty);
  EXPECT_EQ(0, count_list_nodes(made));
}

class StackTest : public testing::Test {
public:
  StackTest() {
    reset_stack();
    six_list = make_list(sixstring.c_str());
    five_list = make_list(fivestring.c_str());
    four_list = make_list(fourstring.c_str());
    three_list = make_list(threestring.c_str());
    two_list = make_list(twostring.c_str());
    one_list = make_list(onestring.c_str());
    empty_list = make_list(emptystring.c_str());
  }
  ~StackTest() {
    delete_list(six_list);
    delete_list(five_list);
    delete_list(four_list);
    delete_list(three_list);
    delete_list(two_list);
    delete_list(one_list);
    delete_list(empty_list);
  }
  string sixstring = "abcdef";
  string fivestring = "abcde";
  string fourstring = "abcd";
  string threestring = "abc";
  string twostring = "ab";
  string onestring = "a";
  string emptystring = "";
  struct node *six_list, *five_list, *four_list, *three_list, *two_list,
      *one_list, *empty_list;
};

TEST_F(StackTest, pop_push_test) {
  push('a');
  EXPECT_FALSE(stack_is_empty());
  EXPECT_FALSE(stack_is_full());
  EXPECT_EQ(1, stack_depth());
  EXPECT_EQ('a', pop());
  EXPECT_EQ(0, stack_depth());
  EXPECT_TRUE(stack_is_empty());
  push('a');
  push('b');
  EXPECT_EQ(2, stack_depth());
  EXPECT_EQ('b', pop());
  EXPECT_EQ(1, stack_depth());
  EXPECT_FALSE(stack_is_empty());
}

TEST_F(StackTest, process_list) {
  struct node *middle = process_list(six_list);
  EXPECT_EQ(3, stack_depth());
  EXPECT_EQ('d', middle->data);
  EXPECT_EQ('c', pop());

  middle = process_list(five_list);
  EXPECT_EQ(3, stack_depth());
  EXPECT_EQ('c', middle->data);
  EXPECT_EQ('c', pop());

  middle = process_list(four_list);
  EXPECT_EQ(2, stack_depth());
  EXPECT_EQ('c', middle->data);
  EXPECT_EQ('b', pop());

  middle = process_list(three_list);
  EXPECT_EQ(2, stack_depth());
  EXPECT_EQ('b', middle->data);
  EXPECT_EQ('b', pop());

  middle = process_list(two_list);
  EXPECT_EQ(1, stack_depth());
  EXPECT_EQ('b', middle->data);
  EXPECT_EQ('b', pop());

  middle = process_list(one_list);
  EXPECT_EQ(1, stack_depth());
  EXPECT_EQ('a', middle->data);
  EXPECT_EQ('a', pop());

  middle = process_list(empty_list);
  EXPECT_EQ(0, stack_depth());
}

TEST(PalindromeTest, is_palindrome) {
  bool answers[SIZE];
  int i = 0;
  for (auto thisstring : teststring) {
    struct node *this_list = make_list(thisstring);
    struct node *middle = process_list(this_list);
    answers[i++] = is_palindrome(middle);
    delete_list(this_list);
  }
  EXPECT_TRUE(answers[0]);
  EXPECT_TRUE(answers[1]);
  EXPECT_TRUE(answers[2]);
  EXPECT_FALSE(answers[3]);
  EXPECT_FALSE(answers[4]);
}

using StackDeathTest = StackTest;

TEST(StackDeathTest, pop_empty_test) {
  reset_stack();
  EXPECT_TRUE(stack_is_empty());
  EXPECT_FALSE(stack_is_full());
  EXPECT_EXIT(pop(), ::testing::KilledBySignal(SIGABRT), "Stack underflow.\n");
}

/* Cannot trap error for some reason.
TEST(StackDeathTest, push_full_test) {
  reset_stack();
  for (int i = 0; i < STACKMAX; i++) {
    push('a');
  }
  EXPECT_FALSE(stack_is_empty());
  EXPECT_TRUE(stack_is_full());
  //EXPECT_EXIT(push('a'), ::testing::KilledBySignal(SIGABRT), "Invalid
argument.\n"); EXPECT_DEATH(push('a'), "Stack overflow.\n");
  // EXPECT_FATAL_FAILURE_ON_ALL_THREADS(push('a'), "Invalid argument.\n");
  //EXPECT_DEATH(push('a'), "Invalid argument.\n");
  // EXPECT_EXIT(push('a'), ::testing::KilledBySignal(SIGABRT), "Stack
overflow.\n");
  } */
