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
        reversed(create_list(reversed_namelist, LISTLEN)) {
    savea = alist;
    saver = reversed;
  }

  ~ReverseListTest() {
    delete_list(&savea);
    delete_list(&saver);
  }

  struct node *alist, *reversed, *savea, *saver;
};

TEST_F(ReverseListTest, CreationIsCorrect) {
  size_t ctr = 0U;
  // The prepend()-based method of creating the list results in reversal.
  for (const char *name : reversed_namelist) {
    EXPECT_EQ(0, strcmp(name, alist->name));
    alist = alist->next;
    ctr++;
  }
  EXPECT_EQ(ctr, LISTLEN);
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
