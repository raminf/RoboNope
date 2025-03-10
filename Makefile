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
TEST_CC = $(CC)
TEST_CFLAGS = -I$(NGINX_SRC_DIR)/core \
              -I$(NGINX_SRC_DIR)/event \
              -I$(NGINX_SRC_DIR)/event/modules \
              -I$(NGINX_SRC_DIR)/os/unix \
              -I$(NGINX_SRC_DIR)/http \
              -I$(NGINX_SRC_DIR)/http/modules \
              -I$(NGINX_OBJS_DIR) \
              -I./src \
              -I$(PCRE_SRC) \
              -I$(PCRE_SRC)/include \
              -I$(OPENSSL_SRC)/include \
              -I$(UNITY_SRC)/src \
              -DNGX_LUA_USE_ASSERT \
              -DNGX_HAVE_PCRE

# Add PCRE include path if detected
ifneq ($(PCRE_INCLUDE_PATH),)
    TEST_CFLAGS += -I$(PCRE_INCLUDE_PATH)
endif

# Update test libs to include required libraries
TEST_LIBS = -L$(UNITY_SRC)/build -lunity \
            -L$(PCRE_SRC)/.libs -lpcre \
            -L$(OPENSSL_SRC) -lssl -lcrypto \
            -lz

# Add PCRE library path if detected
ifneq ($(PCRE_LIB_PATH),)
    TEST_LIBS += -L$(dir $(PCRE_LIB_PATH))
endif

# Set environment variables for test execution
ifeq ($(OS),Darwin)
    TEST_ENV = DYLD_LIBRARY_PATH=$(dir $(PCRE_LIB_PATH)):$(PCRE_SRC)/.libs:$(OPENSSL_SRC)
else ifeq ($(OS),Linux)
    TEST_ENV = LD_LIBRARY_PATH=$(dir $(PCRE_LIB_PATH)):$(PCRE_SRC)/.libs:$(OPENSSL_SRC)
else
    TEST_ENV = PATH="$(dir $(PCRE_LIB_PATH)):$(PCRE_SRC)/.libs:$(OPENSSL_SRC):$(PATH)"
endif

# Enhanced dependency check functions
check_dependency = $(shell which $(1) 2>/dev/null)
check_dependency_version = $(shell $(1) --version 2>/dev/null | head -n 1 || echo "unknown")
check_lib = $(shell $(CC) -l$(1) -o /dev/null -xc - 2>/dev/null <<<'' && echo 'yes' || echo 'no')
check_header = $(shell $(CC) -E -xc - 2>/dev/null <<<'#include <$(1)>' && echo 'yes' || echo 'no')

# pkg-config based library detection
ifeq ($(HAS_PKG_CONFIG),yes)
    check_pkg_config = $(shell $(PKG_CONFIG) --exists $(1) 2>/dev/null && echo 'yes' || echo 'no')
    pkg_config_cflags = $(shell $(PKG_CONFIG) --cflags $(1) 2>/dev/null)
    pkg_config_libs = $(shell $(PKG_CONFIG) --libs $(1) 2>/dev/null)
    pkg_config_version = $(shell $(PKG_CONFIG) --modversion $(1) 2>/dev/null)
else
    check_pkg_config = no
    pkg_config_cflags = 
    pkg_config_libs = 
    pkg_config_version = unknown
endif

# Module output
MODULE_OUTPUT = $(NGINX_SRC)/objs/ngx_http_robonope_module.so

# Release directory
RELEASE_DIR = release

# Demo configuration
DEMO_PORT ?= 8080
DEMO_DIR = build/demo
DEMO_NGINX = $(NGINX_SRC)/objs/nginx
DEMO_CONF = $(DEMO_DIR)/conf/nginx.conf
DEMO_LOGS = $(DEMO_DIR)/logs
DEMO_TEMP = $(DEMO_DIR)/temp

# OpenSSL CFLAGS - disable -Werror and problematic warnings
OPENSSL_CFLAGS = -Wno-missing-field-initializers -Wno-deprecated-declarations
OPENSSL_CONFIG_FLAGS = no-shared no-async

# OpenSSL detection
OPENSSL_MIN_VERSION = 1.1.1

# Check for environment variables first
ifdef OPENSSL_ROOT_DIR
    _OPENSSL_ROOT = $(OPENSSL_ROOT_DIR)
else ifdef OPENSSL_HOME
    _OPENSSL_ROOT = $(OPENSSL_HOME)
else ifdef OPENSSL_PATH
    _OPENSSL_ROOT = $(OPENSSL_PATH)
else
    # Platform-specific OpenSSL detection if no environment variable is set
    ifeq ($(OS),Darwin)
        # macOS - Try Homebrew first, then system paths
        _OPENSSL_ROOT = $(shell \
            if command -v brew >/dev/null 2>&1; then \
                brew --prefix openssl@3 2>/dev/null || brew --prefix openssl 2>/dev/null || brew --prefix openssl@1.1 2>/dev/null || echo "/usr/local/opt/openssl"; \
            else \
                echo "/usr/local/opt/openssl"; \
            fi)
    else ifeq ($(findstring MINGW,$(OS)),MINGW)
        # MinGW/MSYS2 detection
        _OPENSSL_ROOT = $(shell cygpath -m "$$(dirname $$(which openssl.exe))/.." 2>/dev/null || echo "C:/msys64/mingw64")
    else
        # Linux and other Unix-like systems
        _OPENSSL_ROOT = $(shell \
            if pkg-config openssl --exists 2>/dev/null; then \
                echo "$$(pkg-config --variable=prefix openssl)"; \
            elif [ -d "/usr/local/openssl" ]; then \
                echo "/usr/local/openssl"; \
            elif [ -d "/usr/include/openssl" ]; then \
                echo "/usr"; \
            else \
                echo "/usr/local"; \
            fi)
    endif
endif

# Validate OpenSSL path and get include/lib paths
_OPENSSL_INCLUDE = $(shell \
    if [ -f "$(_OPENSSL_ROOT)/include/openssl/ssl.h" ]; then \
        echo "$(_OPENSSL_ROOT)/include"; \
    fi)

_OPENSSL_LIB = $(shell \
    if [ -f "$(_OPENSSL_ROOT)/lib/libssl.a" ] || [ -f "$(_OPENSSL_ROOT)/lib/libssl.so" ] || [ -f "$(_OPENSSL_ROOT)/lib/libssl.so.3" ] || [ -f "$(_OPENSSL_ROOT)/lib/libssl.3.dylib" ] || [ -f "$(_OPENSSL_ROOT)/lib/libssl.dylib" ]; then \
        echo "$(_OPENSSL_ROOT)/lib"; \
    elif [ -f "$(_OPENSSL_ROOT)/lib64/libssl.a" ] || [ -f "$(_OPENSSL_ROOT)/lib64/libssl.so" ] || [ -f "$(_OPENSSL_ROOT)/lib64/libssl.so.3" ]; then \
        echo "$(_OPENSSL_ROOT)/lib64"; \
    fi)

# Get OpenSSL version
_OPENSSL_VERSION = $(shell \
    if [ -x "$(_OPENSSL_ROOT)/bin/openssl" ]; then \
        $(_OPENSSL_ROOT)/bin/openssl version | cut -d' ' -f2; \
    elif [ -f "$(_OPENSSL_ROOT)/include/openssl/opensslv.h" ]; then \
        grep -E 'OPENSSL_VERSION_STR|OPENSSL_VERSION_TEXT' "$(_OPENSSL_ROOT)/include/openssl/opensslv.h" | head -n1 | cut -d'"' -f2 | cut -d' ' -f2; \
    else \
        echo "0.0.0"; \
    fi)

# Function to compare versions (returns yes if ver1 >= ver2)
define check_version
$(shell printf '%s\n%s\n' "$(1)" "$(2)" | sort -V | head -n1 | grep -q "^$(2)$$" && echo "yes" || echo "no")
endef

# Check if system OpenSSL is usable
HAS_SYSTEM_OPENSSL = $(shell \
    if [ -n "$(_OPENSSL_ROOT)" ] && [ -n "$(_OPENSSL_INCLUDE)" ] && [ -n "$(_OPENSSL_LIB)" ] && \
       [ -f "$(_OPENSSL_INCLUDE)/openssl/ssl.h" ] && \
       ([ -f "$(_OPENSSL_LIB)/libssl.a" ] || \
        [ -f "$(_OPENSSL_LIB)/libssl.so" ] || \
        [ -f "$(_OPENSSL_LIB)/libssl.so.3" ] || \
        [ -f "$(_OPENSSL_LIB)/libssl.3.dylib" ] || \
        [ -f "$(_OPENSSL_LIB)/libssl.dylib" ]); then \
        if [ "$$(printf '%s\n%s\n' "$(_OPENSSL_VERSION)" "$(OPENSSL_MIN_VERSION)" | sort -V | head -n1)" = "$(OPENSSL_MIN_VERSION)" ]; then \
            echo "yes"; \
        else \
            echo "no"; \
        fi; \
    else \
        echo "no"; \
    fi)

# Export final paths for use in the build
OPENSSL_SYSTEM_ROOT = $(_OPENSSL_ROOT)
OPENSSL_SYSTEM_INCLUDE = $(_OPENSSL_INCLUDE)
OPENSSL_SYSTEM_LIB = $(_OPENSSL_LIB)
OPENSSL_SYSTEM_VERSION = $(_OPENSSL_VERSION)

# Add OpenSSL paths to compiler flags if using system OpenSSL
ifeq ($(HAS_SYSTEM_OPENSSL),yes)
    NGINX_INCS += -I$(OPENSSL_SYSTEM_INCLUDE)
    TEST_CFLAGS += -I$(OPENSSL_SYSTEM_INCLUDE)
    TEST_LIBS += -L$(OPENSSL_SYSTEM_LIB)
endif

# Standalone module configuration
STANDALONE ?= 0
STANDALONE_DIR = standalone
NGINX_CONFIG_DIR = $(shell nginx -V 2>&1 | grep "configure arguments:" | sed 's/.*--conf-path=\([^ ]*\).*/\1/' | xargs dirname 2>/dev/null || echo "/etc/nginx")
NGINX_MODULE_DIR = $(shell nginx -V 2>&1 | grep "configure arguments:" | sed 's/.*--modules-path=\([^ ]*\).*/\1/' 2>/dev/null || echo "/usr/lib/nginx/modules")

# Add source file tracking for proper rebuilds
SRC_FILES := $(wildcard src/*.c src/*.h)

# Modify build target to track dependencies
.PHONY: build-target
build-target: $(MODULE_OUTPUT) $(DEMO_NGINX)

$(MODULE_OUTPUT) $(DEMO_NGINX): $(SRC_FILES)
	@mkdir -p build
	@echo "Found system OpenSSL version $(OPENSSL_VERSION) at $(OPENSSL_PREFIX)"
	@echo "Include path: $(OPENSSL_INCLUDE)"
	@echo "Library path: $(OPENSSL_LIB)"
	@echo "Downloading nginx source..."
	cd build && wget -q $(NGINX_URL) -O nginx-1.24.0.tar.gz
	cd build && tar xzf nginx-1.24.0.tar.gz
	@echo "Downloading PCRE source..."
	cd build && wget -q $(PCRE_URL) -O pcre-8.45.tar.gz
	cd build && tar xzf pcre-8.45.tar.gz
	@echo "Cleaning up downloaded archives..."
	cd build && rm -f nginx-1.24.0.tar.gz pcre-8.45.tar.gz
	$(MAKE) build-pcre
	$(MAKE) configure-nginx
	@echo "Building nginx and modules..."
	cd build/nginx-1.24.0 && $(MAKE)
	@if [ ! -f "$(DEMO_NGINX)" ]; then \
		echo "ERROR: Failed to build nginx binary. Check build logs for errors."; \
		exit 1; \
	fi
	@if [ "$(STANDALONE)" = "1" ]; then \
		$(MAKE) standalone-build; \
	else \
		if [ ! -f "$(MODULE_OUTPUT)" ]; then \
			echo "Module was not built correctly. Trying explicit build..."; \
			cd $(NGINX_SRC) && CFLAGS="$(NGINX_INCS)" make -f objs/Makefile ngx_http_robonope_module.so; \
		fi && \
		if [ ! -f "$(MODULE_OUTPUT)" ]; then \
			echo "ERROR: Failed to build module. Check build logs for errors."; \
			exit 1; \
		fi; \
	fi
	@echo "Build completed successfully:"
	@echo "  Module: $(MODULE_OUTPUT)"
	@echo "  Nginx:  $(DEMO_NGINX)"

# Update build to use the new target
build: build-target

# Update check-module-binary to use build-target
.PHONY: check-module-binary
check-module-binary:
	@if [ ! -f "$(MODULE_OUTPUT)" ] || [ ! -f "$(DEMO_NGINX)" ] || [ ! -x "$(DEMO_NGINX)" ]; then \
		echo "Module binary or nginx binary not found or not executable. Running clean build..."; \
		$(MAKE) clean-build; \
	else \
		echo "Using existing binaries:"; \
		echo "  Module: $(MODULE_OUTPUT)"; \
		echo "  Nginx:  $(DEMO_NGINX)"; \
		if [ ! -x "$(DEMO_NGINX)" ]; then \
			echo "Warning: nginx binary exists but is not executable. Running clean build..."; \
			$(MAKE) clean-build; \
		fi \
	fi

# Create a standalone distribution package
release: build
	@echo "Creating standalone distribution package..."
	@echo "Checking for module file: $(MODULE_OUTPUT)"
	@ls -la $(NGINX_SRC)/objs/ || echo "Cannot list directory contents"
	@mkdir -p $(RELEASE_DIR)/bin
	@mkdir -p $(RELEASE_DIR)/conf
	@mkdir -p $(RELEASE_DIR)/examples
	@mkdir -p $(RELEASE_DIR)/docs
	@cp $(MODULE_OUTPUT) $(RELEASE_DIR)/bin/
	@cp config/nginx.conf.example $(RELEASE_DIR)/conf/
	@cp examples/robots.txt $(RELEASE_DIR)/examples/
	@cp -r examples/static $(RELEASE_DIR)/examples/
	@cp README.md LICENSE $(RELEASE_DIR)/docs/
	@echo "Creating installation script..."
	@echo '#!/bin/bash' > $(RELEASE_DIR)/install.sh
	@echo '# RoboNope Nginx Module Installation Script' >> $(RELEASE_DIR)/install.sh
	@echo '' >> $(RELEASE_DIR)/install.sh
	@echo '# Detect Nginx modules directory' >> $(RELEASE_DIR)/install.sh
	@echo 'NGINX_PREFIX=$$(nginx -V 2>&1 | grep "configure arguments:" | sed '"'"'s/.*--prefix=\([^ ]*\).*/\1/'"'"')' >> $(RELEASE_DIR)/install.sh
	@echo 'MODULES_DIR="$$NGINX_PREFIX/modules"' >> $(RELEASE_DIR)/install.sh
	@echo '' >> $(RELEASE_DIR)/install.sh
	@echo 'if [ ! -d "$$MODULES_DIR" ]; then' >> $(RELEASE_DIR)/install.sh
	@echo '    echo "Creating modules directory: $$MODULES_DIR"' >> $(RELEASE_DIR)/install.sh
	@echo '    mkdir -p "$$MODULES_DIR"' >> $(RELEASE_DIR)/install.sh
	@echo 'fi' >> $(RELEASE_DIR)/install.sh
	@echo '' >> $(RELEASE_DIR)/install.sh
	@echo '# Copy module binary' >> $(RELEASE_DIR)/install.sh
	@echo 'echo "Installing module to $$MODULES_DIR..."' >> $(RELEASE_DIR)/install.sh
	@echo 'cp bin/ngx_http_robonope_module.so "$$MODULES_DIR/"' >> $(RELEASE_DIR)/install.sh
	@echo '' >> $(RELEASE_DIR)/install.sh
	@echo '# Create configuration' >> $(RELEASE_DIR)/install.sh
	@echo 'echo "Installing configuration example..."' >> $(RELEASE_DIR)/install.sh
	@echo 'NGINX_CONF_DIR="$$NGINX_PREFIX/conf"' >> $(RELEASE_DIR)/install.sh
	@echo 'cp conf/nginx.conf.example "$$NGINX_CONF_DIR/robonope.conf.example"' >> $(RELEASE_DIR)/install.sh
	@echo '' >> $(RELEASE_DIR)/install.sh
	@echo 'echo "Installation complete!"' >> $(RELEASE_DIR)/install.sh
	@echo 'echo ""' >> $(RELEASE_DIR)/install.sh
	@echo 'echo "To enable the module, add the following to your nginx.conf:"' >> $(RELEASE_DIR)/install.sh
	@echo 'echo "  load_module modules/ngx_http_robonope_module.so;"' >> $(RELEASE_DIR)/install.sh
	@echo 'echo ""' >> $(RELEASE_DIR)/install.sh
	@echo 'echo "For configuration examples, see: $$NGINX_CONF_DIR/robonope.conf.example"' >> $(RELEASE_DIR)/install.sh
	@chmod +x $(RELEASE_DIR)/install.sh
	@echo "Creating package..."
	@tar -czf robonope-module-$(shell date +%Y%m%d).tar.gz -C $(RELEASE_DIR) .
	@echo "Standalone distribution package created: robonope-module-$(shell date +%Y%m%d).tar.gz"
	@echo "To install on a target system, extract the package and run ./install.sh"

# Clean targets
.PHONY: clean clean-demo standalone-clean

clean:
	rm -rf build/demo standalone
	rm -f tests/unit/test_robonope
	rm -f tests/integration/nginx_test.conf tests/integration/test.db
	rm -rf tests/integration/www

clean-demo: clean

standalone-clean: clean

# Add standalone targets
.PHONY: standalone-build standalone-install

standalone-build: src/*
	@if [ "$(STANDALONE)" = "1" ]; then \
		echo "Building standalone module..."; \
		nginx_inc_path=$$(nginx -V 2>&1 | grep "configure arguments:" | sed 's/.*--prefix=\([^ ]*\).*/\1/')/include; \
		if [ ! -d "$$nginx_inc_path" ]; then \
			echo "Error: Cannot find Nginx headers. Please ensure Nginx development files are installed."; \
			echo "On Debian/Ubuntu: apt-get install nginx-dev"; \
			echo "On RHEL/CentOS: yum install nginx-devel"; \
			echo "On macOS: brew install nginx"; \
			exit 1; \
		fi; \
		mkdir -p $(STANDALONE_DIR)/objs; \
		$(CC) -c -fPIC \
			-I$$nginx_inc_path \
			-I$$nginx_inc_path/event \
			-I$$nginx_inc_path/os/unix \
			-o $(STANDALONE_DIR)/objs/ngx_http_robonope_module.o \
			src/ngx_http_robonope_module.c; \
		$(CC) -shared \
			-o $(STANDALONE_DIR)/objs/ngx_http_robonope_module.so \
			$(STANDALONE_DIR)/objs/ngx_http_robonope_module.o; \
	else \
		echo "Use STANDALONE=1 to build standalone module"; \
		exit 1; \
	fi

standalone-install: standalone-build
	@if [ "$(STANDALONE)" = "1" ]; then \
		echo "Installing standalone module..."; \
		mkdir -p $(STANDALONE_DIR)/module; \
		mkdir -p $(STANDALONE_DIR)/conf; \
		mkdir -p $(STANDALONE_DIR)/examples; \
		cp $(STANDALONE_DIR)/objs/ngx_http_robonope_module.so $(STANDALONE_DIR)/module/; \
		cp config/nginx.conf.example $(STANDALONE_DIR)/conf/; \
		cp examples/robots.txt $(STANDALONE_DIR)/examples/; \
		cp -r examples/static $(STANDALONE_DIR)/examples/; \
		echo "Module files installed to $(STANDALONE_DIR)/"; \
		echo ""; \
		echo "To install system-wide:"; \
		echo "1. Copy module:"; \
		echo "   sudo cp $(STANDALONE_DIR)/module/ngx_http_robonope_module.so $(NGINX_MODULE_DIR)/"; \
		echo ""; \
		echo "2. Add to nginx.conf:"; \
		echo "   load_module modules/ngx_http_robonope_module.so;"; \
		echo ""; \
		echo "3. Copy configuration:"; \
		echo "   sudo cp $(STANDALONE_DIR)/conf/nginx.conf.example $(NGINX_CONFIG_DIR)/robonope.conf"; \
		echo "   sudo cp $(STANDALONE_DIR)/examples/robots.txt $(NGINX_CONFIG_DIR)/"; \
		echo ""; \
		echo "4. Include in main nginx.conf:"; \
		echo "   include robonope.conf;"; \
		echo ""; \
		echo "5. Test and reload:"; \
		echo "   sudo nginx -t"; \
		echo "   sudo nginx -s reload"; \
	else \
		echo "Use STANDALONE=1 to install standalone module"; \
		exit 1; \
	fi

# Modify existing install target to handle standalone mode
install: build
	@if [ "$(STANDALONE)" = "1" ]; then \
		$(MAKE) standalone-install; \
	else \
		echo "Installing RoboNope module..."; \
		sudo cp $(NGINX_SRC)/objs/ngx_http_robonope_module.so /usr/lib/nginx/modules/; \
		sudo cp config/nginx.conf.example /etc/nginx/nginx.conf; \
		sudo cp examples/robots.txt /etc/nginx/robots.txt; \
		sudo mkdir -p /var/lib/nginx; \
		sudo chown nginx:nginx /var/lib/nginx; \
		sudo systemctl restart nginx; \
	fi

# Update help target to include standalone options
help:
	@echo "RoboNope Nginx Module - Build System"
	@echo "===================================="
	@echo ""
	@echo "Available targets:"
	@echo ""
	@echo "Building:"
	@echo "  make                    - Check dependencies and build the module"
	@echo "  make STANDALONE=1       - Build standalone module (requires nginx-dev)"
	@echo "  make all                - Same as above"
	@echo "  make build              - Build the module"
	@echo "  make DB_ENGINE=duckdb all - Build with DuckDB instead of SQLite"
	@echo "  make ARCH=arm64 all     - Build for ARM64 architecture"
	@echo "  make ARCH=x86_64 all    - Build for x86_64 architecture"
	@echo ""
	@echo "Demo Commands:"
	@echo "  make demo-start         - Start the demo server with RoboNope module"
	@echo "  make demo-test          - Test the module with a disallowed URL"
	@echo "  make demo-logs          - Show all logged requests in a pager"
	@echo "  make demo-stop          - Stop the demo server"
	@echo "  make demo               - Run demo-start and demo-test (but not demo-stop)"
	@echo ""
	@echo "Demo Configuration:"
	@echo "  DB_PATH                 - Override the database path"
	@echo "  DB_ENGINE               - Select database engine (sqlite or duckdb)"
	@echo ""
	@echo "Current Demo Settings:"
	@echo "  Database Path: $(DB_PATH)"
	@echo "  Database Engine: $(DB_ENGINE)"
	@echo ""
	@echo "Examples:"
	@echo "  DB_PATH=/tmp/robonope.db make demo-start  - Use custom database location"
	@echo "  DB_PATH=/tmp/robonope.db make demo-logs   - View logs from custom database"
	@echo "  make demo-logs                            - View logs from default database"
	@echo ""
	@echo "Dependencies:"
	@echo "  make download           - Download required dependencies"
	@echo "  make install-deps       - Install system dependencies"
	@echo "  make check-deps         - Check if all dependencies are satisfied"
	@echo ""
	@echo "Testing:"
	@echo "  make test               - Run all tests"
	@echo "  make test-robonope-only - Run RoboNope module tests"
	@echo ""
	@echo "Distribution:"
	@echo "  make release            - Create a standalone distribution package"
	@echo "  make dist               - Create distribution package"
	@echo ""
	@echo "Installation:"
	@echo "  make install            - Install the module system-wide"
	@echo "  make STANDALONE=1 install - Create local module package"
	@echo ""
	@echo "Cleaning:"
	@echo "  make clean              - Clean build artifacts"
	@echo "  make clean-demo         - Clean demo environment"
	@echo "  make clean-build        - Force a complete rebuild"
	@echo ""
	@echo "Environment variables are automatically set for library paths."
	@echo "Current settings:"
	@echo "  OS: $(OS)"
	@echo "  Architecture: $(ARCH)"
	@echo "  PCRE library: $(PCRE_LIB_PATH)"
	@echo "  PCRE include: $(PCRE_INCLUDE_PATH)"
	@echo ""

# Pre-build checks
.PHONY: check-openssl
check-openssl:
	@if [ "$(HAS_SYSTEM_OPENSSL)" = "yes" ] && [ -z "$(OPENSSL_CHECKED)" ]; then \
		echo "Found system OpenSSL version $(OPENSSL_SYSTEM_VERSION) at $(OPENSSL_SYSTEM_ROOT)"; \
		echo "Include path: $(OPENSSL_SYSTEM_INCLUDE)"; \
		echo "Library path: $(OPENSSL_SYSTEM_LIB)"; \
		export OPENSSL_CHECKED=1; \
	elif [ "$(HAS_SYSTEM_OPENSSL)" != "yes" ] && [ -z "$(OPENSSL_CHECKED)" ]; then \
		echo "No suitable system OpenSSL found (need >= $(OPENSSL_MIN_VERSION)). Will download and build from source."; \
		export OPENSSL_CHECKED=1; \
	fi

# Create build directory
prepare-build:
	mkdir -p $(BUILD_DIR)

# Download and extract sources
download: prepare-build
	@echo "Downloading nginx source..."
	cd $(BUILD_DIR) && wget -q $(NGINX_URL) -O $(NGINX_TAR)
	cd $(BUILD_DIR) && tar xzf $(NGINX_TAR)
	@echo "Downloading PCRE source..."
	cd $(BUILD_DIR) && wget -q $(PCRE_URL) -O $(PCRE_TAR)
	cd $(BUILD_DIR) && tar xzf $(PCRE_TAR)
	@if [ "$(HAS_SYSTEM_OPENSSL)" != "yes" ]; then \
		echo "Downloading OpenSSL source..."; \
		cd $(BUILD_DIR) && wget -q $(OPENSSL_URL) -O $(OPENSSL_TAR); \
		cd $(BUILD_DIR) && tar xzf $(OPENSSL_TAR); \
	fi
	@echo "Cleaning up downloaded archives..."
	cd $(BUILD_DIR) && rm -f $(NGINX_TAR) $(PCRE_TAR) $(if $(filter no,$(HAS_SYSTEM_OPENSSL)),$(OPENSSL_TAR))

# Build PCRE
build-pcre: download
	@if [ ! -d "$(PCRE_SRC)" ]; then \
		echo "PCRE source directory not found. Please run 'make download' first."; \
		exit 1; \
	fi
	@echo "Building PCRE for tests..."
	cd $(PCRE_SRC) && \
	if [ "$(OS)" = "Darwin" ]; then \
		CFLAGS="-O2 -pipe -fvisibility=hidden" \
		CXXFLAGS="-O2 -fvisibility=hidden -fvisibility-inlines-hidden" \
		LDFLAGS="-Wl,-undefined,dynamic_lookup -Wl,-no_fixup_chains" \
		./configure --disable-shared --enable-static \
			--enable-utf8 \
			--enable-unicode-properties \
			--enable-pcre16 \
			--enable-pcre32 \
			--enable-jit \
			--enable-cpp \
			--with-match-limit=10000000 \
			--with-match-limit-recursion=10000000 \
			--enable-rebuild-chartables \
			--enable-newline-is-lf; \
	else \
		CFLAGS="-O2 -pipe -fvisibility=hidden" \
		CXXFLAGS="-O2 -fvisibility=hidden -fvisibility-inlines-hidden" \
		./configure --disable-shared --enable-static \
			--enable-utf8 \
			--enable-unicode-properties \
			--enable-pcre16 \
			--enable-pcre32 \
			--enable-jit \
			--enable-cpp \
			--with-match-limit=10000000 \
			--with-match-limit-recursion=10000000 \
			--enable-rebuild-chartables \
			--enable-newline-is-lf; \
	fi && \
	make && \
	cp .libs/libpcre.a . && \
	cp .libs/libpcre16.a . && \
	cp .libs/libpcre32.a . && \
	cp config.h pcre.h

# Build OpenSSL (only if needed)
build-openssl: download
	@if [ "$(HAS_SYSTEM_OPENSSL)" = "yes" ]; then \
		echo "Using system OpenSSL version $(OPENSSL_SYSTEM_VERSION) from $(OPENSSL_SYSTEM_ROOT)"; \
	else \
		if [ ! -d "$(OPENSSL_SRC)" ]; then \
			echo "OpenSSL source directory not found. Please run 'make download' first."; \
			exit 1; \
		fi; \
		echo "Building OpenSSL from source..."; \
		cd $(OPENSSL_SRC) && \
		if [ "$(OS)" = "Darwin" ]; then \
			if [ "$(ARCH)" = "arm64" ]; then \
				CFLAGS="$(OPENSSL_CFLAGS)" ./Configure darwin64-arm64-cc $(OPENSSL_CONFIG_FLAGS) --prefix=$(CURDIR)/build/openssl-$(OPENSSL_VERSION)/.openssl -Wno-error=missing-field-initializers; \
			else \
				CFLAGS="$(OPENSSL_CFLAGS)" ./Configure darwin64-x86_64-cc $(OPENSSL_CONFIG_FLAGS) --prefix=$(CURDIR)/build/openssl-$(OPENSSL_VERSION)/.openssl -Wno-error=missing-field-initializers; \
			fi; \
		elif [ "$(OS)" = "Linux" ]; then \
			if [ "$(ARCH)" = "x86_64" ]; then \
				CFLAGS="$(OPENSSL_CFLAGS)" ./config $(OPENSSL_CONFIG_FLAGS) -Wno-error=missing-field-initializers; \
			elif [ "$(ARCH)" = "aarch64" ] || [ "$(ARCH)" = "arm64" ]; then \
				CFLAGS="$(OPENSSL_CFLAGS)" ./Configure linux-aarch64 $(OPENSSL_CONFIG_FLAGS) -Wno-error=missing-field-initializers; \
			else \
				CFLAGS="$(OPENSSL_CFLAGS)" ./config $(OPENSSL_CONFIG_FLAGS) -Wno-error=missing-field-initializers; \
			fi; \
		else \
			CFLAGS="$(OPENSSL_CFLAGS)" ./config $(OPENSSL_CONFIG_FLAGS) -Wno-error=missing-field-initializers; \
		fi && \
		make clean && \
		make CFLAGS="$(OPENSSL_CFLAGS) -Wno-error=missing-field-initializers" && \
		$(MAKE) install_sw; \
	fi

# Configure NGINX
configure-nginx: build-pcre $(if $(filter no,$(HAS_SYSTEM_OPENSSL)),build-openssl)
	@if [ ! -d "$(NGINX_SRC)" ]; then \
		echo "NGINX source directory not found. Please run 'make download' first."; \
		exit 1; \
	fi
	@echo "Configuring nginx with RoboNope module..."
	cd $(NGINX_SRC) && \
	if [ "$(DB_ENGINE)" = "duckdb" ]; then \
		ROBONOPE_USE_DUCKDB=1 ./configure --add-dynamic-module=../../src \
			--with-compat \
			--with-threads \
			--with-http_ssl_module \
			--with-pcre=../../$(PCRE_SRC) \
			--with-cc-opt="-I../../$(PCRE_SRC) -I/opt/homebrew/opt/openssl@3/include" \
			--with-ld-opt="-L../../$(PCRE_SRC) -L/opt/homebrew/opt/openssl@3/lib"; \
	else \
		./configure --add-dynamic-module=../../src \
			--with-compat \
			--with-threads \
			--with-http_ssl_module \
			--with-pcre=../../$(PCRE_SRC) \
			--with-cc-opt="-I../../$(PCRE_SRC) -I/opt/homebrew/opt/openssl@3/include" \
			--with-ld-opt="-L../../$(PCRE_SRC) -L/opt/homebrew/opt/openssl@3/lib"; \
	fi

# Generate NGINX headers
generate-headers: configure-nginx
	@echo "Generating NGINX headers..."
	@if [ ! -d "$(NGINX_OBJS_DIR)" ]; then \
		mkdir -p $(NGINX_OBJS_DIR); \
	fi
	@cd $(NGINX_SRC) && ./configure --with-compat

# Add a target to build Unity
build-unity:
	mkdir -p $(UNITY_SRC)/build
	cd $(UNITY_SRC) && cc -c src/unity.c -o build/unity.o
	cd $(UNITY_SRC) && ar rcs build/libunity.a build/unity.o

# Demo targets to test the module
demo-start: build
	@echo "Using existing binaries:"
	@echo "  Module: build/nginx-1.24.0/objs/ngx_http_robonope_module.so"
	@echo "  Nginx:  build/nginx-1.24.0/objs/nginx"
	@echo "Setting up demo environment..."
	@mkdir -p build/demo/conf
	@mkdir -p build/demo/html
	@mkdir -p build/demo/logs
	@mkdir -p $(dir $(DB_PATH))
	@cp examples/static/* build/demo/html/ || true
	@cp examples/robots.txt build/demo/html/ || true
	@echo "Creating mime.types file..."
	@echo "types {" > build/demo/conf/mime.types
	@echo "    text/html                             html htm shtml;" >> build/demo/conf/mime.types
	@echo "    text/css                              css;" >> build/demo/conf/mime.types
	@echo "}" >> build/demo/conf/mime.types
	@echo "Creating nginx.conf file..."
	@echo "load_module $(PWD)/build/nginx-1.24.0/objs/ngx_http_robonope_module.so;" > build/demo/conf/nginx.conf
	@echo "events { worker_connections 1024; }" >> build/demo/conf/nginx.conf
	@echo "http {" >> build/demo/conf/nginx.conf
	@echo "    include mime.types;" >> build/demo/conf/nginx.conf
	@echo "    default_type application/octet-stream;" >> build/demo/conf/nginx.conf
	@echo "    # RoboNope configuration" >> build/demo/conf/nginx.conf
	@echo "    robonope_enable on;" >> build/demo/conf/nginx.conf
	@echo "    robonope_robots_path $(PWD)/build/demo/html/robots.txt;" >> build/demo/conf/nginx.conf
	@echo "    robonope_db_path $(DB_PATH);" >> build/demo/conf/nginx.conf
	@echo "    # Rate limiting zones" >> build/demo/conf/nginx.conf
	@echo "    limit_req_zone \$$binary_remote_addr zone=robonope_limit:10m rate=1r/s;" >> build/demo/conf/nginx.conf
	@echo "    server {" >> build/demo/conf/nginx.conf
	@echo "        listen 8080;" >> build/demo/conf/nginx.conf
	@echo "        server_name localhost;" >> build/demo/conf/nginx.conf
	@echo "        root html;" >> build/demo/conf/nginx.conf
	@echo "        # Rate limiting for disallowed paths" >> build/demo/conf/nginx.conf
	@echo "        location ~ ^/(norobots|private|admin|secret-data|internal)/ {" >> build/demo/conf/nginx.conf
	@echo "            limit_req zone=robonope_limit burst=5 nodelay;" >> build/demo/conf/nginx.conf
	@echo "            robonope_enable on;" >> build/demo/conf/nginx.conf
	@echo "        }" >> build/demo/conf/nginx.conf
	@echo "        # Default location" >> build/demo/conf/nginx.conf
	@echo "        location / {" >> build/demo/conf/nginx.conf
	@echo "            robonope_enable on;" >> build/demo/conf/nginx.conf
	@echo "        }" >> build/demo/conf/nginx.conf
	@echo "    }" >> build/demo/conf/nginx.conf
	@echo "}" >> build/demo/conf/nginx.conf
	@echo "Starting Nginx with RoboNope module..."
	@cd build/demo && ../../build/nginx-1.24.0/objs/nginx -p . -c conf/nginx.conf
	@echo "Demo server started on http://localhost:8080"

demo-test:
	@echo "Testing RoboNope with a disallowed URL..."
	@curl -A "Googlebot" -s "http://localhost:8080/norobots/test.html"
	@echo "\nChecking database for logged requests..."
	@sqlite3 $(DB_PATH) 'SELECT * FROM requests;'

demo-logs:
	@echo "Showing all logged requests..."
	@echo "Database: $(DB_PATH)"
	@echo "\nFormat: ID|Timestamp|IP|User-Agent|URL|Matched Pattern\n"
	@sqlite3 $(DB_PATH) 'SELECT * FROM requests;' | less -S

demo-stop:
	@echo "Stopping demo server..."
	@pkill nginx || true
	@sleep 2
	@echo "Demo server stopped"

# Alias for backward compatibility
demo: demo-start demo-test
	@echo "Demo completed. Run 'make demo-stop' to shutdown the server."

# Add clean-build target for explicit rebuilds
.PHONY: clean-build
clean-build:
	@echo "Cleaning build directory..."
	rm -rf $(BUILD_DIR)
	@echo "Starting fresh build..."
	$(MAKE) download
	$(MAKE) build-pcre
	$(MAKE) configure-nginx
	cd $(NGINX_SRC) && $(MAKE)
	@if [ ! -f "$(DEMO_NGINX)" ]; then \
		echo "ERROR: Failed to build nginx binary. Check build logs for errors."; \
		exit 1; \
	fi
	@if [ ! -f "$(MODULE_OUTPUT)" ]; then \
		echo "ERROR: Failed to build module. Check build logs for errors."; \
		exit 1; \
	fi
	@chmod +x $(DEMO_NGINX)
	@echo "Build completed successfully:"
	@echo "  Module: $(MODULE_OUTPUT)"
	@echo "  Nginx:  $(DEMO_NGINX)" 