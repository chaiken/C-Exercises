/******************************************************************************
 *									      *
 *	File:     cdecl.c						      *
 *	Author:   Alison Chaiken <alison@she-devel.com>			      *
 *	Created:  Sun May  4 21:37:27 CEST 2014				      *
 *	Contents: cdecl is my answer to Peter van der Linden's                *
 *       Programming Challenge to "Write a program to translate C              *
 *       declarations into English" in _Expert C Programming: Deep C Secrets_, *
 *       p. 85 in Chapter 3.  The program is also useful, in that it parses    *
 *       C-language declarations and outputs a natural language equivalent.    *
 *       Not all features of a real compiler's parser are supported, as        *
 *       described in the usage() function.                                    *
 *									      *
 *	Copyright (c) 2014 Alison Chaiken.				      *
 *	All rights reserved.						      *
 *									      *
 ******************************************************************************/

#include <asm-generic/errno.h>
#include <assert.h>
#include <bsd/string.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
/* For __fpurge() */
#include <stdio_ext.h>
#include <stdlib.h>
#include <string.h>

#include "cdecl-internal.h"

/********** cleanup function **********/

/*
 * generalized cleanup function for heap allocations copied from systemd's
 * src/boot/util.h
 *
 * The more obvious
 *     free(p);
 * results in ASAN complaining
 *     ""attempting free on address which was not malloc()-ed""
 * The reason is that the macro is passing a void** pointer, namely &p.
 * Copilot explains,
 *   "the compiler arranges for freep(&var) to be called at scope exit. That
 *    means p inside freep is not the heap pointer itself, but the address of
 *    the local variable copy (a char **)."
 * Without the cast,
 *    "free(p) is freeing the stack address of var, not the heap block returned
 *     by strdup. AddressSanitizer catches this as an invalid free."
 *
 */
static inline void freep(void *p) {
  if ((!p) || (NULL == *(void **)p))
    return;
  free(*(void **)p);
  *(void **)p = NULL;
}

/*
 * subsidiary_parsers_cleanup() is the error handler for
 * process_secondary_params(). It frees subsidiary parsers
 * linked through the top-level one and resets the top-level one to
 * signal failure to calling code.
 */
static inline void subsidiary_parsers_cleanup(void *parserp) {
  if (!parserp || !(*(struct parser_props ***)parserp) ||
      !(**(struct parser_props ***)parserp))
    return;
  struct parser_props *parser = **((struct parser_props ***)parserp);
  release_parser_resources(parser);
}

/********** documentation functions **********/

void usage(void) {
  printf("\ncdecl prints out the English language form of a C declaration.\n");
  printf("Invoke as 'cdecl <declaration>' or\n");
  printf("provide input on stdin and use '-' as the single command-line "
         "argument.\n");
  printf("Input must be terminated with a semicolon and enclosed in quotation "
         "marks.\n");
}

void limitations() {
  printf("Input must be shorter than %u characters, not including quotation "
         "marks and semicolon.\n",
         MAXTOKENLEN);
  printf("Known deficiencies:\n\ta) doesn't handle multiple comma-separated "
         "declarations;\n");
  printf("\tb) includes only the qualifiers defined in ANSI C, not all\n");
  printf("\t   libc, kernel extensions or compiler attributes;\n");
  printf("\tc) does not support C23 or C26 additions;\n");
  printf("\t   unicode, continuation lines, comments,\n");
  printf("\te) or 'inline'.\n");
}

/********** functions to modify the parser **********/

void initialize_parser(struct parser_props *parser) {
  reset_parser(parser);
  parser->out_stream = stdout;
  parser->err_stream = stderr;
}

void reset_parser(struct parser_props *parser) {
  parser->have_identifier = false;
  parser->have_type = false;
  parser->have_qualifier = false;
  parser->last_dimension_unspecified = true;
  parser->is_function = false;
  parser->is_enum = false;
  parser->is_struct_or_union = false;
  parser->is_pointer = false;
  parser->is_function_ptr = false;
  parser->is_typedef = false;
  parser->is_bitfield = false;
  parser->is_declarator_list = false;
  parser->has_enum_constants = false;
  parser->cursor = 0;
  parser->enumerator_list[0] = '\0';
  parser->array_dimensions = 0;
  parser->array_lengths = 0;
  parser->bitfield_width = 0;
  parser->has_function_params = false;
  parser->has_struct_or_union_members = false;
  parser->stacklen = 0;
  parser->stack[0].kind = invalid;
  parser->start_delim = '\0';
  parser->end_delim = '\0';
  parser->separator = '\0';
  parser->prev = NULL;
  parser->next = NULL;
  parser->parent = NULL;
}

struct parser_props *get_head_parser(struct parser_props *parser) {
  struct parser_props *cursor;
  if (!parser)
    return NULL;
  cursor = parser;
  while (cursor->prev) {
    cursor = cursor->prev;
  }
  return cursor;
}

struct parser_props *get_tail_parser(struct parser_props *parser) {
  struct parser_props *cursor;
  if (!parser)
    return NULL;
  cursor = parser;
  while (cursor->next) {
    cursor = cursor->next;
  }
  return cursor;
}

/* The list head is a stack allocation. */
static void _free_all_parsers(struct parser_props *parser) {
  if (!parser)
    return;
  struct parser_props *tail = get_tail_parser(parser);
  while (tail->prev) {
    struct parser_props *save = tail->prev;
#ifdef DEBUG
    fprintf(stderr, "free_all_parsers(): freeing %p\n", tail);
#endif
    free(tail);
    save->next = 0;
    tail = save;
  }
}

/*
 * Note that freeing the allocated parsers must come first since the reset makes
 * the next pointer NULL.
 */
void release_parser_resources(struct parser_props *parser) {
  _free_all_parsers(parser);
}

struct parser_props *make_parser(struct parser_props *const parser) {
  struct parser_props *new_parser =
      (struct parser_props *)malloc(sizeof(struct parser_props));
#ifdef DEBUG
  fprintf(stderr, "Allocated %p\n", new_parser);
#endif
  if (!new_parser) {
    exit(ENOMEM);
  }
  initialize_parser(new_parser);
  new_parser->out_stream = parser->out_stream;
  new_parser->err_stream = parser->err_stream;
  parser->next = new_parser;
  new_parser->prev = parser;
  return new_parser;
}

/********** functions which characterize input **********/

bool is_all_blanks(const char *input) {
  if (!input || !strlen(input)) {
    return false;
  }
  char *token_copy = strdup(input);
  _cleanup_(freep) char *saveptr = token_copy;
  while (token_copy && isprint(*token_copy) && isblank(*token_copy)) {
    token_copy++;
  }
  // Reached end of the string.
  if (!(token_copy && *token_copy)) {
    return true;
  }
  return false;
}

bool has_alnum_chars(const char *input) {
  if (!input || !strlen(input)) {
    return false;
  }
  char *copy = strdup(input);
  _cleanup_(freep) char *saveptr = copy;
  while (*copy && (!isalnum(*copy))) {
    copy++;
  }
  /* Reached the end without finding alphanumeric characters. */
  if (!*copy) {
    return false;
  }
  return true;
}

bool is_numeric(const char *input) {
  if (!input || !strlen(input)) {
    return false;
  }
  char *copy = strdup(input);
  _cleanup_(freep) char *saveptr = copy;
  while (*copy && (isdigit(*copy))) {
    copy++;
  }
  /* Reached the end without finding non-digit characters. */
  if (!*copy) {
    return true;
  }
  return false;
}

static bool is_type_char(const char c) {
  for (long unsigned i = 0; i < ARRAY_SIZE(typechars); i++) {
    if (c == typechars[i]) {
      return true;
    }
  }
  return false;
}

/*
 * For the first character, allow only a-z, A-Z and '_'.
 * Per https://en.cppreference.com/c/language/identifier:
 * A valid identifier must begin with a non-digit character (Latin letter,
 * underscore, or Unicode non-digit character(since C99) . . .
 */
static bool is_first_name_char(const char c) {
  if (isalpha(c) || ('_' == c)) {
    return true;
  }
  return false;
}

/* Digits are in addition allowed after the first character. */
static bool is_following_name_char(const char c) {
  if ((is_first_name_char(c)) || isdigit(c)) {
    return true;
  }
  return false;
}

static bool has_any_name_chars(const char *s) {
  char c;
  if (!s) {
    return false;
  }
  if (is_first_name_char(*s)) {
    return true;
  }
  for (size_t ctr = 1; ctr < strlen(s); ctr++) {
    c = *(s + ctr);
    if (is_following_name_char(c)) {
      return true;
    }
  }
  return false;
}

bool has_any_name_chars_before(const char *s, const char delimiter) {
  const char *delimp = strchr(s, delimiter);
  char delimited[MAXTOKENLEN];
  if (!delimp)
    return false;
  memset(&delimited, '\0', MAXTOKENLEN);
  strlcpy(delimited, s, (delimp - s) + 1);
  return has_any_name_chars(delimited);
}

/* A true return value means no errors. */
bool check_for_array_dimensions(struct parser_props *parser,
                                const char *offset_decl) {
  if ('[' != *offset_decl) {
    return true;
  }
  if (strstr(offset_decl, "]")) {
    parser->array_dimensions++;
    return true;
  }
  fprintf(parser->err_stream, "Mismatched array delimiters: %s\n", offset_decl);
  parser->stacklen = 0;
  return false;
}

/*
 * Return true if parenthes pairs match up, which means not just the count of
 * openers and closers matches, but also that curly braces cannot indicate a
 * scope change between them, and that any square brackets opened between them
 * must be closed.  Set the pair_count variable on success.
 */
bool parens_match(const char *offset_decl, size_t *pair_count) {
  size_t opener_count = 0;
  size_t closer_count = 0;
  size_t unmatched_opening_square_brackets = 0;

  for (size_t ctr = 0; ctr < strlen(offset_decl); ctr++) {
    char c = *(offset_decl + ctr);
    /* The parser is exiting the current scope. Parens-matching is judged on a
     * per-scope basis. */
    if ('{' == c) {
      return true;
    }
    if ('[' == c) {
      unmatched_opening_square_brackets++;
    }
    if (']' == c) {
      unmatched_opening_square_brackets--;
    }
    if ('(' == c) {
      opener_count++;
    }
    if (')' == c) {
      closer_count++;
      if ((closer_count <= opener_count) &&
          (0 == unmatched_opening_square_brackets)) {
        (*pair_count)++;
      } else {
        *pair_count = 0;
        return false;
      }
    }
  }
  if (opener_count != closer_count) {
    *pair_count = 0;
    return false;
  }
  return true;
}

/* A true return value means no errors. */
bool check_for_function_ptr(struct parser_props *parser,
                            const char *offset_decl) {
  if (parser->is_function_ptr ||
      (parser->parent && parser->parent->is_function_ptr)) {
    return true;
  }
  size_t pair_count = 0;
  if (!parens_match(offset_decl, &pair_count)) {
    fprintf(parser->err_stream, "Unmatched parentheses: %s\n", offset_decl);
    return false;
  }
  if ((2 == pair_count) && (strstr(offset_decl, "(*"))) {
    parser->is_function_ptr = true;
  }
  return true;
}

/* A true return value means no errors. */
bool check_for_function_parameters(struct parser_props *parser,
                                   const char *offset_decl) {
  char trimmed[MAXTOKENLEN];
  size_t trimnum = 0;
  if ((parser->is_function_ptr) && (')' == *offset_decl)) {
    /* +1 to go past ')' after function name. */
    trimnum = trim_leading_whitespace(offset_decl + 1, &trimmed[0]);
    /* Go past '(' at start of function parameters. */
    trimnum++;
  } else {
    trimnum = trim_leading_whitespace(offset_decl, &trimmed[0]);
  }
  if ('(' != *(offset_decl + trimnum)) {
    return true;
  }
  const char *params_end = strstr(offset_decl + trimnum, ")");
  if (!params_end) {
    parser->stacklen = 0;
    fprintf(parser->err_stream, "Malformed function declaration.\n");
    return false;
  }
  parser->is_function = true;
  parser->start_delim = '(';
  parser->end_delim = ')';
  parser->separator = ',';
  /* No function parameters since types are at least 3 chars long. */
  if (3 > (params_end - (offset_decl + trimnum))) {
    return true;
  }
  if (is_all_blanks(offset_decl + trimnum)) {
    return true;
  }
  /*
   * There is possibly a type within the parentheses and thus a function
   * parameter. */
  parser->has_function_params = true;
  return true;
}

/* A true return value means no errors. */
bool check_for_struct_or_union_members(struct parser_props *parser,
                                       const char *offset_decl) {
  size_t num_blanks = 0;
  while (isblank(*(offset_decl + num_blanks)))
    num_blanks++;
  const char *member_start = offset_decl + num_blanks;
  if (parser->is_enum || parser->has_struct_or_union_members) {
    return true;
  }
  if ('{' != *(member_start)) {
    return true;
  }
  const char *params_end = strstr(member_start, "}");
  if (!params_end) {
    parser->stacklen = 0;
    fprintf(parser->err_stream, "Malformed struct or union declaration.\n");
    return false;
  }
  /* No struct members since types are at least 3 chars long. */
  if (3 > (params_end - member_start)) {
    return true;
  }
  if (is_all_blanks(member_start)) {
    return true;
  }
  /*
   * There is possibly a type within the parentheses and thus a struct
   * member.
   */
  parser->has_struct_or_union_members = true;
  return true;
}

/*
 * A true return value means no errors.
 * While "enum State;" is invalid, the following are correct:
 * 0. enum State state; 1. enum State { SOLID, LIQUID };
 * 2. enum State { SOLID = 1, LIQUID = 3};
 * 3. enum State { SOLID, LIQUID} state;
 */
bool check_for_enum_constants(struct parser_props *parser,
                              const char *offset_decl) {
  const char *spacep = strstr(offset_decl, " ");
  const char *startbracep = strstr(offset_decl, "{");
  const char *endbracep = strstr(offset_decl, "}");
  if (parser->has_struct_or_union_members) {
    return true;
  }

  /*
   * If the declaration is not an enum, or we've already found enum_constants,
   *  there is nothing to check.
   */
  if ((!parser->is_enum) || (parser->has_enum_constants))
    return true;
  if ((NULL == spacep) && (!have_stacked_compound_type(parser))) {
    fprintf(parser->err_stream, "Enums cannot be forward-declared.\n");
    return false;
  }
  /* The declaration may be of type 0. */
  if (NULL == endbracep) {
    if (NULL == startbracep) {
      return true;
    }
    fprintf(parser->err_stream, "Malformed enumerator declaration %s.\n",
            offset_decl);
    return false;
  }
  if ((spacep > startbracep) || (startbracep > endbracep)) {
    return false;
  }
  /* The presence of enumeration constants is plausible in types 1 or 2. */
  parser->has_enum_constants = true;
  return true;
}

/*
 * Return value of false is an error indication.
 * Unlike functions, unions and structs, declarator lists are a property only of
 * the top-level parser.
 */
void check_for_declarator_list(struct parser_props *parser,
                               const char *user_input) {
  _cleanup_(freep) char *input = strdup(user_input);
  if (!input) {
    exit(ENOMEM);
  }
  char *next_comma_pos = strchr(input, ',');
  char *next_open_parens = strchr(input, '(');
  char *next_close_parens = strchr(input, ')');
  size_t cursor = 0;
  /* Declarator lists are not nested and typedefs are one per line. */
  if (parser->prev || parser->is_typedef || parser->is_enum) {
    return;
  }
  while (cursor < strlen(input)) {
    if (!next_comma_pos || !(*next_comma_pos)) {
      return;
    }
    /* There are at least two items in the declarator list. */
    if (!next_open_parens || !(*next_open_parens) ||
        (next_comma_pos < next_open_parens) ||
        (next_comma_pos == (next_close_parens + 1))) {
      parser->is_declarator_list = true;
      parser->separator = ',';
      return;
    }
    cursor = (next_comma_pos - input) + 1;
    next_comma_pos = strchr(input + cursor, ',');
    /* A series of parameters may belong to a single function, so only advance
     * the parens pointers after considering the complete list. */
    if (next_comma_pos > next_close_parens) {
      next_open_parens = strchr(input + cursor, '(');
      next_close_parens = strchr(input + cursor, ')');
    }
  }
  return;
}

/********** functions which modify input **********/

/*
 * Return value is the number of trimmed characters.
 * trimmed is the same as input except that it will start with a non-whitespace
 * character. If there are no non-whitespace characters, trimmed will be empty.
 * Caller must allocate trimmed.
 */
size_t trim_leading_whitespace(const char *input, char *trimmed) {
  char *copy = strdup(input);
  _cleanup_(freep) char *saveptr = copy;
  size_t removed = 0;

  memset(trimmed, '\0', MAXTOKENLEN);
  if (!input || (0 == strlen(input))) {
    return 0;
  }
  if (is_all_blanks(input)) {
    return strlen(input);
  }
  if (!isblank(*input)) {
    return 0;
  }
  while (copy && isblank(*copy)) {
    copy++;
    removed++;
  }
  /* Copy the non-blank part of the input to the output.*/
  while (copy && *copy) {
    *trimmed = *copy;
    trimmed++;
    copy++;
  }
  *trimmed = '\0';
  return removed;
}

/*
 * Return value is the number of trimmed characters.
 * trimmed is the same as input except that it will end in a non-whitespace
 * character. If there are no non-whitespace characters, trimmed will be empty.
 * Caller must allocate trimmed.
 */
size_t trim_trailing_whitespace(const char *input, char *trimmed) {
  _cleanup_(freep) char *copy = strdup(input);
  char *last_char = copy + (strlen(copy) - 1);
  /*
   * last_char should be greater than input when the loop exits, as otherwise
   * input is all blanks.
   */
  size_t removed = 0;

  memset(trimmed, '\0', MAXTOKENLEN);
  if (!input || (0 == strlen(input))) {
    return 0;
  }
  if (is_all_blanks(input)) {
    return strlen(input);
  }
  while ((last_char > copy) && (isblank(*last_char))) {
    last_char--;
    removed++;
  }
  /* Copy the non-blank part of the input to the output.*/
  for (char *cptr = copy; cptr <= last_char; cptr++) {
    *trimmed = *cptr;
    trimmed++;
  }
  *trimmed = '\0';
  return removed;
}

/*
 * Starting at the '=' of a enumeration constant assignment, overwrite
 * in place subsequent numeric digits and whitespace with the alphanumeric
 * characters which follow them.
 */
void elide_assignments(char **input) {
  size_t equals_offset = strcspn(*input, "=");
  char *cursor = *input + equals_offset;
  const size_t input_len = strlen(*input);
  /* 1 is to go past '='. */
  size_t to_skip = 1;

  if (equals_offset == input_len) {
    return;
  }
  while ((cursor + to_skip) < (*input + input_len)) {
    /* Go past initializer values. */
    while (isdigit(*(cursor + to_skip)) || isblank(*(cursor + to_skip))) {
      to_skip++;
    }
    /*
     * Copy characters past (cursor + to_skip) to cursor, thereby
     * making *input to_skip characters shorter.
     */
    for (; cursor < ((*input + input_len) - (to_skip - 1)); cursor++) {
      *cursor = *(cursor + to_skip);
    }
    *cursor = '\0';
    /* Check for additional assignments. Do not skip past comma. */
    equals_offset = strcspn(*input + equals_offset + 1, "=");
  }
}

/*
 * process_secondary_params() relies on tokenize_function_params() to
 * extract individual tokens from function parameters from a parameter
 * list.  It functions similarly to truncate_input() for the main
 * parser.  If the input begins with '(', character, go past it
 * without advancing the parser cursor.  Otherwise, overwrite the
 * first delimiter character with a NULL. In either case, copy the
 * resulting string to the output.  If the text chunk is the last in
 * input, the effect is to drop the remaining non-name characters.
 */
bool tokenize_function_params(char **output, char *input, const char delim) {
  char *param_end;
  const char *param_start;
  size_t param_len = 0;

  /* The token terminator is ")," if the next token-group is a function ptr. */
  char *end_next_param = strstr(input, "),");
  const char *first_separator = strchr(input, ',');
  const char *first_start_delim = strchr(input, '(');
  if (end_next_param) {
    /* Point to comma after ')'. */
    param_end = end_next_param + 1;
  } else {
    /* Here we have 3 cases.
     * The first is simple text followed by ')' for the last parameter.
     * The second is simple text followed by a comma.
     * The third is text followed by a comma and '(' where the comma precedes
     * the parens. This order indicates a simple parameter preceding a function
     * pointer.
     */
    if ((!first_separator && !first_start_delim) ||
        (first_separator && !first_start_delim) ||
        (first_separator && first_start_delim &&
         (first_separator < first_start_delim))) {
      param_end = strchr(input, delim);
    } else if (strstr(input, "))")) {
      /*
       * The next comma is part of a function-ptr's params.  Skip over it and
       * pass the function ptr as a whole to a subsidiary parser. The pattern
       * holds when a function pointer is last in a function's params, which is
       * the conventional case.
       */
      param_end = strrchr(input, delim);
    }
  }
  if (!param_end) {
    return false;
  }
  /*
   * There is a leading delimiter.   Overwriting it directly with NULL would
   * result in an empty string.
   */
  if (input == param_end) {
    param_start = input + 1;
    param_len = strlen(input) - 1;
  } else {
    /*
     * There is a trailing delimiter, so just overwrite it with NULL.
     */
    param_start = input;
    param_len = param_end - input;
  }
  /* Copy the entire input, as otherwise there is no trailing NULL. */
  strlcpy(*output, param_start, param_len + 1);
  return true;
}

bool tokenize_declarator_list(char **output, char *input, const char delim) {
  char *param_end = input + strlen(input);
  const char *param_start;
  size_t param_len = 0;

  char *end_next_param = strchr(input, delim);
  if (input == end_next_param) {
    /* Point to char after the comma. */
    param_start = input + 1;
    param_end = strchr(param_start, delim);
    param_len = param_end - (param_start + 1);
  } else {
    param_start = input;
    param_len = strlen(input);
  }
  /* Copy the entire input, as otherwise there is no trailing NULL. */
  strlcpy(*output, param_start, param_len + 1);
  return true;
}

/*
 * tokenize_struct_params() is somewhat simpler than
 * tokenize_function_params() because every struct or union member is
 * terminated by a semicolon, unlike the case for function parameters, where
 * the last one is terminated with "))".  Also, '}' is always followed by a
 * semicolon, while the character ')' occurs multiple times in a nested function
 * pointer but only terminates it when followed by a comma or another ')'.  On
 * the other hand, tokenize_struct_params() must deal with trailing identifiers
 * and arbitrarily large nesting levels.
 */
bool tokenize_struct_params(char **output, char *input, const char delim) {
  char *param_end = NULL;
  const char *param_start;
  size_t param_len = 0;

  /* "};" is the token terminator for embedded structs and unions. */
  char *end_next_param = strstr(input, "};");
  /* A semicolon terminates simple tokens and function pointers. */
  const char *first_separator = strchr(input, ';');
  const char *first_start_delim = strchr(input, '{');
  const char *last_end_delim = strrchr(input, '}');

  /*
   * If "};" occurs after the first separator, there's another struct or union
   * member before it which should be processed first.
   */
  /* Here we have 3 cases.
   * The first is text followed by a semicolon and '{' where the semicolon
   * precedes the bracket. This order indicates a simple parameter preceding a
   * subsidiary struct.
   * The second is simple text followed by a semicolon.
   * The third is that the current scope has no parameters left, but the
   * enclosing one does.
   */
  if (end_next_param && first_separator &&
      ((end_next_param < first_separator) ||
       (first_separator > first_start_delim))) {
    /* Case 0: point to semicolon after '}'. */
    param_end = end_next_param + 1;
  } else if ((first_separator && !first_start_delim) ||
             (first_separator && first_start_delim &&
              (first_separator < first_start_delim))) {
    /* Case 1. */
    param_end = strchr(input, delim);
  } else if (last_end_delim == (input + (strlen(input) - 1))) {
    /* Case 2. */
    param_end = (char *)last_end_delim;
  }
  /* Nothing to do. */
  if (!param_end) {
    return false;
  }
  /*
   * There is a leading delimiter.   Overwriting it directly with NULL would
   * result in an empty string.
   */
  if (input == param_end) {
    param_start = input + 1;
    param_len = strlen(input) - 1;
  } else {
    /*
     * There is a trailing delimiter, so just overwrite it with NULL.
     */
    param_start = input;
    param_len = param_end - input;
  }
  if (!param_len) {
    return false;
  }
  /* Copy the entire input, as otherwise there is no trailing NULL. */
  strlcpy(*output, param_start, param_len + 1);
  return true;
}

/*
 * Remove any characters following ';' , ')' or '=' plus any whitespace which
 * precedes these characters.  Returns true if the input contains one of those
 * chars and has any non-whitespace characters before them.  The parser is in
 * its initial state, so checking if it contains an enum or function via
 * parser_props boolean values is not yet possible.
 */
bool truncate_input(char **input, struct parser_props *parser) {
  char trimmed[MAXTOKENLEN];
  char *input_end = NULL;
  const char *found_enum = strstr(*input, "enum ");

  /*
   * Mixing the levels here is awful, but there's not an obvious way to avoid
   * it. Make sure that "enum " starts the input so that "bool renum = true"
   * doesn't pass.
   */
  if (found_enum && (found_enum == *input) && strstr(*input, "=")) {
    elide_assignments(input);
  } else {
    /* Dump chars after '=', if any. */
    input_end = strchr(*input, '=');
  }
  /*
   * If the input after '=' or ',' is not lopped off, the input should terminate
   * with ';' or ')'.
   */
  if (!input_end) {
    /*
     * When the declaration only includes one semicolon, strchr() and strrchr()
     * produce the same result.  struct and union definitions may contain
     * arbitrarily many semicolons.
     */
    input_end = strrchr(*input, ';');
    if (!input_end) {
      input_end = strrchr(*input, ')');
    }
    /* Input with two semicolons or ')' could reach this point. */
    if (input_end == *input) {
      fprintf(parser->err_stream, "Zero-length input string.\n");
      parser->stacklen = 0;
      return false;
    } else if (!input_end) { /* There are no terminators. */
      fprintf(parser->err_stream, "\nImproperly terminated declaration.\n");
      parser->stacklen = 0;
      return false;
    }
  }
  *input_end = '\0';
  if (trim_trailing_whitespace(*input, trimmed)) {
    strlcpy(*input, trimmed, MAXTOKENLEN);
  }
  if (!strlen(*input)) {
    fprintf(parser->err_stream, "Zero-length input string.\n");
    parser->stacklen = 0;
    return false;
  }
  return true;
}

/********** debugging functions **********/

void show_parser_list(const struct parser_props *parser, const int lineno) {
  struct parser_props *head = get_head_parser((struct parser_props *)parser);
  struct parser_props *pnext = head->next;
  if (!parser || !parser->err_stream) {
    return;
  }
  if (!pnext) {
    fprintf(parser->err_stream, "\nNo subsidiary parsers.\n");
    return;
  }
  /* The list head is a stack allocation. */
  fprintf(parser->err_stream, "HEAD at %d: %p-->", lineno, head);
  while (pnext) {
    fprintf(parser->err_stream, "%p", pnext);
    pnext = pnext->next;
    if (pnext) {
      fprintf(parser->err_stream, "-->");
    } else {
      fprintf(parser->err_stream, "\n");
      break;
    }
  }
  fflush(parser->err_stream);
}

void show_parser_reverse_list(const struct parser_props *parser) {
  struct parser_props *pprev = parser->prev;
  if (!pprev) {
    fprintf(parser->err_stream, "\nNo previous parsers.\n");
    return;
  }
  /* The list tail is a stack allocation. */
  fprintf(parser->err_stream, "TAIL: %p<--", parser);
  while (pprev) {
    fprintf(parser->err_stream, "%p", pprev);
    pprev = pprev->prev;
    if (pprev) {
      fprintf(parser->err_stream, "<--");
    } else {
      fprintf(parser->err_stream, "\n");
      break;
    }
  }
  fflush(parser->err_stream);
}

void showstack(const struct token *stack, const size_t stacklen,
               FILE *out_stream, const int lineno) {

  size_t tokennum = 0, ctr;

  if (!stack)
    return;
  fprintf(out_stream, "Stack at %d is:\n", lineno);
  for (ctr = 0; ctr < stacklen; ctr++) {
    fprintf(out_stream, "Token number %lu has kind %s and string %s\n",
            tokennum, kind_names[stack[ctr].kind], stack[ctr].string);
    tokennum++;
  }
  fflush(out_stream);
  return;
}

/********** parser helper functions **********/

bool have_stacked_compound_type(const struct parser_props *parser) {
  const char *spacepos = strchr(parser->stack[0].string, ' ');
  if ((!parser) || (type != parser->stack[0].kind) || (NULL == spacepos)) {
    return false;
  }
  return true;
}

/*
 * Append the name of a struct or union to the type name since in
 * "struct task_struct ts;", the type is "struct task_struct", not
 * "struct".  The parser has already encountered "union", "struct" or
 * "enum", so progress_ptr+offset should point past one of them.  The
 * return value indicates success or failure.
 */
bool handled_compound_type(struct parser_props *parser, char *progress_ptr,
                           struct token *this_token) {
  char compound_type_name[MAXTOKENLEN];
  char *name_end_ptr;
  char *startdelimp =
      strchr(progress_ptr + parser->cursor, parser->start_delim);
  size_t existing_token_len;
  size_t i, j = 0;

  parser->cursor += trim_leading_whitespace(progress_ptr + parser->cursor,
                                            &compound_type_name[0]);
  /*
   * A struct or union inside another struct or union can be
   * anonymous, without a compound type or a trailing instance name.
   * Since for enumerators parser->start_delim is NULL, strchr() will
   * set startdelimp to the trailing NULL in the string.  startdelimp can thus
   * genuinely be NULL for structs, unions or functions, but never will be for
   * enums, so also check *startdelimp.
   */
  if (parser->parent && parser->parent->is_struct_or_union && startdelimp &&
      ('\0' != *startdelimp) &&
      !has_any_name_chars_before(progress_ptr + parser->cursor,
                                 parser->parent->start_delim)) {
    return true;
  }
  /*
   * Since there's no leading whitespace, the next blank terminates the compound
   * identifier.
   */
  name_end_ptr = strchr(compound_type_name, ' ');
  /*
   * When the declaration is something like "struct task_struct;" or
   * "struct msg*", leave any trailing identifier or '*' for subsequent code to
   * process.  Check the value of name_end_ptr since strchr() never sets it to
   * NULL.
   */
  if (!name_end_ptr || !(*name_end_ptr)) {
    name_end_ptr = strchr(compound_type_name, '*');
  }
  if (!name_end_ptr || !(*name_end_ptr)) {
    name_end_ptr = strchr(compound_type_name, parser->separator);
  }
  if (!name_end_ptr || !(*name_end_ptr)) {
    return true;
  }
  /*
   * Enumerations, struct and function parameters may have compound type
   * names.
   */
  startdelimp = strchr(compound_type_name, parser->start_delim);
  /*
   * The space is inside the enumeration list or between struct members, and
   * thus not part of the the compound type.  That can only occur with a
   * malformed declaration, probably one which lacks a space.
   */
  if (startdelimp && (name_end_ptr > startdelimp)) {
    fprintf(parser->err_stream, "Malformed or incomplete compound type '%s'\n",
            this_token->string);
    return false;
  }
  existing_token_len = strlen(this_token->string);
  this_token->string[existing_token_len] = ' ';
  /* +1 for the space and +1 for terminating NULL */
  if ((existing_token_len + strlen(compound_type_name) + 2) > MAXTOKENLEN) {
    return false;
  }
  for (i = existing_token_len + 1;
       i < existing_token_len + 1 + strlen(compound_type_name); i++, j++) {
    /*
     * Reached the end of the compound_type_name, so stop before copying the
     * identifier into the type.
     */
    if ((parser->separator == compound_type_name[j]) ||
        ('*' == compound_type_name[j]) || (' ' == compound_type_name[j])) {
      break;
    }
    this_token->string[i] = compound_type_name[j];
  }
  this_token->string[i] = '\0';
  /*
   * The 1 accounts for the space.  The characters left in progress_ptr should
   * be an identifier which names the instance, if any, of the compound type. If
   * the object is a pointer, leave that also on the stack.  This special
   * handling is needed when '*' immediately follows the compound type name
   * without an intervening space.
   */
  if ('*' == *name_end_ptr) {
    parser->cursor += j;
  } else {
    parser->cursor += j + 1;
  }
  return true;
}

/*
 * Check to see if there are any identifiers which do not appear in enumeration
 * constant list.  If so, the identifier must be the name of an enum instance.
 */
bool all_identifiers_are_enum_constants(const struct parser_props *parser) {
  size_t stacknum = parser->stacklen;
  if (!parser->has_enum_constants) {
    return false;
  }
  while (--stacknum) {
    if (identifier == parser->stack[stacknum].kind) {
      if (!strstr(parser->enumerator_list, parser->stack[stacknum].string)) {
        return false;
      }
    }
  }
  return true;
}

bool first_identifier_is_enumerator(const struct parser_props *parser,
                                    const char *user_input) {
  const char *startbracep = strstr(user_input, "{");
  if ((!parser->is_enum) || (!parser->has_enum_constants)) {
    return false;
  }
  if (!startbracep)
    return false;
  /*
   * The parser already started processing enum_constants, so there is no enum
   * instance name.
   */
  if ((user_input + parser->cursor) > startbracep) {
    return true;
  }
  /* Should not be reached? */
  return false;
}

/*
 * Follow the convention of turning an "unsigned" qualifier which is not
 * accompanied by another type into "unsigned int".
 */
bool reclassify_unsigned_qualifier(struct parser_props *parser) {
  int cursor = 0;
  if (!parser || !parser->stacklen) {
    return false;
  }
  cursor = parser->stacklen - 1;
  while (cursor >= 0) {
    if ((qualifier == parser->stack[cursor].kind) &&
        (!strcmp(parser->stack[cursor].string, "unsigned"))) {
      parser->stack[cursor].kind = type;
      strlcpy(parser->stack[cursor].string, "unsigned int",
              strlen("unsigned int") + 1);
      parser->have_type = true;
      return true;
    }
    cursor--;
  }
  return false;
}

/* Return false is a qualifier-type combination is nonsensical. */
bool qualifier_is_compatible_with_type(const struct parser_props *parser,
                                       const char *type) {
  int cursor = 0;
  bool type_is_atomic = strstr(type, "atomic") == NULL ? false : true;
  if (!parser || !parser->stacklen) {
    return false;
  }
  cursor = parser->stacklen - 1;
  while (cursor >= 0) {
    if ((qualifier == parser->stack[cursor].kind) &&
        (!strcmp(parser->stack[cursor].string, "unsigned"))) {
      if (!strcmp(type, "char") || !strcmp(type, "short") ||
          !strcmp(type, "int")) {
        return true;
      } else {
        fprintf(parser->err_stream,
                "Type %s and qualifier unsigned are incompatible.\n", type);
        return false;
      }
    }
    if (type_is_atomic) {
      if ((qualifier == parser->stack[cursor].kind) &&
          (!strcmp(parser->stack[cursor].string, "atomic"))) {
        fprintf(parser->err_stream,
                "Type and qualifier cannot both be atomic.\n");
        return false;
      }
    }
    cursor--;
  }
  return true;
}

/*
 * Handle the case of an enumeration or struct instance name which follows the
 * enumeration constant or struct member list.
 */
void handle_trailing_instance_name(struct parser_props *parser,
                                   char *user_input) {
  const char *first_end_delim = strchr(user_input, '}');
  const char *last_end_delim = strrchr(user_input, '}');
  int delim_offset = 0;
  struct token this_token;
  if (!first_end_delim) {
    return;
  }
  /*
   * There can only be one instance name.  If the first identifier on the stack
   * is an enumeration constant, then any enumeration instance name must follow
   * the enumeration constants list.  The situation is similar if the object is
   * a struct or union, and parsing has reached the brace indicating the start
   * of the member list.
   */
  if (parser->cursor < strlen(user_input)) {
    if ((parser->is_enum &&
         first_identifier_is_enumerator(parser, user_input)) ||
        (!parser->have_identifier && parser->has_struct_or_union_members)) {
      this_token.kind = invalid;
      strcpy(this_token.string, "");
      /*
       * Advance processing to the char after the brace which closes the
       * enumeration constant or struct member list.  The next character is
       * either a space or the start of the instance name.
       */
      delim_offset = (first_end_delim - (user_input + parser->cursor)) + 1;
      /*
       * Parsing has already passed the first end delimiter, so advance the
       * cursor to the last one.
       */
      if (0 > delim_offset) {
        delim_offset = (last_end_delim - (user_input + parser->cursor)) + 1;
      }
      parser->cursor += delim_offset;
      parser->cursor +=
          gettoken(parser, user_input + parser->cursor, &this_token);
      if ((invalid == this_token.kind) || !strlen(this_token.string)) {
        return;
      }
      push_stack(parser, &this_token);
      /* Check parser->separator since enums set it to NULL. */
      if (parser->separator &&
          (parser->separator == *(user_input + parser->cursor))) {
        parser->cursor++;
      }
    }
  }
}

/*
 * The caller passes either the terminating delimiter or an intermediate
 * separator as the demarcator.
 */
bool load_next_secondary_param(struct parser_props *const current_parser,
                               char *progress_ptr, const char demarcator,
                               const char *err_string) {
  size_t increm = 0;
  _cleanup_(freep) char *next_param = (char *)malloc(MAXTOKENLEN);
  const struct parser_props *head = get_head_parser(current_parser);

  memset(next_param, '\0', MAXTOKENLEN);
  if ((head->is_declarator_list && current_parser->has_function_params) ||
      (current_parser->parent && current_parser->parent->has_function_params)) {
    if (!tokenize_function_params(&next_param, progress_ptr, demarcator)) {
      fprintf(current_parser->err_stream, "Failed to process %s %s\n",
              err_string, next_param ? next_param : "");
      return false;
    }
  } else if ((head->is_declarator_list &&
              current_parser->has_struct_or_union_members) ||
             (current_parser->parent &&
              current_parser->parent->has_struct_or_union_members)) {
    if (!tokenize_struct_params(&next_param, progress_ptr, demarcator)) {
      fprintf(current_parser->err_stream, "Failed to process %s %s\n",
              err_string, next_param ? next_param : "");
      return false;
    }
  } else if (head->is_declarator_list) {
    if (!tokenize_declarator_list(&next_param, progress_ptr, demarcator)) {
      return false;
    }
  } else {
    return true;
  }
  increm = load_stack(current_parser, next_param);
  if (!increm) {
    fprintf(head->err_stream, "Failed to load %s %s\n", err_string,
            next_param ? next_param : "");
    return false;
  }
  /* +1 to go past demarcator, which is not included in the cursor count. */
  current_parser->parent->cursor += increm + 1;
  return true;
}

/*
 * When processing a struct with a function pointer member with no
 * parameters, advance past "();". has_any_name_chars_before() is needed because
 * has_any_name_chars() will also observe subsequent struct members and trailing
 * instance names.
 */
static void advance_past_separator(struct parser_props *parser,
                                   const char *input) {
  if (strchr(input + parser->cursor, parser->separator) &&
      (!has_any_name_chars_before(input + parser->cursor, parser->separator))) {
    while (parser->separator != *(input + parser->cursor)) {
      parser->cursor++;
    }
    parser->cursor++;
  }
}

static void advance_past_start_delim(struct parser_props *parser,
                                     const char *input) {
  _cleanup_(freep) char *next_member = (char *)malloc(MAXTOKENLEN);
  if (!parser->end_delim) {
    return;
  }
  /*
   * Function pointers need to advance past the ')' which follows the function
   * name.
   */
  if (parser->end_delim == *(input + parser->cursor)) {
    parser->cursor++;
  }
  parser->cursor +=
      trim_leading_whitespace(input + parser->cursor, next_member);
  /* Finally advance the parser into the parameters or members. */
  if (parser->start_delim == *(input + parser->cursor)) {
    parser->cursor++;
  }
}

static bool next_separator_is_inside_delims(const struct parser_props *parser,
                                            const char *input) {
  if (!parser->start_delim) {
    return false;
  }
  const char *startp = strchr(input, parser->start_delim);
  const char *sepp = strchr(input, parser->separator);
  if (!sepp)
    return true;
  if (!startp)
    return false;
  return (startp < sepp);
}

/*
 * Spawn a new parser for each function parameter or struct member and
 * link it into a list whose head is the top-level parser.  Note that
 * the function or struct name is on the original parser's stack. enumerations
 * do not call this function, since the enumeration constants are simple strings
 * which are copied directly to the output.
 */
bool process_secondary_params(struct parser_props *parser, char *user_input) {
  struct parser_props *params_parser;
  struct parser_props *tail_parser = get_tail_parser(parser);
  char *progress_ptr = user_input + parser->cursor;

  /*
   * Create a pointer which the code doesn't otherwise need as a peg
   * on which to hang the function's error handler.  The approach is
   * inspired by rzg2l_irqc_common_init() in
   * https://github.com/linux4microchip/linux/blob/4d72aeabedfa202d12869e52c40eeabc5401c839/
   * drivers/irqchip/irq-renesas-rzg2l.c#L596
   */
  _cleanup_(subsidiary_parsers_cleanup) struct parser_props **dummy_parserp =
      &parser;
  char err_string[MAXTOKENLEN];
  char final_err_string[MAXTOKENLEN];

  /* TODO: make these enums */
  if (parser->has_function_params) {
    strcpy(err_string, "function parameter");
  } else if (parser->has_struct_or_union_members) {
    strcpy(err_string, "struct or union member");
  } else if (parser->is_declarator_list) {
    strcpy(err_string, "declarator list item");
  } else {
    return true;
  }
  strcpy(final_err_string, "last ");
  strcat(final_err_string, err_string);
  advance_past_start_delim(parser, user_input);
  progress_ptr = user_input + parser->cursor;
  while (strlen(progress_ptr)) {
    /*
     * has_any_chars() triggers a break if the input consists only of separators
     * and delimiters. The second check prevents a trailing instance name from
     * being stacked in the tail parser. Instead,
     * handle_trailing_instance_name() should stack it in the head parser so
     * that it can be popped at the start of output.  Functions have no trailing
     * instance names, so the end delimiter in question is '}'.
     */
    if (has_any_name_chars(progress_ptr)) {
      if (parser->end_delim && (parser->end_delim == *progress_ptr)) {
        break;
      }
      advance_past_separator(parser, user_input);
      progress_ptr = user_input + parser->cursor;
      // Freed in pop_stack().
      params_parser = make_parser(tail_parser);
      params_parser->parent = parser;
    }
    /*
     * There may be more than one function parameter or struct member yet to
     * process.
     */
    if (!next_separator_is_inside_delims(parser, progress_ptr)) {
      if (parser->parent && parser->parent->is_declarator_list) {
        char *firstbracep = strchr(progress_ptr, '{');
        char *firstparensp = strchr(progress_ptr, '(');
        char *firstcommap = strchr(progress_ptr, ',');
        if (strstr(progress_ptr, "enum") && (firstbracep < firstcommap)) {
          params_parser->is_enum = true;
          parser->start_delim = '\0';
          parser->end_delim = '\0';
          parser->separator = '\0';
        }
        if ((strstr(progress_ptr, "struct") ||
             (strstr(progress_ptr, "union"))) &&
            (firstparensp < firstcommap)) {
          params_parser->is_struct_or_union = true;
          parser->start_delim = '{';
          parser->end_delim = '}';
          parser->separator = ';';
        }
        if (!check_for_array_dimensions(parser, progress_ptr)) {
          if ((parser->prev) || !reclassify_unsigned_qualifier(parser)) {
            break;
          }
          parser->stacklen = 0;
          return false;
        }
      }
      if (params_parser &&
          !load_next_secondary_param(params_parser, progress_ptr,
                                     parser->separator, err_string)) {
        /*
         * Automated cleanup causes use-after-free when the parser is not the
         * top-level one.
         */
        dummy_parserp = NULL;
        return false;
      }
      progress_ptr = user_input + parser->cursor;
      /* All done with struct, union or functions params or members. */
      if (!has_any_name_chars_before(progress_ptr, parser->end_delim)) {
        break;
      }
#ifdef DEBUG
      show_parser_list(parser, __LINE__);
#endif
    } else if (parser->has_function_params ||
               parser->has_struct_or_union_members) {
      /*
       * There is only one remaining parameter without a separator.
       * While the trailing comma in a list of function parameters is optional,
       * each struct member declaration must end with a semicolon.
       * Pass progress_ptr rather than user_input since the params parser only
       * processes what's inside the delimiters.  Comma-separated declarator
       * lists have no end_delim.
       */
      if (!load_next_secondary_param(params_parser, progress_ptr,
                                     parser->end_delim, final_err_string)) {
        dummy_parserp = NULL;
        return false;
      }
#ifdef DEBUG
      show_parser_list(parser, __LINE__);
#endif
      progress_ptr = user_input + parser->cursor;
      /* Exit the current scope. */
      if (parser->separator == *progress_ptr) {
        parser->cursor++;
        progress_ptr = user_input + parser->cursor;
      }
      /* The surrounding scope may yet have parameters. */
      if (!next_separator_is_inside_delims(parser, progress_ptr) ||
          parser->is_declarator_list) {
        tail_parser = get_tail_parser(parser);
        continue;
      }
      break;
    } else {
      return false;
    }
    tail_parser = get_tail_parser(parser);
  }
  /*
   * Prevent the error handler from running by, essentially, putting the
   * dummy_parserp flag down.
   */
  dummy_parserp = NULL;
  handle_trailing_instance_name(parser, user_input);
  return true;
}

size_t process_array_length(struct parser_props *parser,
                            const char *offset_string,
                            struct token *this_token) {
  size_t ctr = 0;
  /* Check if the array is ill-formed. */
  if (NULL == strstr(offset_string, "]")) {
    /* Indicate hard failure. */
    parser->stacklen = 0;
    this_token->kind = invalid;
    return 0;
  }
  for (ctr = 0;
       offset_string && isdigit(*(offset_string + ctr)) && ctr < MAXTOKENLEN;
       ctr++) {
    this_token->string[ctr] = *(offset_string + ctr);
  }
  this_token->kind = length;
  if (!finish_token(parser, offset_string, this_token, ctr)) {
    parser->stacklen = 0;
    this_token->kind = invalid;
    return 0;
  }
  return ctr;
}

bool parser_has_atomic_qualifier(const struct parser_props *parser) {
  size_t stacktop = parser->stacklen - 1;
  if (!parser || !parser->stacklen) {
    return false;
  }
  while (stacktop-- > 0) {
    if ((qualifier == parser->stack[stacktop].kind) &&
        strstr(parser->stack[stacktop].string, "atomic")) {
      return true;
    }
  }
  return false;
}

bool process_array_dimensions(struct parser_props *parser, char *user_input,
                              struct token *this_token) {
  char *next_dim;
  char *progress_ptr;
  size_t increm = 0;

  if (!strlen(user_input + parser->cursor)) {
    return true;
  }
  do {
    if (']' == *(user_input + parser->cursor)) {
      parser->cursor++;
    }
    /* Skip '['. */
    parser->cursor++;
    progress_ptr = user_input + parser->cursor;
    /* We've encountered "[]", which always terminates C array-length
     * declarations. Return without advancing the cursor.
     */
    if (']' == *progress_ptr) {
      /*
       * The first increment of array_dimensions happens in finish_token() after
       * finding the identifier.
       * The first dimension must have a length.  If we've observed a length,
       * the dimension is not the first, so the counter should be incremented.
       */
      if (parser->array_lengths) {
        parser->array_dimensions++;
      }
      break;
    }
    increm = gettoken(parser, progress_ptr, this_token);
    if (!increm) {
      parser->stacklen = 0;
      this_token->kind = invalid;
      return false;
    }
    parser->cursor += increm;
    if ((length == this_token->kind) && (strlen(this_token->string))) {
      push_stack(parser, this_token);
    } else {
      fprintf(parser->err_stream,
              "Array declarations must be followed by (possibly empty) "
              "lengths, not %s.\n",
              progress_ptr);
      return false;
    }
    next_dim = strstr(user_input + parser->cursor, "[");
    if (next_dim) {
      if (2 >= strlen(next_dim)) {
        /* No array length; return without incrementing offset. */
        parser->array_dimensions++;
        break;
      }
    }
  } while ((NULL != next_dim) && (parser->cursor <= strlen(user_input)));
  if (parser->array_dimensions == parser->array_lengths) {
    parser->last_dimension_unspecified = false;
  }
  return true;
}

bool handle_bitfield_width(struct parser_props *parser,
                           const char *user_input) {
  const char *colon_pos = strchr(user_input + parser->cursor, ':');
  size_t bf_width = 0;

  if (!colon_pos) {
    fprintf(parser->err_stream, "Bitfield length not found.\n");
    return false;
  }
  while ('\0' != *(user_input + parser->cursor)) {
    if (isdigit(*(user_input + parser->cursor))) {
      /* Only works because the maximum bitfield width is 8, which is a single
       * digit. */
      bf_width = atol(user_input + parser->cursor);
      if (bf_width) {
        parser->bitfield_width = bf_width;
        return true;
      } else {
        fprintf(parser->err_stream, "Bitfield length cannot be zero.\n");
        return false;
      }
    }
    parser->cursor++;
  }
  return false;
}

bool has_digit_after_possible_blanks(const char *s) {
  _cleanup_(freep) char *searched = strdup(s);
  if ((!s) || ('\0' == *s)) {
    return false;
  }
  while (s++) {
    if (isdigit(*s)) {
      return true;
    }
    if (!isblank(*s)) {
      return false;
    }
  }
  return false;
}

/*
 * In a bitfield declaration, the identifier is followed by possible
 * blanks, then a colon, then possible blanks, then a digit. Any
 * non-whitespace characters between the colon and the digit are
 * erroneous.
 */
bool check_for_bitfield(struct parser_props *parser, const char *offset_decl) {
  const char *colon_pos = strchr(offset_decl, ':');
  if (!colon_pos) {
    return true;
  }
  if (has_digit_after_possible_blanks(colon_pos)) {
    parser->is_bitfield = true;
    return true;
  }
  fprintf(parser->err_stream, "Malformed bitfield specification %s\n",
          offset_decl);
  return false;
}

/*
 * C does not support bitfields wider than integers. The only other choice is
 * bool.  See https://en.cppreference.com/c/language/bit_field
 */
bool type_is_bitfield_compatible(const struct parser_props *parser) {
  size_t stacktop = parser->stacklen - 1;
  if (!parser || !parser->stacklen || !parser->have_type) {
    return false;
  }
  /*
   * Decrementing the counter after the test means that it must be >0 rather
   * than >=0, which would allow negative indices inside the loop.
   */
  while (stacktop-- > 0) {
    if (type == parser->stack[stacktop].kind) {
      if ((!strcmp(parser->stack[stacktop].string, "int")) ||
          (!strcmp(parser->stack[stacktop].string, "unsigned int"))) {
        if (BITS_PER_INT >= parser->bitfield_width) {
          return true;
        } else {
          fprintf(parser->err_stream,
                  "Bitfield width %ld too wide for integer type.\n",
                  parser->bitfield_width);
        }
      } else if (!strcmp(parser->stack[stacktop].string, "bool")) {
        if (1 == parser->bitfield_width) {
          return true;
        }
        fprintf(parser->err_stream, "Boolean bitfields must have width 1.\n");
      } else {
        fprintf(parser->err_stream, "Type does not support bitfields.\n");
      }
    }
  }
  return false;
}

/*
 * check_for_enum_constants() has performed limited sanity-checking for
 * enumeration constants and set has_enum_constants = true, but there still may
 * not be any.  Return false if an error is encountered.  Otherwise, add
 * any enumeration constants to the enumerator_list, check for a trailing
 * instance name, and return true.
 */
bool process_enum_constants(struct parser_props *parser, char *user_input) {
  struct token this_token;
  char *progress_ptr = user_input + parser->cursor;
  const char *startbracep = strchr(progress_ptr, '{');
  const char *endbracep = strchr(progress_ptr, '}');
  _cleanup_(freep) char *first_non_blank = (char *)malloc(MAXTOKENLEN);
  char *commapos;
  size_t list_capacity;

  if (!startbracep) {
    parser->has_enum_constants = false;
    return true;
  }
  parser->cursor += trim_leading_whitespace(progress_ptr, first_non_blank);
  /*
   * trim_leading_whitespace() should have advanced parsing to '{'.  If not,
   * there is unanticipated input before the enumeration constants, so return an
   * error. Most likely the error will occur if there is no space between an
   * enumeration instance name and '{'.
   * Another possible erroneous condition is that the parser is at '{', but has
   * already passed '}'.
   */
  if ((startbracep != (user_input + parser->cursor)) ||
      (endbracep && (startbracep > endbracep))) {
    parser->has_enum_constants = false;
    return false;
  }
  progress_ptr = user_input + parser->cursor;
  do {
    if (',' == *progress_ptr) {
      progress_ptr++;
    }
    /* Parsing is done. */
    if (('}' == *progress_ptr) || (!has_any_name_chars(progress_ptr))) {
      if (!strlen(parser->enumerator_list)) {
        fprintf(parser->err_stream,
                "Enumeration constant list cannot be empty.\n");
        return false;
      }
      break;
    }
    parser->cursor += gettoken(parser, progress_ptr, &this_token);
    /*
     * The classifier should assess the enumeration constants as identifiers,
     * but the type is not used.  Strictly speaking, the code should fail if
     * it encounters an identifier when the parser already has one, but
     * instead it ignores identifiers after the first, except in this case.
     */
    if ((invalid == this_token.kind) || (0 == strlen(this_token.string)) ||
        (strlen(this_token.string) > MAXTOKENLEN)) {
      fprintf(parser->err_stream, "Invalid enumerator %s\n", this_token.string);
      parser->stacklen = 0;
      return false;
    }
    list_capacity = (MAXTOKENLEN - strlen(parser->enumerator_list)) - 1;
    if (strlen(parser->enumerator_list)) {
      strlcat(parser->enumerator_list, ",", list_capacity);
    }
    strlcat(parser->enumerator_list, this_token.string, list_capacity);
    commapos = strchr(progress_ptr, ',');
    /* Go past comma or end brace. */
    parser->cursor++;
    progress_ptr = user_input + parser->cursor;
  } while (commapos && (commapos < endbracep) && (progress_ptr < endbracep));
  handle_trailing_instance_name(parser, user_input);
  return true;
}

/* Deal with arrays, functions and enumeration constants. */
/* Either function parameter list is "()" and there are no new characters
 * processed, or there are function parameters, each handled by a new parser.
 * For structs, "{}" is illegal.
 */
bool handled_extended_parsing(struct parser_props *parser, char *user_input,
                              struct token *this_token) {
  if (!check_for_bitfield(parser, user_input)) {
    return false;
  }
  if (parser->is_bitfield) {
    if (!handle_bitfield_width(parser, user_input)) {
      return false;
    }
    if (!type_is_bitfield_compatible(parser)) {
      return false;
    }
  }
  if (parser->array_dimensions) {
    if (!process_array_dimensions(parser, user_input, this_token)) {
      return false;
    }
  } else if (parser->has_function_params ||
             parser->has_struct_or_union_members ||
             parser->is_declarator_list) {
    /* Move past the already-processed characters and '('. */
    if (!process_secondary_params(parser, user_input)) {
      return false;
    }
  } else if (parser->has_enum_constants) {
    if (!process_enum_constants(parser, user_input)) {
      return false;
    }
  }
  return true;
}

/********** output functions **********/

void reverse_lengths(struct parser_props *parser) {
  // Intentionally truncate in the case of an odd number of lengths.
  const size_t num_pairs = (size_t)parser->array_lengths / 2;
  // The token at parser->stacklen-1 is the identifier.
  const size_t top_len_idx = parser->stacklen - 2;
  size_t bottom_len_idx;
  if (parser->array_lengths % 2) {
    bottom_len_idx = (top_len_idx - num_pairs) - 1;
  } else {
    bottom_len_idx = top_len_idx - num_pairs;
  }
  for (size_t ctr = 0; ctr < num_pairs; ctr++) {
    struct token bottom_len = parser->stack[bottom_len_idx + ctr];
    struct token top_len = parser->stack[top_len_idx - ctr];
    strlcpy(parser->stack[top_len_idx].string, bottom_len.string, MAXTOKENLEN);
    strlcpy(parser->stack[bottom_len_idx].string, top_len.string, MAXTOKENLEN);
  }
}

/*
 * Most qualifiers modify the type, not the identifier. Therefore reorder
 * qualifiers and types on the stack in order to produce correct output.  The
 * exceptions are "static" and "extern", which describe the storage duration and
 * linker visibility of the identifier, and "inline", which is a compiler
 * attribute masquerading as a keyword.
 */
void reorder_qualifier_and_type(struct parser_props *parser) {
  size_t stacktop = parser->stacklen;
  if (!parser->stacklen) {
    return;
  }
  if (parser->have_type) {
    while (--stacktop > 0) {
      if ((0 == strcmp("extern", parser->stack[stacktop - 1].string)) ||
          (0 == strcmp("static", parser->stack[stacktop - 1].string))) {
        continue;
      }
      if ((type == parser->stack[stacktop].kind) &&
          (qualifier == parser->stack[stacktop - 1].kind) &&
          (0 != strcmp("*", parser->stack[stacktop - 1].string))) {
        /* Save type element's string. */
        char type_name[MAXTOKENLEN];
        strlcpy(type_name, parser->stack[stacktop].string, MAXTOKENLEN);
        /* Overwrite type (top element) with the 2nd element from top
         * (qualifier). */
        parser->stack[stacktop].kind = qualifier;
        strlcpy(parser->stack[stacktop].string,
                parser->stack[stacktop - 1].string, MAXTOKENLEN);
        /* Complete the swap. */
        parser->stack[stacktop - 1].kind = type;
        strlcpy(parser->stack[stacktop - 1].string, type_name, MAXTOKENLEN);
      }
    }
  }
}

/*
 * If the declaration describes a 1-dimensional array with a specified length,
 * the top of the stack holds the array lengths and the element below them is
 * the identifier.
 */
void reorder_array_identifier_and_lengths(struct parser_props *parser) {
  if (!parser->stacklen || !parser->array_lengths) {
    return;
  }
  const size_t stacklast = parser->stacklen - 1;
  /*
   * The identifier name starts at the top.  We want it below the array
   * dimensions.
   */
  size_t unprocessed_lengths = parser->array_lengths;
  /*
   * Move the identifier to the stack top by swapping it with each identifier in
   * turn.
   */
  while (unprocessed_lengths) {
    struct token name = parser->stack[stacklast - unprocessed_lengths];
    struct token arraylen =
        parser->stack[(stacklast - unprocessed_lengths) + 1];
    if ((length != arraylen.kind) || (identifier != name.kind)) {
      return;
    }
    strlcpy(parser->stack[(stacklast - unprocessed_lengths) + 1].string,
            name.string, MAXTOKENLEN);
    strlcpy(parser->stack[stacklast - unprocessed_lengths].string,
            arraylen.string, MAXTOKENLEN);
    parser->stack[(stacklast - unprocessed_lengths) + 1].kind = identifier;
    parser->stack[stacklast - unprocessed_lengths].kind = length;
    unprocessed_lengths--;
  }
  if (parser->array_lengths > 1) {
    reverse_lengths(parser);
  }
}

/* Order the stacked token for the convenience of the pop_stack() function. */
void reorder_stacks(struct parser_props *parser) {
  reorder_array_identifier_and_lengths(parser);
  reorder_qualifier_and_type(parser);
  struct parser_props *next_parser = parser->next;
  while (next_parser) {
    reorder_array_identifier_and_lengths(next_parser);
    reorder_qualifier_and_type(next_parser);
    next_parser = next_parser->next;
  }
}

/* Return true on success. */
bool pop_stack(struct parser_props *parser, bool no_enum_instance,
               bool is_second_pointer_qualifier) {
  size_t stacktop;
  if (!parser->stacklen) {
    fprintf(parser->err_stream, "Attempt to pop empty stack.\n");
    return false;
  }
  /* Last element of stack with stacklen=n is at index = n-1. */
  stacktop = parser->stacklen - 1;
  /*
   * Qualifiers following * apply to the pointer itself, rather than to the
   * object to which the pointer points.
   */
  if (!strcmp(parser->stack[stacktop].string, "*")) {
    if (parser->is_function_ptr && !is_second_pointer_qualifier) {
      fprintf(parser->out_stream, "pointer to a function which returns ");
    } else {
      fprintf(parser->out_stream, "pointer to ");
    }
    if (parser->is_declarator_list && stacktop &&
        (identifier == parser->stack[stacktop - 1].kind)) {
      fprintf(parser->out_stream, " and ");
    }
  } else {
    switch (parser->stack[stacktop].kind) {
    case whitespace:
      __attribute__((fallthrough));
    case qualifier:
      if (!strcmp("volatile", parser->stack[stacktop].string) &&
          parser->is_function) {
        /* Purge any already printed messages from the output stream which are
         * now irrelevant. */
        __fpurge(parser->out_stream);
        fprintf(parser->err_stream,
                "Function return types cannot be volatile.\n");
        return false;
      }
      if ((0 == strcmp("extern", parser->stack[stacktop].string)) ||
          (0 == strcmp("static", parser->stack[stacktop].string))) {
        fprintf(parser->out_stream,
                "and which has static storage duration and %s linkage",
                !strcmp(parser->stack[stacktop].string, "extern") ? "external"
                                                                  : "internal");
        break;
      }
      fprintf(parser->out_stream, "%s ", parser->stack[stacktop].string);
      break;
    /* Process the function parameters right after processing the return value
     * of a function.  */
    case type:
      fprintf(parser->out_stream, "%s ", parser->stack[stacktop].string);
      /*
       * If the function is itself part of a union or struct, then
       * parser->next could be populated without function params.
       */
      if (parser->has_function_params) {
        struct parser_props *cursor = parser->next;
        size_t depth = 0;
        while (cursor && cursor->stacklen) {
          if (depth) {
            fprintf(parser->out_stream, "and ");
          } else {
            fprintf(parser->out_stream, "and takes param(s) ");
          }
          if (!pop_all(cursor)) {
            return false;
          }
          depth++;
          struct parser_props *save_next = cursor->next;
          struct parser_props *save_prev = cursor->prev;
#ifdef DEBUG
          fprintf(stderr, "pop_stack(): freeing %p at %d\n", cursor, __LINE__);
#endif
          free(cursor);
          save_prev->next = save_next;
          if (save_next) {
            save_next->prev = save_prev;
          }
          cursor = save_next;
        }
      }
      if (parser->has_struct_or_union_members) {
        struct parser_props *cursor = parser->next;
        size_t depth = 0;
        if (parser->have_identifier) {
          fprintf(parser->out_stream, "which ");
        }
        while (cursor && cursor->stacklen) {
          if (depth) {
            fprintf(parser->out_stream, "and ");
          } else {
            fprintf(parser->out_stream, "has member(s) ");
          }
          if (!pop_all(cursor)) {
            return false;
          }
          depth++;
          struct parser_props *save_next = cursor->next;
          struct parser_props *save_prev = cursor->prev;
#ifdef DEBUG
          fprintf(stderr, "pop_stack(): freeing %p at %d\n", cursor, __LINE__);
#endif
          free(cursor);
          save_prev->next = save_next;
          if (save_next) {
            save_next->prev = save_prev;
          }
          cursor = save_next;
        }
      }
      if (parser->has_enum_constants) {
        if (no_enum_instance) {
          fprintf(parser->out_stream, "has enum constant");
        } else {
          fprintf(parser->out_stream, "with enum constant");
        }
        fprintf(parser->out_stream, " %s ", parser->enumerator_list);
      }
      if (parser->is_bitfield) {
        if (parser->bitfield_width) {
          fprintf(parser->out_stream, "bitfield of width %ld",
                  parser->bitfield_width);
        } else {
          fprintf(parser->err_stream, "ERROR: bitfield has no width.\n");
          return false;
        }
      }
      break;
    case identifier:
      if (parser->is_declarator_list && stacktop &&
          (identifier == parser->stack[stacktop - 1].kind)) {
        fprintf(parser->out_stream, "%s is a(n) and ",
                parser->stack[stacktop].string);
      } else {
        fprintf(parser->out_stream, "%s is a(n) ",
                parser->stack[stacktop].string);
      }
      if (parser->is_typedef) {
        fprintf(parser->out_stream, "alias for ");
      }
      if (parser->array_dimensions) {
        fprintf(parser->out_stream, "array of ");
      } else if ((parser->is_function) && (!parser->is_function_ptr)) {
        fprintf(parser->out_stream, "function which returns ");
      } else if ((parser->has_enum_constants) &&
                 strlen(parser->enumerator_list) &&
                 (strstr(parser->enumerator_list,
                         parser->stack[stacktop].string))) {
        /*
         * If the identifier is in the enumerator_list, it is not the
         * name of a enum instance.
         */
        break;
      }
      break;
    case length:
      if (parser->array_dimensions) {
        fprintf(parser->out_stream, "%s", parser->stack[stacktop].string);
        if (parser->array_lengths > 1) {
          fprintf(parser->out_stream, "x");
        } else {
          if (parser->last_dimension_unspecified) {
            fprintf(parser->out_stream, "x? ");
          } else {
            fprintf(parser->out_stream, " ");
          }
        }
      } else {
        fprintf(parser->err_stream, "\nError: found length without array.\n");
      }
      parser->array_lengths--;
      break;
    case invalid:
      break;
    case typedefn:
      break;
    default:
      fprintf(parser->err_stream,
              "\nError: element %s is of unknown type %d.\n",
              parser->stack[stacktop].string, parser->stack[stacktop].kind);
      return false;
    }
  }
  memset(parser->stack[stacktop].string, '\0', MAXTOKENLEN);
  parser->stack[stacktop].kind = invalid;
  return true;
}

bool pop_all(struct parser_props *parser) {
  /* If there is a non-enumeration constant identifier, it will be at the top of
   * the stack.  Therefore, the comparison with the enumerator_list
   * must proceed the first call to pop_stack() and be passed to it.
   */
  const bool no_enum_instance = all_identifiers_are_enum_constants(parser);
  bool passed_pointer_qualifier = false;
  char save[MAXTOKENLEN] = {};
  while (parser && parser->stacklen) {
    /* pop_stack() erases the final token. Save its string. */
    if (parser->stacklen) {
      strlcpy(save, parser->stack[parser->stacklen - 1].string,
              strlen(parser->stack[parser->stacklen - 1].string) + 1);
    } else {
      strlcpy(save, "", 1);
    }
    if (!pop_stack(parser, no_enum_instance, passed_pointer_qualifier)) {
      return false;
    }
    /*
     * If the saved string is "*", notify pop_stack() on the next iteration.
     * The point is to correctly handle two instances of "*" in a row.
     */
    if (!strcmp(save, "*")) {
      passed_pointer_qualifier = true;
    }
    parser->stacklen--;
  }
  return true;
}

/********** the core parser functions **********/

enum token_class get_kind(const char *intoken) {
  size_t numel = 0, ctr;

  if ((!intoken) || (!strlen(intoken))) {
    return invalid;
  }
  if (is_all_blanks(intoken)) {
    return whitespace;
  }

  if (!strcmp(intoken, "typedef")) {
    return typedefn;
  }
  numel = ARRAY_SIZE(types);
  for (ctr = 0; ctr < numel; ctr++) {
    if (!strcmp(intoken, types[ctr]))
      return type;
  }

  numel = ARRAY_SIZE(qualifiers);
  for (ctr = 0; ctr < numel; ctr++) {
    if (!strcmp(intoken, qualifiers[ctr]))
      return qualifier;
  }

  if (is_numeric(intoken)) {
    return length;
  }

  /*
   * A string without alphanumeric chars must be whitespace, a delimiter, or
   * garbage.
   */
  if (!has_alnum_chars(intoken)) {
    return invalid;
  }

  return identifier;
}

/* Moves to the right through the declaration, returning one space- or
 * delimiter-separated token at a time.
 * The parameter this_token returns the next token in the string.
 * The return value is the offset where parsing should resume in the next pass.
 *
 * If the parser has a type, non-name characters trigger termination of an
 * identifier.
 *
 * If the parser has no type, characters which are neither name nor type
 * characters trigger termination of input.
 */
size_t gettoken(struct parser_props *parser, const char *declstring,
                struct token *this_token) {

  const size_t tokenlen = strlen(declstring);
  /* tokenoffset is the parser's overall progress counter. */
  size_t tokenoffset = 0;
  /*
   * ctr records how many characters are written to the output token.
   * tokenoffset is >= ctr because it includes delimiters which are not
   * copied into tokens.
   */
  size_t ctr = 0;
  size_t increm = 0;
  char trimmed[MAXTOKENLEN];
  const char *startbracep = strchr(declstring, '{');
  char nextchar = '\0';
  const size_t trimnum = trim_leading_whitespace(declstring, trimmed);

  memset(this_token->string, '\0', MAXTOKENLEN);
  this_token->kind = invalid;

  if (!tokenlen) {
    return 0;
  }
  if (tokenlen > (MAXTOKENLEN - 1)) {
    fprintf(stderr, "\nToken too long %s.\n", declstring);
    return 0;
  }
  /* Move past leading whitespace. */
  tokenoffset = trimnum;
  /* Process array length, if any. We should already have an identifier. */
  if (parser->array_dimensions) {
    if ('[' == *(declstring + tokenoffset)) { tokenoffset++;}
    increm = process_array_length(parser, declstring + tokenoffset, this_token);
    if (!increm) {
      fprintf(parser->err_stream, "Array-length processing failed.\n");
      this_token->kind = invalid;
      parser->stacklen = 0;
      return 0;
    }
    return tokenoffset + increm;
  }
  /* Move past '(' enclosing the function pointer name to '*'. */
  if (parser->is_function_ptr && ('(' == *(declstring + tokenoffset))) {
    tokenoffset++;
  }
  /* The token is a single character. */
  if ('*' == *(declstring + tokenoffset)) {
    strlcpy(this_token->string, "*", 2);
    tokenoffset++;
    ctr++;
    if (!finish_token(parser, declstring + tokenoffset, this_token, ctr)) {
      this_token->kind = invalid;
      parser->stacklen = 0;
      return 0;
    }
    return tokenoffset;
  }
  nextchar = *(declstring + tokenoffset);
  /* The token has multiple characters, so copy them all. */
  for (int i = 0; i < (int)tokenlen; i++) {
    if ('\0' == nextchar) {
      break;
    }
    if (parser->have_type) {
      /*
       * We are looking for an identifier. Finding an identifier terminates
       * parsing unless we are processing an enum with an enumerator list.
       */
      if (!is_following_name_char(nextchar)) {
        if ((('{' == nextchar) || ('=' == nextchar) ||
             (isdigit(nextchar) || isblank(nextchar))) &&
            (startbracep && (startbracep <= (declstring + tokenoffset)))) {
          if (parser->is_enum) {
            /*
             * Setting has_enum_constants prevents check_for_enumerators() from
             * running later.
             */
            parser->has_enum_constants = true;
            /* Move past curly braces. */
            tokenoffset++;
            nextchar = *(declstring + tokenoffset);
            continue;
          }
          if (parser->is_struct_or_union) {
            parser->has_struct_or_union_members = true;
            /* Account for '{'. */
            ctr++;
          }
        } else if (('[' != nextchar) && (']' != nextchar) &&
                   (parser->array_dimensions || parser->array_lengths)) {
          /* Proceed past array dimension delimiters , but otherwise fail. */
          return 0;
        } else if ((',' == nextchar) && parser->is_declarator_list) {
          /* Effectively put the comma back. */
          nextchar = '\0';
        }
        break; /* end of if (is_following_name_char(nextchar)) */
      } else if (((trimnum == tokenoffset) || (0 == tokenoffset)) &&
                 (!is_first_name_char(nextchar))) {
        /* The first character of a name cannot be a digit, so special-case it.
         */
        return 0;
      }
    } else {
      /*
       * We are looking for a type but we might first see a qualifier composed
       * of name_chars.
       */
      if (!is_following_name_char(nextchar) && !is_type_char(nextchar)) {
        break;
      }
    }
    this_token->string[ctr] = nextchar;
    ctr++;
    tokenoffset++;
    nextchar = *(declstring + tokenoffset);
  } /* end of character-copying for-loop */
  if (ctr) {
    if (!finish_token(parser, declstring + tokenoffset, this_token, ctr)) {
      this_token->kind = invalid;
      parser->stacklen = 0;
      return 0;
    }
  }
  return tokenoffset;
}

/*
 * finish token() receives an unterminated string from gettoken() which it
 * readies for pushing onto the stack.  Upon failure, it resets the parser and
 * returns false.
 */
bool finish_token(struct parser_props *parser, const char *offset_decl,
                  struct token *this_token, const size_t ctr) {
  this_token->string[ctr + 1] = '\0';
  if (!ctr) {
    parser->stacklen = 0;
    return false;
  }
  this_token->kind = get_kind(this_token->string);
  switch (this_token->kind) {
  case identifier:
    /*
     * Enum constants are classified as identifiers.   Otherwise, duplicate
     * identifiers is an error.
     */
    if ((parser->have_identifier) &&
        (!parser->has_enum_constants && !parser->is_declarator_list)) {
      this_token->kind = invalid;
      parser->stacklen = 0;
      return false;
    }
    parser->have_identifier = true;
    if (!parser->have_type ||
        !check_for_array_dimensions(parser, offset_decl) ||
        !check_for_function_parameters(parser, offset_decl) ||
        (parser->is_struct_or_union &&
         !check_for_struct_or_union_members(parser, offset_decl)) ||
        (parser->is_enum && !check_for_enum_constants(parser, offset_decl))) {
      /*
       * In the first case, if parser->prev is set, the current parser is a
       * subsidiary one. Failing here would prevent processing of trailing
       * struct instance names. The parent parser may have a type and identifier
       * already.  Rely on the subsequent check in input_parsing_successful() to
       * catch incomplete declarations.
       *
       * In the second case, reclassify_unsigned_qualifier() may set
       * parser->have_type to true.
       */
      if ((parser->prev) || reclassify_unsigned_qualifier(parser)) {
        break;
      }
      parser->stacklen = 0;
      return false;
    }
    if ((parser->array_dimensions || parser->is_function) &&
        parser_has_atomic_qualifier(parser)) {
      fprintf(parser->err_stream,
              "Function return values and arrays cannot be atomic.\n");
      this_token->kind = invalid;
      parser->stacklen = 0;
      return false;
    }
    break;
  case type:
    /* Indicate hard failure. */
    if (parser->have_type) {
      this_token->kind = invalid;
      parser->stacklen = 0;
      return false;
    }
    parser->have_type = true;
    check_for_declarator_list(parser, offset_decl);
    if (!strcmp("enum", this_token->string)) {
      parser->is_enum = true;
      parser->start_delim = '\0';
      parser->end_delim = '\0';
      parser->separator = '\0';
    }
    if ((!strcmp("struct", this_token->string)) ||
        (!strcmp("union", this_token->string))) {
      parser->is_struct_or_union = true;
      parser->start_delim = '{';
      parser->end_delim = '}';
      parser->separator = ';';
    }
    if (parser->have_qualifier) {
      if (!qualifier_is_compatible_with_type(parser, this_token->string)) {
        return false;
      }
    }
    /*
     * The identifier which follows will be inside parens, so gettoken() will
     * fail unless the parsing context is adjusted first. Therefore the
     * function_ptr must be detected here.  Avoid a duplicate check if the
     * parser has already identified a function or function pointer.
     */
    if (!(parser->is_function || parser->is_function_ptr)) {
      if (!check_for_function_ptr(parser, offset_decl)) {
        parser->stacklen = 0;
        return false;
      }
    }
    break;
  case length:
    if ((!parser->have_identifier) || (!parser->array_dimensions)) {
      /* Indicate hard failure. */
      parser->stacklen = 0;
      this_token->kind = invalid;
      strncpy(this_token->string, "\0", 1);
      break;
    }
    /* The first array dimension is accounted for in the identifier block above.
     * Any subsequent ones with lengths are accounted for here. Subsequent
     * lengthless dimensions are accounted for in load_stack().  Perhaps
     * handling all this counting in the delimiter case below would have been
     * wiser.
     */
    if (parser->array_lengths) {
      parser->array_dimensions++;
    }
    parser->array_lengths++;
    if (parser->array_lengths > parser->array_dimensions) {
      /* The first dimension of an array must always have a declared length, so
       * the number of dimensions >= the number of lengths.  This code should be
       * unreachable since "[]" terminates the stack-loading.
       */
      parser->stacklen = 0;
    }
    break;
  case qualifier:
    if (!strcmp("*", this_token->string)) {
      /*
       * Don't set parser->have_qualifier, as that would cause restricted
       * pointers to fail.
       */
      parser->is_pointer = true;
    } else if (!strcmp("restrict", this_token->string)) {
      if (!parser->is_pointer || parser->have_qualifier) {
        fprintf(parser->err_stream, "The restrict qualifier only applies to "
                                    "otherwise unqualified pointers.\n");
        parser->stacklen = 0;
        return false;
      }
    } else if ((!strcmp("extern", this_token->string) ||
                !strcmp("static", this_token->string)) &&
               (parser->parent && parser->parent->has_function_params)) {
      /*
       * https://en.cppreference.com/c/language/storage_duration
       * "The extern specifier specifies static storage duration . . . It can
       * be used with function and object declarations in both file and block
       * scope (excluding function parameter lists)."   The same is true of
       * "static", but not of "volatile," which may apply to function
       * parameters per https://en.cppreference.com/c/language/volatile.
       */
      fprintf(parser->err_stream, "Function parameters cannot be %s.\n",
              this_token->string);
      parser->stacklen = 0;
      return false;
    } else {
      parser->have_qualifier = true;
    }
    break;
  case typedefn:
    parser->is_typedef = true;
    break;
  case invalid:
    /*
     * Compound types plus anonymous structs and unions result in empty tokens,
     * so don't print an error.
     */
    if (!(strlen(this_token->string))) {
      if (!parser->is_struct_or_union) {
        fprintf(parser->err_stream, "Cannot process empty token.\n");
      }
    } else {
      fprintf(parser->err_stream, "Cannot process invalid token %s\n",
              this_token->string);
      return false;
    }
    break;
  default:
    break;
  }
  return true;
}

/*
 * Adds an element created from this_token to parser->stack and increments
 * stacklen.
 */
void push_stack(struct parser_props *parser, struct token *this_token) {
  if (parser->stacklen >= MAXTOKENS) {
    fprintf(parser->err_stream, "\nStack overflow.\n");
    exit(-ENOMEM);
  }

  parser->stack[parser->stacklen].kind = this_token->kind;
  strlcpy(parser->stack[parser->stacklen].string, this_token->string,
          strlen(this_token->string) + 1);
  parser->stacklen++;
  return;
}

size_t load_stack(struct parser_props *parser, char *user_input) {
  struct token this_token;
  size_t increm = 0;
  this_token.kind = invalid;
  strcpy(this_token.string, "");
  while (parser->cursor <= strlen(user_input)) {
    /*
     * Finding the identifier terminates initial stack loading since it comes
     * last, as long as there are no function arguments or array delimiters.
     */
    while (strlen(parser->cursor + user_input)) {
      if ((!parser->is_declarator_list) && (this_token.kind == identifier)) {
        break;
      }
      increm = gettoken(parser, user_input + parser->cursor, &this_token);
      /* Reached end of input, or hit an error. */
      if (!increm || (invalid == this_token.kind)) {
        /* There is an error. */
        if (!parser->stacklen && !parser->prev) {
          return 0;
        }
        break;
      }
      parser->cursor += increm;
      /* Don't place "typedef" or ":" on the stack. */
      if (!strcmp("typedef", this_token.string)) {
        continue;
      }
      if ((type == this_token.kind) && (!strcmp("union", this_token.string) ||
                                        !strcmp("struct", this_token.string) ||
                                        !strcmp("enum", this_token.string))) {
        if (!handled_compound_type(parser, user_input, &this_token)) {
          return 0;
        }
      }
      /*
       * If there is no enum instance name, only a type declaration, then do not
       * place the encountered enumerator on the stack. Set
       * parser->have_identifier true so that pop_stack() doesn't fail.
       */
      if ((identifier == this_token.kind) &&
          (first_identifier_is_enumerator(parser, user_input))) {
        /*
         * Put the characters in the token and '{' back on the stack for
         * process_enum_constants().  This clumsy approach means that the parser
         * state upon entry to process_enum_constants() does not depend on
         * whether or not the enum has an instance name.
         */
        parser->cursor -= strlen(this_token.string) + 1;
        break;
      }
      push_stack(parser, &this_token);
    } /* while !parser->have_identifier */
    break;
  } /* while parser->cursor <= strlen(user_input) */
  if (!parser->have_identifier) {
    /*
     * As with enumerators, the instance name of a struct is optional.
     * While a function pointer itself must be named, the function
     * parameters need only have types.
     */
    if (!(parser->has_struct_or_union_members ||
          (parser->parent && (parser->parent->is_function_ptr ||
                              parser->parent->has_function_params)))) {
      parser->stacklen = 0;
      fprintf(parser->err_stream,
              "Input lacks required identifier or type element.\n");
      return 0;
    }
  }
  if (!handled_extended_parsing(parser, user_input, &this_token)) {
    parser->stacklen = 0;
    return 0;
  }
  if ((parser->cursor < strlen(user_input)) &&
      (has_any_name_chars(user_input + parser->cursor))) {
    if ((parser->has_function_params && strchr(user_input, ')')) ||
        (parser->has_struct_or_union_members && strchr(user_input, '}')) ||
        (parser->is_bitfield)) {
      if (!handled_extended_parsing(parser, user_input, &this_token)) {
        parser->stacklen = 0;
        return 0;
      }
    } else {
      /* There is unclassifiable junk at the end of the expression. */
      fprintf(parser->err_stream, "Expression ends with erroneous output: %s\n",
              user_input + parser->cursor);
      parser->stacklen = 0;
      return 0;
    }
  }
  reorder_stacks(parser);
#ifdef DEBUG
  showstack(parser->stack, parser->stacklen, parser->out_stream, __LINE__);
#endif
  return parser->cursor;
}

/********** functions to process user input **********/

/*
 * Returns true iff input is successfully parsed.  Actual parsing begins here.
 * The parser pointer is passed in rather than allocated here only because
 * doing so facilitates testing.
 */
bool input_parsing_successful(struct parser_props *parser, char inputstr[]) {
  /* Allocate and call strlcpy() in case the input is too long. */
  _cleanup_(freep) char *user_input = (char *)malloc(MAXTOKENLEN);
  _cleanup_(freep) char *trimmed = (char *)malloc(MAXTOKENLEN);

  strlcpy(user_input, inputstr, MAXTOKENLEN);
  if (!has_any_name_chars(user_input)) {
    fprintf(parser->err_stream, "Input lacks required elements: %s\n",
            user_input);
    return false;
  }
  if (!truncate_input(&user_input, parser)) {
    return false;
  }
  parser->cursor = trim_trailing_whitespace(user_input, trimmed);
  if (strlen(trimmed)) {
    strlcpy(user_input, trimmed, MAXTOKENLEN);
  }
  if (!load_stack(parser, user_input)) {
    return false;
  }
  if (0 == parser->stacklen) {
    fprintf(parser->err_stream, "Unable to parse garbled input.\n");
    release_parser_resources(parser);
    return false;
  }
#ifdef DEBUG
  showstack(parser->stack, parser->stacklen, parser->out_stream, __LINE__);
#endif
  if (!pop_all(parser)) {
    release_parser_resources(parser);
    return false;
  }
  fprintf(parser->out_stream, "\n");
  fflush(parser->out_stream);
  return true;
}

/* The FILE* parameter is provided for the unit test.
 * The function expects storage of size MAXTOKENLEN and a pointer to an open
 * stream.
 * The function returns the first newline-terminated line of the stream in
 * stdinp, unless that line is longer than MAXTOKENLEN, in which it is
 * truncated.
 */
size_t process_stdin(char stdinp[], FILE *input_stream) {
  char raw_input[MAXTOKENLEN];
  if (fgets(raw_input, MAXTOKENLEN, input_stream) != NULL) {
    /* A line from stdin which will always end in '\n'. */
    char *newline_pos = strstr(raw_input, "\n");
    if (NULL == newline_pos) {
      /* Input was truncated by fgets(). */
      fprintf(stderr,
              "Input from stdin must be less than %u characters long.\n",
              MAXTOKENLEN - 1);
    } else {
      /* Because fgets() writes to the array on the stack rather than stdinp,
         the pointer arithmetic is well-defined.
      */
      const size_t offset = newline_pos - &raw_input[0];
      /*
       * strlcpy() returns the source length, not the number of written bytes.
       * The +1 allows for the NULL.   Otherwise the last character will be
       * truncated.
       */
      (void)strlcpy(stdinp, raw_input, offset + 1);
      /*
        Return the lesser of the destination capacity and the requested copy
        length.
       */
      return ((MAXTOKENLEN - 1) < (offset + 1)) ? (MAXTOKENLEN - 1)
                                                : (offset + 1);
    }
  } else {
    fprintf(stderr, "Malformed input.\n");
  }
  return 0;
}

/*
 * The stream parameter is for the unit tests. from_user[] is the prompt.  That
 * part of the prompt which the program should analyze further is returned in
 * the inputstr[] array.  The function returns the number of characters in
 * input_str[].
 */
size_t find_input_string(const char from_user[], char inputstr[],
                         FILE *stream) {
  /*
   * Without the length check, providing "-val;" as input triggers a hang, as
   * process_stdin() never receives any chars in its fgets() call and waits
   * forever.
   */
  if ((1 == strlen(from_user)) && (from_user[0] == '-')) {
    return process_stdin(inputstr, stream);
  } else {
    /* read input from CLI */
    const size_t requested = strlcpy(inputstr, from_user, MAXTOKENLEN - 1);
    /*
     * Subtract 1 since strlcpy() ALWAYS apppends a NULL.
     * Return the lesser of the input length and the destination capacity.
     */
    return (requested < MAXTOKENLEN - 1) ? requested : MAXTOKENLEN - 1;
  }
}

#ifndef TESTING
int main(int argc, char **argv) {
  char inputstr[MAXTOKENLEN] = {0};
  struct parser_props parser;
  initialize_parser(&parser);

  if ((argc != 2)) {
    usage();
    limitations();
    exit(EXIT_SUCCESS);
  }
  /* For the case where input is provided on stdin, the strlen is 1 for '-'. */
  if (strlen(argv[1]) > (MAXTOKENLEN - 1)) {
    limitations();
    exit(-E2BIG);
  }
  if (!find_input_string(argv[1], inputstr, stdin)) {
    fprintf(stderr, "Input is either malformed or empty.\n");
    usage();
    limitations();
    exit(EINVAL);
  }
  if (!input_parsing_successful(&parser, inputstr)) {
    exit(EXIT_FAILURE);
  }
  printf("\n");
  exit(EXIT_SUCCESS);
}
#endif
