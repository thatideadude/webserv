#include "webserv.hpp"

Response::Response(void)
{
	std::cout << "Response default constructor called\n";
}

Response::Response(const Response &other)
{
	std::cout << "Response copy constructor called\n";
	*this = other;
}

Response	&Response::operator=(const Response &other)
{
	_status_line = other._status_line;
	_headers = other._headers;
	_body = other._body;
	return (*this);
}

Response::~Response(void)
{
	std::cout << "Response destructor called\n";
}

void	Response::clear(void)
{
	_status_line.clear();
	_headers.clear();
	_body.clear();
}

const std::string	&Response::getStatusLine(void) const
{
	return (_status_line);
}

const std::map<std::string, std::string>	&Response::getHeaders(void) const
{
	return (_headers);
}

const std::string	&Response::getBody(void) const
{
	return (_body);
}

std::string	Response::build(void) const
{
	std::string	result;
	std::map<std::string, std::string>::const_iterator	it = _headers.begin();
	
	result += _status_line + "\r\n";
	while (it != _headers.end())
	{
		result += it->first + ": " + it->second + "\r\n";
		++it;
	}
	result += "\r\n";
	result += _body;
	return (result);
}
