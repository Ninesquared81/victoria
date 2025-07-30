#include <assert.h>  // assert.
#include <stdio.h>   // sprintf.
#include <string.h>  // memcpy, strlen.

#include "crvic_string_buffer.h"

#define SB_LEFT_COUNT(sb) (STRING_BUFFER_SIZE - 1 - (sb).count)
#define SB_CURRENT(sb) ((sb).buffer + (sb).count)

void sb_clear(struct string_buffer *sb) {
    memset(sb->buffer, 0, sb->count);
    sb->count = 0;
    sb->had_error = false;
}

bool sb_add_string(struct string_buffer *sb, const char *string) {
    assert(sb != NULL);
    assert(sb->count < STRING_BUFFER_SIZE);
    assert(SB_CURRENT(*sb)[0] == '\0');
    if (string == NULL) return true;
    if (sb->had_error) return false;
    size_t left_count = SB_LEFT_COUNT(*sb);
    size_t str_count = strlen(string);
    if (str_count > left_count) {
        sb->had_error = true;
        return false;
    }
    memcpy(SB_CURRENT(*sb), string, str_count);
    sb->count += str_count;
    return true;
}

bool sb_add_formatted(struct string_buffer *sb, const char *fmt, ...) {
    assert(sb != NULL);
    assert(fmt != NULL);
    assert(sb->count < STRING_BUFFER_SIZE);
    if (sb->had_error) return false;
    va_list vargs;
    va_start(vargs, fmt);
    bool ret = sb_add_vformatted(sb, fmt, vargs);
    va_end(vargs);
    return ret;
}

bool sb_add_vformatted(struct string_buffer *sb, const char *fmt, va_list vargs) {
    assert(sb != NULL);
    assert(fmt != NULL);
    assert(sb->count < STRING_BUFFER_SIZE);
    assert(SB_CURRENT(*sb)[0] == '\0');
    if (sb->had_error) return false;
    size_t left_count = SB_LEFT_COUNT(*sb);
    size_t dry_run_count = vsnprintf(NULL, 0, fmt, vargs);
    if (dry_run_count > left_count) {
        sb->had_error = true;
        return false;
    }
    size_t real_count = vsnprintf(SB_CURRENT(*sb), left_count + 1, fmt, vargs);
    assert(real_count == dry_run_count);  // This REALLY ought to hold. If it DOES fail, we jump ship quick.
    sb->count += real_count;
    return true;
}
