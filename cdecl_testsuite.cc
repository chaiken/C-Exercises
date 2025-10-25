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
  EXPECT_THAT(inputstr, StrEq("int x;"));
}

TEST(ProcessStringInputSuite, Empty) {
  char inputstr[MAXTOKENLEN];
  const std::string empty{""};
  EXPECT_THAT(find_input_string(empty.c_str(), inputstr, stdin),
              Eq(empty.size()));
  EXPECT_THAT(strlen(inputstr), Eq(0));
}

TEST(ProcesStringInputSuite, LeadingDash) {
  char inputstr[MAXTOKENLEN];
  const std::string dashes{"--;"};
  EXPECT_THAT(find_input_string(dashes.c_str(), inputstr, stdin), Eq(3));
  EXPECT_THAT(inputstr, StrEq("--;"));
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
  EXPECT_THAT(trim_trailing_whitespace("    ", trimmed), Eq(4));
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

TEST(StringManipulateSuite, GetArrayLength) {
  EXPECT_THAT(get_kind("42"), Eq(length));
}

TEST(StringManipulateSuite, GetKindIdentifiers) {
  EXPECT_THAT(get_kind(" myvar "), Eq(identifier));
  EXPECT_THAT(get_kind(" myvar\n"), Eq(identifier));
  EXPECT_THAT(get_kind(" myvar;"), Eq(identifier));
}

struct TokenizerSuite : public Test {
  TokenizerSuite() { initialize_parser(&parser); }
  struct token this_token;
  struct parser_props parser;
};

TEST_F(TokenizerSuite, Empty) {
  char input[] = "";
  EXPECT_THAT(gettoken(&parser, input, &this_token), Eq(0));
  EXPECT_THAT(this_token.string, IsEmpty());
  EXPECT_THAT(this_token.kind, Eq(invalid));
  EXPECT_THAT(parser.have_identifier, IsFalse());
  EXPECT_THAT(parser.have_type, IsFalse());
  EXPECT_THAT(parser.is_array, IsFalse());
}

TEST_F(TokenizerSuite, SimpleType) {
  char input[] = "int";
  // There is only one token, so it is strlen(input) long.
  EXPECT_THAT(gettoken(&parser, input, &this_token), Eq(strlen(input)));
  EXPECT_THAT(this_token.string, StrEq("int"));
  EXPECT_THAT(this_token.kind, Eq(type));
  EXPECT_THAT(parser.have_type, IsTrue());
}

TEST_F(TokenizerSuite, IncludesPtr) {
  char input[] = "int*";
  // The first token is 3 chars long.
  EXPECT_THAT(gettoken(&parser, input, &this_token), Eq(3));
  EXPECT_THAT(this_token.string, StrEq("int"));
  EXPECT_THAT(this_token.kind, Eq(type));
  EXPECT_THAT(parser.have_type, IsTrue());
}

TEST_F(TokenizerSuite, SimpleQualifier) {
  char input[] = "const int";
  EXPECT_THAT(gettoken(&parser, input, &this_token), Eq(5));
  EXPECT_THAT(this_token.string, StrEq("const"));
  EXPECT_THAT(this_token.kind, Eq(qualifier));
  EXPECT_THAT(parser.have_identifier, IsFalse());
}

// Since the parser does not move past the initial space, it not included in the
// returned count.
TEST_F(TokenizerSuite, TrailingWhitespace) {
  char input[] = "int    ";
  EXPECT_THAT(gettoken(&parser, input, &this_token), Eq(3));
  EXPECT_THAT(this_token.string, StrEq("int"));
  EXPECT_THAT(this_token.kind, Eq(type));
  EXPECT_THAT(parser.have_type, IsTrue());
}

// Since the parser moved past the initial space, it is included in the returned
// offset value.
TEST_F(TokenizerSuite, LeadingWhitespace) {
  char input[] = " int";
  EXPECT_THAT(gettoken(&parser, input, &this_token), Eq(4));
  EXPECT_THAT(this_token.string, StrEq("int"));
  EXPECT_THAT(this_token.kind, Eq(type));
  EXPECT_THAT(parser.have_type, IsTrue());
}

TEST_F(TokenizerSuite, IsArray) {
  char input[] = "val[]";
  // Array is detected, but the brackets are not part of the token.
  EXPECT_THAT(gettoken(&parser, input, &this_token), Eq(3));
  EXPECT_THAT(this_token.string, StrEq("val"));
  EXPECT_THAT(this_token.kind, Eq(identifier));
  EXPECT_THAT(parser.is_array, IsTrue());
}

TEST_F(TokenizerSuite, IsArrayWithLength) {
  char input[] = "val[42]";
  // Array is detected, but the brackets are not part of the token.
  EXPECT_THAT(gettoken(&parser, input, &this_token), Eq(3));
  EXPECT_THAT(this_token.string, StrEq("val"));
  EXPECT_THAT(this_token.kind, Eq(identifier));
  EXPECT_THAT(parser.is_array, IsTrue());
}

TEST_F(TokenizerSuite, IsOnlyArrayLength) {
  // load_stack() skips over leading '['.
  char input[] = "42]";
  parser.is_array = true;
  // Parsing stops at last numeric character.
  EXPECT_THAT(gettoken(&parser, input, &this_token), Eq(2));
  EXPECT_THAT(this_token.string, StrEq("42"));
  EXPECT_THAT(this_token.kind, Eq(length));
}

TEST_F(TokenizerSuite, HasDash) {
  char input[] = "first-val";
  EXPECT_THAT(gettoken(&parser, input, &this_token), Eq(strlen(input)));
  EXPECT_THAT(this_token.string, StrEq("first-val"));
  EXPECT_THAT(this_token.kind, Eq(identifier));
}

TEST_F(TokenizerSuite, HasUnderscore) {
  char input[] = "first_val";
  EXPECT_THAT(gettoken(&parser, input, &this_token), Eq(strlen(input)));
  EXPECT_THAT(this_token.string, StrEq("first_val"));
  EXPECT_THAT(this_token.kind, Eq(identifier));
}

// Unallowed characters and anything following them are simply cut off. */
TEST_F(TokenizerSuite, IgnoreUnallowedChars) {
  char input[] = "f3asdf";
  EXPECT_THAT(gettoken(&parser, input, &this_token), Eq(1));
  EXPECT_THAT(this_token.string, StrEq("f"));
  EXPECT_THAT(this_token.kind, Eq(identifier));
}

TEST_F(TokenizerSuite, ElideTrailingDash) {
  char input[] = "val-";
  EXPECT_THAT(gettoken(&parser, input, &this_token), Eq(strlen(input)));
  EXPECT_THAT(this_token.string, StrEq("val"));
  EXPECT_THAT(this_token.kind, Eq(identifier));
}

TEST_F(TokenizerSuite, ElideLeadingDash) {
  char input[] = "--val";
  EXPECT_THAT(gettoken(&parser, input, &this_token), Eq(strlen(input)));
  EXPECT_THAT(this_token.string, StrEq("val"));
  EXPECT_THAT(this_token.kind, Eq(identifier));
}

TEST_F(TokenizerSuite, DoNotElideLeadingUnderscore) {
  char input[] = "__val";
  EXPECT_THAT(gettoken(&parser, input, &this_token), Eq(strlen(input)));
  EXPECT_THAT(this_token.string, StrEq("__val"));
  EXPECT_THAT(this_token.kind, Eq(identifier));
}

TEST_F(TokenizerSuite, PushEmptyStack) {
  struct token token0{type, "int"};
  EXPECT_THAT(parser.stacklen, Eq(0));
  push_stack(&parser, &token0, stderr);
  EXPECT_THAT(parser.stack[0].kind, Eq(type));
  EXPECT_THAT(parser.stack[0].string, StrEq("int"));
  EXPECT_THAT(parser.stacklen, Eq(1));
}

TEST_F(TokenizerSuite, Push2ndElement) {
  EXPECT_THAT(parser.stacklen, Eq(0));
  struct token token0{type, "int"};
  push_stack(&parser, &token0, stderr);
  struct token token1{qualifier, "const"};
  push_stack(&parser, &token1, stderr);
  EXPECT_THAT(parser.stack[0].kind, Eq(type));
  EXPECT_THAT(parser.stack[0].string, StrEq("int"));
  EXPECT_THAT(parser.stack[1].kind, Eq(qualifier));
  EXPECT_THAT(parser.stack[1].string, StrEq("const"));
  EXPECT_THAT(parser.stacklen, Eq(2));
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
    initialize_parser(&parser);
  }

  ~ParserSuite() override {
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
    std::vector<std::string> stack_contents{};
    while (getline(&out_str, &buffer_size, fake_stdout) > 0) {
      stack_contents.push_back(std::string{out_str});
    }
    if (stack_contents.empty()) {
      std::cerr << "Unable to read stack: " << strerror(errno) << std::endl;
      return false;
    }
    for (const std::string &printed : stack_contents) {
      if (std::string::npos != printed.find(expected)) {
        free(out_str);
        return true;
      }
    }
    free(out_str);
    return false;
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
    std::vector<std::string> stack_contents{};
    while (getline(&err_str, &buffer_size, fake_stderr) > 0) {
      stack_contents.push_back(std::string{err_str});
    }
    if (stack_contents.empty()) {
      std::cerr << "Unable to read stack: " << strerror(errno) << std::endl;
      return false;
    }
    for (const std::string &printed : stack_contents) {
      if (std::string::npos != printed.find(expected)) {
        free(err_str);
        return true;
      }
    }
    free(err_str);
    return false;
  }
  struct parser_props parser;
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

TEST_F(ParserSuite, Truncation) {
  char *token = (char *)malloc(MAXTOKENLEN);

  strlcpy(token, "int x;", MAXTOKENLEN);
  EXPECT_THAT(truncate_input(&token, fake_stderr), IsTrue());
  // Checking strlen() assures that the result is NULL-terminated.
  EXPECT_THAT(strlen(token), Eq(5));
  EXPECT_THAT(token, StrEq("int x"));

  bzero(token, MAXTOKENLEN);
  strlcpy(token, "int x   ;", MAXTOKENLEN);
  EXPECT_THAT(truncate_input(&token, fake_stderr), IsTrue());
  EXPECT_THAT(strlen(token), Eq(5));
  EXPECT_THAT(token, StrEq("int x"));

  bzero(token, MAXTOKENLEN);
  strlcpy(token, "int x = 2;", MAXTOKENLEN);
  EXPECT_THAT(truncate_input(&token, fake_stderr), IsTrue());
  EXPECT_THAT(strlen(token), Eq(5));
  EXPECT_THAT(token, StrEq("int x"));

  bzero(token, MAXTOKENLEN);
  strlcpy(token, "const int x;", MAXTOKENLEN);
  EXPECT_THAT(truncate_input(&token, fake_stderr), IsTrue());
  EXPECT_THAT(strlen(token), Eq(11));
  EXPECT_THAT(token, StrEq("const int x"));

  bzero(token, MAXTOKENLEN);
  strlcpy(token, "int x", MAXTOKENLEN);
  EXPECT_THAT(truncate_input(&token, fake_stderr), IsFalse());
  EXPECT_THAT(StderrMatches("Improperly terminated declaration."), IsTrue());

  bzero(token, MAXTOKENLEN);
  strlcpy(token, ";int x", MAXTOKENLEN);
  EXPECT_THAT(truncate_input(&token, fake_stderr), IsFalse());
  EXPECT_THAT(StderrMatches("Zero-length input string."), IsTrue());

  bzero(token, MAXTOKENLEN);
  strlcpy(token, "   = ", MAXTOKENLEN);
  EXPECT_THAT(truncate_input(&token, fake_stderr), IsFalse());
  EXPECT_THAT(StderrMatches("Zero-length input string."), IsTrue());

  free(token);
}

TEST_F(ParserSuite, PopEmpty) {
  EXPECT_THAT(pop_stack(&parser, fake_stdout, fake_stderr), Eq(-ENODATA));
  EXPECT_THAT(StderrMatches("Attempt to pop empty stack."), IsTrue());
}

TEST_F(ParserSuite, PopOne) {
  struct token token0{type, "int"};
  push_stack(&parser, &token0, fake_stderr);

  EXPECT_THAT(pop_stack(&parser, fake_stdout, fake_stderr), Eq(0));
  EXPECT_THAT(StdoutMatches("int"), IsTrue());
}

TEST_F(ParserSuite, Showstack) {
  struct token token0{type, "int"};
  push_stack(&parser, &token0, fake_stderr);
  struct token token1{qualifier, "const"};
  push_stack(&parser, &token1, fake_stderr);
  showstack(&parser.stack[0], parser.stacklen, fake_stdout);
  EXPECT_THAT(StdoutMatches("Stack is:"), IsTrue());
  EXPECT_THAT(StdoutMatches("Token number 0 has kind 2 and string int"),
              IsTrue());
  EXPECT_THAT(StdoutMatches("Token number 1 has kind 3 and string const"),
              IsTrue());
  showstack(&parser.stack[0], parser.stacklen, stdout);
}

TEST_F(ParserSuite, LoadStackWorks) {
  char nexttoken[MAXTOKENLEN];
  const char *probe = "const int* x;";
  strlcpy(nexttoken, probe, strlen(probe) + 1);
  std::size_t consumed =
      load_stack(&parser, nexttoken, fake_stdout, fake_stderr);
  // consumed = strlen()-1 since the trailing ';' is elided before gettoken()
  // processing begins.
  EXPECT_THAT(consumed, Eq(strlen(probe) - 1));
  EXPECT_THAT(StdoutMatches("Token number 0 has kind 2 and string int"),
              IsTrue());
  EXPECT_THAT(StdoutMatches("Token number 1 has kind 3 and string const"),
              IsTrue());
  EXPECT_THAT(StdoutMatches("Token number 2 has kind 3 and string *"),
              IsTrue());
  EXPECT_THAT(StdoutMatches("Token number 3 has kind 4 and string x"),
              IsTrue());
  showstack(&parser.stack[0], parser.stacklen, stdout);
}

TEST_F(ParserSuite, LoadStackEqualsTerminator) {
  struct parser_props parser;
  char nexttoken[MAXTOKENLEN];
  const char *probe = "static double val = 2;";
  strlcpy(nexttoken, probe, strlen(probe) + 1);
  std::size_t consumed =
      load_stack(&parser, nexttoken, fake_stdout, fake_stderr);
  EXPECT_THAT(consumed, Eq(strlen(probe) - strlen(" = 2;")));
  EXPECT_THAT(StdoutMatches("Token number 0 has kind 2 and string double"),
              IsTrue());
  EXPECT_THAT(StdoutMatches("Token number 1 has kind 3 and string static"),
              IsTrue());
  EXPECT_THAT(StdoutMatches("Token number 2 has kind 4 and string val"),
              IsTrue());
  showstack(&parser.stack[0], parser.stacklen, stdout);
}

TEST_F(ParserSuite, LoadStackArrayLength) {
  struct parser_props parser;
  char nexttoken[MAXTOKENLEN];
  const char *probe = "double val[42];";
  strlcpy(nexttoken, probe, strlen(probe) + 1);
  std::size_t consumed =
      load_stack(&parser, nexttoken, fake_stdout, fake_stderr);
  // In load_stack(), '[' is skipped, and "];" is not consumed. */
  EXPECT_THAT(consumed, Eq(strlen(probe) - strlen("[];")));
  EXPECT_THAT(StdoutMatches("Token number 0 has kind 2 and string double"),
              IsTrue());
  EXPECT_THAT(StdoutMatches("Token number 1 has kind 5 and string 42"),
              IsTrue());
  EXPECT_THAT(StdoutMatches("Token number 2 has kind 4 and string val"),
              IsTrue());
  showstack(&parser.stack[0], parser.stacklen, stdout);
}

TEST_F(ParserSuite, LoadStackBadArray) {
  struct parser_props parser;
  char nexttoken[MAXTOKENLEN];
  const char *probe = "double val[42;";
  strlcpy(nexttoken, probe, strlen(probe) + 1);
  std::size_t consumed =
      load_stack(&parser, nexttoken, fake_stdout, fake_stderr);
  EXPECT_THAT(consumed, Eq(0));
}

TEST_F(ParserSuite, NothingToLoad) {
  char nexttoken[MAXTOKENLEN];
  const char *probe = "=;";
  strlcpy(nexttoken, probe, strlen(probe) + 1);
  std::size_t consumed =
      load_stack(&parser, nexttoken, fake_stdout, fake_stderr);
  EXPECT_THAT(consumed, Eq(0));
  EXPECT_THAT(StderrMatches("Zero-length input string."), IsTrue());
}

TEST_F(ParserSuite, LotsOfWhitespace) {
  char nexttoken[MAXTOKENLEN];
  const char *probe = "     ;";
  strlcpy(nexttoken, probe, strlen(probe) + 1);
  std::size_t consumed =
      load_stack(&parser, nexttoken, fake_stdout, fake_stderr);
  EXPECT_THAT(consumed, Eq(0));
  EXPECT_THAT(StderrMatches("Zero-length input string."), IsTrue());
}

TEST_F(ParserSuite, SimpleExpression) {
  char inputstr[] = "int x;";
  EXPECT_THAT(input_parsing_successful(inputstr, fake_stdout, fake_stderr),
              IsTrue());
  // The output has a trailng space in case there's output after the type.
  EXPECT_THAT(StdoutMatches("x is a(n) int "), IsTrue());
}

TEST_F(ParserSuite, PtrExpression) {
  char inputstr[] = "int* x;";
  EXPECT_THAT(input_parsing_successful(inputstr, fake_stdout, fake_stderr),
              IsTrue());
  // The output has a trailng space in case there's output after the type.
  EXPECT_THAT(StdoutMatches("x is a(n) pointer(s) to int "), IsTrue());
}

TEST_F(ParserSuite, QualfiedExpression) {
  char inputstr[] = "const int x;";
  EXPECT_THAT(input_parsing_successful(inputstr, fake_stdout, fake_stderr),
              IsTrue());
  // The output has a trailng space in case there's output after the type.
  EXPECT_THAT(StdoutMatches("x is a(n) const int "), IsTrue());
}

TEST_F(ParserSuite, ConstPtr) {
  char inputstr[] = "int * const x;";
  EXPECT_THAT(input_parsing_successful(inputstr, fake_stdout, fake_stderr),
              IsTrue());
  EXPECT_THAT(StdoutMatches("x is a(n) const pointer(s) to int "), IsTrue());
}

TEST_F(ParserSuite, SimpleArray) {
  char inputstr[] = "const double x[]];";
  EXPECT_THAT(input_parsing_successful(inputstr, fake_stdout, fake_stderr),
              IsTrue());
  EXPECT_THAT(StdoutMatches("x is an array of const double "), IsTrue());
}

TEST_F(ParserSuite, PtrArray) {
  char inputstr[] = "double* x[]];";
  EXPECT_THAT(input_parsing_successful(inputstr, fake_stdout, fake_stderr),
              IsTrue());
  EXPECT_THAT(StdoutMatches("x is an array of pointer(s) to double "),
              IsTrue());
}

TEST_F(ParserSuite, ArrayWithLength) {
  char inputstr[] = "char val[9];";
  EXPECT_THAT(input_parsing_successful(inputstr, fake_stdout, fake_stderr),
              IsTrue());
  EXPECT_THAT(StdoutMatches("val is an array of 9 char"), IsTrue());
}

TEST_F(ParserSuite, ArrayWithBadLength) {
  char inputstr[] = "char val[9;";
  EXPECT_THAT(input_parsing_successful(inputstr, fake_stdout, fake_stderr),
              IsFalse());
  EXPECT_THAT(StderrMatches("Input lacks required identifier or type element"),
              IsTrue());
}

TEST_F(ParserSuite, Reorder) {
  char nexttoken[MAXTOKENLEN];
  const char *probe = "const int x;";
  strlcpy(nexttoken, probe, strlen(probe) + 1);
  std::size_t consumed =
      load_stack(&parser, nexttoken, fake_stdout, fake_stderr);
  // consumed = strlen()-1 since the trailing ';' is elided before gettoken()
  // processing begins.
  EXPECT_THAT(consumed, Eq(strlen(probe) - 1));
  EXPECT_THAT(StdoutMatches("Token number 0 has kind 2 and string int"),
              IsTrue());
  EXPECT_THAT(StdoutMatches("Token number 1 has kind 3 and string const"),
              IsTrue());
  EXPECT_THAT(StdoutMatches("Token number 2 has kind 4 and string x"),
              IsTrue());
}
