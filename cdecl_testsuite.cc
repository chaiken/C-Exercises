#include <fcntl.h>
#include <stdio.h>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include <iostream>
#include <string>

#define DEBUG
#define TESTING

#include "cdecl.c"

using namespace ::testing;

void set_test_streams(struct parser_props *parser, FILE *fake_stdout,
                      FILE *fake_stderr) {
  parser->out_stream = fake_stdout;
  parser->err_stream = fake_stderr;
}

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

struct ProcessInputSuite : public Test {
  ProcessInputSuite() : fake_stdin(tmpfile()) {}
  ~ProcessInputSuite() override { fclose(fake_stdin); }
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
  // clang-format off
  std::string too_long("01234567890ABCDEFGHIJKMLNOPQRSTUVWYZabcedfghijklmonopqrtsuvwyz0123456789tsuvwyz0123456789tsuvwyz0123456789tsuvwyz01234567890123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ;\n");
  // clang-format on
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
  EXPECT_THAT(trim_trailing_whitespace("\0", trimmed), Eq(0));
  EXPECT_THAT(trim_trailing_whitespace("    ", trimmed), Eq(4));
  EXPECT_THAT(trim_trailing_whitespace("c    ", trimmed), Eq(4));
  EXPECT_THAT(std::string(trimmed), StrEq("c"));
  free(trimmed);
}

TEST(StringManipulateSuite, TrimmedLeadingWhitepace) {
  char *trimmed = (char *)malloc(MAXTOKENLEN);
  EXPECT_THAT(trim_leading_whitespace("a", trimmed), Eq(0));
  EXPECT_THAT(trim_leading_whitespace("c    ", trimmed), Eq(0));
  EXPECT_THAT(std::string(trimmed), StrEq(""));
  EXPECT_THAT(trim_leading_whitespace(" ", trimmed), Eq(1));
  EXPECT_THAT(trim_leading_whitespace("\0", trimmed), Eq(0));
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

TEST(StringManipulateSuite, GetKindQualifiers) {
  EXPECT_THAT(get_kind("const"), Eq(qualifier));
  EXPECT_THAT(get_kind("volatile"), Eq(qualifier));
  EXPECT_THAT(get_kind("static"), Eq(qualifier));
  EXPECT_THAT(get_kind("*"), Eq(qualifier));
  EXPECT_THAT(get_kind("extern"), Eq(qualifier));
  EXPECT_THAT(get_kind("unsigned"), Eq(qualifier));
  EXPECT_THAT(get_kind("restrict"), Eq(qualifier));
  EXPECT_THAT(get_kind("atomic"), Eq(qualifier));
}

struct KindCheckerTest : public TestWithParam<std::string> {};

const std::size_t typenum = sizeof(types) / sizeof(char *);
std::vector<std::string> all_types;
std::vector<std::string> make_all_types() {
  for (std::size_t i = 0; i < typenum; i++) {
    all_types.push_back(types[i]);
  }
  return all_types;
}

TEST_P(KindCheckerTest, GetKindTypes) {
  std::string input = GetParam();
  EXPECT_THAT(get_kind(input.c_str()), Eq(type));
}

INSTANTIATE_TEST_CASE_P(ProvideTypes, KindCheckerTest,
                        testing::ValuesIn(make_all_types()));

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
  EXPECT_THAT(parser.array_dimensions, Eq(0));
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
  parser.have_type = true;
  // Array is detected, but the brackets are not part of the token.
  EXPECT_THAT(gettoken(&parser, input, &this_token), Eq(3));
  EXPECT_THAT(this_token.string, StrEq("val"));
  EXPECT_THAT(this_token.kind, Eq(identifier));
  EXPECT_THAT(parser.array_dimensions, Eq(1));
}

TEST_F(TokenizerSuite, IsArrayWithLength) {
  char input[] = "val[42]";
  parser.have_type = true;
  // Array is detected, but the brackets are not part of the token.
  EXPECT_THAT(gettoken(&parser, input, &this_token), Eq(3));
  EXPECT_THAT(this_token.string, StrEq("val"));
  EXPECT_THAT(this_token.kind, Eq(identifier));
  EXPECT_THAT(parser.array_dimensions, Eq(1));
}

TEST_F(TokenizerSuite, IsOnlyArrayLength) {
  // load_stack() skips over leading '['.
  char input[] = "5555]";
  parser.array_dimensions = 1;
  parser.have_identifier = true;
  parser.have_type = true;
  // Parsing stops at last numeric character.
  EXPECT_THAT(gettoken(&parser, input, &this_token), Eq(strlen(input) - 1));
  EXPECT_THAT(this_token.string, StrEq("5555"));
  EXPECT_THAT(this_token.kind, Eq(length));
}

TEST_F(TokenizerSuite, HasUnderscore) {
  parser.have_type = true;
  char input[] = "first_val";
  EXPECT_THAT(gettoken(&parser, input, &this_token), Eq(9));
  EXPECT_THAT(this_token.string, StrEq("first_val"));
  EXPECT_THAT(this_token.kind, Eq(identifier));
}

TEST_F(TokenizerSuite, IgnoreUnallowedLeadingCharsNoType) {
  parser.have_type = true;
  char input[] = "5fasdf";
  EXPECT_THAT(gettoken(&parser, input, &this_token), Eq(0));
}

TEST_F(TokenizerSuite, IgnoreAllowedFollowingCharsNoType) {
  char input[] = "has_32bit_inodes";
  parser.have_type = true;
  EXPECT_THAT(gettoken(&parser, input, &this_token), Eq(strlen(input)));
  EXPECT_THAT(this_token.string, StrEq(input));
  EXPECT_THAT(this_token.kind, Eq(identifier));
}

TEST_F(TokenizerSuite, IgnoreUnallowedCharsStdintNoType) {
  char input[] = "uint32_t";
  EXPECT_THAT(gettoken(&parser, input, &this_token), strlen(input));
  EXPECT_THAT(this_token.string, StrEq(input));
  EXPECT_THAT(this_token.kind, Eq(type));
}

TEST_F(TokenizerSuite, IgnoreUnallowedCharsHasType) {
  parser.have_type = true;
  char input[] = "f&asdf";
  EXPECT_THAT(gettoken(&parser, input, &this_token), Eq(1));
  EXPECT_THAT(this_token.string, StrEq("f"));
  EXPECT_THAT(this_token.kind, Eq(identifier));
}

TEST_F(TokenizerSuite, IgnoreUnallowedCharsHasTypeBadFirst) {
  parser.have_type = true;
  char input[] = "2fasdf";
  EXPECT_THAT(gettoken(&parser, input, &this_token), Eq(0));
  EXPECT_THAT(strlen(this_token.string), Eq(0));
  EXPECT_THAT(kind_names[this_token.kind], StrEq("invalid"));
}

// '2' is an allowed type character, but ']' is not.
TEST_F(TokenizerSuite, IgnoreUnallowedCharsNoTypeHasDelimIsNotArray) {
  char input[] = "2]fasdf";
  EXPECT_THAT(gettoken(&parser, input, &this_token), Eq(1));
  EXPECT_THAT(strlen(this_token.string), Eq(1));
  EXPECT_THAT(kind_names[this_token.kind], StrEq("invalid"));
}

// '2' is an allowed type character, but ']' is not.
TEST_F(TokenizerSuite, IgnoreUnallowedCharsHasTypeHasDelimIsNotArray) {
  parser.have_type = true;
  char input[] = "2]fasdf";
  EXPECT_THAT(gettoken(&parser, input, &this_token), Eq(0));
  EXPECT_THAT(strlen(this_token.string), Eq(0));
  EXPECT_THAT(kind_names[this_token.kind], StrEq("invalid"));
}

// Without ']', if we have a type, ']' is unallowed.
TEST_F(TokenizerSuite, IgnoreUnallowedCharsNoDelimIsArray) {
  parser.have_type = true;
  parser.array_dimensions = 1;
  char input[] = "2fasdf";
  EXPECT_THAT(gettoken(&parser, input, &this_token), Eq(0));
  EXPECT_THAT(strlen(this_token.string), Eq(0));
  EXPECT_THAT(kind_names[this_token.kind], StrEq("invalid"));
}

TEST_F(TokenizerSuite, IgnoreUnallowedCharsHasDelimIsArray) {
  parser.have_identifier = true;
  parser.have_type = true;
  parser.array_dimensions = 1;
  char input[] = "123456]fasdf";
  EXPECT_THAT(gettoken(&parser, input, &this_token), Eq(strlen("123456")));
  EXPECT_THAT(strlen(this_token.string), Eq(strlen("123456")));
  EXPECT_THAT(this_token.string, StrEq("123456"));
  EXPECT_THAT(kind_names[this_token.kind], StrEq("length"));
}

// An expression which has no type when processing encounters the identifier is
// ill-formed.
TEST_F(TokenizerSuite, IgnoreUnallowedCharsNoTypeIsArray) {
  parser.array_dimensions = 1;
  char input[] = "2fasdf";
  EXPECT_THAT(gettoken(&parser, input, &this_token), Eq(0));
  EXPECT_THAT(strlen(this_token.string), Eq(0));
  EXPECT_THAT(kind_names[this_token.kind], StrEq("invalid"));
}

TEST_F(TokenizerSuite, DoNotElideLeadingUnderscore) {
  parser.have_type = true;
  char input[] = "__val";
  EXPECT_THAT(gettoken(&parser, input, &this_token), Eq(5));
  EXPECT_THAT(this_token.string, StrEq("__val"));
  EXPECT_THAT(this_token.kind, Eq(identifier));
}

TEST_F(TokenizerSuite, PushEmptyStack) {
  struct token token0{type, "int"};
  EXPECT_THAT(parser.stacklen, Eq(0));
  push_stack(&parser, &token0);
  EXPECT_THAT(parser.stack[0].kind, Eq(type));
  EXPECT_THAT(parser.stack[0].string, StrEq("int"));
  EXPECT_THAT(parser.stacklen, Eq(1));
}

TEST_F(TokenizerSuite, Push2ndElement) {
  EXPECT_THAT(parser.stacklen, Eq(0));
  struct token token0{type, "int"};
  push_stack(&parser, &token0);
  struct token token1{qualifier, "const"};
  push_stack(&parser, &token1);
  EXPECT_THAT(parser.stack[0].kind, Eq(type));
  EXPECT_THAT(parser.stack[0].string, StrEq("int"));
  EXPECT_THAT(parser.stack[1].kind, Eq(qualifier));
  EXPECT_THAT(parser.stack[1].string, StrEq("const"));
  EXPECT_THAT(parser.stacklen, Eq(2));
}

// c_str() and unique_ptr.get() are both r-values.
TEST(HandleTrailingDelimSuite, OnlyDelim) {
  _cleanup_(freep) char *output = (char *)malloc(MAXTOKENLEN);
  char input[MAXTOKENLEN];
  strlcpy(input, ")", 2);
  EXPECT_THAT(tokenize_function_params(&output, &input[0], ')'), IsTrue());
}

TEST(HandleTrailingDelimSuite, DelimWithSpaces) {
  _cleanup_(freep) char *output = (char *)malloc(MAXTOKENLEN);
  char input[MAXTOKENLEN];
  strlcpy(input, "struct node *next; } ", strlen("struct node *next; } ") + 1);
  EXPECT_THAT(tokenize_struct_params(&output, &input[0], '}'), IsTrue());
}

/* finish_token() observes "enum " and sets parser.is_enum = true. */
TEST(CheckForEnumerators, WellFormedSimple) {
  struct parser_props parser;
  const char *offset_decl = "State state";
  initialize_parser(&parser);
  parser.is_enum = true;
  EXPECT_THAT(check_for_enum_constants(&parser, offset_decl), IsTrue());
  EXPECT_THAT(parser.has_enum_constants, IsFalse());
}

TEST(CheckForEnumerators, WellFormedEnumerators) {
  struct parser_props parser;
  const char *offset_decl = "State state { SOLID, LIQUID}";
  initialize_parser(&parser);
  parser.is_enum = true;
  EXPECT_THAT(check_for_enum_constants(&parser, offset_decl), IsTrue());
  EXPECT_THAT(parser.has_enum_constants, IsTrue());
}

TEST(CheckForEnumerators, MismatchedDelims) {
  struct parser_props parser;
  const char *offset_decl = "State state {";
  initialize_parser(&parser);
  parser.is_enum = true;
  EXPECT_THAT(check_for_enum_constants(&parser, offset_decl), IsFalse());
  EXPECT_THAT(parser.has_enum_constants, IsFalse());
}

TEST(ElideAssignments, NoEquals) {
  const char *probe = "enum State state;";
  _cleanup_(freep) char *input = (char *)malloc(strlen(probe) + 1);
  strlcpy(input, probe, strlen(probe) + 1);
  elide_assignments(&input);
  EXPECT_THAT(input, StrEq(probe));
}

TEST(ElideAssignments, OneEnumConstantNoAssignment) {
  const char *probe = "enum State state {GAS};";
  _cleanup_(freep) char *input = (char *)malloc(strlen(probe) + 1);
  strlcpy(input, probe, strlen(probe) + 1);
  elide_assignments(&input);
  EXPECT_THAT(input, StrEq(probe));
}

TEST(ElideAssignments, OneEnumConstantNoIdentifierNoAssignment) {
  const char *probe = "enum State {GAS};";
  _cleanup_(freep) char *input = (char *)malloc(strlen(probe) + 1);
  strlcpy(input, probe, strlen(probe) + 1);
  elide_assignments(&input);
  EXPECT_THAT(input, StrEq(probe));
}

TEST(ElideAssignments, OneEnumConstantNoIdentifierOneAssignment) {
  const char *probe = "enum State {GAS=1};";
  _cleanup_(freep) char *input = (char *)malloc(strlen(probe) + 1);
  strlcpy(input, probe, strlen(probe) + 1);
  elide_assignments(&input);
  EXPECT_THAT(input, StrEq("enum State {GAS};"));
}

TEST(ElideAssignments, OneEnumConstantWithIdentifierOneAssignment) {
  const char *probe = "enum State state {GAS=1};";
  _cleanup_(freep) char *input = (char *)malloc(strlen(probe) + 1);
  strlcpy(input, probe, strlen(probe) + 1);
  elide_assignments(&input);
  EXPECT_THAT(input, StrEq("enum State state {GAS};"));
}

TEST(ElideAssignments, TwoEnumConstantNoIdentifierTwoConstantsOneAssignment) {
  const char *probe = "enum State {GAS=1,LIQUID};";
  _cleanup_(freep) char *input = (char *)malloc(strlen(probe) + 1);
  strlcpy(input, probe, strlen(probe) + 1);
  elide_assignments(&input);
  EXPECT_THAT(input, StrEq("enum State {GAS,LIQUID};"));
}

TEST(ElideAssignments,
     TwoEnumConstantNoIdentifierTwoConstantsOneAssignmentWhitespace) {
  const char *probe = "enum State { GAS=1 , LIQUID };";
  _cleanup_(freep) char *input = (char *)malloc(strlen(probe) + 1);
  strlcpy(input, probe, strlen(probe) + 1);
  elide_assignments(&input);
  EXPECT_THAT(input, StrEq("enum State { GAS, LIQUID };"));
}

TEST(ElideAssignments,
     TwoEnumConstantNoIdentifierTwoConstantsTrailingAssignment) {
  const char *probe = "enum State {GAS,LIQUID=3};";
  _cleanup_(freep) char *input = (char *)malloc(strlen(probe) + 1);
  strlcpy(input, probe, strlen(probe) + 1);
  elide_assignments(&input);
  EXPECT_THAT(input, StrEq("enum State {GAS,LIQUID};"));
}

TEST(ElideAssignments,
     ThreeEnumConstantNoIdentifierTwoConstantsMiddleAssignment) {
  const char *probe = "enum State {GAS,LIQUID=3,SOLID};";
  _cleanup_(freep) char *input = (char *)malloc(strlen(probe) + 1);
  strlcpy(input, probe, strlen(probe) + 1);
  elide_assignments(&input);
  EXPECT_THAT(input, StrEq("enum State {GAS,LIQUID,SOLID};"));
}

TEST(ElideAssignments, TwoEnumConstantsNoIdentifierSpaceAfterComma) {
  const char *probe = "enum State {GAS,LIQUID ,};";
  _cleanup_(freep) char *input = (char *)malloc(strlen(probe) + 1);
  strlcpy(input, probe, strlen(probe) + 1);
  elide_assignments(&input);
  EXPECT_THAT(input, StrEq(probe));
}

TEST(ElideAssignments, TwoEnumConstantsNoIdentifierSpaceAfterFirstComma) {
  const char *probe = "enum State {GAS, LIQUID=3};";
  _cleanup_(freep) char *input = (char *)malloc(strlen(probe) + 1);
  strlcpy(input, probe, strlen(probe) + 1);
  elide_assignments(&input);
  EXPECT_THAT(input, StrEq("enum State {GAS, LIQUID};"));
}

TEST(ElideAssignments, TwoEnumConstantsNoIdentifierSpaceBeforeFirstComma) {
  const char *probe = "enum State {GAS=2 ,LIQUID,};";
  _cleanup_(freep) char *input = (char *)malloc(strlen(probe) + 1);
  strlcpy(input, probe, strlen(probe) + 1);
  elide_assignments(&input);
  EXPECT_THAT(input, StrEq("enum State {GAS,LIQUID,};"));
}

TEST(ElideAssignments, TwoEnumConstantsNoIdentifierSpaceBeforeSecondComma) {
  const char *probe = "enum State {GAS,LIQUID=2 ,};";
  _cleanup_(freep) char *input = (char *)malloc(strlen(probe) + 1);
  strlcpy(input, probe, strlen(probe) + 1);
  elide_assignments(&input);
  EXPECT_THAT(input, StrEq("enum State {GAS,LIQUID,};"));
}

TEST(ElideAssignments, ThreeEnumConstantsNoIdentifierSpaceBeforeComma) {
  const char *probe = "enum State {GAS,LIQUID=3 ,SOLID};";
  _cleanup_(freep) char *input = (char *)malloc(strlen(probe) + 1);
  strlcpy(input, probe, strlen(probe) + 1);
  elide_assignments(&input);
  EXPECT_THAT(input, StrEq("enum State {GAS,LIQUID,SOLID};"));
}

TEST(ElideAssignments, ThreeEnumConstantsWihIdentifierSpaceBeforeComma) {
  const char *probe = "enum State state {GAS,LIQUID=3 ,SOLID};";
  _cleanup_(freep) char *input = (char *)malloc(strlen(probe) + 1);
  strlcpy(input, probe, strlen(probe) + 1);
  elide_assignments(&input);
  EXPECT_THAT(input, StrEq("enum State state {GAS,LIQUID,SOLID};"));
}

TEST(ParensMatch, SimpleCase) {
  const char *probe = "int (*ap)[2] = &a;";
  size_t pair_count = 0;
  EXPECT_THAT(parens_match(probe, &pair_count), IsTrue());
  EXPECT_THAT(pair_count, Eq(1));
}

TEST(ParensMatch, NoOpener) {
  const char *probe = "int *ap)[2] = &a;";
  size_t pair_count = 0;
  EXPECT_THAT(parens_match(probe, &pair_count), IsFalse());
  EXPECT_THAT(pair_count, Eq(0));
}

TEST(ParensMatch, NoCloser) {
  const char *probe = "int (*ap[2] = &a;";
  size_t pair_count = 0;
  EXPECT_THAT(parens_match(probe, &pair_count), IsFalse());
  EXPECT_THAT(pair_count, Eq(0));
}

TEST(ParensMatch, CountMatchesWrongOrder) {
  const char *probe = "int )*ap([2] = &a;";
  size_t pair_count = 0;
  EXPECT_THAT(parens_match(probe, &pair_count), IsFalse());
  EXPECT_THAT(pair_count, Eq(0));
}

TEST(ParensMatch, Nested) {
  const char *probe = "(int ((*ap)[2])) = &a;";
  size_t pair_count = 0;
  EXPECT_THAT(parens_match(probe, &pair_count), IsTrue());
  EXPECT_THAT(pair_count, Eq(3));
}

TEST(ParensMatch, NestedWrongOrder) {
  const char *probe = "int ()*ap)[2] = &a;";
  size_t pair_count = 0;
  EXPECT_THAT(parens_match(probe, &pair_count), IsFalse());
  EXPECT_THAT(pair_count, Eq(0));
}

TEST(ParensMatch, NewScopeBrace) {
  const char *probe = "struct nodelist (*node)[2] { int (*payload)[2]; };";
  size_t pair_count = 0;
  EXPECT_THAT(parens_match(probe, &pair_count), IsTrue());
  EXPECT_THAT(pair_count, Eq(1));
}

TEST(ParensMatch, NewScopeArray) {
  const char *probe = "struct nodelist (*node[2)];";
  size_t pair_count = 0;
  EXPECT_THAT(parens_match(probe, &pair_count), IsFalse());
  EXPECT_THAT(pair_count, Eq(0));
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

TEST(CheckForDeclaratorListTest, OneDeclarator) {
  struct parser_props parser;
  const char *user_input = "double hash[4]";
  check_for_declarator_list(&parser, user_input);
  EXPECT_THAT(parser.is_declarator_list, IsFalse());
}

TEST(CheckForDeclaratorListTest, TwoDeclarators) {
  struct parser_props parser;
  const char *user_input = "double hash[4], sum";
  check_for_declarator_list(&parser, user_input);
  EXPECT_THAT(parser.is_declarator_list, IsTrue());
}

TEST(CheckForDeclaratorListTest, Function) {
  struct parser_props parser;
  const char *user_input = "double hash(uint64_t seed, const char *key)";
  check_for_declarator_list(&parser, user_input);
  EXPECT_THAT(parser.is_declarator_list, IsFalse());
}

TEST(CheckForDeclaratorListTest, DeclaratorAndFunctionLast) {
  struct parser_props parser;
  const char *user_input =
      "double sum, hash(uint64_t seed, const char *key, uint8_t flags)";
  check_for_declarator_list(&parser, user_input);
  EXPECT_THAT(parser.is_declarator_list, IsTrue());
}

TEST(CheckForDeclaratorListTest, DeclaratorAndFunctionFirst) {
  struct parser_props parser;
  const char *user_input =
      "double hash(uint64_t seed, const char *key, uint8_t flags), sum";
  check_for_declarator_list(&parser, user_input);
  EXPECT_THAT(parser.is_declarator_list, IsTrue());
}

TEST(CheckForDeclaratorListTest, FunctionPtr) {
  struct parser_props parser;
  const char *user_input = "int (*open) (struct inode *blk, struct file *dir);";
  check_for_declarator_list(&parser, user_input);
  EXPECT_THAT(parser.is_declarator_list, IsFalse());
}

TEST(CheckForDeclaratorListTest, Enum) {
  struct parser_props parser;
  const char *user_input = "enum State {GAS, LIQUID}";
  check_for_declarator_list(&parser, user_input);
  EXPECT_THAT(parser.is_declarator_list, IsFalse());
}
