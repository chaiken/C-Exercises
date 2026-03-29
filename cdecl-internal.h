#ifndef CDECL_INTERNAL
#define CDECL_INTERNAL

#define MAXTOKENLEN 64
#define MAXTOKENS 256
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#define _cleanup_(x) __attribute__((__cleanup__(x)))

const char typechars[] = {'1', '2', '3', '4', '6', '8', 'a', 'b',
                          'c', 'd', 'e', 'f', 'g', 'h', 'i', 'l',
                          'n', 'o', 'r', 's', 't', 'u'};
const char *types[] = {"char",     "short",   "int",      "float",    "double",
                       "long",     "struct",  "enum",     "union",    "void",
                       "int8_t",   "uint8_t", "int8_t",   "uint16_t", "int16_t",
                       "uint32_t", "int32_t", "uint64_t", "int64_t",  "size_t",
                       "ssize_t"};
const char *qualifiers[] = {"const",  "volatile", "static",  "*",
                            "extern", "unsigned", "restrict"};
enum token_class {
  invalid = 0,
  type,
  qualifier,
  identifier,
  length,
  typedefn,
  whitespace
};
const char *kind_names[] = {"invalid", "type",     "qualifier", "identifier",
                            "length",  "typedefn", "whitespace"};
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
  bool have_qualifier;
  bool last_dimension_unspecified;
  bool is_function;
  bool is_enum;
  bool is_struct;
  bool is_pointer;
  bool is_typedef;
  bool has_enum_constants;
  size_t cursor;
  char enumerator_list[MAXTOKENLEN];
  size_t array_dimensions;
  size_t array_lengths;
  bool has_function_params;
  bool has_struct_members;
  size_t stacklen;
  struct token stack[MAXTOKENS];
  struct parser_props *prev;
  struct parser_props *next;
  FILE *out_stream;
  FILE *err_stream;
};

/* documentation functions */
void usage(void);
void limitations();

/* function to modify the parser */
void initialize_parser(struct parser_props *parser);
void reset_parser(struct parser_props *parser);
struct parser_props *make_parser(struct parser_props *const parser);
void free_all_parsers(struct parser_props *parser);

/*
 * Functions which characterize input.  A returned false value indicates an
 * error.  Functions with two parameters modify the non-const one.
 */
bool is_all_blanks(const char *input);
bool has_alnum_chars(const char *input);
bool is_numeric(const char *input);
static bool is_type_char(const char c);
static bool is_name_char(const char c);
static bool has_any_name_chars(const char *s);
bool parens_match(const char *offset_decl, size_t *pair_count);
bool check_for_array_dimensions(struct parser_props *parser,
                                const char *offset_decl);
bool check_for_function_parameters(struct parser_props *parser,
                                   const char *offset_decl);
bool check_for_struct_members(struct parser_props *parser,
                              const char *offset_decl);
bool check_for_enum_constants(struct parser_props *parser,
                              const char *offset_decl);

/* functions which modify input */
size_t trim_leading_whitespace(const char *input, char *trimmed);
size_t trim_trailing_whitespace(const char *input, char *trimmed);
void elide_assignments(char **input);
bool handle_trailing_delim(char **output, const char *input, const char delim);
bool truncate_input(char **input, const struct parser_props *parser);

/* debugging functions */
void show_parser_list(const struct parser_props *parser);
void showstack(const struct token *stack, const size_t stacklen,
               FILE *out_stream);

/* parser helper functions */
bool have_stacked_compound_type(const struct parser_props *parser);
bool handled_compound_type(struct parser_props *parser,
                           const char *progress_ptr, struct token *this_token);
bool all_identifiers_are_enum_constants(const struct parser_props *parser);
bool first_identifier_is_enumerator(const struct parser_props *parser,
                                    const char *user_input);
void handle_trailing_instance_name(struct parser_props *parser,
                                   char *user_input);
bool process_secondary_params(struct parser_props *parser, char *user_input);
size_t process_array_length(struct parser_props *parser,
                            const char *offset_string,
                            struct token *this_token);
void process_array_dimensions(struct parser_props *parser, char *user_input,
                              struct token *this_token);
bool process_enum_constants(struct parser_props *parser, char *user_input);
bool handled_extended_parsing(struct parser_props *parser, char *user_input,
                              struct token *this_token);

/* output functions */
void reverse_lengths(struct parser_props *parser);
void reorder_qualifier_and_type(struct parser_props *parser);
void reorder_array_identifier_and_lengths(struct parser_props *parser);
void reorder_stacks(struct parser_props *parser);
int pop_stack(struct parser_props *parser, bool no_enum_instance);
int pop_all(struct parser_props *parser);

/* the core parser functions */
enum token_class get_kind(const char *intoken);
size_t gettoken(struct parser_props *parser, const char *declstring,
                struct token *this_token);
void finish_token(struct parser_props *parser, const char *offset_decl,
                  struct token *this_token, const size_t ctr);
void push_stack(struct parser_props *parser, struct token *this_token);
size_t load_stack(struct parser_props *parser, char *user_input);

/* functions to process user input */
bool input_parsing_successful(struct parser_props *parser, char inputstr[]);
size_t process_stdin(char stdinp[], FILE *input_stream);
size_t find_input_string(const char from_user[], char inputstr[], FILE *stream);

#endif
