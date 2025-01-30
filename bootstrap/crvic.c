#include <stdlib.h>  // EXIT_SUCCESS, EXIT_FAILURE.
#include <stdio.h>

#include "region.h"

#include "crvic_ast.h"
#include "crvic_backend.h"
#include "crvic_frontend.h"
#include "crvic_type.h"

#define REGION_SIZE 0x4000

int main(void) {
    const char *filename = "out.c";
    const char test_prog[] =
        "func putchar(c: i32) -> i32 := external\n"
        "func main() {"
        "  var a := 42\n"
        "  var b := 5\n"
        "  a := (a + 7) * b + 1\n"
        "}\n"
        ;
    init_lexer(LXL_SV_FROM_STRLIT(test_prog));
    struct region *region = create_region(STDLIB_ALLOCATOR_AD, REGION_SIZE);
    struct ast_list nodes = parse(region);
    static struct string_buffer sb = {0};
    enum cgen_error error = crvic_generate_c_file(nodes, &sb);
    destroy_region(region);
    if (error) {
        fprintf(stderr, "Something went wrong: %d\n", error);
        return EXIT_FAILURE;
    }
    FILE *f = fopen(filename, "w");
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
