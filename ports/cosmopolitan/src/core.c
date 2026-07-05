#define _POSIX_C_SOURCE 200809L

#include "llm_term.h"

#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <windows.h>
#else
#include <limits.h>
#include <unistd.h>
#endif

#if defined(__APPLE__)
#include <mach-o/dyld.h>
#endif

#if defined(__COSMOPOLITAN__)
#include "libc/dce.h"
#endif

#include "yyjson.h"

extern const unsigned char llm_term_ca_bundle[];
extern const size_t llm_term_ca_bundle_len;

static int streq(const char *a, const char *b) {
    return strcmp(a, b) == 0;
}

static int streq_ci(const char *a, const char *b) {
    while (*a && *b) {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) return 0;
        a++;
        b++;
    }
    return *a == '\0' && *b == '\0';
}

static int starts_with_ci(const char *s, const char *prefix) {
    while (*prefix) {
        if (*s == '\0') return 0;
        if (tolower((unsigned char)*s) != tolower((unsigned char)*prefix)) return 0;
        s++;
        prefix++;
    }
    return 1;
}

static void copy_str(char *dst, size_t dst_len, const char *src) {
    if (dst_len == 0) return;
    snprintf(dst, dst_len, "%s", src ? src : "");
}

static int is_openai_provider(const LlmtConfig *config) {
    return streq_ci(config->provider, "openai");
}

static int running_windows(void) {
#if defined(__COSMOPOLITAN__)
    return IsWindows();
#elif defined(_WIN32)
    return 1;
#else
    return 0;
#endif
}

static const char *runtime_os_name(void) {
#if defined(__COSMOPOLITAN__)
    if (IsWindows()) return "windows";
    if (IsXnu()) return "macos";
    if (IsLinux()) return "linux";
    if (IsFreebsd()) return "freebsd";
    if (IsOpenbsd()) return "openbsd";
    if (IsNetbsd()) return "netbsd";
    return "unknown";
#elif defined(_WIN32)
    return "windows";
#elif defined(__APPLE__)
    return "macos";
#elif defined(__linux__)
    return "linux";
#elif defined(__FreeBSD__)
    return "freebsd";
#elif defined(__OpenBSD__)
    return "openbsd";
#elif defined(__NetBSD__)
    return "netbsd";
#else
    return "unknown";
#endif
}

void llmt_error(LlmtError *err, const char *fmt, ...) {
    va_list ap;
    if (!err) return;
    va_start(ap, fmt);
    vsnprintf(err->message, sizeof(err->message), fmt, ap);
    va_end(ap);
}

char *llmt_strdup(const char *s) {
    size_t len;
    char *out;
    if (!s) return NULL;
    len = strlen(s);
    out = (char *)malloc(len + 1);
    if (!out) return NULL;
    memcpy(out, s, len + 1);
    return out;
}

char *llmt_strndup(const char *s, size_t n) {
    char *out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n);
    out[n] = '\0';
    return out;
}

char *llmt_trim_copy(const char *s) {
    const char *start = s;
    const char *end;
    if (!s) return llmt_strdup("");
    while (*start && isspace((unsigned char)*start)) start++;
    end = start + strlen(start);
    while (end > start && isspace((unsigned char)end[-1])) end--;
    return llmt_strndup(start, (size_t)(end - start));
}

void llmt_free(void *p) {
    free(p);
}

void llmt_config_defaults(LlmtConfig *config) {
    memset(config, 0, sizeof(*config));
    copy_str(config->provider, sizeof(config->provider), "openai");
    copy_str(config->model, sizeof(config->model), "gpt-5.4-mini");
    config->max_tokens = 1024;
    config->temperature = 0.5;
    copy_str(config->base_url, sizeof(config->base_url), "https://api.openai.com/v1");
}

int llmt_parse_args(int argc, char **argv, LlmtOptions *options, LlmtError *err) {
    int i;
    memset(options, 0, sizeof(*options));
    for (i = 1; i < argc; i++) {
        const char *arg = argv[i];
        if (streq(arg, "--help") || streq(arg, "-h")) {
            options->help = 1;
        } else if (streq(arg, "--version") || streq(arg, "-V")) {
            options->version = 1;
        } else if (streq(arg, "--setup") || streq(arg, "--config") || streq(arg, "-c")) {
            options->setup = 1;
        } else if (streq(arg, "--disable-cache")) {
            options->disable_cache = 1;
        } else if (streq(arg, "--config-file")) {
            if (i + 1 >= argc) {
                llmt_error(err, "--config-file requires a path");
                return 0;
            }
            options->config_file = argv[++i];
        } else if (starts_with_ci(arg, "--config-file=")) {
            options->config_file = arg + strlen("--config-file=");
            if (options->config_file[0] == '\0') {
                llmt_error(err, "--config-file requires a path");
                return 0;
            }
        } else if (arg[0] == '-') {
            llmt_error(err, "unknown option: %s", arg);
            return 0;
        } else if (!options->prompt) {
            options->prompt = arg;
        } else {
            llmt_error(err, "only one prompt argument is supported; quote prompts that contain spaces");
            return 0;
        }
    }
    return 1;
}

static int yy_get_string(yyjson_val *obj, const char *key, char *dst, size_t dst_len) {
    yyjson_val *v = yyjson_obj_get(obj, key);
    if (!yyjson_is_str(v)) return 0;
    copy_str(dst, dst_len, yyjson_get_str(v));
    return 1;
}

static int yy_get_int(yyjson_val *obj, const char *key, int *out) {
    yyjson_val *v = yyjson_obj_get(obj, key);
    if (!yyjson_is_num(v)) return 0;
    *out = (int)yyjson_get_sint(v);
    return 1;
}

static int yy_get_real(yyjson_val *obj, const char *key, double *out) {
    yyjson_val *v = yyjson_obj_get(obj, key);
    if (!yyjson_is_num(v)) return 0;
    *out = yyjson_get_real(v);
    return 1;
}

static void normalize_config(LlmtConfig *config) {
    if (!streq_ci(config->provider, "ollama")) {
        copy_str(config->provider, sizeof(config->provider), "openai");
    }
    if (config->model[0] == '\0') {
        copy_str(config->model, sizeof(config->model),
                 streq_ci(config->provider, "ollama") ? "llama3.1" : "gpt-5.4-mini");
    }
    if (config->max_tokens <= 0 || config->max_tokens > 4096) config->max_tokens = 1024;
    if (config->temperature < 0.0 || config->temperature > 2.0) config->temperature = 0.5;
    if (config->base_url[0] == '\0') {
        copy_str(config->base_url, sizeof(config->base_url),
                 streq_ci(config->provider, "ollama") ? "http://localhost:11434/v1"
                                                       : "https://api.openai.com/v1");
    }
}

int llmt_parse_config_text(const char *text, LlmtConfig *config, LlmtError *err) {
    yyjson_doc *doc;
    yyjson_val *root;
    yyjson_val *model;
    if (!text || text[0] == '\0') {
        llmt_error(err, "configuration is empty");
        return 0;
    }
    llmt_config_defaults(config);
    doc = yyjson_read(text, strlen(text), 0);
    if (!doc) {
        llmt_error(err, "configuration is not valid JSON");
        return 0;
    }
    root = yyjson_doc_get_root(doc);
    if (!yyjson_is_obj(root)) {
        yyjson_doc_free(doc);
        llmt_error(err, "configuration root must be a JSON object");
        return 0;
    }

    yy_get_string(root, "provider", config->provider, sizeof(config->provider));
    yy_get_string(root, "base_url", config->base_url, sizeof(config->base_url));
    yy_get_int(root, "max_tokens", &config->max_tokens);
    yy_get_real(root, "temperature", &config->temperature);

    model = yyjson_obj_get(root, "model");
    if (yyjson_is_str(model)) {
        copy_str(config->model, sizeof(config->model), yyjson_get_str(model));
        if (streq_ci(config->model, "ollama")) {
            copy_str(config->provider, sizeof(config->provider), "ollama");
            copy_str(config->model, sizeof(config->model), "llama3.1");
        }
    } else if (yyjson_is_obj(model)) {
        yyjson_val *ollama = yyjson_obj_get(model, "ollama");
        if (yyjson_is_str(ollama)) {
            copy_str(config->provider, sizeof(config->provider), "ollama");
            copy_str(config->model, sizeof(config->model), yyjson_get_str(ollama));
            copy_str(config->base_url, sizeof(config->base_url), "http://localhost:11434/v1");
        }
    }

    normalize_config(config);
    yyjson_doc_free(doc);
    return 1;
}

char *llmt_config_to_json(const LlmtConfig *config) {
    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *root = yyjson_mut_obj(doc);
    char *json;
    yyjson_mut_doc_set_root(doc, root);
    yyjson_mut_obj_add_strcpy(doc, root, "provider", config->provider);
    yyjson_mut_obj_add_strcpy(doc, root, "model", config->model);
    yyjson_mut_obj_add_int(doc, root, "max_tokens", config->max_tokens);
    yyjson_mut_obj_add_real(doc, root, "temperature", config->temperature);
    yyjson_mut_obj_add_strcpy(doc, root, "base_url", config->base_url);
    json = yyjson_mut_write(doc, YYJSON_WRITE_PRETTY | YYJSON_WRITE_NEWLINE_AT_END, NULL);
    yyjson_mut_doc_free(doc);
    return json;
}

static int read_line_trim(char *buf, size_t len) {
    size_t n;
    if (!fgets(buf, (int)len, stdin)) return 0;
    n = strlen(buf);
    while (n > 0 && (buf[n - 1] == '\n' || buf[n - 1] == '\r')) buf[--n] = '\0';
    return 1;
}

int llmt_prompt_config(LlmtConfig *config, LlmtError *err) {
    char input[256];
    llmt_config_defaults(config);
    for (;;) {
        printf("\033[36mSelect model:\n 1 for gpt-5.4-mini\n 2 for gpt-5.4\n 3 for ollama (llama3.1)\033[0m\n");
        fflush(stdout);
        if (!read_line_trim(input, sizeof(input))) {
            llmt_error(err, "failed to read model choice");
            return 0;
        }
        if (streq(input, "1")) {
            copy_str(config->provider, sizeof(config->provider), "openai");
            copy_str(config->model, sizeof(config->model), "gpt-5.4-mini");
            copy_str(config->base_url, sizeof(config->base_url), "https://api.openai.com/v1");
            break;
        }
        if (streq(input, "2")) {
            copy_str(config->provider, sizeof(config->provider), "openai");
            copy_str(config->model, sizeof(config->model), "gpt-5.4");
            copy_str(config->base_url, sizeof(config->base_url), "https://api.openai.com/v1");
            break;
        }
        if (streq(input, "3")) {
            copy_str(config->provider, sizeof(config->provider), "ollama");
            copy_str(config->model, sizeof(config->model), "llama3.1");
            copy_str(config->base_url, sizeof(config->base_url), "http://localhost:11434/v1");
            break;
        }
        printf("\033[31mInvalid choice. Please try again.\033[0m\n");
    }

    for (;;) {
        int tokens;
        printf("\033[36mEnter max tokens (1-4096): \033[0m");
        fflush(stdout);
        if (!read_line_trim(input, sizeof(input))) {
            llmt_error(err, "failed to read max tokens");
            return 0;
        }
        tokens = atoi(input);
        if (tokens > 0 && tokens <= 4096) {
            config->max_tokens = tokens;
            break;
        }
        printf("\033[31mInvalid input. Please enter a number between 1 and 4096.\033[0m\n");
    }
    return 1;
}

void llmt_cache_init(LlmtCache *cache) {
    memset(cache, 0, sizeof(*cache));
}

void llmt_cache_free(LlmtCache *cache) {
    size_t i;
    for (i = 0; i < cache->len; i++) {
        free(cache->entries[i].prompt);
        free(cache->entries[i].command);
    }
    free(cache->entries);
    llmt_cache_init(cache);
}

const char *llmt_cache_get(const LlmtCache *cache, const char *prompt) {
    size_t i;
    for (i = 0; i < cache->len; i++) {
        if (streq(cache->entries[i].prompt, prompt)) return cache->entries[i].command;
    }
    return NULL;
}

int llmt_cache_put(LlmtCache *cache, const char *prompt, const char *command, LlmtError *err) {
    size_t i;
    LlmtCacheEntry *entries;
    for (i = 0; i < cache->len; i++) {
        if (streq(cache->entries[i].prompt, prompt)) {
            char *copy = llmt_strdup(command);
            if (!copy) {
                llmt_error(err, "out of memory while updating cache");
                return 0;
            }
            free(cache->entries[i].command);
            cache->entries[i].command = copy;
            return 1;
        }
    }
    if (cache->len == cache->cap) {
        size_t next = cache->cap ? cache->cap * 2 : 8;
        entries = (LlmtCacheEntry *)realloc(cache->entries, next * sizeof(*entries));
        if (!entries) {
            llmt_error(err, "out of memory while growing cache");
            return 0;
        }
        cache->entries = entries;
        cache->cap = next;
    }
    cache->entries[cache->len].prompt = llmt_strdup(prompt);
    cache->entries[cache->len].command = llmt_strdup(command);
    if (!cache->entries[cache->len].prompt || !cache->entries[cache->len].command) {
        free(cache->entries[cache->len].prompt);
        free(cache->entries[cache->len].command);
        llmt_error(err, "out of memory while adding cache entry");
        return 0;
    }
    cache->len++;
    return 1;
}

int llmt_cache_remove(LlmtCache *cache, const char *prompt) {
    size_t i;
    for (i = 0; i < cache->len; i++) {
        if (streq(cache->entries[i].prompt, prompt)) {
            free(cache->entries[i].prompt);
            free(cache->entries[i].command);
            if (i + 1 < cache->len) {
                memmove(&cache->entries[i], &cache->entries[i + 1],
                        (cache->len - i - 1) * sizeof(cache->entries[i]));
            }
            cache->len--;
            return 1;
        }
    }
    return 0;
}

int llmt_parse_cache_text(const char *text, LlmtCache *cache, LlmtError *err) {
    yyjson_doc *doc;
    yyjson_val *root;
    yyjson_val *key;
    yyjson_val *val;
    yyjson_obj_iter iter;
    llmt_cache_init(cache);
    if (!text || text[0] == '\0') return 1;
    doc = yyjson_read(text, strlen(text), 0);
    if (!doc) {
        llmt_error(err, "cache is not valid JSON");
        return 0;
    }
    root = yyjson_doc_get_root(doc);
    if (!yyjson_is_obj(root)) {
        yyjson_doc_free(doc);
        llmt_error(err, "cache root must be a JSON object");
        return 0;
    }
    yyjson_obj_iter_init(root, &iter);
    while ((key = yyjson_obj_iter_next(&iter)) != NULL) {
        val = yyjson_obj_iter_get_val(key);
        if (yyjson_is_str(key) && yyjson_is_str(val)) {
            if (!llmt_cache_put(cache, yyjson_get_str(key), yyjson_get_str(val), err)) {
                yyjson_doc_free(doc);
                return 0;
            }
        }
    }
    yyjson_doc_free(doc);
    return 1;
}

char *llmt_cache_to_json(const LlmtCache *cache) {
    size_t i;
    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *root = yyjson_mut_obj(doc);
    char *json;
    yyjson_mut_doc_set_root(doc, root);
    for (i = 0; i < cache->len; i++) {
        yyjson_mut_obj_add_strcpy(doc, root, cache->entries[i].prompt, cache->entries[i].command);
    }
    json = yyjson_mut_write(doc, YYJSON_WRITE_PRETTY | YYJSON_WRITE_NEWLINE_AT_END, NULL);
    yyjson_mut_doc_free(doc);
    return json;
}

int llmt_parse_url(const char *url_text, LlmtUrl *url, LlmtError *err) {
    const char *p;
    const char *host_start;
    const char *host_end;
    const char *path_start;
    const char *colon;
    size_t host_len;
    memset(url, 0, sizeof(*url));
    if (starts_with_ci(url_text, "https://")) {
        url->https = 1;
        url->port = 443;
        host_start = url_text + 8;
    } else if (starts_with_ci(url_text, "http://")) {
        url->https = 0;
        url->port = 80;
        host_start = url_text + 7;
    } else {
        llmt_error(err, "URL must start with http:// or https://");
        return 0;
    }
    path_start = strchr(host_start, '/');
    if (!path_start) path_start = host_start + strlen(host_start);
    host_end = path_start;
    colon = NULL;
    for (p = host_start; p < host_end; p++) {
        if (*p == ':') colon = p;
    }
    if (colon) {
        host_len = (size_t)(colon - host_start);
        url->port = atoi(colon + 1);
        if (url->port <= 0 || url->port > 65535) {
            llmt_error(err, "URL port is invalid");
            return 0;
        }
    } else {
        host_len = (size_t)(host_end - host_start);
    }
    if (host_len == 0 || host_len >= sizeof(url->host)) {
        llmt_error(err, "URL host is invalid");
        return 0;
    }
    memcpy(url->host, host_start, host_len);
    url->host[host_len] = '\0';
    if (*path_start) {
        copy_str(url->path, sizeof(url->path), path_start);
    } else {
        copy_str(url->path, sizeof(url->path), "/");
    }
    return 1;
}

static char *find_header_end(char *raw, size_t *header_len) {
    char *p = strstr(raw, "\r\n\r\n");
    if (p) {
        *header_len = (size_t)(p - raw + 4);
        return p + 4;
    }
    p = strstr(raw, "\n\n");
    if (p) {
        *header_len = (size_t)(p - raw + 2);
        return p + 2;
    }
    return NULL;
}

char *llmt_decode_chunked(const char *body, size_t body_len, LlmtError *err) {
    size_t pos = 0;
    size_t out_len = 0;
    size_t out_cap = body_len + 1;
    char *out = (char *)malloc(out_cap);
    if (!out) {
        llmt_error(err, "out of memory while decoding chunked response");
        return NULL;
    }
    while (pos < body_len) {
        size_t line_start = pos;
        size_t chunk_size;
        char size_buf[32];
        size_t size_len;
        char *endptr;
        while (pos < body_len && body[pos] != '\n') pos++;
        if (pos >= body_len) break;
        size_len = pos - line_start;
        if (size_len > 0 && body[line_start + size_len - 1] == '\r') size_len--;
        if (size_len >= sizeof(size_buf)) {
            free(out);
            llmt_error(err, "chunk size line is too long");
            return NULL;
        }
        memcpy(size_buf, body + line_start, size_len);
        size_buf[size_len] = '\0';
        endptr = strchr(size_buf, ';');
        if (endptr) *endptr = '\0';
        chunk_size = (size_t)strtoul(size_buf, &endptr, 16);
        if (endptr == size_buf) {
            free(out);
            llmt_error(err, "invalid chunk size");
            return NULL;
        }
        pos++;
        if (chunk_size == 0) break;
        if (pos + chunk_size > body_len) {
            free(out);
            llmt_error(err, "chunk extends beyond response body");
            return NULL;
        }
        if (out_len + chunk_size + 1 > out_cap) {
            char *next;
            out_cap = out_len + chunk_size + 1;
            next = (char *)realloc(out, out_cap);
            if (!next) {
                free(out);
                llmt_error(err, "out of memory while expanding decoded response");
                return NULL;
            }
            out = next;
        }
        memcpy(out + out_len, body + pos, chunk_size);
        out_len += chunk_size;
        pos += chunk_size;
        if (pos < body_len && body[pos] == '\r') pos++;
        if (pos < body_len && body[pos] == '\n') pos++;
    }
    out[out_len] = '\0';
    return out;
}

int llmt_parse_http_response(char *raw, LlmtHttpResponse *response, LlmtError *err) {
    char *body;
    char *line;
    size_t header_len = 0;
    int chunked = 0;
    memset(response, 0, sizeof(*response));
    body = find_header_end(raw, &header_len);
    if (!body) {
        llmt_error(err, "HTTP response did not include headers");
        return 0;
    }
    raw[header_len - ((raw[header_len - 1] == '\n' && raw[header_len - 2] == '\n') ? 2 : 4)] = '\0';
    if (sscanf(raw, "HTTP/%*s %d", &response->status) != 1) {
        llmt_error(err, "HTTP status line is invalid");
        return 0;
    }
    line = raw;
    while ((line = strchr(line, '\n')) != NULL) {
        line++;
        while (*line == '\r') line++;
        if (starts_with_ci(line, "transfer-encoding:") && strstr(line, "chunked")) {
            chunked = 1;
        }
    }
    if (chunked) {
        response->body = llmt_decode_chunked(body, strlen(body), err);
    } else {
        response->body = llmt_strdup(body);
    }
    if (!response->body) return 0;
    return 1;
}

void llmt_http_response_free(LlmtHttpResponse *response) {
    if (!response) return;
    free(response->body);
    response->body = NULL;
    response->status = 0;
}

char *llmt_build_chat_request(const LlmtConfig *config, const char *system_prompt,
                              const char *user_prompt, LlmtError *err) {
    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *root = yyjson_mut_obj(doc);
    yyjson_mut_val *messages = yyjson_mut_arr(doc);
    yyjson_mut_val *system = yyjson_mut_obj(doc);
    yyjson_mut_val *user = yyjson_mut_obj(doc);
    char *json;
    if (!doc || !root || !messages || !system || !user) {
        llmt_error(err, "out of memory while creating request JSON");
        yyjson_mut_doc_free(doc);
        return NULL;
    }
    yyjson_mut_doc_set_root(doc, root);
    yyjson_mut_obj_add_strcpy(doc, root, "model", config->model);
    if (is_openai_provider(config)) {
        yyjson_mut_obj_add_int(doc, root, "max_completion_tokens", config->max_tokens);
    } else {
        yyjson_mut_obj_add_int(doc, root, "max_tokens", config->max_tokens);
    }
    yyjson_mut_obj_add_real(doc, root, "temperature", config->temperature);
    yyjson_mut_obj_add_bool(doc, root, "stream", 0);
    yyjson_mut_obj_add_val(doc, root, "messages", messages);

    yyjson_mut_obj_add_strcpy(doc, system, "role", "system");
    yyjson_mut_obj_add_strcpy(doc, system, "content", system_prompt);
    yyjson_mut_arr_add_val(messages, system);

    yyjson_mut_obj_add_strcpy(doc, user, "role", "user");
    yyjson_mut_obj_add_strcpy(doc, user, "content", user_prompt);
    yyjson_mut_arr_add_val(messages, user);

    json = yyjson_mut_write(doc, 0, NULL);
    yyjson_mut_doc_free(doc);
    return json;
}

char *llmt_extract_api_error(const char *body) {
    yyjson_doc *doc = yyjson_read(body, strlen(body), 0);
    yyjson_val *root;
    yyjson_val *error;
    yyjson_val *message;
    char *out = NULL;
    if (!doc) return NULL;
    root = yyjson_doc_get_root(doc);
    error = yyjson_obj_get(root, "error");
    message = yyjson_is_obj(error) ? yyjson_obj_get(error, "message") : NULL;
    if (yyjson_is_str(message)) out = llmt_strdup(yyjson_get_str(message));
    yyjson_doc_free(doc);
    return out;
}

char *llmt_extract_chat_command(const char *body, LlmtError *err) {
    yyjson_doc *doc = yyjson_read(body, strlen(body), 0);
    yyjson_val *root;
    yyjson_val *choices;
    yyjson_val *first;
    yyjson_val *message;
    yyjson_val *content;
    char *trimmed;
    if (!doc) {
        llmt_error(err, "API response is not valid JSON");
        return NULL;
    }
    root = yyjson_doc_get_root(doc);
    choices = yyjson_obj_get(root, "choices");
    first = yyjson_arr_get(choices, 0);
    message = yyjson_obj_get(first, "message");
    content = yyjson_obj_get(message, "content");
    if (!yyjson_is_str(content)) {
        char *api_error = llmt_extract_api_error(body);
        yyjson_doc_free(doc);
        if (api_error) {
            llmt_error(err, "%s", api_error);
            free(api_error);
        } else {
            llmt_error(err, "API response did not include choices[0].message.content");
        }
        return NULL;
    }
    trimmed = llmt_trim_copy(yyjson_get_str(content));
    yyjson_doc_free(doc);
    if (!trimmed) {
        llmt_error(err, "out of memory while reading command");
        return NULL;
    }
    return trimmed;
}

static int dirname_of(const char *path, char *dir, size_t dir_len) {
    const char *slash = strrchr(path, '/');
    const char *backslash = strrchr(path, '\\');
    const char *last = slash > backslash ? slash : backslash;
    if (!last) {
        copy_str(dir, dir_len, ".");
        return 1;
    }
    if ((size_t)(last - path) >= dir_len) return 0;
    memcpy(dir, path, (size_t)(last - path));
    dir[last - path] = '\0';
    return 1;
}

static int executable_path(const char *argv0, char *path, size_t path_len) {
#if defined(_WIN32)
    DWORD n = GetModuleFileNameA(NULL, path, (DWORD)path_len);
    if (n > 0 && n < path_len) return 1;
#endif
#if !defined(_WIN32)
    ssize_t n = readlink("/proc/self/exe", path, path_len - 1);
    if (n > 0 && (size_t)n < path_len) {
        path[n] = '\0';
        return 1;
    }
#if defined(__APPLE__)
    {
        uint32_t size = (uint32_t)path_len;
        if (_NSGetExecutablePath(path, &size) == 0) return 1;
    }
#endif
#endif
    if (argv0 && argv0[0]) {
        copy_str(path, path_len, argv0);
        return 1;
    }
    return 0;
}

int llmt_default_paths(const char *argv0, char *config_path, size_t config_path_len,
                       char *cache_path, size_t cache_path_len, LlmtError *err) {
    char exe[LLMT_MAX_PATH];
    char dir[LLMT_MAX_PATH];
    if (!executable_path(argv0, exe, sizeof(exe)) || !dirname_of(exe, dir, sizeof(dir))) {
        llmt_error(err, "failed to determine executable directory");
        return 0;
    }
    snprintf(config_path, config_path_len, "%s%cconfig.json", dir, running_windows() ? '\\' : '/');
    snprintf(cache_path, cache_path_len, "%s%ccache.json", dir, running_windows() ? '\\' : '/');
    return 1;
}

char *llmt_read_file(const char *path, LlmtError *err) {
    FILE *f = fopen(path, "rb");
    long len;
    char *buf;
    size_t got;
    if (!f) {
        llmt_error(err, "failed to open %s: %s", path, strerror(errno));
        return NULL;
    }
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        llmt_error(err, "failed to seek %s", path);
        return NULL;
    }
    len = ftell(f);
    if (len < 0) {
        fclose(f);
        llmt_error(err, "failed to measure %s", path);
        return NULL;
    }
    rewind(f);
    buf = (char *)malloc((size_t)len + 1);
    if (!buf) {
        fclose(f);
        llmt_error(err, "out of memory while reading %s", path);
        return NULL;
    }
    got = fread(buf, 1, (size_t)len, f);
    fclose(f);
    if (got != (size_t)len) {
        free(buf);
        llmt_error(err, "failed to read %s", path);
        return NULL;
    }
    buf[len] = '\0';
    return buf;
}

int llmt_write_file(const char *path, const char *text, LlmtError *err) {
    FILE *f = fopen(path, "wb");
    size_t len = strlen(text);
    if (!f) {
        llmt_error(err, "failed to open %s for writing: %s", path, strerror(errno));
        return 0;
    }
    if (fwrite(text, 1, len, f) != len) {
        fclose(f);
        llmt_error(err, "failed to write %s", path);
        return 0;
    }
    fclose(f);
    return 1;
}

void llmt_detect_shell(LlmtShell *shell) {
    const char *env_shell;
    memset(shell, 0, sizeof(*shell));
    copy_str(shell->os_name, sizeof(shell->os_name), runtime_os_name());
    if (running_windows()) {
        copy_str(shell->command, sizeof(shell->command), "powershell");
        copy_str(shell->arg, sizeof(shell->arg), "-Command");
        copy_str(shell->description, sizeof(shell->description), "Windows PowerShell");
        return;
    }
    env_shell = getenv("SHELL");
    if (!env_shell) env_shell = "sh";
    if (strstr(env_shell, "zsh")) {
        copy_str(shell->command, sizeof(shell->command), "zsh");
        copy_str(shell->description, sizeof(shell->description), "Z Shell (zsh)");
    } else if (strstr(env_shell, "fish")) {
        copy_str(shell->command, sizeof(shell->command), "fish");
        copy_str(shell->description, sizeof(shell->description), "Friendly Interactive Shell (fish)");
    } else if (strstr(env_shell, "dash")) {
        copy_str(shell->command, sizeof(shell->command), "dash");
        copy_str(shell->description, sizeof(shell->description), "Debian Almquist Shell (dash)");
    } else if (strstr(env_shell, "ksh")) {
        copy_str(shell->command, sizeof(shell->command), "ksh");
        copy_str(shell->description, sizeof(shell->description), "Korn Shell (ksh)");
    } else if (strstr(env_shell, "csh")) {
        copy_str(shell->command, sizeof(shell->command), "csh");
        copy_str(shell->description, sizeof(shell->description), "C Shell (csh)");
    } else {
        copy_str(shell->command, sizeof(shell->command), "sh");
        copy_str(shell->description, sizeof(shell->description), "Bourne Again Shell (bash / sh)");
    }
    copy_str(shell->arg, sizeof(shell->arg), "-c");
}

char *llmt_build_system_prompt(const LlmtShell *shell) {
    const char *fmt =
        "You are a professional IT worker who only speaks in complete, %s compatible CLI commands running on the %s operating system. "
        "Only respond by translating the user's input into that language. Be precise because the user may execute what you return. "
        "Do not wrap the command in string delimiters. Do not include explanations, alternatives, comments, Markdown, formatting, or fenced code blocks. "
        "Return exactly one actionable command that can run by itself without error. If you are not confident in the translation, return an empty string. "
        "Assume you are operating in the current directory unless explicitly stated otherwise.";
    size_t len = snprintf(NULL, 0, fmt, shell->description, shell->os_name);
    char *out = (char *)malloc(len + 1);
    if (!out) return NULL;
    snprintf(out, len + 1, fmt, shell->description, shell->os_name);
    return out;
}

int llmt_generate_command(const LlmtConfig *config, const char *prompt, char **command,
                          LlmtError *err) {
    LlmtShell shell;
    LlmtUrl base;
    LlmtHttpResponse response;
    char endpoint[768];
    char *system_prompt;
    char *request_json;
    char *api_error;
    char auth_header[512];
    const char *auth = NULL;
    const char *key;
    int ok = 0;

    llmt_detect_shell(&shell);
    system_prompt = llmt_build_system_prompt(&shell);
    if (!system_prompt) {
        llmt_error(err, "out of memory while creating system prompt");
        return 0;
    }
    request_json = llmt_build_chat_request(config, system_prompt, prompt, err);
    free(system_prompt);
    if (!request_json) return 0;

    snprintf(endpoint, sizeof(endpoint), "%s/chat/completions", config->base_url);
    if (!llmt_parse_url(endpoint, &base, err)) {
        free(request_json);
        return 0;
    }

    if (is_openai_provider(config)) {
        key = getenv("OPENAI_API_KEY");
        if (!key || key[0] == '\0') {
            free(request_json);
            llmt_error(err, "OPENAI_API_KEY environment variable is not set");
            return 0;
        }
        snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s\r\n", key);
        auth = auth_header;
    }

    memset(&response, 0, sizeof(response));
    if (!llmt_http_post_json(&base, auth, request_json, &response, err)) {
        free(request_json);
        return 0;
    }
    free(request_json);

    if (response.status < 200 || response.status >= 300) {
        api_error = llmt_extract_api_error(response.body ? response.body : "");
        if (api_error) {
            llmt_error(err, "HTTP %d: %s", response.status, api_error);
            free(api_error);
        } else {
            llmt_error(err, "HTTP %d from model provider", response.status);
        }
        llmt_http_response_free(&response);
        return 0;
    }
    *command = llmt_extract_chat_command(response.body, err);
    if (*command) ok = 1;
    llmt_http_response_free(&response);
    return ok;
}
