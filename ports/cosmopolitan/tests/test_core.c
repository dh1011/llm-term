#include "llm_term.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int llmt_http_post_json(const LlmtUrl *url, const char *auth_header, const char *json_body,
                        LlmtHttpResponse *response, LlmtError *err) {
    (void)url;
    (void)auth_header;
    (void)json_body;
    (void)response;
    llmt_error(err, "network stub");
    return 0;
}

int llmt_execute_command(const LlmtShell *shell, const char *command, LlmtError *err) {
    (void)shell;
    (void)command;
    llmt_error(err, "execution stub");
    return 0;
}

static void test_args(void) {
    LlmtOptions options;
    LlmtError err;
    char *argv[] = {"llm-term", "--disable-cache", "--config-file", "cfg.json", "list files"};
    assert(llmt_parse_args(5, argv, &options, &err));
    assert(options.disable_cache == 1);
    assert(strcmp(options.config_file, "cfg.json") == 0);
    assert(strcmp(options.prompt, "list files") == 0);
}

static void test_config_new_schema(void) {
    LlmtConfig config;
    LlmtError err;
    const char *json =
        "{\"provider\":\"ollama\",\"model\":\"llama3.2\",\"max_tokens\":77,"
        "\"temperature\":0.25,\"base_url\":\"http://localhost:11434/v1\"}";
    assert(llmt_parse_config_text(json, &config, &err));
    assert(strcmp(config.provider, "ollama") == 0);
    assert(strcmp(config.model, "llama3.2") == 0);
    assert(config.max_tokens == 77);
}

static void test_config_defaults(void) {
    LlmtConfig config;
    llmt_config_defaults(&config);
    assert(strcmp(config.provider, "openai") == 0);
    assert(strcmp(config.model, "gpt-5.4-mini") == 0);
}

static void test_config_legacy_openai(void) {
    LlmtConfig config;
    LlmtError err;
    assert(llmt_parse_config_text("{\"model\":\"gpt-4o\",\"max_tokens\":42}", &config, &err));
    assert(strcmp(config.provider, "openai") == 0);
    assert(strcmp(config.model, "gpt-4o") == 0);
    assert(config.max_tokens == 42);
    assert(strcmp(config.base_url, "https://api.openai.com/v1") == 0);
}

static void test_config_legacy_ollama(void) {
    LlmtConfig config;
    LlmtError err;
    assert(llmt_parse_config_text("{\"model\":{\"ollama\":\"llama3.1\"},\"max_tokens\":99}",
                                  &config, &err));
    assert(strcmp(config.provider, "ollama") == 0);
    assert(strcmp(config.model, "llama3.1") == 0);
    assert(strcmp(config.base_url, "http://localhost:11434/v1") == 0);
}

static void test_cache(void) {
    LlmtCache cache;
    LlmtError err;
    char *json;
    assert(llmt_parse_cache_text("{\"a\":\"b\"}", &cache, &err));
    assert(strcmp(llmt_cache_get(&cache, "a"), "b") == 0);
    assert(llmt_cache_put(&cache, "x", "y", &err));
    assert(strcmp(llmt_cache_get(&cache, "x"), "y") == 0);
    assert(llmt_cache_remove(&cache, "a"));
    assert(llmt_cache_get(&cache, "a") == NULL);
    json = llmt_cache_to_json(&cache);
    assert(json != NULL);
    assert(strstr(json, "\"x\"") != NULL);
    free(json);
    llmt_cache_free(&cache);
}

static void test_url(void) {
    LlmtUrl url;
    LlmtError err;
    assert(llmt_parse_url("https://api.openai.com/v1/chat/completions", &url, &err));
    assert(url.https == 1);
    assert(strcmp(url.host, "api.openai.com") == 0);
    assert(url.port == 443);
    assert(strcmp(url.path, "/v1/chat/completions") == 0);
    assert(llmt_parse_url("http://localhost:11434/v1", &url, &err));
    assert(url.https == 0);
    assert(url.port == 11434);
}

static void test_chunked(void) {
    LlmtError err;
    char *decoded = llmt_decode_chunked("4\r\nWiki\r\n5\r\npedia\r\n0\r\n\r\n", 24, &err);
    assert(decoded != NULL);
    assert(strcmp(decoded, "Wikipedia") == 0);
    free(decoded);
}

static void test_http_parse(void) {
    LlmtHttpResponse response;
    LlmtError err;
    char raw[] = "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n4\r\nTest\r\n0\r\n\r\n";
    assert(llmt_parse_http_response(raw, &response, &err));
    assert(response.status == 200);
    assert(strcmp(response.body, "Test") == 0);
    llmt_http_response_free(&response);
}

static void test_chat_extract(void) {
    LlmtError err;
    char *cmd = llmt_extract_chat_command(
        "{\"choices\":[{\"message\":{\"content\":\"  ls -la\\n\"}}]}", &err);
    assert(cmd != NULL);
    assert(strcmp(cmd, "ls -la") == 0);
    free(cmd);
}

static void test_request_build(void) {
    LlmtConfig config;
    LlmtError err;
    char *json;
    llmt_config_defaults(&config);
    json = llmt_build_chat_request(&config, "system", "user", &err);
    assert(json != NULL);
    assert(strstr(json, "\"messages\"") != NULL);
    assert(strstr(json, "\"max_completion_tokens\"") != NULL);
    free(json);
}

static void test_shell_detect(void) {
    LlmtShell shell;
    llmt_detect_shell(&shell);
    assert(shell.command[0] != '\0');
    assert(shell.arg[0] != '\0');
    assert(shell.os_name[0] != '\0');
}

int main(void) {
    test_args();
    test_config_new_schema();
    test_config_defaults();
    test_config_legacy_openai();
    test_config_legacy_ollama();
    test_cache();
    test_url();
    test_chunked();
    test_http_parse();
    test_chat_extract();
    test_request_build();
    test_shell_detect();
    puts("core tests passed");
    return 0;
}
