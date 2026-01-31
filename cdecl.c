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
#include <bsd/string.h>
#include <assert.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
/* For __fpurge() */
#include <stdio_ext.h>
#include <stdlib.h>
#include <string.h>

#define MAXTOKENLEN 64
#define MAXTOKENS 256
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#define _cleanup_(x) __attribute__((__cleanup__(x)))

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
  if ((!p) || (NULL == *(void **)p)) return;
  free(*(void **) p);
  *(void**)p = NULL;
}

const char *types[] = {"char", "short", "int", "float", "double",
                       "long", "struct", "enum", "union", "void", "int8_t",
                       "uint8_t", "int8_t", "uint16_t", "int16_t", "uint32_t",
                       "int32_t", "uint64_t", "int64_t"};
const char *qualifiers[] = {"const", "volatile", "static", "*", "extern",
                            "unsigned", "restrict"};
enum token_class { invalid = 0, type, qualifier, identifier, length,
                   whitespace };
const char *kind_names[] = {"invalid", "type", "qualifier", "identifier",
                            "length", "whitespace" };

struct token {
  enum token_class kind;
  char string[MAXTOKENLEN];
};

/*
 * A well-formed declaration must have exactly one of each of the following:
 * a type;
 * an identifier;
 * a terminating ';' or '='.
 */
struct parser_props {
  bool have_identifier;
  bool have_type;
  bool last_dimension_unspecified;
  bool is_function;
  bool is_enum;
  bool has_enumerators;
  char enumerator_list[MAXTOKENLEN];
  size_t array_dimensions;
  size_t array_lengths;
  bool has_function_params;
  size_t stacklen;
  struct token stack[MAXTOKENS];
  struct parser_props *prev;
  struct parser_props *next;
  FILE *out_stream;
  FILE *err_stream;
};

void reset_parser(struct parser_props* parser){
  parser->have_identifier = false;
  parser->have_type = false;
  parser->last_dimension_unspecified = true;
  parser->is_function = false;
  parser->is_enum = false;
  parser->has_enumerators = false;
  parser->enumerator_list[0] = '\0';
  parser->array_dimensions = 0;
  parser->array_lengths = 0;
  parser->has_function_params = false;
  parser->stacklen = 0;
  parser->stack[0].kind = invalid;
  parser->prev = NULL;
  parser->next = NULL;
}

void initialize_parser(struct parser_props* parser) {
  reset_parser(parser);
  parser->out_stream = stdout;
  parser->err_stream = stderr;
}

void usage(void) {
  printf("\ncdecl prints out the English language form of a C declaration.\n");
  printf("Invoke as 'cdecl <declaration>' or\n");
  printf("provide input on stdin and use '-' as the single command-line "
         "argument.\n");
  printf("Input must be terminated with a semicolon and enclosed in quotation marks.\n");
}

void limitations() {
  printf("Input must be shorter than %u characters, not including quotation marks and semicolon.\n",
	 MAXTOKENLEN);
  printf("Known deficiencies:\n\ta) doesn't handle multi-line struct and union "
         "declarations;\n");
  printf("\tb) doesn't handle multiple comma-separated declarations;\n");
  printf("\tc) includes only the qualifiers defined in ANSI C, not LIBC\n");
  printf("\t   or kernel extensions.\n");
  exit(-1);
}

bool is_all_blanks(const char* input) {
  if (!input || !strlen(input)) {
    return false;
  }
  char* token_copy = strdup(input);
  _cleanup_(freep) char *saveptr = token_copy;
  while (token_copy && isprint(*token_copy) && isblank(*token_copy)) {
      token_copy++;
  }
  // Reached end of the string.
  if (!strlen(token_copy)) {
    return true;
  }
  return false;
}

/*
 * Return value is the number of trimmed characters.
 * trimmed is the same as input except that it will end in a non-whitespace character.
 * If there are no non-whitespace characters, trimmed will be empty.
 * Caller must allocate trimmed.
 */
size_t trim_trailing_whitespace(const char* input, char* trimmed) {
  if (!input || (0 == strlen(input))) {
    return 0;
  }
  if (is_all_blanks(input)) {
    bzero(trimmed, MAXTOKENLEN);
    return(strlen(input));
  }
  _cleanup_(freep) char* copy = strdup(input);
  char* last_char = copy + (strlen(copy) - 1);
  // last_char should be greater than input when the loop exits, as otherwise input is all blanks.
  size_t removed = 0;
  while ((last_char > copy) && (isblank(*last_char))) {
    last_char--;
    removed++;
  }
  if ((copy + (strlen(copy) - 1)) == last_char) {
    return 0;
  }
  /* Copy the non-blank part of the input to the output.*/
  for (char* cptr = copy; cptr <= last_char; cptr++ ) {
    *trimmed = *cptr;
    trimmed++;
  }
  *trimmed = '\0';
  return removed;
}

/* Caller must allocate trimmed. */
size_t trim_leading_whitespace(const char* input, char* trimmed) {
  char* copy = strdup(input);
  _cleanup_(freep) char* saveptr = copy;
  size_t removed = 0;
  /* Make sure trimmed starts empty. */
  *trimmed = '\0';
  if (!input || (0 == strlen(input)) || (is_all_blanks(input))) {
    return 0;
  }
  if (!isblank(*input)) {
    return 0;
  }
  while (copy && isblank(*copy)) {
    copy++;
    removed++;
  }
  /* Copy the non-blank part of the input to the output.*/
  while (*copy) {
    *trimmed = *copy;
    trimmed++;
    copy++;
  }
  *trimmed = '\0';
  return removed;
}

bool has_alnum_chars(const char* input) {
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

bool is_numeric(const char* input) {
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

/*
 * Remove any characters following ';' , ')' or '=' plus any whitespace which
 * precedes these characters.  Returns true if the input contains one of those
 * chars and has any non-whitespace characters before them.
 */
bool truncate_input(char** input, const struct parser_props *parser) {
  char trimmed[MAXTOKENLEN];
  char *input_end = NULL;
  if (!parser->is_function) {
    /* Dump chars after '=', if any. */
    input_end = strstr(*input, "=");
  }
  /*
   * If the input after '=' or ',' is not lopped off, the input should terminate
   * with ';' or ')'.
   */
  if (!input_end) {
    if ((!parser->is_function)) {
      input_end = strstr(*input, ";");
    } else {
      input_end = strstr(*input, ")");
    }
   /* Input with two semicolons or ')' could reach this point. */
    if (input_end == *input) {
      fprintf(parser->err_stream, "Zero-length input string.\n");
      return false;
    } else if (!input_end) { /* There are no terminators. */
      fprintf(parser->err_stream, "\nImproperly terminated declaration.\n");
      return false;
    }
  }
  *input_end = '\0';
  if (trim_trailing_whitespace(*input, trimmed)) {
    strlcpy(*input, trimmed, MAXTOKENLEN);
  }
  if (!strlen(*input)) {
    fprintf(parser->err_stream, "Zero-length input string.\n");
    return false;
  }
  return true;
}

enum token_class get_kind(const char *intoken) {
  size_t numel = 0, ctr;

  if ((!intoken) || (!strlen(intoken))) {
    return invalid;
  }
  if (is_all_blanks(intoken)) {
    return whitespace;
  }

  numel = ARRAY_SIZE(types);
  for (ctr = 0; ctr < numel; ctr++) {
    if (!strcmp(intoken, types[ctr]))
      return (type);
  }

  numel = ARRAY_SIZE(qualifiers);
  for (ctr = 0; ctr < numel; ctr++) {
    if (!strcmp(intoken, qualifiers[ctr]))
      return (qualifier);
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

void show_parser_list(const struct parser_props *parser) {
  struct parser_props *pnext = parser->next;
  if (!pnext) {
    fprintf(stderr, "\nNo subsidiary parsers.\n");
    return;
  }
  /* The list head is a stack allocation. */
  fprintf(stderr, "HEAD: %p-->", parser);
  while (pnext) {
    fprintf(stderr, "%p", pnext);
    pnext = pnext->next;
    if (pnext) {
      fprintf(stderr, "-->");
    } else {
      fprintf(stderr, "\n");
      break;
    }
  }
}

void showstack(const struct token* stack, const size_t stacklen, FILE* out_stream) {

  size_t tokennum = 0, ctr;

  fprintf(out_stream, "Stack is:\n");
  for (ctr = 0; ctr < stacklen; ctr++) {
    fprintf(out_stream, "Token number %lu has kind %s and string %s\n", tokennum,
           kind_names[stack[ctr].kind], stack[ctr].string);
    tokennum++;
  }

  return;
}

size_t load_stack(struct parser_props* parser, char* user_input,
		  bool needs_truncation);

struct parser_props *make_parser(struct parser_props* const parser) {
    struct parser_props *new_parser = (struct parser_props *)malloc(sizeof(struct parser_props));
#ifdef TESTING
    fprintf(stderr, "Allocated %p\n", new_parser);
#endif
    if(!new_parser) {
      exit(ENOMEM);
    }
    initialize_parser(new_parser);
    new_parser->out_stream = parser->out_stream;
    new_parser->err_stream = parser->err_stream;
    parser->next = new_parser;
    new_parser->prev = parser;
    return new_parser;
}

bool overwrite_trailing_delim(char **output, const char *input, const char delim) {
  const char *param_end = strchr(input, delim);
  if (!param_end) {
    return false;
  }
  size_t param_len = param_end - input;
  /* Copy the entire input, as otherwise there is no trailing NULL. */
  strncpy(*output, input, param_len+2);
  /* Overwrite the delimiter with a NULL. */
  *(*output + param_len) = '\0';
  return true;
}

/* The list head is a stack allocation. */
void free_all_parsers(struct parser_props *parser) {
  while (parser->next) {
    struct parser_props *save = parser->next->next;
#ifdef TESTING
            fprintf(stderr, "free_all_parsers(): freeing %p\n", parser->next);
#endif
    free(parser->next);
    parser->next = save;
  }
}

/*
 * function_params_cleanup() is the error handler for process_function_params().
 * It frees subsidiary parsers linked through the top-level one and resets the
 * top-level one to signal failure to calling code.
 */
void function_params_cleanup(void *parserp) {
  if (!parserp || !(*(struct parser_props ***)parserp) ||
      !(**(struct parser_props***)parserp)) return;
  struct parser_props *parser = **((struct parser_props ***)parserp);
  free_all_parsers(parser);
  reset_parser(parser);
}

/*
 * Spawn a new parser for each function parameter and link it into a
 * list whose head is the top-level parser.  Note that the function
 * name is on the original parser's stack.
 */
bool process_function_params(struct parser_props *parser, char* user_input,
                             size_t *offset, char** progress_ptr) {
  struct parser_props *params_parser;
  struct parser_props *tail_parser = parser;
  size_t increm = 0;
  _cleanup_(freep) char *next_param = (char *)malloc(MAXTOKENLEN);
  /*
   * Create a pointer which the code doesn't otherwise need as a peg
   * on which to hang the function's error handler.  The approach is
   * inspired by rzg2l_irqc_common_init() in
   * https://github.com/linux4microchip/linux/blob/4d72aeabedfa202d12869e52c40eeabc5401c839/
   * drivers/irqchip/irq-renesas-rzg2l.c#L596
   */
  _cleanup_(function_params_cleanup)struct parser_props **dummy_parserp = &parser;

  /* Guard against the caller swapping the two character strings */
  if (strlen(user_input) < strlen(*progress_ptr)) {
    fprintf(parser->err_stream, "Swapped parameters in %s\n", __func__);
    abort();
  }

  if (!parser->has_function_params) {
    return false;
  }
  /* +1 to go past '('. */
  *progress_ptr = user_input + *offset + 1;
  while (strlen(*progress_ptr)) {
    // Freed in pop_stack().
    params_parser = make_parser(tail_parser);
    /* There may be more than one function parameter yet to process. */
    if (strchr(*progress_ptr, ',')) {
      if (!overwrite_trailing_delim(&next_param, *progress_ptr, ',')) {
        fprintf(parser->err_stream, "Failed to process list function args %s\n", next_param);
      }
      increm = load_stack(params_parser, next_param, false);
      if (!increm) {
        fprintf(parser->err_stream, "Failed to load list function parameter %s\n", next_param);
	return false;
      }
      *offset += increm;
      /* +1 to go past ','. The comma is not included in offset. */
      *progress_ptr += increm + 1;
#ifdef TESTING
      show_parser_list(parser);
#endif
    } else {
      /*
       * There is only one remaining parameter.
       * Pass progress_ptr rather than user_input since the params parser only
       * processes what's inside the parentheses.
       */
      if (!overwrite_trailing_delim(&next_param, *progress_ptr, ')')) {
        fprintf(parser->err_stream, "Failed to process last function arg\n");
	return false;
      }
      increm = load_stack(params_parser, next_param, false);
      if (!increm) {
        fprintf(parser->err_stream, "Failed to load last function arg %s\n", next_param);
        return false;
      }
      /* 1 is for ')'. */
      *offset += increm + 1;
      *progress_ptr = user_input + *offset; /* Skip past ')'. */
#ifdef TESTING
  show_parser_list(parser);
#endif
       break;
    }
    tail_parser = params_parser;
  }
  /*
   * Prevent the error handler from running by, essentially, putting the
   * dummy_parserp flag down.
   */
  dummy_parserp = NULL;
  return true;
}

/* Do not allow any characters in identifiers besides a-z, '_' and '-'. */
static bool is_name_char(const char c) {
  if (isalpha(c) || ('-' == c) || ('_' == c)) {
    return true;
  }
  return false;
}

const char typechars[] = {'1', '2', '3', '4', '6', '8', 'a', 'b', 'c', 'd', 'e',
			  'f', 'g', 'h', 'i', 'l', 'n', 'o', 'r', 's', 't', 'u'};

static bool is_type_char(const char c) {
  for (long unsigned i=0; i < ARRAY_SIZE(typechars); i++) {
    if (c == typechars[i]) {
      return true;
    }
  }
  return false;
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
  reset_parser(parser);
  return false;
}

/* A true return value means no errors. */
bool check_for_function_parameters(struct parser_props *parser,
				   const char *offset_decl) {
  if ('(' != *offset_decl) {
    return true;
  }
  const char *params_end = strstr(offset_decl, ")");
  if (!params_end) {
    reset_parser(parser);
    fprintf(parser->err_stream, "Malformed function declaration.\n");
    return false;
  }
  parser->is_function = true;
  /* No function parameters since types are at least 3 chars long. */
  if (3 > (params_end - offset_decl)) {
    return true;
  }
  if (is_all_blanks(offset_decl)) {
    return true;
  }
  /*
   * There is possibly a type within the parentheses and thus a function
   * parameter. */
  parser->has_function_params = true;
  return true;
}

bool have_stacked_compound_type(const struct parser_props *parser) {
  const char *spacepos = strchr(parser->stack[0].string, ' ');
  if ((!parser) || (type != parser->stack[0].kind) || (NULL == spacepos)) {
   return false;
  }
  return true;
}

/*
 * Check to see if there are any identifiers which do not appear in enumerator
 * list.  If so, the identifier must be the name of an enum instance.
 */
bool all_identifiers_are_enumerators(const struct parser_props *parser) {
  size_t stacknum = parser->stacklen;
  if (!parser->has_enumerators) {
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



/*
 * A true return value means no errors.
 * While "enum State;" is invalid, the following are correct:
 * 0. enum State state; 1. enum State { SOLID, LIQUID };
 * 2. enum State { SOLID = 1, LIQUID = 3};
 * Not supported by this program:
 * 3. enum State { SOLID, LIQUID} state;
 */
bool check_for_enumerators(struct parser_props *parser, const char *offset_decl) {
  const char *spacep = strstr(offset_decl, " ");
  const char *startbracep = strstr(offset_decl, "{");
  const char *endbracep  = strstr(offset_decl, "}");

  /*
   * If the declaration is not an enum, or we've already found enumerators,
   *  there is nothing to check.
   */
  if ((!parser->is_enum) || (parser->has_enumerators)) return true;
  /*
   * The enumerators were already checked when handling the compound type
   * name.
   */
  if (parser->has_enumerators) return true;
  if ((NULL == spacep) && (!have_stacked_compound_type(parser))) {
    fprintf(parser->err_stream, "Enums cannot be forward-declared.\n");
    return false;
    }
  /* The declaration may be of type 0. */
  if (NULL == startbracep) {
      if (NULL == endbracep) {
        return true;
      }
      fprintf(stderr, "\nMalformed enumerator declaration %s.\n", offset_decl);
      return false;
  }
  if ((spacep > startbracep) || (startbracep > endbracep)) {
    return false;
  }
  /* The presence of enumerators is plausible in types 1 or 2. */
  parser->has_enumerators = true;
  return true;
}

/*
 * Append the name of a struct or union to the type name since in
 * "struct task_struct ts;", the type is "struct task_struct", not
 * "struct".  The parser has already encountered "union", "struct" or
 * "enum", so progress_ptr+offset should point past one of them.  The
 * return value indicates success or failure.
 */
bool handled_compound_type(const char *progress_ptr, struct token *this_token,
                           size_t *offset) {
  char compound_type_name[MAXTOKENLEN];
  char *name_end_ptr;
  size_t existing_token_len;
  size_t i;
  size_t j;

  /*
   * Starting with "struct page *pp", compound type name is "page" since "pp" is
   * the identifier, not handled by this function.
   */
  *offset += trim_leading_whitespace(progress_ptr+*offset,
				     &compound_type_name[0]);
  /*
   * Since there's no leading whitespace, the next blank terminates the compound
   * identifier.
   */
  name_end_ptr = strchr(compound_type_name, ' ');
  /*
   * The declaration is something like "struct task_struct;", not "struct
   * task_struct ts;" so no further action is needed.  Leave identifier for subsequent code to process.
   */
  if (!name_end_ptr) {
    return true;
  }

  existing_token_len = strlen(this_token->string);
  this_token->string[existing_token_len] = ' ';
  /* +1 for the space and +1 for terminating NULL */
  if ((existing_token_len + strlen(compound_type_name) + 2) > MAXTOKENLEN) {
    return false;
  }
  j = 0;
  for (i = existing_token_len+1; i < existing_token_len + 1 + strlen(compound_type_name); i++, j++) {
    /* Reached the end of the compound_type_name, so stop before copying the identifier into the type. */
    if (' ' == compound_type_name[j]) break;
    this_token->string[i] = compound_type_name[j];
  }
  this_token->string[i] = '\0';
  /*
   * The 1 accounts for the space.  The characters left in progress_ptr should
   *  be an identifier which names the instance of the compound type.
   */
  *offset += j + 1;
  return true;
}

/*
 * finish token() receives an unterminated string from gettoken() which it
 * readies for pushing onto the stack.
 */
void finish_token(struct parser_props* parser, const char *offset_decl,
		  struct token *this_token, const size_t ctr) {
  this_token->string[ctr + 1] = '\0';
  this_token->kind = get_kind(this_token->string);
  switch(this_token->kind) {
  case identifier:
    if ((!parser->have_type) ||
	(!check_for_array_dimensions(parser, offset_decl)) ||
	(!check_for_function_parameters(parser, offset_decl)) ||
	(!check_for_enumerators(parser, offset_decl))) {
      /* Indicate hard failure. */
      reset_parser(parser);
      return;
    }
    parser->have_identifier = true;
    break;
  case type:
    parser->have_type = true;
    if (!strcmp("enum",this_token->string)) {
      parser->is_enum = true;
    }
    break;
  case length:
    if ((!parser->have_identifier) || (!parser->array_dimensions)) {
      /* Indicate hard failure. */
      reset_parser(parser);
      this_token->kind = invalid;
      strcpy(this_token->string, "");
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
      reset_parser(parser);
    }
    break;
  case qualifier:
      __attribute__((fallthrough));
  case invalid:
      __attribute__((fallthrough));
  default:
    break;
  }
}

size_t process_array_length(struct parser_props* parser,
			    const char* offset_string, struct token* this_token) {
    size_t ctr = 0;
    /* Check if the array is ill-formed. */
    if (NULL == strstr(offset_string, "]")) {
      /* Indicate hard failure. */
      reset_parser(parser);
      return 0;
    }
    for (ctr = 0; offset_string && isdigit(*(offset_string+ctr)) &&
	   ctr < MAXTOKENLEN; ctr++) {
      this_token->string[ctr] = *(offset_string + ctr);
    }
    this_token->kind = length;
    finish_token(parser, offset_string, this_token, ctr);
    return ctr;
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
size_t gettoken(struct parser_props* parser, const char *declstring,
		struct token *this_token) {

  const size_t tokenlen = strlen(declstring);
  size_t tokenoffset = 0;
  /*
   * ctr may = -1 if the first character is '{' since the ctr loop
   * below will otherwise fail to write the first character of the
   * output.
   */
  int ctr = 0;
  char trimmed[MAXTOKENLEN];
  memset(this_token->string, '\0', MAXTOKENLEN);
  this_token->kind = invalid;

  if (!tokenlen) {
    return 0;
  }
  if (tokenlen > (MAXTOKENLEN-1)) {
    fprintf(stderr, "\nToken too long %s.\n", declstring);
    return 0;
  }

  /* Move past leading whitespace. */
  const size_t trimnum = trim_leading_whitespace(declstring, trimmed);
  tokenoffset = trimnum;
  /* Move past leading '-' in identifier.  Leading underscores are okay. */
  while ('-' == *(declstring + tokenoffset)) {
    tokenoffset++;
  }

  /* Process array length, if any. We should already have an identifier. */
  if (parser->array_dimensions) {
    tokenoffset += process_array_length(parser, declstring + tokenoffset,
					this_token);
    return tokenoffset;
  }

  /*
   * The token is a single character.  Even stdint types include numerals, they
   *  do not begin with them.
   */
  if ('*' == *(declstring + tokenoffset)) {
    strlcpy(this_token->string, "*", 2);
    tokenoffset++;
    finish_token(parser, declstring + tokenoffset, this_token, ctr);
    return tokenoffset;
  }

  /* The token has multiple characters, so copy them all. */
  for (ctr = 0; ctr <= (int)tokenlen; ctr++) {
    char nextchar = *(declstring + tokenoffset);
    if (parser->have_type) {
      /*
       * We are looking for an identifier. Finding an identifier terminates
       * parsing unless we are processing an enum with an enumerator list.
       */
      if (!is_name_char(nextchar)) {
        if (parser->is_enum && ('{' == nextchar)) {
          /*
	   * Setting has_enumerators prevents check_for_enumerators() from
	   * running later.
	   */
          parser->has_enumerators = true;
          tokenoffset++;
          /*
	   * The loop increases the ctr without having written anything to the
	   * output.
	   */
	  ctr--;
	  continue;
        }
        break;
      }
    } else {
      /*
       * We are looking for a type but we might first see a qualifier composed
       * of name_chars.
       */
      if (!is_name_char(nextchar) && !is_type_char(nextchar)) {
        break;
      }
    }
    this_token->string[ctr] = nextchar;
    tokenoffset++;
  }
  /* Overwrite any trailing dash with a NUL. */
  if ('-' == this_token->string[ctr-1]) {
    this_token->string[ctr-1] = '\0';
  }
  finish_token(parser, declstring + tokenoffset, this_token, ctr);
  return tokenoffset;
}

/*
 * Adds an element created from this_token to parser->stack and increments
 * stacklen.
 */
void push_stack(struct parser_props* parser, struct token* this_token) {
  if (parser->stacklen >= MAXTOKENS) {
    fprintf(parser->err_stream, "\nStack overflow.\n");
    exit(-ENOMEM);
  }

  parser->stack[parser->stacklen].kind = this_token->kind;
  strcpy(parser->stack[parser->stacklen].string, this_token->string);
  parser->stacklen++;
  return;
}

int pop_all(struct parser_props *parser);

/* Return 0 on success, an error code on failure */
int pop_stack(struct parser_props* parser, bool no_enum_instance) {
  int ret;
  /* Last element of stack with stacklen=n is at index = n-1. */
  const size_t stacktop = parser->stacklen - 1;
  if (!parser->stacklen) {
    fprintf(parser->err_stream, "Attempt to pop empty stack.\n");
    return -ENODATA;
  }
  /*
   * Qualifiers following * apply to the pointer itself, rather than to the
   * object to which the pointer points.
   */
  if (!strcmp(parser->stack[stacktop].string, "*")) {
    fprintf(parser->out_stream, "pointer(s) to ");
  } else {
    switch (parser->stack[stacktop].kind) {
    case whitespace:
      __attribute__((fallthrough));
    case qualifier:
      fprintf(parser->out_stream, "%s ", parser->stack[stacktop].string);
      break;
    /* Process the function parameters right after processing the return value of a function.  */
    case type:
      fprintf(parser->out_stream, "%s ", parser->stack[stacktop].string);
        /*
	 * If the function is itself part of a union or struct, then
	 * parser->next could be populated without function params.
	 */
        if(parser->has_function_params) {
          struct parser_props *cursor = parser->next;
	  size_t depth = 0;
          while (cursor && cursor->stacklen) {
            if (depth) {
              fprintf(parser->out_stream, "and ");
            } else {
              fprintf(parser->out_stream, "and takes param(s) ");
	    }
            ret = pop_all(cursor);
	    depth++;
            struct parser_props *save = cursor->next;
#ifdef TESTING
            fprintf(stderr, "pop_stack(): freeing %p\n", cursor);
#endif
            free(cursor);
	    cursor = save;
	    if (ret) return ret;
          }
	}
        if (parser->has_enumerators) {
	  if (no_enum_instance) {
	    fprintf(parser->out_stream, "has enumerator(s)");
	  } else {
	    fprintf(parser->out_stream, "with enumerator(s)");
	  }
          fprintf(parser->out_stream, " %s ", parser->enumerator_list);
        }
      break;
    case identifier:
      if (parser->array_dimensions) {
        fprintf(parser->out_stream, "%s is an array of ",
		parser->stack[stacktop].string);
      } else if (parser->is_function) {
        fprintf(parser->out_stream, "%s is a function which returns ",
	        parser->stack[stacktop].string);
      } else if ((parser->has_enumerators) && strlen(parser->enumerator_list) &&
		 (strstr(parser->enumerator_list, parser->stack[stacktop].string))) {
        /*
	 * If the identifier is in the enumerator_list, it is not the
	 * name of a enum instance.
	 */
	  break;
      } else {
        fprintf(parser->out_stream, "%s is a(n) ",
		parser->stack[stacktop].string);
      }
      break;
    case length:
      if (parser->array_dimensions) {
        fprintf(parser->out_stream, "%s", parser->stack[stacktop].string);
	if (parser->array_lengths > 1) {
	  fprintf(parser->out_stream, "x");
	} else {
          if (parser->last_dimension_unspecified)  {
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
      __attribute__((fallthrough));
    default:
      fprintf(parser->err_stream, "\nError: element %s is of unknown type %d.\n",
              parser->stack[stacktop].string, parser->stack[stacktop].kind);
      return -EINVAL;
    }
  }
  memset(parser->stack[stacktop].string, '\0', MAXTOKENLEN);
  parser->stack[stacktop].kind = invalid;
  return 0;
}

int pop_all(struct parser_props *parser) {
  int ret;
  /* If there is a non-enumerator identifier, it will be at the top of
   * the stack.  Therefore, the comparison with the enumerator_list
   * must proceed the first call to pop_stack() and be passed to it.
   */
  const bool no_enum_instance = all_identifiers_are_enumerators(parser);
  while (parser && parser->stacklen) {
    ret = pop_stack(parser, no_enum_instance);
    if (ret) return ret;
    parser->stacklen--;
  }
  return 0;
}

void reverse_lengths(struct parser_props* parser) {
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
 * If the declaration describes a 1-dimensional array with a specified length,
 * the top of the stack holds the array lengths and the element below them is
 * the identifier.
 */
void reorder_array_identifier_and_lengths(struct parser_props* parser) {
  if (!parser->array_lengths) {
    return;
  }
  const size_t stacklast = parser->stacklen - 1;
  /*
   * The identifier name starts at the top.  We want it below the array
   * dimensions.
   */
  size_t unprocessed_lengths = parser->array_lengths;
  // Move the identifier to the stack top by swapping it with each identifier in turn.
  while (unprocessed_lengths){
    struct token name = parser->stack[stacklast -unprocessed_lengths];
    struct token arraylen = parser->stack[(stacklast - unprocessed_lengths)+1];
    if ((length != arraylen.kind) || (identifier != name.kind)) {
      return;
    }
    strlcpy(parser->stack[(stacklast-unprocessed_lengths)+1].string, name.string, MAXTOKENLEN);
    strlcpy(parser->stack[stacklast-unprocessed_lengths].string, arraylen.string, MAXTOKENLEN);
    parser->stack[(stacklast-unprocessed_lengths)+1].kind = identifier;
    parser->stack[stacklast-unprocessed_lengths].kind = length;
    unprocessed_lengths--;
  }
  if (parser->array_lengths > 1) {
    reverse_lengths(parser);
  }
}

/*
 * Qualifiers modify the type, not the identifier. Therefore reorder qualifiers
 * and types on the stack in order to produce corector output.
 */
void reorder_qualifier_and_type(struct parser_props* parser) {
  if (parser->have_type) {
    size_t stacktop = parser->stacklen-1;
    while (stacktop) {
      if ((type == parser->stack[stacktop].kind) &&
	  (qualifier == parser->stack[stacktop-1].kind) &&
	  (0 != strcmp("*", parser->stack[stacktop-1].string))) {
        /* Save type element's string. */
        char type_name[MAXTOKENLEN];
	strlcpy(type_name, parser->stack[stacktop].string, MAXTOKENLEN);
        /* Overwrite type (top element) with the 2nd element from top (qualifier). */
	parser->stack[stacktop].kind = qualifier;
	strlcpy(parser->stack[stacktop].string, parser->stack[stacktop-1].string,
		MAXTOKENLEN);
        /* Complete the swap. */
	parser->stack[stacktop-1].kind = type;
	strlcpy(parser->stack[stacktop-1].string, type_name, MAXTOKENLEN);
      }
      stacktop -= 1;
    }
  }
}


void process_array_dimensions(struct parser_props* parser, char* user_input,
			      size_t *offset, char** progress_ptr, struct token* this_token) {
  char *next_dim;

  /* Guard against the caller swapping the two character strings */
  if (strlen(user_input) < strlen(*progress_ptr)) {
    fprintf(parser->err_stream, "Swapped parameters in %s\n", __func__);
    abort();
  }

  do {
    /* Skip '['. */
    (*offset)++;
    *progress_ptr = user_input + *offset;
    /* We've encountered "[]", which always terminates C array-length declarations.
     * Return without incrementing offset.
     */
    if (']' == **progress_ptr) {
      /*
       * The first increment of array_dimensions happens in finish_token() after
       * finding the identifier.
       * The first dimension must have a length.  If we've observed a length, the
       * dimension is not the first, so the counter should be incremented.
       */
      if (parser->array_lengths) {
        parser->array_dimensions++;
      }
      break;
    }
    (*offset) += gettoken(parser, *progress_ptr, this_token);
    if ((length == this_token->kind) && (strlen(this_token->string))) {
      push_stack(parser, this_token);
    }
    next_dim = strstr(user_input+*offset, "[");
    if (next_dim) {
      if (2 >= strlen(next_dim)) {
        /* No array length; return without incrementing offset. */
        parser->array_dimensions++;
        break;
      }
    }
  } while ((NULL != next_dim) && (*offset <= strlen(user_input)));
}

/*
 * check_for_enumerators() has performed limited sanity-checking for
 * enumerators and set has_enumerators = true, but there still may not
 * be any.  Return false if an error is encountered.  Otherwise, add
 * any enumerators to the enumerator_list and return true.
 */
bool process_enumerators(struct parser_props *parser, const char* user_input,
			 size_t *offset, char** progress_ptr) {
  size_t brace_offset;
  const char *startbracep = strchr(user_input, '{');
  char *commapos = strchr((char *)user_input, ',');
  struct token this_token;

  /* Guard against the caller swapping the two character strings */
  if (strlen(user_input) < strlen(*progress_ptr)) {
    fprintf(parser->err_stream, "Swapped parameters in %s\n", __func__);
    abort();
  }

  if (startbracep) {
    brace_offset = startbracep - (user_input+*offset);
    /* 1 is to go past '{'. */
    *offset += brace_offset + 1;
  } else {
    parser->has_enumerators = false;
    return false;
  }
  *progress_ptr = (char *)user_input + *offset;
  (*offset) += gettoken(parser, user_input + *offset, &this_token);
  *progress_ptr = (char *)user_input + *offset;
  /*
   * The classifier should assess the enumerators as identifiers, but
   * the type is not used.  Strictly speaking, the code should fail if
   * it encounters an identifier when the parser already has one, but
   * instead it ignores identifiers after the first, except in this case.
   */
  if ((invalid == this_token.kind) || (0 == strlen(this_token.string)) ||
      (strlen(this_token.string) > MAXTOKENLEN)) {
    reset_parser(parser);
    return false;
  }
  strlcpy(parser->enumerator_list, this_token.string,
	  strlen(this_token.string) + 1);
  if (!commapos) {
    return true;
  }
  /* REMOVE */
  return false;
}

// Only the tests make use of the return value.
size_t load_stack(struct parser_props* parser, char* user_input, bool needs_truncation) {
  struct token this_token;
  char trimmed[MAXTOKENLEN];
  char* progress_ptr = user_input;
  /*
   * offset is the number of characters consumed by gettoken().
   * offset >= strlen(this_token->string) since leading whitespace in
   * user_input will be skipped.
   */
  size_t offset = 0;
  if (needs_truncation) {
   if (!truncate_input(&user_input, parser)) {
      free_all_parsers(parser);
      return 0;
    }
  }
  this_token.kind = invalid;
  strcpy(this_token.string, "");
  while (offset <= strlen(user_input)) {
    /*
     * Finding the identifier terminates initial stack loading since it comes
     * last, as long as there are no function arguments or array delimiters.
     */
    while (this_token.kind != identifier) {
      progress_ptr = user_input + offset;
      offset += gettoken(parser, progress_ptr, &this_token);
      if ((!offset) || (invalid == this_token.kind)) {
	break;
      }
      size_t trailing = trim_trailing_whitespace(user_input, trimmed);
      if (trailing) {
        strlcpy(user_input, trimmed, MAXTOKENLEN);
        offset += trailing;
      }
      if ((type == this_token.kind) && (!strcmp("union", this_token.string) ||
                                        !strcmp("struct", this_token.string) ||
                                        !strcmp("enum", this_token.string))) {
        if (!handled_compound_type(progress_ptr, &this_token, &offset)) {
          free_all_parsers(parser);
          return 0;
        }
      }
      push_stack(parser, &this_token);
    }
    if (parser->array_dimensions) {
      process_array_dimensions(parser, user_input, &offset, &progress_ptr,
			       &this_token);
    }
    /* Either the parameter list is "()" and there are no new characters
     * processed, or there are new function parameters,
     * each handled by a new parser.
     */
    if (parser->has_function_params) {
      /* Move past the already-processed characters and '('. */
      if (!process_function_params(parser, user_input, &offset,
				   &progress_ptr)) {
	reset_parser(parser);
	return 0;
      }
    }
    break;
  }
  if (!parser->have_identifier) {
    free_all_parsers(parser);
    return 0;
  }
  if (parser->array_dimensions &&
      (parser->array_dimensions == parser->array_lengths)) {
    parser->last_dimension_unspecified = false;
  }
  reorder_array_identifier_and_lengths(parser);
  reorder_qualifier_and_type(parser);
  if (parser->has_enumerators) {
      if (!process_enumerators(parser, user_input, &offset, &progress_ptr)) {
	reset_parser(parser);
	return 0;
      }
  }
  struct parser_props *next_parser = parser->next;
  while (next_parser) {
    reorder_array_identifier_and_lengths(next_parser);
    reorder_qualifier_and_type(next_parser);
    next_parser = next_parser->next;
  }
#ifdef TESTING
  showstack(parser->stack, parser->stacklen, parser->out_stream);
#endif
  return offset;   /* Only used by tests. */
}

/*
 * Returns true iff input is successfully parsed.  Actual parsing begins here.
 * The parser pointer is passed in rather than allocated here only because
 * doing so facilitates testing.
 */
bool input_parsing_successful(struct parser_props *parser, char inputstr[]) {
  /* Allocate and call strlcpy() in case the input is too long. */
  _cleanup_(freep) char *user_input = (char *)malloc(MAXTOKENLEN);
  strlcpy(user_input, inputstr, MAXTOKENLEN);
  load_stack(parser, user_input, true);
  if (0 == parser->stacklen) {
    fprintf(parser->err_stream, "Unable to parse garbled input.\n");
    free_all_parsers(parser);
    return false;
  }
  if ((!parser->have_type) || (!parser->have_identifier && (!strlen(parser->enumerator_list)))) {
    fprintf(parser->err_stream, "Input lacks required identifier or type element.\n");
    free_all_parsers(parser);
    return false;
  }
#ifdef TESTING
  showstack(parser->stack, parser->stacklen, parser->out_stream);
#endif
  if (pop_all(parser)) {
   free_all_parsers(parser);
   return false;
  }
  fprintf(parser->out_stream, "\n");
  fflush(parser->out_stream);
  return true;
}

/* The FILE* parameter is provided for the unit test.
 * The function expects storage of size MAXTOKENLEN and a pointer to an open
 * stream.
 * The function returns the first newline-terminated line of the stream in stdinp,
 * unless that line is longer than MAXTOKENLEN, in which it is truncated.
*/
size_t process_stdin(char stdinp[], FILE *input_stream) {
  char raw_input[MAXTOKENLEN];
  if (fgets(raw_input, MAXTOKENLEN, input_stream) != NULL) {
    /* A line from stdin which will always end in '\n'. */
    char *newline_pos = strstr(raw_input, "\n");
    if (NULL == newline_pos) {
      // Input was truncated by fgets().
      fprintf(stderr, "Input from stdin must be less than %u characters long.\n",
	      MAXTOKENLEN-1);
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
	Return the lesser of the destination capacity and the requested copy length.
       */
      return ((MAXTOKENLEN -1) <(offset + 1)) ? (MAXTOKENLEN - 1) : (offset + 1);
    }
  } else {
    fprintf(stderr, "Malformed input.\n");
  }
  return 0;
}

/*
 * The stream parameter is for the unit tests. from_user[] is the prompt.  That
 * part of the prompt which the program should analyze further is returned in
 * the inputstr[] array.  The function returns the number of characters in input_str[].
 */

size_t find_input_string(const char from_user[], char inputstr[], FILE *stream) {
  /*
   * Without the length check, providing "-val;" as input triggers a hang, as
   * process_stdin() never receives any chars in its fgets() call and waits
   * forever.
   */
  if ((1 == strlen(from_user)) && (from_user[0] == '-')) {
    return process_stdin(inputstr, stream);
  } else{
    /* read input from CLI */
    const size_t requested = strlcpy(inputstr, from_user, MAXTOKENLEN-1);
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
