#include "webserv.hpp"

Request::Request(void) : _headers_parsed(false), _body_parsed(false), _content_length(0), _chunked(false), _body_bytes_read(0)
{
	std::cout << "Request default constructor called\n";
}

Request::Request(const Request &other)
{
	std::cout << "Request copy constructor called\n";
	*this = other;
}

Request	&Request::operator=(const Request &other)
{
	(void) other;
	return (*this);
}

Request::~Request(void)
{
	std::cout << "Request destructor called\n";
}

bool	Request::parseHeaders(const std::string &raw_data)
{
	if (_headers_parsed)
		return (true);

	size_t header_end = raw_data.find("\r\n\r\n");
	if (header_end == std::string::npos)
		return (false);
	std::string			header_part = raw_data.substr(0, header_end);
	std::istringstream	stream(header_part);
	std::string			line;
	bool				first_line = true;

	while (std::getline(stream, line))
	{
		if (!line.empty() && line[line.size() - 1] == '\r')
			line.erase(line.size() - 1);
		if (first_line)
		{
			if (!_parseRequestLine(line))
				return (false);
			first_line = false;
		}
		else
		{
			if (!_parseHeaderLine(line))
				return (false);
		}
	}
	std::string	content_len = getHeader("Content-Length");
	if (!content_len.empty())
	{
		_content_length = std::atol(content_len.c_str());
		_body_bytes_read = 0;
	}
	std::string	transfer_encoding = getHeader("Transfer-Enconding");
	if (transfer_encoding.find("chunked") != std::string::npos)
		_chunked = true;
	std::string connection = getHeader("Connection");
	_headers_parsed = true;
	return (true);
}

bool	Request::parseBody(const std::string &raw_data)
{
	if (_body_parsed)
		return (true);
	size_t header_end = raw_data.find("\r\n\r\n");
	if (header_end == std::string::npos)
		return (false);
	std::string	body_data = raw_data.substr(header_end + 4);
	if (_chunked)
	{
		_body += body_data;
		_body_parsed = true;
		return (true);
	}
	else if (_content_length > 0)
	{
		if (body_data.size() >= _content_length)
		{
			_body = body_data.substr(0, _content_length);
			_body_parsed = true;
			return (true);
		}
		return (false);
	}
	_body_parsed = true;
	return (true);
}

void		Request::clear()
{
	_method.clear();
	_uri.clear();
	_version.clear();
	_headers.clear();
	_body.clear();
	_headers_parsed = false;
	_body_parsed = false;
	_content_length = 0;
	_chunked = false;
	_body_bytes_read = 0;
}

bool	Request::_parseRequestLine(const std::string &line)
{
	std::istringstream	stream(line);
	stream >> _method >> _uri >> _version;

	if (_method.empty() || _uri.empty() || _version.empty())
		return (false);
	if (_method != "GET" && _method != "POST" && _method != "DELETE"
		&& _method != "HEAD" && _method != "PUT")
		return (false);
	return (true);
}

bool	Request::_parseHeaderLine(const std::string &line)
{
	size_t	colon = line.find(':');
	if (colon == std::string::npos)
		return (false);
	std::string key = _trim(line.substr(0, colon));
	std::string value = _trim(line.substr(colon + 1));
	size_t	i = 0;
	while (i < key.size())
		key[i] = std::tolower(key[i]);
	_headers[key] = value;
	return (true);
}

std::string	Request::_trim(const std::string &str)
{
	size_t	start = str.find_first_not_of(" \t");
	if (start == std::string::npos)
		return ("");

	size_t	end = str.find_last_not_of(" \t\r\n");
	return (str.substr(start, end - start + 1));
}

const std::string	&Request::getMethod(void) const
{
	return (_method);
}

const std::string	&Request::getUri(void) const
{
	return (_uri);
}

const std::string	&Request::getVersion(void) const
{
	return (_version);
}

const std::map<std::string, std::string>	&Request::getHeaders(void) const
{
	return (_headers);
}

const std::string	&Request::getBody(void) const
{
	return (_body);
}

std::string	Request::getHeader(const std::string &key) const
{
	std::string	key_lower = key;
	size_t	i = 0;

	while (i < key_lower.size())
		key_lower[i] = std::tolower(key_lower[i]);
	std::map<std::string, std::string>::const_iterator it = _headers.find(key_lower);
	if (it != _headers.end())
		return (it->second);
	return ("");
}

bool	Request::hasBody(void) const
{
	return (_content_length > 0 || _chunked);
}

size_t	Request::getContentLength(void) const
{
	return (_content_length);
}

bool	Request::isChunked(void) const
{
	return (_chunked);
}

void	Request::setMethod(const std::string &method)
{
	_method = method;
}

void	Request::setUri(const std::string &uri)
{
	_uri = uri;
}
