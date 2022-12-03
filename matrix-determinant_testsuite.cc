#include "gtest/gtest.h"

#define TESTING

#include "matrix-determinant.c"

const double test_matrix[SIZE][SIZE] = {
    {0.0, 2.0, 2.0}, {6.0, 4.0, 10.0}, {6.0, 14.0, 8.0}};

TEST(SimpleMatrixTest, FindIndex) {
  EXPECT_EQ(1U, find_row_index((&test_matrix[0][0]) + 3, test_matrix));
  EXPECT_EQ(0U, find_column_index((&test_matrix[0][0]) + 3, test_matrix));
}

TEST(SimpleMatrixTest, Equality) {
  ASSERT_TRUE(square_are_equal(test_matrix, test_matrix));
  // One matrix is multiple of the other.
  const double test_matrix2[SIZE][SIZE] = {
      {0, 0 - 2.0, -2.0}, {-6.0, -4.0, -10.0}, {-6.0, -14.0, -8.0}};
  ASSERT_FALSE(square_are_equal(test_matrix, test_matrix2));
  // One matrix is subset of the other.
  const double test_matrix4[SIZE][SIZE] = {*test_matrix[0], *test_matrix[1]};
  ASSERT_FALSE(square_are_equal(test_matrix4, test_matrix));
  // One matrix is subset and multiple of the other.
  const double test_matrix3[SIZE][SIZE] = {{0, 0 - 2.0, -2.0},
                                           {-6.0, -4.0, -10.0}};
  ASSERT_FALSE(square_are_equal(test_matrix2, test_matrix3));
}

TEST(SimpleMatrixTest, Submatrix) {
  double upperleft[] = {0, 0, 0, 0};
  get_submatrix(upperleft, SIZE - 1, SIZE - 1, test_matrix);
  double ans[]{
      test_matrix[0][0],
      test_matrix[0][1],
      test_matrix[1][0],
      test_matrix[1][1],
  };
  EXPECT_TRUE(vector_are_equal(upperleft, ans, 2U));
}
