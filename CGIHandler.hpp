#pragma once
#include "webserv.hpp"

class	Request;
class	Location;

enum	CGIState
{
	CGI_NOT_STARTED,
	CGI_WRITING_BODY,
	CGI_READING_OUTPUT,
	CGI_DONE,
	CGI_ERROR,
	CGI_TIMEOUT	
};

# define CGI_TIMEOUT_SECONDS 10

class	CGIHandler
{
	public:
		CGIHandler(void);
		CGIHandler(const CGIHandler &other);
		CGIHandler	&operator=(const CGIHandler &other);
		~CGIHandler(void);

		bool								start(const Request &request, const Location &location, const std::string &script_path, const std::string &uri,
											const std::string &server_name, int server_port, const std::string &interpreter);
		int									getInputFd(void) const;
		int									getOutputFd(void) const;
		CGIState							getState(void) const;
		bool								hasTimeOut(void);

		bool								writeToInput(void);
		bool								readFromOutput(void);

		bool								finalize(void);
		void								kill(void);
		void								forgetFd(int fd);
		
		const std::string					&getRawOutput(void) const;
		const std::string					getParsedBody(void) const;
		std::map<std::string, std::string>	getParsedHeaders(void) const;
		int									getStatusCode(void) const;
	private:
		pid_t								_pid;
		int									_input_fd;
		int									_output_fd;
		time_t								_start_time;
		CGIState							_state;
		std::string							_output_buffer;
		std::string							_request_body;
		size_t								_body_bytes_written;
		std::string							_parsed_body;
		std::map<std::string, std::string>	_parsed_headers;
		int									_status_code;

		char								**_buildEnv(const Request &request, const Location &location, const std::string &script_path, const std::string &uri, const std::string &server_name, int server_port);
		char								**_buildArgv(const std::string &interpreter, const std::string &script_path);
		char								**_buildArgv(const Location &location, const std::string &script_path);
		void								_freeArray(char **arr);
		void								_parseOutput(void);
		void								_closeInput(void);
		void								_closeOutput(void);
		char								*_makeString(const std::string str);
};
