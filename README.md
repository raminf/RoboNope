# SayPlease - Nginx Module

[![Build and Test](https://github.com/raminf/SayPlease/actions/workflows/build.yml/badge.svg)](https://github.com/raminf/SayPlease/actions/workflows/build.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](https://opensource.org/licenses/MIT)
[![Nginx Version](https://img.shields.io/badge/Nginx-1.18%2B-green.svg)](https://nginx.org/)
![Version](https://img.shields.io/badge/version-1.0.0-brightgreen.svg)
![Language](https://img.shields.io/badge/language-C-orange.svg)
![GitHub last commit](https://img.shields.io/github/last-commit/raminf/SayPlease)

`SayPlease` is an Nginx module designed to handle requests that match patterns in robots.txt Disallow entries. It serves randomly generated content to bots that ignore robots.txt rules and tracks these violations.

If a bot spidering your website ignores the `robots.txt` file, they will find themselves in a [Maze of Twisty Little Passages](https://en.wikipedia.org/wiki/Colossal_Cave_Adventure).

## Features

- Parses robots.txt files to extract Disallow patterns
- Serves randomly generated Lorem Ipsum content to requests matching Disallow patterns
- Fingerprints and logs bot information in a database (SQLite or DuckDB)
- Injects invisible honeypot links into HTML responses
- Caches request fingerprints for faster response

## Prerequisites

- Nginx source code (version 1.18.0 or higher recommended)
- SQLite3 development libraries (default) or DuckDB development libraries
- C compiler (gcc or clang)
- Make
- PCRE library (libpcre)
- OpenSSL development libraries

## Building the Module

1. Clone this repository with submodules:
   ```
   git clone --recursive https://github.com/yourusername/sayplease.git
   cd sayplease
   ```

   If you've already cloned the repository without the `--recursive` flag, you can initialize and update the submodules with:
   ```
   git submodule init
   git submodule update
   ```

2. View available commands and options:
   ```bash
   make help
   ```

3. Install dependencies (choose one):
   ```bash
   # For SQLite (default)
   make install-deps

   # For DuckDB
   make DB_ENGINE=duckdb install-deps
   ```

4. Build the module (choose one):
   ```bash
   # For SQLite (default)
   make all

   # For DuckDB
   make DB_ENGINE=duckdb all
   
   # For specific architecture (e.g., ARM64)
   make ARCH=arm64 all
   ```

5. Install the module:
   ```bash
   make install
   ```

## Demo

You can quickly test the module with a demo URL to see how it responds to bot requests:

```bash
# Run a demo with a specific URL
make demo URL=http://localhost:8080/secret.html

# View the response
cat demo/response.html

# Check the database for tracked bot requests
sqlite3 demo/sayplease.db 'SELECT * FROM bot_requests;'
```

The demo:
1. Sets up a temporary Nginx configuration
2. Starts Nginx with the SayPlease module on port 8080
3. Sends a request with a Googlebot user agent
4. Saves the response to demo/response.html
5. Stops Nginx
6. Provides instructions to view the tracked data

## Standalone Installation

If you want to install the module on an existing Nginx installation without building from source:

1. Download the latest release package from the [Releases](https://github.com/yourusername/sayplease/releases) page
2. Extract the package: `tar -xzf sayplease-module-*.tar.gz`
3. Run the installation script: `./install.sh`
4. Add the module to your Nginx configuration: `load_module modules/ngx_http_sayplease_module.so;`
5. Configure the module as described in the Configuration section
6. Restart Nginx

You can also build the standalone package yourself:

```bash
make release
```

This creates a tarball containing the compiled module and all necessary files for installation.

## Configuration

Add the following to your nginx.conf file:

```nginx
load_module modules/ngx_http_sayplease_module.so;

http {
    # ... other http configurations ...
    
    sayplease_enable on;
    sayplease_robots_path /path/to/robots.txt;
    sayplease_db_path /path/to/database;  # .db for SQLite, .duckdb for DuckDB
    sayplease_static_content_path /path/to/static/content;
    sayplease_dynamic_content on; # or off to use static content
    
    # ... other configurations ...
}
```

## Database Options

### SQLite (Default)
- Lightweight, serverless database
- Perfect for single-instance deployments
- File-based storage with ACID properties

### DuckDB
- Analytical database optimized for OLAP workloads
- Better performance for complex queries and analytics
- Excellent for analyzing bot patterns and generating reports

To switch between databases:
1. Clean any previous build: `make clean`
2. Install appropriate dependencies: `make DB_ENGINE=duckdb install-deps`
3. Rebuild with chosen engine: `make DB_ENGINE=duckdb all`

## Cross-Platform Support

The SayPlease module is designed to work across different platforms. The build system automatically detects your operating system and architecture to configure the build process appropriately.

### Architecture Support

You can build for your current architecture (default) or specify a different target architecture:

```bash
# Build for the current architecture
make all

# Build for ARM64
make ARCH=arm64 all

# Build for x86_64
make ARCH=x86_64 all
```

## Handling PCRE Library Issues

The PCRE (Perl Compatible Regular Expressions) library is a critical dependency for the SayPlease module. The build system automatically detects and uses the appropriate PCRE library for your platform. Library paths are automatically set in the environment, so you don't need to manually set `DYLD_LIBRARY_PATH`, `LD_LIBRARY_PATH`, or `PATH`.

If you still encounter issues, here are platform-specific solutions:

### macOS

On macOS, PCRE libraries are typically installed via Homebrew in `/opt/homebrew/lib/` (for Apple Silicon) or `/usr/local/lib/` (for Intel Macs).

If you encounter library loading errors:

```bash
# Check if PCRE is installed
brew list pcre

# Reinstall PCRE if needed
brew reinstall pcre
```

### Linux

On Linux systems, PCRE libraries are typically installed in `/usr/lib/` or `/usr/lib64/`.

If you encounter library loading errors:

```bash
# Debian/Ubuntu
sudo apt-get install libpcre3-dev

# Red Hat/CentOS/Fedora
sudo yum install pcre-devel

# Arch Linux
sudo pacman -S pcre
```

### Windows (MinGW/MSYS2)

On Windows with MinGW/MSYS2, PCRE libraries are typically installed in `/mingw64/lib/` or `/mingw32/lib/`.

```bash
# Install PCRE using pacman
pacman -S mingw-w64-x86_64-pcre
```

## Testing

The SayPlease module includes a comprehensive test suite to ensure functionality works as expected. To run the tests:

```bash
# Run all tests
make test

# Run only the SayPlease module tests
make test-sayplease-only
```

The test system automatically sets the appropriate library paths (`DYLD_LIBRARY_PATH` on macOS, `LD_LIBRARY_PATH` on Linux, or `PATH` on Windows) to ensure the PCRE library is found during test execution.

### Troubleshooting Tests

If you encounter issues with tests:

1. Verify that the PCRE library is installed and detected correctly:
   ```bash
   make check-deps
   ```

2. If you see architecture mismatch warnings for OpenSSL, these can generally be ignored for testing purposes. If you want to eliminate these warnings, you can rebuild OpenSSL for your architecture:
   ```bash
   make clean-openssl
   make build-openssl ARCH=$(uname -m)
   ```

3. For detailed debugging, you can run the tests manually:
   ```bash
   # On macOS
   DYLD_LIBRARY_PATH=/path/to/pcre/lib tests/unit/test_sayplease
   
   # On Linux
   LD_LIBRARY_PATH=/path/to/pcre/lib tests/unit/test_sayplease
   
   # On Windows (MinGW/MSYS2)
   PATH="/path/to/pcre/lib:$PATH" tests/unit/test_sayplease
   ```

## License

This project is licensed under the MIT License - see the LICENSE file for details.

## Contributing

Contributions are welcome! Please feel free to submit a Pull Request. 