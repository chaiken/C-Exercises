/* Calculate the determinant of a random 3x3 matrix. */

#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#define SIZE 3

/* Ended up not needing these two functions for the simple 3x3 determinant. */
int find_row_index(const double *elementp, const double (*source)[SIZE]) {
  int offset = (elementp - &source[0][0]);
  return (offset / SIZE);
}

int find_column_index(const double *elementp, const double (*source)[SIZE]) {
  int offset = (elementp - &source[0][0]);
  return (offset % SIZE);
}

/* Change the value of  what submatrix points to, not the submatrix pointer. */
void get_submatrix(double *submatrix, const int excluded_row,
                   const int excluded_column, const double (*source)[SIZE]) {
  for (int i = 0; i < SIZE; i++) {
    for (int j = 0; j < SIZE; j++) {
      if ((excluded_row == i) || (excluded_column == j)) {
        continue;
      }
      *submatrix = source[i][j];
      submatrix++;
    }
  }
}

/* For the 2x2 matrix,
 * the determinant is ((matrix[0] * matrix[3]) - (matrix[1]  * matrix[2])).
 */
double submatrix_determinant(const double *submatrix) {
  return (((*submatrix) * (*(submatrix + 3))) -
          ((*(submatrix + 1)) * (*(submatrix + 2))));
}

/*  For each element of an nxn (square) matrix, multiply the element by the
 * value of the determinant of the submatrix which includes neither the column
 * nor the row of the element. */
double determinant(const double (*source)[SIZE]) {
  double sum = 0.0;
  for (int j = 0; j < SIZE; j++) {
    //  Flatten the 2x2 submatrix for convenience.
    double submatrix[4] = {0, 0, 0, 0};
    get_submatrix(submatrix, 0, j, source);
    sum += pow(-1.0, j) * source[0][j] * submatrix_determinant(submatrix);
  }
  return sum;
}

bool vector_are_equal(const double *mat1, const double *mat2, size_t len) {
  while (len--) {
    if (*mat1 != *mat2) {
      return false;
    }
  }
  return true;
}

bool square_are_equal(const double (*mat1)[SIZE], const double (*mat2)[SIZE]) {
  /*
Fails to have intended effect since the function receives a SIZExSIZE object no
matter what is is passed.
clang-format off
  if ((SIZE*SIZE != (sizeof(mat1)/sizeof(double))) || (SIZE*SIZE !=
(sizeof(mat2)/sizeof(double)))) { return false;
  }
clang-format on
  */
  for (int i = 0; i < SIZE; i++) {
    for (int j = 0; j < SIZE; j++) {
      if (mat1[i][j] != mat2[i][j]) {
        return false;
      }
    }
  }
  return true;
}

#ifndef TESTING

/* det = 0*(4*8 - 4*14)  - 2*(6*8 - 6*10) + 2*(6*14 - 4*6) = 24 + 120 = 144
 */
int main(void) {
  const double test_matrix[SIZE][SIZE] = {
      {0.0, 2.0, 2.0}, {6.0, 4.0, 10.0}, {6.0, 14.0, 8.0}};

  assert(144.0 == determinant(test_matrix));
  exit(EXIT_SUCCESS);
}

#endif
