/*
 * Lexel -- a simple, general purpose lexing library in C.
 *
 * Lexel is a single-header ibrary; this file comprises the entirety of the library.
 * To include with function definitions:
 * + #define LEXEL_IMPLEMENTATION
 *   #include "lexel.h"
 * To include only function/type declarations:
 *   // LEXEL_IMPLEMENTATION not defined.
 *   #include "lexel.h"
 *
 * MIT License
 *
 * Copyright (c) 2024 Ninesquared81
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */

#ifndef LEXEL_H
#define LEXEL_H

#include <stdbool.h>  // bool, false, true -- requires C99
#include <stddef.h>   // size_t, ptrdiff_t
#include <limits.h>   // INT_MAX

// CUSTOMISATION OPTIONS.

// These options customise certain behaviours of lexel.
// They're defined at the top of the file for visibility.

// Customisable options can be given custom definitions before including lexel.h.


// LEXEL_IMPLEMENTATION enables implementation of lexel functions. It should be defined at most ONCE.

// LXL_NO_ASSERT disables lexel library assertions. This setting is independant of NDEBUG.


// This option controls which macro should be used for lexel library assertions.
// The default value is the standard assert() macro.
// NOTE: this macro should not be customised to merely disable assertions. To do that, use LXL_NO_ASSERT.
#ifndef LXL_ASSERT_MACRO
 #include <assert.h>
 #define LXL_ASSERT_MACRO assert
#endif

// END CUSTOMISATION OPTIONS.

// META-DEFINITIONS.

// These definitions are not part of the lexel interface per se, but have special signficance within this file.

// LXL_ASSERT() is lexel's library assertion macro. It should not be directly customised.
// Use LXL_ASSERT_MACRO and LXL_NO_ASSERT to cusomise instead.
#ifndef LXL_NO_ASSERT
 #define LXL_ASSERT(...) LXL_ASSERT_MACRO(__VA_ARGS__)
#else  // Disable library assertions.
 #define LXL_ASSERT(...) ((void)0)
#endif

// This marks a location in code as logically unreachable.
// If the assertion fires, it suggests a bug in lexel itself.
#define LXL_UNREACHABLE() LXL_ASSERT(0 && "Unreachable. This may be a bug in lexel.")

// END META-DEFINITIONS.

// LEXEL CORE.

// These are the core definitions for lexel -- the lexer and token.

// The line and column position with text.
struct lxl_location {
    int line, column;
};

// Lex error codes.
enum lxl_lex_error {
    LXL_LERR_OK = 0,                  // No error.
    LXL_LERR_GENERIC = -16,           // Generic error.
    LXL_LERR_EOF = -17,               // Unexepected EOF.
    LXL_LERR_UNCLOSED_COMMENT = -18,  // A block comment had no closing delimiter before the end.
    LXL_LERR_UNCLOSED_STRING = -19,   // A string-like literal had no closing delimiter before the end.
    LXL_LERR_INVALID_INTEGER = -20,   // An integer literal was invalid (e.g. had a prefix but no payload).
    LXL_LERR_INVALID_FLOAT = -21,     // A floating-point literal was invalid.
};

// Lexer status.
enum lxl_lexer_status {
    LXL_LSTS_READY,              // Ready to lex next token.
    LXL_LSTS_LEXING,             // In the process of lexing a token.
    LXL_LSTS_FINISHED,           // Reached the end of tokens.
    LXL_LSTS_FINISHED_ABNORMAL,  // Reached the end of tokens abnormally.
};

enum lxl_word_lexing_rule {
    LXL_LEX_SYMBOLIC,  // Lex all symbolic characters (any non-whitespace).
    LXL_LEX_WORD,      // Lex only word characters (any non-reserved symbolic).
};

// A pair of delimiters for strings and block comments, e.g. "/*" and "*/" for C-style comments.
struct lxl_delim_pair {
    const char *opener;
    const char *closer;
};

// Whether a string should be lexed as single line or multiline.
// Used as an argument of `lxl_lexer__lex_string()`
enum lxl_string_type {
    LXL_STRING_LINE,
    LXL_STRING_MULTILINE,
};

// The main lexer object.
struct lxl_lexer {
    const char *start;        // The start of the lexer's source code.
    const char *end;          // The end of the lexer's source code.
    const char *current;      // Pointer to the current character.
    struct lxl_location pos;  // The current position (line, column) in the source.
    const char *const *line_comment_openers;            // List of line comment openers.
    const struct lxl_delim_pair *nestable_comment_delims;   // List of paired nestable comment delimiters.
    const struct lxl_delim_pair *unnestable_comment_delims; // List of paired unnestable comment delimiters.
    const struct lxl_delim_pair *line_string_delims;  // List of paired line string-like literal delimiters.
    const struct lxl_delim_pair *multiline_string_delims; // List of multiline string-like literal delimiters.
    const char *string_escape_chars;   // List of escape characters in strings (ignore delimiters after).
    const int *line_string_types;      // List of token types associated with each line string delimiter.
    const int *multiline_string_types; // List of token types associated with each multiline string delimiter.
    const char *digit_separators;      // List of digit separator characters allowed in number literals.
    const char *const *number_signs;      // List of signs which can precede number literals (e.g. "+", "-").
    const char *const *integer_prefixes;  // List of prefixes for integer literals.
    const int *integer_bases;             // List of bases associated with each prefix.
    const char *const *integer_suffixes;  // List of suffixes for integer literals.
    int default_int_type;                 // Default token type for integer literals.
    int default_int_base;                 // Default base for (unprefixed) integer literals.
    const char *const *float_prefixes;    // List of prefixes for floating-point literals.
    const int *float_bases;               // List of bases associated with each float prefix.
    const char *const *exponent_markers;  // List of exponent markers (e.g. "e") for each float prefix.
    const char *const *exponent_signs;    // List of signs allowed in float exponents (default: ["+", "-"]).
    const char *const *radix_separators;  // List of radix separators for float literals (default: ["."]).
    const char *const *float_suffixes;    // List of suffixes for float literals.
    int default_float_type;               // Default token type for float literals.
    int default_float_base;               // Default base for (unprefixed) float literals.
    const char *default_exponent_marker; // Default exponent marker for float literals (default: "e").
    const char *const *puncts;   // List of (non-word) punctaution token values (e.g., "+", "==", ";", etc.).
    const int *punct_types;      // List of token types corresponding to each punctuation token above.
    const char *const *keywords; // List of keywords (word tokens with unique types).
    const int *keyword_types;    // List of token types corresponding to each keyword.
    int default_word_type;       // Default word token type (for non-keywords).
    enum lxl_word_lexing_rule word_lexing_rule;  // The word lexing rule to use (default: symbolic).
    int previous_token_type;      // The type of the most recently lexed token.
    int line_ending_type;         // The type to use for line ending tokens (default: LXL_TOKEN_LINE_ENDING).
    enum lxl_lex_error error;     // Error code set to the current lexing error.
    enum lxl_lexer_status status; // Current status of the lexer.
    bool emit_line_endings;       // Should line endings have their own tokens? (default: false)
    bool collect_line_endings;    // Should consecutive line ending tokens be combined? (default: true)
};

// A lexical token.
// The token's value is stored as a string (via the `start` and `end` pointers).
// Further processing of this value is left to the caller.
// The `token_type` determines the type of the token. The meanings of different types
// is left to the caller, but negative types are reserved by lexel and have special
// meanings. For example, a value of -1 (see LXL_TOKENS_END) denotes the end of the
// token stream.
struct lxl_token {
    const char *start;        // The start of the token.
    const char *end;          // The end of the token.
    struct lxl_location loc;  // The location (line, column) of the token in the source.
    int token_type;           // The type of the lexical token. Negative values have special meanings.
};

// END LEXEL CORE.

// LEXEL ADDITIONAL.

// Additional definitions beyond the core above.

// A string view.
// This is a read-only (non-owning) view into a string, consiting of a `start` pointer and `length`.
struct lxl_string_view {
    const char *start;
    size_t length;
};

// END LEXEL ADDITIONAL.


// LEXEL MAGIC VALUES (MVs).

// These enums define human-friendly names for the various magic values used by lexel.

enum lxl__token_mvs {
    LXL_TOKENS_END = -1,           // Special token type signifying the end of the token stream.
    LXL_TOKEN_UNINIT = -2,         // Special token type for a token whose type is yet to be determined.
    LXL_TOKENS_END_ABNORMAL = -3,  // Special token type signifying an abnormal end of the token stream.
    LXL_TOKEN_LINE_ENDING = -4,    // Special token type signifying the end of a line.
    LXL_TOKEN_NO_TOKEN = -5,       // Special token type for a non-existant token.
    // See enum lxl_lex_error for token error types.
};


// A string containing all the characters lexel considers whitespace apart from line feed, which has
// special handling in the lexer.
#define LXL_WHITESPACE_CHARS_NO_LF " \t\r\f\v"
// A string contining all the characters lexel considers whitespace (including line feed).
#define LXL_WHITESPACE_CHARS LXL_WHITESPACE_CHARS_NO_LF "\n"

// A string containing the basic uppercase latin alphabet in order.
#define LXL_BASIC_UPPER_LATIN_CHARS "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
// A string containing the basic lowercase latin alphabet in order.
#define LXL_BASIC_LOWER_LATIN_CHARS "abcdefghijklmnopqrstuvwxyz"
// A string containing the basic mixed case latin alphabet in alphabetical order.
#define LXL_BASIC_MIXED_LATIN_CHARS "AaBbCcDdEeFfGgHhIiJjKkLlMmNnOoPpQqRrSsTtUuVvWwXxYyZz"
// A string containing the digits 0--9 in order.
#define LXL_DIGITS "0123456789"

// END LEXEL MAGIC VALUES.


// TOKEN INTERFACE.

// These functions are for working with tokens.

// Return whether `tok` is a special end-of-tokens token.
#define LXL_TOKEN_IS_END(tok) \
    ((tok).token_type == LXL_TOKENS_END || (tok).token_type == LXL_TOKENS_END_ABNORMAL)

// Return whetehr `tok` is a special error token.
#define LXL_TOKEN_IS_ERROR(tok) ((tok).token_type <= LXL_LERR_GENERIC)

// Return the token's value as a string view.
struct lxl_string_view lxl_token_value(struct lxl_token token);

// Return a textual representation of the error code.
const char *lxl_error_message(enum lxl_lex_error error);

// END TOKEN INTERFACE.


// LEXER EXTERNAL INTERFACE.

// These functions are for communicating with the lexer, e.g. when parsing.

// Create a new `lxl_lexer` object from start and end pointers.
// `start` and `end` must be valid, non-NULL pointers, moreover, `end` must point one past the end
//  of the the string beginning at `start`.
struct lxl_lexer lxl_lexer_new(const char *start, const char *end);

// Create a new `lxl_lexer` object from a string view.
struct lxl_lexer lxl_lexer_from_sv(struct lxl_string_view sv);

// Get the next token from the lexer. A token of type LXL_TOKENS_END is returned when
// the token stream is exhausted.
struct lxl_token lxl_lexer_next_token(struct lxl_lexer *lexer);

// Return whether the token stream of the lexer is exhausted
// (i.e. there are no more tokens in the source code).
bool lxl_lexer_is_finished(struct lxl_lexer *lexer);

// Reset the lexer to the start of its input.
void lxl_lexer_reset(struct lxl_lexer *lexer);

// Construct a zero-terminated array to use for setting lexer fields calling for lists.
// Requires at least one element.
#define LXL_LIST(type, ...) ((type[]) {__VA_ARGS__, 0})

// Construct a NULL-terminated list of strings.
#define LXL_LIST_STR(...) LXL_LIST(const char *, __VA_ARGS__)

// Construct a {0}-terminated list of delimiter pairs.
#define LXL_LIST_DELIMS(...) ((struct lxl_delim_pair[]) {__VA_ARGS__, {0}})

// END LEXER EXTERNAL INTERFACE.


// LEXER INTERNAL INTERFACE.

// These functions are used by the lexer to alter its own state.
// They should not be used from a parser, but the interface is exposed to make writing a custom lexer easier.


// Return the number of characters consumed so far.
ptrdiff_t lxl_lexer__head_length(struct lxl_lexer *lexer);
// Return the number of characters left in the lexer's source.
ptrdiff_t lxl_lexer__tail_length(struct lxl_lexer *lexer);
// Return the number of characters consumed after `start_point`.
ptrdiff_t lxl_lexer__length_from(struct lxl_lexer *lexer, const char *start_point);
// Return the number of characters between now and `end_point`.
ptrdiff_t lxl_lexer__length_to(struct lxl_lexer *lexer, const char *end_point);

// Return whether the lexer is at the end of its input.
bool lxl_lexer__is_at_end(struct lxl_lexer *lexer);
// Return whether the lexer is at the start of its input.
bool lxl_lexer__is_at_start(struct lxl_lexer *lexer);
// Return the current character and advance the lexer to the next character.
char lxl_lexer__advance(struct lxl_lexer *lexer);
// Advance the lexer by up to n characters and return whether all n characters could be advanced
// (this will be false when there are fewer than n characters left, in which case, the lexer
// reaches the end and stops).
bool lxl_lexer__advance_by(struct lxl_lexer *lexer, size_t n);
// Advance the lexer to a future point in its input.
bool lxl_lexer__advance_to(struct lxl_lexer *lexer, const char *future);
// Rewind the lexer to the previous character and return whether the rewind was successful (the lexer
// cannot be rewound beyond its starting point).
bool lxl_lexer__rewind(struct lxl_lexer *lexer);
// Rewind the lexer by up to n characters and return whether all n characters could be rewound.
bool lxl_lexer__rewind_by(struct lxl_lexer *lexer, size_t n);
// Rewind the lexer to a previous point in its input and return whether all characters could be rewound.
bool lxl_lexer__rewind_to(struct lxl_lexer *lexer, const char *prev);

// Recalculate the current column in the lexer.
void lxl_lexer__recalc_column(struct lxl_lexer *lexer);

// Return whether the lexer can emit a line ending token when it sees an LF.
bool lxl_lexer__can_emit_line_ending(struct lxl_lexer *lexer);

// Return non-NULL if the current current matches any of those passed but do not consume it, otherwise,
// return NULL. On success, the return value is the pointer to the matching character, i.e., into the
// null-terminated string `chars`.
const char *lxl_lexer__check_chars(struct lxl_lexer *lexer, const char *chars);
// Return whether the next characters match exactly the string passed, but do not consume them.
bool lxl_lexer__check_string(struct lxl_lexer *lexer, const char *s);
// Return whether the next n characters match the first n characters of the string passed,
// but do not consume them.
bool lxl_lexer__check_string_n(struct lxl_lexer *lexer, const char *s, size_t n);
// Return whether the next characters match one of the strings passed, but do not consume the string.
bool lxl_lexer__check_strings(struct lxl_lexer *lexer, const char *const *strings);
// Return whether the current character is whitespace (see LXL_WHITESPACE_CHARS).
bool lxl_lexer__check_whitespace(struct lxl_lexer *lexer);
// Return whether the current characer is reserved (has a special meaning, like starting a comment or string).
bool lxl_lexer__check_reserved(struct lxl_lexer *lexer);
// Return whether the current character is a line comment opener.
bool lxl_lexer__check_line_comment(struct lxl_lexer *lexer);
// Return whether the next characters comprise a block comment.
bool lxl_lexer__check_block_comment(struct lxl_lexer *lexer);
// Return whether the next characters comprise a nestable block comment.
bool lxl_lexer__check_nestable_comment(struct lxl_lexer *lexer);
// Return whether the next charactrs comprise an unnestable block comment.
bool lxl_lexer__check_unnestable_comment(struct lxl_lexer *lexer);
// Return non-NULL if the next characters comprise one of the lexer's string openers but do not consume
// them, otherwise, return NULL. On success, the return value is the pointer to the matching opener.
const struct lxl_delim_pair *lxl_lexer__check_string_opener(struct lxl_lexer *lexer,
                                                        enum lxl_string_type string_type);
// Return whether the current character is digit of the specified base but do not consume it. The base is
// an integer in the range  2--36 (inclusive). For bases 11+, the letters a--z (case-insensitive) are
// used for digit values 10+ (as in hexadecimal).
bool lxl_lexer__check_digit(struct lxl_lexer *lexer, int base);
// Return whether the current character is a digit separator but do not consume it.
bool lxl_lexer__check_digit_separator(struct lxl_lexer *lexer);
// Return whether the current character is a digit (see above) or digit separator but do not consume it.
bool lxl_lexer__check_digit_or_separator(struct lxl_lexer *lexer, int base);
// Return non-zero if the next characters comprise an integer literal prefix but do not consume them. The
// return value is the base corresponding to the matched prefix.
int lxl_lexer__check_int_prefix(struct lxl_lexer *lexer);
// Return whether the next characters comprise an integer literal suffix but do not consume them.
bool lxl_lexer__check_int_suffix(struct lxl_lexer *lexer);
// Return non-zero if the next characters comprise a float literal prefix but do not consume them. The
// return value is the base corresponding to the matched prefix and OUT_exponent_marker is written
// with the corresponding exponent marker.
int lxl_lexer__check_float_prefix(struct lxl_lexer *lexer, const char **OUT_exponent_marker);
// Return whether the next characters comprise a float literal suffix but do not consume them.
bool lxl_lexer__check_float_suffix(struct lxl_lexer *lexer);
// Return whether the next characters comprise a number literal sign (e.g. "+", "-") but do not consume them.
bool lxl_lexer__check_number_sign(struct lxl_lexer *lexer);
// Return whether the next characters comprise a float radix separator (e.g. ".") but do not consume them.
bool lxl_lexer__check_radix_separator(struct lxl_lexer *lexer);
// Return whether the next characters comprise a float exponent sign (e.g. "+", "-") but do not consume them.
bool lxl_lexer__check_exponent_sign(struct lxl_lexer *lexer);
// Return non-NULL if the next characters comprise an punct but do not consume them, otherwise,
// return NULL. On success, the return value is the pointer to the matching punct in the .puncts list.
const char *const *lxl_lexer__check_punct(struct lxl_lexer *lexer);

// Return non-NULL if the current current matches any of those passed and consume it if so, otherwise,
// return NULL. On success, the return value is the pointer to the matching character, i.e., into the
// null-terminated string `chars`.
const char *lxl_lexer__match_chars(struct lxl_lexer *lexer, const char *chars);
// Return whether the next characters match exactly the string passed, and consume them if so.
bool lxl_lexer__match_string(struct lxl_lexer *lexer, const char *s);
// Return whether the next n characters match the first n characters of the string passed,
// and consume them if so.
bool lxl_lexer__match_string_n(struct lxl_lexer *lexer, const char *s, size_t n);
// Return whether the next characters match one of the strings passed, and consume the string if so.
bool lxl_lexer__match_strings(struct lxl_lexer *lexer, const char *const *strings);
// Return whether the next characters comprise a line comment, and consume them if so.
bool lxl_lexer__match_line_comment(struct lxl_lexer *lexer);
// Return wheteher the next characters comprise a block comment, and consume them if so.
bool lxl_lexer__match_block_comment(struct lxl_lexer *lexer);
// Return whether the next characters comprise a nestable block comment, and consume them if so.
bool lxl_lexer__match_nestable_comment(struct lxl_lexer *lexer);
// Return whether the next characters comprise an unnestable block comment, and consume them if so.
bool lxl_lexer__match_unnestable_comment(struct lxl_lexer *lexer);
// Return non-NULL if the next characters comprise one of the lexer's string delimiters and consume them
// if so, otherwise, return NULL. On success, the return value is the pointer to the matching opener.
const struct lxl_delim_pair *lxl_lexer__match_string_opener(struct lxl_lexer *lexer,
                                                        enum lxl_string_type string_type);
// Return whether the current character is digit of the specified base, and consume it if so. The base is
// an integer in the range  2--36 (inclusive). For bases 11+, the letters a--z (case-insensitive) are
// used for digit values 10+ (as in hexadecimal).
bool lxl_lexer__match_digit(struct lxl_lexer *lexer, int base);
// Return whether the current character is a digit separator but do not consume it.
bool lxl_lexer__match_digit_separator(struct lxl_lexer *lexer);
// Return whether the current character is a digit (see above) or digit separator, and consume it if so.
bool lxl_lexer__match_digit_or_separator(struct lxl_lexer *lexer, int base);
// Return non-zero if the next characters comprise an integer literal prefix, and consume them if so. The
// return value is the base corresponding to the matched prefix.
int lxl_lexer__match_int_prefix(struct lxl_lexer *lexer);
// Return whether the next characters comprise an integer literal suffix, and consume them if so.
bool lxl_lexer__match_int_suffix(struct lxl_lexer *lexer);
// Return non-zero if the next characters comprise a float literal prefix, and consume them if so. The
// return value is the base corresponding to the matched prefix and OUT_exponent_marker is written
// with the corresponding exponent marker.
int lxl_lexer__match_float_prefix(struct lxl_lexer *lexer, const char **OUT_exponent_marker);
// Return whether the next characters comprise a float literal suffix, and consume them if so.
bool lxl_lexer__match_float_suffix(struct lxl_lexer *lexer);
// Return whether the next characters comprise a number literal sign (e.g. "+", "-"), and consume them if so.
bool lxl_lexer__match_number_sign(struct lxl_lexer *lexer);
// Return whether the next characters comprise a float radix separator (e.g. "."), and consume them if so.
bool lxl_lexer__match_radix_separator(struct lxl_lexer *lexer);
// Return whether the next characters comprise a float exponent sign (e.g. "+", "-"), and consume them if so.
bool lxl_lexer__match_exponent_sign(struct lxl_lexer *lexer);
// Return non-NULL if the next characters comprise an punct and consume them if so, otherwise,
// return NULL. On success, the return value is the pointer to the matching punct in the .puncts list.
const char *const *lxl_lexer__match_punct(struct lxl_lexer *lexer);

// Advance the lexer past any whitespace characters and return the number of characters consumed.
int lxl_lexer__skip_whitespace(struct lxl_lexer *lexer);
// Advance the lexer past the rest of the current line and return the number of characters consumed.
int lxl_lexer__skip_line(struct lxl_lexer *lexer);
// Advance the lexer past the current (possibly nestable) block comment (opener already consumed)
// and return the number of characters consumed.
int lxl_lexer__skip_block_comment(struct lxl_lexer *lexer, struct lxl_delim_pair delims, bool nested);

// Create an unitialised token starting at the lexer's current position.
struct lxl_token lxl_lexer__start_token(struct lxl_lexer *lexer);
// Finish the token ending at the lexer's current position. If an error ocurred during lexing of this
// token, emit an error token instead. The value still includes all the characters lexed.
void lxl_lexer__finish_token(struct lxl_lexer *lexer, struct lxl_token *token);
// Create a special `LXL_TOKENS_END` token at the lexer's current position.
struct lxl_token lxl_lexer__create_end_token(struct lxl_lexer *lexer);
// Create a special error token at the lexer's current position. If the lexer has no error set, use
// LXL_LERR_GENERIC as the error type. Note that this function will not include a value in the token.
// To emit a non-empty error token, use `lxl_lexer__finish_token()` instead.
struct lxl_token lxl_lexer__create_error_token(struct lxl_lexer *lexer);

// Consume all non-whitespace characters and return the number consumed.
int lxl_lexer__lex_symbolic(struct lxl_lexer *lexer);
// Consume a word token (non-reserved symbolic) and return the number of characters read.
int lxl_lexer__lex_word(struct lxl_lexer *lexer);
// Consume a string-like token delimited by `delim` and return the number of characters read.
int lxl_lexer__lex_string(struct lxl_lexer *lexer, const char *closer, enum lxl_string_type string_type);
// Consume the digits of an integer literal in the given base (2--36).
int lxl_lexer__lex_integer(struct lxl_lexer *lexer, int base);
// Consume the digits of a floating-point literal with the given base and exponent marker.
int lxl_lexer__lex_float(struct lxl_lexer *lexer, int index, const char *exponent_marker);

// Get the token type corresponding to the word specified.
int lxl_lexer__get_word_type(struct lxl_lexer *lexer, const char *word_start);

// END LEXER INTERNAL INTERFACE.


// LEXEL STRING VIEW.

// Functions and macros for working with string views.

// Create a `string_view` object from a null-terminated string.
struct lxl_string_view lxl_sv_from_string(const char *s);
// Create a `string_view` object from a C string literal token.
#define LXL_SV_FROM_STRLIT(lit) (struct lxl_string_view) {.start = lit, .length = sizeof(lit) - 1}
// Create a `string_view` object from `start` and `end` pointers.
struct lxl_string_view lxl_sv_from_startend(const char *start, const char * end);

// Get a pointer to (one past) the end of a string view.
#define LXL_SV_END(sv) ((sv).start + (sv).length)

// Format specifier for printf et al.
#define LXL_SV_FMT_SPEC "%.*s"
// Use when printing a string view with LXL_SV_FMT_SPEC. The argument is evalucated multiple times.
#define LXL_SV_FMT_ARG(sv) ((sv).length < INT_MAX) ? (int)(sv).length : INT_MAX, (sv).start

// END LEXEL STRING VIEW.


// Implementation.

#ifdef LEXEL_IMPLEMENTATION

#include <string.h>

// TOKEN FUNCTIONS.

struct lxl_string_view lxl_token_value(struct lxl_token token) {
    return lxl_sv_from_startend(token.start, token.end);
}

const char *lxl_error_message(enum lxl_lex_error error) {
    switch (error) {
    case LXL_LERR_OK: return "No error";
    case LXL_LERR_GENERIC: return "Generic error";
    case LXL_LERR_EOF: return "Unexpected EOF";
    case LXL_LERR_UNCLOSED_COMMENT: return "Unclosed block comment";
    case LXL_LERR_UNCLOSED_STRING: return "Unclosed string-like literal";
    case LXL_LERR_INVALID_INTEGER: return "Inavlid integer";
    case LXL_LERR_INVALID_FLOAT: return "Invalid floating-point literal";
    }
    LXL_UNREACHABLE();
    return NULL;  // Unreachable.
}

// END TOKEN FUNCTIONS.


// LEXER FUNCTIONS.

struct lxl_lexer lxl_lexer_new(const char *start, const char *end) {
    static const char *default_exponent_signs[] = {"+", "-", NULL};
    static const char *default_radix_separators[] = {".", NULL};
    return (struct lxl_lexer) {
        .start = start,
        .end = end,
        .current = start,
        .pos = {0, 0},
        .line_comment_openers = NULL,
        .nestable_comment_delims = NULL,
        .unnestable_comment_delims = NULL,
        .line_string_delims = NULL,
        .multiline_string_delims = NULL,
        .string_escape_chars = NULL,
        .line_string_types = NULL,
        .multiline_string_types = NULL,
        .number_signs = NULL,
        .digit_separators = NULL,
        .integer_prefixes = NULL,
        .integer_bases = NULL,
        .integer_suffixes = NULL,
        .default_int_type = LXL_LERR_GENERIC,
        .default_int_base = 0,
        .float_prefixes = NULL,
        .float_bases = NULL,
        .exponent_markers = NULL,
        .exponent_signs = default_exponent_signs,
        .radix_separators = default_radix_separators,
        .float_suffixes = NULL,
        .default_float_type = LXL_LERR_GENERIC,
        .default_float_base = 0,
        .default_exponent_marker = "e",
        .puncts = NULL,
        .punct_types = NULL,
        .keywords = NULL,
        .keyword_types = NULL,
        .default_word_type = LXL_TOKEN_UNINIT,
        .word_lexing_rule = LXL_LEX_SYMBOLIC,
        .previous_token_type = LXL_TOKEN_NO_TOKEN,
        .line_ending_type = LXL_TOKEN_LINE_ENDING,
        .error = LXL_LERR_OK,
        .status = LXL_LSTS_READY,
        .emit_line_endings = false,
        .collect_line_endings = true,
    };
}

struct lxl_lexer lxl_lexer_from_sv(struct lxl_string_view sv) {
    return lxl_lexer_new(sv.start, LXL_SV_END(sv));
}

struct lxl_token lxl_lexer_next_token(struct lxl_lexer *lexer) {
    if (lxl_lexer_is_finished(lexer)) {
        return lxl_lexer__create_end_token(lexer);
    }
    lxl_lexer__skip_whitespace(lexer);
    if (lexer->error) {
        return lxl_lexer__create_error_token(lexer);
    }
    else if (lxl_lexer__is_at_end(lexer)) {
        return lxl_lexer__create_end_token(lexer);
    }
    struct lxl_token token = lxl_lexer__start_token(lexer);
    const char *const *matched_string = NULL;
    const struct lxl_delim_pair *matched_lxl_delim_pair = NULL;
    int number_base = 0;
    const char *exponent_marker = NULL;
    if (lxl_lexer__match_chars(lexer, "\n")) {
        // If we cannot emit line endings, we should have already skipped this LF.
        LXL_ASSERT(lxl_lexer__can_emit_line_ending(lexer));
        token.token_type = lexer->line_ending_type;
    }
    else if ((matched_lxl_delim_pair = lxl_lexer__match_string_opener(lexer, LXL_STRING_LINE))) {
        lxl_lexer__lex_string(lexer, matched_lxl_delim_pair->closer, LXL_STRING_LINE);
        int delim_index = matched_lxl_delim_pair - lexer->line_string_delims;
        LXL_ASSERT(lexer->line_string_types != NULL);
        token.token_type = lexer->line_string_types[delim_index];
    }
    else if ((matched_lxl_delim_pair = lxl_lexer__match_string_opener(lexer, LXL_STRING_MULTILINE))) {
        lxl_lexer__lex_string(lexer, matched_lxl_delim_pair->closer, LXL_STRING_MULTILINE);
        int delim_index = matched_lxl_delim_pair - lexer->multiline_string_delims;
        LXL_ASSERT(lexer->multiline_string_types != NULL);
        token.token_type = lexer->multiline_string_types[delim_index];
    }
    else if ((number_base = lxl_lexer__match_int_prefix(lexer))) {
        LXL_ASSERT(number_base > 1);  // Base should be valid here.
        if (lxl_lexer__lex_integer(lexer, number_base)) {
            token.token_type = lexer->default_int_type;
            if (lxl_lexer__check_radix_separator(lexer) && lexer->default_float_base != 0) {
                // Re-lex as float.
                lxl_lexer__rewind_to(lexer, token.start);
                if ((number_base = lxl_lexer__match_float_prefix(lexer, &exponent_marker))) {
                    goto try_lex_float;
                }
                else {
                    token.token_type = LXL_LERR_INVALID_INTEGER;
                }
            }
        }
        else {
            token.token_type = LXL_LERR_INVALID_INTEGER;
        }
        if (!lxl_lexer__match_int_suffix(lexer)) {

        }
    }
    else if ((number_base = lxl_lexer__match_float_prefix(lexer, &exponent_marker))) {
try_lex_float:
        LXL_ASSERT(number_base > 1);  // Base should be valid here.
        LXL_ASSERT(exponent_marker != NULL);
        if (lxl_lexer__lex_float(lexer, number_base, exponent_marker)) {
            token.token_type = lexer->default_float_type;
        }
        else {
            token.token_type = LXL_LERR_INVALID_FLOAT;
        }

    }
    else if ((matched_string = lxl_lexer__match_punct(lexer))) {
        int punct_index = matched_string - lexer->puncts;
        LXL_ASSERT(lexer->punct_types != NULL);
        token.token_type = lexer->punct_types[punct_index];
    }
    else {
        switch (lexer->word_lexing_rule) {
        case LXL_LEX_SYMBOLIC:
            lxl_lexer__lex_symbolic(lexer);
            break;
        case LXL_LEX_WORD:
            lxl_lexer__lex_word(lexer);
            break;
        }
        token.token_type = lxl_lexer__get_word_type(lexer, token.start);
    }
    lxl_lexer__finish_token(lexer, &token);
    return token;
}

bool lxl_lexer_is_finished(struct lxl_lexer *lexer) {
    return lexer->status == LXL_LSTS_FINISHED || lexer->status == LXL_LSTS_FINISHED_ABNORMAL;
}

void lxl_lexer_reset(struct lxl_lexer *lexer) {
    lexer->current = lexer->start;
    lexer->status = LXL_LSTS_READY;
}

ptrdiff_t lxl_lexer__head_length(struct lxl_lexer *lexer) {
    return lexer->current - lexer->start;
}

ptrdiff_t lxl_lexer__tail_length(struct lxl_lexer *lexer) {
    return lexer->end - lexer->current;
}

ptrdiff_t lxl_lexer__length_from(struct lxl_lexer *lexer, const char *start_point) {
    return lexer->current - start_point;
}

ptrdiff_t lxl_lexer__length_to(struct lxl_lexer *lexer, const char *end_point) {
    return end_point - lexer->current;
}

bool lxl_lexer__is_at_end(struct lxl_lexer *lexer) {
    return lexer->current >= lexer->end;
}

bool lxl_lexer__is_at_start(struct lxl_lexer *lexer) {
    return lexer->current <= lexer->start;
}

char lxl_lexer__advance(struct lxl_lexer *lexer) {
    if (lxl_lexer__is_at_end(lexer)) return '\0';
    if (*lexer->current != '\n') {
        ++lexer->pos.column;
    }
    else {
        lexer->pos.column = 0;
        ++lexer->pos.line;
    }
    ++lexer->current;
    return lexer->current[-1];
}

bool lxl_lexer__advance_by(struct lxl_lexer *lexer, size_t n) {
    while (n-- > 0) {
        if (lxl_lexer__is_at_end(lexer)) return false;
        ++lexer->current;
        if (*lexer->current == '\n') {
            ++lexer->pos.line;
        }
    }
    lxl_lexer__recalc_column(lexer);
    return true;
}

bool lxl_lexer__advance_to(struct lxl_lexer *lexer, const char *future) {
    ptrdiff_t length = lxl_lexer__length_to(lexer, future);
    LXL_ASSERT(length >= 0);
    return lxl_lexer__advance_by(lexer, length);
}

bool lxl_lexer__rewind(struct lxl_lexer *lexer) {
    if (lxl_lexer__is_at_start(lexer)) return false;
    --lexer->current;
    if (*lexer->current != '\n') {
        --lexer->pos.column;
    }
    else {
        lxl_lexer__recalc_column(lexer);
        --lexer->pos.line;
    }
    return true;
}

bool lxl_lexer__rewind_by(struct lxl_lexer *lexer, size_t n) {
    while (n-- > 0) {
        if (lxl_lexer__is_at_start(lexer)) return false;
        --lexer->current;
        if (*lexer->current == '\n') {
            --lexer->pos.line;
        }
    }
    lxl_lexer__recalc_column(lexer);
    return true;
}

bool lxl_lexer__rewind_to(struct lxl_lexer *lexer, const char *prev) {
    ptrdiff_t length = lxl_lexer__length_from(lexer, prev);
    LXL_ASSERT(length >= 0);
    return lxl_lexer__rewind_by(lexer, length);
}

void lxl_lexer__recalc_column(struct lxl_lexer *lexer) {
    lexer->pos.column = 0;
    for (const char *p = lexer->current; p != lexer->start && *p != '\n'; --p) {
        ++lexer->pos.column;
    }
}

bool lxl_lexer__can_emit_line_ending(struct lxl_lexer *lexer) {
    if (!lexer->emit_line_endings) return false;
    if (lexer->previous_token_type == LXL_TOKEN_LINE_ENDING) {
        return !lexer->collect_line_endings;
    }
    return true;
}

const char *lxl_lexer__check_chars(struct lxl_lexer *lexer, const char *chars) {
    if (chars == NULL) return NULL;  // Allow NULL.
    while (*chars != '\0') {
        if (*lexer->current == *chars) return chars;
        ++chars;
    }
    return NULL;
}

bool lxl_lexer__check_string(struct lxl_lexer *lexer, const char *s) {
    if (s == NULL) return false;
    size_t tail_length = lxl_lexer__tail_length(lexer);
    size_t n = strlen(s);
    if (n > tail_length) return false;
    return memcmp(lexer->current, s, n) == 0;
}

bool lxl_lexer__check_string_n(struct lxl_lexer *lexer, const char *s, size_t n) {
    if (s == NULL) return false;
    size_t tail_length = lxl_lexer__tail_length(lexer);
    if (n > tail_length) n = tail_length;
    return strncmp(lexer->current, s, n) == 0;
}

bool lxl_lexer__check_strings(struct lxl_lexer *lexer, const char *const *strings) {
    if (strings == NULL) return NULL;
    for (; *strings != NULL; ++strings) {
        if (lxl_lexer__check_string(lexer, *strings)) return true;
    }
    return false;
}

bool lxl_lexer__check_whitespace(struct lxl_lexer *lexer) {
    if (!lxl_lexer__can_emit_line_ending(lexer)) {
        return lxl_lexer__check_chars(lexer, LXL_WHITESPACE_CHARS);
    }
    return lxl_lexer__check_chars(lexer, LXL_WHITESPACE_CHARS_NO_LF);
}

bool lxl_lexer__check_reserved(struct lxl_lexer *lexer) {
    return lxl_lexer__check_whitespace(lexer)
        || lxl_lexer__check_line_comment(lexer)
        || lxl_lexer__check_block_comment(lexer)
        || !!lxl_lexer__check_string_opener(lexer, LXL_STRING_LINE)
        || !!lxl_lexer__check_string_opener(lexer, LXL_STRING_MULTILINE)
        || !!lxl_lexer__check_punct(lexer)
        ;
}

bool lxl_lexer__check_line_comment(struct lxl_lexer *lexer) {
    return lxl_lexer__check_strings(lexer, lexer->line_comment_openers);
}

bool lxl_lexer__check_block_comment(struct lxl_lexer *lexer) {
    return lxl_lexer__check_nestable_comment(lexer) || lxl_lexer__check_unnestable_comment(lexer);
}

bool lxl_lexer__check_nestable_comment(struct lxl_lexer *lexer) {
    if (lexer->nestable_comment_delims == NULL) return false;
    for (const struct lxl_delim_pair *delims = lexer->nestable_comment_delims;
         delims->opener != NULL;
         ++delims) {
        if (lxl_lexer__check_string(lexer, delims->opener)) return true;
    }
    return false;
}

bool lxl_lexer__check_unnestable_comment(struct lxl_lexer *lexer) {
    if (lexer->unnestable_comment_delims == NULL) return false;
    for (const struct lxl_delim_pair *delims = lexer->unnestable_comment_delims;
         delims->opener != NULL;
         ++delims) {
        if (lxl_lexer__check_string(lexer, delims->opener)) return true;
    }
    return false;
}

const struct lxl_delim_pair *lxl_lexer__check_string_opener(struct lxl_lexer *lexer,
                                                        enum lxl_string_type string_type) {
    const struct lxl_delim_pair *string_delims = (string_type == LXL_STRING_LINE)
        ? lexer->line_string_delims
        : lexer->multiline_string_delims;
    if (string_delims == NULL) return NULL;
    for (const struct lxl_delim_pair *delims = string_delims; delims->opener != NULL; ++delims) {
        if (lxl_lexer__check_chars(lexer, delims->opener)) return delims;
    }
    return NULL;
}

bool lxl_lexer__check_digit(struct lxl_lexer *lexer, int base) {
    if (base == 0) return false;
    LXL_ASSERT(2 <= base && base <= 26);
    char digits[] = LXL_DIGITS "" LXL_BASIC_MIXED_LATIN_CHARS;
    int end_digit_index = (base <= 10) ? base : 10 + 2*(base - 10);
    digits[end_digit_index] = '\0';  // Truncate array to only contain the needed digits.
    return lxl_lexer__check_chars(lexer, digits);
}

bool lxl_lexer__check_digit_separator(struct lxl_lexer *lexer) {
    if (lexer->digit_separators == NULL) return false;
    return lxl_lexer__check_chars(lexer, lexer->digit_separators);
}

bool lxl_lexer__check_digit_or_separator(struct lxl_lexer *lexer, int base) {
    return lxl_lexer__check_digit(lexer, base) || lxl_lexer__check_digit_separator(lexer);
}

int lxl_lexer__check_int_prefix(struct lxl_lexer *lexer) {
    const char *start = lexer->current;
    // Consume any leading sign to make detecting the prefix easier.
    // We'll rewind the lexer before returning.
    lxl_lexer__match_number_sign(lexer);
    if (lexer->integer_prefixes != NULL) {
        LXL_ASSERT(lexer->integer_bases != NULL);
        for (int i = 0; lexer->integer_prefixes[i] != NULL; ++i) {
            if (lxl_lexer__check_string(lexer, lexer->integer_prefixes[i])) {
                lxl_lexer__rewind_to(lexer, start);
                return lexer->integer_bases[i];
            }
        }
    }
    int default_base = lexer->default_int_base;
    return (lxl_lexer__check_digit(lexer, default_base)) ? default_base : 0;
}

bool lxl_lexer__check_int_suffix(struct lxl_lexer *lexer) {
    return lxl_lexer__check_strings(lexer, lexer->integer_suffixes);
}

int lxl_lexer__check_float_prefix(struct lxl_lexer *lexer, const char **OUT_exponent_marker) {
    const char *start = lexer->current;
    lxl_lexer__match_number_sign(lexer);
    if (lexer->float_prefixes) {
        LXL_ASSERT(lexer->float_bases != NULL);
        LXL_ASSERT(lexer->exponent_markers != NULL);
        for (int i = 0; lexer->float_prefixes[i] != NULL; ++i) {
            if (lxl_lexer__check_string(lexer, lexer->float_prefixes[i])) {
                lxl_lexer__rewind_to(lexer, start);
                *OUT_exponent_marker = lexer->exponent_markers[i];
                return lexer->float_bases[i];
            }
        }
    }
    lxl_lexer__rewind_to(lexer, start);
    if (!lxl_lexer__check_digit(lexer, lexer->default_float_base)) return 0;
    *OUT_exponent_marker = lexer->default_exponent_marker;
    return lexer->default_float_base;
}

bool lxl_lexer__check_float_suffix(struct lxl_lexer *lexer) {
    return lxl_lexer__check_strings(lexer, lexer->float_suffixes);
}

bool lxl_lexer__check_number_sign(struct lxl_lexer *lexer) {
    return lxl_lexer__check_strings(lexer, lexer->number_signs);
}

bool lxl_lexer__check_radix_separator(struct lxl_lexer *lexer) {
    return lxl_lexer__check_strings(lexer, lexer->radix_separators);
}

bool lxl_lexer__check_exponent_sign(struct lxl_lexer *lexer) {
    return lxl_lexer__check_strings(lexer, lexer->exponent_signs);
}

const char *const *lxl_lexer__check_punct(struct lxl_lexer *lexer) {
    if (lexer->puncts == NULL) return NULL;
    for (const char *const *punct = lexer->puncts; *punct != NULL; ++punct) {
        if (lxl_lexer__check_string(lexer, *punct)) return punct;
    }
    return NULL;
}

const char *lxl_lexer__match_chars(struct lxl_lexer *lexer, const char *chars) {
    const char *p = lxl_lexer__check_chars(lexer, chars);
    if (p != NULL) {
        lxl_lexer__advance(lexer);
    }
    return p;
}

bool lxl_lexer__match_string(struct lxl_lexer *lexer, const char *s) {
    if (lxl_lexer__check_string(lexer, s)) {
        size_t length = strlen(s);
        return lxl_lexer__advance_by(lexer, length);
    }
    return false;
}

bool lxl_lexer__match_string_n(struct lxl_lexer *lexer, const char *s, size_t n) {
    if (lxl_lexer__check_string_n(lexer, s, n)) {
        return lxl_lexer__advance_by(lexer, n);
    }
    return false;
}

bool lxl_lexer__match_strings(struct lxl_lexer *lexer, const char *const *strings) {
    if (strings == NULL) return NULL;
    for (; *strings != NULL; ++strings) {
        if (lxl_lexer__match_string(lexer, *strings)) return true;
    }
    return false;
}

bool lxl_lexer__match_line_comment(struct lxl_lexer *lexer) {
    if (!lxl_lexer__check_line_comment(lexer)) return false;
    lxl_lexer__skip_line(lexer);
    return true;
}

bool lxl_lexer__match_block_comment(struct lxl_lexer *lexer) {
    if (lxl_lexer__match_nestable_comment(lexer)) return true;
    return lxl_lexer__match_unnestable_comment(lexer);
}

bool lxl_lexer__match_nestable_comment(struct lxl_lexer *lexer) {
    if (lexer->nestable_comment_delims == NULL) return false;
    for (const struct lxl_delim_pair *delims = lexer->nestable_comment_delims;
         delims->opener != NULL;
         ++delims) {
        if (lxl_lexer__match_string(lexer, delims->opener)) {
            lxl_lexer__skip_block_comment(lexer, *delims, true);
            return true;
        }
    }
    return false;
}

bool lxl_lexer__match_unnestable_comment(struct lxl_lexer *lexer) {
    if (lexer->unnestable_comment_delims == NULL) return false;
    for (const struct lxl_delim_pair *delims = lexer->unnestable_comment_delims;
         delims->opener != NULL;
         ++delims) {
        if (lxl_lexer__match_string(lexer, delims->opener)) {
            lxl_lexer__skip_block_comment(lexer, *delims, false);
            return true;
        }
    }
    return false;
}

const struct lxl_delim_pair *lxl_lexer__match_string_opener(struct lxl_lexer *lexer,
                                                        enum lxl_string_type string_type) {
    const struct lxl_delim_pair *string_delims = (string_type == LXL_STRING_LINE)
        ? lexer->line_string_delims
        : lexer->multiline_string_delims;
    if (string_delims == NULL) return NULL;
    for (const struct lxl_delim_pair *delims = string_delims; delims->opener != NULL; ++delims) {
        if (lxl_lexer__match_chars(lexer, delims->opener)) return delims;
    }
    return NULL;
}

bool lxl_lexer__match_digit(struct lxl_lexer *lexer, int base) {
    if (!lxl_lexer__check_digit(lexer, base)) return false;
    return lxl_lexer__advance(lexer);
}

bool lxl_lexer__match_digit_separator(struct lxl_lexer *lexer) {
    if (!lxl_lexer__check_digit_separator(lexer)) return false;
    return lxl_lexer__advance(lexer);
}

bool lxl_lexer__match_digit_or_separator(struct lxl_lexer *lexer, int base) {
    if (!lxl_lexer__check_digit_or_separator(lexer, base)) return false;
    return lxl_lexer__advance(lexer);
}

int lxl_lexer__match_int_prefix(struct lxl_lexer *lexer) {
    lxl_lexer__match_number_sign(lexer);
    if (lexer->integer_prefixes != NULL) {
        LXL_ASSERT(lexer->integer_bases != NULL);
        for (int i = 0; lexer->integer_prefixes[i] != NULL; ++i) {
            if (lxl_lexer__match_string(lexer, lexer->integer_prefixes[i])) {
                return lexer->integer_bases[i];
            }
        }
    }
    return (lxl_lexer__check_digit(lexer, lexer->default_int_base)) ? lexer->default_int_base : 0;
}

bool lxl_lexer__match_int_suffix(struct lxl_lexer *lexer) {
    return lxl_lexer__match_strings(lexer, lexer->integer_suffixes);
}

int lxl_lexer__match_float_prefix(struct lxl_lexer *lexer, const char **OUT_exponent_marker) {
    lxl_lexer__match_number_sign(lexer);
    if (lexer->float_prefixes) {
        LXL_ASSERT(lexer->float_bases != NULL);
        LXL_ASSERT(lexer->exponent_markers != NULL);
        for (int i = 0; lexer->float_prefixes[i]; ++i) {
            if (lxl_lexer__match_string(lexer, lexer->float_prefixes[i])) {
                *OUT_exponent_marker = lexer->exponent_markers[i];
                return lexer->float_bases[i];
            }
        }
    }
    if (!lxl_lexer__match_digit(lexer, lexer->default_float_base)) return 0;
    *OUT_exponent_marker = lexer->default_exponent_marker;
    return lexer->default_float_base;
}

bool lxl_lexer__match_float_suffix(struct lxl_lexer *lexer) {
    return lxl_lexer__match_strings(lexer, lexer->float_suffixes);
}

bool lxl_lexer__match_number_sign(struct lxl_lexer *lexer) {
    return lxl_lexer__match_strings(lexer, lexer->number_signs);
}

bool lxl_lexer__match_radix_separator(struct lxl_lexer *lexer) {
    return lxl_lexer__match_strings(lexer, lexer->radix_separators);
}

bool lxl_lexer__match_exponent_sign(struct lxl_lexer *lexer) {
    return lxl_lexer__match_strings(lexer, lexer->exponent_signs);
}

const char *const *lxl_lexer__match_punct(struct lxl_lexer *lexer) {
    if (lexer->puncts == NULL) return NULL;
    for (const char *const *punct = lexer->puncts; *punct != NULL; ++punct) {
        if (lxl_lexer__match_string(lexer, *punct)) return punct;
    }
    return NULL;
}

int lxl_lexer__skip_whitespace(struct lxl_lexer *lexer) {
    const char *whitespace_start = lexer->current;
    for(;;) {
        if (lxl_lexer__check_whitespace(lexer)) {
            // Whitespace, skip.
            if (!lxl_lexer__advance(lexer)) break;
        }
        else if (lxl_lexer__check_string(lexer, "\n")) {
            // LF should have already been considered whitespace if we cannot emit a line ending here.
            LXL_ASSERT(lxl_lexer__can_emit_line_ending(lexer));
            break;
        }
        else if (lxl_lexer__match_line_comment(lexer)) {
            /* Do nothing; comment already consumed. */
        }
        else if (lxl_lexer__match_block_comment(lexer)) {
            /* Do nothing; comment already consumed. */
        }
        else {
            // Not a comment or whitespace.
            break;
        }
    }
    return lxl_lexer__length_from(lexer, whitespace_start);
}

int lxl_lexer__skip_line(struct lxl_lexer *lexer) {
    const char *line_start = lexer->current;
    // NOTE: final LF is NOT consumed.
    while (!lxl_lexer__check_chars(lexer, "\n")) {
        if (!lxl_lexer__advance(lexer)) break;
    }
    return lxl_lexer__length_from(lexer, line_start);
}

int lxl_lexer__skip_block_comment(struct lxl_lexer *lexer, struct lxl_delim_pair delims, bool nestable) {
    const char *comment_start = lexer->start;
    while (!lxl_lexer__match_string(lexer, delims.closer)) {
        if (nestable && lxl_lexer__match_string(lexer, delims.opener)) {
            if (lxl_lexer__skip_block_comment(lexer, delims, true) <= 0) {
                // Unclosed nested comment.
                lexer->error = LXL_LERR_UNCLOSED_COMMENT;
                break;
            }
        }
        else {
            if (!lxl_lexer__advance(lexer)) {
                lexer->error = LXL_LERR_UNCLOSED_COMMENT;
                break;
            }
        }
    }
    return lxl_lexer__length_from(lexer, comment_start);
}

struct lxl_token lxl_lexer__start_token(struct lxl_lexer *lexer) {
    if (lexer->status == LXL_LSTS_READY) lexer->status = LXL_LSTS_LEXING;
    return (struct lxl_token) {
        .start = lexer->current,
        .end = lexer->current,
        .loc = lexer->pos,
        .token_type = LXL_TOKEN_UNINIT,
    };
}

void lxl_lexer__finish_token(struct lxl_lexer *lexer, struct lxl_token *token) {
    token->end = lexer->current;
    if (lexer->error) {
        token->token_type = lexer->error;  // Set error as token type.
        lexer->error = LXL_LERR_OK;  // Clear error.
    }
    lexer->previous_token_type = token->token_type;
    if (lexer->status == LXL_LSTS_LEXING) lexer->status = LXL_LSTS_READY;  // Ready for the next token.
}

struct lxl_token lxl_lexer__create_end_token(struct lxl_lexer *lexer) {
    struct lxl_token token = lxl_lexer__start_token(lexer);
    if (lexer->status != LXL_LSTS_FINISHED_ABNORMAL) {
        token.token_type = LXL_TOKENS_END;
        lexer->status = LXL_LSTS_FINISHED;
    }
    else {
        token.token_type = LXL_TOKENS_END_ABNORMAL;
    }
    lxl_lexer__finish_token(lexer, &token);
    return token;
}

struct lxl_token lxl_lexer__create_error_token(struct lxl_lexer *lexer) {
    struct lxl_token token = lxl_lexer__start_token(lexer);
    if (!lexer->error) lexer->error = LXL_LERR_GENERIC;  // Emit a generic error token if no error is set.
    lxl_lexer__finish_token(lexer, &token);  // This function handles setting the error type.
    return token;
}

int lxl_lexer__lex_symbolic(struct lxl_lexer *lexer) {
    int count = 0;
    while (!lxl_lexer__is_at_end(lexer) && !lxl_lexer__check_whitespace(lexer)) {
        lxl_lexer__advance(lexer);
        ++count;
    }
    return count;
}

int lxl_lexer__lex_word(struct lxl_lexer *lexer) {
    int count = 0;
    while (!lxl_lexer__is_at_end(lexer) && !lxl_lexer__check_reserved(lexer)) {
        lxl_lexer__advance(lexer);
        ++count;
    }
    return count;
}

int lxl_lexer__lex_string(struct lxl_lexer *lexer, const char *closer, enum lxl_string_type string_type) {
    LXL_ASSERT(closer != NULL);
    const char *start = lexer->current;
    while (!lxl_lexer__match_string(lexer, closer)) {
        if (lxl_lexer__match_chars(lexer, lexer->string_escape_chars)) {
            // Consume escaped closer.
            lxl_lexer__match_string(lexer, closer);
        }
        // Consume non-delimiter character.
        char c = lxl_lexer__advance(lexer);
        if (c == '\0' || (c == '\n' && string_type == LXL_STRING_LINE)) {
            lexer->error = LXL_LERR_UNCLOSED_STRING;
            return lxl_lexer__length_from(lexer, start);
        }
    }
    return lxl_lexer__length_from(lexer, start);
}

int lxl_lexer__lex_integer(struct lxl_lexer *lexer, int base) {
    const char *start = lexer->current;
    int digit_count = 0;
    for (;;) {
        if (lxl_lexer__match_digit(lexer, base)) {
            ++digit_count;
        }
        else if (lxl_lexer__match_digit_separator(lexer)) {
            /* Do nothing. */
        }
        else {
            // Not a digit or separator.
            break;
        }
    }
    if (digit_count <= 0) {
        // Un-lex token which is not a valid integer literal.
        lxl_lexer__rewind_to(lexer, start);
    }
    return lxl_lexer__length_from(lexer, start);
}

int lxl_lexer__lex_float(struct lxl_lexer *lexer, int base, const char *exponent_marker) {
    const char *start = lexer->current;
    int digit_length = lxl_lexer__lex_integer(lexer, base);
    if (lxl_lexer__match_radix_separator(lexer)) {
        // Part after '.' OE.
        digit_length += lxl_lexer__lex_integer(lexer, base);
    }
    if (lxl_lexer__match_string(lexer, exponent_marker)) {
        // Part after 'e' OE.
        lxl_lexer__match_exponent_sign(lexer);  // Consume sign before actual exponent.
        digit_length += lxl_lexer__lex_integer(lexer, base);
    }
    if (digit_length <= 0) {
        // Un-lex token which is not a valid floating-point literal.
        lxl_lexer__rewind_to(lexer, start);
    }
    return lxl_lexer__length_from(lexer, start);
}

int lxl_lexer__get_word_type(struct lxl_lexer *lexer, const char *word_start) {
    if (lexer->keywords == NULL) return lexer->default_word_type;
    LXL_ASSERT(lexer->keyword_types != NULL);
    ptrdiff_t word_length = lxl_lexer__length_from(lexer, word_start);
    LXL_ASSERT(word_length > 0);  // Length = 0 is invalid.
    for (int i = 0; lexer->keywords[i] != NULL; ++i) {
        const char *keyword = lexer->keywords[i];
        size_t keyword_length = strlen(keyword);
        if (keyword_length != (size_t)word_length) continue;  // No match.
        if (memcmp(word_start, keyword, keyword_length) == 0) {
            return lexer->keyword_types[i];
        }
    }
    return lexer->default_word_type;
}

// END LEXER FUNCTIONS.

// STRING VIEW FUNCTIONS.

struct lxl_string_view lxl_sv_from_string(const char *s) {
    return (struct lxl_string_view) {.start = s, .length = strlen(s)};
}

struct lxl_string_view lxl_sv_from_startend(const char *start, const char *end) {
    LXL_ASSERT(start <= end);
    return (struct lxl_string_view) {.start = start, .length = end - start};
}

// END STRING VIEW FUNCTIONS.

#endif  // LEXEL_IMPLEMENTATION

#endif  // LEXEL_H
