#!/bin/bash
# ============================================
# webserv test script for the current config
# Usage: ./test.sh [host]
# Assumes the server is already running with minimal.conf
# ============================================

HOST="${1:-localhost}"
PASS=0
FAIL=0

GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m'

# check_status <name> <expected_code> <curl args...>
check_status() {
    local name="$1"
    local expected="$2"
    shift 2
    local code
    code=$(curl -sS -o /tmp/webserv_test_body -w "%{http_code}" "$@")
    if [ "$code" == "$expected" ]; then
        echo -e "${GREEN}PASS${NC}  $name (got $code)"
        PASS=$((PASS+1))
    else
        echo -e "${RED}FAIL${NC}  $name (expected $expected, got $code)"
        FAIL=$((FAIL+1))
    fi
}

# check_contains <name> <expected_substring> <curl args...>
check_contains() {
    local name="$1"
    local needle="$2"
    shift 2
    local body
    body=$(curl -sS "$@")
    if echo "$body" | grep -q "$needle"; then
        echo -e "${GREEN}PASS${NC}  $name (found '$needle')"
        PASS=$((PASS+1))
    else
        echo -e "${RED}FAIL${NC}  $name (missing '$needle')"
        FAIL=$((FAIL+1))
    fi
}

section() {
    echo ""
    echo -e "${YELLOW}=== $1 ===${NC}"
}

# Clean upload directories used by the current config.
mkdir -p ./www/files/uploads ./www/redirects/uploads
rm -f ./www/files/delete_me.txt ./www/files/uploads/* ./www/redirects/uploads/*

# ============================================
# SERVER 1: port 8080 - Main website
# ============================================
section "Server 1 (8080) - Main site"

check_status  "GET / (index.html)"                     200 "http://$HOST:8080/"
check_status  "GET /test.html"                         200 "http://$HOST:8080/test.html"
check_status  "GET nonexistent -> 404"                 404 "http://$HOST:8080/doesnotexist.html"
check_status  "HEAD /"                                 200 -I "http://$HOST:8080/"
check_status  "GET /files/ (autoindex on)"            200 "http://$HOST:8080/files/"
check_contains "Autoindex lists files"                 "uploads" "http://$HOST:8080/files/"
check_status  "POST / (root echo)"                     200 -X POST -d "hello=world" "http://$HOST:8080/"
check_status  "DELETE on directory -> 403"             403 -X DELETE "http://$HOST:8080/files/"
check_status  "POST /files/ (upload accepted)"         201 -X POST -d "x" "http://$HOST:8080/files/"

# DELETE a real file that we create explicitly.
echo "temp content" > /tmp/webserv_delete_target.txt
cp /tmp/webserv_delete_target.txt ./www/files/delete_me.txt 2>/dev/null
check_status  "DELETE /files/delete_me.txt"            204 -X DELETE "http://$HOST:8080/files/delete_me.txt"
check_status  "DELETE again -> 404 (already gone)"     404 -X DELETE "http://$HOST:8080/files/delete_me.txt"

check_status  "Host: main.webserv.local"               200 -H "Host: main.webserv.local" "http://$HOST:8080/"
check_status  "Redirect 301 helper"                    301 "http://$HOST:8080/redirects/redirect-301"
check_status  "Redirect 302 helper"                    302 "http://$HOST:8080/redirects/redirect-302"

# ============================================
# SERVER 2: port 8081 - Redirects and upload endpoint
# ============================================
section "Server 2 (8081) - Redirect + upload"

check_status  "GET / (redirect)"                       301 "http://$HOST:8081/"
check_status  "Host: redirects.webserv.local"          301 -H "Host: redirects.webserv.local" "http://$HOST:8081/"
check_status  "GET /upload/ (no GET allowed)"          405 "http://$HOST:8081/upload/"
check_status  "POST /upload/ (text/plain)"             201 -X POST --data-binary "some file content" \
              -H "Content-Type: text/plain" "http://$HOST:8081/upload/"
check_status  "GET /upload/ still 405 after upload"    405 "http://$HOST:8081/upload/"

echo "multipart test data" > /tmp/webserv_upload_test.txt
check_status  "POST /upload/ multipart file"           201 -X POST -F "file=@/tmp/webserv_upload_test.txt" \
              "http://$HOST:8081/upload/"

# ============================================
# SERVER 3: port 8082 - File server
# ============================================
section "Server 3 (8082) - File server"

check_status  "GET / (autoindex)"                      200 "http://$HOST:8082/"
check_contains "Autoindex lists uploads"               "uploads" "http://$HOST:8082/"
check_status  "Host: files.webserv.local"              200 -H "Host: files.webserv.local" "http://$HOST:8082/"

# ============================================
# SERVER 4: port 8083 - CGI server
# ============================================
section "Server 4 (8083) - CGI server"

check_status  "GET /cgi-bin/test.py"                  200 "http://$HOST:8083/cgi-bin/test.py"
check_status  "GET /cgi-bin/test.php"                 200 "http://$HOST:8083/cgi-bin/test.php"
check_status  "GET /cgi-bin/envtest.py"               200 "http://$HOST:8083/cgi-bin/envtest.py"
check_status  "POST /cgi-bin/test.py with body"       200 -X POST -d "foo=bar" "http://$HOST:8083/cgi-bin/test.py"
check_status  "GET / (no CGI here) -> 403"            403 "http://$HOST:8083/"
check_status  "GET /cgi-bin/nonexistent.py -> 404"    404 "http://$HOST:8083/cgi-bin/nope.py"

# ============================================
# Cross-cutting checks
# ============================================
section "General / edge cases"

check_status  "Wrong port for hostname falls back to first server" 200 -H "Host: nope.local" "http://$HOST:8080/"
check_status  "Empty GET with Connection: close header" 200 -H "Connection: close" "http://$HOST:8080/"
check_status  "POST small body under limit"          200 -X POST -d "small body" "http://$HOST:8080/"

# ============================================
# Subject-level protocol checks
# ============================================
section "Subject-level protocol checks"

# Oversized POST body: should be rejected with 413, not silently accepted.
python3 - <<'PY' > /tmp/webserv_oversized_body.bin
import sys
sys.stdout.buffer.write(b'A' * (11 * 1024 * 1024))
PY
check_status  "POST 11MB body on port 8080 -> 413"    413 --data-binary @/tmp/webserv_oversized_body.bin "http://$HOST:8080/"

# Valid chunked upload must be accepted by the upload route.
status_code=$(python3 - "$HOST" 8081 <<'PY'
import socket, sys
host = sys.argv[1]
port = int(sys.argv[2])
req = (
    "POST /upload/ HTTP/1.1\r\n"
    "Host: localhost\r\n"
    "Transfer-Encoding: chunked\r\n"
    "Content-Type: text/plain\r\n"
    "Connection: close\r\n\r\n"
    "5\r\nhello\r\n"
    "5\r\nworld\r\n"
    "0\r\n\r\n"
)
s = socket.create_connection((host, port), timeout=5)
s.sendall(req.encode('latin1'))
raw = b''
while True:
    try:
        chunk = s.recv(65535)
    except socket.timeout:
        break
    if not chunk:
        break
    raw += chunk
    if b'\r\n\r\n' in raw and raw.startswith(b'HTTP/'):
        break
print(raw.split(b' ', 2)[1].decode('ascii'))
PY
)
if [ "$status_code" = "201" ]; then echo -e "${GREEN}PASS${NC}  Valid chunked upload accepted (got 201)"; PASS=$((PASS+1)); else echo -e "${RED}FAIL${NC}  Valid chunked upload accepted (expected 201, got $status_code)"; FAIL=$((FAIL+1)); fi

# Malformed chunked body should be rejected instead of silently treated as valid.
status_code=$(python3 - "$HOST" 8081 <<'PY'
import socket, sys
host = sys.argv[1]
port = int(sys.argv[2])
req = (
    "POST /upload/ HTTP/1.1\r\n"
    "Host: localhost\r\n"
    "Transfer-Encoding: chunked\r\n"
    "Content-Type: text/plain\r\n"
    "Connection: close\r\n\r\n"
    "Z\r\nhello\r\n"
    "0\r\n\r\n"
)
s = socket.create_connection((host, port), timeout=5)
s.sendall(req.encode('latin1'))
raw = b''
while True:
    try:
        chunk = s.recv(65535)
    except socket.timeout:
        break
    if not chunk:
        break
    raw += chunk
    if b'\r\n\r\n' in raw and raw.startswith(b'HTTP/'):
        break
print(raw.split(b' ', 2)[1].decode('ascii'))
PY
)
if [ "$status_code" = "400" ]; then echo -e "${GREEN}PASS${NC}  Malformed chunked upload rejected (got 400)"; PASS=$((PASS+1)); else echo -e "${RED}FAIL${NC}  Malformed chunked upload rejected (expected 400, got $status_code)"; FAIL=$((FAIL+1)); fi

# ============================================
# Concurrency sanity check
# ============================================
section "Concurrency sanity check"
for i in 1 2 3 4 5; do
    curl -sS -o /dev/null "http://$HOST:8080/" &
done
wait
echo "Fired 5 parallel requests to :8080 (check server didn't crash)"
check_status "Server still alive after parallel burst" 200 "http://$HOST:8080/"

# ============================================
# Summary
# ============================================
echo ""
echo "============================================"
echo -e "Results: ${GREEN}$PASS passed${NC}, ${RED}$FAIL failed${NC}"
echo "============================================"

rm -f /tmp/webserv_test_body /tmp/webserv_delete_target.txt /tmp/webserv_upload_test.txt /tmp/webserv_oversized_body.bin

if [ "$FAIL" -gt 0 ]; then
    exit 1
fi
exit 0
