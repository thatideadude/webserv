#include "webserv.hpp"

//	Webserver::Webserver(void)
//	{
//		std::cout << "Webserver default construcrtor called\n";
//	}

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
	}
}

void	Webserver::_processRequest(int client_fd)
{
	Client	*client = _clients[client_fd];

	if (!client)
		return ;

    std::cout << "Building response for client " << client_fd << std::endl;
	std::string response = "HTTP/1.1 200 OK\r\n";
	response += "Content-Type: text/html\r\n";
    response += "Content-Length: 20\r\n";
    response += "Connection: close\r\n";
    response += "\r\n";
    response += "<h1>Hello World!</h1>";

    std::cout << " Sending response (" << response.size() << " bytes)" << std::endl;
	client->setSendBuffer(response);
	client->setBytesSent(0);
	client->setState(SENDING_HEADERS);

	_updatePollEvents(client_fd, POLLIN | POLLOUT);
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

	while (it != _poll_fds.end())
	{
		if (it->fd == client_fd)
		{
			_poll_fds.erase(it);
			break ;
		}
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

void	Webserver::_handleClientData(int client_fd)
{
	Client*		client = _clients[client_fd];
	char		buffer[BUFFER_SIZE];

	if (!client)
	{
        std::cout << "⚠️  Client " << client_fd << " not found in _clients map!" << std::endl;
        return;
    }
	int			bytes = recv(client_fd, buffer, BUFFER_SIZE - 1, 0);
	if (bytes <= 0)
	{
        if (bytes == 0)
            std::cout << " Client " << client_fd << " disconnected" << std::endl;
        else if (errno != EWOULDBLOCK && errno != EAGAIN)
            std::cerr << "recv() error: " << strerror(errno) << std::endl;
		_removeClient(client_fd);
		return ;
	}
	buffer[bytes] = 0;
	client->appendReadData(buffer, bytes);
	client->setLastActivity(time(NULL));
    std::cout << " Received " << bytes << " bytes from client " << client_fd << std::endl;
    std::cout << " Data: " << buffer << std::endl;
	if (client->getState() == READING_HEADERS)
	{
		if (client->getRequest().parseHeaders(client->getReadBuffer()))
		{
			if (client->getRequest().hasBody())
				client->setState(READING_BODY);
			else
			{
				client->setState(PROCESSING);
				_processRequest(client_fd);
			}
		}
	}
	if (client->getState() == READING_BODY)
	{
		if (client->getRequest().parseBody(client->getReadBuffer()))
		{
				client->setState(PROCESSING);
				_processRequest(client_fd);
		}
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
    
    // Set non-blocking
    fcntl(client_fd, F_SETFL, O_NONBLOCK);
    
    // Create Client object
    Client* client = new Client(client_fd);
    client->setLastActivity(time(NULL));
    
    // --- CRITICAL: Add to poll ---
    struct pollfd pfd;
    pfd.fd = client_fd;
    pfd.events = POLLIN;      // Initially only reading
    pfd.revents = 0;
    _poll_fds.push_back(pfd);
    
    std::cout << "Added client fd " << client_fd << " to poll (total: " << _poll_fds.size() << ")" << std::endl;
    
    // Store in clients map
    _clients[client_fd] = client;
}

void	Webserver::run(void)
{
	_setupSockets();
	_running = true;
	while (_running)
	{
        std::cout << "Monitoring " << _poll_fds.size() << " fds: ";
        for (size_t i = 0; i < _poll_fds.size(); ++i)
            std::cout << _poll_fds[i].fd << " ";
        std::cout << std::endl;
		int	ret = poll(_poll_fds.data(), _poll_fds.size(), 1000);
		if (ret < 0)
		{
			if (errno == EINTR) // need to implement alternative to checking errno
				continue ;
			break;
		}
		if (ret > 0)
            std::cout << "poll() returned " << ret << " event(s)" << std::endl;
		size_t	i = 0;
		while (i < _poll_fds.size())
		{
			if (_poll_fds[i].revents & POLLIN)
			{
				std::map<int, Server>::iterator it = _server_sockets.find(_poll_fds[i].fd);
                std::cout << "POLLIN on fd " << _poll_fds[i].fd << std::endl;
				if (it != _server_sockets.end())
				{
					_acceptNewConnection(_poll_fds[i].fd);
                    std::cout << "Accepting new connection on server fd " << _poll_fds[i].fd << std::endl;
				}
				else
				{
					std::cout << "Reading data from client fd " << _poll_fds[i].fd << std::endl;
					_handleClientData(_poll_fds[i].fd);
				}
			}
			if (_poll_fds[i].revents & POLLOUT)
			{
                std::cout << "POLLOUT on fd " << _poll_fds[i].fd << std::endl;
				_handleClientWrite(_poll_fds[i].fd);
			}
			if (_poll_fds[i].revents & (POLLERR | POLLHUP | POLLNVAL))
			{
                std::cout << "Error on fd " << _poll_fds[i].fd << std::endl;
				_removeClient(_poll_fds[i].fd);
			}
			++i;
		}
		std::cout << "checking timeouts\n";
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
