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
	std::cout << "Request assignment operator called\n";
	(void) other;
	return (*this);
}

Request::~Request(void)
{
	std::cout << "Request destructor called\n";
}

bool	Request::parseHeaders(const std::string &raw_data)
{
	std::cout << "parseHeaders called\n";
	std::cout << "raw_data size: " << raw_data.size() << std::endl;

	if (_headers_parsed)
		return (true);
	size_t	header_end = raw_data.find("\r\n\r\n");
	if (header_end == std::string::npos)
		return (true);
	std::string	header_part = raw_data.substr(0, header_end);
	std::cout << "header_part size: " << header_part.size() << std::endl;

	size_t	pos = 0;
	bool	first_line  = true;
	int		line_count = 0;

	while (pos < header_part.size())
	{
		size_t line_end = header_part.find("\r\n", pos);
		if (line_end == std::string::npos)
			break ;
		std::string	line = header_part.substr(pos, line_end - pos);
		pos = line_end + 2;
		line_count++;
		std::cout << "Line " << line_count << ": '" << line << "'" << std::endl;
		if (line.empty())
			continue ;
		if (first_line)
		{
			std::istringstream	line_ss(line);
			line_ss >> _method >> _uri >> _version;
			std::cout << "Method: " << _method << ", URI: " << _uri << std::endl;
			first_line = false;
		}
		else
		{
			size_t	colon = line.find(":");
			if (colon != std::string::npos)
			{
				std::string	key = line.substr(0, colon);
				std::string	value = line.substr(colon + 1);
				size_t	start = key.find_first_not_of(" \t");
				if (start != std::string::npos)
					key = key.substr(start);
				size_t	end = key.find_last_not_of(" \t");
				if (end != std::string::npos)
					key = key.substr(0, end + 1);
				start = value.find_first_not_of(" \t");
				if (start != std::string::npos)
					value = value.substr(start);
				end = value.find_last_not_of(" \t");
				if (end != std::string::npos)
					value = value.substr(0, end + 1);
				size_t	i = 0;
				while (i < key.size())
				{
					key[i] = std::tolower(key[i]);
					++i;
				}
				_headers[key] = value;
				std::cout << "Header: " << key << " = " << value << std::endl;
			}
		}
	}
	std::cout << "Parsed: " << line_count << " lines" << std::endl;
	std::string	cl = getHeader("content-length");
	if (!cl.empty())
		_content_length = std::atol(cl.c_str());
	_headers_parsed = true;
	return (true);
}

bool	Request::parseBody(const std::string &raw_data)
{
	std::cout << "parseBody() called" << std::endl;
	std::cout << "raw_data size: " << raw_data.size() << std::endl;
	std::cout << "_headers_parsed: " << _headers_parsed << std::endl;
	std::cout << "_body_parsed: " << _body_parsed << std::endl;
	std::cout << "_content_length: " << _content_length << std::endl;
	if (_body_parsed)
	{
		std::cout << "Already parsed, returning true" << std::endl;
		return (true);
	}
	if (!_headers_parsed)
	{
		std::cout << "Headers not parsed yet\n";
		return (false);
	}
	size_t header_end = raw_data.find("\r\n\r\n");
	if (header_end == std::string::npos)
	{
		std::cout << "No \\r\\n\\r\\n found in data\n";
		return (false);
	}
	std::string	body_data = raw_data.substr(header_end + 4);
	std::cout << "body_data size: " << body_data.size() << " bytes" << std::endl;
	if (_content_length > 0)
	{
		if (body_data.size() >= _content_length)
		{
			_body = body_data.substr(0, _content_length);
			_body_parsed = true;
			std::cout << "Body parsed: " << _body.size() << " bytes" << std::endl;
			std::cout << "Body content: '" << _body << "'" << std::endl;
			return (true);
		}
	}
	else if (_chunked)
	{
		_body += body_data;
		_body_parsed = true;
		return (true);
	}
	return (true);
}

void	Request::clear(void)
{
	_method.clear();
	_uri.clear();
	_version.clear();
	_headers.clear();
	_body.clear();
	_headers_parsed = false;
	_body_parsed = false;
	_content_length = 0;
	_chunked = 0;
	_body_bytes_read = 0;
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
	std::string key_lower = key;
	size_t	i = 0;

	while (i < key_lower.size())
	{
		key_lower = Parser::toLower(key_lower[i]);
		++i;
	}
	std::map<std::string, std::string>::const_iterator	it = _headers.find(key_lower);
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

void	Request::setBody(const std::string &body)
{
	_body = body;
	_body_parsed = true;
}

void	Request::setMethod(const std::string &method)
{
	_method = method;
}

void	Request::setUri(const std::string &uri)
{
	_uri = uri;
}

std::string	Request::decodeUri(const std::string &uri) const
{
	std::string	result;
	size_t	i = 0;

	while (i < uri.length())
	{
		if (uri[i] == '%' && i + 2 < uri.length())
		{
			char	hex[3] = {uri[i + 1], uri[i + 2], '\0'}; 
			int		value = std::strtol(hex, NULL, 16);
			result += static_cast<char>(value);
			i += 2;
		}
		else if (uri[i] == '+')
			result += ' ';
		else
			result += uri[i];
	}
	return (result);
}

bool	Request::_parseRequestLine(const std::string &line)
{
	std::cout << "Parsing request line: '" << line << "'" << std::endl;

	std::istringstream	stream(line);
	stream >> _method >> _uri >> _version;

	std::cout << "Method: '" << _method << "'" << std::endl;
	std::cout << "URI: '" << _uri << "'" << std::endl;
	std::cout << "Version: '" << _version << "'" << std::endl;

	if (_method.empty() || _uri.empty() || _version.empty())
		return (false);
	if (_method != "GET" && _method != "POST" && _method != "DELETE"
			&& _method != "HEAD" && _method != "PUT")
	{
		std::cout << "Unknown method: " << _method << std::endl;
		return (false);
	}
	return (true);
}

bool	Request::_parseHeaderLine(const std::string &line)
{
	std::cout << "Parsing header: '" << line << "'" << std::endl;

	size_t	colon = line.find(':');
	if (colon == std::string::npos)
	{
		std::cout << "No colon found in header" << std::endl;
		std::cout << "Line: '" << line << "'" << std::endl;
		return (false);
	}
	std::string	key = _trim(line.substr(0, colon));
	std::string	value = _trim(line.substr(colon + 1));
	size_t	i = 0;
	while (i < key.size())
	{
		key[i] = std::tolower(key[i]);
		++i;
	}
	std::cout << "Parsed: '" << key << "' = '" << value << "'" << std::endl;
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

void	Request::_parseUri(void)
{
	std::string	raw_uri = _uri;
	size_t	qmark = raw_uri.find('?');
	if (qmark != std::string::npos)
	{
		_path = raw_uri.substr(0, qmark);
		_query_string = raw_uri.substr(qmark + 1);
	}
	else
	{
		_path = raw_uri;
		_query_string = raw_uri.substr(qmark + 1);
	}
	_decoded_uri = decodeUri(_path);
}
