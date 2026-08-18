#include "webserv.hpp"

Client::Client(int fd) : _fd(fd), _state(READING_HEADERS), _last_activity(time(NULL)), _bytes_sent(0), _keep_alive(true)
{
	std::cout << "Client constructor called with fd: " << fd << "\n";
}

Client::Client(const Client &other)
{
	std::cout << "Client copy constructor called\n";
	*this = other;
}

Client	&Client::operator=(const Client &other)
{
	_fd = other._fd;
	//copy every attribute one by one
	return (*this);
}

Client::~Client(void)
{
	std::cout << "Client destructor called\n";
	if (_fd > 0)
		close(_fd);
}

int						Client::getFd(void) const
{
	return (_fd);
}

ClientState				Client::getState(void) const
{
	return (_state);
}

const std::string		&Client::getReadBuffer(void) const
{
	return (_read_buffer);
}

const std::string		&Client::getSendBuffer(void) const
{
	return (_send_buffer);
}

size_t					Client::getBytesSent(void) const
{
	return (_bytes_sent);
}

time_t					Client::getLastActivity(void) const
{
	return (_last_activity);
}

Request					&Client::getRequest(void)
{
	return (_request);
}

Response				&Client::getResponse(void)
{
	return (_response);
}

bool					Client::shouldKeepAlive(void) const
{
	return (_keep_alive);
}

bool					Client::isComplete(void) const
{
	return (_state == COMPLETE || _state == CLOSED);
}


void					Client::setState(ClientState state)
{
	_state = state;
}

void					Client::setBytesSent(size_t bytes)
{
	_bytes_sent = bytes;
}

void					Client::setLastActivity(time_t time)
{
	_last_activity = time;
}

void					Client::setSendBuffer(const std::string &buffer)
{
	_send_buffer = buffer;
	_bytes_sent = 0;
}

void					Client::appendSendBuffer(const std::string &data)
{
	_send_buffer += data;
}

void					Client::appendReadData(const char *data, size_t len)
{
	_read_buffer.append(data, len);
	_last_activity = time(NULL);
}

void					Client::appendReadData(const std::string &data)
{
	_read_buffer.append(data);
	_last_activity = time(NULL);
}

void					Client::clearReadBuffer(void)
{
	_read_buffer.clear();
}

void					Client::clearSendBuffer(void)
{
	_send_buffer.clear();
	_bytes_sent = 0;
}

void					Client::reset()
{
	_read_buffer.clear();
	_send_buffer.clear();
	_bytes_sent = 0;
	_state = READING_HEADERS;
	//_request.clear();
	//_response.clear();
	_last_activity = time(NULL);
}

void				Client::buildResponse(void)
{
	std::cout << "Building response for client " << _fd << std::endl;
}
