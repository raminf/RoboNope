#!/bin/bash
# Test script for RoboNope with redirect to instructions (instructions_url set)

# Set up colors for output
GREEN='\033[0;32m'
RED='\033[0;31m'
NC='\033[0m' # No Color

# Function to print success/failure messages
print_result() {
    if [ $1 -eq 0 ]; then
        echo -e "${GREEN}✓ $2${NC}"
    else
        echo -e "${RED}✗ $2${NC}"
        exit 1
    fi
}

# Ensure we're in the project root
cd "$(dirname "$0")/.." || exit 1

echo "Testing RoboNope with redirect to instructions (instructions_url set)..."

# Stop any running demo
make demo-stop >/dev/null 2>&1

# Set custom instructions URL for testing
TEST_INSTRUCTIONS_URL="https://example.com/robots-instructions"

# Start demo with default configuration
make demo-start >/dev/null 2>&1
print_result $? "Started demo server"

# Modify the nginx.conf file to enable instructions_url
sed -i.bak 's/# robonope_instructions_url/robonope_instructions_url/g' build/demo/conf/nginx.conf
sed -i.bak "s|\"https://developers.google.com/search/docs/crawling-indexing/robots/intro\"|\"$TEST_INSTRUCTIONS_URL\"|g" build/demo/conf/nginx.conf

# Restart nginx to apply changes
cd build/demo && ../../build/nginx-1.24.0/objs/nginx -p . -c conf/nginx.conf -s reload >/dev/null 2>&1
cd ../.. # Return to project root
print_result $? "Applied configuration changes"

# Wait for server to fully start
sleep 2

# Test 1: Check if we get a response from a disallowed URL
RESPONSE=$(curl -s -A "Googlebot" "http://localhost:8080/private/index.html")
if [ -n "$RESPONSE" ]; then
    print_result 0 "Received response from disallowed URL"
else
    print_result 1 "Failed to get response from disallowed URL"
fi

# Debug output
echo "Response content (first 10 lines):"
echo "$RESPONSE" | head -10

# Test 2: Check if the response contains a link to the instructions URL
if echo "$RESPONSE" | grep -q "<a href=\"$TEST_INSTRUCTIONS_URL\""; then
    print_result 0 "Response contains a link to the instructions URL"
else
    # Try with the default Google URL if custom URL wasn't applied
    if echo "$RESPONSE" | grep -q '<a href="https://developers.google.com/search/docs/crawling-indexing/robots/intro"'; then
        print_result 0 "Response contains a link to the default instructions URL"
    else
        print_result 1 "Response does not contain a link to any instructions URL"
    fi
fi

# Check if instructions_url is set in the nginx.conf file
INSTRUCTIONS_URL=$(grep "robonope_instructions_url" build/demo/conf/nginx.conf | grep -v "#" | awk '{print $2}' | tr -d '";')
echo "Instructions URL in config: $INSTRUCTIONS_URL"

# Test 3: Check if the link is to the expected URL type
if [ -z "$INSTRUCTIONS_URL" ]; then
    # If instructions_url is not set, expect an internal link
    if echo "$RESPONSE" | grep -q '<a href="http'; then
        print_result 1 "Link is to an external URL (should be internal)"
    else
        print_result 0 "Link is to an internal path as expected"
    fi
else
    # If instructions_url is set, expect the link to match the instructions URL
    if echo "$RESPONSE" | grep -q "<a href=\"$INSTRUCTIONS_URL\""; then
        print_result 0 "Link is to the instructions URL as expected"
    else
        print_result 1 "Link does not match the instructions URL"
    fi
fi

# Test 4: Check if the link has the hidden CSS class
CSS_CLASS=$(echo "$RESPONSE" | grep -o '<a href="[^"]*" class="[^"]*">' | grep -o 'class="[^"]*"')
if [ -n "$CSS_CLASS" ]; then
    print_result 0 "Link has a CSS class for hiding"
else
    print_result 1 "Link does not have a CSS class for hiding"
fi

# Test 5: Check if the database logging is disabled by default
# Get the database path from the nginx.conf file
DB_PATH=$(grep "robonope_db_path" build/demo/conf/nginx.conf | grep -v "#" | awk '{print $2}' | tr -d ';')
echo "Database path in config: $DB_PATH"
if [ -z "$DB_PATH" ]; then
    print_result 0 "Database logging is disabled by default as expected"
else
    # If DB_PATH is set, check if the database file exists
    if [ -f "$DB_PATH" ]; then
        DB_ENTRIES=$(sqlite3 "$DB_PATH" "SELECT COUNT(*) FROM requests WHERE url LIKE '%/private/index.html%';" 2>/dev/null || echo "0")
        if [ "$DB_ENTRIES" -gt 0 ]; then
            print_result 1 "Database logging is enabled (should be disabled by default)"
        else
            print_result 0 "Database logging is disabled as expected"
        fi
    else
        print_result 0 "Database file does not exist as expected"
    fi
fi

# Clean up
make demo-stop >/dev/null 2>&1 || true
echo -e "${GREEN}✓ Stopped demo server${NC}"

echo -e "${GREEN}All tests passed!${NC}"
exit 0 