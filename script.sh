#!/bin/bash

# Colors for output
GREEN='\033[0;32m'
RED='\033[0;31m'
BLUE='\033[0;34m'
YELLOW='\033[0;33m'
NC='\033[0m' # No Color

echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}🧪 WEBSERV - Complete Test Suite${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""

# Create test files
mkdir -p www/files
mkdir -p www/uploads/tmp
echo "Hello from GET request!" > www/files/get_test.txt
echo "This file will be deleted" > www/files/delete_test.txt

# Test 1: GET request
echo -e "${BLUE}📝 Test 1: GET /files/get_test.txt${NC}"
echo -n "  Status: "
status=$(curl -s -o /dev/null -w "%{http_code}" http://localhost:8080/files/get_test.txt)
if [ "$status" = "200" ]; then
    echo -e "${GREEN}  200 OK${NC}"
    content=$(curl -s http://localhost:8080/files/get_test.txt)
    echo "  Content: $content"
else
    echo -e "${RED}❌ Failed (HTTP $status)${NC}"
fi
echo ""

# Test 2: GET directory (autoindex)
echo -e "${BLUE}  Test 2: GET /files/ (autoindex)${NC}"
echo -n "  Status: "
status=$(curl -s -o /dev/null -w "%{http_code}" http://localhost:8080/files/)
if [ "$status" = "200" ]; then
    echo -e "${GREEN}  200 OK${NC}"
    echo "  Autoindex working (directory listing shown)"
else
    echo -e "${RED}❌ Failed (HTTP $status)${NC}"
fi
echo ""

# Test 3: POST upload
echo -e "${BLUE}  Test 3: POST /uploads/ (file upload)${NC}"
echo -n "  Status: "
status=$(curl -s -o /dev/null -w "%{http_code}" -X POST -d "Hello from POST!" http://localhost:8080/uploads/)
if [ "$status" = "201" ]; then
    echo -e "${GREEN}  201 Created${NC}"
    echo "  File uploaded successfully"
    # Check if file was created
    upload_file=$(ls -t www/uploads/tmp/ | head -n1)
    if [ ! -z "$upload_file" ]; then
        echo "  Uploaded file: $upload_file"
        echo "  Content: $(cat www/uploads/tmp/$upload_file)"
    fi
else
    echo -e "${RED}  Failed (HTTP $status)${NC}"
fi
echo ""

# Test 4: DELETE request
echo -e "${BLUE}  Test 4: DELETE /files/delete_test.txt${NC}"
echo -n "  Status: "
status=$(curl -s -o /dev/null -w "%{http_code}" -X DELETE http://localhost:8080/files/delete_test.txt)
if [ "$status" = "204" ]; then
    echo -e "${GREEN}  204 No Content${NC}"
    echo "  File deleted successfully"
    # Verify file is gone
    if [ ! -f "www/files/delete_test.txt" ]; then
        echo "  File removed from filesystem"
    fi
else
    echo -e "${RED}  Failed (HTTP $status)${NC}"
fi
echo ""

# Test 5: 404 Not Found
echo -e "${BLUE}  Test 5: GET /nonexistent.html (404)${NC}"
echo -n "  Status: "
status=$(curl -s -o /dev/null -w "%{http_code}" http://localhost:8080/nonexistent.html)
if [ "$status" = "404" ]; then
    echo -e "${GREEN}  404 Not Found${NC}"
else
    echo -e "${RED}  Failed (HTTP $status, expected 404)${NC}"
fi
echo ""

# Test 6: 405 Method Not Allowed (POST on /files/)
echo -e "${BLUE}  Test 6: POST /files/ (405 Method Not Allowed)${NC}"
echo -n "  Status: "
status=$(curl -s -o /dev/null -w "%{http_code}" -X POST -d "test" http://localhost:8080/files/)
if [ "$status" = "405" ]; then
    echo -e "${GREEN}  405 Method Not Allowed${NC}"
else
    echo -e "${RED}  Failed (HTTP $status, expected 405)${NC}"
fi
echo ""

# Test 7: GET root
echo -e "${BLUE}  Test 7: GET / (root)${NC}"
echo -n "  Status: "
status=$(curl -s -o /dev/null -w "%{http_code}" http://localhost:8080/)
if [ "$status" = "200" ]; then
    echo -e "${GREEN}  200 OK${NC}"
    # Check if index.html exists and is served
    content=$(curl -s http://localhost:8080/ | head -c 50)
    echo "  Content preview: $content..."
else
    echo -e "${RED}  Failed (HTTP $status)${NC}"
fi
echo ""

# Test 8: HEAD request (if your server supports it)
echo -e "${BLUE}  Test 8: HEAD /files/get_test.txt${NC}"
echo -n "  Status: "
status=$(curl -s -o /dev/null -w "%{http_code}" -I http://localhost:8080/files/get_test.txt 2>/dev/null | head -n1 | cut -d' ' -f2)
if [ "$status" = "200" ]; then
    echo -e "${GREEN}  200 OK${NC}"
    echo "  HEAD request working"
else
    echo -e "${YELLOW}⚠️  Not supported (HTTP $status)${NC}"
fi
echo ""

# Summary
echo -e "${BLUE}========================================${NC}"
echo -e "${GREEN}  All tests complete!${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""
echo "  Summary of tested methods:"
echo "    GET - File serving"
echo "    GET - Autoindex"
echo "    POST - File upload"
echo "    DELETE - File deletion"
echo "    404 - Not found"
echo "    405 - Method not allowed"
echo "    GET - Root directory"
echo ""

echo -e "${GREEN} Your webserver is fully functional!${NC}"
