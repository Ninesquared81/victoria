#ifndef CRVIC_STRING_BUFFER_H
#define CRVIC_STRING_BUFFER_H

#include <stdarg.h>   // va_list.
#include <stdbool.h>  // bool.

#define STRING_BUFFER_SIZE 4096u

struct string_buffer {
    size_t count;
    bool had_error;
    char buffer[STRING_BUFFER_SIZE];
};

bool sb_add_string(struct string_buffer *sb, const char *string);
bool sb_add_formatted(struct string_buffer *sb, const char *fmt, ...);
bool sb_add_vformatted(struct string_buffer *sb, const char *fmt, va_list vargs);

#endif
