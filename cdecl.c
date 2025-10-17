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


const char delimiters[] = {'(', ')', '[', ']', '{', '}', ','};
const char *types[] = {"char", "short", "int", "float", "double",
		       "long", "struct", "enum", "union", "void", "int8_t", "uint8_t",
		       "int8_t", "uint16_t", "int16_t", "uint32_t", "int32_t", "uint64_t",
		       "int64_t"};
const char *qualifiers[] = {"const", "volatile", "static", "*", "extern", "unsigned"};

enum token_class { invalid = 0, delimiter, type, qualifier, identifier, whitespace };

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
  bool have_delimiter;
  size_t stacklen;
  struct token stack[MAXTOKENS];
};

void initialize_parser(struct parser_props* parser) {
  parser->have_identifier = false;
  parser->have_type = false;
  parser->have_delimiter = false;
  parser->stacklen = 0;
}

void usage(void) {
  printf("\ncdecl prints out the English language form of a C declaration.\n");
  printf("Invoke as 'cdecl <declaration>' or\n");
  printf("provide input on stdin and use '-' as the single command-line "
         "argument.\n");
  printf("Input must be terminated with a semicolon and enclosed in quotation marks.\n");
}

void limitations() {
  printf("Input must be shorter than %u characters, not including quotation marks and semicolon\n", MAXTOKENLEN);
  printf("Known deficiencies:\n\ta) doesn't handle multi-line struct and union "
         "declarations;\n");
  printf("\tb) doesn't handle multiple comma-separated identifiers;\n");
  printf("\tc) handles multi-dimensional arrays awkwardly;\n");
  printf("\td) includes only the most basic checks for declaration errors;\n");
  printf("\te) includes only the qualifiers defined in ANSI C, not LIBC\n");
  printf("or kernel extensions;\n");
  printf("\tf) doesn't process identifiers in function argument lists.\n ");
  exit(-1);
}

bool is_all_blanks(const char* input) {
  if (!input || !strlen(input)) {
    return false;
  }
  char* token_copy = strdup(input);
  char *saveptr = token_copy;
  while (token_copy && isprint(*token_copy) && isblank(*token_copy)) {
      token_copy++;
  }
  // Reached end of the string.
  if (!strlen(token_copy)) {
    free(saveptr);
    return true;
  }
  free(saveptr);
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
  char* copy = strdup(input);
  char* last_char = copy + (strlen(copy) - 1);
  // last_char should be greater than input when the loop exits, as otherwise input is all blanks.
  size_t removed = 0;
  while ((last_char > copy) && (isblank(*last_char))) {
    last_char--;
    removed++;
  }
  if ((copy + (strlen(copy) - 1)) == last_char) {
    free(copy);
    return 0;
  }
  /* Copy the non-blank part of the input to the output.*/
  for (char* cptr = copy; cptr <= last_char; cptr++ ) {
    *trimmed = *cptr;
    trimmed++;
  }
  *trimmed = '\0';
  free(copy);
  return removed;
}

/* Caller must allocate trimmed. */
size_t trim_leading_whitespace(const char* input, char* trimmed) {
  if (!input || (0 == strlen(input)) || (is_all_blanks(input))) {
    return 0;
  }
  if (!isblank(*input)) {
    return 0;
  }
  char* copy = strdup(input);
  char* saveptr = copy;
  size_t removed = 0;
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
  free(saveptr);
  return removed;
}

bool has_alnum_chars(const char* input) {
  if (!input || !strlen(input)) {
    return false;
  }
  char *copy = strdup(input);
  char *saveptr = copy;
  while (*copy && (!isalnum(*copy))) {
    copy++;
  }
  /* Reached the end without finding alphanumeric characters. */
  if (!*copy) {
    free(saveptr);
    return false;
  }
  free(saveptr);
  return true;
}

/*
 * Remove any characters following ';' or '=' plus any whitespace which precedes
 * these characters.  Returns true if the input contains one of those chars and
 * has any non-whitespace characters before them.
 */
bool truncate_input(char** input, FILE* err_stream) {
  char trimmed[MAXTOKENLEN];
  char *input_end = NULL;
  /* Dump chars after '=', if any. */
  input_end = strstr(*input, "=");
  /* If the input after '='is not lopped off, the input should terminate with ';'. */
  if (!input_end) {
    input_end = strstr(*input, ";");
   /* Input with two semicolons could reach this point. */
    if (input_end == *input) {
      fprintf(err_stream, "Zero-length input string.\n");
      return false;
    } else if (!input_end) { /* there is neither an '=' or a ';' */
      fprintf(err_stream, "\nImproperly terminated declaration.\n");
      return false;
    }
  }
  *input_end = '\0';
  if (trim_trailing_whitespace(*input, trimmed)) {
    strlcpy(*input, trimmed, MAXTOKENLEN);
  }
  if (!strlen(*input)) {
    fprintf(err_stream, "Zero-length input string.\n");
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

  numel = ARRAY_SIZE(delimiters);
  for (ctr = 0; ctr < numel; ctr++) {
    if ((1 == strlen(intoken)) && (*intoken == delimiters[ctr])) {
      return (delimiter);
    }
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

  /*
   * A string without alphanumeric chars must be whitespace, a delimiter, or
   * garbage.
   */
  if (!has_alnum_chars(intoken)) {
    return invalid;
  }

  return identifier;
}

void showstack(const struct token* stack, FILE* out_stream) {

  size_t tokennum = 0, ctr;

  fprintf(out_stream, "Stack is:\n");
  for (ctr = 0; (stack[ctr].kind > 0); ctr++) {
    fprintf(out_stream, "Token number %lu has kind %d and string %s\n", tokennum,
           stack[ctr].kind, stack[ctr].string);
    tokennum++;
  }

  return;
}

/* fails for multiple arguments */
bool processed_function_args(char startstring[], size_t *arglength, FILE* out_stream, FILE* err_stream) {
  char endstring[MAXTOKENLEN];
  char *argstring = (char *)malloc(strlen(startstring));
  char *saveptr = argstring;
  size_t ctr, printargs = 0;

  assert(startstring);

  *arglength = 0;
  strcpy(endstring, startstring);

  while ((*arglength < strlen(startstring)) && (endstring[*arglength] != ')'))
    (*arglength)++;

  if (endstring[*arglength] != ')') {
    fprintf(err_stream, "Malformed function arguments %s\n", argstring);
    return false;
  }

  printargs = *arglength;

  if (strstr(endstring, ". . .")) {
    fprintf(out_stream, "a variadic function ");
    /* 3 '.', 3 spaces, one comma */
    printargs -= 7;
  } else
    fprintf(out_stream, "a function ");

  if (printargs) {
    fprintf(out_stream, "that takes ");

    for (ctr = 0; ctr < printargs; ctr++) {
      if (endstring[ctr] == ',')
        fprintf(out_stream, " and");
      else
        putchar(endstring[ctr]);
    }
    fprintf(out_stream, " args and ");
  }

  fprintf(out_stream, "that returns ");
  free(saveptr);

  return true;
}

bool processed_array(char startstring[], size_t *sizelen, const struct
		     parser_props *parser, FILE* out_stream, FILE* err_stream) {
  char endstring[MAXTOKENLEN];
  size_t ctr = 0;
  *sizelen = 0;

  assert(startstring);

  strcpy(endstring, (const char *)startstring);
  fprintf(out_stream, "array of ");

  while ((*sizelen < strlen(startstring)) && (endstring[*sizelen] != ']'))
    (*sizelen)++;

  if (endstring[*sizelen] == ']') {
    /* print any array size values */
    if (*sizelen >= 1) {
      while (ctr++ < *sizelen)
        fprintf(out_stream, "%c", startstring[ctr - 1]);
    }
    fprintf(out_stream, " ");
    return true;
  }

  showstack(parser->stack, out_stream);
  fprintf(err_stream, "\nMismatched array delimiters.\n");
  return false;
}

/* Moves to the right through the declaration, returning one space- or
 * delimiter-separated token at a time.
 * The parameter this_token returns the next token in the string.
 * The return value is the offset where parsing should resume in the next pass.
 */
size_t gettoken(struct parser_props* parser, const char *declstring,
		struct token *this_token) {

  const size_t tokenlen = strlen(declstring);
  size_t ctr = 0, tokenoffset = 0;
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

  /* use first non-blank character whether it is alphanumeric or no */
  this_token->string[0] = *(declstring + tokenoffset);
  tokenoffset++;

  /* Non-alphanumeric token has is always a single char. */
  /* Here we implicitly disallow dash and underscore in identifiers. */
  if (!(isalnum(this_token->string[0])))
    goto done;

  for (ctr = 0; (isalnum(*(declstring + tokenoffset)) && (ctr <= tokenlen));
       ctr++) {
    this_token->string[ctr + 1] = *(declstring + tokenoffset);
    tokenoffset++;
  }

done:
  this_token->string[ctr + 1] = '\0';
  this_token->kind = get_kind(this_token->string);
  switch(this_token->kind) {
  case identifier:
    parser->have_identifier = true;
    break;
  case type:
    parser->have_type = true;
    break;
  case delimiter:
    parser->have_delimiter = true;
    break;
  case qualifier:
      __attribute__((fallthrough));
  case invalid:
      __attribute__((fallthrough));
  default:
    break;
  }
  return (tokenoffset);
}

/*
 * Adds an element created from this_token to parser->stack and increments
 * stacklen.
 */
void push_stack(struct parser_props* parser, struct token* this_token, FILE* err_stream) {
  if (parser->stacklen >= MAXTOKENS) {
    fprintf(err_stream, "\nStack overflow.\n");
    exit(-ENOMEM);
  }

  parser->stack[parser->stacklen].kind = this_token->kind;
  strcpy(parser->stack[parser->stacklen].string, this_token->string);
  parser->stacklen++;
  return;
}

/* move back to the left */
/* Return 0 on success, an error code on failure */
int pop_stack(struct parser_props* parser, FILE* out_stream, FILE* err_stream) {
  /* Last element of stack with stacklen=n is at index = n-1. */
  const size_t stacktop = parser->stacklen - 1;
  if (!parser->stacklen) {
    fprintf(err_stream, "Attempt to pop empty stack.\n");
    return -ENODATA;
  }
  /*
   * Qualifiers following * apply to the pointer itself, rather than to the
   * object to which the pointer points.
   */
  if (!strcmp(parser->stack[stacktop].string, "*")) {
    fprintf(out_stream, "pointer(s) to ");
  } else {
    switch (parser->stack[stacktop].kind) {
    case whitespace:
      __attribute__((fallthrough));
    case qualifier:
      __attribute__((fallthrough));
    case type:
      fprintf(out_stream, "%s ", parser->stack[stacktop].string);
      break;
    /* TODO: process functions args and arrays here. */
    case delimiter:
      /* Ignore this_token token, which should be the
         opening parenthesis of a function pointer.
         There should be no array delimiters on the stack */
      parser->stacklen--;
      pop_stack(parser, out_stream, err_stream);
      break;
    case identifier:
      fprintf(out_stream, "%s is a(n) ", parser->stack[stacktop].string);
      break;
    case invalid:
      __attribute__((fallthrough));
    default:
      fprintf(err_stream, "\nError: element %s is of unknown type %d.\n",
              parser->stack[stacktop].string, parser->stack[stacktop].kind);
      return -EINVAL;
    }
  }
  memset(parser->stack[stacktop].string, '\0', MAXTOKENLEN);
  parser->stack[stacktop].kind = invalid;
  return 0;
}

/* Return 0 on on success, an error code on failure. */
int parse_declarator(char input[], struct parser_props* parser,
		     struct token* this_token, FILE* out_stream, FILE* err_stream) {
  const char *strp = strstr((const char *)input, (const char *)this_token->string);
  if (!strp) {
      fprintf(stderr, "Bad token: %s\n", this_token->string);
      return -EINVAL;
  }

  char *declp = strdup(strp);
  if (!declp) {
    fprintf(stderr, "OOM\n");
    exit(-ENOMEM);
  }
  char* saveptr = declp;
  char *commapos;
  size_t offset = 0;
  static int declnum = 0;
  size_t *argoffset = (size_t *)malloc(sizeof(size_t));
  *argoffset = 0;

#ifdef DEBUG
  printf("\n\tDeclarator number %d\n", declnum);
#endif
  /* advance past the end of the identifier the first time we're called */
  if (!declnum)
    declp += strlen(this_token->string);
  declnum++;
  offset = strlen(this_token->string);

  /* process arguments on the right of identifier */
  while ((strlen(declp)) && (offset > 0)) {
    offset += gettoken(parser, declp, this_token);
    declp += offset;
    switch (this_token->kind) {
    case invalid:
      fprintf(stderr, "Invalid input.\n");
      return -EINVAL;
    case delimiter:
      if (!(strcmp(this_token->string, "["))) {
        if (!processed_array(&input[offset], argoffset, parser, out_stream, err_stream)) {
	  return -EINVAL;
	}
        offset += *argoffset;
        /* go past array size specification */
        while ((*argoffset)--)
          declp++;
        *argoffset = 0;
        break;
      }
      if (!(strcmp(this_token->string, "]")))
        break;
      /* this_token opening parenthesis is to the right of the
      identifier, so it can only enclose functions args */
      if (!(strcmp(this_token->string, "("))) {
        if (!processed_function_args(&input[offset], argoffset, out_stream, err_stream)) {
	  return -EINVAL;
	}
        offset += *argoffset;
        /* go past any function args */
        while ((*argoffset)--)
          declp++;
        *argoffset = 0;
        /* go past closing ')' */
        declp++;
        offset++;
        break;
      }
      /* here process function pointers on the stack,
         as the closing ')' of function args was
         discarded above
      */
      if (!(strcmp(this_token->string, ")"))) {
        int err = pop_stack(parser, out_stream, err_stream);
	if (err) {
	  return err;
	}
        break;
      }

      if (!strcmp(this_token->string, ",")) {
        /* Function args come after parentheses
        and shouldn't be processed here */
        commapos = strrchr(input, ',');
        /* terminate string at last comma */
        if (commapos)
          *declp = '\0';
        fprintf(stderr, "WARNING: only the first of a set of comma-delimited "
                        "declarations is processed.\n");
        break;
      }
      break;
    case type: /* no types to the right of identifier */
      fprintf(stderr,
              "\nType declaration to the right of the identifier in %s is "
              "illegal.\n",
              this_token->string);
      break;
    case qualifier:
      break;
    case whitespace:
      break;
    case identifier:
      showstack(parser->stack, err_stream);
      fprintf(stderr, "Second identifier %s is illegal.\n", this_token->string);
      fflush(stderr);
      /* Prevent chars speculatively written to stdout from printing. */
      __fpurge(stdout);
      if (argoffset) {
        free(argoffset);
      }
      return -EINVAL;
    }
  }
  memset(this_token->string, '\0', MAXTOKENLEN);
  this_token->kind = invalid;

  while (parser->stacklen) {
    size_t stacktop = parser->stacklen - 1;
    if ((strcmp(parser->stack[stacktop].string, ")")) ||
        (strcmp(parser->stack[stacktop].string, "*")) || (parser->stack[stacktop].kind == qualifier)) {
      pop_stack(parser, out_stream, err_stream);
    }
    if (parser->stacklen) {
      parse_declarator(input, parser, this_token, out_stream, err_stream);
    }
  }

  if (saveptr) {
    free(saveptr);
  }
  if (argoffset) {
    free(argoffset);
  }
  return 0;
}

/*
 * Qualifiers modify the type, not the identifier. Therefore reorder qualifiers
 * and types on the stack in order to produce corector output.
 * Since * is a qualifier, this function does not break
 * "char * const typec_pwr_opmodes;"
 */
void reorder_qualifier_and_type(struct parser_props* parser) {
  if (parser->have_type) {
    size_t stackleft = parser->stacklen;
    while (stackleft > 1) {
      if ((type == parser->stack[stackleft-1].kind) &&
	  (qualifier == parser->stack[stackleft-2].kind) &&
	  (0 != strcmp("*", parser->stack[stackleft-2].string))) {
        /* Save top element's string. */
        char type_name[MAXTOKENLEN];
	strlcpy(type_name, parser->stack[stackleft-1].string, MAXTOKENLEN);
        /* Overwrite top element (type) with the qualifier 2nd from top element (qualifier). */
	parser->stack[stackleft-1].kind = qualifier;
	strlcpy(parser->stack[stackleft-1].string, parser->stack[stackleft-2].string, MAXTOKENLEN);
        /* Complete the swap. */
	parser->stack[stackleft-2].kind = type;
	strlcpy(parser->stack[stackleft-2].string, type_name, MAXTOKENLEN);
      }
      stackleft -= 1;
    }
  }
}

size_t load_stack(struct parser_props* parser, char* nexttoken,
		  [[maybe_unused]] FILE* out_stream, FILE* err_stream) {
  struct token this_token;
  char trimmed[MAXTOKENLEN];
  this_token.kind = invalid;
   /*
    * offset is the number of characters consumed by gettoken().
    * offset >= this_token->string since leading whitespace in nexttoken will be
    * skipped.
    */
  size_t offset = 0;
  if (!truncate_input(&nexttoken, err_stream)) {
    return 0;
  }
  while ((this_token.kind != identifier) &&
        (offset < strlen(nexttoken))) {
    char* input_cursor  = nexttoken + offset;
    offset += gettoken(parser, input_cursor, &this_token);
    size_t trailing = trim_trailing_whitespace(nexttoken, trimmed);
    if (trailing) {
      strlcpy(nexttoken, trimmed, MAXTOKENLEN);
      offset += trailing;
    }
    push_stack(parser, &this_token, err_stream);
  }
  reorder_qualifier_and_type(parser);
#ifdef TESTING
  showstack(parser->stack, out_stream);
#endif
  return offset;
}


/*
 * Returns true iff input is successfully parsed.  Actual parsing begins here.
 */
bool input_parsing_successful(char inputstr[], FILE *out_stream,
			      FILE* err_stream) {
  /* Allocate and call strlcpy() in case the input is too long. */
  char *nexttoken = (char *)malloc(MAXTOKENLEN);
  struct parser_props parser;

  initialize_parser(&parser);
  strlcpy(nexttoken, inputstr, MAXTOKENLEN);
  load_stack(&parser, nexttoken, out_stream, err_stream);
  if (0 == parser.stacklen) {
    fprintf(err_stream, "Unable to read garbled input.\n");
    free(nexttoken);
    return false;
  }
  if (!(parser.have_identifier && parser.have_type)) {
    fprintf(err_stream, "Input lacks required identifier or type element.\n");
    free(nexttoken);
    return false;
  }
#ifdef TESTING
  showstack(parser.stack, out_stream);
#endif
  while (parser.stacklen && (!pop_stack(&parser, out_stream, err_stream))) {
    parser.stacklen--;
  }
  fprintf(out_stream, "\n");
  fflush(out_stream);
  free(nexttoken);
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
  if (from_user[0] == '-') {
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

  if ((argc != 2)) {
    usage();
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
    exit(EINVAL);
  }
  if (!input_parsing_successful(inputstr, stdout, stderr)) {
    exit(EXIT_FAILURE);
  }
  printf("\n");
  exit(EXIT_SUCCESS);
}
#endif
