#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <unikey.hpp>

void unikey_server()
{
	int serverSocket = socket(AF_INET, SOCK_STREAM, 0);

	struct sockaddr_in serverAddress;
	serverAddress.sin_family = AF_INET;
	serverAddress.sin_port = htons(8080);
	serverAddress.sin_addr.s_addr = INADDR_ANY;

	if (bind(serverSocket, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) != 0)
	{
		// Handle error here
	}
	

}