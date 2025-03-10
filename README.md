# RoboNope - Nginx Module

[![Build and Test](https://github.com/raminf/RoboNope/actions/workflows/build.yml/badge.svg)](https://github.com/raminf/RoboNope/actions/workflows/build.yml)
[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![Nginx Version](https://img.shields.io/badge/nginx-1.24.0-brightgreen.svg)](https://nginx.org/)
[![Version](https://img.shields.io/badge/version-1.0.0-blue.svg)](https://github.com/raminf/RoboNope/releases)
[![Language](https://img.shields.io/badge/language-C-orange.svg)](https://en.wikipedia.org/wiki/C_(programming_language))
[![GitHub last commit](https://img.shields.io/github/last-commit/raminf/RoboNope)](https://github.com/raminf/RoboNope/commits/main)

`RoboNope` is an Nginx module designed to handle requests that match patterns in robots.txt Disallow entries. It serves randomly generated content to bots that ignore robots.txt rules and tracks these requests in a database.

## Features

- Parses and enforces robots.txt rules
- Generates dynamic content for disallowed paths
- Tracks bot requests in SQLite or DuckDB
- Supports both static and dynamic content generation
- Configurable caching for performance
- Honeypot link generation
- Comprehensive test suite
- Cross-platform support

## Quick Start

### Prerequisites

- Nginx (1.24.0 or later recommended)
- PCRE library
- OpenSSL
- SQLite3 or DuckDB
- C compiler (gcc/clang)
- make

### Building from Source

```bash
# Clone the repository
git clone --recursive https://github.com/raminf/RoboNope.git
cd robonope

# Build with SQLite (default)
make

# Or build with DuckDB
make DB_ENGINE=duckdb all
```

### Running Tests

```bash
# Run all tests
make test

# Run specific test suites
make test-unit
make test-integration
```

### Running the Demo

The demo will:
1. Set up a test environment
2. Start Nginx with the RoboNope module on port 8080
3. Send a test request
4. Show the results

```bash
make demo URL=http://localhost:8080/secret.html
```

To view the tracked requests:
```bash
sqlite3 demo/robonope.db 'SELECT * FROM bot_requests;'
```

## Installation

### From Release Package

1. Download the latest release package from the [Releases](https://github.com/raminf/robonope/releases) page
2. Extract the package: `tar -xzf robonope-module-*.tar.gz`
3. Run the installation script: `sudo ./install.sh`
4. Add the module to your Nginx configuration: `load_module modules/ngx_http_robonope_module.so;`

### Manual Installation

1. Build the module (see Building from Source)
2. Copy the module to Nginx modules directory:
   ```bash
   sudo cp build/nginx-*/objs/ngx_http_robonope_module.so /usr/lib/nginx/modules/
   ```
3. Configure Nginx to use the module (see Configuration)

## Configuration

Add to your main nginx.conf:

```nginx
load_module modules/ngx_http_robonope_module.so;

http {
    # ... other settings ...

    robonope_enable on;
    robonope_robots_path /path/to/robots.txt;
    robonope_db_path /path/to/database;  # .db for SQLite, .duckdb for DuckDB
    robonope_static_content_path /path/to/static/content;
    robonope_dynamic_content on; # or off to use static content
}
```

## Platform Support

The RoboNope module is designed to work across different platforms. The build system automatically detects your operating system and architecture to configure the build process appropriately.

### Supported Platforms

- Linux (x86_64, aarch64)
- macOS (x86_64, arm64)
- FreeBSD
- Windows (via MinGW)

### Architecture Support

The module supports both x86_64 and ARM architectures. For ARM builds:

```bash
make ARCH=arm64 all
```

## PCRE Support

The PCRE (Perl Compatible Regular Expressions) library is a critical dependency for the RoboNope module. The build system automatically detects and uses the appropriate PCRE library for your platform. Library paths are automatically set in the environment, so you don't need to manually configure them.

### macOS

On macOS, the build system looks for PCRE in the following locations:
- Homebrew: `/opt/homebrew/lib`
- System: `/usr/local/lib`, `/usr/lib`

### Linux

On Linux systems, it checks:
- `/usr/lib`
- `/usr/lib64`
- `/usr/local/lib`

### Windows (MinGW)

For Windows builds using MinGW:
- `/mingw64/lib`
- `/mingw32/lib`

## Testing

The RoboNope module includes a comprehensive test suite to ensure functionality works as expected. To run the tests:

```bash
# Run all tests
make test

# Run only unit tests
make test-unit

# Run only the RoboNope module tests
make test-robonope-only

# Run integration tests
make test-integration
```

### Manual Testing

If you need to run tests manually, ensure the library path is set correctly:

macOS:
```bash
DYLD_LIBRARY_PATH=/path/to/pcre/lib tests/unit/test_robonope
```

Linux:
```bash
LD_LIBRARY_PATH=/path/to/pcre/lib tests/unit/test_robonope
```

Windows (MinGW):
```bash
PATH="/path/to/pcre/lib:$PATH" tests/unit/test_robonope
```

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details. 