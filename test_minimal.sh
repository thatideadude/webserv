#!/bin/bash

GREEN='\033[0;32m'
RED='\033[0;31m'
BLUE='\033[0;34m'
NC='\033[0m'

echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}🧪 Testing Minimal Webserv Config${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""

# Function to test endpoint
test_endpoint() {
    local name=$1
    local url=$2
    local expected=$3
    
    echo -n "📝 $name ... "
    response=$(curl -s -o /dev/null -w "%{http_code}" "$url" 2>/dev/null)
    
    if [ "$response" == "$expected" ]; then
        echo -e "${GREEN}✅ PASS${NC} (HTTP $response)"
        return 0
    else
        echo -e "${RED}❌ FAIL${NC} (HTTP $response, expected $expected)"
        return 1
    fi
}

# Run tests
echo -e "${BLUE}Running tests on http://localhost:8080${NC}"
echo ""

test_endpoint "Root (GET /)" "http://localhost:8080/" "200"
test_endpoint "Test page (GET /test.html)" "http://localhost:8080/test.html" "200"
test_endpoint "404 error (GET /nonexistent)" "http://localhost:8080/nonexistent" "404"
test_endpoint "Autoindex (GET /files/)" "http://localhost:8080/files/" "200"
test_endpoint "File in directory (GET /files/file1.txt)" "http://localhost:8080/files/file1.txt" "200"

# Optional: POST test
echo -n "📝 POST request ... "
response=$(curl -s -o /dev/null -w "%{http_code}" -X POST -d "test=data" http://localhost:8080/ 2>/dev/null)
if [ "$response" == "200" ]; then
    echo -e "${GREEN}✅ PASS${NC} (HTTP $response)"
else
    echo -e "${RED}❌ FAIL${NC} (HTTP $response, expected 200)"
fi

echo ""
echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}✅ Test complete!${NC}"
echo -e "${BLUE}========================================${NC}"
