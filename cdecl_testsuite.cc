#include <fcntl.h>
#include <stdio.h>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include <string>

#define TESTING

#include "cdecl.c"

using namespace ::testing;

TEST(ProcessStringInputSuite, WellFormed) {
  char inputstr[MAXTOKENLEN];
  const std::string well_formed{"int x;"};
  EXPECT_THAT(find_input_string(well_formed.c_str(), inputstr, stdin),
              Eq(well_formed.size()));
}

TEST(ProcessStringInputSuite, Empty) {
  char inputstr[MAXTOKENLEN];
  const std::string empty{""};
  EXPECT_THAT(find_input_string(empty.c_str(), inputstr, stdin),
              Eq(empty.size()));
}

struct ProcessInputSuite : public Test {
  ProcessInputSuite()
      : ftemplate("/tmp/fake_stdin_path.XXXXXX"),
        fpath(mktemp(const_cast<char *>(ftemplate.c_str()))) {
    fake_stdin = fopen(fpath, std::string("w+").c_str());
  }
  ~ProcessInputSuite() override { fclose(fake_stdin); }
  void TearDown() override { ASSERT_THAT(unlink(fpath), Eq(0)); }
  void WriteStdin(const std::string &input) {
    ASSERT_THAT(fake_stdin, Ne(nullptr));
    // Without the newline, the code will seek past the end of the buffer.
    // On success, fread() and fwrite() return the  number  of  items  read  or
    // written.   This  number equals the number of bytes transferred only when
    // size is 1.
    size_t written = fwrite(input.c_str(), input.size(), 1, fake_stdin);
    ASSERT_THAT(written, Eq(1));
    // Otherwise stream may appear empty.
    ASSERT_THAT(fflush(fake_stdin), Eq(0));
    // Otherwise code will see EOF even though the buffer has content.
    rewind(fake_stdin);
  }
  std::string ftemplate;
  char *fpath;
  FILE *fake_stdin;
  char inputstr[MAXTOKENLEN];
};

// Make certain that the
TEST_F(ProcessInputSuite, WellFormedStdin0) {
  // Without the newline, the code will seek past the end of the buffer.
  const std::string well_formed("int x;\n");
  WriteStdin(well_formed.c_str());
  /* strlcpy()'s return value includes the trailing '\0'. */
  EXPECT_THAT(process_stdin(inputstr, fake_stdin), Eq(well_formed.size()));
}

TEST_F(ProcessInputSuite, WellFormedStdin1) {
  const char stdin_indicator[2] = {'-', 0};
  char inputstr[MAXTOKENLEN] = {0};
  const std::string well_formed("int x;\n");
  WriteStdin(well_formed.c_str());
  EXPECT_THAT(find_input_string(stdin_indicator, inputstr, fake_stdin),
              Eq(well_formed.size()));
}

TEST_F(ProcessInputSuite, EmptyStdin0) {
  const std::string empty_input(";\n");
  WriteStdin(empty_input.c_str());
  /* strlcpy()'s return value includes the trailing '\0'. */
  EXPECT_THAT(process_stdin(&inputstr[0], fake_stdin), Eq(empty_input.size()));
}

TEST_F(ProcessInputSuite, EmptyStdin1) {
  const char stdin_indicator[2] = {'-', 0};
  const std::string empty_input(";\n");
  WriteStdin(empty_input.c_str());
  EXPECT_THAT(find_input_string(stdin_indicator, inputstr, fake_stdin),
              Eq(empty_input.size()));
}

TEST_F(ProcessInputSuite, TooLongStdin) {
  std::string too_long("01234567890ABCDEFGHIJKMLNOPQRSTUVWYZabcedfghijklmonopqr"
                       "tsuvwyz0123456789;\n");
  WriteStdin(too_long.c_str());
  EXPECT_THAT(process_stdin(&inputstr[0], fake_stdin), Eq(0));
}

TEST(StringManipulateSuite, IsAllBlanks) {
  EXPECT_THAT(is_all_blanks(" "), IsTrue());
  EXPECT_THAT(is_all_blanks("a"), IsFalse());
  EXPECT_THAT(is_all_blanks(" a "), IsFalse());
  EXPECT_THAT(is_all_blanks(""), IsFalse());
  EXPECT_THAT(is_all_blanks("\0"), IsFalse());
}

TEST(StringManipulateSuite, TrimmedTrailingWhitepace) {
  char *trimmed = (char *)malloc(MAXTOKENLEN);
  EXPECT_THAT(trim_trailing_whitespace("a", trimmed), Eq(0));
  EXPECT_THAT(trim_trailing_whitespace(" b", trimmed), Eq(0));
  EXPECT_THAT(trim_trailing_whitespace("c    ", trimmed), Eq(4));
  EXPECT_THAT(std::string(trimmed), StrEq("c"));
  free(trimmed);
}

TEST(StringManipulateSuite, TrimmedLeadingWhitepace) {
  char *trimmed = (char *)malloc(MAXTOKENLEN);
  EXPECT_THAT(trim_leading_whitespace("a", trimmed), Eq(0));
  EXPECT_THAT(trim_leading_whitespace("c    ", trimmed), Eq(0));
  EXPECT_THAT(trim_leading_whitespace(" b", trimmed), Eq(1));
  EXPECT_THAT(std::string(trimmed), StrEq("b"));
  free(trimmed);
}

TEST(StringManipulateSuite, HasAlnumChars) {
  EXPECT_THAT(has_alnum_chars(""), IsFalse());
  EXPECT_THAT(has_alnum_chars("\0"), IsFalse());
  EXPECT_THAT(has_alnum_chars(";"), IsFalse());
  EXPECT_THAT(has_alnum_chars("\n"), IsFalse());
  EXPECT_THAT(has_alnum_chars("a"), IsTrue());
  EXPECT_THAT(has_alnum_chars("(a"), IsTrue());
}

TEST(StringManipulateSuite, GetKindBad) {
  EXPECT_THAT(get_kind(""), Eq(invalid));
  EXPECT_THAT(get_kind(";"), Eq(invalid));
  EXPECT_THAT(get_kind("@"), Eq(invalid));
  EXPECT_THAT(get_kind("\0"), Eq(invalid));
}

/* Tab is classified as invalid. */
TEST(StringManipulateSuite, GetKindWhitespace) {
  EXPECT_THAT(get_kind(" "), Eq(whitespace));
}

TEST(StringManipulateSuite, GetKindDelimiters) {
  EXPECT_THAT(get_kind("("), Eq(delimiter));
  EXPECT_THAT(get_kind(")"), Eq(delimiter));
  EXPECT_THAT(get_kind("["), Eq(delimiter));
  EXPECT_THAT(get_kind("{"), Eq(delimiter));
  EXPECT_THAT(get_kind("}"), Eq(delimiter));
  EXPECT_THAT(get_kind(","), Eq(delimiter));
  //  EXPECT_THAT(get_kind("())"), Eq(delimiter));
  char *trimmed = (char *)malloc(MAXTOKENLEN);
  EXPECT_THAT(trim_trailing_whitespace(", ", trimmed), IsTrue());
  EXPECT_THAT(get_kind(trimmed), Eq(delimiter));
  bzero(trimmed, strlen(trimmed));
  EXPECT_THAT(trim_leading_whitespace(" ,", trimmed), IsTrue());
  EXPECT_THAT(get_kind(trimmed), Eq(delimiter));
  free(trimmed);
}

TEST(StringManipulateSuite, GetKindQualifiers) {
  EXPECT_THAT(get_kind("const"), Eq(qualifier));
  EXPECT_THAT(get_kind("volatile"), Eq(qualifier));
  EXPECT_THAT(get_kind("static"), Eq(qualifier));
  EXPECT_THAT(get_kind("extern"), Eq(qualifier));
  EXPECT_THAT(get_kind("*"), Eq(qualifier));
  EXPECT_THAT(get_kind("unsigned"), Eq(qualifier));
}

TEST(StringManipulateSuite, GetKindTypes) {
  EXPECT_THAT(get_kind("char"), Eq(type));
  EXPECT_THAT(get_kind("short"), Eq(type));
  EXPECT_THAT(get_kind("int"), Eq(type));
  EXPECT_THAT(get_kind("float"), Eq(type));
  EXPECT_THAT(get_kind("double"), Eq(type));
  EXPECT_THAT(get_kind("long"), Eq(type));
  EXPECT_THAT(get_kind("struct"), Eq(type));
  EXPECT_THAT(get_kind("enum"), Eq(type));
  EXPECT_THAT(get_kind("union"), Eq(type));
  EXPECT_THAT(get_kind("void"), Eq(type));
  EXPECT_THAT(get_kind("int8_t"), Eq(type));
  EXPECT_THAT(get_kind("uint8_t"), Eq(type));
  EXPECT_THAT(get_kind("int16_t"), Eq(type));
  EXPECT_THAT(get_kind("uint16_t"), Eq(type));
  EXPECT_THAT(get_kind("int32_t"), Eq(type));
  EXPECT_THAT(get_kind("uint32_t"), Eq(type));
  EXPECT_THAT(get_kind("int64_t"), Eq(type));
  EXPECT_THAT(get_kind("uint64_t"), Eq(type));
}

TEST(StringManipulateSuite, GetKindIdentifiers) {
  EXPECT_THAT(get_kind(" myvar "), Eq(identifier));
  EXPECT_THAT(get_kind(" myvar\n"), Eq(identifier));
  EXPECT_THAT(get_kind(" myvar;"), Eq(identifier));
}

TEST(TokenizerSuite, Empty) {
  struct token this_token;
  char input[] = "";
  EXPECT_THAT(gettoken(input, &this_token), Eq(0));
  EXPECT_THAT(this_token.string, IsEmpty());
  EXPECT_THAT(this_token.kind, Eq(invalid));
}

TEST(TokenizerSuite, SimpleType) {
  struct token this_token;
  char input[] = "int";
  EXPECT_THAT(gettoken(input, &this_token), Eq(3));
  EXPECT_THAT(this_token.string, StrEq("int"));
  EXPECT_THAT(this_token.kind, Eq(type));
}

TEST(TokenizerSuite, IncludesPtr) {
  struct token this_token;
  char input[] = "int*";
  EXPECT_THAT(gettoken(input, &this_token), Eq(3));
  EXPECT_THAT(this_token.string, StrEq("int"));
  EXPECT_THAT(this_token.kind, Eq(type));
}

TEST(TokenizerSuite, SimpleQualifier) {
  struct token this_token;
  char input[] = "const int";
  EXPECT_THAT(gettoken(input, &this_token), Eq(5));
  EXPECT_THAT(this_token.string, StrEq("const"));
  EXPECT_THAT(this_token.kind, Eq(qualifier));
}

// Since the parser does not move past the initial space, it not included in the
// returned count.
TEST(TokenizerSuite, TrailingWhitespace) {
  struct token this_token;
  char input[] = "int    ";
  EXPECT_THAT(gettoken(input, &this_token), Eq(3));
  EXPECT_THAT(this_token.string, StrEq("int"));
  EXPECT_THAT(this_token.kind, Eq(type));
}

// Since the parser moved past the initial space, it is included in the returned
// offset value.
TEST(TokenizerSuite, LeadingWhitespace) {
  struct token this_token;
  char input[] = " int";
  EXPECT_THAT(gettoken(input, &this_token), Eq(4));
  EXPECT_THAT(this_token.string, StrEq("int"));
  EXPECT_THAT(this_token.kind, Eq(type));
}

TEST(TokenizerSuite, LeadingDelimiter) {
  struct token this_token;
  char input[] = " { ";
  EXPECT_THAT(gettoken(input, &this_token), Eq(2));
  EXPECT_THAT(this_token.string, StrEq("{"));
  EXPECT_THAT(this_token.kind, Eq(delimiter));
}

TEST(TokenizerSuite, LeadingDelimiter2) {
  struct token this_token;
  char input[] = " )";
  EXPECT_THAT(gettoken(input, &this_token), Eq(2));
  EXPECT_THAT(this_token.string, StrEq(")"));
  EXPECT_THAT(this_token.kind, Eq(delimiter));
}

TEST(StackTest, PushEmptyStack) {
  struct token stack[MAXTOKENS];
  stack[0].kind = invalid;
  strcpy(stack[0].string, "");
  struct token this_token{type, "int"};
  size_t tokennum = 1;
  push_stack(tokennum, &this_token, &stack[0]);
  EXPECT_THAT(stack[0].kind, Eq(invalid));
  EXPECT_THAT(stack[0].string, StrEq(""));
  EXPECT_THAT(stack[1].kind, Eq(type));
  EXPECT_THAT(stack[1].string, StrEq("int"));
}

TEST(StackTest, Push2ndElement) {
  struct token stack[MAXTOKENS];
  stack[0].kind = invalid;
  strcpy(stack[0].string, "");
  struct token this_token0{type, "int"};
  size_t tokennum = 1;
  push_stack(tokennum, &this_token0, &stack[0]);
  struct token this_token1{qualifier, "const"};
  tokennum = 2;
  push_stack(tokennum, &this_token1, &stack[0]);
  EXPECT_THAT(stack[0].kind, Eq(invalid));
  EXPECT_THAT(stack[0].string, StrEq(""));
  EXPECT_THAT(stack[1].kind, Eq(type));
  EXPECT_THAT(stack[1].string, StrEq("int"));
  EXPECT_THAT(stack[2].kind, Eq(qualifier));
  EXPECT_THAT(stack[2].string, StrEq("const"));
}

bool reset_stream_is_ok(FILE *stream) {
  if (fflush(stream) || fseek(stream, 0, SEEK_SET)) {
    return false;
  }
  if (feof(stream) || ferror(stream)) {
    return false;
  }
  return true;
}

struct ParserSuite : public Test {
  ParserSuite()
      : fotemplate("/tmp/fake_stdout_path.XXXXXX"),
        fetemplate("/tmp/fake_stderr_path.XXXXXX"),
        fopath(mktemp(const_cast<char *>(fotemplate.c_str()))),
        fepath(mktemp(const_cast<char *>(fetemplate.c_str()))) {
    // Opening with "r+" means that the file is not created if it doesn't exist.
    fake_stdout = fopen(fopath, std::string("w+").c_str());
    EXPECT_THAT(fake_stdout, Ne(nullptr));
    fake_stderr = fopen(fepath, std::string("w+").c_str());
    EXPECT_THAT(fake_stderr, Ne(nullptr));
    this_token.kind = invalid;
    bzero(this_token.string, MAXTOKENLEN);
  }

  ~ParserSuite() override {
    free(out_str);
    free(err_str);
    fclose(fake_stdout);
    fclose(fake_stderr);
  }
  void TearDown() override {
    ASSERT_THAT(unlink(fopath), Eq(0));
    ASSERT_THAT(unlink(fepath), Eq(0));
  }
  bool StdoutMatches(const std::string &expected) {
    // Cannot have an assertion in a googletest function which returns a value,
    // as then the function returns the wrong kind of value.
    // cdecl_testsuite.cc:216:5: error: void value not ignored as it ought to be
    //  216 |     ASSERT_THAT(fflush(fake_stdout), Gt(0));
    if (!reset_stream_is_ok(fake_stdout)) {
      return false;
    }
    // If the stream doesn't contain the requested number of bytes, fread()
    // reads nothing and returns 0.
    // std::size_t num_read = fread(output_str, MAXTOKENLEN -1 , 1,
    // fake_stdout);
    size_t buffer_size = 0;
    // Cannot call std::getline() since there is no way to turn FILE* into a C++
    // stream.
    ssize_t num_read = getline(&out_str, &buffer_size, fake_stdout);
    if (-1 == num_read) {
      std::cerr << "getline() failed: " << strerror(errno) << std::endl;
      return false;
    }

    return (std::string::npos != std::string(out_str).find(expected));
  }
  bool StderrMatches(const std::string &expected) {
    if (!reset_stream_is_ok(fake_stderr)) {
      return false;
    }
    // If the stream doesn't contain the requested number of bytes, fread()
    // reads nothing and returns 0.
    // std::size_t num_read = fread(output_str, MAXTOKENLEN -1 , 1,
    // fake_stdout);
    size_t buffer_size = 0;
    // Cannot call std::getline() since there is no way to turn FILE* into a C++
    // stream.
    ssize_t num_read = getline(&err_str, &buffer_size, fake_stderr);
    if (-1 == num_read) {
      std::cerr << "getline() failed: " << strerror(errno) << std::endl;
      return false;
    }
    return (std::string::npos != std::string(err_str).find(expected));
  }
  std::string fotemplate;
  std::string fetemplate;
  char *fopath;
  char *fepath;
  FILE *fake_stdout;
  FILE *fake_stderr;
  // The following causes a memory leak:
  // char* output_str = (char*)malloc(MAXTOKENLEN);
  //
  // The reason is that getline() changes the address of output_str.  One can
  // save the initial value of output_str and free() that, but memory which
  // glibc allocated will still leak:
  // ==2131242==ERROR: LeakSanitizer: detected memory leaks
  // Direct leak of 120 byte(s) in 1 object(s) allocated from:
  // #0 0x7f8587cf4c57 in malloc
  // ../../../../src/libsanitizer/asan/asan_malloc_linux.cpp:69 #1
  // 0x7f8586e896dd in __GI___getdelim libio/iogetdelim.c:65
  char *out_str;
  char *err_str;
  struct token this_token;
};

TEST_F(ParserSuite, PopEmpty) {
  struct token stack[MAXTOKENS];
  stack[0].kind = invalid;
  strcpy(stack[0].string, "");
  size_t tokennum = 0;
  struct token this_token;
  EXPECT_THAT(
      pop_stack(&tokennum, &this_token, &stack[0], fake_stdout, fake_stderr),
      Eq(-ENODATA));
  EXPECT_THAT(StderrMatches("Attempt to pop empty stack."), IsTrue());
}

TEST_F(ParserSuite, PopOne) {
  struct token stack[MAXTOKENS];
  stack[0].kind = invalid;
  strcpy(stack[0].string, "");
  struct token this_token0{type, "int"};
  size_t tokennum = 1;
  push_stack(tokennum, &this_token0, &stack[0]);

  struct token this_token;
  EXPECT_THAT(
      pop_stack(&tokennum, &this_token, &stack[0], fake_stdout, fake_stderr),
      Eq(0));
  EXPECT_THAT(StdoutMatches("int"), IsTrue());
}

TEST_F(ParserSuite, SimpleExpression) {
  char inputstr[] = "int x;";
  EXPECT_THAT(
      input_parsing_successful(inputstr, &this_token, fake_stdout, stderr),
      IsTrue());
  // The output has a trailng space in case there's output after the type.
  EXPECT_THAT(StdoutMatches("x is a(n) int "), IsTrue());
}
