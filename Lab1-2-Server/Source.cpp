#pragma comment(lib, "ws2_32.lib")
#include <WinSock2.h>
#include <stdlib.h>
#include <Windows.h>
#include <iostream>
#include <WS2tcpip.h>
#include <vector>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <thread>
#include <mutex>
#include <fstream>

#include "../Lab1-2/SendTypes.h"


#define DEFAULT_PORT "4321"


int add_message(std::vector<std::pair<int, std::string>>& history, int token, char* msg, int& offset) {
	history.push_back({ token, msg });
	if (history.size() > 20) {
		history.erase(history.begin(), history.begin() + 10);
		offset += 10;
	}
	return offset + history.size();
}
std::string get_message(std::vector<std::pair<int, std::string>>& history, int id, int offset, std::unordered_map<int, std::pair<std::string, bool>> &users) {
	std::string message = users[history[id - offset].first].first;
	return message + ": " + history[id - offset].second;
}

int main(int argc, char** argv) {
	WSADATA wsaData;
	int iResult;

	int newId = 0;
	int msgId = 0;
	int msgOffset = 0;
	std::string strId, response;
	std::vector<std::pair<int, std::string>> messageHistory;
	std::unordered_map<int, std::pair<std::string, bool>> users;
	std::mutex mut;
	std::unordered_set<std::string> bannedIPs;

	std::string ipLine;
	std::ifstream bannedFile("banned.txt");
	if (bannedFile.is_open()) {
		while (getline(bannedFile, ipLine)) {
			if (!ipLine.empty())
				bannedIPs.insert(ipLine);
		}
		bannedFile.close();
	}

	iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (iResult != 0) {
		std::cout << "Error was occured at startup" << std::endl;
		return 1;
	}

	addrinfo* result = nullptr, * ptr = nullptr, hints;

	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_flags = AI_PASSIVE;

	iResult = getaddrinfo(NULL, DEFAULT_PORT, &hints, &result);
	if (iResult != 0) {
		std::cout << "getaddrinfo failed: " << iResult << std::endl;
		WSACleanup();
		return 1;
	}

	SOCKET ListenSocket = INVALID_SOCKET;
	ListenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
	if (ListenSocket == INVALID_SOCKET) {
		std::cout << "socket failed: " << iResult << std::endl;
		freeaddrinfo(result);
		WSACleanup();
		return 1;
	}

	iResult = bind(ListenSocket, result->ai_addr, (int)result->ai_addrlen);
	if (iResult == SOCKET_ERROR) {
		std::cout << "bind failed: " << iResult << std::endl;
		freeaddrinfo(result);
		closesocket(ListenSocket);
		WSACleanup();
		return 1;
	}

	if (listen(ListenSocket, SOMAXCONN) == SOCKET_ERROR) {
		std::cout << "listen failed: " << WSAGetLastError() << std::endl;
		closesocket(ListenSocket);
		WSACleanup();
		return 1;
	}

	// AFK user thread
	//std::thread([&]() {
	//	std::this_thread::sleep_for(std::chrono::seconds(10));
	//	{
	//		std::unique_lock<std::mutex> lk(mut);
	//		for (auto it = users.begin(); it != users.end();) {
	//			bool isActive = it->second.second;
	//			if (!isActive) it = users.erase(it);
	//			else { it->second.second = false; it++; }
	//		}
	//	}
	//}).detach();

	while (true) {
		SOCKET ClientSocket = INVALID_SOCKET;
		SOCKADDR_IN client_info = { 0 };
		int addrsize = sizeof(client_info);

		ClientSocket = accept(ListenSocket, (sockaddr*)&client_info, &addrsize);
		if (ClientSocket == INVALID_SOCKET) {
			std::cout << "accept failed: " << WSAGetLastError() << std::endl;
			closesocket(ListenSocket);
			WSACleanup();
			return 1;
		}

		std::cout << "Connection accepted: " << ClientSocket << std::endl;

		char ip[INET_ADDRSTRLEN];
		InetNtop(AF_INET, &client_info.sin_addr, ip, sizeof(ip));
		std::string str_ip(ip);

		std::cout << "IP: " << str_ip << std::endl;
		if (bannedIPs.find(str_ip) != bannedIPs.end()) {
			std::cout << "Try to connect from banned ip" << std::endl;

			closesocket(ClientSocket);
			continue;
		}

		// Start thread
		std::thread([&](SOCKET ClientSocket) {
			// Login
			int iRecvResult;
			int iSendResult;
			const int header_size = sizeof(send_type) + sizeof(int) + sizeof(int);
			char header[header_size];
			const int send_header_size = sizeof(send_type) + sizeof(int);
			char send_header[send_header_size];
			char* buffer;
			send_type type;
			int payload_size;
			int token;

			iRecvResult = recv(ClientSocket, header, header_size, MSG_WAITALL);
			if (iRecvResult != header_size) {
				std::cout << "Got wrong header from new user" << std::endl;
				closesocket(ClientSocket);
				return;
			}

			type = *(send_type*)(header);
			payload_size = *(int*)(header + sizeof(send_type) + sizeof(int));

			if (type != send_type::LOGIN) {
				std::cout << "First message must be login" << std::endl;
				closesocket(ClientSocket);
				return;
			}

			buffer = new char[payload_size + 1];
			buffer[payload_size] = '\0';

			iRecvResult = recv(ClientSocket, buffer, payload_size, MSG_WAITALL);
			if (iRecvResult != payload_size) {
				std::cout << "recv (login name) failed: " << WSAGetLastError();
				closesocket(ClientSocket);
				delete[] buffer;
				return;
			}

			std::string name(buffer);
			delete[] buffer;
			token = newId;
			newId++;
			{
				std::unique_lock<std::mutex> lk(mut);
				users[token] = { name, true };
			}

			*(send_type*)send_header = send_type::LOGIN;
			*(int*)(send_header + sizeof(send_type)) = sizeof(int) * 2;

			iSendResult = send(ClientSocket, (char*)& send_header, send_header_size, 0);
			if (iSendResult == SOCKET_ERROR) {
				std::unique_lock<std::mutex> lk(mut);
				users.erase(token);
				std::cout << "send (login header) failed: " << WSAGetLastError() << std::endl;
				closesocket(ClientSocket);
				return;
			}

			iSendResult = send(ClientSocket, (char*)& token, sizeof(int), 0);
			if (iSendResult == SOCKET_ERROR) {
				std::unique_lock<std::mutex> lk(mut);
				users.erase(token);
				std::cout << "send (login token) failed: " << WSAGetLastError() << std::endl;
				closesocket(ClientSocket);
				return;
			}

			msgId = messageHistory.size() + msgOffset;
			iSendResult = send(ClientSocket, (char*)& msgId, sizeof(int), 0);
			if (iSendResult == SOCKET_ERROR) {
				std::unique_lock<std::mutex> lk(mut);
				users.erase(token);
				std::cout << "send (login msg id) failed: " << WSAGetLastError() << std::endl;
				closesocket(ClientSocket);
				return;
			}

			while (true) {
				iRecvResult = recv(ClientSocket, header, header_size, MSG_WAITALL);
				if (iRecvResult != header_size) {
					std::unique_lock<std::mutex> lk(mut);
					users.erase(token);
					std::cout << "recv (header) failed: " << WSAGetLastError() << std::endl;
					closesocket(ClientSocket);
					return;
				}

				type = *(send_type*)header;
				token = *(int*)(header + sizeof(send_type));
				payload_size = *(int*)(header + sizeof(send_type) + sizeof(int));
				bool ret;

				users[token].second = true;

				std::string message;
				int num;

				switch (type)
				{
				case INTEGER:
					// unused
					break;

				case STRING:
					buffer = new char[payload_size + 1];
					buffer[payload_size] = '\0';
					iRecvResult = recv(ClientSocket, buffer, payload_size, MSG_WAITALL);
					if (iRecvResult != payload_size) {
						std::unique_lock<std::mutex> lk(mut);
						users.erase(token);
						std::cout << "recv (string) failed: " << WSAGetLastError() << std::endl;
						closesocket(ClientSocket);
						delete[] buffer;
						return;
					}
					msgId = add_message(messageHistory, token, buffer, msgOffset) - 1;
					delete[] buffer;

					*(send_type*)send_header = send_type::INTEGER;
					*(int*)(send_header + sizeof(send_type)) = sizeof(int);

					iSendResult = send(ClientSocket, send_header, send_header_size, 0);
					if (iSendResult == SOCKET_ERROR) {
						std::unique_lock<std::mutex> lk(mut);
						users.erase(token);
						std::cout << "send (header string) failed: " << WSAGetLastError() << std::endl;
						closesocket(ClientSocket);
						return;
					}
					iSendResult = send(ClientSocket, (char*)& msgId, sizeof(int), 0);
					if (iSendResult == SOCKET_ERROR) {
						std::unique_lock<std::mutex> lk(mut);
						users.erase(token);
						std::cout << "send (string) failed: " << WSAGetLastError() << std::endl;
						closesocket(ClientSocket);
						return;
					}
					break;

				case LOGOUT:
					{
						std::unique_lock<std::mutex> lk(mut);
						users.erase(token);
					}
					std::cout << "User logout: " << ClientSocket << " (id: " << token << ")" << std::endl;
					closesocket(ClientSocket);
					return;

				case GET_USERS:
					{
						message = "Users (" + std::to_string(users.size()) + "):";
						std::unique_lock<std::mutex> lk(mut);
						for (auto u : users) {
							message += '\n' + u.second.first;
						}
					}

					*(send_type*)send_header = send_type::STRING;
					*(int*)(send_header + sizeof(send_type)) = message.size();

					iSendResult = send(ClientSocket, send_header, send_header_size, 0);
					if (iSendResult == SOCKET_ERROR) {
						std::unique_lock<std::mutex> lk(mut);
						users.erase(token);
						std::cout << "send (header users) failed: " << WSAGetLastError() << std::endl;
						closesocket(ClientSocket);
						return;
					}
					iSendResult = send(ClientSocket, message.c_str(), message.size(), 0);
					if (iSendResult == SOCKET_ERROR) {
						std::unique_lock<std::mutex> lk(mut);
						users.erase(token);
						std::cout << "send (users) failed: " << WSAGetLastError() << std::endl;
						closesocket(ClientSocket);
						return;
					}
					break;

				case GET_MESSAGE:
					if (payload_size != sizeof(int)) {
						std::cout << "wrong payload (get msg)" << std::endl;
						closesocket(ClientSocket);
						return;
					}

					iRecvResult = recv(ClientSocket, (char*)& num, sizeof(int), MSG_WAITALL);
					if (iRecvResult != payload_size) {
						std::unique_lock<std::mutex> lk(mut);
						users.erase(token);
						std::cout << "recv (get msg) failed: " << WSAGetLastError() << std::endl;
						closesocket(ClientSocket);
						return;
					}

					if (num >= messageHistory.size() + msgOffset) {
						*(send_type*)send_header = send_type::STRING;
						*(int*)(send_header + sizeof(send_type)) = 0;

						iSendResult = send(ClientSocket, send_header, send_header_size, 0);
						if (iSendResult == SOCKET_ERROR) {
							std::unique_lock<std::mutex> lk(mut);
							users.erase(token);
							std::cout << "send (header get msg) failed: " << WSAGetLastError() << std::endl;
							closesocket(ClientSocket);
							return;
						}
					}
					else {
						message = get_message(messageHistory, num, msgOffset, users);
						*(send_type*)send_header = send_type::STRING;
						*(int*)(send_header + sizeof(send_type)) = message.size();

						iSendResult = send(ClientSocket, send_header, send_header_size, 0);
						if (iSendResult == SOCKET_ERROR) {
							std::unique_lock<std::mutex> lk(mut);
							users.erase(token);
							std::cout << "send (header get msg) failed: " << WSAGetLastError() << std::endl;
							closesocket(ClientSocket);
							return;
						}
						iSendResult = send(ClientSocket, message.c_str(), message.size(), 0);
						if (iSendResult == SOCKET_ERROR) {
							std::unique_lock<std::mutex> lk(mut);
							users.erase(token);
							std::cout << "send (get msg) failed: " << WSAGetLastError() << std::endl;
							closesocket(ClientSocket);
							return;
						}
					}

					break;

				case LOGIN:
				default:
					std::cout << "unknown header type" << std::endl;
					closesocket(ClientSocket);
					return;
				}
			}
		}, ClientSocket).detach();
	}

	WSACleanup();

	return 0;
}
