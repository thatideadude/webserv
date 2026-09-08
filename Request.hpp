#pragma once

class	Request
{
	public:
		Request(void);
		Request(const Request &other);
		Request	&operator=(const Request &other);
		~Request(void);

		bool	parseHeaders(const std::string &raw_data);
		bool	parseBody(const std::string &raw_data);
		void	clear(void);

		const std::string	&getMethod(void) const;
		const std::string	&getUri(void) const;
		const std::string	&getVersion(void) const;
		const std::string	&getBody(void) const;
		std::string			getHeader(const std::string &key) const;
		const std::map<std::string, std::string>	&getHeaders(void) const;
		bool	hasBody(void) const;
		size_t	getContentLength(void) const;
		bool	isChunked(void) const;

		void	setBody(const std::string &body);
		void	setMethod(const std::string &method);
		void	setUri(const std::string &uri);

		std::string	decodeUri(const std::string &uri) const;
	private:
		bool		_parseRequestLine(const std::string &line);
		bool		_parseHeaderLine(const std::string &line);
		void		_parseUri(void);
		std::string	_trim(const std::string &str);

		std::string	_method;
		std::string	_uri;
		std::string	_version;
		std::string _body;
		std::map<std::string, std::string>	_headers;
		bool		_headers_parsed;
		bool		_body_parsed;
		size_t		_content_length;
		bool		_chunked;
		size_t		_body_bytes_read;
		std::string	_decoded_uri;
		std::string	_query_string;
		std::string	_path;
};
