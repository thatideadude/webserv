CXX			= c++
CXXFLAGS	= -Wall -Wextra -Werror -std=c++98 -g
NAME		= webserv
SRCS		= main.cpp ConfigParser.cpp Webserver.cpp split.cpp 
OBJS		= $(SRCS:.cpp=.o)

all: $(NAME)

$(NAME): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $(NAME) $(OBJS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS)

fclean: clean
	rm -f $(NAME)

re: fclean all

.PHONY: all clean fclean re
