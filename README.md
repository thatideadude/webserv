*This project has been created as part of the 42 curriculum by diogribe and marcemon.*

# Description

This project is a custom HTTP server written in C++98, built to satisfy the requirements of the 42 webserv subject. The goal is not to recreate an industrial server like NGINX, but to implement a small, correct, and testable web server that understands the basics of HTTP communication, static file serving, CGI execution, route matching, and file uploads.

The project was built around a simple but important idea: an HTTP server must stay non-blocking, react to socket readiness through a single event loop, and remain stable under normal traffic and edge cases. Instead of writing a monolithic program, the architecture separates request parsing, routing, CGI management, and response generation so that each part is easier to reason about and verify.

The result is a server that can handle a static website, manage multiple ports from a configuration file, serve error pages, process uploads, and run CGI scripts using standard POSIX mechanisms.

## Why this project exists

In the context of 42, the project teaches the essential principles behind modern web servers:

- sockets and listening ports
- request parsing and header handling
- routing based on URL paths
- method handling (GET, POST, DELETE, HEAD)
- CGI execution and environment setup
- non-blocking I/O with poll()
- response generation and status codes
- safe shutdown and failure handling

The main challenge is not just making the server respond to a request, but making it do so correctly, without blocking, and without violating the project constraints.

## Main features

- Multiple listening ports and server blocks
- Route-based matching for URL paths
- Static file hosting and autoindex listing
- Directory index resolution
- Redirection rules from the configuration file
- Support for GET, POST, DELETE, and HEAD methods
- File upload handling to a configured storage directory
- CGI execution for supported script extensions such as .py and .php
- Request body limits and server-side error pages
- Graceful handling of closed client connections

# Instructions

## Requirements

This project must be compiled under the following constraints:

- C++98 standard
- `-Wall -Wextra -Werror` flags
- a valid Makefile with `NAME`, `all`, `clean`, `fclean`, and `re`
- configuration file passed as argument to the program

## Compilation

From the project root, run:

```bash
make
```

For a full rebuild:

```bash
make re
```

To remove generated object files:

```bash
make clean
```

To remove the binary as well:

```bash
make fclean
```

## Running the server

```bash
./webserv [configuration file]
```

Example:

```bash
./webserv minimal.conf
```

To test the server with the provided validation script:

```bash
bash ./test.sh localhost
```

## Example requests

A few standard interactions with the server look like this:

```bash
curl http://localhost:8080/
curl -I http://localhost:8080/
curl -X POST -d "hello=world" http://localhost:8080/
curl -X DELETE http://localhost:8080/files/delete_me.txt
curl -X POST -F "file=@example.txt" http://localhost:8081/upload/
```

# Configuration

The server reads a custom nginx-like configuration file. It supports:

- multiple `server { ... }` blocks
- listening ports and host names
- `location` blocks for route matching
- root directories for each location
- default index files and autoindex configuration
- allowed methods per route
- redirect rules
- upload storage directories
- CGI extension configuration
- body size limits and error pages

The provided `minimal.conf` file is a working example used for testing and demonstration.

# Project structure

- `main.cpp` – entry point and signal handling
- `Webserver.cpp` / `Webserver.hpp` – socket setup, polling loop, request dispatch
- `Router.cpp` / `Router.hpp` – route resolution and file path building
- `Request.cpp` / `Request.hpp` – HTTP request parsing and validation
- `Response.cpp` / `Response.hpp` – HTTP response generation
- `CGIHandler.cpp` / `CGIHandler.hpp` – CGI environment and process management
- `Parser.cpp` / `Parser.hpp` – configuration parsing
- `minimal.conf` – sample configuration
- `www/` – static test website and CGI assets

# Resources

## References

- RFC 7230 – HTTP/1.1 Message Syntax and Routing
- RFC 7231 – HTTP/1.1 Semantics and Content
- RFC 7232 – HTTP Conditional Requests
- NGINX documentation for routing, server blocks, redirects, and upload handling
- 42 project subject and associated notes on non-blocking I/O and HTTP server behavior

## AI usage

AI tools were used during development for the following purposes:

- understanding the non-blocking socket architecture and event loop design
- debugging request parsing and header normalization issues
- validating route selection and redirect behavior against expected HTTP semantics
- reviewing CGI execution flow and relative working-directory logic
- checking body-size validation and protocol edge cases before final testing

# Notes

This project follows the constraints imposed by the subject: C++98 compliance, a clean Makefile, non-blocking networking, a single poll-driven event loop, route-based configuration, and robust support for static content, uploads, redirects, and CGI. It is intentionally limited to the features required by the assignment while remaining compatible with real HTTP clients and test scripts.
