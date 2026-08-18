#include "webserv.hpp"

Webserver::Webserver(std::vector<Server> &servers) : _configs(servers)
{
	std::cout << "Webserver constructor called\n";
}

Webserver::Webserver(const Webserver &other) : _configs(other._configs)
{
	if (this != &other)
		*this = other;
	std::cout << "Webserver copy constructor called\n";
}

Webserver	&Webserver::operator=(const Webserver &other)
{
	if (this != &other)
	{
		// Copy all members one by one
		_configs = other._configs;
		_server_sockets = other._server_sockets;
		_poll_fds = other._poll_fds;
		_clients = other._clients;
		_running = other._running;
	}
	return (*this);
}

Webserver::~Webserver(void)
{
	std::cout << "Webserver destructor called\n";
	_cleanup();
}

void	Webserver::_updatePollEvents(int fd, short events)
{
	size_t	i = 0;

	while (i < _poll_fds.size())
	{
		if (_poll_fds[i].fd == fd)
		{
			_poll_fds[i].events = events;
			return ;
		}
		++i;
	}
}

void	Webserver::_processRequest(int client_fd)
{
	Client	*client = _clients[client_fd];
	if (!client)
		return ;
	std::cout << "Building response for client " << client_fd << std::endl;

	Request		&request = client->getRequest();
	std::string	method = request.getMethod();
	std::string	uri = request.getUri();
	std::string	host_header = request.getHeader("Host");
	Server		*server = _findServer(request.getHeader("Host"));
	if (!server)
	{
		_sendErrorResponse(client, 400, "Bad Request - No server found");
		return ;
	}
	std::cout << "  Available locations:" << std::endl;
	size_t	i = 0;
	while (i < server->locations.size())
		std::cout << "	'" << server->locations[i++].path << "'" << std::endl;
	std::cout << "  URI: '" << uri << "'" << std::endl;
	i = 0;
	while (i < server->locations.size())
	{
		std::cout << "	Location: '" << server->locations[i].path << "'" << std::endl;
		std::cout << "	  uri.find(path) = " << uri.find(server->locations[i].path) << std::endl;
		++i;
	}
	const Location	*location = _router.findLocation(*server, uri);
	if (!location)
	{
		_sendErrorResponse(client, 404, "Not Found - No matching location");
		return ;
	}
	std::cout << "  Matched location: '" << location->path << "'" << std::endl;
	std::cout << "  upload_store: '" << location->upload_store << "'" << std::endl;
	if (!_router.isMethodAllowed(*location, method))
	{
		_sendErrorResponse(client, 405, "Method Not Allowed");
		return ;
	}
	std::string		fs_path = _router.buildPath(*location, uri);
	if (method == "GET")
		_handleGetRequest(client, (Location *)location, fs_path, uri);
	else if (method == "POST")
		_handlePostRequest(client, (Location *)location);
	else if (method == "DELETE")
		_handleDeleteRequest(client, fs_path);
	else if (method == "HEAD")
		_handleHeadRequest(client, *location, fs_path);
	else
		_sendErrorResponse(client, 405, "Method Not Allowed");
	const Location *loc = _router.findLocation(*server, uri);
	if (!loc)
	{
		_sendErrorResponse(client, 404, "Not Found - No matching location");
		return ;
	}
	
	std::cout << "  Matched location: '" << location->path << "'" << std::endl;
	std::cout << "  upload_store: '" << location->upload_store << "'" << std::endl;
}

void	Webserver::_cleanup(void)
{
	std::map<int, Client *>::iterator client_it = _clients.begin();
	while (client_it != _clients.end())
	{
		if (client_it->second)
			delete client_it->second;
		close(client_it->first);
		++client_it;
	}
	_clients.clear();

	std::map<int, Server>::iterator server_it = _server_sockets.begin();
	while (server_it != _server_sockets.end())
	{
		close(server_it->first);
		++server_it;
	}
	_server_sockets.clear();
	_poll_fds.clear();
	_running = false;
}

void	Webserver::_checkTimeouts(void)
{
	time_t now = time(NULL);

	std::map<int, Client *>::iterator it = _clients.begin();
	while (it != _clients.end())
	{
		Client *client = it->second;
		if (client && (now - client->getLastActivity() > TIMEOUT_SECONDS))
		{
			std::cout << "Client " << it->first << " timed out" << std::endl;
			int		fd	 = it->first;
			_removeClient(fd);
		}
		++it;
	}
}
void	Webserver::_removeClient(int client_fd)
{
	std::vector<struct pollfd>::iterator it = _poll_fds.begin();

	std::cout << "  _removeClient called for fd " << client_fd << std::endl;
	while (it != _poll_fds.end())
	{
		if (it->fd == client_fd)
		{
			_poll_fds.erase(it);
			break ;
		}
		++it;
	}
	std::map<int, Client*>::iterator it2 = _clients.find(client_fd);
	if (it2 != _clients.end())
	{
		delete it2->second;
		_clients.erase(it2);
	}
	else
		std::cout << "Client " << client_fd << " not found in _clients map" << std::endl;
	if (client_fd >= 0)
	{
		close(client_fd);
		std::cout << "Closed fd " << client_fd << std::endl;
	}
}

void	Webserver::_handleClientWrite(int client_fd)
{
	Client* client = _clients[client_fd];

	if (client->getState() != SENDING_HEADERS && client->getState() != SENDING_BODY)
		return;
	const std::string	&send_buffer = client->getSendBuffer();
	size_t bytes_sent = client->getBytesSent();
	size_t total_bytes = send_buffer.size();

	size_t chunck_size = std::min((size_t)CHUNK_SIZE, total_bytes - bytes_sent);
	int sent = send(client_fd, send_buffer.c_str() + bytes_sent, chunck_size, 0);

	if (sent <= 0)
	{
		if (total_bytes != client->getBytesSent())
			return;
		_removeClient(client_fd);
		return ;
	}
	client->setBytesSent(bytes_sent + sent);
	client->setLastActivity(time(NULL));
	if (client->getBytesSent() >= total_bytes)
	{
		if (client->shouldKeepAlive())
		{
			client->reset();
			client->setState(READING_HEADERS);
			_updatePollEvents(client_fd, POLLIN);
		}
		else
			_removeClient(client_fd);
	}
}

void Webserver::_handleClientData(int client_fd)
{
	std::cout << "	_handleClientData() called" << std::endl;

	Client* client = _clients[client_fd];
	if (!client)
		return;

	char buffer[BUFFER_SIZE];
	int bytes = recv(client_fd, buffer, BUFFER_SIZE - 1, 0);

	if (bytes <= 0)
	{
		if (bytes == 0)
			std::cout << "  Client " << client_fd << " disconnected" << std::endl;
		else
			std::cerr << "recv() error: " << strerror(errno) << std::endl;
		_removeClient(client_fd);
		return;
	}

	buffer[bytes] = '\0';
	client->appendReadData(buffer, bytes);
	client->setLastActivity(time(NULL));

	std::cout << " Received " << bytes << " bytes from client " << client_fd << std::endl;
	std::cout << " client->getState() = " << client->getState() << std::endl;

	// ============================================================
	// ALWAYS process the request after reading data
	// ============================================================
	if (client->getState() == READING_HEADERS)
	{
		std::cout << "  Processing request data..." << std::endl;

		// Parse headers (ignore return value)
		client->getRequest().parseHeaders(client->getReadBuffer());
		std::cout << "  parseHeaders() returned" << std::endl;

		// ============================================================
		// Extract body directly from raw data
		// ============================================================
		const std::string& raw_data = client->getReadBuffer();
		size_t header_end = raw_data.find("\r\n\r\n");

		if (header_end != std::string::npos)
		{
			std::string body_data = raw_data.substr(header_end + 4);
			std::cout << "  Body extracted: " << body_data.size() << " bytes" << std::endl;
			std::cout << "  Body: '" << body_data << "'" << std::endl;

			// Directly set body in Request
			client->getRequest().setBody(body_data);
		}
		else
		{
			std::cout << "  No body data found" << std::endl;
			client->getRequest().setBody("");
		}

		// ============================================================
		// Call _processRequest directly
		// ============================================================
		std::cout << "  Calling _processRequest()" << std::endl;
		client->setState(PROCESSING);
		_processRequest(client_fd);
	}
}

void Webserver::_acceptNewConnection(int server_fd)
{
	struct sockaddr_in client_addr;
	socklen_t addr_len = sizeof(client_addr);
	
	int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &addr_len);
	if (client_fd < 0)
	{
		if (errno != EWOULDBLOCK && errno != EAGAIN)
			std::cerr << "accept() failed: " << strerror(errno) << std::endl;
		return;
	}
	
	std::cout << "Accepted new client: fd " << client_fd << std::endl;
	int flags = fcntl(client_fd, F_GETFL, 0);
	if (flags < 0)
	{
		std::cerr << "  fcntl F_GETFL failed: " << strerror(errno) << std::endl;
		close(client_fd);
		return;
	}
	
	std::cout << "  Client fd " << client_fd << " flags before: " << flags << std::endl;
	
	if (fcntl(client_fd, F_SETFL, flags | O_NONBLOCK) < 0)
	{
		std::cerr << "  fcntl F_SETFL failed: " << strerror(errno) << std::endl;
		close(client_fd);
		return;
	}
	std::cout << "  Client fd " << client_fd << " flags after: " << flags << std::endl;
	if (flags & O_NONBLOCK)
	{
		std::cout << "  Client is non-blocking" << std::endl;
	}
	else
	{
		std::cout << "  Client is STILL blocking!" << std::endl;
	}
	Client* client = new Client(client_fd);
	client->setLastActivity(time(NULL));
	
	_clients[client_fd] = client;
	std::cout << "Stored client " << client_fd << " in _clients map (size: " << _clients.size() << ")" << std::endl;
	
	struct pollfd pfd;
	pfd.fd = client_fd;
	pfd.events = POLLIN;
	pfd.revents = 0;
	_poll_fds.push_back(pfd);
	
	std::cout << "Added client fd " << client_fd << " to poll (total: " << _poll_fds.size() << ")" << std::endl;
	std::cout << "  Now monitoring " << _poll_fds.size() << " fds" << std::endl;
}

void Webserver::run(void)
{
	_setupSockets();
	_running = true;
	
	std::cout << "  Server running on " << _server_sockets.size() << " socket(s)" << std::endl;
	
	while (_running)
	{
		int ret = poll(_poll_fds.data(), _poll_fds.size(), 1000);
		
		if (ret < 0)
		{
			if (errno == EINTR)
				continue;
			std::cerr << "poll() error: " << strerror(errno) << std::endl;
			break;
		}
		
		std::cout << "  Monitoring " << _poll_fds.size() << " fds: ";
		for (size_t i = 0; i < _poll_fds.size(); ++i)
		{
			std::cout << _poll_fds[i].fd;
			if (_poll_fds[i].fd == 4) 
				std::cout << "(client)";
			if (i + 1 < _poll_fds.size())
				std::cout << ", ";
		}
		std::cout << std::endl;
		
		if (ret > 0)
		{
			std::cout << "  poll() returned " << ret << " event(s)" << std::endl;
			
			for (size_t i = 0; i < _poll_fds.size(); ++i)
			{
				if (_poll_fds[i].revents)
				{
					std::cout << "  fd " << _poll_fds[i].fd 
							  << ": revents = " << _poll_fds[i].revents 
							  << " (POLLIN=" << POLLIN << ")" << std::endl;
					
					if (_poll_fds[i].revents & POLLIN)
					{
						std::cout << "  POLLIN on fd " << _poll_fds[i].fd << std::endl;
						
						std::map<int, Server>::iterator it = _server_sockets.find(_poll_fds[i].fd);
						if (it != _server_sockets.end())
						{
							std::cout << "  Server socket, accepting..." << std::endl;
							_acceptNewConnection(_poll_fds[i].fd);
						}
						else
						{
							std::cout << "  Client socket, calling _handleClientData..." << std::endl;
							_handleClientData(_poll_fds[i].fd);
						}
					}
					
					if (_poll_fds[i].revents & POLLOUT)
						_handleClientWrite(_poll_fds[i].fd);
					
					if (_poll_fds[i].revents & (POLLERR | POLLHUP | POLLNVAL))
						_removeClient(_poll_fds[i].fd);
				}
			}
		}
		_checkTimeouts();
	}
}

void	Webserver::stop(void)
{
	_running = false;
}

void	Webserver::_createSocket(const Server& config)
{
	int		sockfd = socket(AF_INET, SOCK_STREAM, 0);

	if (sockfd < 0)
		throw std::runtime_error("Failed to create socket: " + std::string(strerror(errno)));
	int		opt = 1;
	std::cout << "Created socket fd: " << sockfd << std::endl;

	if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
	{
		close(sockfd);
		throw std::runtime_error("Failed to set SO_REUSEADDR: " + std::string(strerror(errno)));
	}
	int		flags = fcntl(sockfd, F_GETFL, 0);
	if (flags < 0 || fcntl(sockfd, F_SETFL, flags | O_NONBLOCK) < 0)
	{
		close(sockfd);
		throw std::runtime_error("Failed to set non-blocking mode: " + std::string(strerror(errno)));
	}

	struct sockaddr_in	addr;
	std::memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(config.port);
	if (config.host.empty() || config.host == "0.0.0.0")
		addr.sin_addr.s_addr = INADDR_ANY;
	else
	{
			struct in_addr ip_addr;
			if (!inet_aton(config.host.c_str(), &ip_addr))
			{
				close(sockfd);
				throw std::runtime_error("Invalid IP address: " + config.host);
			}
			addr.sin_addr = ip_addr;
	}
	std::cout << "config.host = " << config.host << " config.port = " << config.port << std::endl << std::endl;
	if (bind(sockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0)
	{
		close(sockfd);
		throw std::runtime_error("Failed to bind port to " + std::to_string(config.port) +
			": " + std::string (strerror(errno)));
	}
	if (listen(sockfd, BACKLOG) < 0)
	{
			close(sockfd);
			throw std::runtime_error("Failed to listen on socket: " + std::string(strerror(errno)));
	}
	std::cout << "Listening on fd " << sockfd << " port " << config.port << std::endl;

	_server_sockets[sockfd] = config;
	struct pollfd	pfd;
	pfd.fd = sockfd;
	pfd.events = POLLIN;
	pfd.revents = 0;
	_poll_fds.push_back(pfd);
 std::cout << "Added to poll: fd=" << pfd.fd 
			  << ", events=" << pfd.events 
			  << " (POLLIN=" << POLLIN << ")" << std::endl;
	std::cout << "_poll_fds now has " << _poll_fds.size() << " entries" << std::endl;
	
	std::cout << "Added fd " << sockfd << " to poll (total: " << _poll_fds.size() << ")" << std::endl;
}

void	Webserver::_setupSockets(void)
{
	size_t		i = 0;

	while (i < _configs.size())
		_createSocket(_configs[i++]);
}

void Webserver::_handleGetRequest(Client *client, Location *location, const std::string &fs_path, const std::string &uri)
{
    std::cout << "  _handleGetRequest called" << std::endl;
    std::cout << "  fs_path: '" << fs_path << "'" << std::endl;
    
    struct stat st;
    if (stat(fs_path.c_str(), &st) != 0)
    {
        std::cout << "  File not found: " << fs_path << std::endl;
        _sendErrorResponse(client, 404, "Not Found");
        return;
    }
    
    std::cout << "  File exists! Size: " << st.st_size << " bytes" << std::endl;
    
    // If it's a directory
    if (S_ISDIR(st.st_mode))
    {
        std::cout << "  Is directory, handling autoindex..." << std::endl;
        
        // Check for index file
        if (!location->index.empty())
        {
            std::string index_path = fs_path;
            if (fs_path[fs_path.size() - 1] != '/')
                index_path += '/';
            index_path += location->index;
            
            if (stat(index_path.c_str(), &st) == 0 && !S_ISDIR(st.st_mode))
            {
                std::cout << "  Found index file: " << index_path << std::endl;
                _serveFile(client, index_path);
                return;
            }
        }
        
        // Check autoindex
        if (location->autoindex)
        {
            std::cout << "  Generating directory listing..." << std::endl;
            std::string listing = _router.generateDirectoryListing(fs_path, uri);
            std::string response = _buildResponse(200, "OK", "text/html", listing);
            client->setSendBuffer(response);
            client->setState(SENDING_HEADERS);
            _updatePollEvents(client->getFd(), POLLIN | POLLOUT);
            return;
        }
        else
        {
            _sendErrorResponse(client, 403, "Forbidden");
            return;
        }
    }
    
    // It's a file - serve it!
    std::cout << "  It's a file, calling _serveFile..." << std::endl;
    _serveFile(client, fs_path);
}

void Webserver::_serveFile(Client *client, const std::string &path)
{
    std::cout << "  _serveFile called with path: '" << path << "'" << std::endl;
    
    // Open file
    std::ifstream file(path.c_str(), std::ios::binary);
    if (!file.is_open())
    {
        std::cout << "  File not found!" << std::endl;
        _sendErrorResponse(client, 404, "Not Found");
        return;
    }
    
    // Read the ENTIRE file using stringstream
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();
    file.close();
    
    std::cout << "  Content size: " << content.size() << " bytes" << std::endl;
    std::cout << "  Content: '" << content << "'" << std::endl;
    
    // Get MIME type
    std::string mime_type = _router.getMimeType(path);
    std::cout << "  MIME type: " << mime_type << std::endl;
    
    // Build response
    std::string response = _buildResponse(200, "OK", mime_type, content);
    std::cout << "  Response size: " << response.size() << " bytes" << std::endl;
    
    client->setSendBuffer(response);
    client->setState(SENDING_HEADERS);
    _updatePollEvents(client->getFd(), POLLIN | POLLOUT);
}

std::string	Webserver::_buildResponse(int status, const std::string &status_text, const std::string &content_type, const std::string &body)
{
	std::cout << "  _buildResponse: status=" << status << ", body_size=" << body.size() << std::endl;
	std::string	response;
	response += "HTTP/1.1 " + std::to_string(status) + " " + status_text + "\r\n";
	response += "Content-Type: " + content_type + "\r\n";
	response += "Content-Length: " + std::to_string(body.size()) + "\r\n";
	response += "Connection: close\r\n";
	response += "\r\n";
	response += body;
	std::cout << "  Response length: " << response.size() << " bytes" << std::endl;
	return (response);
}

void	Webserver::_sendErrorResponse(Client *client, int status, const std::string &status_text)
{
	std::string	body = "<html><body><h1>" + std::to_string(status) + " " + status_text + "</h1></body></html>";
	std::string	response = _buildResponse(status, status_text, "text/html", body);
	client->setSendBuffer(response);
	client->setState(SENDING_HEADERS);
	_updatePollEvents(client->getFd(), POLLIN | POLLOUT);
}

Server	*Webserver::_findServer(const std::string &host_header)
{
	std::string	hostname = host_header;
	size_t		colon = hostname.find(':');
	size_t		i = 0;
	if (host_header.empty())
	{
		if (!_configs.empty())
			return (&_configs[0]);
		return (NULL);
	}
	if (colon != std::string::npos)
		hostname = hostname.substr(0, colon);
	while (i < _configs.size())
	{
		Server	&server = _configs[i];
		size_t	j = 0;
		while (j < server.server_names.size())
		{
			if (server.server_names[j] == hostname)
				return (&server);
			++j;
		}
		++i;
	}
	if (!_configs.empty())
		return (&_configs[0]);
	return (NULL);
}

void Webserver::_handlePostRequest(Client *client, Location *location)
{
    std::cout << "  _handlePostRequest called" << std::endl;
    std::cout << "  upload_store: '" << location->upload_store << "'" << std::endl;
    std::cout << "  upload_store.empty(): " << location->upload_store.empty() << std::endl;
    
    Request &request = client->getRequest();
    std::string body = request.getBody();
    
    std::cout << "  Body size: " << body.size() << " bytes" << std::endl;
    std::cout << "  Body content: '" << body << "'" << std::endl;
    
    // If root location or no upload_store, return 200 with echo
    if (location->path == "/" || location->upload_store.empty())
    {
        std::cout << "  No upload_store, returning 200 OK with echo" << std::endl;
        
        std::string response_body = "<html><body>";
        response_body += "<h1>POST Received</h1>";
        response_body += "<p>Method: POST</p>";
        response_body += "<p>URI: " + request.getUri() + "</p>";
        response_body += "<p>Body: " + body + "</p>";
        response_body += "</body></html>";
        
        std::string response = _buildResponse(200, "OK", "text/html", response_body);
        client->setSendBuffer(response);
        client->setState(SENDING_HEADERS);
        _updatePollEvents(client->getFd(), POLLIN | POLLOUT);
        return;
    }
    
    // File upload handling
    std::string content_type = request.getHeader("Content-Type");
    std::string filename;
    std::string content = body;
    
    if (content_type.find("multipart/form-data") != std::string::npos)
    {
        _handleMultipartUpload(client, *location, content_type, body);
        return;
    }
    
    if (content_type.find("application/x-www-form-urlencoded") != std::string::npos)
        filename = "form_data_" + std::to_string(time(NULL)) + ".txt";
    else if (content_type.find("text/plain") != std::string::npos)
        filename = "raw_data_" + std::to_string(time(NULL)) + ".txt";
    else
        filename = "upload_" + std::to_string(time(NULL)) + ".bin";
    
    _handleFileUpload(client, *location, filename, content);
}

void	Webserver::_handleMultipartUpload(Client *client, const Location &location, const std::string &content_type, const std::string &body)
{
	std::string	boundary = "--";
	size_t		boundary_pos = content_type.find("boundary=");
	if (boundary_pos != std::string::npos)
		boundary += content_type.substr(boundary_pos + 9);
	else
	{
		_sendErrorResponse(client, 400, "Bad Request - Missing boundary");
		return ;
	}
	std::string	filename;
	std::string	content;

	size_t	filename_pos = body.find("filename=\"");
	if (filename_pos != std::string::npos)
	{
		size_t	start = filename_pos + 10;
		size_t	end = body.find("\"", start);
		if (end != std::string::npos)
			filename = body.substr(start, end - start);
	}
	size_t	content_start = body.find("\r\n\r\n");
	if (content_start != std::string::npos)
	{
		content_start += 4;
		size_t	content_end = body.find(boundary, content_start);
		if (content_end != std::string::npos)
		{
			content = body.substr(content_start, content_end - content_start);
			if (content.size() >= 2 && content[content.size() - 2] == '\r')
				content = content.substr(0, content.size() - 2);
		}
		else
			content = body.substr(content_start);
	}
	_handleFileUpload(client, location, filename, content);
}

void	Webserver::_handleFileUpload(Client *client, const Location &location, const std::string &filename, const std::string &content)
{
	std::string	upload_path = location.upload_store;
	if (upload_path[upload_path.size() - 1] != '/')
		upload_path += '/';
	upload_path += filename;
	std::cout << "Uploading File: " << upload_path << " (" << content.size() << " bytes)" << std::endl;

	std::string	dir_path = location.upload_store;
	if (dir_path[dir_path.size() - 1] != '/')
		dir_path += '/';

	struct stat st;
	if (stat(dir_path.c_str(), &st) != 0)
	{
		if (mkdir(dir_path.c_str(), 0755) != 0 && errno != EEXIST) // need to implemnt alternative to checking errno
		{
			std::cerr << "Failed to open file for writing: " << upload_path << std::endl;
			_sendErrorResponse(client, 500, "Internal Server Error");
			return ;
		}
	}
	std::ofstream		file(upload_path.c_str(), std::ios::binary);
	if (!file.is_open())
	{
		std::cerr << "Failed to open file for writing: " << upload_path << std::endl;
		_sendErrorResponse(client, 500, "Internal Server Error");
		return ;
	}
	file.write(content.c_str(), content.size());
	file.close();
	std::cout << "File uploaded successfully: " << upload_path << std::endl;
	std::string	response_body = "<html><body>";
	response_body += "<h1>201 Created</h1>";
	response_body += "<p>File uploaded successfully: " + filename + "</p>";
	response_body += "<p>Size: " + std::to_string(content.size()) + " bytes</p>";
	response_body += "</body></html>";
	std::string	response = _buildResponse(201, "Created", "text/html", response_body);
	client->setSendBuffer(response);
	client->setState(SENDING_HEADERS);
	_updatePollEvents(client->getFd(), POLLIN | POLLOUT);
}

void	Webserver::_handleDeleteRequest(Client *client, const std::string &fs_path)
{
	std::cout << "Delete request for: " << fs_path << std::endl;
	struct stat	st;
	if (stat(fs_path.c_str(), &st) != 0)
	{
		_sendErrorResponse(client, 404,  "Not Found");
		return ;
	}
	if (S_ISDIR(st.st_mode))
	{
		_sendErrorResponse(client, 403, "Forbidden - Cannot delete directories");
		return ;
	}
	if (unlink(fs_path.c_str()) != 0) // need alternative to unlink()
	{
		std::cerr << "Failed to delete this: " << fs_path << " (" << strerror(errno) << ")" << std::endl;
		_sendErrorResponse(client, 500, "Internal Server Error");
		return ;
	}
	std::cout << "File deleted: " << fs_path << std::endl;
	std::string response_body = "<html><body>";
	response_body += "<h1>204 No Content</h1>";
	response_body += "<p>File deleted successfully: " + fs_path + "</p>";
	response_body += "</body></html>";
	std::string	response = _buildResponse(204, "No Content", "text/html", response_body);
	client->setSendBuffer(response);
	client->setState(SENDING_HEADERS);
	_updatePollEvents(client->getFd(), POLLIN | POLLOUT);
}

void	Webserver::_handleHeadRequest(Client *client, const Location &location, const std::string &fs_path)
{
	struct stat st;
	if (stat(fs_path.c_str(), &st) != 0)
	{
		_sendErrorResponse(client, 404, "Not Found");
		return ;
	}
	std::string response;
	std::string	index_path = fs_path;
	if (S_ISDIR(st.st_mode))
	{
		if (!location.index.empty())
		{
			if (fs_path[fs_path.size() - 1] != '/')
				index_path += '/';
			index_path += location.index;
		}
		if (stat(index_path.c_str(), &st) == 0 && !S_ISDIR(st.st_mode))
		{
			response += "HTTP/1.1 200 OK\r\n";
			response += "Content-Type: " + _router.getMimeType(index_path) + "\r\n";
			response += "Content-Length: " + std::to_string(st.st_size) + "\r\n";
			response += "Connection: close\r\n";
			response += "\r\n";
			client->setSendBuffer(response);
			client->setState(SENDING_HEADERS);
			_updatePollEvents(client->getFd(), POLLIN | POLLOUT);
			return ;
		}
	}
	else
	{
		response += "HTTP/1.1 200 OK\r\n";
		response += "Content-Type: " + _router.getMimeType(fs_path) + "\r\n";
		response += "Content-Length: " + std::to_string(st.st_size) + "\r\n";
		response += "Connection: close\r\n";
		response += "\r\n";
	}
	client->setSendBuffer(response);
	client->setState(SENDING_HEADERS);
	_updatePollEvents(client->getFd(), POLLIN | POLLOUT);
	return ;
}
