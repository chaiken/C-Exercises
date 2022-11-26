#include "gtest/gtest.h"

#define TESTING

#include "matrix-determinant.c"

TEST(SimpleMatrixTest, FindIndex) {
  EXPECT_EQ(1U, find_row_index((&test_matrix[0][0]) + 3));
  EXPECT_EQ(0U, find_column_index((&test_matrix[0][0]) + 3));
}

TEST(SimpleMatrixTest, Submatrix) {
  ASSERT_EQ(test_matrix, test_matrix);
  const double test_matrix2[SIZE][SIZE] = {
      {0, 0 - 2.0, -2.0}, {-6.0, -4.0, -10.0}, {-6.0, -14.0, -8.0}};
  ASSERT_NE(test_matrix, test_matrix2);
}
