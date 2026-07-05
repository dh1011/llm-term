#ifndef LLM_TERM_H
#define LLM_TERM_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LLMT_VERSION "1.0.0"
#define LLMT_MAX_PATH 1024
#define LLMT_MAX_ERROR 512

typedef struct {
    char message[LLMT_MAX_ERROR];
} LlmtError;

typedef struct {
    char provider[16];
    char model[128];
    int max_tokens;
    double temperature;
    char base_url[256];
} LlmtConfig;

typedef struct {
    int help;
    int version;
    int setup;
    int disable_cache;
    const char *config_file;
    const char *prompt;
} LlmtOptions;

typedef struct {
    char *prompt;
    char *command;
} LlmtCacheEntry;

typedef struct {
    LlmtCacheEntry *entries;
    size_t len;
    size_t cap;
} LlmtCache;

typedef struct {
    int https;
    char host[256];
    int port;
    char path[512];
} LlmtUrl;

typedef struct {
    char command[32];
    char arg[16];
    char description[96];
    char os_name[48];
} LlmtShell;

typedef struct {
    int status;
    char *body;
} LlmtHttpResponse;

void llmt_error(LlmtError *err, const char *fmt, ...);
char *llmt_strdup(const char *s);
char *llmt_strndup(const char *s, size_t n);
char *llmt_trim_copy(const char *s);
void llmt_free(void *p);

void llmt_config_defaults(LlmtConfig *config);
int llmt_parse_args(int argc, char **argv, LlmtOptions *options, LlmtError *err);
int llmt_parse_config_text(const char *text, LlmtConfig *config, LlmtError *err);
char *llmt_config_to_json(const LlmtConfig *config);
int llmt_prompt_config(LlmtConfig *config, LlmtError *err);

void llmt_cache_init(LlmtCache *cache);
void llmt_cache_free(LlmtCache *cache);
const char *llmt_cache_get(const LlmtCache *cache, const char *prompt);
int llmt_cache_put(LlmtCache *cache, const char *prompt, const char *command, LlmtError *err);
int llmt_cache_remove(LlmtCache *cache, const char *prompt);
int llmt_parse_cache_text(const char *text, LlmtCache *cache, LlmtError *err);
char *llmt_cache_to_json(const LlmtCache *cache);

int llmt_parse_url(const char *url_text, LlmtUrl *url, LlmtError *err);
char *llmt_decode_chunked(const char *body, size_t body_len, LlmtError *err);
int llmt_parse_http_response(char *raw, LlmtHttpResponse *response, LlmtError *err);
void llmt_http_response_free(LlmtHttpResponse *response);

char *llmt_build_chat_request(const LlmtConfig *config, const char *system_prompt,
                              const char *user_prompt, LlmtError *err);
char *llmt_extract_chat_command(const char *body, LlmtError *err);
char *llmt_extract_api_error(const char *body);

int llmt_default_paths(const char *argv0, char *config_path, size_t config_path_len,
                       char *cache_path, size_t cache_path_len, LlmtError *err);
char *llmt_read_file(const char *path, LlmtError *err);
int llmt_write_file(const char *path, const char *text, LlmtError *err);

void llmt_detect_shell(LlmtShell *shell);
char *llmt_build_system_prompt(const LlmtShell *shell);

int llmt_http_post_json(const LlmtUrl *url, const char *auth_header, const char *json_body,
                        LlmtHttpResponse *response, LlmtError *err);
int llmt_execute_command(const LlmtShell *shell, const char *command, LlmtError *err);

int llmt_generate_command(const LlmtConfig *config, const char *prompt, char **command,
                          LlmtError *err);

#ifdef __cplusplus
}
#endif

#endif
