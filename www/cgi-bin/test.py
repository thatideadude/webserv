#!/usr/bin/python3
import os

print("Content-Type: text/html")
print()
print("<html><body>")
print("<h1>Python CGI Test</h1>")
print("<p>Python is working through CGI!</p>")
print("<p>Request Method: " + os.environ.get('REQUEST_METHOD', '') + "</p>")
print("</body></html>")
