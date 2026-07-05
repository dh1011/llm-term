#define _POSIX_C_SOURCE 200809L

#include "llm_term.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <fcntl.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include "mbedtls/error.h"
#include "mbedtls/net_sockets.h"
#include "mbedtls/ssl.h"
#include "mbedtls/x509_crt.h"
#include "psa/crypto.h"
#include "psa/crypto_extra.h"

extern const unsigned char llm_term_ca_bundle[];
extern const size_t llm_term_ca_bundle_len;

typedef struct {
    int fd;
    mbedtls_ssl_context ssl;
    int tls;
} Transport;

static int append_bytes(char **buf, size_t *len, size_t *cap, const unsigned char *data, size_t n) {
    char *next;
    if (*len + n + 1 > *cap) {
        size_t new_cap = *cap ? *cap * 2 : 8192;
        while (new_cap < *len + n + 1) new_cap *= 2;
        next = (char *)realloc(*buf, new_cap);
        if (!next) return 0;
        *buf = next;
        *cap = new_cap;
    }
    memcpy(*buf + *len, data, n);
    *len += n;
    (*buf)[*len] = '\0';
    return 1;
}

static void mbed_error(LlmtError *err, const char *prefix, int code) {
    char text[256];
    mbedtls_strerror(code, text, sizeof(text));
    llmt_error(err, "%s: %s", prefix, text);
}

static int retryable_socket_error(void) {
    return errno == EINTR || errno == EAGAIN
#if defined(EWOULDBLOCK) && EWOULDBLOCK != EAGAIN
           || errno == EWOULDBLOCK
#endif
        ;
}

static int connect_tcp(const LlmtUrl *url, LlmtError *err) {
    struct addrinfo hints;
    struct addrinfo *result = NULL;
    struct addrinfo *rp;
    char port[16];
    int fd = -1;
    struct timeval timeout;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    snprintf(port, sizeof(port), "%d", url->port);
    if (getaddrinfo(url->host, port, &hints, &result) != 0) {
        llmt_error(err, "failed to resolve %s", url->host);
        return -1;
    }
    timeout.tv_sec = 60;
    timeout.tv_usec = 0;
    for (rp = result; rp; rp = rp->ai_next) {
        fd = (int)socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (fd < 0) continue;
        setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, (const char *)&timeout, sizeof(timeout));
        setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, (const char *)&timeout, sizeof(timeout));
        if (connect(fd, rp->ai_addr, (socklen_t)rp->ai_addrlen) == 0) break;
        close(fd);
        fd = -1;
    }
    freeaddrinfo(result);
    if (fd < 0) {
        llmt_error(err, "failed to connect to %s:%d", url->host, url->port);
    }
    return fd;
}

static int send_all_plain(int fd, const unsigned char *buf, size_t len, LlmtError *err) {
    size_t sent = 0;
    while (sent < len) {
        ssize_t n = send(fd, (const char *)buf + sent, len - sent, 0);
        if (n < 0 && retryable_socket_error()) continue;
        if (n <= 0) {
            llmt_error(err, "failed to send request: %s", strerror(errno));
            return 0;
        }
        sent += (size_t)n;
    }
    return 1;
}

static int tls_send(void *ctx, const unsigned char *buf, size_t len) {
    int fd = *(int *)ctx;
    ssize_t n = send(fd, (const char *)buf, len, 0);
    if (n < 0 && retryable_socket_error()) return MBEDTLS_ERR_SSL_WANT_WRITE;
    if (n < 0) return MBEDTLS_ERR_NET_SEND_FAILED;
    return (int)n;
}

static int tls_recv(void *ctx, unsigned char *buf, size_t len) {
    int fd = *(int *)ctx;
    ssize_t n = recv(fd, (char *)buf, len, 0);
    if (n < 0 && retryable_socket_error()) return MBEDTLS_ERR_SSL_WANT_READ;
    if (n < 0) return MBEDTLS_ERR_NET_RECV_FAILED;
    if (n == 0) return MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY;
    return (int)n;
}

static int send_all_tls(mbedtls_ssl_context *ssl, const unsigned char *buf, size_t len,
                        LlmtError *err) {
    size_t sent = 0;
    while (sent < len) {
        int n = mbedtls_ssl_write(ssl, buf + sent, len - sent);
        if (n == MBEDTLS_ERR_SSL_WANT_READ || n == MBEDTLS_ERR_SSL_WANT_WRITE) continue;
        if (n < 0) {
            mbed_error(err, "failed to send TLS request", n);
            return 0;
        }
        sent += (size_t)n;
    }
    return 1;
}

static char *read_all_plain(int fd, LlmtError *err) {
    char tmp[8192];
    char *buf = NULL;
    size_t len = 0;
    size_t cap = 0;
    for (;;) {
        ssize_t n = recv(fd, tmp, sizeof(tmp), 0);
        if (n == 0) break;
        if (n < 0 && retryable_socket_error()) continue;
        if (n < 0) {
            free(buf);
            llmt_error(err, "failed to read response: %s", strerror(errno));
            return NULL;
        }
        if (!append_bytes(&buf, &len, &cap, (unsigned char *)tmp, (size_t)n)) {
            free(buf);
            llmt_error(err, "out of memory while reading response");
            return NULL;
        }
    }
    if (!buf) buf = llmt_strdup("");
    return buf;
}

static char *read_all_tls(mbedtls_ssl_context *ssl, LlmtError *err) {
    unsigned char tmp[8192];
    char *buf = NULL;
    size_t len = 0;
    size_t cap = 0;
    for (;;) {
        int n = mbedtls_ssl_read(ssl, tmp, sizeof(tmp));
        if (n == 0 || n == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY) break;
        if (n == MBEDTLS_ERR_SSL_WANT_READ || n == MBEDTLS_ERR_SSL_WANT_WRITE ||
            n == MBEDTLS_ERR_SSL_RECEIVED_NEW_SESSION_TICKET) {
            continue;
        }
        if (n < 0) {
            free(buf);
            mbed_error(err, "failed to read TLS response", n);
            return NULL;
        }
        if (!append_bytes(&buf, &len, &cap, tmp, (size_t)n)) {
            free(buf);
            llmt_error(err, "out of memory while reading response");
            return NULL;
        }
    }
    if (!buf) buf = llmt_strdup("");
    return buf;
}

static char *build_request(const LlmtUrl *url, const char *auth_header, const char *json_body,
                           LlmtError *err) {
    const char *host_fmt = (url->port == 80 || url->port == 443) ? "%s" : "%s:%d";
    char host_header[320];
    size_t len;
    char *request;
    if (url->port == 80 || url->port == 443) {
        snprintf(host_header, sizeof(host_header), host_fmt, url->host);
    } else {
        snprintf(host_header, sizeof(host_header), host_fmt, url->host, url->port);
    }
    len = snprintf(NULL, 0,
                   "POST %s HTTP/1.1\r\n"
                   "Host: %s\r\n"
                   "User-Agent: llm-term/%s\r\n"
                   "Accept: application/json\r\n"
                   "Content-Type: application/json\r\n"
                   "%s"
                   "Content-Length: %zu\r\n"
                   "Connection: close\r\n"
                   "\r\n"
                   "%s",
                   url->path, host_header, LLMT_VERSION, auth_header ? auth_header : "",
                   strlen(json_body), json_body);
    request = (char *)malloc(len + 1);
    if (!request) {
        llmt_error(err, "out of memory while building HTTP request");
        return NULL;
    }
    snprintf(request, len + 1,
             "POST %s HTTP/1.1\r\n"
             "Host: %s\r\n"
             "User-Agent: llm-term/%s\r\n"
             "Accept: application/json\r\n"
             "Content-Type: application/json\r\n"
             "%s"
             "Content-Length: %zu\r\n"
             "Connection: close\r\n"
             "\r\n"
             "%s",
             url->path, host_header, LLMT_VERSION, auth_header ? auth_header : "",
             strlen(json_body), json_body);
    return request;
}

static int setup_tls(const LlmtUrl *url, int *fd, mbedtls_ssl_context *ssl,
                     mbedtls_ssl_config *conf, mbedtls_x509_crt *cacert, LlmtError *err) {
    int ret;
    psa_status_t psa_status;
    mbedtls_ssl_init(ssl);
    mbedtls_ssl_config_init(conf);
    mbedtls_x509_crt_init(cacert);

    psa_status = psa_crypto_init();
    if (psa_status != PSA_SUCCESS) {
        llmt_error(err, "failed to initialize PSA crypto: %d", (int)psa_status);
        return 0;
    }
    ret = mbedtls_x509_crt_parse(cacert, llm_term_ca_bundle, llm_term_ca_bundle_len + 1);
    if (ret < 0) {
        mbed_error(err, "failed to parse CA bundle", ret);
        return 0;
    }
    ret = mbedtls_ssl_config_defaults(conf, MBEDTLS_SSL_IS_CLIENT, MBEDTLS_SSL_TRANSPORT_STREAM,
                                      MBEDTLS_SSL_PRESET_DEFAULT);
    if (ret != 0) {
        mbed_error(err, "failed to configure TLS", ret);
        return 0;
    }
    mbedtls_ssl_conf_authmode(conf, MBEDTLS_SSL_VERIFY_REQUIRED);
    mbedtls_ssl_conf_ca_chain(conf, cacert, NULL);
    ret = mbedtls_ssl_setup(ssl, conf);
    if (ret != 0) {
        mbed_error(err, "failed to set up TLS", ret);
        return 0;
    }
    ret = mbedtls_ssl_set_hostname(ssl, url->host);
    if (ret != 0) {
        mbed_error(err, "failed to set TLS host name", ret);
        return 0;
    }
    mbedtls_ssl_set_bio(ssl, fd, tls_send, tls_recv, NULL);
    while ((ret = mbedtls_ssl_handshake(ssl)) != 0) {
        if (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE) continue;
        mbed_error(err, "TLS handshake failed", ret);
        return 0;
    }
    if (mbedtls_ssl_get_verify_result(ssl) != 0) {
        llmt_error(err, "TLS certificate verification failed");
        return 0;
    }
    return 1;
}

int llmt_http_post_json(const LlmtUrl *url, const char *auth_header, const char *json_body,
                        LlmtHttpResponse *response, LlmtError *err) {
    int fd;
    char *request;
    char *raw;
    int ok;
    mbedtls_ssl_context ssl;
    mbedtls_ssl_config conf;
    mbedtls_x509_crt cacert;
    int tls_ready = 0;

    memset(response, 0, sizeof(*response));
    request = build_request(url, auth_header, json_body, err);
    if (!request) return 0;

    fd = connect_tcp(url, err);
    if (fd < 0) {
        free(request);
        return 0;
    }

    if (url->https) {
        if (!setup_tls(url, &fd, &ssl, &conf, &cacert, err)) {
            free(request);
            mbedtls_x509_crt_free(&cacert);
            mbedtls_ssl_config_free(&conf);
            mbedtls_ssl_free(&ssl);
            mbedtls_psa_crypto_free();
            close(fd);
            return 0;
        }
        tls_ready = 1;
        ok = send_all_tls(&ssl, (const unsigned char *)request, strlen(request), err);
        raw = ok ? read_all_tls(&ssl, err) : NULL;
    } else {
        ok = send_all_plain(fd, (const unsigned char *)request, strlen(request), err);
        raw = ok ? read_all_plain(fd, err) : NULL;
    }

    free(request);
    if (tls_ready) {
        mbedtls_ssl_close_notify(&ssl);
        mbedtls_x509_crt_free(&cacert);
        mbedtls_ssl_config_free(&conf);
        mbedtls_ssl_free(&ssl);
        mbedtls_psa_crypto_free();
    }
    close(fd);
    if (!raw) return 0;
    ok = llmt_parse_http_response(raw, response, err);
    free(raw);
    return ok;
}
