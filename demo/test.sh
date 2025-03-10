#!/bin/bash

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Default values
URL=${1:-"http://localhost:8080/secret/test.html"}
USER_AGENT="RoboNopeTestBot/1.0"
OUTPUT_DIR="demo/output"
DB_FILE="demo/robonope.db"

# Create output directory if it doesn't exist
mkdir -p "$OUTPUT_DIR"

echo -e "${BLUE}Testing RoboNope module with URL: $URL${NC}"
echo -e "${BLUE}Using User-Agent: $USER_AGENT${NC}"

# Send request and save response
echo -e "\n${GREEN}Sending test request...${NC}"
curl -A "$USER_AGENT" -s "$URL" -o "$OUTPUT_DIR/response.html"

# Check if response was received
if [ -s "$OUTPUT_DIR/response.html" ]; then
    echo -e "${GREEN}Response saved to $OUTPUT_DIR/response.html${NC}"
    echo -e "\n${BLUE}First few lines of response:${NC}"
    head -n 5 "$OUTPUT_DIR/response.html"
else
    echo -e "${RED}No response received or empty response${NC}"
    exit 1
fi

# Check database entries
if [ -f "$DB_FILE" ]; then
    echo -e "\n${BLUE}Checking database entries...${NC}"
    echo -e "${GREEN}Latest entries in robonope.db:${NC}"
    sqlite3 "$DB_FILE" "SELECT * FROM bot_requests ORDER BY timestamp DESC LIMIT 5;"
else
    echo -e "\n${RED}Database file $DB_FILE not found${NC}"
    exit 1
fi

echo -e "\n${GREEN}Test completed successfully${NC}" 