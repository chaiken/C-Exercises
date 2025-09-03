#include <fcntl.h>
#include <stdio.h>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include <string>

#define TESTING

#include "cdecl.c"

struct ProcessInputSuite : public ::testing::Test {
  ProcessInputSuite() : ftemplate("/tmp/fake_stdin_path.XXXXXX"), fpath(mktemp(const_cast<char*>(ftemplate.c_str()))) {
    fake_stdin = fopen(fpath, std::string("w+").c_str());
  }
  ~ProcessInputSuite() override {
    fclose(fake_stdin);
  }
  void WriteStdin(const std::string& input) {
    ASSERT_THAT(fake_stdin, ::testing::Ne(nullptr));
    // Without the newline, the code will seek past the end of the buffer.
    // On success, fread() and fwrite() return the  number  of  items  read  or
    // written.   This  number equals the number of bytes transferred only when
    // size is 1.
    size_t written = fwrite(input.c_str(), input.size(), 1, fake_stdin);
    ASSERT_THAT(written, ::testing::Eq(1));
    // Otherwise stream may appear empty.
    ASSERT_THAT(fflush(fake_stdin), ::testing::Eq(0));
    // Otherwise code will see EOF even though the buffer has content.
    rewind(fake_stdin);
  }
  std::string ftemplate;
  char *fpath;
  FILE* fake_stdin;
  char inputstr[MAXTOKENLEN];
};

// Make certain that the
TEST_F(ProcessInputSuite, WellFormedStdin) {
  // Without the newline, the code will seek past the end of the buffer.
  std::string well_formed("int x;\n");
  WriteStdin(well_formed.c_str());
  // Otherwise stream may appear empty.
  ASSERT_THAT(fflush(fake_stdin), ::testing::Eq(0));
  // Otherwise code will see EOF even though the buffer has content.
  rewind(fake_stdin);
  // string.size() does not include newline.
  // &inputstr[0] because simple &inputstr is char (*)[64].
  EXPECT_THAT(process_stdin(&inputstr[0], fake_stdin), ::testing::Eq(well_formed.size() - 1));
}

TEST_F(ProcessInputSuite, EmptyStdin) {
  std::string empty_input("\n;");
  WriteStdin(empty_input.c_str());
  ASSERT_THAT(fflush(fake_stdin), ::testing::Eq(0));
  // Otherwise code will see EOF even though the buffer has content.
  rewind(fake_stdin);
  // string.size() does not include newline.
  // &inputstr[0] because simple &inputstr is char (*)[64].
  EXPECT_THAT(process_stdin(&inputstr[0], fake_stdin), ::testing::Eq(0));
}
