# RoboNope - Nginx Module

[![Build and Test](https://github.com/raminf/RoboNope/actions/workflows/build.yml/badge.svg)](https://github.com/raminf/RoboNope/actions/workflows/build.yml)
[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![Nginx Version](https://img.shields.io/badge/nginx-1.24.0-brightgreen.svg)](https://nginx.org/)
[![Version](https://img.shields.io/badge/version-1.0.0-blue.svg)](https://github.com/raminf/RoboNope/releases)
[![Language](https://img.shields.io/badge/language-C-orange.svg)](https://en.wikipedia.org/wiki/C_(programming_language))
[![GitHub last commit](https://img.shields.io/github/last-commit/raminf/RoboNope)](https://github.com/raminf/RoboNope/commits/main)

![RoboNope](img/RoboNope.png)

`RoboNope` is an Nginx module designed to deny access to files specified in the robots.txt `Disallow` entries. It serves randomly generated content to bots that ignore `robots.txt` rules. Ignore at your peril.

According to [W3Techs](https://w3techs.com/technologies/overview/web_server) the top 5 most popular webservers as of March 2025 are:

- [nginx](https://nginx.org) (33.8%)
- [Apache](https://httpd.apache.org) (26.8%)
- [Cloudflare Server](https://www.cloudflare.com) (23.2%)
- [Litespeed](https://www.litespeedtech.com/products/litespeed-web-server) (14.5%)
- [Node.js](https://nodejs.org) (4.2%)

This version works with `nginx`. Plan is to release separate versions for other servers with the same functionality.

## Features

- Parses and enforces robots.txt rules
- Generates _dynamic_ content for disallowed paths
- Tracks bot requests in SQLite (or DuckDB -- _work in progress_)
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
- SQLite3 or DuckDB (under development)
- C compiler (gcc/clang)
- make

### Building from Source

```bash
# Clone the repository
git clone --recursive https://github.com/raminf/RoboNope.git
cd robonope

# Check dependencies
make check-deps

# Build with SQLite (default)
make

# Or build with DuckDB (work in progress)
make DB_ENGINE=duckdb all

# For specific architectures
make ARCH=arm64 all     # Build for ARM64
make ARCH=x86_64 all    # Build for x86_64

# For standalone module (requires nginx-dev)
make STANDALONE=1
```

The build system will automatically:
- Download and configure required dependencies
- Detect your platform and architecture
- Set up the appropriate build environment
- Compile the module and necessary components

### Running Tests

```bash
# Run all tests
make test

# Run RoboNope module tests only
make test-robonope-only
```

### Running the Demo

The demo will:
1. Set up a test environment
2. Start a local build of Nginx with the RoboNope module on port 8080
3. Send a test request to a disallowed path
4. Show the results

```bash
# Start the demo server
make demo-start

# Test with a disallowed URL
make demo-test

# View all logged requests (with pagination from the database)
make demo-logs

# Stop the demo server
make demo-stop

# Or run everything at once (except stop)
make demo
```

### Demo Configuration

You can customize the demo environment using these variables:

- `DB_PATH`: Override the default database location
  ```bash
  # Use custom database location
  DB_PATH=/tmp/robonope.db make demo-start
  
  # View logs from custom database
  DB_PATH=/tmp/robonope.db make demo-logs
  ```

- `DB_ENGINE`: Select the database engine (sqlite or duckdb)
  ```bash
  # Build and run with DuckDB (work in progress)
  DB_ENGINE=duckdb make all
  make demo-start
  ```

Default settings:
- Database Path: `build/demo/db/robonope.db`
- Database Engine: SQLite

### Rate Limiting

The demo server includes built-in rate limiting for disallowed paths:

- Limits requests to 1 per second per IP address
- Allows bursts of up to 5 requests
- Only applies to paths matching robots.txt Disallow patterns
- Protected paths:
  - /norobots/
  - /private/
  - /admin/
  - /secret-data/
  - /internal/

NOTE: All these paths may be overridden in `robots.txt` and configuration files.

### Viewing Logs

The demo stores all bot requests in a SQLite database (or DuckDB if configured). You can view the logs using:

```bash
make demo-logs
```

The log entries include:
- ID: Unique request identifier
- Timestamp: When the request was made
- IP: Client IP address
- User-Agent: Bot identifier
- URL: Requested path
- Matched Pattern: Which robots.txt pattern was matched

### Cleaning Up

```bash
# Clean build artifacts
make clean

# Clean demo environment
make clean-demo

# Force a complete rebuild
make clean-build
```

The Makefile is organized into logical sections for easier maintenance:
- BUILD TARGETS - For building the module and its dependencies
- CLEAN TARGETS - For cleaning up build artifacts and environments
- STANDALONE MODULE TARGETS - For standalone module builds and installation
- DEMO TARGETS - For running and managing the demo environment
- DEPENDENCY CHECKS - For verifying and configuring dependencies

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

### Standalone Installation

For systems with Nginx already installed:

```bash
# Build standalone module
make STANDALONE=1

# Create local package
make STANDALONE=1 install
```

This will create a `standalone` directory containing:
- `module/ngx_http_robonope_module.so` - The compiled module
- `conf/nginx.conf.example` - Example configuration
- `examples/` - Example content and robots.txt

The standalone build automatically detects your Nginx installation and configures the module accordingly. It provides detailed instructions for system-wide installation after the build is complete.

## Configuration

Add to your main nginx.conf:

```nginx
load_module modules/ngx_http_robonope_module.so;

http {
    # ... other settings ...

    # Optional rate limiting configuration for disallowed paths
    limit_req_zone $binary_remote_addr zone=robonope_limit:10m rate=1r/s;

    # RoboNope configuration
    robonope_enable on;
    robonope_robots_path /path/to/robots.txt;
    robonope_db_path /path/to/database;  # .db for SQLite, .duckdb for DuckDB
    robonope_static_content_path /path/to/static/content;
    robonope_dynamic_content on; # or off to use static content

    server {
        # ... server settings ...

        # Rate limiting for disallowed paths
        location ~ ^/(norobots|private|admin|secret-data|internal)/ {
            limit_req zone=robonope_limit burst=5 nodelay;
            robonope_enable on;  # Can be overridden per location
        }

        # Default location without rate limiting
        location / {
            robonope_enable on;  # Can be overridden per location
        }
    }
}
```

### Rate Limiting Options

The module supports Nginx's built-in rate limiting for disallowed paths. The configuration above:

- Creates a 10MB zone named `robonope_limit` that tracks client IP addresses
- Limits requests to 1 per second (1r/s) per IP address
- Allows a burst of 5 requests with no delay
- Only applies to paths that match the disallowed patterns in robots.txt

You can adjust these values based on your needs:
- `rate=1r/s`: Change the number before `r/s` to allow more/fewer requests per second
- `burst=5`: Change this number to allow larger/smaller bursts of requests
- `nodelay`: Remove this to queue excess requests instead of rejecting them
- `zone=robonope_limit:10m`: Adjust the 10m value to allocate more/less memory for tracking clients

Note: The `robonope_db_path` and other global settings must be defined in the `http` context, while `robonope_enable` can be toggled per location.

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

## Build System

The RoboNope module uses a sophisticated build system with the following features:

### Makefile Organization

The Makefile is organized into logical sections for easier maintenance and development:
- **BUILD TARGETS** - For building the module and its dependencies
- **CLEAN TARGETS** - For cleaning up build artifacts and environments
- **STANDALONE MODULE TARGETS** - For standalone module builds and installation
- **DEMO TARGETS** - For running and managing the demo environment
- **DEPENDENCY CHECKS** - For verifying and configuring dependencies

### Automatic Dependency Management

The build system automatically:
- Detects and uses system libraries when available (OpenSSL, PCRE)
- Downloads and builds dependencies when needed
- Sets up the appropriate environment variables for your platform
- Configures build flags based on your architecture and OS

### Cross-Platform Support

The build process is designed to work seamlessly across:
- Different operating systems (Linux, macOS, FreeBSD, Windows via MinGW)
- Different architectures (x86_64, ARM64)
- Different build environments (with or without system dependencies)

### Efficient Rebuilds

The build system tracks source file dependencies to avoid unnecessary rebuilds:
- Only rebuilds components that have changed
- Provides targeted clean targets for specific components
- Supports incremental builds for faster development

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details. 

## Disclosure

Most of the project, the README, and the artwork was assisted by AI. Even the name was workshopped with an AI. 

![Magritte Pipe](img/MagrittePipe.jpg)
