# RoboNope-nginx: enforce robots.txt rules

[![Build and Test](https://github.com/raminf/RoboNope/actions/workflows/build.yml/badge.svg)](https://github.com/raminf/RoboNope-nginx/actions/workflows/build.yml)
[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![Nginx Version](https://img.shields.io/badge/nginx-1.24.0-brightgreen.svg)](https://nginx.org/)
[![Version](https://img.shields.io/badge/version-1.0.0-blue.svg)](https://github.com/raminf/RoboNope-nginx/releases)
[![Language](https://img.shields.io/badge/language-C-orange.svg)](https://en.wikipedia.org/wiki/C_(programming_language))
[![GitHub last commit](https://img.shields.io/github/last-commit/raminf/RoboNope)](https://github.com/raminf/RoboNope-nginx/commits/main)

![RoboNope](img/RoboNope.png)

`RoboNope-nginx` is a module designed to deny access to files specified in the robots.txt `Disallow` entries. It can serve randomly generated content to bots that ignore [robots.txt](https://developers.google.com/search/docs/crawling-indexing/robots/intro) rules. Ignore at your peril.

## Example

Web crawlers are supposed to honor `robots.txt` but there have been many reports of [companies ignoring them](https://mjtsai.com/blog/2024/06/24/ai-companies-ignoring-robots-txt/).

Let's assume your `robots.txt` file looks like this:

```
User-agent: *
Allow: /
Disallow: /norobots/
Disallow: /private/
Disallow: /admin/
Disallow: /secret-data/
Disallow: /internal/
```

A web crawler would download this and abide by the list of disallowed links. But what if they ignored the wishes of the site owner? _Is there anything you can do?_

- Add meta tags to each page: `<meta name="robots" content="noindex,nofollow">`.
- Add `nofollow` attributed to links: `<a href="http://destination.com/" rel="nofollow">link text</a>` 
- Make every link go through a javascript filter.
- Set up password access through `.htaccess`

The first two are voluntary and can be ignored, and the last two are a pain to set up and test.

Why not just enforce `robots.txt` and make it __mandatory__ instead of __optional__?

With `RoboNope-nginx`, if someone tries to access a private page, they get:

```
% curl http://localhost:8080/private/index.html
<html>
  <head>
    <style>
      .RRsNdyetNRjW { opacity: 0; position: absolute; top: -9999px; }
    </style>
  </head>
  <body>
    <div class="content">
      the platform seamlessly monitors integrated data requests. therefore the service manages network traffic. the website intelligently manages robust service endpoints. the system seamlessly analyzes optimized service endpoints. the platform dynamically handles secure network traffic. while our network validates cache entries.
    </div>
    <a href="/norobots/index.html" class="RRsNdyetNRjW">Important Information</a>
  </body>
</html>
```

To humans, this looks like this:

![Browser Page](img/browser-page.png)

But to a mis-behaving crawler, it offers a tantalizing link to follow (notice the link is invisible to humans): 

```
<a href="/norobots/index.html" class="RRsNdyetNRjW">Important Information</a>
```

Following that link, they'll receive a different file:

```
<html>
  <head>
    <style>
      .RRsNdyetNRjW { opacity: 0; position: absolute; top: -9999px; }
    </style>
  </head>
  <body>
    <div class="content">
      the platform seamlessly monitors integrated data requests. therefore the service manages network traffic. the website intelligently manages robust service endpoints. the system seamlessly analyzes optimized service endpoints. the platform dynamically handles secure network traffic. while our network validates cache entries.
    </div>
    <a href="/norobots/index.html" class="RRsNdyetNRjW">Important Information</a>
  </body>
</html>
```

This has a link to a different page (randomly selected from whatever has been explicitly disallowed inside `robots.txt`):

```
<a href="/norobots/index.html" class="RRsNdyetNRjW">Important Information</a>
```

And so on and so forth...

## Honeypot Link Configuration

The downside to this endless cat and mouse game is that your web-server may get pounded by a mis-behaving crawler, generating an endless series of links. As satisfying as this might be, _you_ are paying for this misbehavior. 

You can configure the module to direct crawlers to an educational resource instead of an endless loop by setting the `instructions_url` directive:

```
    # Instructions URL configuration
    # Uncomment the following line to direct crawlers to an educational resource
    # robonope_instructions_url "https://developers.google.com/search/docs/crawling-indexing/robots/intro";
```

When the `instructions_url` directive is set, the generated honeypot link will point to the specified URL:

```
<a href="https://developers.google.com/search/docs/crawling-indexing/robots/intro" class="wgUxnAjBuYDQ">Important Information</a>
```

If the `instructions_url` directive is not set, the module will generate internal honeypot links that lead to other disallowed paths.

## Logging

The system can maintain a log of mis-behaving bots in a local database (default is `SQLite` but also work-in-progress to use `DuckDB`).

To enable logging, simply set the `robonope_db_path` directive in your configuration:

```
    # Database logging configuration
    # Uncomment the following line to enable database logging
    # robonope_db_path /path/to/robonope.db;
```

When the database path is not set, logging is disabled.

You can run the test version and see for yourself what it stores:

```
sqlite3 demo/robonope.db .tables
```

## Why nginx?

According to [W3Techs](https://w3techs.com/technologies/overview/web_server) the top 5 most popular webservers as of March 2025 are:

- [nginx](https://nginx.org) (33.8%)
- [Apache](https://httpd.apache.org) (26.8%)
- [Cloudflare Server](https://www.cloudflare.com) (23.2%)
- [Litespeed](https://www.litespeedtech.com/products/litespeed-web-server) (14.5%)
- [Node.js](https://nodejs.org) (4.2%)

This first version has been tested with `nginx` v1.26. If there is demand, separate versions for other servers with the same functionality will also be released. This also includes [Wordpress robots.txt](https://docs.wpvip.com/security-controls/robots-txt/).

And of course, community contributions are most Welcome!

## Features

- Parses and enforces robots.txt rules
- Generates _dynamic_ content for disallowed paths
- Tracks bot requests in SQLite (or DuckDB -- _work in progress_) when database path is configured
- Supports both static and dynamic content generation
- Configurable caching for performance
- Honeypot link generation with configurable destination via `instructions_url`
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
git clone --recursive https://github.com/raminf/RoboNope-nginx.git
cd robonope-nginx

# Build the module
make
```

### Running the Demo

```bash
# Start the demo server (runs on port 8080)
make demo-start

# Test with a disallowed URL
make demo-test

# View logged requests (if database logging is enabled)
make demo-logs

# Stop the demo server
make demo-stop
```

### Demo Configuration Options

You can customize the demo environment using these variables:

```bash
# Enable database logging with custom location
DB_PATH=/tmp/robonope.db make demo-start

# Use DuckDB instead of SQLite (work in progress)
DB_ENGINE=duckdb make all demo-start

# Customize the instructions URL for honeypot links
INSTRUCTIONS_URL=https://your-custom-url.com make demo-start
```

### Configuration in nginx.conf

Add to your main nginx.conf:

```nginx
load_module modules/ngx_http_robonope_module.so;

http {
    # RoboNope configuration
    robonope_enable on;
    robonope_robots_path /path/to/robots.txt;
    
    # Optional: Enable database logging
    # robonope_db_path /path/to/database;
    
    # Optional: Set instructions URL for honeypot links
    # robonope_instructions_url "https://your-custom-url.com";
    
    # Optional rate limiting for disallowed paths
    limit_req_zone $binary_remote_addr zone=robonope_limit:10m rate=1r/s;
    
    server {
        # Apply rate limiting to disallowed paths
        location ~ ^/(norobots|private|admin|secret-data|internal)/ {
            limit_req zone=robonope_limit burst=5 nodelay;
            robonope_enable on;
        }
    }
}
```

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details. 

## Disclosure

Most of the project, the README, and the artwork was assisted by AI. Even the name was workshopped with an AI. 

![Magritte Pipe](img/MagrittePipe.jpg)
