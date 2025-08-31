#include <fcntl.h>
#include <stdio.h>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include <string>

#define TESTING

#include "cdecl.c"

TEST(SimpleParseTest, WellFormedStdin) {
  std::string ftemplate("/tmp/fake_stdin_path.XXXXXX");
  const char *fpath = mktemp(const_cast<char*>(ftemplate.c_str()));
  FILE* fake_stdin = fopen(fpath, std::string("w+").c_str());
  ASSERT_THAT(fake_stdin, ::testing::Ne(nullptr));
  // Without the newline, the code will seek past the end of the buffer.
  std::string well_formed("int x;\n");
  // On success, fread() and fwrite() return the  number  of  items  read  or
  // written.   This  number equals the number of bytes transferred only when
  // size is 1.
  size_t written = fwrite(well_formed.c_str(), well_formed.size(), 1, fake_stdin);
  ASSERT_THAT(written, ::testing::Eq(1));
  // Otherwise stream may appear empty.
  ASSERT_THAT(fflush(fake_stdin), ::testing::Eq(0));
  // Otherwise code will see EOF even though the buffer has content.
  rewind(fake_stdin);
  char inputstr[MAXTOKENLEN];
  // string.size() does not include newline.
  // &inputstr[0] because simple &inputstr is char (*)[64].
  EXPECT_THAT(process_stdin(&inputstr[0], fake_stdin), ::testing::Eq(well_formed.size() - 1));
  fclose(fake_stdin);
}
