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

#include "Response.hpp"
#include "Request.hpp"
#include "ConfigParser.hpp"
#include "Webserver.hpp"
#include "Client.hpp"

#define BUFFER_SIZE 4096
#define BACKLOG 128
#define TIMEOUT_SECONDS 60

#define CHUNK_SIZE 512
#define MAX_EVENTS 1024

std::vector<std::string>	string_split(std::string str);
