APP := llm-term.exe
VERSION := 1.0.0

ifeq ($(origin CC), default)
CC := cosmocc
COSMOFLAGS ?= -mclang -mcosmo
AR ?= cosmoar
RANLIB ?= cosmoranlib
else
COSMOFLAGS ?=
AR ?= ar
RANLIB ?= ranlib
endif
HOST_CC ?= clang
CMAKE ?= cmake
CMAKE_GENERATOR ?= Unix Makefiles

BUILD_DIR := build
DIST_DIR := dist
MBEDTLS_BUILD := $(BUILD_DIR)/mbedtls
MBEDTLS_STAMP := $(MBEDTLS_BUILD)/.built
ifeq ($(OS),Windows_NT)
TEST_EXE := $(BUILD_DIR)/test_core.exe
else
TEST_EXE := $(BUILD_DIR)/test_core
endif

COMMON_INCLUDES := -Isrc -Ithird_party/yyjson/src
TLS_INCLUDES := -Ithird_party/mbedtls/include -Ithird_party/mbedtls/tf-psa-crypto/include -Ithird_party/mbedtls/tf-psa-crypto/drivers/builtin/include
WARNINGS := -Wall -Wextra -Wpedantic
CFLAGS ?= -std=c99 -O2 $(WARNINGS) -D_COSMO_SOURCE $(COMMON_INCLUDES) $(TLS_INCLUDES)
TEST_CFLAGS ?= -std=c99 -O0 -g $(WARNINGS) $(COMMON_INCLUDES)
ifeq ($(OS),Windows_NT)
CFLAGS += -D_CRT_SECURE_NO_WARNINGS
TEST_CFLAGS += -D_CRT_SECURE_NO_WARNINGS
endif

APP_SRCS := src/main.c src/core.c src/http.c src/exec.c third_party/yyjson/src/yyjson.c generated/ca_bundle.c
TEST_SRCS := tests/test_core.c src/core.c third_party/yyjson/src/yyjson.c

MBEDTLS_LIBS := \
	$(MBEDTLS_BUILD)/library/libmbedtls.a \
	$(MBEDTLS_BUILD)/library/libmbedx509.a \
	$(MBEDTLS_BUILD)/library/libmbedcrypto.a

.PHONY: all test clean mbedtls check-cc

all: $(DIST_DIR)/$(APP)

$(DIST_DIR)/$(APP): $(APP_SRCS) $(MBEDTLS_STAMP)
	$(CMAKE) -E make_directory $(DIST_DIR)
	$(CC) $(COSMOFLAGS) $(CFLAGS) $(APP_SRCS) $(MBEDTLS_LIBS) -o $@

mbedtls: $(MBEDTLS_STAMP)

check-cc:
	$(CC) --version

$(MBEDTLS_STAMP): check-cc
	$(CMAKE) -E make_directory $(MBEDTLS_BUILD)
	$(CMAKE) -G "$(CMAKE_GENERATOR)" -S third_party/mbedtls -B $(MBEDTLS_BUILD) \
		-DCMAKE_BUILD_TYPE=Release \
		-DCMAKE_C_COMPILER=$(CC) \
		-DCMAKE_C_FLAGS="$(COSMOFLAGS) -D_COSMO_SOURCE" \
		-DCMAKE_AR=$(AR) \
		-DCMAKE_RANLIB=$(RANLIB) \
		-DENABLE_TESTING=OFF \
		-DENABLE_PROGRAMS=OFF \
		-DGEN_FILES=OFF \
		-DUSE_STATIC_MBEDTLS_LIBRARY=ON \
		-DUSE_SHARED_MBEDTLS_LIBRARY=OFF \
		-DUSE_STATIC_TF_PSA_CRYPTO_LIBRARY=ON \
		-DUSE_SHARED_TF_PSA_CRYPTO_LIBRARY=OFF \
		-DMBEDTLS_FATAL_WARNINGS=OFF \
		-DTF_PSA_CRYPTO_FATAL_WARNINGS=OFF
	$(CMAKE) --build $(MBEDTLS_BUILD) --config Release --target mbedtls
	$(CMAKE) -E touch $(MBEDTLS_STAMP)

test: $(TEST_EXE)
	$(TEST_EXE)

$(TEST_EXE): $(TEST_SRCS)
	$(CMAKE) -E make_directory $(BUILD_DIR)
	$(HOST_CC) $(TEST_CFLAGS) $(TEST_SRCS) -o $@

clean:
	$(CMAKE) -E rm -rf $(BUILD_DIR) $(DIST_DIR)
