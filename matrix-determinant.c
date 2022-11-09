/* Calculate the determinant of a random 3x3 matrix. */

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define SIZE 3

/* Declaration with SIZE results in "error: variably modified ‘test_matrix’ at
 * file scope" */
const double test_matrix[SIZE][SIZE] = {
    {0.0, 2.0, 2.0}, {6.0, 4.0, 10.0}, {6.0, 14.0, 8.0}};

/* Ended up not needing these two functions for the simple 3x3 determinant. */
int find_row_index(const double *elementp) {
  int offset = (elementp - &test_matrix[0][0]);
  return (offset / SIZE);
}

int find_column_index(const double *elementp) {
  int offset = (elementp - &test_matrix[0][0]);
  return (offset % SIZE);
}

/* Change the value of  what submatrix points to, not the submatrix pointer. */
void get_submatrix(double *submatrix, const int excluded_row,
                   const int excluded_column) {
  for (int i = 0; i < SIZE; i++) {
    for (int j = 0; j < SIZE; j++) {
      if ((excluded_row == i) || (excluded_column == j)) {
        continue;
      }
      *submatrix = test_matrix[i][j];
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
double determinant() {
  double sum = 0.0;
  for (int j = 0; j < SIZE; j++) {
    //  Flatten the 2x2 submatrix for convenience.
    double submatrix[4] = {0, 0, 0, 0};
    get_submatrix(submatrix, 0, j);
    sum += pow(-1.0, j) * test_matrix[0][j] * submatrix_determinant(submatrix);
  }
  return sum;
}

/* det = 0*(4*8 - 4*14)  - 2*(6*8 - 6*10) + 2*(6*14 - 4*6) = 24 + 120 = 144
 */
int main(void) {
  assert(144.0 == determinant());
  exit(EXIT_SUCCESS);
}
