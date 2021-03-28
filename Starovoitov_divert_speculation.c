#include <stdio.h>
#include <stdlib.h>

union access_bytes {
  long int idx;
  int fourbytes[2];
  unsigned char eightbytes[8];
};

void pretty_print(union access_bytes val) {
  printf("\nlong int: %ld\n", val.idx);
  printf("int: %d %d\n", val.fourbytes[0], val.fourbytes[1]);
  printf("bytes: ");
  for (int i = 0; i < 8; i++) {
    printf("0x%x ", val.eightbytes[i]);
  }
  printf("\n");
}

void test_val(int val) {
  union access_bytes anint, bint, cint, dint;
  anint.fourbytes[0] = val;
  if (val < 0) {
    anint.fourbytes[1] = val;
  } else {
    anint.fourbytes[1] = 0;
  }

  pretty_print(anint);

  printf("b: ");
  bint.idx = (anint.idx | (anint.idx - 1 - 256));
  pretty_print(bint);

  printf("c: ");
  cint.idx = ~(long)(anint.idx | (anint.idx - 1 - 256));
  pretty_print(cint);

  printf("d: ");
  dint.idx = (~(long)(anint.idx | (anint.idx - 1 - 256))) >> 63;
  pretty_print(dint);
}

int main() {
  test_val(256);
  printf("\n");
  test_val(-1);
  printf("\n");
  test_val(257);
  exit(EXIT_SUCCESS);
}
