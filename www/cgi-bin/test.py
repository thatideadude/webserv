#!/usr/bin/python3
import os
print("Content-Type: text/html")
print()
print("<html><body>")
print("<h1>CGI Test Working!</h1>")
print(f"<p>Port: {os.environ.get('SERVER_PORT', 'Unknown')}</p>")
print("</body></html>")
