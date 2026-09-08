#include "webserv.hpp"
#include "CGIHandler.hpp"

Client::Client(int fd) : _fd(fd), _state(READING_HEADERS), _last_activity(time(NULL)), _bytes_sent(0), _keep_alive(true), _cgi(NULL), _listen_port(-1)
{
	std::cout << "Client constructor called with fd: " << fd << "\n";
}

Client::Client(const Client &other)
{
	std::cout << "Client copy constructor called\n";
	if (this != &other)
		*this = other;
}

Client	&Client::operator=(const Client &other)
{
	std::cout << "Client assgnment operator called\n";
	if (this != &other)
	{
		_fd = other._fd;
		_state = other._state;
		_last_activity = other._last_activity;
		_bytes_sent = other._bytes_sent;
		_keep_alive = other._keep_alive;
		_cgi = other._cgi;
		_listen_port = other._listen_port;
		_read_buffer = other._read_buffer;
		_send_buffer = other._send_buffer;
		_request = other._request;
		_response = other._response;
		_cgi = other._cgi;
	}
	return (*this);
}

Client::~Client(void)
{
	std::cout << "Client destructor called\n";
	if (_cgi)
	{
		_cgi->kill();
		delete _cgi;
	}
	if (_fd > 0)
		close(_fd);
}

int	Client::getFd(void) const
{
	return (_fd);
}

ClientState	Client::getState(void) const
{
	return (_state);
}

const std::string	&Client::getReadBuffer(void) const
{
	return (_read_buffer);
}

const std::string	&Client::getSendBuffer(void) const
{
	return (_send_buffer);
}

size_t	Client::getBytesSent(void) const
{
	return (_bytes_sent);
}

time_t	Client::getLastActivity(void) const
{
	return (_last_activity);
}

Request	&Client::getRequest(void)
{
	return (_request);
}

Response	&Client::getResponse(void)
{
	return (_response);
}

bool	Client::shouldKeepAlive(void) const
{
	return (_keep_alive);
}

bool	Client::isComplete(void) const
{
	return (_state == COMPLETE || _state == CLOSED);
}

CGIHandler	*Client::getCgi(void) const
{
	return (_cgi);
}

int	Client::getListenPort(void) const
{
	return (_listen_port);
}

void	Client::setListenPort(int port)
{
	_listen_port = port;
}

void	Client::setState(ClientState state)
{
	_state = state;
}

void	Client::setBytesSent(size_t bytes)
{
	_bytes_sent = bytes;
}

void	Client::setLastActivity(time_t time)
{
	_last_activity = time;
}

void	Client::setSendBuffer(const std::string &buffer)
{
	_send_buffer = buffer;
	_bytes_sent = 0;
}

void	Client::appendSendBuffer(const std::string &data)
{
	_send_buffer += data;
}

void	Client::setCgi(CGIHandler *cgi)
{
	_cgi = cgi;
}

void	Client::appendReadData(const char *data, size_t len)
{
	_read_buffer.append(data, len);
	_last_activity = time(NULL);
}

void	Client::clearReadBuffer(void)
{
	_read_buffer.clear();
}

void	Client::clearSendBuffer(void)
{
	_send_buffer.clear();
	_bytes_sent = 0;
}

void	Client::reset()
{
	_read_buffer.clear();
	_send_buffer.clear();
	_bytes_sent = 0;
	_state = READING_HEADERS;
	_last_activity = time(NULL);
}
