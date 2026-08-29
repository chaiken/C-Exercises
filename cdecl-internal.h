#ifndef CDECL_INTERNAL
#define CDECL_INTERNAL

#define MAXTOKENLEN 128
#define MAXTOKENS 256
#define MAXIDENTIFIERS 4
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#define _cleanup_(x) __attribute__((__cleanup__(x)))

#include <math.h>

const size_t BITS_PER_INT = (size_t)floor(log2(8.0 * sizeof(int)));

const wchar_t typechars[] = {'1', '2', '3', '4', '6', '8', 'a', 'b',
                             'c', 'd', 'e', 'f', 'g', 'h', 'i', 'l',
                             'n', 'o', 'r', 's', 't', 'u'};
const wchar_t *types[] = {L"char",
                          L"short",
                          L"long",
                          L"int",
                          L"float",
                          L"double",
                          L"long",
                          L"struct",
                          L"enum",
                          L"union",
                          L"void",
                          L"int8_t",
                          L"uint8_t",
                          L"int8_t",
                          L"uint16_t",
                          L"int16_t",
                          L"uint32_t",
                          L"int32_t",
                          L"uint64_t",
                          L"int64_t",
                          L"size_t",
                          L"ssize_t",
                          L"bool",
                          L"u8",
                          L"s8",
                          L"u16",
                          L"s16",
                          L"u32",
                          L"s32",
                          L"u64",
                          L"s64",
                          L"intptr_t",
                          L"uintptr_t",
                          L"ptrdiff_t"
                          L"wchar_t",
                          L"atomic_bool",
                          L"atomic_char",
                          L"atomic_schar",
                          L"atomic_uchar",
                          L"atomic_short",
                          L"atomic_ushort",
                          L"atomic_int",
                          L"atomic_uint",
                          L"atomic_long",
                          L"atomic_ulong",
                          L"atomic_llong",
                          L"atomic_ullong",
                          L"atomic_char8",
                          L"atomic_char16",
                          L"atomic_char32",
                          L"atomic_char",
                          L"atomic_intptr",
                          L"atomic_uintptr",
                          L"atomic_size",
                          L"atomic_ptrdiff",
                          L"atomic_intmax",
                          L"atomic_uintmax"};
const wchar_t *qualifiers[] = {L"const",    L"volatile", L"static",
                               L"*",        L"extern",   L"unsigned",
                               L"restrict", L"atomic",   L"inline"};
enum token_class { invalid = 0, type, qualifier, identifier, length, typedefn };
enum specifier_state { UNKNOWN, UNSPECIFIED, SPECIFIED };
const wchar_t *kind_names[] = {L"invalid",    L"type",   L"qualifier",
                               L"identifier", L"length", L"typedefn"};
struct token {
  enum token_class kind;
  wchar_t string[MAXTOKENLEN];
};

/*
 * Store the identifier info in a struct of arrays since an array of structs is
 * too horrible an antipattern even for a fun project.
 */
struct identifier_props {
  size_t array_dimensions[MAXIDENTIFIERS];
  size_t array_lengths[MAXIDENTIFIERS];
  enum specifier_state last_dimension[MAXIDENTIFIERS];
};

/*
 * A well-formed declaration must have exactly one of each of the following:
 * a type;
 * an identifier;
 * a terminating ';' or '='.
 */
struct parser_props {
  /* These latching bools keep track of which elements the parser has
   * encountered.  The output stage makes use of them.
   */
  bool have_type;
  bool have_qualifier;
  /* These bools describe the high-level identity of the parsed object. */
  bool is_function;
  bool is_enum;
  bool is_struct_or_union;
  bool is_pointer;
  bool is_function_ptr;
  bool is_typedef;
  bool is_bitfield;
  bool is_declarator_list;
  bool is_inline;
  /* Enumeration, function and struct objects contain subsidiary objects. */
  bool has_enum_constants;
  bool has_function_params;
  bool has_struct_or_union_members;
  wchar_t enumerator_list[MAXTOKENLEN];
  size_t num_identifiers;
  size_t bitfield_width;
  /* These parameters describe the internal parser state. */
  size_t cursor;
  size_t stacklen;
  wchar_t start_delim;
  wchar_t end_delim;
  wchar_t separator;
  struct token stack[MAXTOKENS];
  struct identifier_props ident;
  struct parser_props *prev;
  struct parser_props *next;
  struct parser_props *parent;
  /* The I/O streams are settable for the convenience of the tests. */
  FILE *out_stream;
  FILE *err_stream;
};

/* documentation functions */
void usage(void);
void limitations();

/* function to modify the parser */
void initialize_parser(struct parser_props *parser);
void reset_parser(struct parser_props *parser);
void release_parser_resources(struct parser_props *parser);
struct parser_props *make_parser(struct parser_props *const parser);

/*
 * Functions which characterize input.  A returned false value indicates an
 * error.  Functions with two parameters modify the non-const one. None of the
 * functions advances the parser cursor.
 */
bool is_all_blanks(const wchar_t *input);
bool has_alnum_chars(const wchar_t *input);
bool is_numeric(const wchar_t *input);
static bool is_type_char(const wchar_t c);
static bool has_any_name_chars(const wchar_t *s);
bool parens_match(const wchar_t *offset_decl, size_t *pair_count);
bool check_for_array_dimensions(struct parser_props *parser,
                                const wchar_t *offset_decl);
bool check_for_function_parameters(struct parser_props *parser,
                                   const wchar_t *offset_decl);
bool check_for_struct_or_union_members(struct parser_props *parser,
                                       const wchar_t *offset_decl);
bool check_for_enum_constants(struct parser_props *parser,
                              const wchar_t *offset_decl);
bool check_for_function_ptr(struct parser_props *parser,
                            const wchar_t *offset_decl);
void check_for_declarator_list(struct parser_props *parser,
                               const wchar_t *user_input);

/* functions which modify input */
size_t trim_leading_whitespace(const wchar_t *input, wchar_t *trimmed);
size_t trim_trailing_whitespace(const wchar_t *input, wchar_t *trimmed);
void elide_assignments(wchar_t **input);
bool tokenize_function_params(wchar_t **output, wchar_t *input,
                              const wchar_t delim);
bool tokenize_struct_params(wchar_t **output, wchar_t *input,
                            const wchar_t delim);
bool truncate_input(wchar_t **input, struct parser_props *parser);

/* debugging functions */
struct parser_props *get_head_parser(struct parser_props *parser);
void show_parser_list(const struct parser_props *parser, const int lineno);
void showstack(const struct token *stack, const size_t stacklen,
               FILE *out_stream, const int lineno);

/* parser helper functions */
bool have_stacked_compound_type(const struct parser_props *parser);
bool handled_compound_type(struct parser_props *parser, wchar_t *progress_ptr,
                           struct token *this_token);
bool all_identifiers_are_enum_constants(const struct parser_props *parser);
bool first_identifier_is_enumerator(const struct parser_props *parser,
                                    const wchar_t *user_input);
void handle_trailing_instance_name(struct parser_props *parser,
                                   wchar_t *user_input);
bool process_secondary_params(struct parser_props *parser, wchar_t *user_input);
size_t process_array_length(struct parser_props *parser,
                            const wchar_t *offset_string,
                            struct token *this_token);
bool process_array_dimensions(struct parser_props *parser, wchar_t *user_input,
                              struct token *this_token);
bool process_enum_constants(struct parser_props *parser, wchar_t *user_input);
bool handled_extended_parsing(struct parser_props *parser, wchar_t *user_input,
                              struct token *this_token);

/* output functions */
void reverse_lengths(struct parser_props *parser, const size_t top_ident,
                     const size_t current_stack_top);
void reorder_qualifier_and_type(struct parser_props *parser);
void reorder_array_identifier_and_lengths(struct parser_props *parser);
void reorder_stacks(struct parser_props *parser);
bool pop_stack(struct parser_props *parser, bool no_enum_instance,
               bool is_second_pointer_qualifier);
bool pop_all(struct parser_props *parser);

/* the core parser functions */
enum token_class get_kind(const wchar_t *intoken);
size_t gettoken(struct parser_props *parser, const wchar_t *declstring,
                struct token *this_token);
bool finish_token(struct parser_props *parser, const wchar_t *offset_decl,
                  struct token *this_token, const size_t ctr);
void push_stack(struct parser_props *parser, struct token *this_token);
size_t load_stack(struct parser_props *parser, wchar_t *user_input);

/* functions to process user input */
bool input_parsing_successful(struct parser_props *parser, wchar_t inputstr[]);
size_t process_stdin(wchar_t stdinp[], FILE *input_stream);

size_t find_input_string(const wchar_t from_user[], wchar_t inputstr[],
                         FILE *stream);

#endif
