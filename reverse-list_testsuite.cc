#include "gtest/gtest.h"

using namespace std;

#define DEBUG

// Hide main.
#define TESTING

#include "reverse-list.c"

const char *reversed_namelist[LISTLEN] = {"oil",  "our", "have",  "you",
                                          "that", "out", "turns", "it"};

struct ReverseListTest : public ::testing::Test {
  ReverseListTest()
      : alist(create_list(namelist, LISTLEN)),
        reversed(create_list(reversed_namelist, LISTLEN)) {}
  ~ReverseListTest() {
    delete_list(&alist);
    delete_list(&reversed);
  }
  struct node *alist, *reversed;
};

TEST(SimpleListTest, EmptyInput) {
  struct node *HEAD = create_list(NULL, 0);
  EXPECT_EQ(NULL, HEAD);
  EXPECT_EQ(0U, count_nodes(HEAD));
  reverse_list(&HEAD);
  EXPECT_TRUE(are_equal(NULL, HEAD));
  relink_and_delete_successor(HEAD);
  EXPECT_TRUE(are_equal(NULL, HEAD));
  struct node *HEAD2 = create_list(namelist, 0);
  EXPECT_TRUE(are_equal(HEAD2, HEAD));
}

TEST_F(ReverseListTest, CreationIsCorrect) {
  size_t ctr = 0U;
  struct node *const savea = alist;
  // The prepend()-based method of creating the list results in reversal.
  for (const char *name : reversed_namelist) {
    EXPECT_EQ(0, strcmp(name, alist->name));
    alist = alist->next;
    ctr++;
  }
  EXPECT_EQ(ctr, LISTLEN);
  // Restore for the dtor.
  alist = savea;
}

TEST_F(ReverseListTest, AreEqual) {
  struct node *blist = create_list(namelist, LISTLEN);
  EXPECT_TRUE(are_equal(alist, blist));
  delete_list(&blist);
}

TEST_F(ReverseListTest, DoubleReverseIsIdempotent) {
  EXPECT_FALSE(are_equal(reversed, alist));
  reverse_list(&alist);
  EXPECT_TRUE(are_equal(reversed, alist));
  reverse_list(&alist);
  EXPECT_FALSE(are_equal(reversed, alist));
}

TEST_F(ReverseListTest, CountNodes) { EXPECT_EQ(LISTLEN, count_nodes(alist)); }

TEST_F(ReverseListTest, DeletedList) {
  delete_list(&alist);
  EXPECT_EQ(0, count_nodes(alist));
}

TEST_F(ReverseListTest, DeletedNode) {
  relink_and_delete_successor(alist);
  EXPECT_EQ(LISTLEN - 1, count_nodes(alist));
}
