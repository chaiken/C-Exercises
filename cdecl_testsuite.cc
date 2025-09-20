#include <fcntl.h>
#include <stdio.h>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include <string>

#define TESTING

#include "cdecl.c"

TEST(ProcessStringInputSuite, WellFormed) {
  char inputstr[MAXTOKENLEN];
  const std::string well_formed{"int x;"};
  EXPECT_THAT(find_input_string(well_formed.c_str(), inputstr,stdin),
              ::testing::Eq(well_formed.size()));
}

TEST(ProcessStringInputSuite, Empty) {
  char inputstr[MAXTOKENLEN];
  const std::string empty{""};
  EXPECT_THAT(find_input_string(empty.c_str(), inputstr,stdin),
              ::testing::Eq(empty.size()));
}

struct ProcessInputSuite : public ::testing::Test {
  ProcessInputSuite() : ftemplate("/tmp/fake_stdin_path.XXXXXX"),
                        fpath(mktemp(const_cast<char*>(ftemplate.c_str()))) {
    fake_stdin = fopen(fpath, std::string("w+").c_str());
  }
  ~ProcessInputSuite() override {
    fclose(fake_stdin);
  }
  void TearDown() override {
    ASSERT_THAT(unlink(fpath), ::testing::Eq(0));
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
TEST_F(ProcessInputSuite, WellFormedStdin0) {
  // Without the newline, the code will seek past the end of the buffer.
  const std::string well_formed("int x;\n");
  WriteStdin(well_formed.c_str());
  /* strlcpy()'s return value includes the trailing '\0'. */
  EXPECT_THAT(process_stdin(inputstr, fake_stdin), ::testing::Eq(well_formed.size()));
}

TEST_F(ProcessInputSuite, WellFormedStdin1) {
  const char stdin_indicator[2] = {'-', 0};
  char inputstr[MAXTOKENLEN] = {0};
  const std::string well_formed("int x;\n");
  WriteStdin(well_formed.c_str());
  EXPECT_THAT(  find_input_string(stdin_indicator, inputstr, fake_stdin),
                ::testing::Eq(well_formed.size()));
}

TEST_F(ProcessInputSuite, EmptyStdin0) {
  const std::string empty_input(";\n");
  WriteStdin(empty_input.c_str());
  /* strlcpy()'s return value includes the trailing '\0'. */
  EXPECT_THAT(process_stdin(&inputstr[0], fake_stdin), ::testing::Eq(empty_input.size()));
}

TEST_F(ProcessInputSuite, EmptyStdin1) {
  const char stdin_indicator[2] = {'-', 0};
  const std::string empty_input(";\n");
  WriteStdin(empty_input.c_str());
  EXPECT_THAT(find_input_string(stdin_indicator, inputstr, fake_stdin),
              ::testing::Eq(empty_input.size()));
}

TEST_F(ProcessInputSuite, TooLongStdin) {
  std::string
    too_long("01234567890ABCDEFGHIJKMLNOPQRSTUVWYZabcedfghijklmonopqrtsuvwyz0123456789;\n");
  WriteStdin(too_long.c_str());
  EXPECT_THAT(process_stdin(&inputstr[0], fake_stdin), ::testing::Eq(0));
}

TEST(StringManipulateSuite, IsAllBlanks) {
  EXPECT_THAT(is_all_blanks(" "), ::testing::IsTrue());
  EXPECT_THAT(is_all_blanks("a"), ::testing::IsFalse());
  EXPECT_THAT(is_all_blanks(" a "), ::testing::IsFalse());
  EXPECT_THAT(is_all_blanks(""), ::testing::IsFalse());
  EXPECT_THAT(is_all_blanks("\0"), ::testing::IsFalse());
}

TEST(StringManipulateSuite, TrimmedTrailingWhitepace) {
  char *trimmed = (char *) malloc(MAXTOKENLEN);
  EXPECT_THAT(trimmed_trailing_whitespace("a", trimmed),::testing::IsFalse());
  EXPECT_THAT(trimmed_trailing_whitespace(" b", trimmed), ::testing::IsFalse());
  EXPECT_THAT(trimmed_trailing_whitespace("c    ", trimmed), ::testing::IsTrue());
  EXPECT_THAT(std::string(trimmed), ::testing::StrEq("c"));
  free(trimmed);
}

TEST(StringManipulateSuite, TrimmedLeadingWhitepace) {
  char *trimmed = (char *) malloc(MAXTOKENLEN);
  EXPECT_THAT(trimmed_leading_whitespace("a", trimmed),::testing::IsFalse());
  EXPECT_THAT(trimmed_leading_whitespace("c    ", trimmed), ::testing::IsFalse());
  EXPECT_THAT(trimmed_leading_whitespace(" b", trimmed), ::testing::IsTrue());
  EXPECT_THAT(std::string(trimmed), ::testing::StrEq("b"));
  free(trimmed);
}

TEST(StringManipulateSuite, HasAlnumChars) {
  EXPECT_THAT(has_alnum_chars(""), ::testing::IsFalse());
  EXPECT_THAT(has_alnum_chars("\0"), ::testing::IsFalse());
  EXPECT_THAT(has_alnum_chars(";"), ::testing::IsFalse());
  EXPECT_THAT(has_alnum_chars("\n"), ::testing::IsFalse());
  EXPECT_THAT(has_alnum_chars("a"), ::testing::IsTrue());
  EXPECT_THAT(has_alnum_chars("(a"), ::testing::IsTrue());
}

TEST(StringManipulateSuite, GetKindBad) {
  EXPECT_THAT(get_kind(""), ::testing::Eq(invalid));
  EXPECT_THAT(get_kind(";"), ::testing::Eq(invalid));
  EXPECT_THAT(get_kind("@"), ::testing::Eq(invalid));
  EXPECT_THAT(get_kind("\0"), ::testing::Eq(invalid));
}

/* Tab is classified as invalid. */
TEST(StringManipulateSuite, GetKindWhitespace) {
  EXPECT_THAT(get_kind(" "), ::testing::Eq(whitespace));
}

TEST(StringManipulateSuite, GetKindDelimiters) {
  EXPECT_THAT(get_kind("("), ::testing::Eq(delimiter));
  EXPECT_THAT(get_kind(")"), ::testing::Eq(delimiter));
  EXPECT_THAT(get_kind("["), ::testing::Eq(delimiter));
  EXPECT_THAT(get_kind("{"), ::testing::Eq(delimiter));
  EXPECT_THAT(get_kind("}"), ::testing::Eq(delimiter));
  EXPECT_THAT(get_kind(","), ::testing::Eq(delimiter));
  //  EXPECT_THAT(get_kind("())"), ::testing::Eq(delimiter));
  char *trimmed = (char *) malloc(MAXTOKENLEN);
  EXPECT_THAT(trimmed_trailing_whitespace(", ", trimmed), ::testing::IsTrue());
  EXPECT_THAT(get_kind(trimmed), ::testing::Eq(delimiter));
  bzero(trimmed, strlen(trimmed));
  EXPECT_THAT(trimmed_leading_whitespace(" ,", trimmed), ::testing::IsTrue());
  EXPECT_THAT(get_kind(trimmed), ::testing::Eq(delimiter));
  free(trimmed);
}

TEST(StringManipulateSuite, GetKindQualifiers) {
  EXPECT_THAT(get_kind("const"), ::testing::Eq(qualifier));
  EXPECT_THAT(get_kind("volatile"), ::testing::Eq(qualifier));
  EXPECT_THAT(get_kind("static"), ::testing::Eq(qualifier));
  EXPECT_THAT(get_kind("extern"), ::testing::Eq(qualifier));
  EXPECT_THAT(get_kind("*"), ::testing::Eq(qualifier));
  EXPECT_THAT(get_kind("unsigned"), ::testing::Eq(qualifier));
}

TEST(StringManipulateSuite, GetKindTypes) {
  EXPECT_THAT(get_kind("char"), ::testing::Eq(type));
  EXPECT_THAT(get_kind("short"), ::testing::Eq(type));
  EXPECT_THAT(get_kind("int"), ::testing::Eq(type));
  EXPECT_THAT(get_kind("float"), ::testing::Eq(type));
  EXPECT_THAT(get_kind("double"), ::testing::Eq(type));
  EXPECT_THAT(get_kind("long"), ::testing::Eq(type));
  EXPECT_THAT(get_kind("struct"), ::testing::Eq(type));
  EXPECT_THAT(get_kind("enum"), ::testing::Eq(type));
  EXPECT_THAT(get_kind("union"), ::testing::Eq(type));
  EXPECT_THAT(get_kind("void"), ::testing::Eq(type));
  EXPECT_THAT(get_kind("int8_t"), ::testing::Eq(type));
  EXPECT_THAT(get_kind("uint8_t"), ::testing::Eq(type));
  EXPECT_THAT(get_kind("int16_t"), ::testing::Eq(type));
  EXPECT_THAT(get_kind("uint16_t"), ::testing::Eq(type));
  EXPECT_THAT(get_kind("int32_t"), ::testing::Eq(type));
  EXPECT_THAT(get_kind("uint32_t"), ::testing::Eq(type));
  EXPECT_THAT(get_kind("int64_t"), ::testing::Eq(type));
  EXPECT_THAT(get_kind("uint64_t"), ::testing::Eq(type));
}

TEST(StringManipulateSuite, GetKindIdentifiers) {
  EXPECT_THAT(get_kind(" myvar "), ::testing::Eq(identifier));
  EXPECT_THAT(get_kind(" myvar\n"), ::testing::Eq(identifier));
  EXPECT_THAT(get_kind(" myvar;"), ::testing::Eq(identifier));
}
