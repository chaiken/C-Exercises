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

TEST(ProcesStringInputSuite, LeadingDash) {
  char inputstr[MAXTOKENLEN];
  const std::string dashes{"--;"};
  EXPECT_THAT(find_input_string(dashes.c_str(), inputstr, stdin), Eq(3));
  EXPECT_THAT(inputstr, StrEq("--;"));
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

/* Tab is classified as invalid. */
TEST(StringManipulateSuite, GetKindWhitespace) {
  EXPECT_THAT(get_kind(" "), Eq(whitespace));
}

TEST(StringManipulateSuite, GetKindQualifiers) {
  EXPECT_THAT(get_kind("const"), Eq(qualifier));
  EXPECT_THAT(get_kind("volatile"), Eq(qualifier));
  EXPECT_THAT(get_kind("static"), Eq(qualifier));
  EXPECT_THAT(get_kind("extern"), Eq(qualifier));
  EXPECT_THAT(get_kind("*"), Eq(qualifier));
  EXPECT_THAT(get_kind("unsigned"), Eq(qualifier));
  EXPECT_THAT(get_kind("restrict"), Eq(qualifier));
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
  EXPECT_THAT(get_kind("size_t"), Eq(type));
  EXPECT_THAT(get_kind("ssize_t"), Eq(type));
  EXPECT_THAT(get_kind("bool"), Eq(type));
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

// Unallowed characters and anything following them are simply cut off.
// Encountering an identifier without first finding a type means that the
// expression is ill-formed, but that logic is at the stack-loading level, not
// the tokenizer level.
TEST_F(TokenizerSuite, IgnoreUnallowedCharsNoType) {
  // '5' is neither a name nor type char.
  char input[] = "f5asdf";
  EXPECT_THAT(gettoken(&parser, input, &this_token), Eq(1));
  EXPECT_THAT(this_token.string, StrEq("f"));
  EXPECT_THAT(this_token.kind, Eq(identifier));
}

TEST_F(TokenizerSuite, IgnoreUnallowedCharsStdintNoType) {
  char input[] = "uint32_t";
  EXPECT_THAT(gettoken(&parser, input, &this_token), strlen(input));
  EXPECT_THAT(this_token.string, StrEq(input));
  EXPECT_THAT(this_token.kind, Eq(type));
}

TEST_F(TokenizerSuite, IgnoreUnallowedCharsStdintHasType) {
  parser.have_type = true;
  char input[] = "uint32_t";
  EXPECT_THAT(gettoken(&parser, input, &this_token), strlen("uint"));
  EXPECT_THAT(this_token.string, StrEq("uint"));
  EXPECT_THAT(this_token.kind, Eq(identifier));
}

// Produces same result as above, as the comment describes.
TEST_F(TokenizerSuite, IgnoreUnallowedCharsHasType) {
  parser.have_type = true;
  char input[] = "f5asdf";
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
  EXPECT_THAT(strlen(this_token.string), Eq(0));
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
TEST(HandleTrailingDelimSuite, InputOk) {
  _cleanup_(freep) char *output = (char *)malloc(MAXTOKENLEN);
  const char *input = "double val)";
  EXPECT_THAT(tokenize_secondary_params(&output, input, ')'), IsTrue());
  EXPECT_THAT(output, StrEq("double val"));
}

TEST(HandleTrailingDelimSuite, MissingDelim) {
  _cleanup_(freep) char *output = (char *)malloc(MAXTOKENLEN);
  const char *input = "double val";
  EXPECT_THAT(tokenize_secondary_params(&output, input, ')'), IsFalse());
}

TEST(HandleTrailingDelimSuite, OnlyDelim) {
  _cleanup_(freep) char *output = (char *)malloc(MAXTOKENLEN);
  const char *input = ")";
  EXPECT_THAT(tokenize_secondary_params(&output, input, ')'), IsTrue());
}

TEST(HandleTrailingDelimSuite, DelimWithSpaces) {
  _cleanup_(freep) char *output = (char *)malloc(MAXTOKENLEN);
  const char *input = "struct node *next; } ";
  EXPECT_THAT(tokenize_secondary_params(&output, input, '}'), IsTrue());
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

struct ParserSuite : public Test {
  ParserSuite() : fake_stdout(tmpfile()), fake_stderr(tmpfile()) {
    this_token.kind = invalid;
    memset(this_token.string, '\0', MAXTOKENLEN);
    initialize_parser(&parser);
    set_test_streams(&parser, fake_stdout, fake_stderr);
  }

  ~ParserSuite() override {
    fclose(fake_stdout);
    fclose(fake_stderr);
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
  FILE *fake_stdout;
  FILE *fake_stderr;
  // The following causes a memory leak:
  // char* output_str = (char*)malloc(MAXTOKENLEN);
  //
  // The reason is that getline() changes the address of output_str.  One can
  // save the initial value of output_str and free() that, but memory which
  // glibc allocated will still leak:
  // ==2131242==ERROR: LeakSanitizer: detected memory leaks
  // Direct leak of 120 byte in 1 object allocated from:
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
  EXPECT_THAT(truncate_input(&token, &parser), IsTrue());
  // Checking strlen() assures that the result is NULL-terminated.
  EXPECT_THAT(strlen(token), Eq(5));
  EXPECT_THAT(token, StrEq("int x"));

  memset(token, '\0', MAXTOKENLEN);
  strlcpy(token, "int x   ;", MAXTOKENLEN);
  EXPECT_THAT(truncate_input(&token, &parser), IsTrue());
  EXPECT_THAT(strlen(token), Eq(5));
  EXPECT_THAT(token, StrEq("int x"));

  memset(token, '\0', MAXTOKENLEN);
  strlcpy(token, "int x = 2;", MAXTOKENLEN);
  EXPECT_THAT(truncate_input(&token, &parser), IsTrue());
  EXPECT_THAT(strlen(token), Eq(5));
  EXPECT_THAT(token, StrEq("int x"));

  memset(token, '\0', MAXTOKENLEN);
  strlcpy(token, "const int x;", MAXTOKENLEN);
  EXPECT_THAT(truncate_input(&token, &parser), IsTrue());
  EXPECT_THAT(strlen(token), Eq(11));
  EXPECT_THAT(token, StrEq("const int x"));

  memset(token, '\0', MAXTOKENLEN);
  strlcpy(token, "int x", MAXTOKENLEN);
  EXPECT_THAT(truncate_input(&token, &parser), IsFalse());
  EXPECT_THAT(StderrMatches("Improperly terminated declaration."), IsTrue());

  memset(token, '\0', MAXTOKENLEN);
  strlcpy(token, ";int x", MAXTOKENLEN);
  EXPECT_THAT(truncate_input(&token, &parser), IsFalse());
  EXPECT_THAT(StderrMatches("Zero-length input string."), IsTrue());

  memset(token, '\0', MAXTOKENLEN);
  strlcpy(token, "   = ", MAXTOKENLEN);
  EXPECT_THAT(truncate_input(&token, &parser), IsFalse());
  EXPECT_THAT(StderrMatches("Zero-length input string."), IsTrue());

  memset(token, '\0', MAXTOKENLEN);
  strlcpy(token, "uint32_t f[21];", MAXTOKENLEN);
  EXPECT_THAT(truncate_input(&token, &parser), IsTrue());
  EXPECT_THAT(strlen(token), Eq(strlen("uint32_t f[21]")));
  EXPECT_THAT(token, StrEq("uint32_t f[21]"));

  memset(token, '\0', MAXTOKENLEN);
  strlcpy(token, "uint32_t f[2] = {3,4};", MAXTOKENLEN);
  EXPECT_THAT(truncate_input(&token, &parser), IsTrue());
  EXPECT_THAT(strlen(token), Eq(strlen("uint32_t f[2]")));
  EXPECT_THAT(token, StrEq("uint32_t f[2]"));

  free(token);
}

TEST_F(ParserSuite, LoadStackSimpleExcessidentifier) {
  char user_input[MAXTOKENLEN];
  const char *probe = "double hash payload";
  strlcpy(user_input, probe, strlen(probe) + 1);
  std::size_t consumed = load_stack(&parser, user_input);
  EXPECT_THAT(consumed, Eq(0));
  EXPECT_THAT(parser.stacklen, Eq(0));
}

TEST_F(ParserSuite, LoadStackSimpleExcessType) {
  char user_input[MAXTOKENLEN];
  const char *probe = "double int hash";
  strlcpy(user_input, probe, strlen(probe) + 1);
  std::size_t consumed = load_stack(&parser, user_input);
  EXPECT_THAT(consumed, Eq(0));
  EXPECT_THAT(parser.stacklen, Eq(0));
}

TEST_F(ParserSuite, ProcessFunctionParamsOneParam) {
  char user_input[MAXTOKENLEN];
  const char *query = "double sqrt(double val)";
  // The following characters were processed by the first parser.
  parser.cursor = strlen("double sqrt");

  parser.has_function_params = true;
  strlcpy(user_input, query, strlen(query) + 1);

  process_secondary_params(&parser, user_input);

  // When process_secondary_params() runs, the first parser has already handled
  // all the text before the opening parentheses.
  EXPECT_THAT(parser.stacklen, Eq(0));
  EXPECT_THAT(parser.cursor, Eq(strlen("double sqrt(double val)")));
  // The whole string has been consumed.
  EXPECT_THAT(user_input + parser.cursor, StrEq(""));
  ASSERT_THAT(parser.next, Not(IsNull()));
  EXPECT_THAT(parser.next->stacklen, Eq(2));
  EXPECT_THAT(parser.next->stack[0].kind, Eq(type));
  EXPECT_THAT(parser.next->stack[0].string, StrEq("double"));
  EXPECT_THAT(parser.next->stack[1].kind, Eq(identifier));
  EXPECT_THAT(parser.next->stack[1].string, StrEq("val"));
  // Normally freed by pop_stack().
  free(parser.next);
}

TEST_F(ParserSuite, ProcessFunctionParamsOneParamBadDelim) {
  char user_input[MAXTOKENLEN];
  const char *query = "double sqrt(double val";
  // The following characters were processed by the first parser.
  parser.cursor = strlen("double sqrt");

  parser.has_function_params = true;
  strlcpy(user_input, query, strlen(query) + 1);

  process_secondary_params(&parser, user_input);
  ASSERT_THAT(parser.next, IsNull());
  EXPECT_THAT(StderrMatches("Failed to process last function parameter"),
              IsTrue());
}

TEST_F(ParserSuite, LoadStackFunctionExcessidentifier) {
  char user_input[MAXTOKENLEN];
  const char *probe = "double hash(int payload foo)";
  strlcpy(user_input, probe, strlen(probe) + 1);
  std::size_t consumed = load_stack(&parser, user_input);
  EXPECT_THAT(consumed, Eq(0));
  EXPECT_THAT(parser.stacklen, Eq(0));
}

TEST_F(ParserSuite, LoadStackFunctionExcessType) {
  char user_input[MAXTOKENLEN];
  const char *probe = "double hash(int enum foo)";
  strlcpy(user_input, probe, strlen(probe) + 1);
  std::size_t consumed = load_stack(&parser, user_input);
  parser.err_stream = stderr;
  showstack(&parser.stack[0], parser.stacklen, stdout, __LINE__);
  EXPECT_THAT(consumed, Eq(0));
  EXPECT_THAT(parser.stacklen, Eq(0));
}

TEST_F(ParserSuite, ProcessFunctionParamsTwoParams) {
  char user_input[MAXTOKENLEN];
  const char *query = "uint64_t hash(char *key, uint64_t seed)";
  // The following characters were processed by the first parser.
  parser.cursor = strlen("uint64_t hash");

  parser.has_function_params = true;
  strlcpy(user_input, query, strlen(query) + 1);

  process_secondary_params(&parser, user_input);
  ASSERT_THAT(parser.next, Not(IsNull()));

  EXPECT_THAT(StdoutMatches("Token number 0 has kind type and string char"),
              IsTrue());
  EXPECT_THAT(StdoutMatches("Token number 1 has kind qualifier and string *"),
              IsTrue());
  EXPECT_THAT(
      StdoutMatches("Token number 2 has kind identifier and string key"),
      IsTrue());
  EXPECT_THAT(StdoutMatches("Token number 0 has kind type and string uint64_t"),
              IsTrue());
  EXPECT_THAT(
      StdoutMatches("Token number 1 has kind identifier and string seed"),
      IsTrue());
  struct parser_props *pnext = parser.next;
  while (pnext) {
    struct parser_props *save = pnext->next;
    std::cout << "Test: freeing pnext: " << std::hex << pnext << std::endl;
    free(pnext);
    pnext = save;
  }
}

TEST_F(ParserSuite, ProcessFunctionParamsStrayComma) {
  char user_input[MAXTOKENLEN];
  const char *query = "double sqrt(double val,)";
  // The following characters were processed by the first parser.
  parser.cursor = strlen("double sqrt");

  parser.has_function_params = true;
  strlcpy(user_input, query, strlen(query) + 1);

  EXPECT_THAT(process_secondary_params(&parser, user_input), IsTrue());
  ASSERT_THAT(parser.next, Not(IsNull()));
  free_all_parsers(&parser);
}

// Parsing succeeds as a side effect of the code added to
// process_secondary_params() in order to deal with function pointers as struct
// members.
TEST_F(ParserSuite, ProcessFunctionParamsStrayMiddleComma) {
  char user_input[MAXTOKENLEN];
  const char *query = "uint64_t hash(char *key, , uint64_t seed)";
  // The following characters were processed by the first parser.
  parser.cursor = strlen("uint64_t hash");

  parser.has_function_params = true;
  strlcpy(user_input, query, strlen(query) + 1);

  EXPECT_THAT(process_secondary_params(&parser, user_input), IsTrue());
  ASSERT_THAT(parser.next, Not(IsNull()));
  EXPECT_THAT(parser.next->next, Not(IsNull()));
  free_all_parsers(&parser);
}

TEST_F(ParserSuite, ProcessFunctionParamsLeadingWhitespace) {
  char user_input[MAXTOKENLEN];
  const char *query = "uint64_t hash(   char *key, uint64_t seed)";
  // The following characters were processed by the first parser.
  parser.cursor = strlen("uint64_t hash");

  parser.has_function_params = true;
  strlcpy(user_input, query, strlen(query) + 1);

  process_secondary_params(&parser, user_input);
  ASSERT_THAT(parser.next, Not(IsNull()));

  EXPECT_THAT(StdoutMatches("Token number 0 has kind type and string char"),
              IsTrue());
  EXPECT_THAT(StdoutMatches("Token number 1 has kind qualifier and string *"),
              IsTrue());
  EXPECT_THAT(
      StdoutMatches("Token number 2 has kind identifier and string key"),
      IsTrue());
  EXPECT_THAT(StdoutMatches("Token number 0 has kind type and string uint64_t"),
              IsTrue());
  EXPECT_THAT(
      StdoutMatches("Token number 1 has kind identifier and string seed"),
      IsTrue());
  struct parser_props *pnext = parser.next;
  while (pnext) {
    struct parser_props *save = pnext->next;
    std::cout << "Test: freeing pnext: " << std::hex << pnext << std::endl;
    free(pnext);
    pnext = save;
  }
}

TEST_F(ParserSuite, ProcessStructMembersOneMemberWithInstanceName) {
  char user_input[MAXTOKENLEN];
  const char *query = "struct node nodelist {int payload;}";
  // The following characters were processed by the first parser.
  parser.cursor = strlen("struct node nodelist");

  parser.is_struct = true;
  parser.has_struct_members = true;
  strlcpy(user_input, query, strlen(query) + 1);

  process_secondary_params(&parser, user_input);

  // When process_secondary_params() runs, the first parser has already handled
  // all the text before the opening parentheses.
  EXPECT_THAT(parser.stacklen, Eq(0));
  EXPECT_THAT(parser.cursor, Eq(strlen("struct node nodelist {int payload};")));
  EXPECT_THAT(user_input + parser.cursor, StrEq(""));
  EXPECT_THAT(parser.is_function, IsFalse());
  EXPECT_THAT(parser.is_enum, IsFalse());
  ASSERT_THAT(parser.next, Not(IsNull()));
  // The subsidiary parser doesn't see "struct".
  EXPECT_THAT(parser.next->is_struct, IsFalse());
  EXPECT_THAT(parser.next->is_function, IsFalse());
  EXPECT_THAT(parser.next->is_enum, IsFalse());
  EXPECT_THAT(parser.next->stacklen, Eq(2));
  EXPECT_THAT(parser.next->stack[0].kind, Eq(type));
  EXPECT_THAT(parser.next->stack[0].string, StrEq("int"));
  EXPECT_THAT(parser.next->stack[1].kind, Eq(identifier));
  EXPECT_THAT(parser.next->stack[1].string, StrEq("payload"));
  EXPECT_THAT(parser.next->next, IsNull());
  // Normally freed by pop_stack().
  free_all_parsers(&parser);
}

TEST_F(ParserSuite, PopEmpty) {
  EXPECT_THAT(pop_stack(&parser, true), Eq(-ENODATA));
  EXPECT_THAT(StderrMatches("Attempt to pop empty stack."), IsTrue());
}

TEST_F(ParserSuite, PopOne) {
  struct token token0{type, "int"};
  push_stack(&parser, &token0);

  EXPECT_THAT(pop_stack(&parser, false), Eq(0));
  EXPECT_THAT(StdoutMatches("int"), IsTrue());
}

TEST_F(ParserSuite, PopAll) {
  struct token token0{type, "char"};
  push_stack(&parser, &token0);
  struct token token1{qualifier, "*"};
  push_stack(&parser, &token1);
  struct token token2{identifier, "buffer"};
  push_stack(&parser, &token2);

  EXPECT_THAT(pop_all(&parser), Eq(0));
  EXPECT_THAT(StdoutMatches("buffer"), IsTrue());
  EXPECT_THAT(StdoutMatches("is a(n) pointer to"), IsTrue());
  EXPECT_THAT(StdoutMatches("char"), IsTrue());
}

TEST_F(ParserSuite, PopAllOneFunctionParam) {
  struct token token0{type, "double"};
  push_stack(&parser, &token0);
  struct token token1{identifier, "sqrt"};
  push_stack(&parser, &token1);
  parser.has_function_params = true;

  struct parser_props *params_parser = make_parser(&parser);
  ASSERT_THAT(parser.next, Not(IsNull()));
  ASSERT_THAT(parser.next->prev, Not(IsNull()));
  ASSERT_THAT(params_parser->prev, Not(IsNull()));

  struct token token2{type, "int64_t"};
  push_stack(parser.next, &token2);
  struct token token3{identifier, "val"};
  push_stack(parser.next, &token3);

  EXPECT_THAT(pop_all(&parser), Eq(0));
  EXPECT_THAT(StdoutMatches("sqrt"), IsTrue());
  EXPECT_THAT(StdoutMatches("double"), IsTrue());
  EXPECT_THAT(StdoutMatches("val"), IsTrue());
  EXPECT_THAT(StdoutMatches("int64_t"), IsTrue());
}

TEST_F(ParserSuite, PopAllTwoFunctionParams) {
  struct token token0{type, "double"};
  push_stack(&parser, &token0);
  struct token token1{identifier, "hash"};
  push_stack(&parser, &token1);
  parser.has_function_params = true;

  struct parser_props *params_parser = make_parser(&parser);
  ASSERT_THAT(parser.next, Not(IsNull()));
  ASSERT_THAT(parser.next->prev, Not(IsNull()));
  ASSERT_THAT(params_parser->prev, Not(IsNull()));

  struct token token2{type, "char"};
  push_stack(parser.next, &token2);
  struct token token3{qualifier, "*"};
  push_stack(parser.next, &token3);
  struct token token4{identifier, "key"};
  push_stack(parser.next, &token4);

  struct parser_props *params_parser2 = make_parser(parser.next);
  ASSERT_THAT(parser.next->next, Not(IsNull()));
  ASSERT_THAT(parser.next->next->prev, Not(IsNull()));
  ASSERT_THAT(params_parser2->prev, Not(IsNull()));

  struct token token5{type, "int64_t"};
  push_stack(parser.next->next, &token5);
  struct token token6{identifier, "seed"};
  push_stack(parser.next->next, &token6);

  EXPECT_THAT(pop_all(&parser), Eq(0));
  EXPECT_THAT(StdoutMatches("hash"), IsTrue());
  EXPECT_THAT(StdoutMatches("double"), IsTrue());
  EXPECT_THAT(StdoutMatches("key"), IsTrue());
  EXPECT_THAT(StdoutMatches("pointer"), IsTrue());
  EXPECT_THAT(StdoutMatches("char"), IsTrue());
  EXPECT_THAT(StdoutMatches("seed"), IsTrue());
  EXPECT_THAT(StdoutMatches("int64_t"), IsTrue());
}

TEST_F(ParserSuite, Showstack) {
  struct token token0{type, "int"};
  push_stack(&parser, &token0);
  struct token token1{qualifier, "const"};
  push_stack(&parser, &token1);
  showstack(&parser.stack[0], parser.stacklen, parser.out_stream, __LINE__);
  EXPECT_THAT(StdoutMatches("Stack at"), IsTrue());
  EXPECT_THAT(StdoutMatches("Token number 0 has kind type and string int"),
              IsTrue());
  EXPECT_THAT(
      StdoutMatches("Token number 1 has kind qualifier and string const"),
      IsTrue());
  showstack(&parser.stack[0], parser.stacklen, stdout, __LINE__);
}

TEST_F(ParserSuite, ShowParsersFromHead) {
  // Create and check the parser list.
  _cleanup_(freep) struct parser_props *parser1 = make_parser(&parser);
  ASSERT_THAT(parser1, Not(IsNull()));
  _cleanup_(freep) struct parser_props *parser2 = make_parser(parser1);
  ASSERT_THAT(parser2, Not(IsNull()));
  EXPECT_THAT(parser1->prev, Eq(&parser));
  EXPECT_THAT(parser.next->next, Eq(parser2));
  EXPECT_THAT(parser1->next, Eq(parser2));
  EXPECT_THAT(parser2->prev, Eq(parser1));
  EXPECT_THAT(parser2->prev->prev, Eq(&parser));

  show_parser_list(&parser, __LINE__);

  const std::string descender{"-->"};
  const std::string head{"HEAD at"};
  std::ostringstream oss0{};
  oss0 << &parser;
  std::ostringstream oss1{};
  oss1 << parser1;
  std::ostringstream oss2{};
  oss2 << parser2;
  EXPECT_THAT(StderrMatches(head) && StderrMatches(oss0.str() + descender),
              IsTrue());
  EXPECT_THAT(StderrMatches(descender + oss1.str()), IsTrue());
  EXPECT_THAT(StderrMatches(descender + oss2.str()), IsTrue());
}

TEST_F(ParserSuite, ShowParsersFromTail) {
  // Create and check the parser list.
  _cleanup_(freep) struct parser_props *parser1 = make_parser(&parser);
  ASSERT_THAT(parser1, Not(IsNull()));
  _cleanup_(freep) struct parser_props *parser2 = make_parser(parser1);
  ASSERT_THAT(parser2, Not(IsNull()));
  EXPECT_THAT(parser1->prev, Eq(&parser));
  EXPECT_THAT(parser.next->next, Eq(parser2));
  EXPECT_THAT(parser1->next, Eq(parser2));
  EXPECT_THAT(parser2->prev, Eq(parser1));
  EXPECT_THAT(parser2->prev->prev, Eq(&parser));

  show_parser_list(parser2, __LINE__);
  const std::string descender{"-->"};
  const std::string head{"HEAD at"};
  std::ostringstream oss0{};
  oss0 << &parser;
  std::ostringstream oss1{};
  oss1 << parser1;
  std::ostringstream oss2{};
  oss2 << parser2;
  EXPECT_THAT(StderrMatches(head) && StderrMatches(oss0.str() + descender),
              IsTrue());
  EXPECT_THAT(StderrMatches(descender + oss1.str()), IsTrue());
  EXPECT_THAT(StderrMatches(descender + oss2.str()), IsTrue());
}

TEST_F(ParserSuite, ShowParsersReverse) {
  // Create and check the parser list.
  _cleanup_(freep) struct parser_props *parser1 = make_parser(&parser);
  _cleanup_(freep) struct parser_props *parser2 = make_parser(parser1);

  show_parser_reverse_list(parser2);

  const std::string descender{"<--"};
  const std::string tail{"TAIL: "};
  std::ostringstream oss0{};
  oss0 << &parser;
  std::ostringstream oss1{};
  oss1 << parser1;
  std::ostringstream oss2{};
  oss2 << parser2;
  EXPECT_THAT(StderrMatches(tail) && StderrMatches(oss2.str() + descender),
              IsTrue());
  EXPECT_THAT(StderrMatches(descender + oss1.str()), IsTrue());
  EXPECT_THAT(StderrMatches(descender + oss0.str()), IsTrue());
}

TEST_F(ParserSuite, GetHeadParser) {
  struct parser_props *parser0 = make_parser(&parser);
  struct parser_props *parser1 = make_parser(parser0);
  struct parser_props *parser2 = make_parser(parser1);
  struct parser_props *head = get_head_parser(parser2);
  std::ostringstream oss0{};
  oss0 << head;
  std::ostringstream oss1{};
  oss1 << &parser;
  ASSERT_THAT(oss0.str(), StrEq(oss1.str()));
  free_all_parsers(head);
}

TEST_F(ParserSuite, LoadStackWorks) {
  char user_input[MAXTOKENLEN];
  const char *probe = "const int* x";
  strlcpy(user_input, probe, strlen(probe) + 1);
  std::size_t consumed = load_stack(&parser, user_input);
  EXPECT_THAT(consumed, Eq(strlen(probe)));
  EXPECT_THAT(StdoutMatches("Token number 0 has kind type and string int"),
              IsTrue());
  EXPECT_THAT(
      StdoutMatches("Token number 1 has kind qualifier and string const"),
      IsTrue());
  EXPECT_THAT(StdoutMatches("Token number 2 has kind qualifier and string *"),
              IsTrue());
  EXPECT_THAT(StdoutMatches("Token number 3 has kind identifier and string x"),
              IsTrue());
  showstack(&parser.stack[0], parser.stacklen, stdout, __LINE__);
}

TEST_F(ParserSuite, LoadStackEqualsTerminator) {
  char user_input[MAXTOKENLEN];
  const char *probe = "static double val       = 2";
  strlcpy(user_input, probe, strlen(probe) + 1);
  std::size_t consumed = load_stack(&parser, user_input);

  EXPECT_THAT(consumed, Eq(strlen("static double val")));
  EXPECT_THAT(StdoutMatches("Token number 0 has kind type and string double"),
              IsTrue());
  EXPECT_THAT(
      StdoutMatches("Token number 1 has kind qualifier and string static"),
      IsTrue());
  EXPECT_THAT(
      StdoutMatches("Token number 2 has kind identifier and string val"),
      IsTrue());
  showstack(&parser.stack[0], parser.stacklen, stdout, __LINE__);
}

TEST_F(ParserSuite, SimpleFunction) {
  char user_input[MAXTOKENLEN];
  const char *probe = "double sqrt()";
  strlcpy(user_input, probe, strlen(probe) + 1);
  std::size_t consumed = load_stack(&parser, user_input);
  EXPECT_THAT(consumed, Eq(strlen("double sqrt")));
  // When there are no function parameters, there is no second parser.
  EXPECT_THAT(parser.next, IsNull());
  EXPECT_THAT(parser.stack[1].kind, Eq(identifier));
  EXPECT_THAT(parser.stack[1].string, StrEq("sqrt"));
}

TEST_F(ParserSuite, SimpleFunctionBadDelims) {
  char user_input[MAXTOKENLEN];
  const char *probe = "double sqrt(";
  strlcpy(user_input, probe, strlen(probe) + 1);
  std::size_t consumed = load_stack(&parser, user_input);
  EXPECT_THAT(consumed, Eq(0));
  EXPECT_THAT(StderrMatches("Unmatched parentheses:  sqrt("), IsTrue());
}

TEST_F(ParserSuite, LoadStackParensTerminatorOneFunctionParam) {
  char user_input[MAXTOKENLEN];
  const char *probe = "uint64_t hash(char *str)";
  strlcpy(user_input, probe, strlen(probe) + 1);
  EXPECT_THAT(load_stack(&parser, user_input),
              Eq(strlen("uint64_t hash(char *str)")));
  EXPECT_THAT(parser.is_function, IsTrue());
  EXPECT_THAT(parser.has_function_params, IsTrue());
  ASSERT_THAT(parser.next, Not(IsNull()));
  EXPECT_THAT(StdoutMatches("Token number 0 has kind type and string char"),
              IsTrue());
  EXPECT_THAT(StdoutMatches("Token number 1 has kind qualifier and string *"),
              IsTrue());
  EXPECT_THAT(
      StdoutMatches("Token number 2 has kind identifier and string str"),
      IsTrue());
  EXPECT_THAT(StdoutMatches("Token number 0 has kind type and string uint64_t"),
              IsTrue());
  EXPECT_THAT(
      StdoutMatches("Token number 1 has kind identifier and string hash"),
      IsTrue());
  // Otherwise freed by pop_all().
  free(parser.next);
}

TEST_F(ParserSuite, LoadStackCommaTerminatorFunction) {
  char user_input[MAXTOKENLEN];
  const char *probe = "uint64_t hash(char *str, uint64_t seed)";
  strlcpy(user_input, probe, strlen(probe) + 1);
  EXPECT_THAT(load_stack(&parser, user_input), Eq(strlen(probe)));
  show_parser_list(&parser, __LINE__);
  ASSERT_THAT(parser.next, Not(IsNull()));
  EXPECT_THAT(parser.next->next, Not(IsNull()));
  EXPECT_THAT(StdoutMatches("Token number 0 has kind type and string uint64_t"),
              IsTrue());
  EXPECT_THAT(
      StdoutMatches("Token number 1 has kind identifier and string seed"),
      IsTrue());
  EXPECT_THAT(StdoutMatches("Token number 0 has kind type and string char"),
              IsTrue());
  EXPECT_THAT(StdoutMatches("Token number 1 has kind qualifier and string *"),
              IsTrue());
  EXPECT_THAT(
      StdoutMatches("Token number 2 has kind identifier and string str"),
      IsTrue());
  EXPECT_THAT(StdoutMatches("Token number 0 has kind type and string uint64_t"),
              IsTrue());
  EXPECT_THAT(
      StdoutMatches("Token number 1 has kind identifier and string hash"),
      IsTrue());
  // Otherwise freed by pop_all().
  free(parser.next->next);
  free(parser.next);
}

TEST_F(ParserSuite, LoadStackCommaTerminatorFunctionSpaces) {
  char user_input[MAXTOKENLEN];
  const char *probe =
      "uint64_t hash( char *str,  uint64_t seed, size_t len,, )";
  strlcpy(user_input, probe, strlen(probe) + 1);

  // tokenize_secondary_params() in process_secondary_params() overwrites the
  // first comma with NULL, dropping the rest of the string.
  EXPECT_THAT(load_stack(&parser, user_input),
              Eq(strlen(probe) - strlen(", )")));
  show_parser_list(&parser, __LINE__);
  ASSERT_THAT(parser.next, Not(IsNull()));
  EXPECT_THAT(parser.next->next, Not(IsNull()));
  EXPECT_THAT(StdoutMatches("Token number 0 has kind type and string uint64_t"),
              IsTrue());
  EXPECT_THAT(
      StdoutMatches("Token number 1 has kind identifier and string seed"),
      IsTrue());
  EXPECT_THAT(StdoutMatches("Token number 0 has kind type and string char"),
              IsTrue());
  EXPECT_THAT(StdoutMatches("Token number 1 has kind qualifier and string *"),
              IsTrue());
  EXPECT_THAT(
      StdoutMatches("Token number 2 has kind identifier and string str"),
      IsTrue());
  EXPECT_THAT(StdoutMatches("Token number 0 has kind type and string uint64_t"),
              IsTrue());
  EXPECT_THAT(
      StdoutMatches("Token number 1 has kind identifier and string hash"),
      IsTrue());
  // Otherwise freed by pop_all().
  free_all_parsers(&parser);
}

TEST_F(ParserSuite, LoadStackArrayNoLength) {
  char user_input[MAXTOKENLEN];
  const char *probe = "double val[]";
  strlcpy(user_input, probe, strlen(probe) + 1);
  std::size_t consumed = load_stack(&parser, user_input);
  //  First '[' is consumed; "]" is not. */
  EXPECT_THAT(consumed, Eq(strlen(probe) - 1));
  EXPECT_THAT(parser.array_dimensions, Eq(1));
  EXPECT_THAT(parser.array_lengths, Eq(0));
  EXPECT_THAT(StdoutMatches("Token number 0 has kind type and string double"),
              IsTrue());
  EXPECT_THAT(
      StdoutMatches("Token number 1 has kind identifier and string val"),
      IsTrue());
  showstack(&parser.stack[0], parser.stacklen, stdout, __LINE__);
}

TEST_F(ParserSuite, LoadStackArrayLength) {
  char user_input[MAXTOKENLEN];
  const char *probe = "double val[111]";
  strlcpy(user_input, probe, strlen(probe) + 1);
  std::size_t consumed = load_stack(&parser, user_input);
  // In load_stack(), "]" is not consumed. */
  EXPECT_THAT(consumed, Eq(strlen(probe) - 1));
  EXPECT_THAT(parser.array_dimensions, Eq(1));
  EXPECT_THAT(parser.array_lengths, Eq(1));
  EXPECT_THAT(StdoutMatches("Token number 0 has kind type and string double"),
              IsTrue());
  EXPECT_THAT(StdoutMatches("Token number 1 has kind length and string 111"),
              IsTrue());
  EXPECT_THAT(
      StdoutMatches("Token number 2 has kind identifier and string val"),
      IsTrue());
  showstack(&parser.stack[0], parser.stacklen, stdout, __LINE__);
}

TEST_F(ParserSuite, LoadStackTwoDimArrayOneLength) {
  char user_input[MAXTOKENLEN];
  const char *probe = "double val[1][]";
  strlcpy(user_input, probe, strlen(probe) + 1);
  // Only the first '[' is consumed.  "[]" terminates processing.
  // As noted in process_array_dimension(), "return without advancing
  // the cursor."
  EXPECT_THAT(load_stack(&parser, user_input),
              Eq(strlen(probe) - strlen("][]")));
  EXPECT_THAT(parser.array_dimensions, Eq(2));
  EXPECT_THAT(parser.array_lengths, Eq(1));
  showstack(&parser.stack[0], parser.stacklen, stdout, __LINE__);
}

TEST_F(ParserSuite, LoadStackTwoDimArrayTwoLengths) {
  char user_input[MAXTOKENLEN];
  const char *probe = "double val[8][4]";
  strlcpy(user_input, probe, strlen(probe) + 1);
  EXPECT_THAT(load_stack(&parser, user_input), Eq(strlen(probe) - 1));
  EXPECT_THAT(parser.array_dimensions, Eq(2));
  EXPECT_THAT(parser.array_lengths, Eq(2));
  EXPECT_THAT(StdoutMatches("Token number 0 has kind type and string double"),
              IsTrue());
  EXPECT_THAT(StdoutMatches("Token number 1 has kind length and string 4"),
              IsTrue());
  EXPECT_THAT(StdoutMatches("Token number 2 has kind length and string 8"),
              IsTrue());
  EXPECT_THAT(
      StdoutMatches("Token number 3 has kind identifier and string val"),
      IsTrue());
  showstack(&parser.stack[0], parser.stacklen, stdout, __LINE__);
}

TEST_F(ParserSuite, LoadStackThreeDimArrayTwoLengths) {
  char user_input[MAXTOKENLEN];
  const char *probe = "double val[8][4]";
  strlcpy(user_input, probe, strlen(probe) + 1);
  EXPECT_THAT(load_stack(&parser, user_input), Eq(strlen(probe) - 1));
  EXPECT_THAT(parser.array_dimensions, Eq(2));
  EXPECT_THAT(parser.array_lengths, Eq(2));
  EXPECT_THAT(StdoutMatches("Token number 0 has kind type and string double"),
              IsTrue());
  EXPECT_THAT(StdoutMatches("Token number 1 has kind length and string 4"),
              IsTrue());
  EXPECT_THAT(StdoutMatches("Token number 2 has kind length and string 8"),
              IsTrue());
  EXPECT_THAT(
      StdoutMatches("Token number 3 has kind identifier and string val"),
      IsTrue());
  showstack(&parser.stack[0], parser.stacklen, stdout, __LINE__);
}

TEST_F(ParserSuite, LoadStackThreeDimArrayThreeLengths) {
  char user_input[MAXTOKENLEN];
  const char *probe = "double val[8][4]";
  strlcpy(user_input, probe, strlen(probe) + 1);
  EXPECT_THAT(load_stack(&parser, user_input), Eq(strlen(probe) - 1));
  EXPECT_THAT(parser.array_dimensions, Eq(2));
  EXPECT_THAT(parser.array_lengths, Eq(2));
  EXPECT_THAT(StdoutMatches("Token number 0 has kind type and string double"),
              IsTrue());
  EXPECT_THAT(StdoutMatches("Token number 1 has kind length and string 4"),
              IsTrue());
  EXPECT_THAT(StdoutMatches("Token number 2 has kind length and string 8"),
              IsTrue());
  EXPECT_THAT(
      StdoutMatches("Token number 3 has kind identifier and string val"),
      IsTrue());
  showstack(&parser.stack[0], parser.stacklen, stdout, __LINE__);
}

TEST_F(ParserSuite, LoadStackBadArray) {
  char user_input[MAXTOKENLEN];
  const char *probe = "double val[42";
  strlcpy(user_input, probe, strlen(probe) + 1);
  std::size_t consumed = load_stack(&parser, user_input);
  EXPECT_THAT(consumed, Eq(0));
}

TEST_F(ParserSuite, LegalEnumForwardDeclaration) {
  char user_input[MAXTOKENLEN];
  const char *probe = "enum State state";
  strlcpy(user_input, probe, strlen(probe) + 1);
  std::size_t consumed = load_stack(&parser, user_input);
  EXPECT_THAT(consumed, Eq(strlen(probe)));
  EXPECT_THAT(parser.is_enum, IsTrue());
  EXPECT_THAT(
      StdoutMatches("Token number 0 has kind type and string enum State"),
      IsTrue());
  EXPECT_THAT(
      StdoutMatches("Token number 1 has kind identifier and string state"),
      IsTrue());
}

TEST_F(ParserSuite, TypedefEnumForwardDeclaration) {
  char user_input[MAXTOKENLEN];
  const char *probe = "typedef enum State state";
  strlcpy(user_input, probe, strlen(probe) + 1);
  std::size_t consumed = load_stack(&parser, user_input);
  EXPECT_THAT(consumed, Eq(strlen(probe)));
  EXPECT_THAT(parser.is_enum, IsTrue());
  EXPECT_THAT(
      StdoutMatches("Token number 0 has kind type and string enum State"),
      IsTrue());
  EXPECT_THAT(
      StdoutMatches("Token number 1 has kind identifier and string state"),
      IsTrue());
}
TEST_F(ParserSuite, IllegalEnumForwardDeclaration) {
  char user_input[MAXTOKENLEN];
  const char *probe = "enum State";
  strlcpy(user_input, probe, strlen(probe) + 1);
  std::size_t consumed = load_stack(&parser, user_input);
  EXPECT_THAT(consumed, Eq(0));
  EXPECT_THAT(StderrMatches("Enums cannot be forward-declared."), IsTrue());
  EXPECT_THAT(parser.is_enum, IsFalse());
}

TEST_F(ParserSuite, LoadStackOneEnumeratorNoIdentifier) {
  char user_input[MAXTOKENLEN];
  const char *probe = "enum State {GAS}";
  strlcpy(user_input, probe, strlen(probe) + 1);
  std::size_t consumed = load_stack(&parser, user_input);
  EXPECT_THAT(consumed, Eq(strlen(probe)));
  EXPECT_THAT(parser.is_enum, IsTrue());
  EXPECT_THAT(parser.stacklen, Eq(1));
  EXPECT_THAT(
      StdoutMatches("Token number 0 has kind type and string enum State"),
      IsTrue());
  EXPECT_THAT(parser.enumerator_list, StrEq("GAS"));
}

TEST_F(ParserSuite, LoadStackOneEnumeratorWithIdentifier) {
  char user_input[MAXTOKENLEN];
  const char *probe = "enum State state {GAS}";
  strlcpy(user_input, probe, strlen(probe) + 1);
  std::size_t consumed = load_stack(&parser, user_input);
  EXPECT_THAT(consumed, Eq(strlen(probe)));
  EXPECT_THAT(parser.is_enum, IsTrue());
  EXPECT_THAT(parser.stacklen, Eq(2));
  EXPECT_THAT(
      StdoutMatches("Token number 0 has kind type and string enum State"),
      IsTrue());
  EXPECT_THAT(
      StdoutMatches("Token number 1 has kind identifier and string state"),
      IsTrue());
  EXPECT_THAT(parser.enumerator_list, StrEq("GAS"));
}

TEST_F(ParserSuite, LoadStackOneEnumeratorWithTrailingIdentifier) {
  char user_input[MAXTOKENLEN];
  const char *probe = "enum State {GAS} state";
  strlcpy(user_input, probe, strlen(probe) + 1);
  std::size_t consumed = load_stack(&parser, user_input);
  EXPECT_THAT(consumed, Eq(strlen(probe)));
  EXPECT_THAT(parser.is_enum, IsTrue());
  EXPECT_THAT(parser.stacklen, Eq(2));
  EXPECT_THAT(
      StdoutMatches("Token number 0 has kind type and string enum State"),
      IsTrue());
  EXPECT_THAT(
      StdoutMatches("Token number 1 has kind identifier and string state"),
      IsTrue());
  EXPECT_THAT(parser.enumerator_list, StrEq("GAS"));
}

TEST_F(ParserSuite, LoadStackSimpleStruct) {
  char user_input[MAXTOKENLEN];
  const char *probe = "struct node nodelist { int payload; struct node *next;}";
  strlcpy(user_input, probe, strlen(probe) + 1);
  std::size_t consumed = load_stack(&parser, user_input);
  // -1 because the code returns when '}' is reached.
  EXPECT_THAT(consumed, Eq(strlen(probe) - 1));
  EXPECT_THAT(parser.has_struct_members, IsTrue());
  EXPECT_THAT(parser.stacklen, Eq(2));
  EXPECT_THAT(
      StdoutMatches("Token number 0 has kind type and string struct node"),
      IsTrue());
  EXPECT_THAT(
      StdoutMatches("Token number 1 has kind identifier and string nodelist"),
      IsTrue());

  ASSERT_THAT(parser.next, Not(IsNull()));
  EXPECT_THAT(parser.next->stacklen, Eq(2));
  EXPECT_THAT(StdoutMatches("Token number 0 has kind type and string int"),
              IsTrue());
  EXPECT_THAT(
      StdoutMatches("Token number 1 has kind identifier and string payload"),
      IsTrue());

  ASSERT_THAT(parser.next->next, Not(IsNull()));
  EXPECT_THAT(parser.next->next->stacklen, Eq(3));
  EXPECT_THAT(
      StdoutMatches("Token number 0 has kind type and string struct node"),
      IsTrue());
  EXPECT_THAT(StdoutMatches("Token number 1 has kind qualifier and string *"),
              IsTrue());
  EXPECT_THAT(
      StdoutMatches("Token number 2 has kind identifier and string next"),
      IsTrue());
  free_all_parsers(&parser);
}

TEST_F(ParserSuite, LoadStackStructTrailingInstanceName) {
  char user_input[MAXTOKENLEN];
  const char *probe = "struct node { int payload; struct node *next;} nodelist";
  strlcpy(user_input, probe, strlen(probe) + 1);
  std::size_t consumed = load_stack(&parser, user_input);
  // -1 because final ';' is not counted.
  EXPECT_THAT(consumed, Eq(strlen(probe)));
  EXPECT_THAT(parser.has_struct_members, IsTrue());
  EXPECT_THAT(parser.stacklen, Eq(2));
  EXPECT_THAT(
      StdoutMatches("Token number 0 has kind type and string struct node"),
      IsTrue());
  EXPECT_THAT(
      StdoutMatches("Token number 1 has kind identifier and string nodelist"),
      IsTrue());

  ASSERT_THAT(parser.next, Not(IsNull()));
  EXPECT_THAT(parser.next->stacklen, Eq(2));
  EXPECT_THAT(StdoutMatches("Token number 0 has kind type and string int"),
              IsTrue());
  EXPECT_THAT(
      StdoutMatches("Token number 1 has kind identifier and string payload"),
      IsTrue());

  ASSERT_THAT(parser.next->next, Not(IsNull()));
  EXPECT_THAT(parser.next->next->stacklen, Eq(3));
  EXPECT_THAT(
      StdoutMatches("Token number 0 has kind type and string struct node"),
      IsTrue());
  EXPECT_THAT(StdoutMatches("Token number 1 has kind qualifier and string *"),
              IsTrue());
  EXPECT_THAT(
      StdoutMatches("Token number 2 has kind identifier and string next"),
      IsTrue());
  free_all_parsers(&parser);
}

TEST_F(ParserSuite, LoadStackStructNoInstanceName) {
  char user_input[MAXTOKENLEN];
  const char *probe = "struct node { int payload; struct node *next;}";
  strlcpy(user_input, probe, strlen(probe) + 1);
  std::size_t consumed = load_stack(&parser, user_input);
  // -1 because '}' is not counted.
  EXPECT_THAT(consumed, Eq(strlen(probe)));
  EXPECT_THAT(parser.has_struct_members, IsTrue());
  EXPECT_THAT(parser.stacklen, Eq(1));
  EXPECT_THAT(
      StdoutMatches("Token number 0 has kind type and string struct node"),
      IsTrue());

  ASSERT_THAT(parser.next, Not(IsNull()));
  EXPECT_THAT(parser.next->stacklen, Eq(2));
  EXPECT_THAT(StdoutMatches("Token number 0 has kind type and string int"),
              IsTrue());
  EXPECT_THAT(
      StdoutMatches("Token number 1 has kind identifier and string payload"),
      IsTrue());

  ASSERT_THAT(parser.next->next, Not(IsNull()));
  EXPECT_THAT(parser.next->next->stacklen, Eq(3));
  EXPECT_THAT(
      StdoutMatches("Token number 0 has kind type and string struct node"),
      IsTrue());
  EXPECT_THAT(StdoutMatches("Token number 1 has kind qualifier and string *"),
              IsTrue());
  EXPECT_THAT(
      StdoutMatches("Token number 2 has kind identifier and string next"),
      IsTrue());
  free_all_parsers(&parser);
}

TEST_F(ParserSuite, LoadStackStructNoInstanceNameMissingLeadingSpace) {
  char user_input[MAXTOKENLEN];
  const char *probe = "struct node{ int payload; struct node *next;}";
  strlcpy(user_input, probe, strlen(probe) + 1);
  std::size_t consumed = load_stack(&parser, user_input);
  EXPECT_THAT(consumed, Eq(0));
  EXPECT_THAT(parser.stacklen, Eq(0));
}

TEST_F(ParserSuite, LoadStackStructExcessidentifier) {
  char user_input[MAXTOKENLEN];
  const char *probe = "struct node { int payload foo; struct node *next;}";
  strlcpy(user_input, probe, strlen(probe) + 1);
  std::size_t consumed = load_stack(&parser, user_input);
  EXPECT_THAT(consumed, Eq(0));
  EXPECT_THAT(parser.stacklen, Eq(0));
}

TEST_F(ParserSuite, LoadStackStructExcessType) {
  char user_input[MAXTOKENLEN];
  const char *probe = "struct node { int enum payload; struct node *next;}";
  strlcpy(user_input, probe, strlen(probe) + 1);
  std::size_t consumed = load_stack(&parser, user_input);
  parser.err_stream = stderr;
  showstack(&parser.stack[0], parser.stacklen, stdout, __LINE__);
  EXPECT_THAT(consumed, Eq(0));
  EXPECT_THAT(parser.stacklen, Eq(0));
}

TEST_F(ParserSuite, LoadStackEnumInstanceNameMissingLeadingSpace) {
  char user_input[MAXTOKENLEN];
  const char *probe = "enum State state{GAS}";
  strlcpy(user_input, probe, strlen(probe) + 1);
  std::size_t consumed = load_stack(&parser, user_input);
  parser.err_stream = stderr;
  showstack(&parser.stack[0], parser.stacklen, stdout, __LINE__);
  EXPECT_THAT(consumed, Eq(0));
  EXPECT_THAT(parser.stacklen, Eq(0));
}

TEST_F(ParserSuite, LoadStackTypedef) {
  char user_input[MAXTOKENLEN];
  const char *probe = "typedef int mm_id_t";
  strlcpy(user_input, probe, strlen(probe) + 1);
  std::size_t consumed = load_stack(&parser, user_input);
  EXPECT_THAT(consumed, Eq(strlen(probe)));
  EXPECT_THAT(parser.is_typedef, IsTrue());
  EXPECT_THAT(parser.stacklen, Eq(2));
  EXPECT_THAT(StdoutMatches("Token number 0 has kind type and string int"),
              IsTrue());
  EXPECT_THAT(
      StdoutMatches("Token number 1 has kind identifier and string mm_id_t"),
      IsTrue());
}

TEST_F(ParserSuite, LoadStackFunctionPtrOneParamWithIdentifier) {
  char user_input[MAXTOKENLEN];
  const char *probe = "int (*open) (struct inode *blk);";
  strlcpy(user_input, probe, strlen(probe) + 1);
  std::size_t consumed = load_stack(&parser, user_input);
  // -1 due to semicolon.
  EXPECT_THAT(consumed, Eq(strlen(probe) - 1));
  EXPECT_THAT(StdoutMatches("Token number 0 has kind type and string int"),
              IsTrue());
  EXPECT_THAT(StdoutMatches("Token number 1 has kind qualifier and string *"),
              IsTrue());
  EXPECT_THAT(
      StdoutMatches("Token number 2 has kind identifier and string open"),
      IsTrue());
  EXPECT_THAT(
      StdoutMatches("Token number 0 has kind type and string struct inode"),
      IsTrue());
  EXPECT_THAT(StdoutMatches("Token number 1 has kind qualifier and string *"),
              IsTrue());
  EXPECT_THAT(
      StdoutMatches("Token number 2 has kind identifier and string blk"),
      IsTrue());
  free_all_parsers(&parser);
}

TEST_F(ParserSuite, LoadStackFunctionPtrOneParamNoIdentifier) {
  char user_input[MAXTOKENLEN];
  const char *probe = "int (*open) (struct inode *);";
  strlcpy(user_input, probe, strlen(probe) + 1);
  std::size_t consumed = load_stack(&parser, user_input);
  EXPECT_THAT(consumed, Eq(strlen(probe) - 1));
  EXPECT_THAT(StdoutMatches("Token number 0 has kind type and string int"),
              IsTrue());
  EXPECT_THAT(StdoutMatches("Token number 1 has kind qualifier and string *"),
              IsTrue());
  EXPECT_THAT(
      StdoutMatches("Token number 2 has kind identifier and string open"),
      IsTrue());
  EXPECT_THAT(
      StdoutMatches("Token number 0 has kind type and string struct inode"),
      IsTrue());
  EXPECT_THAT(StdoutMatches("Token number 1 has kind qualifier and string *"),
              IsTrue());
  free_all_parsers(&parser);
}

TEST_F(ParserSuite, LoadStackFunctionPtrTwoParamsWithIdentifiers) {
  char user_input[MAXTOKENLEN];
  const char *probe = "int (*open) (struct inode *blk, struct file *dir);";
  strlcpy(user_input, probe, strlen(probe) + 1);
  std::size_t consumed = load_stack(&parser, user_input);
  EXPECT_THAT(consumed, Eq(strlen(probe) - 1));
  EXPECT_THAT(StdoutMatches("Token number 0 has kind type and string int"),
              IsTrue());
  EXPECT_THAT(StdoutMatches("Token number 1 has kind qualifier and string *"),
              IsTrue());
  EXPECT_THAT(
      StdoutMatches("Token number 2 has kind identifier and string open"),
      IsTrue());
  EXPECT_THAT(
      StdoutMatches("Token number 0 has kind type and string struct inode"),
      IsTrue());
  EXPECT_THAT(StdoutMatches("Token number 1 has kind qualifier and string *"),
              IsTrue());
  EXPECT_THAT(
      StdoutMatches("Token number 2 has kind identifier and string blk"),
      IsTrue());
  EXPECT_THAT(
      StdoutMatches("Token number 0 has kind type and string struct file"),
      IsTrue());
  EXPECT_THAT(StdoutMatches("Token number 1 has kind qualifier and string *"),
              IsTrue());
  EXPECT_THAT(
      StdoutMatches("Token number 2 has kind identifier and string dir"),
      IsTrue());
  free_all_parsers(&parser);
}

TEST_F(ParserSuite, LoadStackFunctionPtrTwoParamsNoIdentifiers) {
  char user_input[MAXTOKENLEN];
  const char *probe = "int (*open) (struct inode *, struct file *);";
  strlcpy(user_input, probe, strlen(probe) + 1);
  std::size_t consumed = load_stack(&parser, user_input);
  EXPECT_THAT(consumed, Eq(strlen(probe) - 1));
  EXPECT_THAT(StdoutMatches("Token number 0 has kind type and string int"),
              IsTrue());
  EXPECT_THAT(StdoutMatches("Token number 1 has kind qualifier and string *"),
              IsTrue());
  EXPECT_THAT(
      StdoutMatches("Token number 2 has kind identifier and string open"),
      IsTrue());
  EXPECT_THAT(
      StdoutMatches("Token number 0 has kind type and string struct inode"),
      IsTrue());
  EXPECT_THAT(StdoutMatches("Token number 1 has kind qualifier and string *"),
              IsTrue());
  EXPECT_THAT(
      StdoutMatches("Token number 0 has kind type and string struct file"),
      IsTrue());
  EXPECT_THAT(StdoutMatches("Token number 1 has kind qualifier and string *"),
              IsTrue());
  free_all_parsers(&parser);
}

TEST_F(ParserSuite, LoadStackTwoFunctionPtrsInStructNoFunctionParams) {
  char user_input[MAXTOKENLEN];
  const char *probe =
      "struct file_operations {int (*open)(); ssize_t (*read)(); };";
  strlcpy(user_input, probe, strlen(probe) + 1);
  std::size_t consumed = load_stack(&parser, user_input);
  EXPECT_THAT(consumed, Eq(strlen(probe) - 1));
  EXPECT_THAT(StdoutMatches("Token number 0 has kind type and string int"),
              IsTrue());
  EXPECT_THAT(StdoutMatches("Token number 1 has kind qualifier and string *"),
              IsTrue());
  EXPECT_THAT(
      StdoutMatches("Token number 2 has kind identifier and string open"),
      IsTrue());
  EXPECT_THAT(StdoutMatches("Token number 0 has kind type and string ssize_t"),
              IsTrue());
  EXPECT_THAT(StdoutMatches("Token number 1 has kind qualifier and string *"),
              IsTrue());
  EXPECT_THAT(
      StdoutMatches("Token number 2 has kind identifier and string read"),
      IsTrue());
  EXPECT_THAT(
      StdoutMatches(
          "Token number 0 has kind type and string struct file_operations"),
      IsTrue());
  ASSERT_THAT(parser.next, Not(IsNull()));
  ASSERT_THAT(parser.next->next, Not(IsNull()));
  // Assure that there are no unexpected parsers producing garbage on the
  // output.
  EXPECT_THAT(parser.next->next->next, IsNull());
  free_all_parsers(&parser);
}

TEST_F(ParserSuite, LoadStackTwoFunctionPtrsInStructTrailingFunctionParam) {
  char user_input[MAXTOKENLEN];
  const char *probe =
      "struct f {int (*open)(); ssize_t (*read)(struct inode *); };";
  strlcpy(user_input, probe, strlen(probe) + 1);
  std::size_t consumed = load_stack(&parser, user_input);
  EXPECT_THAT(consumed, Eq(strlen(probe) - 1));
  EXPECT_THAT(StdoutMatches("Token number 0 has kind type and string int"),
              IsTrue());
  EXPECT_THAT(StdoutMatches("Token number 1 has kind qualifier and string *"),
              IsTrue());
  EXPECT_THAT(
      StdoutMatches("Token number 2 has kind identifier and string open"),
      IsTrue());
  EXPECT_THAT(
      StdoutMatches("Token number 0 has kind type and string struct inode"),
      IsTrue());
  EXPECT_THAT(StdoutMatches("Token number 1 has kind qualifier and string *"),
              IsTrue());
  EXPECT_THAT(StdoutMatches("Token number 0 has kind type and string ssize_t"),
              IsTrue());
  EXPECT_THAT(StdoutMatches("Token number 1 has kind qualifier and string *"),
              IsTrue());
  EXPECT_THAT(
      StdoutMatches("Token number 2 has kind identifier and string read"),
      IsTrue());
  EXPECT_THAT(StdoutMatches("Token number 0 has kind type and string struct f"),
              IsTrue());
  ASSERT_THAT(parser.next, Not(IsNull()));
  ASSERT_THAT(parser.next->next, Not(IsNull()));
  EXPECT_THAT(parser.next->next->next, Not(IsNull()));
  // Assure that there are no unexpected parsers producing garbage on the
  // output.
  EXPECT_THAT(parser.next->next->next->next, IsNull());
  free_all_parsers(&parser);
}

TEST_F(ParserSuite, LoadStackReorder) {
  char user_input[MAXTOKENLEN];
  const char *probe = "const int x";
  strlcpy(user_input, probe, strlen(probe) + 1);
  std::size_t consumed = load_stack(&parser, user_input);
  EXPECT_THAT(consumed, Eq(strlen(probe)));
  EXPECT_THAT(StdoutMatches("Token number 0 has kind type and string int"),
              IsTrue());
  EXPECT_THAT(
      StdoutMatches("Token number 1 has kind qualifier and string const"),
      IsTrue());
  EXPECT_THAT(StdoutMatches("Token number 2 has kind identifier and string x"),
              IsTrue());
}

TEST_F(ParserSuite, ParseSimpleExpression) {
  char inputstr[] = "int x;";
  EXPECT_THAT(input_parsing_successful(&parser, inputstr), IsTrue());
  // The output has a trailng space in case there's output after the type.
  EXPECT_THAT(StdoutMatches("x is a(n) int "), IsTrue());
}

TEST_F(ParserSuite, ParsePtrExpression) {
  char inputstr[] = "int *x;";
  EXPECT_THAT(input_parsing_successful(&parser, inputstr), IsTrue());
  EXPECT_THAT(StdoutMatches("x is a(n) pointer to int "), IsTrue());
}

TEST_F(ParserSuite, ParsePtrExpressionWithSpace) {
  char inputstr[] = "int * x;";
  EXPECT_THAT(input_parsing_successful(&parser, inputstr), IsTrue());
  EXPECT_THAT(StdoutMatches("x is a(n) pointer to int "), IsTrue());
}

TEST_F(ParserSuite, ParsePtrExpressionAlternate) {
  char inputstr[] = "int* x;";
  EXPECT_THAT(input_parsing_successful(&parser, inputstr), IsTrue());
  EXPECT_THAT(StdoutMatches("x is a(n) pointer to int "), IsTrue());
}

TEST_F(ParserSuite, ParsePtrPtrExpression) {
  char inputstr[] = "int **x;";
  EXPECT_THAT(input_parsing_successful(&parser, inputstr), IsTrue());
  EXPECT_THAT(StdoutMatches("x is a(n) pointer to pointer to int "), IsTrue());
}

TEST_F(ParserSuite, ParsePtrPtrExpressionSpace) {
  char inputstr[] = "int * *x;";
  EXPECT_THAT(input_parsing_successful(&parser, inputstr), IsTrue());
  EXPECT_THAT(StdoutMatches("x is a(n) pointer to pointer to int "), IsTrue());
}

TEST_F(ParserSuite, ParseRestrictedPtrExpression) {
  char inputstr[] = "int * restrict x;";
  EXPECT_THAT(input_parsing_successful(&parser, inputstr), IsTrue());
  EXPECT_THAT(parser.is_pointer, IsTrue());
  EXPECT_THAT(parser.have_identifier, IsTrue());
  EXPECT_THAT(StdoutMatches("x is a(n) restrict pointer to int "), IsTrue());
}

TEST_F(ParserSuite, ParseRestrictedPtrWithQualifierExpression) {
  char inputstr[] = "const int * restrict x;";
  EXPECT_THAT(input_parsing_successful(&parser, inputstr), IsFalse());
  EXPECT_THAT(parser.is_pointer, IsFalse());
}

TEST_F(ParserSuite, ParseQualfiedExpression) {
  char inputstr[] = "const int x;";
  EXPECT_THAT(input_parsing_successful(&parser, inputstr), IsTrue());
  EXPECT_THAT(StdoutMatches("x is a(n) const int "), IsTrue());
}

TEST_F(ParserSuite, ParseConstPtr) {
  char inputstr[] = "int * const x;";
  EXPECT_THAT(input_parsing_successful(&parser, inputstr), IsTrue());
  EXPECT_THAT(StdoutMatches("x is a(n) const pointer to int "), IsTrue());
}

// From https://en.cppreference.com/w/c/language/pointer.html
TEST_F(ParserSuite, ParseConstPtrPtr) {
  char inputstr[] = "int *const *npp = &np;";
  EXPECT_THAT(input_parsing_successful(&parser, inputstr), IsTrue());
  // Website says "non-const pointer to const pointer to non-const int""
  EXPECT_THAT(StdoutMatches("npp is a(n) pointer to const pointer to int"),
              IsTrue());
}

// From https://en.cppreference.com/w/c/language/pointer.html
TEST_F(ParserSuite, ParsePtrArray) {
  char inputstr[] = "int *ap[2] = &a;";
  EXPECT_THAT(input_parsing_successful(&parser, inputstr), IsTrue());
  // Website says "pointer to array of int""
  EXPECT_THAT(StdoutMatches("ap is a(n) array of 2 pointer to int"), IsTrue());
}

TEST_F(ParserSuite, ParseSimpleArray) {
  char inputstr[] = "const double x[]];";
  EXPECT_THAT(input_parsing_successful(&parser, inputstr), IsTrue());
  EXPECT_THAT(parser.array_lengths, Eq(0));
  EXPECT_THAT(StdoutMatches("x is a(n) array of const double "), IsTrue());
}

TEST_F(ParserSuite, ParseArrayWithLength) {
  char inputstr[] = "char val[9];";
  EXPECT_THAT(input_parsing_successful(&parser, inputstr), IsTrue());
  // pop_stack() decrements array_lengths.
  EXPECT_THAT(parser.array_lengths, Eq(0));
  EXPECT_THAT(StdoutMatches("val is a(n) array of 9 char"), IsTrue());
}

TEST_F(ParserSuite, ParseArrayWithTwoDimsOneLength) {
  char inputstr[] = "char val[9][];";
  EXPECT_THAT(input_parsing_successful(&parser, inputstr), IsTrue());
  EXPECT_THAT(parser.array_lengths, Eq(0));
  EXPECT_THAT(StdoutMatches("val is a(n) array of 9x? char"), IsTrue());
}

TEST_F(ParserSuite, ParseArrayWithTwoLengths) {
  char inputstr[] = "char val[9][11];";
  EXPECT_THAT(input_parsing_successful(&parser, inputstr), IsTrue());
  EXPECT_THAT(parser.array_lengths, Eq(0));
  EXPECT_THAT(StdoutMatches("val is a(n) array of 9x11 char"), IsTrue());
}

TEST_F(ParserSuite, ParseArrayWithThreeDimTwoLengths) {
  char inputstr[] = "char val[9][11][];";
  EXPECT_THAT(input_parsing_successful(&parser, inputstr), IsTrue());
  EXPECT_THAT(parser.array_lengths, Eq(0));
  EXPECT_THAT(StdoutMatches("val is a(n) array of 9x11x? char"), IsTrue());
}

TEST_F(ParserSuite, ParseArrayWithThreeLengths) {
  char inputstr[] = "char val[9][11][6];";
  EXPECT_THAT(input_parsing_successful(&parser, inputstr), IsTrue());

  EXPECT_THAT(parser.array_lengths, Eq(0));
  EXPECT_THAT(StdoutMatches("val is a(n) array of 9x11x6 char"), IsTrue());
}

TEST_F(ParserSuite, ParseArrayWithBadLength) {
  char inputstr[] = "char val[9;";
  EXPECT_THAT(input_parsing_successful(&parser, inputstr), IsFalse());
  EXPECT_THAT(StderrMatches("Input lacks required identifier or type element"),
              IsTrue());
}

TEST_F(ParserSuite, ParseSimpleFunctionOutput) {
  char inputstr[] = "double sqrt();";
  EXPECT_THAT(input_parsing_successful(&parser, inputstr), IsTrue());
  EXPECT_THAT(parser.has_function_params, IsFalse());
  EXPECT_THAT(StdoutMatches("sqrt is a(n) function which returns double"),
              IsTrue());
}

TEST_F(ParserSuite, ParseFunctionOutputOneParam) {
  char inputstr[] = "double sqrt(const double x);";
  EXPECT_THAT(input_parsing_successful(&parser, inputstr), IsTrue());
  EXPECT_THAT(parser.has_function_params, IsTrue());
  // clang-format off
  EXPECT_THAT(StdoutMatches("sqrt is a(n) function which returns double and takes param(s) x is a(n) const double"),
              IsTrue());
  // clang-format on
}

TEST_F(ParserSuite, ParseFunctionOutputOneParamQualifier) {
  char inputstr[] = "volatile double sqrt(const double x);";
  EXPECT_THAT(input_parsing_successful(&parser, inputstr), IsTrue());
  EXPECT_THAT(parser.has_function_params, IsTrue());
  // clang-format off
  EXPECT_THAT(StdoutMatches("sqrt is a(n) function which returns volatile double and takes param(s) x is a(n) const double"),
              IsTrue());
  // clang-format on
}

TEST_F(ParserSuite, ParseFunctionOutputTwoParams) {
  char inputstr[] = "uint64_t hash(char *key, uint64_t seed);";
  EXPECT_THAT(input_parsing_successful(&parser, inputstr), IsTrue());
  EXPECT_THAT(parser.has_function_params, IsTrue());
  // clang-format off
  EXPECT_THAT(StdoutMatches("hash is a(n) function which returns uint64_t and takes param(s) key is a(n) pointer to char and seed is a(n) uint64_t"),
              IsTrue());
  // clang-format on
}

TEST_F(ParserSuite, ParseFunctionOutputLeadingWhitespace) {
  char inputstr[] = "double sqrt(   const double x);";
  EXPECT_THAT(input_parsing_successful(&parser, inputstr), IsTrue());
  EXPECT_THAT(parser.has_function_params, IsTrue());
  // clang-format off
  EXPECT_THAT(StdoutMatches("sqrt is a(n) function which returns double and takes param(s) x is a(n) const double"),
              IsTrue());
  // clang-format on
}

TEST_F(ParserSuite, ParseFunctionOutputNoWhitespace) {
  char inputstr[] = "uint64_t hash(char *key,uint64_t seed);";
  EXPECT_THAT(input_parsing_successful(&parser, inputstr), IsTrue());
  EXPECT_THAT(parser.has_function_params, IsTrue());
  // clang-format off
  EXPECT_THAT(StdoutMatches("hash is a(n) function which returns uint64_t and takes param(s) key is a(n) pointer to char and seed is a(n) uint64_t"),
              IsTrue());
  // clang-format on
}

TEST_F(ParserSuite, ParseFunctionPtrWithOneParamIdentifier) {
  char inputstr[] = "int (*open) (struct file *dir);";
  EXPECT_THAT(input_parsing_successful(&parser, inputstr), IsTrue());
  // clang-format off
  EXPECT_THAT(StdoutMatches("open is a(n) pointer to a function which returns int and takes param(s) dir is a(n) pointer to struct file"),
              IsTrue());
  // clang-format on
}

TEST_F(ParserSuite, ParseFunctionPtrWithTwoParamIdentifiers) {
  char inputstr[] = "int (*open) (struct inode *blk, struct file *dir);";
  EXPECT_THAT(input_parsing_successful(&parser, inputstr), IsTrue());
  // clang-format off
  EXPECT_THAT(StdoutMatches("open is a(n) pointer to a function which returns int and takes param(s) blk is a(n) pointer to struct inode and dir is a(n) pointer to struct file"),
              IsTrue());
  // clang-format on
}

TEST_F(ParserSuite, ParseFunctionPtrWithTwoParamsNoIdentifiers) {
  char inputstr[] = "int (*open) (struct inode *, struct file *);";
  EXPECT_THAT(input_parsing_successful(&parser, inputstr), IsTrue());
  // clang-format off
  EXPECT_THAT(StdoutMatches("open is a(n) pointer to a function which returns int and takes param(s) pointer to struct inode and pointer to struct file"),
              IsTrue());
  // clang-format on
}

TEST_F(ParserSuite, ParseFunctionPtrWithTwoParamsNoIdentifiersVariantSpaces) {
  char inputstr[] = "int (*open)( struct inode *, struct file *);";
  EXPECT_THAT(input_parsing_successful(&parser, inputstr), IsTrue());
  // clang-format off
  EXPECT_THAT(StdoutMatches("open is a(n) pointer to a function which returns int and takes param(s) pointer to struct inode and pointer to struct file"),
              IsTrue());
  // clang-format on
}

TEST_F(ParserSuite, ParseFunctionPtrInStructNoParams) {
  char inputstr[] = "struct file { int (*open)(); };";
  EXPECT_THAT(input_parsing_successful(&parser, inputstr), IsTrue());
  // clang-format off
  EXPECT_THAT(StdoutMatches("struct file has member(s) open is a(n) pointer to a function which returns int"),
              IsTrue());
  // clang-format on
}

TEST_F(ParserSuite, ParseFunctionPtrInStructTwoMembersNoParamsPtrFirst) {
  char inputstr[] = "struct file { int (*open)(); int payload; };";
  EXPECT_THAT(input_parsing_successful(&parser, inputstr), IsTrue());
  // clang-format off
  EXPECT_THAT(StdoutMatches("struct file has member(s) open is a(n) pointer to a function which returns int"),
              IsTrue());
  // clang-format on
}

TEST_F(ParserSuite, ParseFunctionPtrInStructTwoMembersNoParamsPtrLast) {
  char inputstr[] = "struct file { int payload; int (*open)(); };";
  EXPECT_THAT(input_parsing_successful(&parser, inputstr), IsTrue());
  // clang-format off
  EXPECT_THAT(StdoutMatches("struct file has member(s) payload is a(n) int and open is a(n) pointer to a function which returns int"),
              IsTrue());
  // clang-format on
}

TEST_F(ParserSuite, ParseFunctionPtrTwoInStructNoParams) {
  char inputstr[] = "struct file { int (*open)(); int (*read)(); };";
  EXPECT_THAT(input_parsing_successful(&parser, inputstr), IsTrue());
  // clang-format off
  EXPECT_THAT(StdoutMatches("struct file has member(s) open is a(n) pointer to a function which returns int and read is a(n) pointer to a function which returns int"),
              IsTrue());
  // clang-format on
}

TEST_F(ParserSuite, ParseFunctionPtrInStructOneParamWithIdentifier) {
  char inputstr[] = "struct file { int (*open)(struct inode *blk); };";
  EXPECT_THAT(input_parsing_successful(&parser, inputstr), IsTrue());
  // clang-format off
  EXPECT_THAT(StdoutMatches("struct file has member(s) open is a(n) pointer to a function which returns int"),
              IsTrue());
  // clang-format on
}

TEST_F(ParserSuite, ParseFunctionPtrInStructOneParamNoIdentifier) {
  char inputstr[] = "struct file { int (*open)(struct inode *); };";
  EXPECT_THAT(input_parsing_successful(&parser, inputstr), IsTrue());
  // clang-format off
  EXPECT_THAT(StdoutMatches("struct file has member(s) open is a(n) pointer to a function which returns int"),
              IsTrue());
  // clang-format on
}

TEST_F(ParserSuite, ParseFunctionPtrInStructTwoParamsNoIdentifiers) {
  char inputstr[] =
      "struct file { int (*open)(struct inode *, struct file *); };";
  EXPECT_THAT(input_parsing_successful(&parser, inputstr), IsTrue());
  // clang-format off
  EXPECT_THAT(StdoutMatches("struct file has member(s) open is a(n) pointer to a function which returns int and takes param(s) pointer to struct inode and pointer to struct file"),
              IsTrue());
  // clang-format on
}

TEST_F(ParserSuite, ParseFunctionPtrsInStructTrailingFunctionParam) {
  char inputstr[] =
      "struct f {int (*open)(); ssize_t (*read)(struct inode *); };";
  EXPECT_THAT(input_parsing_successful(&parser, inputstr), IsTrue());
  // clang-format off
  EXPECT_THAT(StdoutMatches("struct f has member(s) open is a(n) pointer to a function which returns int and read is a(n) pointer to a function which returns ssize_t and takes param(s) pointer to struct inode "),
              IsTrue());
  // clang-format on
}

TEST_F(ParserSuite, ParseUnionSimpleDeclaration) {
  char inputstr[] = "union msi_domain_cookie;";
  EXPECT_THAT(input_parsing_successful(&parser, inputstr), IsTrue());
  // clang-format off
  EXPECT_THAT(StdoutMatches("msi_domain_cookie is a(n) union"),
              IsTrue());
  // clang-format on
}

TEST_F(ParserSuite, ParseUnionForwardDeclaration) {
  char inputstr[] = "union msi_domain_cookie dcookie;";
  EXPECT_THAT(input_parsing_successful(&parser, inputstr), IsTrue());
  // clang-format off
  EXPECT_THAT(StdoutMatches("dcookie is a(n) union msi_domain_cookie"),
              IsTrue());
  // clang-format on
}

TEST_F(ParserSuite, ParseStructForwardDeclaration) {
  char inputstr[] = "struct list_head list;";
  EXPECT_THAT(input_parsing_successful(&parser, inputstr), IsTrue());
  // clang-format off
  EXPECT_THAT(StdoutMatches("list is a(n) struct list_head"),
              IsTrue());
  // clang-format on
}

TEST_F(ParserSuite, ParseStructForwardDeclarationWhitespace) {
  char inputstr[] = "struct   list_head   list;";
  EXPECT_THAT(input_parsing_successful(&parser, inputstr), IsTrue());
  // clang-format off
  EXPECT_THAT(StdoutMatches("list is a(n) struct list_head"),
              IsTrue());
  // clang-format on
}

TEST_F(ParserSuite, ParseStructForwardDeclarationNoName) {
  char inputstr[] = "struct *;";
  EXPECT_THAT(input_parsing_successful(&parser, inputstr), IsFalse());
  // clang-format off
  EXPECT_THAT(StderrMatches("Input lacks required identifier or type element."),
              IsTrue());
  // clang-format on
}

TEST_F(ParserSuite, ParseEnumWithIdentifierNoEnumerators) {
  char inputstr[] = "enum State state;";
  EXPECT_THAT(input_parsing_successful(&parser, inputstr), IsTrue());
  EXPECT_THAT(StdoutMatches("state is a(n) enum State"), IsTrue());
}

TEST_F(ParserSuite, ParseEnumWithIdentifierOneEnumerator) {
  char inputstr[] = "enum State state {GAS};";
  EXPECT_THAT(input_parsing_successful(&parser, inputstr), IsTrue());
  EXPECT_THAT(StdoutMatches("state is a(n) enum State with enum constant GAS"),
              IsTrue());
}

TEST_F(ParserSuite, ParseEnumWithIdentifierOneEnumeratorTrailingInstanceName) {
  char inputstr[] = "enum State {GAS} state;";
  EXPECT_THAT(input_parsing_successful(&parser, inputstr), IsTrue());
  EXPECT_THAT(StdoutMatches("state is a(n) enum State with enum constant GAS"),
              IsTrue());
}

TEST_F(ParserSuite, ParseEnumWithIdentifierOneEnumeratorDoubleInstanceNames) {
  char inputstr[] = "enum State state {GAS} foo;";
  EXPECT_THAT(input_parsing_successful(&parser, inputstr), IsTrue());
  EXPECT_THAT(StdoutMatches("state is a(n) enum State with enum constant GAS"),
              IsTrue());
}

TEST_F(ParserSuite, ParseEnumNoIdentifierThreeEnumerators) {
  char inputstr[] = "enum State {GAS,LIQUID,SOLID};";
  EXPECT_THAT(input_parsing_successful(&parser, inputstr), IsTrue());
  EXPECT_THAT(StdoutMatches("enum State has enum constant GAS,LIQUID,SOLID"),
              IsTrue());
}

TEST_F(ParserSuite, ParseEnumNoIdentifierOneEnumerator) {
  char inputstr[] = "enum State {GAS};";
  EXPECT_THAT(input_parsing_successful(&parser, inputstr), IsTrue());
  EXPECT_THAT(StdoutMatches("enum State has enum constant GAS"), IsTrue());
}

TEST_F(ParserSuite, ParseEnumNoIdentifierTwoEnumerators) {
  char inputstr[] = "enum State {GAS, LIQUID };";
  EXPECT_THAT(input_parsing_successful(&parser, inputstr), IsTrue());
  EXPECT_THAT(StdoutMatches("enum State has enum constant GAS,LIQUID"),
              IsTrue());
}

TEST_F(ParserSuite, ParseOneEnumeratorMissingLeadingSpaceInstanceName) {
  char inputstr[] = "enum State state{GAS};";
  EXPECT_THAT(input_parsing_successful(&parser, inputstr), IsFalse());
}

TEST_F(ParserSuite, ParseOneEnumeratorMissingLeadingSpaceNoInstanceName) {
  char inputstr[] = "enum State{GAS=1, LIQUID};";
  EXPECT_THAT(input_parsing_successful(&parser, inputstr), IsFalse());
}

TEST_F(ParserSuite, ParseOneEnumeratorMissingLeadingSpaceTrailingInstanceName) {
  char inputstr[] = "enum State{GAS=1, LIQUID} state;";
  EXPECT_THAT(input_parsing_successful(&parser, inputstr), IsFalse());
}

TEST_F(ParserSuite, ParseEnumWithIdentifierBadFormat) {
  char inputstr[] = "enum State state { GAS ,};";
  EXPECT_THAT(input_parsing_successful(&parser, inputstr), IsTrue());
  EXPECT_THAT(StdoutMatches("state is a(n) enum State with enum constant GAS"),
              IsTrue());
}

TEST_F(ParserSuite, ParseEnumNoIdentifierCommaMadness) {
  char inputstr[] = "enum State state { , , ,};";
  EXPECT_THAT(input_parsing_successful(&parser, inputstr), IsFalse());
  EXPECT_THAT(StderrMatches("Enumeration constant list cannot be empty."),
              IsTrue());
}

TEST_F(ParserSuite, ParseForwardDeclarationBadDelim) {
  char inputstr[] = "enum State state {;";
  EXPECT_THAT(input_parsing_successful(&parser, inputstr), IsFalse());
  // clang-format off
  EXPECT_THAT(StderrMatches("Input lacks required identifier or type element."),
              IsTrue());
  // clang-format on
  EXPECT_THAT(parser.is_enum, IsFalse());
}

TEST_F(ParserSuite, ParseForwardDeclarationBadDelim2) {
  char inputstr[] = "enum State } state;";
  EXPECT_THAT(input_parsing_successful(&parser, inputstr), IsFalse());
  // clang-format off
  EXPECT_THAT(StderrMatches("Input lacks required identifier or type element."),
              IsTrue());
  // clang-format on
}

TEST_F(ParserSuite, ParseOneEnumeratorStrayComma) {
  char inputstr[] = "enum State state {,GAS};";
  EXPECT_THAT(input_parsing_successful(&parser, inputstr), IsFalse());
  // clang-format off
  EXPECT_THAT(StderrMatches("Unable to parse garbled input."),
              IsTrue());
  // clang-format on
  EXPECT_THAT(parser.is_enum, IsFalse());
}

TEST_F(ParserSuite, ParseOneEnumeratorWithAssignment) {
  char inputstr[] = "enum State state { GAS=1,};";
  EXPECT_THAT(input_parsing_successful(&parser, inputstr), IsTrue());
  EXPECT_THAT(StdoutMatches("state is a(n) enum State with enum constant GAS"),
              IsTrue());
}

TEST_F(ParserSuite, ParseOneEnumeratorWithEndingAssignment) {
  char inputstr[] = "enum State state {GAS,LIQUID,SOLID=5};";
  EXPECT_THAT(input_parsing_successful(&parser, inputstr), IsTrue());
  EXPECT_THAT(
      StdoutMatches(
          "state is a(n) enum State with enum constant GAS,LIQUID,SOLID"),
      IsTrue());
}

TEST_F(ParserSuite, ParseOneEnumeratorWitMiddleAssignment) {
  char inputstr[] = "enum State state {GAS,LIQUID=4,SOLID};";
  EXPECT_THAT(input_parsing_successful(&parser, inputstr), IsTrue());
  EXPECT_THAT(
      StdoutMatches(
          "state is a(n) enum State with enum constant GAS,LIQUID,SOLID"),
      IsTrue());
}

TEST_F(ParserSuite, ParseOneEnumeratorWithAssignmentNoInstanceName) {
  char inputstr[] = "enum State {GAS=1,};";
  EXPECT_THAT(input_parsing_successful(&parser, inputstr), IsTrue());
  EXPECT_THAT(StdoutMatches("enum State has enum constant GAS"), IsTrue());
}

TEST_F(ParserSuite, ParseTwoEnumeratorWithAssignmentNoInstanceName) {
  char inputstr[] = "enum State {GAS=1,LIQUID};";
  EXPECT_THAT(input_parsing_successful(&parser, inputstr), IsTrue());
  EXPECT_THAT(StdoutMatches("enum State has enum constant GAS,LIQUID"),
              IsTrue());
}

TEST_F(ParserSuite, ParseOneEnumeratorWithTrailingAssignmentNoInstanceName) {
  char inputstr[] = "enum State {GAS,LIQUID=4};";
  EXPECT_THAT(input_parsing_successful(&parser, inputstr), IsTrue());
  EXPECT_THAT(StdoutMatches("enum State has enum constant GAS,LIQUID"),
              IsTrue());
}

TEST_F(ParserSuite, ParseTwoEnumeratorWithAssignmentInstanceNameWhitespace) {
  char inputstr[] = "enum State {GAS,LIQUID=2 ,};";
  EXPECT_THAT(input_parsing_successful(&parser, inputstr), IsTrue());
  EXPECT_THAT(StdoutMatches("enum State has enum constant GAS,LIQUID"),
              IsTrue());
}

TEST_F(ParserSuite,
       ParseTwoEnumeratorWithAssignmentInstanceNameWhitespaceComma) {
  char inputstr[] = "enum State state {GAS, LIQUID = 2,};";
  EXPECT_THAT(input_parsing_successful(&parser, inputstr), IsTrue());
  EXPECT_THAT(
      StdoutMatches("state is a(n) enum State with enum constant GAS,LIQUID"),
      IsTrue());
}

TEST_F(ParserSuite,
       ParseTwoEnumeratorWithAssignmentInstanceNameWhitespaceComma1) {
  char inputstr[] = "enum State {GAS,LIQUID=2 ,};";
  EXPECT_THAT(input_parsing_successful(&parser, inputstr), IsTrue());
  EXPECT_THAT(StdoutMatches("enum State has enum constant GAS,LIQUID"),
              IsTrue());
}

TEST_F(ParserSuite,
       ParseTwoEnumeratorWithAssignmentInstanceNameWhitespaceComma2) {
  char inputstr[] = "enum State {GAS,LIQUID =2,};";
  EXPECT_THAT(input_parsing_successful(&parser, inputstr), IsTrue());
  EXPECT_THAT(StdoutMatches("enum State has enum constant GAS,LIQUID"),
              IsTrue());
}

TEST_F(ParserSuite, ParseStructTwoMembersWithInstanceName) {
  char inputstr[] = "struct node nodelist {int payload; struct node *next;};";
  EXPECT_THAT(input_parsing_successful(&parser, inputstr), IsTrue());
  // clang-format off
  EXPECT_THAT(
      StdoutMatches("nodelist is a(n) struct node which has member(s) payload is a(n) int and next is a(n) pointer to struct node"),
      IsTrue());
  // clang-format on
}

TEST_F(ParserSuite, ParseStructTwoMembersWithEnumName) {
  char inputstr[] =
      "struct node nodelist {enum State state; struct node *next;};";
  EXPECT_THAT(input_parsing_successful(&parser, inputstr), IsTrue());
  // clang-format off
  EXPECT_THAT(
      StdoutMatches("nodelist is a(n) struct node which has member(s) state is a(n) enum State and next is a(n) pointer to struct node"),
      IsTrue());
  // clang-format on
}

TEST_F(ParserSuite, ParseStructTwoMembersTrailingInstanceName) {
  char inputstr[] = "struct node {int payload; struct node *next;} nodelist;";
  EXPECT_THAT(input_parsing_successful(&parser, inputstr), IsTrue());
  // clang-format off
  EXPECT_THAT(
      StdoutMatches("nodelist is a(n) struct node which has member(s) payload is a(n) int and next is a(n) pointer to struct node"),
      IsTrue());
  // clang-format on
  EXPECT_THAT(StdoutMatches("and nodelist is a(n)"), IsFalse());
}

TEST_F(ParserSuite, ParseStructTwoMembersNoInstanceName) {
  char inputstr[] = "struct node {int payload; struct node *next;};";
  EXPECT_THAT(input_parsing_successful(&parser, inputstr), IsTrue());
  // clang-format off
  EXPECT_THAT(StdoutMatches("struct node has member(s) payload is a(n) int and next is a(n) pointer to struct node"),
              IsTrue());
  // clang-format on
}

TEST_F(ParserSuite, ParseStructTwoMembersNoInstanceNameSpaces) {
  char inputstr[] = "struct node  { int payload; struct node *next; };";
  EXPECT_THAT(input_parsing_successful(&parser, inputstr), IsTrue());
  // clang-format off
  EXPECT_THAT(StdoutMatches("struct node has member(s) payload is a(n) int and next is a(n) pointer to struct node"),
              IsTrue());
  // clang-format on
}

TEST_F(ParserSuite, ParseStructTwoMembersTrailingInstanceNameNoSpaces) {
  char inputstr[] = "struct node {ssize_t payload;struct node *next;}nodelist;";
  EXPECT_THAT(input_parsing_successful(&parser, inputstr), IsTrue());
  // clang-format off
  EXPECT_THAT(StdoutMatches("nodelist is a(n) struct node which has member(s) payload is a(n) ssize_t and next is a(n) pointer to struct node"),
              IsTrue());
  // clang-format on
}

TEST_F(ParserSuite, ParseStructComplexMembers) {
  char inputstr[] = "struct message {enum priority prio; uint8_t bytes[8];};";
  EXPECT_THAT(input_parsing_successful(&parser, inputstr), IsTrue());
  // clang-format off
  EXPECT_THAT(StdoutMatches("struct message has member(s) prio is a(n) enum priority and bytes is a(n) array of 8 uint8_t"),
              IsTrue());
  // clang-format on
}

// It seems unnecessary that the parser can read this input, but since it does
// so correctly, there's no need to make it fail.
TEST_F(ParserSuite, ParseStructOneMemberMissingLeadingSpaceInstanceName) {
  char inputstr[] = "struct node nodelist{int payload;struct node *next;};";
  EXPECT_THAT(input_parsing_successful(&parser, inputstr), IsTrue());
  // clang-format off
  EXPECT_THAT(StdoutMatches("nodelist is a(n) struct node which has member(s) payload is a(n) int and next is a(n) pointer to struct node"),
              IsTrue());
  // clang-format on
}

TEST_F(ParserSuite, ParseStructOneMemberMissingLeadingSpaceNoInstanceName) {
  char inputstr[] = "struct node{int payload;};";
  EXPECT_THAT(input_parsing_successful(&parser, inputstr), IsFalse());
}

TEST_F(ParserSuite,
       ParseStructOneMemberMissingLeadingSpaceTrailingInstanceName) {
  char inputstr[] = "struct node{int payload;}nodelist;";
  EXPECT_THAT(input_parsing_successful(&parser, inputstr), IsFalse());
}

TEST_F(ParserSuite, ParseStructOneMemberMissingSemicolonTrailingInstanceName) {
  char inputstr[] = "struct node {int payload} nodelist;";
  EXPECT_THAT(input_parsing_successful(&parser, inputstr), IsFalse());
}

TEST_F(ParserSuite,
       ParseStructOneMemberMissingSecondSemicolonTrailingInstanceName) {
  char inputstr[] = "struct node {int payload; struct node *next} nodelist;";
  EXPECT_THAT(input_parsing_successful(&parser, inputstr), IsFalse());
}

TEST_F(ParserSuite, NothingToLoad) {
  char inputstr[] = "=;";
  EXPECT_THAT(input_parsing_successful(&parser, inputstr), IsFalse());
  EXPECT_THAT(StderrMatches("Input lacks required elements:"), IsTrue());
}

TEST_F(ParserSuite, LotsOfWhitespace) {
  char inputstr[] = "     ";
  EXPECT_THAT(input_parsing_successful(&parser, inputstr), IsFalse());
  EXPECT_THAT(StderrMatches("Input lacks required elements:"), IsTrue());
}

TEST_F(ParserSuite, ParseTypedef) {
  char inputstr[] = "typedef size_t mm_id_t;";
  EXPECT_THAT(input_parsing_successful(&parser, inputstr), IsTrue());
  EXPECT_THAT(StdoutMatches("mm_id_t is a(n) alias for size_t"), IsTrue());
}

TEST_F(ParserSuite, ParseTypedefArray) {
  char inputstr[] = "typedef int A[]];";
  EXPECT_THAT(input_parsing_successful(&parser, inputstr), IsTrue());
  EXPECT_THAT(StdoutMatches("A is a(n) alias for array of int"), IsTrue());
}

TEST_F(ParserSuite, ParseTypedefCompoundType) {
  char inputstr[] = "typedef struct list_head list;";
  EXPECT_THAT(input_parsing_successful(&parser, inputstr), IsTrue());
  // clang-format off
  EXPECT_THAT(StdoutMatches("list is a(n) alias for struct list_head"),
              IsTrue());
  // clang-format on
}

TEST_F(ParserSuite, ParseTypedefFunction) {
  char inputstr[] = "typedef int proc_handler(const struct ctl_table *ctl);";
  EXPECT_THAT(input_parsing_successful(&parser, inputstr), IsTrue());
  // clang-format off
  EXPECT_THAT(
      StdoutMatches("proc_handler is a(n) alias for function which returns int and takes param(s) ctl is a(n) pointer to const struct ctl_table"),
      // clang-format on
      IsTrue());
}
