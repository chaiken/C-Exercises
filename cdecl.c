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

/* Caller must allocate trimmed. */
size_t trim_trailing_whitespace(const char* input, char* trimmed) {
  if (!input || (0 == strlen(input)) || (is_all_blanks(input))) {
    return 0;
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

void showstack(const struct token* stack) {

  size_t tokennum = 1, ctr;

  printf("\nStack is:\n");
  /* stack starts at 1 so that (!slen) means empty*/
  for (ctr = 1; (stack[ctr].kind > 0); ctr++) {
    printf("Token number %lu has kind %d and string %s\n", tokennum,
           stack[ctr].kind, stack[ctr].string);
    tokennum++;
  }

  return;
}

/* fails for multiple arguments */
bool processed_function_args(char startstring[], size_t *arglength, FILE* output_stream) {
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
    fprintf(stderr, "Malformed function arguments %s\n", argstring);
    return false;
  }

  printargs = *arglength;

  if (strstr(endstring, ". . .")) {
    fprintf(output_stream, "a variadic function ");
    /* 3 '.', 3 spaces, one comma */
    printargs -= 7;
  } else
    fprintf(output_stream, "a function ");

  if (printargs) {
    fprintf(output_stream, "that takes ");

    for (ctr = 0; ctr < printargs; ctr++) {
      if (endstring[ctr] == ',')
        fprintf(output_stream, " and");
      else
        putchar(endstring[ctr]);
    }
    fprintf(output_stream, " args and ");
  }

  fprintf(output_stream, "that returns ");
  free(saveptr);

  return true;
}

bool processed_array(char startstring[], size_t *sizelen, const struct token* stack, FILE* output_stream) {
  char endstring[MAXTOKENLEN];
  size_t ctr = 0;
  *sizelen = 0;

  assert(startstring);

  strcpy(endstring, (const char *)startstring);
  fprintf(output_stream, "array of ");

  while ((*sizelen < strlen(startstring)) && (endstring[*sizelen] != ']'))
    (*sizelen)++;

  if (endstring[*sizelen] == ']') {
    /* print any array size values */
    if (*sizelen >= 1) {
      while (ctr++ < *sizelen)
        fprintf(output_stream, "%c", startstring[ctr - 1]);
    }
    fprintf(output_stream, " ");
    return true;
  }

  showstack(stack);
  fprintf(stderr, "\nMismatched array delimiters.\n");
  return false;
}

/* Moves to the right through the declaration, returning space- or
 * delimiter-separated token at a time.
 * The parameter this_token returns the next token in the string.
 * The return value is the offset where parsing should resume in the next pass.
 */
size_t gettoken(char *declstring, struct token *this_token) {

  const size_t tokenlen = strlen(declstring);
  size_t ctr = 0, tokenoffset = 0;
  char trimmed[MAXTOKENLEN];
  memset(this_token->string, '\0', MAXTOKENLEN);

  if (!tokenlen) {
    return 0;
  }
  if (tokenlen > (MAXTOKENLEN-1)) {
    fprintf(stderr, "\nToken too long %s.\n", declstring);
    return 0;
  }

  /* Move past leading whitespace. */
  const size_t trimnum = trim_leading_whitespace(declstring, trimmed);
  declstring += trimnum;
  tokenoffset += trimnum;

  /* use first non-blank character whether it is alphanumeric or no */
  this_token->string[0] = *declstring;
  declstring++;
  tokenoffset++;

  /* Non-alphanumeric token has is always a single char. */
  /* Here we implicitly disallow dash and underscore in identifiers. */
  if (!(isalnum(this_token->string[0])))
    goto done;

  for (ctr = 0; (isalnum(*declstring) && (ctr <= tokenlen)); ctr++) {
    this_token->string[ctr + 1] = *(declstring++);
    tokenoffset++;
  }

done:
  this_token->string[ctr + 1] = '\0';
  this_token->kind = get_kind(this_token->string);
  return (tokenoffset);
}

void push_stack(size_t tokennum, struct token* this_token, struct token* stack) {

  if (tokennum >= MAXTOKENS) {
    fprintf(stderr, "\nStack overflow.\n");
    exit(-ENOMEM);
  }

  stack[tokennum].kind = this_token->kind;
  strcpy(stack[tokennum].string, this_token->string);
  return;
}

/* move back to the left */
/* Return 0 on success, an error code on failure */
int pop_stack(size_t *tokennum, struct token* this_token, struct token* stack, FILE* output_stream) {

  if (!(*tokennum)) {
    fprintf(stderr, "\nAttempt to pop empty stack.\n");
    return -ENODATA;
  }

  /* Qualifiers following * apply to the pointer itself,
     rather than to the object to which the pointer points.
  */
  if (!strcmp(stack[*tokennum].string, "*"))
    fprintf(output_stream, "pointer(s) to ");
  /* print all qualifiers but pointer */
  else
    switch (stack[*tokennum].kind) {
    case whitespace:
      __attribute__((fallthrough));
    case qualifier:
      __attribute__((fallthrough));
    case type:
      fprintf(output_stream, "%s ", stack[*tokennum].string);
      break;
    case delimiter:
      /* Ignore this_token token, which should be the
         opening parenthesis of a function pointer.
         There should be no array delimiters on the stack */
      (*tokennum)--;
      pop_stack(tokennum, this_token, stack, output_stream);
      break;
    case invalid:
      __attribute__((fallthrough));
    default:
      fprintf(stderr, "\nError: element %s is of unknown type %d.\n",
              this_token->string, this_token->kind);
      return -EINVAL;
    }

  memset(stack[*tokennum].string, '\0', MAXTOKENLEN);
  stack[*tokennum].kind = invalid;
  if (*tokennum)
    (*tokennum)--;
  return 0;
}

/* Return 0 on on success, an error code on failure. */
int parse_declarator(char input[], size_t *slen, struct token* this_token, struct token* stack, FILE* output_stream) {
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
    offset += gettoken(declp, this_token);
    declp += offset;
    switch (this_token->kind) {
    case invalid:
      fprintf(stderr, "Invalid input.\n");
      return -EINVAL;
    case delimiter:
      if (!(strcmp(this_token->string, "["))) {
        if (!processed_array(&input[offset], argoffset, stack, output_stream)) {
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
        if (!processed_function_args(&input[offset], argoffset, output_stream)) {
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
        int err = pop_stack(slen, this_token, stack, output_stream);
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
      push_stack(++(*slen), this_token, stack);
      break;
    case whitespace:
      break;
    case identifier:
      fprintf(stderr, "\nSecond identifier %s is illegal.\n", this_token->string);
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

  /* (!(*slen)) means empty stack */
  while (*slen) {
    if ((strcmp(stack[*slen].string, ")")) ||
        (strcmp(stack[*slen].string, "*")) || (stack[*slen].kind == qualifier))
      pop_stack(slen, this_token, stack, output_stream);
    if (slen)
      parse_declarator(input, slen, this_token, stack, output_stream);
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
 * Returns true iff input is successfully parsed.  Actual parsing begins here.
 */
bool input_parsing_successful(char inputstr[], struct token* this_token, FILE *output_stream) {
  char *nexttoken = (char *)malloc(MAXTOKENLEN);
  /*
   * ptr_offset is the difference between the current value of nexttoken and the
   * allocated value on the heap.
   */
  size_t ptr_offset = 0;
  /* stack starts at 1 so that (!stacklen) means an empty stack,
  but initialize to zero because counter is incremented before
  calling push_stack */
  size_t stacklen = 0;
  char *input_end = NULL;

  strlcpy(nexttoken, inputstr, MAXTOKENLEN);
  /* Dump chars after '=', if any. */
  input_end = strstr(nexttoken, "=");
  /* If the input after '='is not lopped off, the input should terminate with ';'. */
  if (!input_end) {
    input_end = strstr(nexttoken, ";");
   /* Input with two semicolons could reach this point. */
    if (input_end == nexttoken) {
      fprintf(stderr, "Zero-length input string.\n");
      free(nexttoken);
      return false;
    }
  }
 if (!input_end) {
    fprintf(stderr, "\nImproperly terminated declaration.\n");
    free(nexttoken);
    return false;
  }
  strlcpy(inputstr, nexttoken, (input_end - nexttoken) + 1);
  strlcpy(nexttoken, inputstr, MAXTOKENLEN);

  struct token stack[MAXTOKENS];
  while ((ptr_offset = gettoken(nexttoken + ptr_offset, this_token) > 0) && (this_token->kind != identifier))
    push_stack(++stacklen, this_token, &stack[0]); /* moving to the right */
  if (0 == strlen(this_token->string)) {
    fprintf(stderr, "Unable to read garbled input.\n");
    free(nexttoken - ptr_offset);
    return false;
  }

  if (this_token->kind == identifier) {
    fprintf(output_stream, "%s is a(n) ", this_token->string);
  } else {
    fprintf(stderr, "\nNo identifiers in input string '%s'\n", inputstr);
    free(nexttoken - ptr_offset);
    return false;
  }

  /* if there's stuff on the stack or to the right of the identifier */
  if ((stacklen) || (nexttoken < (strlen(inputstr) + &(inputstr[0])))) {
    if(parse_declarator(inputstr, &stacklen, this_token, &stack[0], output_stream)) {
      return false;
    }
  }

  free(nexttoken - ptr_offset);
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
  struct token this_token;
  this_token.kind = invalid;
  strcpy(this_token.string, "");
  if (!input_parsing_successful(inputstr, &this_token, stdout)) {
    exit(EXIT_FAILURE);
  }
  /*	showstack(); */
  printf("\n");
  exit(EXIT_SUCCESS);
}
#endif
