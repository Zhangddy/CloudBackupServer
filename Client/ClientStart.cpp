#include "Client.hpp"


constexpr auto STORE_FILE = "./list.txt";
constexpr auto LISTEN_DIR = "./backup";
constexpr auto SERVER_IP = "192.168.66.129";
constexpr auto SERVER_PORT = 9000;

int main()
{
	
	CloudClient client(LISTEN_DIR, STORE_FILE, SERVER_IP, SERVER_PORT);
	client.Start();
	return 0;
}