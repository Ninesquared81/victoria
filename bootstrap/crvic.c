#include <stdlib.h>  // EXIT_SUCCESS, EXIT_FAILURE.
#include <stdio.h>

#include "region.h"

#include "crvic_ast.h"
#include "crvic_backend.h"
#include "crvic_frontend.h"
#include "crvic_type.h"

#define REGION_SIZE 0x4000
#define INBUF_SIZE 0x8000

struct lxl_string_view read_source(const char *in_filename) {
    static char input_buffer[INBUF_SIZE] = {0};
    FILE *f = fopen(in_filename, "r");
    if (f == NULL) {
        perror("Failed to open input file");
        exit(1);
    }
    size_t length = fread(input_buffer, 1, INBUF_SIZE - 1, f);
    if (ferror(f)) {
        perror("Failed to read input file");
        fclose(f);
        exit(1);
    }
    if (!feof(f)) {
        fprintf(stderr, "Warning: input file truncated.\n");
    }
    fclose(f);
    return (struct lxl_string_view) {
        .start = input_buffer,
        .length = length,
    };
}

int main(void) {
    const char *in_filename = "in.vic";
    const char *out_filename = "out.c";
    struct lxl_string_view source = read_source(in_filename);
    init_parser(source);  // This also initialises the lexer.
    struct region *region = create_region(STDLIB_ALLOCATOR_AD, REGION_SIZE);
    struct ast_list nodes = parse(region);
    init_type_checker();
    if (!type_check(&nodes)) {
        // Error messages already printed.
        destroy_region(region);
        exit(1);
    }
    static struct string_buffer sb = {0};
    enum cgen_error error = crvic_generate_c_file(nodes, &sb);
    destroy_region(region);
    if (error) {
        fprintf(stderr, "Something went wrong: %d\n", error);
        return EXIT_FAILURE;
    }
    FILE *f = fopen(out_filename, "w");
    if (f == NULL) {
        perror("fopen() failed");
        return EXIT_FAILURE;
    }
    int exit_code = EXIT_SUCCESS;
    printf("%zu:\n%s", sb.count, sb.buffer);
    if ((size_t)fprintf(f, "%s", sb.buffer) != sb.count) {
        perror("Failed to write output file");
        exit_code = EXIT_FAILURE;
    }
    if (fclose(f) == EOF) {
        fprintf(stderr, "fclose() failed!\n");
        exit_code = EXIT_FAILURE;
    }
    return exit_code;
}
