#include "llm_term.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define COLOR_RESET "\033[0m"
#define COLOR_RED "\033[31m"
#define COLOR_GREEN "\033[32m"
#define COLOR_YELLOW "\033[33m"
#define COLOR_CYAN "\033[36m"
#define COLOR_BOLD "\033[1m"

static void print_help(void) {
    printf("llm-term %s\n", LLMT_VERSION);
    printf("Generate and optionally execute terminal commands using OpenAI or Ollama.\n\n");
    printf("Usage:\n");
    printf("  llm-term [--disable-cache] [--config-file PATH] PROMPT\n");
    printf("  llm-term -c\n");
    printf("  llm-term --setup\n\n");
    printf("Options:\n");
    printf("  -c, --config, --setup       Run interactive configuration setup\n");
    printf("  --config-file PATH          Use a custom configuration path\n");
    printf("  --disable-cache             Always query the model instead of using cache\n");
    printf("  -h, --help                  Show this help\n");
    printf("  -V, --version               Show version\n");
}

static int answer_is_yes(const char *s) {
    return s && (s[0] == 'y' || s[0] == 'Y') && (s[1] == '\0' || s[1] == '\n' || s[1] == '\r');
}

static int ask_yes_no(const char *question) {
    char buf[32];
    printf("%s%s%s", COLOR_YELLOW, question, COLOR_RESET);
    printf(" ");
    fflush(stdout);
    if (!fgets(buf, sizeof(buf), stdin)) return 0;
    return answer_is_yes(buf);
}

static int save_config(const char *path, const LlmtConfig *config, LlmtError *err) {
    char *json = llmt_config_to_json(config);
    int ok;
    if (!json) {
        llmt_error(err, "failed to serialize configuration");
        return 0;
    }
    ok = llmt_write_file(path, json, err);
    free(json);
    return ok;
}

static int load_or_create_config(const char *path, LlmtConfig *config, LlmtError *err) {
    char *text = llmt_read_file(path, err);
    if (!text) {
        err->message[0] = '\0';
        if (!llmt_prompt_config(config, err)) return 0;
        if (!save_config(path, config, err)) return 0;
        printf("%sConfiguration saved successfully.%s\n", COLOR_GREEN, COLOR_RESET);
        return 1;
    }
    if (!llmt_parse_config_text(text, config, err)) {
        free(text);
        return 0;
    }
    free(text);
    return 1;
}

static int load_cache_file(const char *path, LlmtCache *cache, LlmtError *err) {
    char *text = llmt_read_file(path, err);
    if (!text) {
        err->message[0] = '\0';
        llmt_cache_init(cache);
        return 1;
    }
    if (!llmt_parse_cache_text(text, cache, err)) {
        free(text);
        return 0;
    }
    free(text);
    return 1;
}

static int save_cache_file(const char *path, const LlmtCache *cache, LlmtError *err) {
    char *json = llmt_cache_to_json(cache);
    int ok;
    if (!json) {
        llmt_error(err, "failed to serialize cache");
        return 0;
    }
    ok = llmt_write_file(path, json, err);
    free(json);
    return ok;
}

static int maybe_execute(const char *command, LlmtError *err) {
    LlmtShell shell;
    printf("%s%s%s%s\n", COLOR_CYAN, COLOR_BOLD, command, COLOR_RESET);
    if (!ask_yes_no("Do you want to execute this command? (y/n)")) {
        printf("%sCommand execution cancelled.%s\n", COLOR_YELLOW, COLOR_RESET);
        return 1;
    }
    printf("%s%sCommand output:%s\n", COLOR_GREEN, COLOR_BOLD, COLOR_RESET);
    llmt_detect_shell(&shell);
    if (!llmt_execute_command(&shell, command, err)) {
        fprintf(stderr, "%sError: %s%s\n", COLOR_RED, err->message, COLOR_RESET);
        err->message[0] = '\0';
    }
    return 1;
}

static int query_and_maybe_cache(const LlmtConfig *config, LlmtCache *cache, const char *cache_path,
                                 const char *prompt, int write_cache, LlmtError *err) {
    char *command = NULL;
    int ok;
    if (!llmt_generate_command(config, prompt, &command, err)) return 0;
    if (!command || command[0] == '\0') {
        free(command);
        printf("%sNo command could be generated.%s\n", COLOR_YELLOW, COLOR_RESET);
        return 1;
    }
    ok = maybe_execute(command, err);
    if (write_cache) {
        if (!llmt_cache_put(cache, prompt, command, err) || !save_cache_file(cache_path, cache, err)) {
            free(command);
            return 0;
        }
    }
    free(command);
    return ok;
}

int main(int argc, char **argv) {
    LlmtOptions options;
    LlmtConfig config;
    LlmtCache cache;
    LlmtError err;
    char default_config_path[LLMT_MAX_PATH];
    char cache_path[LLMT_MAX_PATH];
    const char *config_path;
    const char *cached_command;
    int exit_code = 0;

    err.message[0] = '\0';
    if (!llmt_parse_args(argc, argv, &options, &err)) {
        fprintf(stderr, "%sError: %s%s\n", COLOR_RED, err.message, COLOR_RESET);
        return 2;
    }
    if (options.help) {
        print_help();
        return 0;
    }
    if (options.version) {
        printf("llm-term %s\n", LLMT_VERSION);
        return 0;
    }

    if (!llmt_default_paths(argv[0], default_config_path, sizeof(default_config_path),
                            cache_path, sizeof(cache_path), &err)) {
        fprintf(stderr, "%sError: %s%s\n", COLOR_RED, err.message, COLOR_RESET);
        return 1;
    }
    config_path = options.config_file ? options.config_file : default_config_path;

    if (options.setup) {
        if (!llmt_prompt_config(&config, &err) || !save_config(config_path, &config, &err)) {
            fprintf(stderr, "%sError: %s%s\n", COLOR_RED, err.message, COLOR_RESET);
            return 1;
        }
        printf("%sConfiguration saved successfully.%s\n", COLOR_GREEN, COLOR_RESET);
        return 0;
    }

    if (!load_or_create_config(config_path, &config, &err)) {
        fprintf(stderr, "%sError: %s%s\n", COLOR_RED, err.message, COLOR_RESET);
        return 1;
    }

    if (!options.prompt) {
        printf("%sPlease provide a prompt or use --config to set up the configuration.%s\n",
               COLOR_YELLOW, COLOR_RESET);
        return 0;
    }

    llmt_cache_init(&cache);
    if (!options.disable_cache && !load_cache_file(cache_path, &cache, &err)) {
        fprintf(stderr, "%sError: %s%s\n", COLOR_RED, err.message, COLOR_RESET);
        return 1;
    }

    cached_command = options.disable_cache ? NULL : llmt_cache_get(&cache, options.prompt);
    if (cached_command) {
        printf("%sThis command exists in cache%s\n", COLOR_YELLOW, COLOR_RESET);
        printf("%s%s%s%s\n", COLOR_CYAN, COLOR_BOLD, cached_command, COLOR_RESET);
        if (ask_yes_no("Do you want to execute this command? (y/n)")) {
            LlmtShell shell;
            printf("%s%sCommand output:%s\n", COLOR_GREEN, COLOR_BOLD, COLOR_RESET);
            llmt_detect_shell(&shell);
            if (!llmt_execute_command(&shell, cached_command, &err)) {
                fprintf(stderr, "%sError: %s%s\n", COLOR_RED, err.message, COLOR_RESET);
                exit_code = 1;
            }
        } else if (ask_yes_no("Do you want to invalidate the cache? (y/n)")) {
            llmt_cache_remove(&cache, options.prompt);
            if (!save_cache_file(cache_path, &cache, &err) ||
                !query_and_maybe_cache(&config, &cache, cache_path, options.prompt, 1, &err)) {
                fprintf(stderr, "%sError: %s%s\n", COLOR_RED, err.message, COLOR_RESET);
                exit_code = 1;
            }
        } else {
            printf("%sCommand execution cancelled.%s\n", COLOR_YELLOW, COLOR_RESET);
        }
    } else {
        if (!query_and_maybe_cache(&config, &cache, cache_path, options.prompt,
                                   !options.disable_cache, &err)) {
            fprintf(stderr, "%sError: %s%s\n", COLOR_RED, err.message, COLOR_RESET);
            exit_code = 1;
        }
    }

    llmt_cache_free(&cache);
    return exit_code;
}
