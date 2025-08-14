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
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAXTOKENLEN 64
#define MAXTOKENS 256
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

enum token_class { delimiter = 1, type, qualifier, identifier };

struct token {
  enum token_class kind;
  char string[MAXTOKENLEN];
};

struct token stack[MAXTOKENS];
struct token this;

void usage(void) {
  printf("cdecl prints out the English language form of a C declaration.\n");
  printf("Invoke as 'cdecl <declaration>' or\n");
  printf("provide input on stdin and use '-' as the single command-line "
         "argument.\n");

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

enum token_class get_kind(const char *intoken) {

  char *delimiters[] = {"(", ")", "[", "]", "{", "}", ","};
  char *types[] = {"char", "short",  "unsigned", "int",   "float", "double",
                   "long", "struct", "enum",     "union", "void"};
  char *qualifiers[] = {"const",  "register", "volatile",
                        "static", "*",        "extern"};
  size_t numel = 0, ctr;

  assert(intoken);

  numel = ARRAY_SIZE(delimiters);
  for (ctr = 0; ctr < numel; ctr++) {
    if (!strcmp(intoken, delimiters[ctr]))
      return (delimiter);
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

  return (identifier);
}

void showstack(void) {

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
void process_function_args(char startstring[], size_t *arglength) {
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
    exit(-1);
  }

  printargs = *arglength;

  if (strstr(endstring, ". . .")) {
    printf("a variadic function ");
    /* 3 '.', 3 spaces, one comma */
    printargs -= 7;
  } else
    printf("a function ");

  if (printargs) {
    printf("that takes ");

    for (ctr = 0; ctr < printargs; ctr++) {
      if (endstring[ctr] == ',')
        printf(" and");
      else
        putchar(endstring[ctr]);
    }
    printf(" args and ");
  }

  printf("that returns ");
  free(saveptr);
  return;

  showstack();
  printf("\nMismatched function arg delimiters.\n");
  if (argstring)
    free(saveptr);
  exit(-1);
}

void process_array(char startstring[], size_t *sizelen) {
  char endstring[MAXTOKENLEN];
  size_t ctr = 0;
  *sizelen = 0;

  assert(startstring);

  strcpy(endstring, (const char *)startstring);
  printf("array of ");

  while ((*sizelen < strlen(startstring)) && (endstring[*sizelen] != ']'))
    (*sizelen)++;

  if (endstring[*sizelen] == ']') {
    /* print any array size values */
    if (*sizelen >= 1) {
      while (ctr++ < *sizelen)
        printf("%c", startstring[ctr - 1]);
    }
    printf(" ");
    return;
  }

  showstack();
  printf("\nMismatched array delimiters.\n");
  exit(-1);
}

void classify_string(struct token *newtoken) {

  assert(newtoken);

  /* default is identifier */
  newtoken->kind = get_kind(newtoken->string);

  switch (newtoken->kind) {
  case type:
    printf("token %s is a type.\n", newtoken->string);
    break;
  case qualifier:
    printf("token %s is a qualifier.\n", newtoken->string);
    break;
  case identifier:
    printf("token %s is an identifier.\n", newtoken->string);
    break;
  default:
    fprintf(stderr, "Illegal token kind.\n");
    exit(-1);
  }

  return;
}

/* move to the right through the declaration */
size_t gettoken(char **declstring) {

  int tokenlen, ctr = 0, tokenoffset = 0;
  memset(this.string, '\0', MAXTOKENLEN);

  if ((tokenlen = strlen(*declstring)) > MAXTOKENLEN) {
    fprintf(stderr, "\nToken too long %s.\n", *declstring);
    exit(-EOVERFLOW);
  }
  if (!tokenlen)
    return (0);

  while (isblank(**declstring)) {
    (*declstring)++;
    /* include whitespace in tokenoffset */
    tokenoffset++;
  }

  /* use first non-blank character whether its alphanumeric or no */
  this.string[0] = **declstring;
  (*declstring)++;
  tokenoffset++;

  /* non-alphanumeric token has is always a single-char */
  if (!(isalnum(this.string[0])))
    goto out;

  for (ctr = 0; (isalnum(**declstring) && (ctr <= tokenlen)); ctr++) {
    this.string[ctr + 1] = **declstring;
    (*declstring)++;
    tokenoffset++;
  }

out:
  this.string[ctr + 1] = '\0';
  this.kind = get_kind(this.string);
  return (tokenoffset);
}

void push_stack(size_t tokennum) {

  if (tokennum >= MAXTOKENS) {
    printf("\nStack overflow.\n");
    exit(-ENOMEM);
  }

  stack[tokennum].kind = this.kind;
  strcpy(stack[tokennum].string, this.string);
  return;
}

/* move back to the left */
void pop_stack(size_t *tokennum) {

  if (!(*tokennum)) {
    printf("\nAttempt to pop empty stack.\n");
    exit(-ENODATA);
  }

  /* Qualifiers following * apply to the pointer itself,
     rather than to the object to which the pointer points.
  */
  if (!strcmp(stack[*tokennum].string, "*"))
    printf("pointer(s) to ");
  /* print all qualifiers but pointer */
  else
    switch (stack[*tokennum].kind) {
    case qualifier:
      /* next two lines are deleted on laptop */
      printf("%s ", stack[*tokennum].string);
      break;
    case type:
      printf("%s ", stack[*tokennum].string);
      break;
    case delimiter:
      /* Ignore this token, which should be the
         opening parenthesis of a function pointer.
         There should be no array delimiters on the stack */
      (*tokennum)--;
      pop_stack(tokennum);
      break;
    default:
      fprintf(stderr, "\nError: element %s is of unknown type %d.\n",
              this.string, this.kind);
      exit(-1);
    }

  memset(stack[*tokennum].string, '\0', MAXTOKENLEN);
  stack[*tokennum].kind = 0;
  if (*tokennum)
    (*tokennum)--;
  return;
}

size_t process_stdin(char *stdinp) {
  char *stringp = (char *)malloc(MAXTOKENLEN);
  char *savep = stringp;

  size_t ctr = 0;
  if (fgets(stdinp, MAXTOKENLEN, stdin) != NULL) {
    strcpy(stringp, stdinp);
    /* line from stdin ends in '\n' */
    /* while ((*stringp++ = getchar())!= '\n') */
    while (*stringp++ != '\n')
      ctr++;
    /* make last character a null instead of '\n' */
    *--stringp = '\0';
  } else {
    fprintf(stderr, "No input.\n");
    if (stringp)
      free(stringp);
    exit(-1);
  }

  free(savep);
  return (ctr);
}

void parse_declarator(char input[], size_t *slen) {

  /* strstr() shouldn't return NULL since we've already determined
  that this.string is present */
  char *declp = strstr((const char *)input, (const char *)this.string);
  char *commapos;
  size_t offset;
  static int declnum = 0;
  size_t *argoffset = (size_t *)malloc(sizeof(size_t));

#ifdef DEBUG
  printf("\n\tDeclarator number %d\n", declnum);
#endif
  /* advance past the end of the identifier the first time we're called */
  if (!declnum)
    declp += strlen(this.string);
  declnum++;
  offset = (declp - input);

  /* process arguments on the right of identifier */
  while ((strlen(declp)) && (offset > 0)) {
    offset += gettoken(&declp);
    switch (this.kind) {
    case delimiter:
      if (!(strcmp(this.string, "["))) {
        process_array(&input[offset], argoffset);
        offset += *argoffset;
        /* go past array size specification */
        while ((*argoffset)--)
          declp++;
        *argoffset = 0;
        break;
      }
      if (!(strcmp(this.string, "]")))
        break;
      /* this opening parenthesis is to the right of the
      identifier, so it can only enclose functions args */
      if (!(strcmp(this.string, "("))) {
        process_function_args(&input[offset], argoffset);
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
      if (!(strcmp(this.string, ")"))) {
        pop_stack(slen);
        break;
      }

      if (!strcmp(this.string, ",")) {
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
              this.string);
      break;
    case qualifier:
      push_stack(++(*slen));
      break;
    case identifier:
      fprintf(stderr, "\nSecond identifier %s is illegal.\n", this.string);
      if (argoffset)
        free(argoffset);
      exit(-1);
    }
  }
  memset(this.string, '\0', MAXTOKENLEN);
  this.kind = 0;

  /* (!(*slen)) means empty stack */
  while (*slen) {
    if ((strcmp(stack[*slen].string, ")")) ||
        (strcmp(stack[*slen].string, "*")) || (stack[*slen].kind == qualifier))
      pop_stack(slen);
    if (slen)
      parse_declarator(input, slen);
  }

  free(argoffset);
  return;
}

int main(int argc, char **argv) {

  /* char inputstr[] = "char* extern *(*bar), int foo; "; */
  char inputstr[MAXTOKENLEN];
  char *nexttoken = (char *)malloc(MAXTOKENLEN);
  /* preserve original pointer so that it can be freed */
  char *saveptr = nexttoken;
  /* stack starts at 1 so that (!stacklen) means an empty stack,
  but initialize to zero because counter is incremented before
  calling push_stack */
  size_t stacklen = 0;
  int retval = 0;

  if (argc == 1)
    usage();

  if (argv[1][0] == '-') {
    /* extract a single declaration from possible multiple
       declarations and initialization of the input string */
    if (!(retval = process_stdin(inputstr))) {
      printf("Bad input from stdin\n");
      usage();
      exit(-1);
    }
  } else /* read input from CLI */
    strcpy(inputstr, argv[1]);

  if (strstr(inputstr, "=")) {
    nexttoken = strtok(inputstr, "=");
    strcpy(inputstr, nexttoken); /* dump chars after '=' */
  } else if (strstr(inputstr, ";")) {
    nexttoken = strtok(inputstr, ";");
    if (!nexttoken) {
      fprintf(stderr, "Zero-length input string.\n");
      exit(-1);
    }
  } else {
    printf("\nImproperly terminated declaration.\n");
    exit(-1);
  }

  while ((gettoken(&nexttoken)) && (get_kind(this.string) != identifier))
    push_stack((++stacklen)); /* moving to the right */

  if (this.kind == identifier)
    printf("%s is a(n) ", this.string);
  else {
    printf("\nNo identifiers in input string '%s'\n", inputstr);
    exit(-1);
  }

  /* if there's stuff on the stack or to the right of the identifier */
  if ((stacklen) || (nexttoken < (strlen(inputstr) + &(inputstr[0]))))
    parse_declarator(inputstr, &stacklen);

  /*	showstack(); */
  printf("\n");
  free(saveptr);
  exit(0);
}
