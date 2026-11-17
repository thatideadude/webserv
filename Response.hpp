#pragma once

class	Response
{
	public:
		Response(void);
		Response(const Response &other);
		Response &operator=(const Response &other);
		~Response(void);

		void	clear(void);

		const std::string	&getStatusLine(void) const;
		const std::string	&getBody(void) const;
		const std::map<std::string, std::string>	&getHeaders(void) const;

		void	setStatusLine(const std::string &status_line); // not implemented
		void	setHeader(const std::string &key, const std::string &value); // not implemented
		void	setBody(const std::string &body); // not implemented here, see Request.cpp

		std::string build(void) const;
	private:
		std::string	_status_line;
		std::string	_body;
		std::map<std::string, std::string>	_headers;
};
