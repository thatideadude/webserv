#!/usr/bin/env python3
import os
import sys

# Get request method
request_method = os.environ.get('REQUEST_METHOD', 'GET')

# Set content type
print("Content-Type: text/html\r\n\r\n", end="")

# Start HTML response
print("<!DOCTYPE html>")
print("<html>")
print("<head>")
print("    <title>CGI Test Form</title>")
print("    <link rel=\"stylesheet\" href=\"../style.css\">")
print("</head>")
print("<body>")
print("    <div class=\"container\">")
print("        <h1>CGI Test Form</h1>")

if request_method == 'GET':
    # Display the form
    print("        <p>This is a GET request. Please fill out the form below:</p>")
    print("        <form method=\"POST\" action=\"/cgi-bin/test_form.py\">")
    print("            <label for=\"name\">Name:</label>")
    print("            <input type=\"text\" id=\"name\" name=\"name\" required><br><br>")
    print("            <label for=\"email\">Email:</label>")
    print("            <input type=\"email\" id=\"email\" name=\"email\" required><br><br>")
    print("            <label for=\"message\">Message:</label><br>")
    print("            <textarea id=\"message\" name=\"message\" rows=\"4\" cols=\"50\" required></textarea><br><br>")
    print("            <input type=\"submit\" value=\"Submit\">")
    print("            <input type=\"reset\" value=\"Reset\">")
    print("        </form>")
elif request_method == 'POST':
    # Process form data
    print("        <p>This is a POST request. Here's what you submitted:</p>")
    
    # Get content length
    content_length = int(os.environ.get('CONTENT_LENGTH', 0))
    
    # Read POST data
    post_data = sys.stdin.read(content_length) if content_length > 0 else ""
    
    # Parse the data (simple parsing for application/x-www-form-urlencoded)
    params = {}
    if post_data:
        pairs = post_data.split('&')
        for pair in pairs:
            if '=' in pair:
                key, value = pair.split('=', 1)
                # Simple URL decoding
                key = key.replace('+', ' ')
                value = value.replace('+', ' ')
                params[key] = value
    
    # Display submitted data
    print("        <ul>")
    for key, value in params.items():
        print(f"            <li><strong>{key}:</strong> {value}</li>")
    print("        </ul>")
    
    # Show all environment variables for debugging
    print("        <h2>Environment Variables:</h2>")
    print("        <pre>")
    for key in sorted(os.environ.keys()):
        print(f"{key}={os.environ[key]}")
    print("        </pre>")

print("    </div>")
print("</body>")
print("</html>")
