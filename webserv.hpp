#pragma once

#include <iostream>
#include <fstream>
#include <cerrno>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <cstring>
#include <cstdlib>
#include <ctime>
#include <climits>

#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/poll.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <dirent.h>
#include <signal.h>
#include <errno.h>

#include "Parser.hpp"
#include "CGIHandler.hpp"
#include "Router.hpp"
#include "Request.hpp"
#include "Response.hpp"
#include "Client.hpp"
#include "Webserver.hpp"

#define BUFFER_SIZE 4096
#define BACKLOG 128
#define TIMEOUT_SECONDS 60

#define CHUNK_SIZE 512
#define MAX_EVENTS 1024

extern volatile sig_atomic_t	g_shutdown;

