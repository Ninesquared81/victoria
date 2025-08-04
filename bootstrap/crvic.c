#include <stdlib.h>  // EXIT_SUCCESS, EXIT_FAILURE.
#include <stdio.h>

#include "region.h"

#include "crvic_ast.h"
#include "crvic_backend.h"
#include "crvic_frontend.h"
#include "crvic_resources.h"
#include "crvic_type.h"

#define INBUF_SIZE 0x8000

#define OUT_FILENAME_SIZE 512

const char *get_input_file(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "ERROR: Missing input file.\n");
        fprintf(stderr, "Usage: %s file\n", argv[0]);
        exit(1);
    }
    if (argc > 2) {
        fprintf(stderr, "WARNING: Extra positional arguments.\n");
    }
    return argv[1];
}

const char *get_output_file(const char *in_filename) {
    static char out_filename[OUT_FILENAME_SIZE];
    int len = snprintf(out_filename, OUT_FILENAME_SIZE, "%s", in_filename);
    if (in_filename[len] != '\0') {
        fprintf(stderr, "Overlong input filename. Please use a shorter name.\n");
        exit(1);
    }
    char *ext = strrchr(out_filename, '.');
    if (!ext || &out_filename[len] - ext < 2) {
        fprintf(stderr, "Bad filename. Include extension.\n");
        exit(1);
    }
    ext[1] = 'c';
    ext[2] = '\0';
    return out_filename;
}

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

int main(int argc, char *argv[]) {
    init_crvic();
    const char *in_filename = get_input_file(argc, argv);
    const char *out_filename = get_output_file(in_filename);
    struct lxl_string_view source = read_source(in_filename);
    init_frontend(source, in_filename);  // This also initialises the lexer.
    struct ast_list nodes = parse();
    if (!type_check(&nodes)) {
        // Error messages already printed.
        exit(1);
    }
    static struct string_buffer sb = {0};
    enum cgen_error error = crvic_generate_c_file(nodes, &sb);
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
    printf("Codegen complete: %zu bytes total.\n", sb.count);
    if ((size_t)fprintf(f, "%s", sb.buffer) != sb.count) {
        perror("Failed to write output file");
        exit_code = EXIT_FAILURE;
    }
    if (fclose(f) == EOF) {
        fprintf(stderr, "fclose() failed!\n");
        exit_code = EXIT_FAILURE;
    }
    printf("Output saved to %s\n", out_filename);
    return exit_code;
}
