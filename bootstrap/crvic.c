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

#define DEFAULT_OUT_FILENAME "out.c"

struct cmdargs {
    // Program name (argv[0]).
    const char *prog_name;
    // Positional arguments.
    struct filelist {
        int count, capacity;
        const char **items;
        struct allocatorARD allocator;
    } in_filenames;
    // Options.
    const char *out_filename;
    // Flags.
    bool positional_only;
};

void print_usage(FILE *fp, const char *prog_name) {
    fprintf(fp, "Usage: %s [-h] file ...\n", prog_name);
}

void print_help(FILE *fp, const char *prog_name) {
    print_usage(fp, prog_name);
    fprintf(fp,
            "Positional arguments\n"
            "  file          a Victoria file to compile\n"
            "Options\n"
            "  -h, --help    show this help message and exit\n"
            "  -o file       save the output to the specified file\n"
            "  --            interpret all following arguments as positional\n"
        );
}

int handle_option(struct cmdargs *args, char *argv[], int i) {
    const char *arg = argv[i];
    if ((strcmp(arg, "-h") == 0 || strcmp(arg, "--help") == 0)) {
        print_help(stdout, args->prog_name);
        exit(0);
    }
    else if (strcmp(arg, "-o") == 0) {
        if (args->out_filename) {
            fprintf(stderr,
                    "WARNING: output file already specified. "
                    "Only the file spcified last will be used.\n");
        }
        args->out_filename = argv[++i];
    }
    else if (strcmp(arg, "--") == 0) {
        args->positional_only = true;
    }
    else {
        fprintf(stderr, "Unknown command line option '%s'.\n", arg);
        print_usage(stderr, args->prog_name);
        exit(1);
    }
    return i;
}

struct cmdargs parse_args(int argc, char *argv[]) {
    struct cmdargs args = {.prog_name = argv[0], .in_filenames.allocator = perm};
    for (int i = 1; i < argc; ++i) {
        const char *arg = argv[i];
        if (!args.positional_only && arg[0] == '-') {
            i = handle_option(&args, argv, i);
        }
        else {
            DA_APPEND(&args.in_filenames, arg);
        }
    }
    if (!args.out_filename) args.out_filename = DEFAULT_OUT_FILENAME;
    return args;
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
    ptrdiff_t out_count = &ext[2] - out_filename;
    assert(out_count > 0);
    char *buffer = ALLOCATE(perm, out_count);
    return memcpy(buffer, out_filename, out_count);
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
    char *new_buffer = ALLOCATE(perm, length + 1);
    memcpy(new_buffer, input_buffer, length + 1);
    return (struct lxl_string_view) {
        .start = new_buffer,
        .length = length,
    };
}

int main(int argc, char *argv[]) {
    init_crvic();
    struct cmdargs args = parse_args(argc, argv);
    int parse_fail_count = 0;
    for (int i  = 0; i < args.in_filenames.count; ++i) {
        const char *in_filename = args.in_filenames.items[i];
        struct lxl_string_view source = read_source(in_filename);
        // NOTE: we re-initialise the frontend for each file module.
        init_frontend(source, in_filename);  // This also initialises the lexer.
        if (!parse()) {
            // This doesn't /need/ to be an if, but imo it's easier to read like this.
            parse_fail_count += 1;
        }
    }
    if (parse_fail_count > 0) {
        exit(1);
    }
    if (!type_check()) {
        // Error messages already printed.
        exit(1);
    }
    static struct string_buffer sb = {0};
    enum cgen_error error = crvic_generate_c_file(get_package(), &sb);
    if (error) {
        fprintf(stderr, "Something went wrong: %d\n", error);
        return EXIT_FAILURE;
    }
    FILE *f = fopen(args.out_filename, "w");
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
    printf("Output saved to %s\n", args.out_filename);
    return exit_code;
}
