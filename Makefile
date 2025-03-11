NGINX_VERSION = 1.24.0
PCRE_VERSION = 8.45
OPENSSL_VERSION = 1.1.1w
BUILD_DIR = build
DIST_DIR = dist

# Source directories
NGINX_SRC = $(BUILD_DIR)/nginx-$(NGINX_VERSION)
PCRE_SRC = $(BUILD_DIR)/pcre-$(PCRE_VERSION)
OPENSSL_SRC = $(BUILD_DIR)/openssl-$(OPENSSL_VERSION)
UNITY_SRC = deps/Unity

# NGINX paths and configuration
NGINX_SRC_DIR = $(NGINX_SRC)/src
NGINX_OBJS_DIR = $(NGINX_SRC)/objs
NGINX_INCS = -I$(NGINX_SRC_DIR)/core \
             -I$(NGINX_SRC_DIR)/event \
             -I$(NGINX_SRC_DIR)/event/modules \
             -I$(NGINX_SRC_DIR)/os/unix \
             -I$(NGINX_SRC_DIR)/http \
             -I$(NGINX_SRC_DIR)/http/modules \
             -I$(NGINX_OBJS_DIR)

# Download files
NGINX_TAR = nginx-$(NGINX_VERSION).tar.gz
PCRE_TAR = pcre-$(PCRE_VERSION).tar.gz
OPENSSL_TAR = openssl-$(OPENSSL_VERSION).tar.gz

# URLs
NGINX_URL = https://nginx.org/download/$(NGINX_TAR)
PCRE_URL = https://sourceforge.net/projects/pcre/files/pcre/$(PCRE_VERSION)/$(PCRE_TAR)/download
OPENSSL_URL = https://www.openssl.org/source/$(OPENSSL_TAR)

# Database selection (default to SQLite)
DB_ENGINE ?= sqlite
DB_PATH ?= $(PWD)/build/demo/db/robonope.db

# Architecture detection and selection
ARCH ?= $(shell uname -m)
OS := $(shell uname -s)

# Tool detection
CC ?= $(shell which gcc 2>/dev/null || which clang 2>/dev/null || echo gcc)
CXX ?= $(shell which g++ 2>/dev/null || which clang++ 2>/dev/null || echo g++)
MAKE ?= $(shell which make 2>/dev/null || echo make)
WGET ?= $(shell which wget 2>/dev/null || echo wget)
CURL ?= $(shell which curl 2>/dev/null || echo curl)
TAR ?= $(shell which tar 2>/dev/null || echo tar)
PKG_CONFIG ?= $(shell which pkg-config 2>/dev/null || echo pkg-config)
PERL ?= $(shell which perl 2>/dev/null || echo perl)

# Check if pkg-config is available
HAS_PKG_CONFIG := $(shell which $(PKG_CONFIG) >/dev/null 2>&1 && echo yes || echo no)

# PCRE library detection
ifeq ($(OS),Darwin)
    # macOS
    PCRE_LIB_PATHS := /opt/homebrew/lib/libpcre.1.dylib /usr/local/lib/libpcre.1.dylib /usr/lib/libpcre.1.dylib
    PCRE_LIB_PATH := $(shell for p in $(PCRE_LIB_PATHS); do if [ -f $$p ]; then echo $$p; break; fi; done)
    PCRE_INCLUDE_PATHS := /opt/homebrew/include /usr/local/include /usr/include
    PCRE_INCLUDE_PATH := $(shell for p in $(PCRE_INCLUDE_PATHS); do if [ -f $$p/pcre.h ]; then echo $$p; break; fi; done)
    # Set DYLD_LIBRARY_PATH for macOS
    PCRE_LIB_DIR := $(dir $(PCRE_LIB_PATH))
    export DYLD_LIBRARY_PATH := $(PCRE_LIB_DIR):$(PCRE_SRC)/.libs:$(OPENSSL_SRC):$(DYLD_LIBRARY_PATH)
else ifeq ($(OS),Linux)
    # Linux
    PCRE_LIB_PATHS := /usr/lib/libpcre.so.1 /usr/lib64/libpcre.so.1 /usr/local/lib/libpcre.so.1
    PCRE_LIB_PATH := $(shell for p in $(PCRE_LIB_PATHS); do if [ -f $$p ]; then echo $$p; break; fi; done)
    PCRE_INCLUDE_PATHS := /usr/include /usr/local/include
    PCRE_INCLUDE_PATH := $(shell for p in $(PCRE_INCLUDE_PATHS); do if [ -f $$p/pcre.h ]; then echo $$p; break; fi; done)
    # Set LD_LIBRARY_PATH for Linux
    PCRE_LIB_DIR := $(dir $(PCRE_LIB_PATH))
    export LD_LIBRARY_PATH := $(PCRE_LIB_DIR):$(PCRE_SRC)/.libs:$(OPENSSL_SRC):$(LD_LIBRARY_PATH)
else ifeq ($(findstring MINGW,$(OS)),MINGW)
    # Windows (MinGW)
    PCRE_LIB_PATHS := /mingw64/lib/libpcre.dll.a /mingw32/lib/libpcre.dll.a
    PCRE_LIB_PATH := $(shell for p in $(PCRE_LIB_PATHS); do if [ -f $$p ]; then echo $$p; break; fi; done)
    PCRE_INCLUDE_PATHS := /mingw64/include /mingw32/include
    PCRE_INCLUDE_PATH := $(shell for p in $(PCRE_INCLUDE_PATHS); do if [ -f $$p/pcre.h ]; then echo $$p; break; fi; done)
    # Update PATH for Windows
    PCRE_LIB_DIR := $(dir $(PCRE_LIB_PATH))
    export PATH := $(PCRE_LIB_DIR):$(PCRE_SRC)/.libs:$(OPENSSL_SRC):$(PATH)
else
    # Default fallback
    PCRE_LIB_PATH := 
    PCRE_INCLUDE_PATH :=
endif

# Test configuration
TEST_DIR = tests
TEST_SRC_DIR = $(TEST_DIR)/src
TEST_BUILD_DIR = $(TEST_DIR)/build
TEST_SOURCES = $(wildcard $(TEST_SRC_DIR)/*.c)
TEST_OBJECTS = $(TEST_SOURCES:$(TEST_SRC_DIR)/%.c=$(TEST_BUILD_DIR)/%.o)
TEST_BINS = $(TEST_SOURCES:$(TEST_SRC_DIR)/%.c=$(TEST_BUILD_DIR)/%)

# Test targets
.PHONY: test test-setup test-build test-run test-clean

test: test-setup test-build test-run

test-setup:
	@mkdir -p $(TEST_BUILD_DIR)
	@if [ ! -d "$(UNITY_SRC)" ]; then \
		echo "Unity framework not found. Initializing submodule..."; \
		git submodule update --init --recursive; \
	fi
	@$(MAKE) build-unity

test-build: test-setup
	@echo "Building tests..."
	@for src in $(TEST_SOURCES); do \
		obj=$${src/$(TEST_SRC_DIR)/$(TEST_BUILD_DIR)}; \
		obj=$${obj/.c/.o}; \
		bin=$${obj/.o/}; \
		echo "  $$src -> $$bin"; \
		$(CC) $(TEST_CFLAGS) -c $$src -o $$obj && \
		$(CC) $$obj $(TEST_LIBS) -o $$bin || exit 1; \
	done

test-run: test-build
	@echo "Running tests..."
	@for test in $(TEST_BINS); do \
		echo "  Running $$(basename $$test)..."; \
		$(TEST_ENV) $$test || exit 1; \
	done

test-clean:
	rm -rf $(TEST_BUILD_DIR)
	rm -rf $(UNITY_SRC)/build

# Add test to clean target
clean: test-clean
	@echo "Cleaning build artifacts..."
	rm -rf $(BUILD_DIR)/nginx-* $(BUILD_DIR)/pcre-* $(BUILD_DIR)/openssl-*
	rm -f $(BUILD_DIR)/*.tar.gz
	rm -f *.o *.a *.so *.lo *.la *.dylib
	rm -rf $(DIST_DIR)
	rm -rf release

# Standalone configuration
STANDALONE_DIR = standalone
STANDALONE_OBJS_DIR = $(STANDALONE_DIR)/objs
STANDALONE_MODULE = $(STANDALONE_OBJS_DIR)/ngx_http_robonope_module.so

# Default target
.PHONY: all
all: $(if $(filter 1,$(STANDALONE)),standalone-build,build)

# Standalone targets
.PHONY: standalone-build standalone-prepare
standalone-prepare:
	@mkdir -p $(STANDALONE_OBJS_DIR)
	@if [ "$(OS)" = "Darwin" ]; then \
		echo "Debug: NGINX_INC_PATH from env = $(NGINX_INC_PATH)"; \
		echo "Debug: Brew prefix = $$(brew --prefix)"; \
		if [ ! -z "$(NGINX_INC_PATH)" ]; then \
			echo "Debug: Checking NGINX_INC_PATH=$(NGINX_INC_PATH)"; \
			ls -la "$(NGINX_INC_PATH)" || true; \
			if [ -d "$(NGINX_INC_PATH)" ]; then \
				nginx_inc_path="$(NGINX_INC_PATH)"; \
			else \
				echo "Warning: NGINX_INC_PATH directory not found"; \
			fi; \
		fi; \
		if [ -z "$$nginx_inc_path" ]; then \
			brew_nginx_path="$$(brew --prefix nginx)/include/nginx"; \
			echo "Debug: Checking brew nginx path=$$brew_nginx_path"; \
			ls -la "$$brew_nginx_path" || true; \
			if [ -d "$$brew_nginx_path" ]; then \
				nginx_inc_path="$$brew_nginx_path"; \
			elif [ -d "$$(brew --prefix)/include/nginx" ]; then \
				nginx_inc_path="$$(brew --prefix)/include/nginx"; \
			elif [ -d "/usr/local/include/nginx" ]; then \
				nginx_inc_path="/usr/local/include/nginx"; \
			else \
				echo "Error: Cannot find Nginx headers. Tried:"; \
				echo "  - $(NGINX_INC_PATH) (from env)"; \
				echo "  - $$(brew --prefix)/opt/nginx/include/nginx"; \
				echo "  - $$(brew --prefix)/include/nginx"; \
				echo "  - /usr/local/include/nginx"; \
				echo "Please ensure nginx is installed via brew"; \
				exit 1; \
			fi; \
		fi; \
		echo "Found nginx headers at $$nginx_inc_path"; \
		ls -la "$$nginx_inc_path" || true; \
	else \
		if [ -d "/usr/include/nginx" ]; then \
			nginx_inc_path="/usr/include/nginx"; \
		else \
			echo "Error: Cannot find Nginx headers at /usr/include/nginx"; \
			echo "Please ensure nginx-dev package is installed"; \
			exit 1; \
		fi; \
	fi; \
	echo "Using nginx headers from $$nginx_inc_path"

standalone-build: standalone-prepare
	@echo "Building standalone module..."
	@if [ "$(OS)" = "Darwin" ]; then \
		if [ -d "$$(brew --prefix)/opt/nginx/include/nginx" ]; then \
			nginx_inc_path="$$(brew --prefix)/opt/nginx/include/nginx"; \
		else \
			nginx_inc_path="$$(brew --prefix)/include/nginx"; \
		fi; \
		$(CC) -c -fPIC \
			-I$$nginx_inc_path \
			-I$$nginx_inc_path/event \
			-I$$nginx_inc_path/os/unix \
			-I$(PCRE_INCLUDE_PATH) \
			-I$(OPENSSL_ROOT_DIR)/include \
			-o $(STANDALONE_OBJS_DIR)/ngx_http_robonope_module.o \
			src/ngx_http_robonope_module.c; \
		$(CC) -shared \
			-o $(STANDALONE_MODULE) \
			$(STANDALONE_OBJS_DIR)/ngx_http_robonope_module.o \
			-L$(PCRE_LIB_DIR) -lpcre \
			-L$(OPENSSL_ROOT_DIR)/lib -lssl -lcrypto; \
	else \
		nginx_inc_path="/usr/include/nginx"; \
		$(CC) -c -fPIC \
			-I$$nginx_inc_path \
			-I$$nginx_inc_path/event \
			-I$$nginx_inc_path/os/unix \
			-I$(PCRE_INCLUDE_PATH) \
			-o $(STANDALONE_OBJS_DIR)/ngx_http_robonope_module.o \
			src/ngx_http_robonope_module.c; \
		$(CC) -shared \
			-o $(STANDALONE_MODULE) \
			$(STANDALONE_OBJS_DIR)/ngx_http_robonope_module.o \
			-L$(PCRE_LIB_DIR) -lpcre; \
	fi
	@echo "Standalone module built successfully at $(STANDALONE_MODULE)"

# Add standalone clean to clean target
clean: standalone-clean

.PHONY: standalone-clean
standalone-clean:
	rm -rf $(STANDALONE_DIR)
