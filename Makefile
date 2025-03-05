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
TEST_CFLAGS = -I$(NGINX_SRC)/src/core \
              -I$(NGINX_SRC)/src/event \
              -I$(NGINX_SRC)/src/event/modules \
              -I$(NGINX_SRC)/src/os/unix \
              -I$(NGINX_SRC)/src/http \
              -I$(NGINX_SRC)/src/http/modules \
              -I$(NGINX_SRC)/objs \
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
MODULE_OUTPUT = $(NGINX_SRC)/objs/ngx_http_sayplease_module.so

# Release directory
RELEASE_DIR = release

.PHONY: all clean download install test test-unit test-integration install-deps check-deps dist build test-only test-unit-only test-integration-only prepare-build prepare-test-deps test-sayplease-only help demo release

all: check-deps build

# Help target
help:
	@echo "SayPlease Nginx Module - Build System"
	@echo "===================================="
	@echo ""
	@echo "Available targets:"
	@echo ""
	@echo "Building:"
	@echo "  make                    - Check dependencies and build the module"
	@echo "  make all                - Same as above"
	@echo "  make build              - Build the module"
	@echo "  make DB_ENGINE=duckdb all - Build with DuckDB instead of SQLite"
	@echo "  make ARCH=arm64 all     - Build for ARM64 architecture"
	@echo "  make ARCH=x86_64 all    - Build for x86_64 architecture"
	@echo ""
	@echo "Dependencies:"
	@echo "  make download           - Download required dependencies"
	@echo "  make install-deps       - Install system dependencies"
	@echo "  make check-deps         - Check if all dependencies are satisfied"
	@echo ""
	@echo "Testing:"
	@echo "  make test               - Run all tests"
	@echo "  make test-sayplease-only - Run SayPlease module tests"
	@echo ""
	@echo "Demo:"
	@echo "  make demo URL=http://localhost:8080/path - Run a demo with the specified URL"
	@echo ""
	@echo "Distribution:"
	@echo "  make release            - Create a standalone distribution package"
	@echo "  make dist               - Create distribution package"
	@echo ""
	@echo "Installation:"
	@echo "  make install            - Install the module"
	@echo ""
	@echo "Cleaning:"
	@echo "  make clean              - Clean build artifacts"
	@echo ""
	@echo "Examples:"
	@echo "  make DB_ENGINE=duckdb install-deps  - Install dependencies for DuckDB"
	@echo "  make ARCH=arm64 all                 - Build for ARM64 architecture"
	@echo "  make demo URL=http://localhost:8080/secret.html - Test with a specific URL"
	@echo ""
	@echo "Environment variables are automatically set for library paths."
	@echo "Current settings:"
	@echo "  OS: $(OS)"
	@echo "  Architecture: $(ARCH)"
	@echo "  PCRE library: $(PCRE_LIB_PATH)"
	@echo "  PCRE include: $(PCRE_INCLUDE_PATH)"
	@echo "  Database engine: $(DB_ENGINE)"
	@echo ""

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
	@echo "Downloading OpenSSL source..."
	cd $(BUILD_DIR) && wget -q $(OPENSSL_URL) -O $(OPENSSL_TAR)
	cd $(BUILD_DIR) && tar xzf $(OPENSSL_TAR)
	@echo "Cleaning up downloaded archives..."
	cd $(BUILD_DIR) && rm -f $(NGINX_TAR) $(PCRE_TAR) $(OPENSSL_TAR)

# Build PCRE
build-pcre: download
	@if [ ! -d "$(PCRE_SRC)" ]; then \
		echo "PCRE source directory not found. Please run 'make download' first."; \
		exit 1; \
	fi
	@echo "Building PCRE for tests..."
	cd $(PCRE_SRC) && ./configure --enable-static --enable-shared && make

# Build OpenSSL
build-openssl: download
	@if [ ! -d "$(OPENSSL_SRC)" ]; then \
		echo "OpenSSL source directory not found. Please run 'make download' first."; \
		exit 1; \
	fi
	@echo "Building OpenSSL for tests..."
	cd $(OPENSSL_SRC) && \
	if [ "$(OS)" = "Darwin" ]; then \
		if [ "$(ARCH)" = "arm64" ]; then \
			./Configure darwin64-arm64-cc no-shared; \
		else \
			./Configure darwin64-x86_64-cc no-shared; \
		fi; \
	elif [ "$(OS)" = "Linux" ]; then \
		if [ "$(ARCH)" = "x86_64" ]; then \
			./config no-shared; \
		elif [ "$(ARCH)" = "aarch64" ] || [ "$(ARCH)" = "arm64" ]; then \
			./Configure linux-aarch64 no-shared; \
		else \
			./config no-shared; \
		fi; \
	else \
		./config no-shared; \
	fi && \
	make

# Configure NGINX
configure-nginx: download build-pcre build-openssl
	@if [ ! -d "$(NGINX_SRC)" ]; then \
		echo "NGINX source directory not found. Please run 'make download' first."; \
		exit 1; \
	fi
	@echo "Configuring nginx with SayPlease module..."
	cd $(NGINX_SRC) && \
	if [ "$(DB_ENGINE)" = "duckdb" ]; then \
		SAYPLEASE_USE_DUCKDB=1 ./configure --add-module=../../src \
			--with-compat \
			--with-threads \
			--with-http_ssl_module \
			--with-pcre=../../$(PCRE_SRC) \
			--with-openssl=../../$(OPENSSL_SRC); \
	else \
		./configure --add-module=../../src \
			--with-compat \
			--with-threads \
			--with-http_ssl_module \
			--with-pcre=../../$(PCRE_SRC) \
			--with-openssl=../../$(OPENSSL_SRC); \
	fi

# Build NGINX module
build: configure-nginx
	@if [ ! -f "$(NGINX_SRC)/Makefile" ]; then \
		echo "NGINX not configured. Please run 'make configure-nginx' first."; \
		exit 1; \
	fi
	@echo "Building nginx with SayPlease module..."
	cd $(NGINX_SRC) && make modules

# Check dependencies
check-deps:
	@echo "Checking build environment and dependencies..."
	@echo "=============================================="
	@echo ""
	@echo "Checking for required tools:"
	@echo "  CC................: $(CC) ($(call check_dependency_version,$(CC)))"
	@echo "  CXX...............: $(CXX) ($(call check_dependency_version,$(CXX)))"
	@echo "  make..............: $(MAKE) ($(call check_dependency_version,$(MAKE)))"
	@echo "  wget..............: $(WGET) ($(call check_dependency_version,$(WGET)))"
	@echo "  curl..............: $(CURL) ($(call check_dependency_version,$(CURL)))"
	@echo "  tar...............: $(TAR) ($(call check_dependency_version,$(TAR)))"
	@echo "  pkg-config........: $(PKG_CONFIG) ($(call check_dependency_version,$(PKG_CONFIG)))"
	@echo "  perl..............: $(PERL) ($(call check_dependency_version,$(PERL)))"
	@echo ""
	@echo "Checking for required libraries:"
	@missing=""
	@if [ ! -x "$$(command -v $(CC))" ]; then missing="$$missing $(CC)"; fi
	@if [ ! -x "$$(command -v $(MAKE))" ]; then missing="$$missing $(MAKE)"; fi
	@if [ ! -x "$$(command -v $(WGET))" ] && [ ! -x "$$(command -v $(CURL))" ]; then missing="$$missing wget/curl"; fi
	@if [ ! -x "$$(command -v $(TAR))" ]; then missing="$$missing $(TAR)"; fi
	@if [ ! -x "$$(command -v $(PERL))" ]; then missing="$$missing $(PERL)"; fi
	
	@echo "  PCRE..............: $(if $(call check_lib,pcre),yes,no) $(if $(HAS_PKG_CONFIG),\($(call pkg_config_version,libpcre)\),)"
	@echo "  zlib..............: $(if $(call check_lib,z),yes,no) $(if $(HAS_PKG_CONFIG),\($(call pkg_config_version,zlib)\),)"
	@echo "  OpenSSL...........: $(if $(call check_lib,ssl),yes,no) $(if $(HAS_PKG_CONFIG),\($(call pkg_config_version,openssl)\),)"
	@if [ "$(DB_ENGINE)" = "duckdb" ]; then \
		echo "  DuckDB............: $(if $(shell command -v duckdb 2>/dev/null),yes,no)"; \
	else \
		echo "  SQLite3...........: $(if $(call check_lib,sqlite3),yes,no) $(if $(HAS_PKG_CONFIG),\($(call pkg_config_version,sqlite3)\),)"; \
	fi
	@echo ""
	
	@if [ "$(call check_lib,pcre)" = "no" ]; then missing="$$missing libpcre"; fi
	@if [ "$(call check_lib,z)" = "no" ]; then missing="$$missing zlib"; fi
	@if [ "$(call check_lib,ssl)" = "no" ] || [ "$(call check_lib,crypto)" = "no" ]; then missing="$$missing openssl"; fi
	@if [ "$(DB_ENGINE)" = "duckdb" ] && [ ! -x "$$(command -v duckdb)" ]; then \
		missing="$$missing duckdb"; \
	elif [ "$(DB_ENGINE)" = "sqlite" ] && [ "$(call check_lib,sqlite3)" = "no" ]; then \
		missing="$$missing sqlite3"; \
	fi
	
	@echo "System information:"
	@echo "  Operating System..: $(OS)"
	@echo "  Architecture......: $(ARCH)"
	@echo "  PCRE library path.: $(PCRE_LIB_PATH)"
	@echo "  PCRE include path.: $(PCRE_INCLUDE_PATH)"
	@echo "  Database engine...: $(DB_ENGINE)"
	@echo ""
	
	@if [ ! -z "$$missing" ]; then \
		echo "ERROR: Missing dependencies:$$missing"; \
		echo "Please run 'make install-deps' to install the required dependencies"; \
		exit 1; \
	fi
	@echo "All dependencies are satisfied. Ready to build."

# Install the module
install: build
	@echo "Installing SayPlease module..."
	sudo cp $(NGINX_SRC)/objs/ngx_http_sayplease_module.so /usr/lib/nginx/modules/
	sudo cp config/nginx.conf.example /etc/nginx/nginx.conf
	sudo cp examples/robots.txt /etc/nginx/robots.txt
	sudo mkdir -p /var/lib/nginx
	sudo chown nginx:nginx /var/lib/nginx
	sudo systemctl restart nginx

# Install dependencies
install-deps:
	@echo "Installing dependencies..."
	if [ "$(DB_ENGINE)" = "duckdb" ]; then \
		if [ -f /etc/debian_version ]; then \
			sudo apt-get install -y build-essential libduckdb-dev unity libpcre3-dev zlib1g-dev libssl-dev perl; \
		elif [ -f /etc/redhat-release ]; then \
			sudo yum install -y gcc gcc-c++ make duckdb-devel unity pcre-devel zlib-devel openssl-devel perl; \
		elif [ -f /etc/arch-release ]; then \
			sudo pacman -S --noconfirm base-devel duckdb unity pcre zlib openssl perl; \
		elif [ "$(OS)" = "Darwin" ]; then \
			brew install pcre zlib openssl@1.1; \
		elif [ "$(findstring MINGW,$(OS))" = "MINGW" ]; then \
			pacman -S --noconfirm mingw-w64-x86_64-gcc mingw-w64-x86_64-pcre mingw-w64-x86_64-zlib mingw-w64-x86_64-openssl; \
		fi \
	else \
		if [ -f /etc/debian_version ]; then \
			sudo apt-get install -y build-essential libsqlite3-dev unity libpcre3-dev zlib1g-dev libssl-dev perl; \
		elif [ -f /etc/redhat-release ]; then \
			sudo yum install -y gcc gcc-c++ make sqlite-devel unity pcre-devel zlib-devel openssl-devel perl; \
		elif [ -f /etc/arch-release ]; then \
			sudo pacman -S --noconfirm base-devel sqlite unity pcre zlib openssl perl; \
		elif [ "$(OS)" = "Darwin" ]; then \
			brew install pcre zlib sqlite openssl@1.1; \
		elif [ "$(findstring MINGW,$(OS))" = "MINGW" ]; then \
			pacman -S --noconfirm mingw-w64-x86_64-gcc mingw-w64-x86_64-pcre mingw-w64-x86_64-zlib mingw-w64-x86_64-openssl mingw-w64-x86_64-sqlite3; \
		fi \
	fi

# Run all tests
test: test-sayplease-only

# This target builds dependencies and runs all tests
test-unit-only: prepare-test-deps
	$(TEST_CC) $(TEST_CFLAGS) -c -o build/unity/unity.o deps/unity/src/unity.c
	$(TEST_CC) $(TEST_CFLAGS) -o tests/unit/test_unit tests/unit/test_unit.c src/ngx_http_sayplease_module_test.c src/ngx_mock.c build/unity/unity.o $(TEST_LIBS)
	@if [ "$(OS)" = "Darwin" ]; then \
		DYLD_LIBRARY_PATH="$(dir $(PCRE_LIB_PATH))" tests/unit/test_unit; \
	elif [ "$(OS)" = "Linux" ]; then \
		LD_LIBRARY_PATH="$(dir $(PCRE_LIB_PATH))" tests/unit/test_unit; \
	else \
		PATH="$(dir $(PCRE_LIB_PATH)):$$PATH" tests/unit/test_unit; \
	fi

# This target only prepares the dependencies needed for testing
prepare-test-deps: download build-pcre build-openssl build-unity configure-nginx
	@echo "Test dependencies prepared"

# This target only runs the SayPlease module tests without building dependencies
test-sayplease-only:
	mkdir -p build/unity
	$(TEST_CC) $(TEST_CFLAGS) -c -o build/unity/unity.o deps/unity/src/unity.c
	$(TEST_CC) $(TEST_CFLAGS) -o tests/unit/test_sayplease tests/unit/test_sayplease.c src/ngx_http_sayplease_module_test.c src/ngx_mock.c build/unity/unity.o $(TEST_LIBS)
	@if [ "$(OS)" = "Darwin" ]; then \
		DYLD_LIBRARY_PATH="$(dir $(PCRE_LIB_PATH))" tests/unit/test_sayplease; \
	elif [ "$(OS)" = "Linux" ]; then \
		LD_LIBRARY_PATH="$(dir $(PCRE_LIB_PATH))" tests/unit/test_sayplease; \
	else \
		PATH="$(dir $(PCRE_LIB_PATH)):$$PATH" tests/unit/test_sayplease; \
	fi

test-integration-only: build
	chmod +x tests/integration/test_integration.sh
	cd tests/integration && ./test_integration.sh

# Clean up
clean:
	rm -rf $(BUILD_DIR)
	rm -f tests/unit/test_sayplease
	rm -f tests/integration/nginx_test.conf tests/integration/test.db
	rm -rf tests/integration/www

dist:
	@echo "Creating distribution package..."
	@echo "To install the SayPlease module, follow these steps:"
	@echo ""
	@echo "1. Copy the following files to your target system:"
	@echo "   - src/                    -> Module source code"
	@echo "   - config/nginx.conf.example -> Example nginx configuration"
	@echo "   - examples/robots.txt     -> Example robots.txt file"
	@echo ""
	@echo "2. On the target system:"
	@echo "   a. Install dependencies (nginx, pcre, openssl)"
	@echo "   b. Copy nginx.conf.example to /etc/nginx/nginx.conf"
	@echo "   c. Copy robots.txt to /etc/nginx/robots.txt"
	@echo "   d. Build and install the module:"
	@echo "      $$ cd src"
	@echo "      $$ nginx -V # Note configure arguments"
	@echo "      $$ ./configure --add-module=../src [previous configure args]"
	@echo "      $$ make modules"
	@echo "      $$ sudo cp objs/ngx_http_sayplease_module.so /usr/lib/nginx/modules/"
	@echo ""
	@echo "3. Create required directories:"
	@echo "   $$ sudo mkdir -p /var/lib/nginx"
	@echo "   $$ sudo chown nginx:nginx /var/lib/nginx"
	@echo ""
	@echo "4. Restart nginx:"
	@echo "   $$ sudo systemctl restart nginx"
	@echo ""
	@echo "To create an actual distribution package, run:"
	@echo "   $$ mkdir -p $(DIST_DIR)"
	@echo "   $$ cp -r src config examples $(DIST_DIR)/"
	@echo "   $$ cp README.md LICENSE $(DIST_DIR)/"
	@echo "   $$ tar czf sayplease-module.tar.gz $(DIST_DIR)/"
	@echo ""
	@echo "The distribution package will be created as 'sayplease-module.tar.gz'"

# Add a target to build Unity
build-unity:
	mkdir -p $(UNITY_SRC)/build
	cd $(UNITY_SRC) && cc -c src/unity.c -o build/unity.o
	cd $(UNITY_SRC) && ar rcs build/libunity.a build/unity.o

# Demo target to test the module with a specific URL
demo:
	@if [ -z "$(URL)" ]; then \
		echo "Error: URL parameter is required. Usage: make demo URL=http://example.com/path"; \
		exit 1; \
	fi
	@echo "Testing SayPlease module with URL: $(URL)"
	@mkdir -p demo
	@cp -f config/nginx.conf.example demo/nginx.conf
	@sed -i.bak 's|/path/to/robots.txt|$(PWD)/examples/robots.txt|g' demo/nginx.conf
	@sed -i.bak 's|/path/to/database|$(PWD)/demo/sayplease.db|g' demo/nginx.conf
	@sed -i.bak 's|/path/to/static/content|$(PWD)/examples/static|g' demo/nginx.conf
	@sed -i.bak 's|listen       80|listen       8080|g' demo/nginx.conf
	@mkdir -p demo/logs
	@echo "Starting nginx with SayPlease module..."
	@nginx -c $(PWD)/demo/nginx.conf -p $(PWD)/demo
	@echo "Sending request to $(URL)..."
	@curl -A "Googlebot" -s "$(URL)" > demo/response.html
	@echo "Response saved to demo/response.html"
	@echo "Stopping nginx..."
	@nginx -c $(PWD)/demo/nginx.conf -p $(PWD)/demo -s stop
	@echo "Demo complete. Check demo/sayplease.db for bot tracking data."
	@echo "To view the database: sqlite3 demo/sayplease.db 'SELECT * FROM bot_requests;'"

# Create a standalone distribution package
release: build
	@echo "Creating standalone distribution package..."
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
	@echo '# SayPlease Nginx Module Installation Script' >> $(RELEASE_DIR)/install.sh
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
	@echo 'cp bin/ngx_http_sayplease_module.so "$$MODULES_DIR/"' >> $(RELEASE_DIR)/install.sh
	@echo '' >> $(RELEASE_DIR)/install.sh
	@echo '# Create configuration' >> $(RELEASE_DIR)/install.sh
	@echo 'echo "Installing configuration example..."' >> $(RELEASE_DIR)/install.sh
	@echo 'NGINX_CONF_DIR="$$NGINX_PREFIX/conf"' >> $(RELEASE_DIR)/install.sh
	@echo 'cp conf/nginx.conf.example "$$NGINX_CONF_DIR/sayplease.conf.example"' >> $(RELEASE_DIR)/install.sh
	@echo '' >> $(RELEASE_DIR)/install.sh
	@echo 'echo "Installation complete!"' >> $(RELEASE_DIR)/install.sh
	@echo 'echo ""' >> $(RELEASE_DIR)/install.sh
	@echo 'echo "To enable the module, add the following to your nginx.conf:"' >> $(RELEASE_DIR)/install.sh
	@echo 'echo "  load_module modules/ngx_http_sayplease_module.so;"' >> $(RELEASE_DIR)/install.sh
	@echo 'echo ""' >> $(RELEASE_DIR)/install.sh
	@echo 'echo "For configuration examples, see: $$NGINX_CONF_DIR/sayplease.conf.example"' >> $(RELEASE_DIR)/install.sh
	@chmod +x $(RELEASE_DIR)/install.sh
	@echo "Creating package..."
	@tar -czf sayplease-module-$(shell date +%Y%m%d).tar.gz -C $(RELEASE_DIR) .
	@echo "Standalone distribution package created: sayplease-module-$(shell date +%Y%m%d).tar.gz"
	@echo "To install on a target system, extract the package and run ./install.sh" 