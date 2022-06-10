#include <sys/socket.h> // For socket functions
#include <netinet/in.h> // For sockaddr_in
#include <cstdlib> // For exit() and EXIT_FAILURE
#include <iostream> // For cout
#include <fstream>
#include <sstream>
#include <unistd.h> // For read
#include <errno.h>
#include <vector>
#include <cstdio>
#include "../includes/Request.hpp"
#include "../includes/Response.hpp"
#include "../includes/MimeParser.hpp"
#include "../includes/HttpCodesParser.hpp"
#include "../includes/ConfigParser.hpp"

#define BUFFER_SIZE 4000
#define PORT 8080
#define NB_OF_CLIENTS 10

#define ROOT "/index.html"

#define DOCUMENT 0
#define IMAGE 1

// Only need 1 mimeParser and 1 httpCodesParser
 MimeParser mimeParser(MIME_MAP_FILE);
HttpCodesParser httpCodesParser(HTTP_CODES_FILE);

int main(int argc, char const *argv[])
{
	std::vector<Server> servers;
	std::string configFile;
	if (argc < 2)
		configFile =  "./conf/webserv.conf";
	else
		configFile = argv[1];

	try
	{
		ConfigParser config(configFile);
		servers = config.parseConfig();
	}
	catch (FileNotFoundException &e)
	{
		std::cout << "Cannot open file " << configFile << std::endl;
		exit(EXIT_FAILURE);
	}

	Server t = servers[0];
	std::cout << "Server[0] locations " << t.routes.size() << ", error pages : " << t.errorPages.size() << std::endl;

	
	
	// Create a socket (IPv4, TCP)
	int sockfd = socket(AF_INET, SOCK_STREAM,0);
	if (sockfd == -1) 
	{
		std::cout << "Failed to create socket. errno: " << errno << std::endl;
		exit(EXIT_FAILURE);
	}
	else
		std::cout << "Opening socket : "<<  sockfd << std::endl;

	const int trueFlag = 1;
	if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &trueFlag, sizeof(int)) < 0)
		 std::cout << "sockopt failed " << errno << std::endl;

	// Listen to port 8080 on any address
	sockaddr_in sockaddr;
	sockaddr.sin_family = AF_INET;
	sockaddr.sin_addr.s_addr = INADDR_ANY;
	sockaddr.sin_port = htons(PORT); // htons is necessary to convert a number to
									// network byte order
	if (bind(sockfd, (struct sockaddr *)&sockaddr, sizeof(sockaddr)) < 0) 
	{
		std::cout << "Failed to bind to port " << PORT << ". errno: " << errno << std::endl;
		exit(EXIT_FAILURE);
	}
	else
	std::cout << "Binding socket : "<<  sockfd << std::endl;

	if(listen(sockfd,NB_OF_CLIENTS) < 0)
	{
		std::cout << "Failed to listen on socket " << PORT << ". errno: " << errno << std::endl;
		exit(EXIT_FAILURE);
	}
	else
	std::cout << "Listen on port "<< ntohs(sockaddr.sin_port) << std::endl << std::endl;

	

	


	while (1)
	{	
		//int connection;
		int addrlen = sizeof(sockaddr);
		int connection = accept(sockfd, (struct sockaddr*)&sockaddr, (socklen_t*) &addrlen);
		if (connection < 0)
		{
			std::cout << "Failed to grab connection. errno: " << errno << std::endl;
			exit(EXIT_FAILURE);
		}

		std::string raw_request;
		while (1)
		{
			//std::cout << "accepting a connection from : "<< sockaddr.sin_addr.s_addr  << std::endl;
			char buffer[BUFFER_SIZE] = {0};
			int r = recv(connection, buffer, BUFFER_SIZE, 0);
			if (r < 0)
				std::cout << "Nothing to read from client connection : " << ntohs(sockaddr.sin_addr.s_addr)  << std::endl;
			raw_request.append(buffer, r);
			
			size_t foundPos;
			if ((foundPos = raw_request.find("\r\n\r\n")) == std::string::npos)
			{
				//std::cout << "WAITING FOR ALL HEADER\n";			
				continue;
			}

			std::cout << "Header fully received\n";

			break;
		}

		size_t foundPos = raw_request.find("\r\n\r\n");
		int readContentBytes = (raw_request.size() - foundPos) - 4;
		Request pouet(raw_request);

		while(1)
		{
			if (pouet.getContentLength() != -1 && readContentBytes < pouet.getContentLength() && pouet.getContentLength() <= servers[0].maxBodySize) // Dont receeive if already read everything with one buffer (eg. small files)
			{
				char buffer[BUFFER_SIZE] = {0};
				int r = recv(connection, buffer, BUFFER_SIZE, 0);
				if (r < 0)
					std::cout << "Nothing to read from client connection : " << ntohs(sockaddr.sin_addr.s_addr)  << std::endl;
				raw_request.append(buffer, r);

				readContentBytes += r;
				if (readContentBytes != pouet.getContentLength())
					continue;
			}
					

			std::cout << "**************************** REQUEST RECEIVED ****************************" << std::endl;

			std::cout << raw_request;

			std::cout << "***************************************************************************" << std::endl << std::endl;

			//std::string request = test;
			// if (request.empty())
			// {
			// 	std::cout << "request is empty" << buffer << std::endl;
			// 	close(connection);
			// 	continue;
			// }
			
			Request request(raw_request);
			Response response(request, servers[0]);
			//std::string response = create_response(request);

			std::string responseStr;
			if (pouet.getContentLength() > servers[0].maxBodySize)
			{
				std::cout << "Body too big :" << pouet.getContentLength() << " bytes but the server only accepts " << servers[0].maxBodySize << " at most\n";
					
				responseStr = response.generateResponse(413, "gang-bang/errors/413.html");
			}
			else
			{
				responseStr = response.generateResponse();
			}
									

			std::cout << "----------------------------  SERVER RESPONSE ----------------------------" << std::endl;

			std::cout << responseStr.c_str();
			send(connection, responseStr.c_str(), responseStr.size(), 0);
			close(connection);

			std::cout << "---------------------------------------------------------------------------" << std::endl << std::endl;
			break;
		}
	}

	/////////////////////////////////////////////////////////////////////////////////////////

	close(sockfd);
}

