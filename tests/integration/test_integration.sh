#!/bin/bash

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
NC='\033[0m'

# Test configuration
TEST_PORT=8888
TEST_HOST=localhost
NGINX_BIN=nginx
CONFIG_FILE=nginx_test.conf
ROBOTS_FILE=robots.txt
DB_FILE=test.db

# Setup function
setup() {
    echo "Setting up test environment..."
    
    # Create test configuration
    cat > $CONFIG_FILE <<EOL
worker_processes 1;
error_log /dev/stderr debug;
pid nginx.pid;

load_module modules/ngx_http_sayplease_module.so;

events {
    worker_connections 1024;
}

http {
    access_log /dev/stdout combined;

    sayplease_enable on;
    sayplease_robots_path $PWD/$ROBOTS_FILE;
    sayplease_db_path $PWD/$DB_FILE;
    sayplease_dynamic_content on;
    sayplease_cache_ttl 3600;
    sayplease_use_lorem_ipsum on;

    server {
        listen $TEST_PORT;
        server_name localhost;

        location / {
            root $PWD/www;
            index index.html;
        }

        location /norobots/ {
            sayplease_enable on;
        }
    }
}
EOL

    # Create test robots.txt
    cat > $ROBOTS_FILE <<EOL
User-agent: *
Allow: /
Disallow: /norobots/

User-agent: Googlebot
Allow: /
Disallow: /norobots/
EOL

    # Create test content
    mkdir -p www/public www/norobots
    echo "Public content" > www/public/index.html
    echo "Secret content" > www/norobots/secret.html

    # Start nginx
    $NGINX_BIN -c $PWD/$CONFIG_FILE
    sleep 1
}

# Cleanup function
cleanup() {
    echo "Cleaning up..."
    $NGINX_BIN -c $PWD/$CONFIG_FILE -s stop
    rm -f $CONFIG_FILE $ROBOTS_FILE nginx.pid
    rm -rf www
    rm -f $DB_FILE
}

# Test functions
test_public_url() {
    echo "Testing public URL access..."
    response=$(curl -s http://$TEST_HOST:$TEST_PORT/public/index.html)
    if [[ "$response" == "Public content" ]]; then
        echo -e "${GREEN}✓ Public URL test passed${NC}"
        return 0
    else
        echo -e "${RED}✗ Public URL test failed${NC}"
        return 1
    fi
}

test_blocked_url() {
    echo "Testing blocked URL access..."
    response=$(curl -s -A "Googlebot" http://$TEST_HOST:$TEST_PORT/norobots/secret.html)
    if [[ "$response" == *"Lorem ipsum"* ]] && [[ "$response" != *"Secret content"* ]]; then
        echo -e "${GREEN}✓ Blocked URL test passed${NC}"
        return 0
    else
        echo -e "${RED}✗ Blocked URL test failed${NC}"
        return 1
    fi
}

test_honeypot_links() {
    echo "Testing honeypot link generation..."
    response=$(curl -s -A "Googlebot" http://$TEST_HOST:$TEST_PORT/norobots/secret.html)
    if [[ "$response" == *"class=\"honeypot\""* ]] && [[ "$response" == *"href=\"/norobots/"* ]]; then
        echo -e "${GREEN}✓ Honeypot link test passed${NC}"
        return 0
    else
        echo -e "${RED}✗ Honeypot link test failed${NC}"
        return 1
    fi
}

test_database_logging() {
    echo "Testing database logging..."
    sleep 1  # Wait for DB write
    if [[ -f "$DB_FILE" ]]; then
        count=$(sqlite3 "$DB_FILE" "SELECT COUNT(*) FROM requests WHERE url LIKE '%/norobots/%';")
        if [[ "$count" -gt 0 ]]; then
            echo -e "${GREEN}✓ Database logging test passed${NC}"
            return 0
        fi
    fi
    echo -e "${RED}✗ Database logging test failed${NC}"
    return 1
}

test_caching() {
    echo "Testing response caching..."
    # Make first request
    response1=$(curl -s -A "Googlebot" http://$TEST_HOST:$TEST_PORT/norobots/secret.html)
    # Make second request
    response2=$(curl -s -A "Googlebot" http://$TEST_HOST:$TEST_PORT/norobots/secret.html)
    
    if [[ "$response1" == "$response2" ]]; then
        echo -e "${GREEN}✓ Caching test passed${NC}"
        return 0
    else
        echo -e "${RED}✗ Caching test failed${NC}"
        return 1
    fi
}

# Run tests
run_tests() {
    local failed=0
    
    test_public_url || ((failed++))
    test_blocked_url || ((failed++))
    test_honeypot_links || ((failed++))
    test_database_logging || ((failed++))
    test_caching || ((failed++))
    
    echo "-------------------"
    if [[ $failed -eq 0 ]]; then
        echo -e "${GREEN}All tests passed!${NC}"
        exit 0
    else
        echo -e "${RED}$failed test(s) failed${NC}"
        exit 1
    fi
}

# Main execution
trap cleanup EXIT
setup
run_tests 