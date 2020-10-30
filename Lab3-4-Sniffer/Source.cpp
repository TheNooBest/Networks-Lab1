#define OLC_PGE_APPLICATION
#define _WINSOCKAPI_
#define _CRT_SECURE_NO_WARNINGS

#include "../Lab1-2/olcPixelGameEngine.h"
#include <unordered_map>
#include <functional>
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <mutex>
#include <algorithm>
#include <memory>
#include <bitset>
#include <sstream>

#pragma comment(lib, "ws2_32.lib")


typedef bool(olc::PixelGameEngine::* Screen)(float);
typedef void(olc::PixelGameEngine::* BtnHandler)();


#pragma region HELP_FUNCS
std::string to_hex(int num) {
	std::stringstream ss;
	ss << std::hex << num;
	return ss.str();
}

std::string to_ip(unsigned int _ip) {
	char ip[INET_ADDRSTRLEN];
	IN_ADDR addr = { _ip };
	InetNtopA(AF_INET, &addr, ip, sizeof(ip));
	return ip;
}
#pragma endregion



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

	ui_element() :
		x(0), y(0), w(0), h(0), bActive(true), bSelected(false), mainColor(olc::WHITE), mouseOnColor(olc::YELLOW), activeColor(olc::BLUE) {}
	ui_element(int _x, int _y, int _w, int _h, bool _bActive = true, olc::Pixel _mainColor = olc::WHITE, olc::Pixel _mouseOnColor = olc::YELLOW, olc::Pixel _activeColor = olc::BLUE) :
		x(_x), y(_y), w(_w), h(_h), bActive(_bActive), bSelected(false), mainColor(_mainColor), mouseOnColor(_mouseOnColor), activeColor(_activeColor) {}

	bool virtual MouseCollide(olc::PixelGameEngine* pge) {
		olc::vi2d mousePos = pge->GetMousePos();
		return mousePos.x >= x && mousePos.x <= x + w && mousePos.y >= y && mousePos.y <= y + h;
	}
	// Must be overrided
	void virtual Render(olc::PixelGameEngine* pge) = 0;
	// You might or not to override
	void virtual OnClick(olc::PixelGameEngine* pge) {};
	void virtual OnMouseWheel(olc::PixelGameEngine* pge) {};
	void virtual Render(olc::PixelGameEngine* pge, int x, int y) {
		int _x = this->x;
		int _y = this->y;
		this->x = x;
		this->y = y;
		Render(pge);
		this->x = _x;
		this->y = _y;
	};

	virtual ~ui_element() = default;
};

struct button : public ui_element
{
	std::string label;
	BtnHandler handler;

	button() :
		ui_element(), label(), handler(nullptr) {}
	button(int _x, int _y, int _w, int _h, const std::string _label = "", BtnHandler _handler = nullptr, bool _bActive = true, olc::Pixel _mainColor = olc::WHITE, olc::Pixel _mouseOnColor = olc::YELLOW, olc::Pixel _activeColor = olc::BLUE) :
		ui_element(_x, _y, _w, _h, _bActive, _mainColor, _mouseOnColor, _activeColor), label(_label), handler(_handler) {}

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
		std::invoke(handler, *pge);
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
	const static int content_width = 0;

	struct data;

	row() :
		ui_element() {}
	row(int _x, int _y) :
		ui_element(_x, _y, content_width, row_height) {}
	row(int _x, int _y, int _w, int _h) :
		ui_element(_x, _y, _w, _h) {}
	void virtual RenderHeader(olc::PixelGameEngine* pge) = 0;
};

struct row_ip : public row
{
	// hardcode
	const static int content_width = 680;
	const static inline std::vector<int> vd = { 32, 64, 96, 176, 224, 272, 312, 344, 392, 424, 552, 0 };

	struct data {
		uint8_t		ver_ihl;
		uint8_t		tos;
		uint16_t	tlen;
		uint16_t	id;
		uint16_t	flags_fo;
		uint8_t		ttl;
		uint8_t		proto;
		uint16_t	crc;
		uint32_t	src_addr;
		uint32_t	dst_addr;
	} header = { 0 };

	row_ip() :
		row() {}
	row_ip(int _x, int _y, data _header) :
		row(_x, _y, content_width, row_height), header(_header) {}
	row_ip(int _x, int _y, int _w, int _h, data _header) :
		row(_x, _y, _w, _h), header(_header) {}

	void Render(olc::PixelGameEngine* pge) override {
		pge->DrawString(x			, y + 2, std::to_string(header.ver_ihl & 0x0f));
		pge->DrawString(x + vd[0]	, y + 2, std::to_string(header.ver_ihl & 0xf0));
		pge->DrawString(x + vd[1]	, y + 2, std::to_string(header.tos));
		pge->DrawString(x + vd[2]	, y + 2, std::to_string(header.tlen));
		pge->DrawString(x + vd[3]	, y + 2, std::to_string(header.id));
		pge->DrawString(x + vd[4]	, y + 2, std::bitset<3>(header.flags_fo).to_string());
		pge->DrawString(x + vd[5]	, y + 2, std::to_string(std::bitset<13>(header.flags_fo >> 3).to_ullong() * 8));
		pge->DrawString(x + vd[6]	, y + 2, std::to_string(header.ttl));
		pge->DrawString(x + vd[7]	, y + 2, std::to_string(header.proto));
		pge->DrawString(x + vd[8]	, y + 2, to_hex(header.crc));
		pge->DrawString(x + vd[9]	, y + 2, to_ip(header.src_addr));
		pge->DrawString(x + vd[10]	, y + 2, to_ip(header.dst_addr));
		pge->DrawLine(x, y + row_height, x + content_width, y + row_height, olc::GREY);
	}
	void Render(olc::PixelGameEngine* pge, int x, int y) override {
		pge->DrawString(x			, y + 2, std::to_string(header.ver_ihl & 0x0f));
		pge->DrawString(x + vd[0]	, y + 2, std::to_string(header.ver_ihl & 0xf0));
		pge->DrawString(x + vd[1]	, y + 2, std::to_string(header.tos));
		pge->DrawString(x + vd[2]	, y + 2, std::to_string(header.tlen));
		pge->DrawString(x + vd[3]	, y + 2, std::to_string(header.id));
		pge->DrawString(x + vd[4]	, y + 2, std::bitset<3>(header.flags_fo).to_string());
		pge->DrawString(x + vd[5]	, y + 2, std::to_string(std::bitset<13>(header.flags_fo >> 3).to_ullong() * 8));
		pge->DrawString(x + vd[6]	, y + 2, std::to_string(header.ttl));
		pge->DrawString(x + vd[7]	, y + 2, std::to_string(header.proto));
		pge->DrawString(x + vd[8]	, y + 2, to_hex(header.crc));
		pge->DrawString(x + vd[9]	, y + 2, to_ip(header.src_addr));
		pge->DrawString(x + vd[10]	, y + 2, to_ip(header.dst_addr));
		pge->DrawLine(x, y + row_height, x + content_width, y + row_height, olc::GREY);
	}
	void RenderHeader(olc::PixelGameEngine* pge) override {
		pge->DrawString(x			, y + 2, "ver");
		pge->DrawString(x + vd[0]	, y + 2, "ihl");
		pge->DrawString(x + vd[1]	, y + 2, "tos");
		pge->DrawString(x + vd[2]	, y + 2, "tlen");
		pge->DrawString(x + vd[3]	, y + 2, "id");
		pge->DrawString(x + vd[4]	, y + 2, "flags");
		pge->DrawString(x + vd[5]	, y + 2, "fo");
		pge->DrawString(x + vd[6]	, y + 2, "ttl");
		pge->DrawString(x + vd[7]	, y + 2, "proto");
		pge->DrawString(x + vd[8]	, y + 2, "crc");
		pge->DrawString(x + vd[9]	, y + 2, "src");
		pge->DrawString(x + vd[10]	, y + 2, "dst");
		pge->DrawLine(x, y + row_height, x + content_width, y + row_height, olc::GREY);
	}
	static int Delimeters(int pos) {
		return vd[pos] - 4;
	}
};

template<typename T>
struct table : public scrollable_vh
{
	static_assert(std::is_base_of<row, T>::value, "Template argument must be derived from 'struct row'");

	T header;
	std::vector<T> rows;

	table() :
		scrollable_vh(), header() {}
	table(int _x, int _y, int _w, int _h, bool _bActive = true, olc::Pixel _mainColor = olc::WHITE, olc::Pixel _mouseOnColor = olc::YELLOW, olc::Pixel _activeColor = olc::BLUE) :
		scrollable_vh(_x, _y, _w, _h, _bActive, _mainColor, _mouseOnColor, _activeColor), header(_x, _y, _w, 10, { 0 }) {
		scroll_v.top = -frame.height / T::row_height + 1;
		scroll_v.current = 0;
		scroll_v.bReversed = true;
	}

	void push_back(typename T::data&& _data) {
		rows.emplace_back(x, y + (rows.size() + 1) * T::row_height, _data);
		scroll_v.top++;
	}
	void Render(olc::PixelGameEngine* pge) override {
		auto layer = pge->GetDrawTarget();
		pge->SetDrawTarget(&frame);
		pge->Clear(olc::BLACK);

		header.RenderHeader(pge);
		for (int i = 1, row = scroll_v.current; i <= (frame.height + T::row_height - 1) / T::row_height && row < rows.size(); i++, row++)
			rows[row].Render(pge, x, y + i * T::row_height);
		for (int i = 0, _x; (_x = T::Delimeters(i)) >= 0; i++)
			pge->DrawLine(_x, 0, _x, min((rows.size() + 1) * T::row_height, frame.height));

		pge->SetDrawTarget(layer);

		pge->DrawSprite(x, y, &frame);
		scroll_h.Render(pge);
		scroll_v.Render(pge);
	}
};
#pragma endregion


class Sniffer : public olc::PixelGameEngine
{
public:
	Sniffer() {
		sAppName = "Sniffer";
	}


protected:
	Screen current_screen = nullptr;
	std::unordered_map<std::string, Screen> screens;

	// ui elements
	std::unordered_map<std::string, std::vector<std::shared_ptr<ui_element>>> uis;		// all app uis
	std::vector<std::shared_ptr<ui_element>> current_uis;								// ui on current screen
	std::vector<std::shared_ptr<button>> buttons;										// buttons on current screen
	std::vector<std::shared_ptr<input>> inputs;											// inputs on current screen
	std::vector<std::shared_ptr<table<row_ip>>> ip_tables;								// tables with row_ip rows on current screen


public:
	bool OnUserCreate() override {
		screens = {
			{ "start",	(Screen)& Sniffer::StartScreen	},
			{ "main",	(Screen)& Sniffer::MainScreen	},
		};

		auto start_btn = std::make_shared<button>((ScreenWidth() - 100) / 2, ScreenHeight() - 40, 100, 20, "Start", (BtnHandler)& Sniffer::StartHandler);
		uis["start"].push_back(start_btn);

		auto t1 = std::make_shared<table<row_ip>>(0, 0, ScreenWidth(), ScreenHeight() - 100);
		t1.get()->push_back({ 4 });
		t1.get()->push_back({ 4 });
		t1.get()->push_back({ 4 });
		t1.get()->push_back({ 4 });
		t1.get()->push_back({ 4 });
		t1.get()->push_back({ 4 });
		t1.get()->push_back({ 4 });
		t1.get()->push_back({ 4 });
		t1.get()->push_back({ 4 });
		t1.get()->push_back({ 4 });
		t1.get()->push_back({ 4 });
		t1.get()->push_back({ 4 });
		t1.get()->push_back({ 4 });
		t1.get()->push_back({ 4 });
		t1.get()->push_back({ 4 });
		uis["main"].push_back(t1);

		ChangeScreen("start");

		return true;
	}

	bool OnUserUpdate(float fElapsedTime) override {
		if (!current_screen)
			return false;

		Clear(olc::BLACK);

		// Controls
		if (GetMouse(0).bPressed)
			for (auto& ui : current_uis)
				if (ui.get()->MouseCollide(this))
					ui.get()->OnClick(this);
		if (GetMouseWheel() != 0)
			for (auto& ui : current_uis)
				if (ui.get()->MouseCollide(this))
					ui.get()->OnMouseWheel(this);

		// Run screen logic
		std::invoke(current_screen, *this, fElapsedTime);

		// Draw
		for (auto& ui : current_uis)
			ui.get()->Render(this);

		return true;
	}


private:
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
			// todo add message
			current_screen = screens[screenName];
		}
		// Ќе круто, что происходит полное копирование
		current_uis = uis[screenName];
		Extract(buttons, current_uis);
		Extract(inputs, current_uis);
		Extract(ip_tables, current_uis);
	}

	bool StartScreen(float fElapsedTime) {
		return true;
	}

	bool MainScreen(float fElapsedTime) {
		return true;
	}

	bool ErrorScreen(float fElapsedTime) {
		return true;
	}

private:
	void StartHandler() {
		// create socket and start listening it

		ChangeScreen("main");
	}
};


int main(int argc, char* argv[]) {
	Sniffer client;
	if (client.Construct(690, 300, 2, 2))
		client.Start();
	return 0;
}
