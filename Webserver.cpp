#include "webserv.hpp"
#include "CGIHandler.hpp"

Webserver::Webserver(std::vector<Server> &servers) :_configs(servers)
{
	std::cout << "Webserver constructor called\n";
}

Webserver::Webserver(const Webserver &other)
{
	std::cout << "Webserver copy constructor called\n";
	*this = other;
}

Webserver	&Webserver::operator=(const Webserver &other)
{
	std::cout << "Webserver assignment operator called\n";
	if (this != &other)
	{
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
	_cleanup();
	std::cout << "Webserver destructor called\n";
}

void Webserver::run(void)
{
	_setupSockets();
	_running = true;

	std::cout << "  Server running on " << _server_sockets.size() << " socket(s)" << std::endl;

	while (_running && !g_shutdown)
	{
		int ret = poll(_poll_fds.data(), _poll_fds.size(), 1000);

		if (ret < 0)
		{
			if (errno == EINTR)
			{
				if (g_shutdown)
					break ;
				continue;
			}
			std::cerr << "poll() error: " << strerror(errno) << std::endl;
			break;
		}

		if (ret > 0)
		{
			std::vector<std::pair<int, short> >	events;
			size_t									i = 0;

			while (i < _poll_fds.size())
			{
				if (_poll_fds[i].revents)
					events.push_back(std::make_pair(_poll_fds[i].fd, _poll_fds[i].revents));
				++i;
			}
			i = 0;
			while (i < events.size())
			{
				int		fd = events[i].first;
				short	revents = events[i].second;
				++i;

				if (!_isFdTracked(fd))
					continue ;
				if (revents & (POLLIN | POLLHUP))
				{
					if (_server_sockets.find(fd) != _server_sockets.end())
					{
						if (revents & POLLIN)
							_acceptNewConnection(fd);
					}
					else if (_cgi_fd_to_client.find(fd) != _cgi_fd_to_client.end())
						_handleCGIRead(fd);
					else
						_handleClientData(fd);
				}
				if (!_isFdTracked(fd))
					continue ;

				if (revents & POLLOUT)
				{
					if (_cgi_fd_to_client.find(fd) != _cgi_fd_to_client.end())
						_handleCGIWrite(fd);
					else
						_handleClientWrite(fd);
				}
				if (!_isFdTracked(fd))
					continue ;
				if (revents & (POLLERR | POLLNVAL))
					_handleFdError(fd);
			}
		}
		_checkTimeouts();
	}
}
void	Webserver::stop(void)
{
	_running = false;
}

void	Webserver::_setupSockets(void)
{
	size_t	i = 0;
	try
	{
		while (i < _configs.size())
			_createSockets(_configs[i++]);
	}
	catch (std::exception &e)
	{
		std::cerr << e.what() << std::endl;
	}
}

void Webserver::_createSockets(const Server& config)
{
	int	sockfd = socket(AF_INET, SOCK_STREAM, 0);

	if (sockfd < 0)
		throw std::runtime_error("Failed to create socket: " + std::string (strerror(errno)));
	int	opt = 1;
	if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
	{
		close(sockfd);
		throw std::runtime_error("Failed to set SO_REUSEADDR: " + std::string(strerror(errno)));
	}
	if (fcntl(sockfd, F_SETFL, O_NONBLOCK) < 0)
	{
		close(sockfd);
		throw std::runtime_error("Failed to set non-blocking mode");
	}
	
	struct sockaddr_in	addr;
	std::memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(config.port);
	if (config.host.empty() || config.host == "0.0.0.0")
		addr.sin_addr.s_addr = INADDR_ANY;
	else
	{
		struct in_addr	ip_addr;
		if (!inet_aton(config.host.c_str(), &ip_addr))
		{
			close(sockfd);
			throw std::runtime_error("Invalid IP address: " + config.host);
		}
		addr.sin_addr = ip_addr;
	}
	if (bind(sockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0)
	{
		close(sockfd);
		throw std::runtime_error("Failed to bind port to " + Parser::toString(config.port) + ": " + std::string (strerror(errno)));
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
}

void	Webserver::_acceptNewConnection(int server_fd)
{
	struct sockaddr_in	client_addr;
	socklen_t			addr_len = sizeof(client_addr);

	int	client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &addr_len);
	if (client_fd < 0)
	{
		std::cerr << "accept() failed\n";
		return;
	}
	int	flags = fcntl(client_fd, F_GETFL, 0);
	if (flags < 0 || fcntl(client_fd, F_SETFL, flags | O_NONBLOCK) < 0)
	{
		std::cerr << "fnctl() failed on client fd: " << strerror(errno) << std::endl;
		close(client_fd);
		return ;
	}
	Client *client = new Client(client_fd);
	client->setLastActivity(time(NULL));
	std::map<int, Server>::iterator	sit = _server_sockets.find(server_fd);
	if (sit != _server_sockets.end())
		client->setListenPort(sit->second.port);
	_clients[client_fd] = client;
	struct pollfd	pfd;
	pfd.fd = client_fd;
	pfd.events = POLLIN;
	pfd.revents = 0;
	_poll_fds.push_back(pfd);
	std::cout << "Accepted new client: fd " << client_fd << std::endl; 
}

bool	Webserver::_isFdTracked(int fd) const
{
	if (_server_sockets.find(fd) != _server_sockets.end())
		return (true);
	if (_clients.find(fd) != _clients.end())
		return (true);
	if (_cgi_fd_to_client.find(fd) != _cgi_fd_to_client.end())
		return (true);
	return (false);
}

void	Webserver::_handleCGIRead(int fd)
{
	std::map<int, int>::iterator	cit = _cgi_fd_to_client.find(fd);
	if (cit == _cgi_fd_to_client.end())
		return ;
	int		client_fd = cit->second;
	Client	*client = _clients.count(cit->second) ? _clients[client_fd] : NULL;
	if (!client || !client->getCgi())
	{
		_removeCGIFd(fd);
		return ;
	}
	if (client->getCgi()->readFromOutput())
	{
		_removeCGIFd(fd);
		_finishCGI(client_fd);
	}
}

void	Webserver::_handleCGIWrite(int fd)
{
	std::map<int, int>::iterator	cit = _cgi_fd_to_client.find(fd);
	if (cit == _cgi_fd_to_client.end())
		return ;
	Client	*client = _clients.count(cit->second) ? _clients[cit->second] : NULL;
	if (!client || !client->getCgi())
	{
		_removeCGIFd(fd);
		return ;
	}
	if (client->getCgi()->writeToInput())
	{
		bool	failed = (client->getCgi()->getState() == CGI_ERROR);
		_removeCGIFd(fd);
		if (failed)
		{
			delete client->getCgi();
			client->setCgi(NULL);
			_sendErrorResponse(client, 500, "Internal Server Error - CGI write failed");
			return ;
		}
		client->setState(CGI_READING);
	}
}

void	Webserver::_finishCGI(int client_fd)
{
	Client		*client = _clients.count(client_fd) ? _clients[client_fd] : NULL;
	if (!client)
		return ;
	CGIHandler	*cgi = client->getCgi();
	if (!cgi)
		return ;
	if (cgi->getState() == CGI_ERROR)
	{
		delete cgi;
		client->setCgi(NULL);
		_sendErrorResponse(client, 502, "Bad Gateway - CGI error");
		return ;
	}

	int									status = cgi->getStatusCode();
	std::string							body = cgi->getParsedBody();
	std::map<std::string, std::string>	headers = cgi->getParsedHeaders();
	std::string							content_type = headers.count("Content-Type") ? headers["Content-Type"] : "text/html";
	std::string							response = "HTTP/1.1 " + Parser::toString(status) + " OK\r\n";
	response += "Content-Type: " + content_type + "\r\n";
	response += "Content-Length: " + Parser::toString(body.size()) + "\r\n";
	
	std::map<std::string, std::string>::iterator	it = headers.begin();
	while (it != headers.end())
	{
		if (it->first != "Content-Type")
			response += it->first + ": " + it->second + "\r\n";
		++it;
	}
	response += "Connection: close\r\n\r\n" + body;

	delete cgi;
	client->setCgi(NULL);
	client->setSendBuffer(response);
	client->setState(SENDING_HEADERS);
	_updatePollEvents(client->getFd(), POLLIN | POLLOUT);
}

void	Webserver::_handleFdError(int fd)
{
	std::map<int, int>::iterator	cit = _cgi_fd_to_client.find(fd);
	if (cit != _cgi_fd_to_client.end())
	{
		int		client_fd = cit->second;
		Client	*client = _clients.count(client_fd) ? _clients[client_fd] : NULL;
		_removeCGIFd(fd);
		if (client && client->getCgi())
		{
			client->getCgi()->kill();
			delete client->getCgi();
			client->setCgi(NULL);
			_sendErrorResponse(client, 502, "Bad Gateway - CGI pipe error");
		}
		return ;
	}
	_removeClient(fd);
}

void	Webserver::_removeCGIFd(int fd)
{
	std::map<int, int>::iterator	cgi_it = _cgi_fd_to_client.find(fd);
	bool								close_fd = true;
	if (cgi_it != _cgi_fd_to_client.end())
	{
		std::map<int, Client *>::iterator	client_it = _clients.find(cgi_it->second);
		if (client_it != _clients.end() && client_it->second->getCgi())
		{
			CGIHandler	*cgi = client_it->second->getCgi();
			if (cgi->getInputFd() != fd && cgi->getOutputFd() != fd)
				close_fd = false;
			else
				cgi->forgetFd(fd);
		}
	}
	std::vector<struct pollfd>::iterator	it = _poll_fds.begin();
	while (it != _poll_fds.end())
	{
		if (it->fd == fd)
		{
			_poll_fds.erase(it);
			break ;
		}
		++it;
	}
	if (close_fd)
		close(fd);
	_cgi_fd_to_client.erase(fd);
}

void	Webserver::_removeClient(int client_fd)
{
	std::map<int, Client *>::iterator	cit = _clients.find(client_fd);
	if (cit != _clients.end())
	{
		Client	*client = cit->second;
		if (client && client->getCgi())
		{
			int	in_fd = client->getCgi()->getInputFd();
			int	out_fd = client->getCgi()->getOutputFd();
			if (in_fd >= 0)
				_removeCGIFd(in_fd);
			if (out_fd >= 0)
				_removeCGIFd(out_fd);
			client->getCgi()->kill();
		}
		delete client;
		_clients.erase(cit);
	}
	else
		std::cout << "Client " << client_fd << " not found in _clients map\n";
	
	std::vector<struct pollfd>::iterator	it = _poll_fds.begin();
	while (it != _poll_fds.end())
	{
		if (it->fd == client_fd)
		{
			_poll_fds.erase(it);
			break ;
		}
		++it;
	}
	if (client_fd >= 0)
	{
		close(client_fd);
		std::cout << "Closed fd " << client_fd << std::endl;
	}
}

void	Webserver::_checkTimeouts(void)
{
	time_t	now = time(NULL);
	std::map<int, Client *>::iterator	it = _clients.begin();
	while (it != _clients.end())
	{
		Client	*client = it->second;
		int		fd = it->first;
		++it;

		if (!client)
			continue ;
		if (client->getCgi() && client->getCgi()->hasTimeOut())
		{
			std::cout << "CGI for client " << fd << " timed out" << std::endl;
			int	in_fd = client->getCgi()->getInputFd();
			int	out_fd = client->getCgi()->getOutputFd();
			if (in_fd >= 0)
				_removeCGIFd(in_fd);
			if (out_fd >= 0)
				_removeCGIFd(out_fd);
			client->getCgi()->kill();
			delete client->getCgi();
			client->setCgi(NULL);
			_sendErrorResponse(client, 504, "Gateway Timeout");
			continue ;
		}
		if (now - client->getLastActivity() > TIMEOUT_SECONDS)
		{
			std::cout << "Client " << fd << " timed out " << std::endl;
			_removeClient(fd);
		}
	}
}

void	Webserver::_cleanup(void)
{
	std::map<int, Client *>::iterator	cit = _clients.begin();
	while (cit != _clients.end())
	{
		if (cit->second)
			delete cit->second;
		close(cit->first);
		++cit;
	}
	_clients.clear();
	std::map<int, Server>::iterator		sit = _server_sockets.begin();
	while (sit != _server_sockets.end())
	{
		close(sit->first);
		++sit;
	}
	_server_sockets.clear();
	_poll_fds.clear();
	_running = false;
}
void	Webserver::_updatePollEvents(int fd, short events)
{
	size_t	i = 0;
	while (i < _poll_fds.size())
	{
		if (_poll_fds[i].fd == fd)
		{
			_poll_fds[i].events =  events;
			break ;
		}
		++i;
	}
}

void	Webserver::_sendErrorResponse(Client *client, int status, const std::string &status_text)
{
	// Check if there's a custom error page configured for this status
	std::string	errorPagePath = _getErrorPagePath(client, status);
	if (!errorPagePath.empty())
	{
		// Try to serve the custom error page
		std::ifstream	file(errorPagePath.c_str(), std::ios::binary);
		if (file.is_open())
		{
			std::stringstream	buffer;
			buffer << file.rdbuf();
			std::string	content = buffer.str();
			file.close();

			std::string	mimeType = _router.getMimeType(errorPagePath);
			if (mimeType.empty())
				mimeType = "text/html";

			std::string	response = _buildResponse(status, status_text, mimeType, content);
			client->setSendBuffer(response);
			client->setState(SENDING_HEADERS);
			_updatePollEvents(client->getFd(), POLLIN | POLLOUT);
			return ;
		}
		// If file doesn't fall back to default error response
	}

	// Default error response (fallback)
	std::string	body = "<html><body><h1>" + Parser::toString(status) + " " + status_text + "</h1></body></html>";
	std::string	response = _buildResponse(status, status_text, "text/html", body);
	client->setSendBuffer(response);
	client->setState(SENDING_HEADERS);
	_updatePollEvents(client->getFd(), POLLIN | POLLOUT);

	return ;
}

std::string	Webserver::_getErrorPagePath(Client *client, int status)
{
	// Find the server configuration for this client
	if (!client)
		return "";

	std::string	hostHeader = client->getRequest().getHeader("Host");
	int listenPort = client->getListenPort();
	Server		*server = _findServer(hostHeader, listenPort);
	if (!server)
		return "";

	// Look for error page in server configuration
	std::map<int, std::string>::const_iterator	it = server->error_pages.find(status);
	if (it != server->error_pages.end())
	{
		// Return the error page path (it->second)
		return it->second;
	}

	return "";
}

std::string	Webserver::_buildResponse(int status, const std::string &status_text, const std::string &content_type, const std::string &body)
{
	std::string	response;
	response += "HTTP/1.1 " + Parser::toString(status) + " " + status_text + "\r\n";
	response += "Content-Type: " + content_type + "\r\n";
	response += "Content-Length: " + Parser::toString(body.size()) + "\r\n";
	response += "Connection: close\r\n\r\n";
	response += body;
	return (response);
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
	std::string	uri_path = uri;
	size_t		qmark = uri.find('?');
	if (qmark != std::string::npos)
		uri_path = uri.substr(0, qmark);
	Server		*server = _findServer(request.getHeader("Host"), client->getListenPort());
	if (!server)
	{
		_sendErrorResponse(client, 400, "Bad Request - No server found");
		return ;
	}
	const Location	*location = _router.findLocation(*server, uri_path);
	if (!location)
	{
		_sendErrorResponse(client, 404, "Not Found - No matching location");
		return ;
	}

	// Check return_redirect first
	if (!location->return_redirect.empty())
	{
		// Parse redirect: "STATUS_CODE URL"
		std::istringstream iss(location->return_redirect);
		int status_code;
		std::string url;
		iss >> status_code >> url;

		if (status_code > 0 && !url.empty())
		{
			std::string response = "HTTP/1.1 " + Parser::toString(status_code) + " ";
			switch (status_code)
			{
				case 301: response += "Moved Permanently"; break;
				case 302: response += "Found"; break;
				case 303: response += "See Other"; break;
				case 307: response += "Temporary Redirect"; break;
				case 308: response += "Permanent Redirect"; break;
				default: response += "Redirect"; break;
			}
			response += "\r\nLocation: " + url + "\r\n";
			response += "Content-Length: 0\r\n";
			response += "Connection: close\r\n\r\n";

			client->setSendBuffer(response);
			client->setState(SENDING_HEADERS);
			_updatePollEvents(client->getFd(), POLLIN | POLLOUT);
			return;
		}
	}

	// Check client_max_body_size for POST requests
	if (method == "POST" && location->client_max_body_size > 0)
	{
		std::string content_length_str = request.getHeader("Content-Length");
		if (!content_length_str.empty())
		{
			size_t content_length = std::atol(content_length_str.c_str());
			if (content_length > location->client_max_body_size)
			{
				_sendErrorResponse(client, 413, "Payload Too Large");
				return;
			}
		}
		// Note: Without Content-Length (chunked encoding), we can't check size here
		// For simplicity, we rely on the body size check after reading in specific handlers
	}

	if (!_router.isMethodAllowed(*location, method))
	{
		_sendErrorResponse(client, 405, "Method Not Allowed");
		return ;
	}
	std::string		fs_path = _router.buildPath(*location, uri_path);

	size_t			dot = uri_path.find_last_of('.');
	std::string		ext = (dot != std::string::npos) ? uri_path.substr(dot) : "";
	std::map<std::string, std::string>::const_iterator	cgi_it = location->cgi_extensions.find(ext);

	if (cgi_it != location->cgi_extensions.end())
	{
		_handleCGIRequest(client, *location, fs_path, uri, server, cgi_it->second);
		return ;
	}
	if (method == "GET")
		_handleGetRequest(client, (Location *)location, fs_path, uri_path);
	else if (method == "POST")
		_handlePostRequest(client, (Location *)location);
	else if (method == "DELETE")
		_handleDeleteRequest(client, fs_path);
	else if (method == "HEAD")
		_handleHeadRequest(client, *location, fs_path);
	else
		_sendErrorResponse(client, 405, "Method Not Allowed");
}

Server	*Webserver::_findServer(const std::string &host_header, int listen_port)
{
	std::string		hostname = host_header;
	size_t			colon = hostname.find(':');

	if (colon != std::string::npos)
		hostname = hostname.substr(0, colon);
	Server			*first_port_match = NULL;
	size_t			i = 0;
	while (i < _configs.size())
	{
		Server		&server = _configs[i];

		if (server.port == listen_port)
		{
			if (!first_port_match)
				first_port_match = &server;
			size_t	j = 0;
			while (j < server.server_names.size())
			{
				if (server.server_names[j] == hostname)
					return (&server);
				++j;
			}
		}
		++i;
	}
	if (first_port_match)
		return (first_port_match);
	if (!_configs.empty())
		return (&_configs[0]);
	return (NULL);
}

void						Webserver::_handleClientWrite(int client_fd)
{
	Client	*client = _clients[client_fd];

	if (client->getState() != SENDING_HEADERS && client->getState() != SENDING_BODY)
		return ;
	const std::string	&send_buffer = client->getSendBuffer();
	size_t	bytes_sent = client->getBytesSent();
	size_t	total_bytes = send_buffer.size();
	size_t	chunk_size = std::min((size_t)CHUNK_SIZE, total_bytes - bytes_sent);
	int		sent = send(client_fd, send_buffer.c_str() + bytes_sent, chunk_size, 0);
	if (sent <= 0)
	{
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

void	Webserver::_handleClientData(int client_fd)
{
	Client	*client = _clients[client_fd];
	if (!client)
		return ;
	char	buffer[BUFFER_SIZE];
	int		bytes = recv(client_fd, buffer, BUFFER_SIZE - 1, 0);
	if (bytes <= 0)
	{
		if (bytes == 0)
			std::cout << "Client " << client_fd << " disconnected" << std::endl;
		_removeClient(client_fd);
		return ;
	}
	buffer[bytes] = '\0';
	client->appendReadData(buffer, bytes);
	client->setLastActivity(time(NULL));
	if (client->getState() == READING_HEADERS)
	{
		client->getRequest().parseHeaders(client->getReadBuffer());
		const std::string	&raw_data = client->getReadBuffer();
		size_t	header_end = raw_data.find("\r\n\r\n");
		if (header_end != std::string::npos)
		{
			Request	&request = client->getRequest();
			std::string	uri_path = request.getUri();
			size_t	qmark = uri_path.find('?');
			if (qmark != std::string::npos)
				uri_path = uri_path.substr(0, qmark);
			Server	*server = _findServer(request.getHeader("Host"), client->getListenPort());
			const Location	*location = server ? _router.findLocation(*server, uri_path) : NULL;
			if (request.getMethod() == "POST" && location
				&& location->client_max_body_size > 0
				&& request.getContentLength() > location->client_max_body_size)
			{
				_sendErrorResponse(client, 413, "Payload Too Large");
				return ;
			}
			if (client->getRequest().isChunked())
			{
				if (client->getRequest().parseBody(raw_data))
				{
					client->setState(PROCESSING);
					_processRequest(client_fd);
				}
				else if (client->getRequest().isMalformed())
					_sendErrorResponse(client, 400, "Bad Request - Malformed chunked body");
				else
					client->setState(READING_BODY);
			}
			else
			{
				std::string	body_data = raw_data.substr(header_end + 4);
				if (client->getRequest().getContentLength() > 0
					&& body_data.size() < client->getRequest().getContentLength())
					client->setState(READING_BODY);
				else
				{
					client->getRequest().setBody(body_data);
					client->setState(PROCESSING);
					_processRequest(client_fd);
				}
			}
		}
		else
		{
			client->getRequest().setBody("");
			client->setState(PROCESSING);
			_processRequest(client_fd);
		}
	}
	else if (client->getState() == READING_BODY)
	{
		if (client->getRequest().parseBody(client->getReadBuffer()))
		{
			client->setState(PROCESSING);
			_processRequest(client_fd);
		}
		else if (client->getRequest().isMalformed())
			_sendErrorResponse(client, 400, "Bad Request - Malformed chunked body");
	}
}

void	Webserver::_handleGetRequest(Client *client, Location *location, const std::string &fs_path, const std::string &uri)
{
	std::cout << "_handleGetRequest called" << std::endl;
	std::cout << "fs_path: " << fs_path << std::endl;

	struct stat	st;
	if (stat(fs_path.c_str(), &st) != 0)
	{
		std::cout << "File not found: " << fs_path << std::endl;
		_sendErrorResponse(client, 404, "Not Found");
		return ;
	}
	std::cout << "File exists! Size: " << st.st_size << " bytes" << std::endl;
	if (S_ISDIR(st.st_mode))
	{
		std::cout << "Is directory, handling autoundex..." << std::endl;
		if (!location->index.empty())
		{
			std::string	index_path = fs_path;
			if (fs_path[fs_path.size() - 1] != '/')
				index_path += '/';
			index_path += location->index;
			if (stat(index_path.c_str(), &st) == 0 && !S_ISDIR(st.st_mode))
			{
				std::cout << "Found index file: " << index_path << std::endl;
				_serveFile(client, index_path);
				return ;
			}
		}
		if (location->autoindex)
		{
			std::cout << "Generating directory listing..." << std::endl;
			std::string	listing = _router.generateDirectoryListing(fs_path, uri);
			std::string	response = _buildResponse(200, "OK", "text/html", listing);
			client->setSendBuffer(response);
			client->setState(SENDING_HEADERS);
			_updatePollEvents(client->getFd(), POLLIN | POLLOUT);
			return ;
		}
		else
		{
			_sendErrorResponse(client, 403, "Forbidden");
			return ;
		}
	}
	std::cout << "It's a file, calling _serveFile()..." << std::endl;
	_serveFile(client, fs_path);
}

void	Webserver::_handlePostRequest(Client *client, Location *location)
{
	std::cout << " _handlePostRequest called" << std::endl;
	std::cout << " upload_store: '" << location->upload_store << std::endl;
	std::cout << " upload_store.empty(): " << location->upload_store.empty() << std::endl;

	Request		&request = client->getRequest();
	std::string	body = request.getBody();

	if (location->path == "/" || location->upload_store.empty())
	{
		std::cout << " No upload_store, returning 200 OK" << std::endl;

		std::string	response_body = "<html><body>";
		response_body += "<h1>POST Received</h1>";
		response_body += "<p>Method: POST</p>";
		response_body += "<p>URI: " + request.getUri() + "</p>";
		response_body += "<p>Body: " + body + "</p>";
		response_body += "</body></html>";

		std::string	response = _buildResponse(200, "OK", "text/html", response_body);
		client->setSendBuffer(response);
		client->setState(SENDING_HEADERS);
		_updatePollEvents(client->getFd(), POLLIN | POLLOUT);
		return ;
	}
	std::string	content_type = request.getHeader("Content-Type");
	std::string	filename;
	std::string	content = body;

	if (content_type.find("multipart/form-data") != std::string::npos)
	{
		_handleMultipartUpload(client, *location, content_type, body);
		return ;
	}
	if (content_type.find("application/x-www-form-urlencoded") != std::string::npos)
		filename = "form_data_" + Parser::toString(time(NULL)) + ".txt";
	else if (content_type.find("text/plain") != std::string::npos)
		filename = "raw_data_" + Parser::toString(time(NULL)) + ".txt";
	else
		filename = "upload_" + Parser::toString(time(NULL)) + ".bin";
	_handleFileUpload(client, *location, filename, content);
}

void	Webserver::_handleDeleteRequest(Client *client, const std::string &fs_path)
{
	std::cout << "Delete request for: " + fs_path << std::endl;
	struct stat	st;
	if (stat(fs_path.c_str(), &st) != 0)
	{
		_sendErrorResponse(client, 404, "Not Found");
		return ;
	}
	if (S_ISDIR(st.st_mode))
	{
		_sendErrorResponse(client, 403, "Forbidden = Cannot delete directories");
		return ;
	}
	if (unlink(fs_path.c_str()) != 0) // needs alternative to unlink()
	{
		std::cerr << "Failed to delete this: " << fs_path << " (" << strerror(errno) << ")" <<std::endl;
		_sendErrorResponse(client, 500, "Internal Server Error");
		return ;
	}
	std::cout << "File deleted: " << fs_path << std::endl;
	std::string	response_body = "<html><body>";
	response_body += "<h1>204 No Content</h1>";
	response_body += "<p>File deleted successfully: " + fs_path + "</p>";
	response_body += "</body></html>";
	std::string	response = _buildResponse(204, "No Content", "text/html", response_body);
	client->setSendBuffer(response);
	client->setState(SENDING_HEADERS);
	_updatePollEvents(client->getFd(), POLLIN | POLLOUT);
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
	struct stat	st;
	if (stat(dir_path.c_str(), &st) != 0)
	{
		if (mkdir(dir_path.c_str(), 0755) != 0 && errno != EEXIST) // needs alternative to checking errno
		{
			std::cerr << "Failed to open file for writing: " << upload_path << std::endl;
			_sendErrorResponse(client, 500, "Internal Server Error");
			return ;
		}
	}
	std::ofstream	file(upload_path.c_str(), std::ios::binary);
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
	response_body += "<p> Size: " + Parser::toString(content.size()) + " bytes</p>";
	response_body += "</body></html>";
	std::string	response = _buildResponse(201, "Created", "text/html", response_body);
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
			response += "Content-Length: " + Parser::toString(st.st_size) + "\r\n";
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
		response += "Content-Length: " + Parser::toString(st.st_size) + "\r\n";
		response += "Connection: close\r\n";
		response += "\r\n";
	}
	client->setSendBuffer(response);
	client->setState(SENDING_HEADERS);
	_updatePollEvents(client->getFd(), POLLIN | POLLOUT);
	return ;
}

void	Webserver::_handleCGIRequest(Client *client, const Location &location,
			const std::string &script_path, const std::string &uri, Server *server,
			const std::string &interpreter)
{
	struct stat st;
	if (stat(script_path.c_str(), &st) != 0)
	{
		_sendErrorResponse(client, 404, "Not Found");
		return ;
	}
	if (S_ISDIR(st.st_mode))
	{
		_sendErrorResponse(client, 403, "Forbidden - CGI path is a directory");
		return ;
	}
	CGIHandler	*cgi = new CGIHandler();
	std::string server_name = server->server_names.empty() ? server->host : server->server_names[0];
	std::cout << "Starting CGI " << script_path << " via " << interpreter << std::endl;
	if (!cgi->start(client->getRequest(), location, script_path, uri, server_name, server->port, interpreter))
	{
		delete cgi;
		_sendErrorResponse(client, 500, "Internal Server Errror - CGI failed to start");
		return ;
	}
	client->setCgi(cgi);
	int	in_fd = cgi->getInputFd();
	if (in_fd >= 0)
	{
		struct pollfd	pfd = {in_fd, POLLOUT, 0};
		_poll_fds.push_back(pfd);
		_cgi_fd_to_client[in_fd] = client->getFd();
		client->setState(CGI_WRITING);
	}
	else
		client->setState(CGI_READING);

	int	out_fd = cgi->getOutputFd();
	struct pollfd	pfd_out = {out_fd, POLLIN, 0};
	_poll_fds.push_back(pfd_out);
	_cgi_fd_to_client[out_fd] = client->getFd();

	_updatePollEvents(client->getFd(), 0);
}

void	Webserver::_serveFile(Client *client, const std::string &path)
{
	std::ifstream	file(path.c_str(), std::ios::binary);
	if (!file.is_open())
	{
		_sendErrorResponse(client, 404, "Not Found");
		return ;
	}
	std::stringstream	buffer;
	buffer << file.rdbuf();
	std::string	content = buffer.str();
	file.close();
	std::string mime_type = _router.getMimeType(path);
	std::string	response = _buildResponse(200, "OK", mime_type, content);
	client->setSendBuffer(response);
	client->setState(SENDING_HEADERS);
	_updatePollEvents(client->getFd(), POLLIN | POLLOUT);
}
