#define OLC_PGE_APPLICATION
#define _WINSOCKAPI_
#define _CRT_SECURE_NO_WARNINGS
#include "olcPixelGameEngine.h"
#include <unordered_map>
#include <functional>
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <mutex>
#include <iomanip>
#include <algorithm>
#include <memory>

#include "SendTypes.h"

#pragma comment(lib, "ws2_32.lib")


#define DEFAULT_BUFF_LEN 512


typedef bool(olc::PixelGameEngine::* Screen)(float);
typedef bool(olc::PixelGameEngine::* BtnHandler)();


struct ui_element
{
	int w, h;
	int x, y;
	olc::Pixel mainColor, mouseOnColor;

	ui_element() :
		w(0), h(0), x(0), y(0), mainColor(olc::WHITE), mouseOnColor(olc::YELLOW) {}
	ui_element(int _w, int _h, int _x, int _y, olc::Pixel _mainColor = olc::WHITE, olc::Pixel _mouseOnColor = olc::YELLOW) :
		w(_w), h(_h), x(_x), y(_y), mainColor(_mainColor), mouseOnColor(_mouseOnColor) {}

	bool virtual MouseCollide(olc::PixelGameEngine* pge) {
		olc::vi2d mousePos = pge->GetMousePos();
		return mousePos.x >= x && mousePos.x <= x + w && mousePos.y >= y && mousePos.y <= y + h;
	}
	void virtual Render(olc::PixelGameEngine* pge) = 0;

	virtual ~ui_element() = default;
};

struct button : public ui_element
{
	std::string label;
	BtnHandler handler;

	button() :
		ui_element(), label(), handler(nullptr) {}
	button(int _w, int _h, int _x, int _y, const std::string _label, BtnHandler _handler = nullptr, olc::Pixel _mainColor = olc::WHITE, olc::Pixel _mouseOnColor = olc::YELLOW) :
		ui_element(_w, _h, _x, _y, _mainColor, _mouseOnColor), label(_label), handler(_handler) {}

	void SetHandler(BtnHandler _handler) { handler = _handler; }
	void Render(olc::PixelGameEngine* pge) override {
		olc::Pixel color = MouseCollide(pge) ? mouseOnColor : mainColor;
		pge->DrawLine(x, y, x + w, y, color);
		pge->DrawLine(x, y, x, y + h, color);
		pge->DrawLine(x, y + h, x + w, y + h, color);
		pge->DrawLine(x + w, y, x + w, y + h, color);
		int32_t stringSize = (int32_t)label.size() * 8;
		pge->DrawString((w - stringSize) / 2 + x, (h - 8) / 2 + y, label, color);
	}
	bool Handle(olc::PixelGameEngine* pge) {
		return std::invoke(handler, *pge);
	}
};

struct input : public ui_element
{
	std::string strInput;
	bool bSelected, bAllowed;
	std::function<bool(std::string)> validator;

	input() :
		ui_element(), strInput(), bSelected(false), bAllowed(true), validator(nullptr) {}
	input(int _w, int _h, int _x, int _y, std::function<bool(std::string)> _validator = nullptr, olc::Pixel _mainColor = olc::WHITE, olc::Pixel _mouseOnColor = olc::YELLOW) :
		ui_element(_w, _h, _x, _y, _mainColor, _mouseOnColor), strInput(), bSelected(false), bAllowed(true), validator(_validator) {}

	void Render(olc::PixelGameEngine* pge) override {
		pge->DrawLine(x, y, x + w, y, mainColor);
		pge->DrawLine(x, y, x, y + h, mainColor);
		pge->DrawLine(x, y + h, x + w, y + h, mainColor);
		pge->DrawLine(x + w, y, x + w, y + h, mainColor);
		pge->DrawString(x + 4, (h - 16) / 2 + y, strInput, mainColor, 2);
	}
	bool Validate() {
		if (validator == nullptr)
			return true;
		return validator(strInput);
	}
	void KeyPressed(olc::Key key, bool bShiftPressed) {
		if (!bAllowed)
			return;
		if (key >= olc::Key::A && key <= olc::Key::Z)
			strInput.push_back(key - olc::Key::A + (bShiftPressed ? 'A' : 'a'));
		if (key >= olc::Key::K0 && key <= olc::Key::K9)
			strInput.push_back(key - olc::Key::K0 + '0');
		if (key == olc::Key::COMMA)
			strInput.push_back(',');
		if (key == olc::Key::DOT)
			strInput.push_back('.');
		if (key == olc::Key::SPACE)
			strInput.push_back(' ');
		if (key == olc::Key::BACK)
			if (!strInput.empty()) strInput.pop_back();
	}
};


typedef std::shared_ptr<ui_element> ui_ptr;
typedef std::shared_ptr<button> button_ptr;
typedef std::shared_ptr<input> input_ptr;


class MemssagesClient : public olc::PixelGameEngine
{
public:
	MemssagesClient() {
		sAppName = "Client - Memsagges App";
	}

protected:
	Screen pPrevScreen = (Screen)&MemssagesClient::MainScreen;
	Screen pCurrentScreen = nullptr;
	std::unordered_map<std::string, Screen> mScreens = {
		{ "main", (Screen)&MemssagesClient::MainScreen },
		{ "error", (Screen)&MemssagesClient::ErrorScreen },
		{ "exit", (Screen)&MemssagesClient::ExitScreen },
		{ "chat", (Screen)&MemssagesClient::ChatScreen },
	};
	std::string errorMessage;
	std::unordered_map<std::string, std::vector<button_ptr>> mButtons;
	std::unordered_map<std::string, std::vector<input_ptr>> mInputs;

	WSADATA wsaData;
	addrinfo* result = NULL, * ptr = NULL, hints;
	SOCKET ConnectSocket;
	std::string addr, port, name;

	int token = 0, lastMsgId = 0;
	std::vector<std::string> messageHistory;
	std::string message;

	std::mutex mut;
	std::condition_variable cv;
	enum btn_command {
		NONE, EXIT, SEND, GET_USERS,
	} current_job;

public:
	bool OnUserCreate() override {
		auto validateIP = [](std::string input) {
			if (input.empty())
				return false;
			in_addr addr;
			return InetPtonA(AF_INET, input.c_str(), &addr) == 1;
		};
		auto validatePort = [](std::string input) {
			if (input.empty())
				return false;
			for (auto ch : input) {
				if (!isdigit(ch))
					return false;
			}
			int port = std::stoi(input);
			if (port > USHRT_MAX)
				return false;
			return true;
		};
		auto validateChatInput = [](std::string input) {
			return !input.empty();
		};
		auto validateName = [](std::string input) {
			if (input.empty())
				return false;
			if (input.find(' ') != std::string::npos)
				return false;
			if (input.find(',') != std::string::npos)
				return false;
			if (input.find('.') != std::string::npos)
				return false;

			return true;
		};

		mButtons = {
			{ "main", {
				button_ptr(new button(200, 70, ScreenWidth() / 2 + 10, 420, "Connect", (BtnHandler)& MemssagesClient::SubmitHandler)),
				button_ptr(new button(200, 70, ScreenWidth() / 2 - 210, 420, "Exit", (BtnHandler)& MemssagesClient::ExitHandler)),
			}},
			{ "error", {
				button_ptr(new button(200, 70, ScreenWidth() / 2 - 100, 420, "Okay", (BtnHandler)& MemssagesClient::ErrorHandler)),
			}},
			{ "chat", {
				button_ptr(new button(200, 40, ScreenWidth() / 2 - 310, 540, "Exit chat", (BtnHandler)& MemssagesClient::ExitChatHandler)),
				button_ptr(new button(200, 40, ScreenWidth() / 2 - 100, 540, "Send", (BtnHandler)& MemssagesClient::SendHandler)),
				button_ptr(new button(200, 40, ScreenWidth() / 2 + 110, 540, "Users", (BtnHandler)& MemssagesClient::UsersHandler)),
			}},
		};
		mInputs = {
			{ "main", {
				input_ptr(new input(400, 40, ScreenWidth() / 2 - 200, 160, validateName)),
				input_ptr(new input(400, 40, ScreenWidth() / 2 - 200, 240, validateIP)),
				input_ptr(new input(400, 40, ScreenWidth() / 2 - 200, 320, validatePort)),
			}},
			{ "chat", {
				input_ptr(new input(800, 40, ScreenWidth() / 2 - 400, 480, validateChatInput)),
			}},
		};

		pCurrentScreen = (Screen)&MemssagesClient::MainScreen;

		return true;
	}

	bool OnUserUpdate(float fElapsedTime) override {
		if (pCurrentScreen == nullptr)
			return false;

		return std::invoke(pCurrentScreen, *this, fElapsedTime);
	}


#pragma region Screens
public:
	bool MainScreen(float fElapsedTime) {
		Clear(olc::BLACK);
		auto& buttons = mButtons["main"];
		auto& inputs = mInputs["main"];

		auto calcX = [this](int nScale, std::string& str) {
			int width = (int32_t)str.size() * nScale * 8;
			return (ScreenWidth() - width) / 2;
		};

		if (GetMouse(0).bPressed) {
			for (auto& btn : buttons)
				if (btn->MouseCollide(this))
					btn->Handle(this);
			for (auto& in : inputs)
				in->bSelected = in->MouseCollide(this);
		}
		bool bShiftPressed = GetKey(olc::Key::SHIFT).bHeld;
		for (int i = 0; i <= olc::Key::DOT; i++) {
			olc::Key key = olc::Key(i);
			if (GetKey(key).bPressed)
				for (auto& in : inputs)
					if (in->bSelected)
						in->KeyPressed(key, bShiftPressed);
		}


		int nScale = 4;
		std::string string = "Welcome";
		olc::vi2d stringPos = { calcX(nScale, string), 40 };
		DrawString(stringPos, string, olc::WHITE, nScale);

		nScale = 2;
		string = "Enter your name:";
		stringPos = { calcX(nScale, string), 140 };
		DrawString(stringPos, string, olc::WHITE, nScale);

		nScale = 2;
		string = "Enter server IP:";
		stringPos = { calcX(nScale, string), 220 };
		DrawString(stringPos, string, olc::WHITE, nScale);

		nScale = 2;
		string = "Enter server port:";
		stringPos = { calcX(nScale, string), 300 };
		DrawString(stringPos, string, olc::WHITE, nScale);

		for (auto& btn : buttons)
			btn->Render(this);
		for (auto& in : inputs)
			in->Render(this);

		return true;
	}

	bool ChatScreen(float fElapsedTime) {
		Clear(olc::BLACK);
		auto& buttons = mButtons["chat"];
		auto& inputs = mInputs["chat"];

		if (GetMouse(0).bPressed) {
			for (auto& btn : buttons)
				if (btn->MouseCollide(this))
					btn->Handle(this);
			for (auto& in : inputs)
				in->bSelected = in->MouseCollide(this);
		}
		bool bShiftPressed = GetKey(olc::Key::SHIFT).bHeld;
		for (int i = 0; i <= olc::Key::DOT; i++) {
			olc::Key key = olc::Key(i);
			if (GetKey(key).bPressed)
				for (auto& in : inputs)
					if (in->bSelected)
						in->KeyPressed(key, bShiftPressed);
		}

		if (mut.try_lock()) {
			int nOffset = 0;
			for (auto it = messageHistory.rbegin(); it != messageHistory.rend(); it++) {
				nOffset += (int32_t)std::count(it->begin(), it->end(), '\n') + 1;
				DrawString(inputs[0]->x, inputs[0]->y - nOffset * 10, *it);
			}
			mut.unlock();
		}
		else {
			DrawString(inputs[0]->x, inputs[0]->y - 10, "Loading...");
		}

		for (auto& btn : buttons)
			btn->Render(this);
		for (auto& in : inputs)
			in->Render(this);

		return true;
	}

	bool ErrorScreen(float fElapsedTime) {
		Clear(olc::BLACK);
		auto& buttons = mButtons["error"];

		if (GetMouse(0).bPressed)
			for (auto& btn : buttons)
				if (btn->MouseCollide(this))
					btn->Handle(this);

		int nScale = 4;
		int width = (int32_t)errorMessage.size() * nScale * 8;
		DrawString((ScreenWidth() - width) / 2, (ScreenHeight() - nScale * 8) / 2, errorMessage, olc::WHITE, nScale);

		for (auto& btn : buttons)
			btn->Render(this);

		return true;
	}

	bool ExitScreen(float fElapsedTime) {
		return false;
	}
#pragma endregion

#pragma region ButtonHandlers
public:
	bool SubmitHandler() {
		auto inputs = mInputs["main"];

		if (inputs[0]->Validate() && inputs[1]->Validate() && inputs[2]->Validate()) {
			name = inputs[0]->strInput;
			addr = inputs[1]->strInput;
			port = inputs[2]->strInput;

			std::thread([&]() {
				if (!Connect()) {
					pCurrentScreen = mScreens["error"];
					closesocket(ConnectSocket);
					WSACleanup();
					goto exit;
				}

				if (!DoLogin()) {
					pCurrentScreen = mScreens["error"];
					closesocket(ConnectSocket);
					WSACleanup();
					goto exit;
				}

				pCurrentScreen = mScreens["chat"];

				while (true) {
					std::unique_lock<std::mutex> lk(mut);
					if (cv.wait_for(lk, std::chrono::seconds(1)) != std::cv_status::timeout) {
						bool ret;
						switch (current_job)
						{
						case btn_command::EXIT:
							DoLogout();
							goto exit;

						case btn_command::SEND:
							ret = DoSend(this->messageHistory, this->message);
							mInputs["chat"][0]->strInput = "";
							this->message = "";
							if (!ret) {
								pCurrentScreen = mScreens["error"];
								closesocket(ConnectSocket);
								WSACleanup();
								goto exit;
							}
							break;

						case btn_command::GET_USERS:
							ret = DoGetUsers(this->messageHistory);
							if (!ret) {
								pCurrentScreen = mScreens["error"];
								closesocket(ConnectSocket);
								WSACleanup();
								goto exit;
							}
							break;

						case btn_command::NONE:
						default:
							break;
						}
						current_job = btn_command::NONE;
						mInputs["chat"][0]->bAllowed = true;
					}

					while (true) {
						std::string msg;
						if (!DoGetMessage(msg, lastMsgId)) {
							pCurrentScreen = mScreens["error"];
							closesocket(ConnectSocket);
							WSACleanup();
							goto exit;
						}

						if (!msg.empty())
							messageHistory.push_back(msg);
						else
							break;
					}
				}

			exit:
				std::cout << "Thread ended" << std::endl;
				current_job = btn_command::NONE;
				mInputs["chat"][0]->bAllowed = true;
			}).detach();
		}
		else {
			errorMessage = "An error occured!";
			pPrevScreen = (Screen)&MemssagesClient::MainScreen;
			pCurrentScreen = mScreens["error"];
		}

		return true;
	}

	bool SendHandler() {
		auto inputs = mInputs["chat"];
		auto chat = inputs[0];
		auto now = std::chrono::system_clock::now();
		auto in_time_t = std::chrono::system_clock::to_time_t(now);
		std::stringstream ss;
		ss << std::put_time(std::localtime(&in_time_t), "%Y-%m-%d %X");
		std::string message = ss.str() + ": " + chat->strInput;

		if (current_job == btn_command::NONE) {
			current_job = btn_command::SEND;
			this->message = message;
			chat->bAllowed = false;
			cv.notify_one();
		}

		return true;
	}

	bool UsersHandler() {
		if (current_job == btn_command::NONE) {
			current_job = btn_command::GET_USERS;
			mInputs["chat"][0]->bAllowed = false;
			cv.notify_one();
		}

		return true;
	}

	bool ExitChatHandler() {
		current_job = btn_command::EXIT;
		cv.notify_one();
		messageHistory.clear();
		pCurrentScreen = mScreens["main"];
		return true;
	}

	bool ExitHandler() {
		pCurrentScreen = mScreens["exit"];
		return true;
	}

	bool ErrorHandler() {
		messageHistory.clear();
		token = 0;
		lastMsgId = 0;
		pCurrentScreen = pPrevScreen;
		return true;
	}
#pragma endregion

	bool Connect() {
		int iResult;

		iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
		if (iResult != 0) {
			errorMessage = "WSAStartup failed: " + std::to_string(iResult);
			pCurrentScreen = mScreens["error"];
			return false;
		}

		ZeroMemory(&hints, sizeof(hints));
		hints.ai_family = AF_UNSPEC;
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_protocol = IPPROTO_TCP;

		iResult = getaddrinfo(addr.c_str(), port.c_str(), &hints, &result);
		if (iResult != 0) {
			errorMessage = "getaddrinfo failed: " + std::to_string(iResult);
			pCurrentScreen = mScreens["error"];
			WSACleanup();
			return false;
		}

		ConnectSocket = INVALID_SOCKET;

		ptr = result;

		ConnectSocket = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
		if (ConnectSocket == INVALID_SOCKET) {
			errorMessage = "Error at socket(): " + std::to_string(WSAGetLastError());
			pCurrentScreen = mScreens["error"];
			freeaddrinfo(result);
			WSACleanup();
			return false;
		}

		iResult = connect(ConnectSocket, ptr->ai_addr, (int)ptr->ai_addrlen);
		if (iResult == SOCKET_ERROR) {
			closesocket(ConnectSocket);
			ConnectSocket = INVALID_SOCKET;
		}

		freeaddrinfo(result);

		if (ConnectSocket == INVALID_SOCKET) {
			errorMessage = "Unable to connect to server!";
			pCurrentScreen = mScreens["error"];
			WSACleanup();
			return false;
		}

		return true;
	}
	bool Disconnect() {
		// AAAAAAAAAAAAAAAAAAAAAAAAAAA
	}

#pragma region RecvFuncs
	bool RecvHeader(send_type& type, int& payload_size) {
		int iRecvResult;

		const int header_size = sizeof(send_type) + sizeof(int);
		char header[header_size];

		iRecvResult = recv(ConnectSocket, (char*)& header, sizeof(send_type) + sizeof(int), MSG_WAITALL);
		if (iRecvResult != sizeof(send_type) + sizeof(int)) {
			errorMessage = "recv (header) failed: " + std::to_string(WSAGetLastError());
			return false;
		}

		type = *(send_type*)header;
		payload_size = *(int*)(header + sizeof(send_type));

		return true;
	}
	bool RecvInt(int& num) {
		int iRecvResult;

		iRecvResult = recv(ConnectSocket, (char*)&num, sizeof(int), MSG_WAITALL);
		if (iRecvResult == sizeof(int))
			return true;

		errorMessage = "recv (int) failed: " + std::to_string(WSAGetLastError());

		return false;
	}
	bool RecvString(std::string& str, int payload_size) {
		int iRecvResult;
		char* recvbuf = new char[payload_size + 1ll];
		recvbuf[payload_size] = '\0';

		iRecvResult = recv(ConnectSocket, recvbuf, payload_size, MSG_WAITALL);
		if (iRecvResult == payload_size) {
			str = std::string(recvbuf);
			delete[] recvbuf;
			return true;
		}
		errorMessage = "recv (string) failed: " + std::to_string(WSAGetLastError());

		delete[] recvbuf;

		return false;
	}
	bool RecvTimestamp() {

	}
#pragma endregion

#pragma region SendFuncs
	bool SendHeader(send_type type, int payload_size) {
		int iSendResult;

		const int header_size = sizeof(send_type) + sizeof(int) + sizeof(int);
		char header[header_size];
		*(send_type*)header = type;
		*(int*)(header + sizeof(send_type)) = token;
		*(int*)(header + sizeof(send_type) + sizeof(int)) = payload_size;

		iSendResult = send(ConnectSocket, header, header_size, 0);
		if (iSendResult == SOCKET_ERROR) {
			errorMessage = "send (header) failed: " + std::to_string(WSAGetLastError());
			return false;
		}

		return true;
	}
	bool SendInt(int num) {
		bool bHeaderResult = SendHeader(send_type::INTEGER, sizeof(int));
		if (!bHeaderResult)
			return false;

		int iSendResult;

		iSendResult = send(ConnectSocket, (char*)&num, sizeof(int), 0);
		if (iSendResult == SOCKET_ERROR) {
			errorMessage = "send (int) failed: " + std::to_string(WSAGetLastError());
			return false;
		}

		return true;
	}
	bool SendString(std::string& str) {
		bool bHeaderResult = SendHeader(send_type::STRING, (int32_t)str.size());
		if (!bHeaderResult)
			return false;

		int iSendResult;

		iSendResult = send(ConnectSocket, str.c_str(), (int32_t)str.size(), 0);
		if (iSendResult == SOCKET_ERROR) {
			errorMessage = "send (string) failed: " + std::to_string(WSAGetLastError());
			return false;
		}

		return true;
	}
	bool SendLogin(std::string& name) {
		if (!SendHeader(send_type::LOGIN, name.size())) {
			errorMessage = "send (login) failed: " + std::to_string(WSAGetLastError());
			return false;
		}

		int iSendResult;

		iSendResult = send(ConnectSocket, name.c_str(), name.size(), 0);
		if (iSendResult == SOCKET_ERROR) {
			errorMessage = "send (login name) failed: " + std::to_string(WSAGetLastError());
			return false;
		}

		return true;
	}
	bool SendLogout() {
		bool bHeaderResult = SendHeader(send_type::LOGOUT, 0);
		if (!bHeaderResult) {
			errorMessage = "send (logout) failed: " + std::to_string(WSAGetLastError());
			return false;
		}
		return true;
	}
	bool SendGetUsers() {
		bool bHeaderResult = SendHeader(send_type::GET_USERS, 0);
		if (!bHeaderResult) {
			errorMessage = "send (get users) failed: " + std::to_string(WSAGetLastError());
			return false;
		}
		return true;
	}
	bool SendGetMessage(int num) {
		bool bHeaderResult = SendHeader(send_type::GET_MESSAGE, sizeof(int));
		if (!bHeaderResult)
			return false;

		int iSendResult;

		iSendResult = send(ConnectSocket, (char*) &num, sizeof(int), 0);
		if (iSendResult == SOCKET_ERROR) {
			errorMessage = "send (get message) failed: " + std::to_string(WSAGetLastError());
			return false;
		}

		return true;
	}
#pragma endregion

#pragma region SendRecvFuncs
	bool DoLogin() {
		if (!SendLogin(name))
			return false;
		send_type type;
		int payload_size;
		if (!RecvHeader(type, payload_size))
			return false;
		if (type != send_type::LOGIN && payload_size != sizeof(int) * 2)
			return false;

		if (!RecvInt(token))
			return false;
		if (!RecvInt(lastMsgId))
			return false;

		return true;
	}
	bool DoLogout() {
		SendLogout();
		return true;
	}
	bool DoGetMessage(std::string& str, int& msg_id) {
		if (!SendGetMessage(msg_id))
			return false;
		send_type type;
		int payload_size;
		if (!RecvHeader(type, payload_size))
			return false;
		if (type != send_type::STRING) {
			errorMessage = "Wrong header (get message)";
			return false;
		}
		if (payload_size == 0)
			return true;

		if (!RecvString(str, payload_size))
			return false;
		msg_id++;

		return true;
	}
	bool DoSend(std::vector<std::string>& vHistory, std::string msg) {
		std::string str;

		if (!SendString(msg))
			return false;

		send_type type;
		int payload_size;
		if (!RecvHeader(type, payload_size))
			return false;
		if (type != send_type::INTEGER) {
			errorMessage = "Wrong header type";
			return false;
		}
		if (payload_size != sizeof(int)) {
			errorMessage = "Wrong payload_size";
			return false;
		}

		int msgId;
		bool ret = RecvInt(msgId);
		while (lastMsgId < msgId) {
			// Get new messages
			std::string str;
			DoGetMessage(str, lastMsgId);
			vHistory.push_back(str);
		}
		// Get my own message
		DoGetMessage(str, lastMsgId);
		vHistory.push_back(msg);

		return true;
	}
	bool DoGetUsers(std::vector<std::string>& vHistory) {
		if (!SendGetUsers())
			return false;

		send_type type;
		int payload_size;
		if (!RecvHeader(type, payload_size))
			return false;
		if (type != send_type::STRING) {
			errorMessage = "Wrong header type (get users)";
			return false;
		}
		std::string users;
		if (!RecvString(users, payload_size))
			return false;

		vHistory.push_back(users);

		return true;
	}
#pragma endregion
};


int main(int argc, char** argv) {
	MemssagesClient client;
	if (client.Construct(1200, 600, 1, 1))
		client.Start();
	return 0;
}
