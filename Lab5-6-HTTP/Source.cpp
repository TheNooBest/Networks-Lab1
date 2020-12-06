#define OLC_PGE_APPLICATION
#define _WINSOCKAPI_
#define _CRT_SECURE_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include "olcPixelGameEngine.h"
#include <unordered_map>
#include <functional>
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <mutex>
#include <algorithm>
#include <memory>
#include <bitset>
#include <iostream>
#include <sstream>
#include <string>
#include <any>

#pragma comment(lib, "ws2_32.lib")


#include <WinInet.h>

#pragma comment(lib, "wininet")


using namespace std::chrono_literals;


typedef bool(olc::PixelGameEngine::* Screen)(float);
typedef void(olc::PixelGameEngine::* BtnHandler)(std::any);


#pragma region UI_ELEMENTS
struct ui_element
{
	int w = 0;
	int h = 0;
	int x = 0;
	int y = 0;
	bool bActive = true;
	bool bSelected = false;
	olc::Pixel mainColor = olc::WHITE;
	olc::Pixel mouseOnColor = olc::YELLOW;
	olc::Pixel activeColor = olc::BLUE;
	BtnHandler handler;

	ui_element() :
		x(0), y(0), w(0), h(0), bActive(true), bSelected(false), mainColor(olc::WHITE), mouseOnColor(olc::YELLOW), activeColor(olc::BLUE), handler(nullptr) {}
	ui_element(int _x, int _y, int _w, int _h, bool _bActive = true, olc::Pixel _mainColor = olc::WHITE, olc::Pixel _mouseOnColor = olc::YELLOW, olc::Pixel _activeColor = olc::BLUE, BtnHandler _handler = nullptr) :
		x(_x), y(_y), w(_w), h(_h), bActive(_bActive), bSelected(false), mainColor(_mainColor), mouseOnColor(_mouseOnColor), activeColor(_activeColor), handler(_handler) {}

	bool virtual MouseCollide(olc::PixelGameEngine* pge) {
		olc::vi2d mousePos = pge->GetMousePos();
		return mousePos.x >= x && mousePos.x <= x + w && mousePos.y >= y && mousePos.y <= y + h;
	}
	// Must be overrided
	void virtual Render(olc::PixelGameEngine* pge) = 0;
	// You might or not to override
	void virtual OnClick(olc::PixelGameEngine* pge) {}
	void virtual OnMouseWheel(olc::PixelGameEngine* pge) {}
	void virtual Render(olc::PixelGameEngine* pge, int x, int y) {
		int _x = this->x;
		int _y = this->y;
		this->x = x;
		this->y = y;
		Render(pge);
		this->x = _x;
		this->y = _y;
	}

	virtual ~ui_element() = default;
};

struct label : public ui_element
{
	enum vPos {
		TOP,
		V_CENTER,
		BOTTOM
	} vPos;
	enum hPos {
		LEFT,
		H_CENTER,
		RIGHT
	} hPos;
	std::string text;

	label() :
		ui_element(), vPos(TOP), hPos(LEFT), text("") {}
	label(int _x, int _y, int _w, int _h, std::string _text = "", enum vPos _vPos = vPos::TOP, enum hPos _hPos = hPos::LEFT, olc::Pixel _mainColor = olc::WHITE) :
		ui_element(_x, _y, _w, _h, text.size() * 8, 8, true, _mainColor), text(_text), vPos(_vPos), hPos(_hPos) {}

	void Render(olc::PixelGameEngine* pge) override {
		int dx = 0, dy = 0;
		//switch (vPos)
		//{
		//case label::TOP:
		//	dy = 0;
		//	break;
		//case label::V_CENTER:
		//	dy = -4;
		//	break;
		//case label::BOTTOM:
		//	dy = -8;
		//	break;
		//default:
		//	break;
		//}
		//switch (hPos)
		//{
		//case label::LEFT:
		//	dx = 0;
		//	break;
		//case label::H_CENTER:
		//	dx = -4 * text.size();
		//	break;
		//case label::RIGHT:
		//	dx = -8 * text.size();
		//	break;
		//default:
		//	break;
		//}
		//pge->DrawString(x + dx, y + dy, text, mainColor);
		int chars_per_line = w / 8;
		int line_count = h / 8;
		//for (int i = 0; i * chars_per_line < text.size() && i < line_count; i++) {
		//	pge->DrawString(x, y + i * 8, text.substr(i * chars_per_line, chars_per_line));
		//}
		int line = 0;
		int pos = 0;
		for (int i = 0; i < text.size(); i++) {
			if (text[i] == '\n') {
				line++;
				pos = 0;
			}
			else if (text[i] == '\t') {
				do { pos++; } while (pos % 4);
				if (pos >= chars_per_line) {
					pos = 0;
					line++;
				}
			}
			else {
				char pseudostring[2] = { 0 };
				pseudostring[0] = text[i];
				pge->DrawString(x + pos * 8, y + line * 8, pseudostring);
				pos++;
				if (pos >= chars_per_line) {
					pos = 0;
					line++;
				}
			}
			if (line >= line_count) break;
		}
	}
};

struct button : public ui_element
{
	std::string label;

	button() :
		ui_element(), label() {}
	button(int _x, int _y, int _w, int _h, const std::string _label = "", BtnHandler _handler = nullptr, bool _bActive = true, olc::Pixel _mainColor = olc::WHITE, olc::Pixel _mouseOnColor = olc::YELLOW, olc::Pixel _activeColor = olc::BLUE) :
		ui_element(_x, _y, _w, _h, _bActive, _mainColor, _mouseOnColor, _activeColor, _handler), label(_label) {}

	void Render(olc::PixelGameEngine* pge) override {
		olc::Pixel color = MouseCollide(pge) ? mouseOnColor : mainColor;
		pge->DrawLine(x, y, x + w, y, color);
		pge->DrawLine(x, y, x, y + h, color);
		pge->DrawLine(x, y + h, x + w, y + h, color);
		pge->DrawLine(x + w, y, x + w, y + h, color);
		int32_t stringSize = (int32_t)label.size() * 8;
		pge->DrawString((w - stringSize) / 2 + x, (h - 8) / 2 + y, label, color);
	}
	void OnClick(olc::PixelGameEngine* pge) override {
		if (handler)
			std::invoke(handler, *pge, *this);
	}
};

struct input : public ui_element
{
	std::string strInput;
	std::function<bool(std::string)> validator;

	input() :
		ui_element(), strInput(), validator(nullptr) {}
	input(int _x, int _y, int _w, int _h, std::function<bool(std::string)> _validator = nullptr, bool _bActive = true, olc::Pixel _mainColor = olc::WHITE, olc::Pixel _mouseOnColor = olc::YELLOW) :
		ui_element(_x, _y, _w, _h, _bActive, _mainColor, _mouseOnColor), strInput(), validator(_validator) {}

	void Render(olc::PixelGameEngine* pge) override {
		olc::Pixel color = bSelected ? activeColor : MouseCollide(pge) ? mouseOnColor : mainColor;
		pge->DrawLine(x, y, x + w, y, color);
		pge->DrawLine(x, y, x, y + h, color);
		pge->DrawLine(x, y + h, x + w, y + h, color);
		pge->DrawLine(x + w, y, x + w, y + h, color);
		pge->DrawString(x + 4, (h - 16) / 2 + y, strInput, color, 2);
	}
	bool Validate() {
		if (validator == nullptr)
			return true;
		return validator(strInput);
	}
	void KeyPressed(olc::Key key, bool bShiftPressed) {
		if (!bActive)
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
		if (key == olc::Key::SLASH)
			strInput.push_back('/');
		if (key == olc::Key::BACK)
			if (!strInput.empty()) strInput.pop_back();
	}
};

struct scrollbar : public ui_element
{
	int current = 0;
	int bottom = 0;
	int top = 0;
	bool bReversed = false;

	scrollbar() :
		ui_element() {}
	scrollbar(int _x, int _y, int _w, int _h, int _max = 100, int _current = 0, int _min = 0, bool _bReversed = false, bool _bActive = true, olc::Pixel _mainColor = olc::WHITE, olc::Pixel _mouseOnColor = olc::YELLOW, olc::Pixel _activeColor = olc::BLUE) :
		ui_element(_x, _y, _w, _h, _bActive, _mainColor, _mouseOnColor, _activeColor), bottom(_min), top(_max), current(_current), bReversed(_bReversed) {}

	void virtual Increase() {
		current = min(current + 1, top);
	}
	void virtual Decrease() {
		current = max(current - 1, bottom);
	}
};

struct scrollbar_v : public scrollbar
{
	int drawY = 0;

	scrollbar_v() :
		scrollbar(), drawY(0) {}
	scrollbar_v(int _x, int _y, int _w, int _h, int _max = 100, int _current = 0, int _min = 0, bool _bReversed = false, bool _bActive = true) :
		scrollbar(_x, _y, _w, _h, _max, _current, _min, _bReversed, _bActive), drawY(_y) {
		if (top <= bottom) return;
		drawY = y + (h - w) * (current - bottom) / (top - bottom);
	}

	void Increase() override {
		if (top <= bottom) return;
		bReversed ? scrollbar::Decrease() : scrollbar::Increase();
		drawY = y + (h - w) * (current - bottom) / (top - bottom);
	}
	void Decrease() override {
		if (top <= bottom) return;
		bReversed ? scrollbar::Increase() : scrollbar::Decrease();
		drawY = y + (h - w) * (current - bottom) / (top - bottom);
	}
	void Render(olc::PixelGameEngine* pge) override {
		pge->FillRect(x, y, w, h, olc::GREY);
		pge->FillRect(x, drawY, w, w, olc::DARK_GREY);
	}
};

struct scrollbar_h : public scrollbar
{
	int drawX = 0;

	scrollbar_h() :
		scrollbar(), drawX(0) {}
	scrollbar_h(int _x, int _y, int _w, int _h, int _max = 100, int _current = 0, int _min = 0, bool _bReversed = false, bool _bActive = true) :
		scrollbar(_x, _y, _w, _h, _max, _current, _min, _bReversed, _bActive), drawX(_x) {
		int maxW = h - w;
		drawX = x + (maxW * (current - bottom)) / (top - bottom);
	}

	void Increase() override {
		if (top <= bottom) return;
		bReversed ? scrollbar::Decrease() : scrollbar::Increase();
		drawX = x + (w - h) * (current - bottom) / (top - bottom);
	}
	void Decrease() override {
		if (top <= bottom) return;
		bReversed ? scrollbar::Increase() : scrollbar::Decrease();
		drawX = x + (w - h) * (current - bottom) / (top - bottom);
	}
	void Render(olc::PixelGameEngine* pge) override {
		pge->FillRect(x, y, w, h, olc::GREY);
		pge->FillRect(drawX, y, h, h, olc::DARK_GREY);
	}
};

struct scrollable : public ui_element
{
	olc::Sprite frame;

	scrollable() :
		ui_element(), frame() {}
	scrollable(int _x, int _y, int _w, int _h, int _frame_w, int _frame_h, bool _bActive = true, olc::Pixel _mainColor = olc::WHITE, olc::Pixel _mouseOnColor = olc::YELLOW, olc::Pixel _activeColor = olc::BLUE) :
		ui_element(_x, _y, _w, _h, _bActive, _mainColor, _mouseOnColor, _activeColor), frame(_frame_w, _frame_h) {}
};

struct scrollable_v : public scrollable
{
	// add scrollbar
};

struct scrollable_h : public scrollable
{
	// add scrollbar
};

struct scrollable_vh : public scrollable
{
	scrollbar_h scroll_h;
	scrollbar_v scroll_v;

	scrollable_vh() :
		scrollable(), scroll_h(), scroll_v() {}
	scrollable_vh(int _x, int _y, int _w, int _h, bool _bActive = true, olc::Pixel _mainColor = olc::WHITE, olc::Pixel _mouseOnColor = olc::YELLOW, olc::Pixel _activeColor = olc::BLUE) :
		scrollable(_x, _y, _w, _h, _w - 10, _h - 10, _bActive, _mainColor, _mouseOnColor, _activeColor), scroll_h(_x, _y + _h - 10, _w - 10, 10), scroll_v(_x + _w - 10, _y, 10, _h - 10) {}

	void Render(olc::PixelGameEngine* pge) override {
		auto layer = pge->GetDrawTarget();
		pge->SetDrawTarget(&frame);
		pge->Clear(olc::BLANK);

		scroll_h.Render(pge);
		scroll_v.Render(pge);

		pge->SetDrawTarget(layer);
		pge->DrawSprite(x, y, &frame);
	}
	void OnMouseWheel(olc::PixelGameEngine* pge) override {
		if (pge->GetKey(olc::Key::CTRL).bHeld)
			pge->GetMouseWheel() > 0 ? scroll_h.Increase() : scroll_h.Decrease();
		else
			pge->GetMouseWheel() > 0 ? scroll_v.Increase() : scroll_v.Decrease();
	}
};

struct row : public ui_element
{
	const static int row_height = 12;
	const static int row_width = 0;
	const static inline std::vector<int> vd = {};

	struct data;
	struct inner_data;

	row() :
		ui_element() {}
	row(int _x, int _y) :
		ui_element(_x, _y, row_width, row_height) {}
	row(int _x, int _y, int _w, int _h) :
		ui_element(_x, _y, _w, _h) {}
	void virtual RenderHeader(olc::PixelGameEngine* pge) = 0;
	void virtual Clear() = 0;
};

struct row_ftp : public row
{
	const static int row_width = 144;
	const static inline std::vector<int> vd = { 16, 0 };

	enum obj_type : uint8_t {
		dir = 0,
		file = 1,
	};

	struct data {
		obj_type type;
		char name[64];
	} info = { dir };

	row_ftp() :
		row() {}
	row_ftp(int x, int y) :
		row(x, y, row_width, row_height) {}
	row_ftp(int x, int y, data& data) :
		row(x, y), info(data) {}

	void Render(olc::PixelGameEngine* pge) override {
		std::string t;
		switch (info.type)
		{
		case obj_type::dir:
			t = "d";
			break;
		case obj_type::file:
			t = "f";
			break;
		default:
			t = "u";
		}

		pge->DrawString(x, y + 2, t);
		pge->DrawString(x + vd[0], y + 2, info.name);
		pge->DrawLine(x, y + row_height, x + row_width, y + row_height);
	}
	void Render(olc::PixelGameEngine* pge, int x, int y) override {
		std::string t;
		switch (info.type)
		{
		case obj_type::dir:
			t = "d";
			break;
		case obj_type::file:
			t = "f";
			break;
		default:
			t = "u";
		}

		pge->DrawString(x, y + 2, t);
		pge->DrawString(x + vd[0], y + 2, info.name);
		pge->DrawLine(x, y + row_height, x + row_width, y + row_height);
	}
	void RenderHeader(olc::PixelGameEngine* pge) override {
		pge->DrawString(x, y + 2, "t");
		pge->DrawString(x + vd[0], y + 2, "name");
		pge->DrawLine(x, y + row_height, x + row_width, y + row_height);
	}
	void OnClick(olc::PixelGameEngine* pge) override {
		if (handler)
			std::invoke(handler, *pge, *this);
	}
	static int Delimeters(int pos) {
		return vd[pos] - 4;
	}
	void Clear() override {

	}
};

// thread-safe
template<typename T>
struct table : public scrollable_vh
{
	static_assert(std::is_base_of<row, T>::value, "Template argument must be derived from 'struct row'");

	T header;
	std::vector<T> rows;
	BtnHandler rowHandler;
	std::mutex mut;

	table() :
		scrollable_vh(), header() {}
	table(int _x, int _y, int _w, int _h, bool _bActive = true, olc::Pixel _mainColor = olc::WHITE, olc::Pixel _mouseOnColor = olc::YELLOW, olc::Pixel _activeColor = olc::BLUE, BtnHandler _rowHandler = nullptr) :
		scrollable_vh(_x, _y, _w, _h, _bActive, _mainColor, _mouseOnColor, _activeColor), header(0, 0), rowHandler(_rowHandler) {
		scroll_v.top = -frame.height / T::row_height + 1;
		scroll_v.current = 0;
		scroll_v.bReversed = true;
	}

	~table() {
		std::lock_guard<std::mutex> lg(mut);
		for (auto& r : rows)
			r.Clear();
	}

	void push_back(typename T::data& _data) {
		std::lock_guard<std::mutex> lg(mut);
		auto& ret = rows.emplace_back(0, (rows.size() + 1) * T::row_height, _data);
		ret.handler = rowHandler;
		scroll_v.top++;
	}
	//void push_back(typename T::data& _data, typename T::inner_data& _inner) {
	//	std::lock_guard<std::mutex> lg(mut);
	//	auto& ret = rows.emplace_back(0, (rows.size() + 1) * T::row_height, _data);
	//	ret.handler = rowHandler;
	//	ret.inner = _inner;
	//	scroll_v.top++;
	//}
	void clear() {
		std::lock_guard<std::mutex> lg(mut);
		rows.clear();
	}
	void Render(olc::PixelGameEngine* pge) override {
		std::lock_guard<std::mutex> lg(mut);
		auto layer = pge->GetDrawTarget();
		pge->SetDrawTarget(&frame);
		pge->Clear(olc::BLACK);

		header.RenderHeader(pge);
		int i = 1;
		for (int row = scroll_v.current; i <= (frame.height + T::row_height - 1) / T::row_height && row < rows.size(); i++, row++)
			rows[row].Render(pge, 0, i * T::row_height);
		for (int j = 0, _x; (_x = T::Delimeters(j)) >= 0; j++)
			pge->DrawLine(_x, 0, _x, i * T::row_height);

		pge->SetDrawTarget(layer);

		pge->DrawSprite(x, y, &frame);
		scroll_h.Render(pge);
		scroll_v.Render(pge);
	}
	void OnClick(olc::PixelGameEngine* pge) override {
		std::lock_guard<std::mutex> lg(mut);
		std::cout << "Table clicked!\n";
		// 1 calc what row was clicked
		int row = (pge->GetMouseY() - y) / T::row_height + scroll_v.current - 1;
		std::cout << "Row: " << row << std::endl;
		// check if clicked empty space
		if (row >= rows.size())
			return;
		// inkoke handler
		rows[row].OnClick(pge);
	}
};
#pragma endregion


class HttpClient : public olc::PixelGameEngine
{
public:
	HttpClient() {
		sAppName = "HTTP Client";
	}


protected:
	Screen current_screen = nullptr;
	std::unordered_map<std::string, Screen> screens;
	std::string errorMessage;

	// ui elements
	std::unordered_map<std::string, std::vector<std::shared_ptr<ui_element>>> uis;		// all app uis
	std::vector<std::shared_ptr<ui_element>> current_uis;								// ui on current screen
	std::vector<std::shared_ptr<label>> labels;											// labels on current screen
	std::vector<std::shared_ptr<button>> buttons;										// buttons on current screen
	std::vector<std::shared_ptr<input>> inputs;											// inputs on current screen
	std::vector<std::shared_ptr<ui_element>> tmp_uis;									// uis must deleted

	// data
	SOCKET sniffer = SOCKET_ERROR;
	ADDRINFO* addrinfo = NULL;
	WSADATA wsa;
	SOCKADDR_IN dest;
	IN_ADDR addr;
	int in;
	std::string server_url, path_url;


public:
	bool OnUserCreate() override {
		screens = {
			{ "start",	(Screen)&HttpClient::StartScreen	},
			{ "main",	(Screen)&HttpClient::MainScreen		},
			{ "error",	(Screen)&HttpClient::ErrorScreen	},
		};

		auto inputAddr = std::make_shared<input>(100, ScreenHeight() - 80, ScreenWidth() - 200, 20);
		auto btnSubmit = std::make_shared<button>((ScreenWidth() - 100) / 2 + 60, ScreenHeight() - 40, 100, 20, "Submit", (BtnHandler)&HttpClient::SubmitHandler);
		auto btnFtp = std::make_shared<button>((ScreenWidth() - 100) / 2 - 60, ScreenHeight() - 40, 100, 20, "Ftp", (BtnHandler)&HttpClient::FtpHandler);
		uis["start"].push_back(inputAddr);
		uis["start"].push_back(btnSubmit);
		uis["start"].push_back(btnFtp);

		auto lblMain = std::make_shared<label>(0, 0, ScreenWidth(), ScreenHeight() - 60);
		auto btnBack = std::make_shared<button>((ScreenWidth() - 100) / 2, ScreenHeight() - 40, 100, 20, "Back", (BtnHandler)&HttpClient::BackHandler);
		uis["main"].push_back(lblMain);
		uis["main"].push_back(btnBack);

		auto btnToStart = std::make_shared<button>((ScreenWidth() - 100) / 2, ScreenHeight() - 40, 100, 20, "Back", (BtnHandler)&HttpClient::BackHandler);
		uis["error"].push_back(btnToStart);

		ChangeScreen("start");

		return true;
	}

	bool OnUserUpdate(float fElapsedTime) override {
		if (!current_screen)
			return false;

		Clear(olc::BLACK);

		// Controls
		if (GetMouse(0).bPressed) {
			for (auto& ui : current_uis)
				if (ui->MouseCollide(this))
					ui->OnClick(this);
			for (auto& ui : tmp_uis)
				if (ui->MouseCollide(this))
					ui->OnClick(this);
			for (auto& in : inputs)
				in->bSelected = in->MouseCollide(this);
		}
		if (GetMouseWheel() != 0) {
			for (auto& ui : current_uis)
				if (ui->MouseCollide(this))
					ui->OnMouseWheel(this);
			for (auto& ui : tmp_uis)
				if (ui->MouseCollide(this))
					ui->OnMouseWheel(this);
		}
		bool bShiftPressed = GetKey(olc::Key::SHIFT).bHeld;
		for (int i = 0; i <= olc::Key::SLASH; i++) {
			olc::Key key = olc::Key(i);
			if (GetKey(key).bPressed)
				for (auto& in : inputs)
					if (in->bSelected)
						in->KeyPressed(key, bShiftPressed);
		}

		// Run screen logic
		std::invoke(current_screen, *this, fElapsedTime);

		// Draw
		for (auto& ui : current_uis)
			ui->Render(this);
		for (auto& ui : tmp_uis)
			ui->Render(this);

		return true;
	}

protected:
	template<typename T>
	void Extract(std::vector<std::shared_ptr<T>>& v, std::vector<std::shared_ptr<ui_element>>& uis) {
		v.clear();
		for (auto& ui : uis) {
			T* item = dynamic_cast<T*>(ui.get());
			if (item) {
				v.emplace_back(std::static_pointer_cast<T>(ui));
			}
		}
	}

	void ChangeScreen(std::string screenName) {
		current_screen = screens[screenName];
		if (current_screen == nullptr) {
			screenName = "error";
			errorMessage = "No such screen";
			current_screen = screens[screenName];
		}
		current_uis = uis[screenName];
		Extract(labels, current_uis);
		Extract(buttons, current_uis);
		Extract(inputs, current_uis);
		tmp_uis.clear();
	}

	bool StartScreen(float fElapsedTime) {
		return true;
	}

	bool MainScreen(float fElapsedTime) {
		return true;
	}

	bool ErrorScreen(float fElapsedTime) {
		DrawString(0, 0, errorMessage);
		return true;
	}


	void ExitHandler(std::any a) {

	}

	void FtpHandler(std::any a) {
		server_url = inputs[0]->strInput;
		auto ftp_table = std::make_shared<table<row_ftp>>(0, 0, ScreenWidth(), row_ftp::row_height * 11);
		ftp_table->rowHandler = (BtnHandler)&HttpClient::RowFtpHandler;

		ChangeScreen("main");

		std::thread([&](std::shared_ptr<table<row_ftp>> ftp_table) {
			HINTERNET internet = InternetOpenA(NULL, INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
			HINTERNET ftp_session = InternetConnectA(internet, server_url.c_str(), INTERNET_DEFAULT_FTP_PORT, NULL, NULL, INTERNET_SERVICE_FTP, 0, 0);

			if (ftp_session == NULL) {
				errorMessage = "ftp error: " + std::to_string(GetLastError());
				InternetCloseHandle(internet);
				ftp_table.reset();
				ChangeScreen("error");
				return;
			}

			WIN32_FIND_DATAA winFindData;
			HINTERNET ftp_next = FtpFindFirstFileA(ftp_session, NULL, &winFindData, NULL, NULL);
			row_ftp::data d = {
				winFindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ?
				row_ftp::obj_type::dir :
				row_ftp::obj_type::file
			};
			std::memcpy(d.name, winFindData.cFileName, 63);
			winFindData = { 0 };
			ftp_table->push_back(d);
			while (InternetFindNextFileA(ftp_next, &winFindData)) {
				row_ftp::data d = {
					winFindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ?
					row_ftp::obj_type::dir :
					row_ftp::obj_type::file
				};
				std::memcpy(d.name, winFindData.cFileName, 63);
				winFindData = { 0 };
				ftp_table->push_back(d);
			}
			if (GetLastError() != ERROR_NO_MORE_FILES) {
				std::cout << "jopa: " << GetLastError() << std::endl;
			}

			InternetCloseHandle(ftp_session);
			InternetCloseHandle(internet);
		}, ftp_table).detach();

		tmp_uis.push_back(ftp_table);
	}

	void SubmitHandler(std::any a) {
		std::string url = inputs[0]->strInput;

		WSADATA wsaData;
		SOCKET sock;
		SOCKADDR_IN sockAddr;
		int lineCount = 0;
		int rowCount = 0;
		hostent* host;
		std::string httpGet;
		char buffer[10000];


		httpGet =
			"GET / HTTP/1.1\r\n"
			"Host: " + url + "\r\n"
			"Connection: close\r\n\r\n";

		if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
			errorMessage = "WSAStartup failed.";
			ChangeScreen("error");
			return;
		}

		sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		host = gethostbyname(url.c_str());

		if (host == nullptr) {
			errorMessage = "No such host";
			WSACleanup();
			ChangeScreen("error");
			return;
		}

		sockAddr.sin_port = htons(80);
		sockAddr.sin_family = AF_INET;
		sockAddr.sin_addr.s_addr = *((unsigned long*)host->h_addr);

		if (connect(sock, (SOCKADDR*)(&sockAddr), sizeof(sockAddr)) != 0) {
			errorMessage = "Could not connect.";
			WSACleanup();
			ChangeScreen("error");
			return;
		}
		send(sock, httpGet.c_str(), httpGet.size(), 0);

		int nDataLength;
		std::string html;
		while ((nDataLength = recv(sock, buffer, 10000, 0)) > 0) {
			int i = 0;
			while (buffer[i] >= 32 || buffer[i] == '\n' || buffer[i] == '\r') {
				i++;
				if (buffer[i] == '\r') continue;
				html += buffer[i];
			}
		}

		closesocket(sock);
		WSACleanup();

		ChangeScreen("main");

		auto lblHtml = std::make_shared<label>(0, 0, ScreenWidth(), ScreenHeight() - 60, html);
		tmp_uis.push_back(lblHtml);
	}

	void BackHandler(std::any a) {
		ChangeScreen("start");
	}

	void RowFtpHandler(std::any a) {
		// enter dir or download file
		row_ftp r = std::any_cast<row_ftp>(a);
		table<row_ftp>* ftp_table = static_cast<table<row_ftp>*>(tmp_uis[0].get());

		std::thread([&](row_ftp r, table<row_ftp>* ftp_table) {
			HINTERNET internet = InternetOpenA(NULL, INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
			HINTERNET ftp_session = InternetConnectA(internet, server_url.c_str(), INTERNET_DEFAULT_FTP_PORT, NULL, NULL, INTERNET_SERVICE_FTP, 0, 0);

			if (ftp_session == NULL) {
				errorMessage = "ftp error: " + std::to_string(GetLastError());
				InternetCloseHandle(internet);
				ChangeScreen("error");
				return;
			}

			// set current dir
			// if dir clicked - update ftp_table
			// if file clicked - download

			WIN32_FIND_DATAA winFindData;
			row_ftp::data d;
			HINTERNET ftp_next;

			switch (r.info.type)
			{
			case row_ftp::obj_type::dir:
				path_url += r.info.name;
				path_url += "/";
				FtpSetCurrentDirectoryA(ftp_session, path_url.c_str());
				ftp_table->clear();
				ftp_next = FtpFindFirstFileA(ftp_session, NULL, &winFindData, NULL, NULL);
				d = {
					winFindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ?
					row_ftp::obj_type::dir :
					row_ftp::obj_type::file
				};
				std::memcpy(d.name, winFindData.cFileName, 63);
				winFindData = { 0 };
				ftp_table->push_back(d);
				while (InternetFindNextFileA(ftp_next, &winFindData)) {
					d = {
						winFindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ?
						row_ftp::obj_type::dir :
						row_ftp::obj_type::file
					};
					std::memcpy(d.name, winFindData.cFileName, 63);
					winFindData = { 0 };
					ftp_table->push_back(d);
				}
				if (GetLastError() != ERROR_NO_MORE_FILES) {
					std::cout << "jopa: " << GetLastError() << std::endl;
				}
				break;

			case row_ftp::obj_type::file:
				FtpSetCurrentDirectoryA(ftp_session, path_url.c_str());
				FtpGetFileA(ftp_session, r.info.name, r.info.name, false, NULL, FTP_TRANSFER_TYPE_BINARY, NULL);
				break;

			default:
				break;
			}

			InternetCloseHandle(ftp_session);
			InternetCloseHandle(internet);
		}, r, ftp_table).detach();
	}
};


int main(int argc, char* argv[]) {
	HttpClient httpClient;
	if (httpClient.Construct(700, 300, 2, 2))
		httpClient.Start();
	return 0;
}
