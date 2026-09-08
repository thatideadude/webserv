#pragma once

class CGIHandler;

enum	ClientState
{
	READING_HEADERS,
	READING_BODY,
	PROCESSING,
	CGI_WRITING,
	CGI_READING,
	SENDING_HEADERS,
	SENDING_BODY,
	COMPLETE,
	CLOSED
};

class	Client
{
	public:
		Client(int fd);
		Client(const Client &other);
		Client &operator=(const Client &other);
		~Client(void);

		int					getFd(void) const;
		ClientState			getState(void) const;
		const std::string	&getReadBuffer(void) const;
		const std::string	&getSendBuffer (void) const;
		size_t				getBytesSent(void) const;
		time_t				getLastActivity(void) const;
		Request				&getRequest(void);
		Response			&getResponse(void);
		bool				shouldKeepAlive(void) const;
		bool				isComplete(void) const;
		CGIHandler			*getCgi(void) const;
		int					getListenPort(void) const;

		void	setState(ClientState state);
		void	setBytesSent(size_t bytes);
		void	setLastActivity(time_t time);
		void	setSendBuffer(const std::string &buffer);
		void	appendSendBuffer(const std::string &data);
		void	setCgi(CGIHandler *cgi);
		void	setListenPort(int port);

		void	appendReadData(const char *date, size_t len);
		void	appendReadData(const std::string &date);
		void	clearReadBuffer(void);
		void	clearSendBuffer(void);
		void	reset(void);

	private:
		int			_fd;
		ClientState	_state;
		time_t		_last_activity;
		std::string	_read_buffer;
		std::string	_send_buffer;
		size_t		_bytes_sent;

		Request		_request;
		Response	_response;

		bool		_keep_alive;
		CGIHandler	*_cgi;
		int			_listen_port;
};
