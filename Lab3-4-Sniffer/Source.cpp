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

#pragma comment(lib, "ws2_32.lib")


typedef bool(olc::PixelGameEngine::* Screen)(float);
typedef void(olc::PixelGameEngine::* BtnHandler)();


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
	std::shared_ptr<BtnHandler> handler;

	ui_element() :
		x(0), y(0), w(0), h(0), bActive(true), bSelected(false), mainColor(olc::WHITE), mouseOnColor(olc::YELLOW), activeColor(olc::BLUE) {}
	ui_element(int _x, int _y, int _w, int _h, bool _bActive = true, olc::Pixel _mainColor = olc::WHITE, olc::Pixel _mouseOnColor = olc::YELLOW, olc::Pixel _activeColor = olc::BLUE) :
		x(_x), y(_y), w(_w), h(_h), bActive(_bActive), bSelected(false), mainColor(_mainColor), mouseOnColor(_mouseOnColor), activeColor(_activeColor) {}

	bool virtual MouseCollide(olc::PixelGameEngine* pge) {
		olc::vi2d mousePos = pge->GetMousePos();
		return mousePos.x >= x && mousePos.x <= x + w && mousePos.y >= y && mousePos.y <= y + h;
	}
	void virtual Render(olc::PixelGameEngine* pge) = 0;
	void virtual OnClick(olc::PixelGameEngine* pge) {};
	void virtual OnMouseWheel(olc::PixelGameEngine* pge) {};

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

	scrollbar() :
		ui_element() {}
	scrollbar(int _x, int _y, int _w, int _h, int _max = 100, int _current = 0, int _min = 0, bool _bActive = true, olc::Pixel _mainColor = olc::WHITE, olc::Pixel _mouseOnColor = olc::YELLOW, olc::Pixel _activeColor = olc::BLUE) :
		ui_element(_x, _y, _w, _h, _bActive, _mainColor, _mouseOnColor, _activeColor), bottom(_min), top(_max), current(_current) {}

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
	scrollbar_v(int _x, int _y, int _w, int _h, int _max = 100, int _current = 0, int _min = 0) :
		scrollbar(_x, _y, _w, _h, _max, _current, _min) {
		int maxH = h - w + 1;
		drawY = y + (maxH * (current - bottom)) / (top - bottom);
	}

	void Increase() override {
		scrollbar::Increase();
		int maxH = h - w + 1;
		drawY = y + (maxH * (current - bottom)) / (top - bottom);
	}
	void Decrease() override {
		scrollbar::Decrease();
		int maxH = h - w + 1;
		drawY = y + (maxH * (current - bottom)) / (top - bottom);
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
	scrollbar_h(int _x, int _y, int _w, int _h, int _max = 100, int _current = 0, int _min = 0) :
		scrollbar(_x, _y, _w, _h, _max, _current, _min) {
		int maxW = h - w + 1;
		drawX = x + (maxW * (current - bottom)) / (top - bottom);
	}

	void Increase() override {
		scrollbar::Increase();
		int maxW = h - w + 1;
		drawX = x + (maxW * (current - bottom)) / (top - bottom);
	}
	void Decrease() override {
		scrollbar::Decrease();
		int maxW = h - w + 1;
		drawX = x + (maxW * (current - bottom)) / (top - bottom);
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
	scrollable(int _x, int _y, int _w, int _h, bool _bActive = true, olc::Pixel _mainColor = olc::WHITE, olc::Pixel _mouseOnColor = olc::YELLOW, olc::Pixel _activeColor = olc::BLUE) :
		ui_element(_x, _y, _w, _h, _bActive, _mainColor, _mouseOnColor, _activeColor), frame(_w, _h) {}
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
		scrollable(_x, _y, _w, _h, _bActive, _mainColor, _mouseOnColor, _activeColor), scroll_h(_x, _y + _h - 10, _w - 10, 10), scroll_v(_x + _w - 10, _y, 10, _h - 10) {}

	void Render(olc::PixelGameEngine* pge) override {
		auto layer = pge->GetDrawTarget();
		pge->SetDrawTarget(&frame);
		pge->Clear(olc::BLANK);

		scroll_h.Render(pge);
		scroll_v.Render(pge);

		pge->SetDrawTarget(layer);
		pge->DrawSprite(x, y, &frame);
	}
};

struct row : public ui_element
{
	row() :
		ui_element() {}
	row(int _x, int _y, int _w, int _h) :
		ui_element(_x, _y, _w, _h) {}
	void virtual RenderHeader(olc::PixelGameEngine* pge) = 0;
};

struct row_ip : public row
{
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
	row_ip(int _x, int _y, int _w, int _h, data _header) :
		row(_x, _y, _w, _h), header(_header) {}

	void Render(olc::PixelGameEngine* pge) override {
		pge->DrawString(x		, y, std::to_string(header.ver_ihl & 0x0f));
		pge->DrawString(x + 32	, y, std::to_string(header.ver_ihl & 0xf0));
		pge->DrawString(x + 64	, y, std::to_string(header.tos));
		pge->DrawString(x + 96	, y, std::to_string(header.tlen));
		//pge->DrawString(x, y + 60, std::to_string(header.id));
	}
	void RenderHeader(olc::PixelGameEngine* pge) override {
		pge->DrawString(x		, y, "ver");
		pge->DrawString(x + 32	, y, "ihl");
		pge->DrawString(x + 64	, y, "tos");
		pge->DrawString(x + 96	, y, "tlen");
	}
};

template<typename T>
struct table : public scrollable_vh
{
	T header;
	std::vector<T> rows;

	table() :
		scrollable_vh(), header() {}
	table(int _x, int _y, int _w, int _h, bool _bActive = true, olc::Pixel _mainColor = olc::WHITE, olc::Pixel _mouseOnColor = olc::YELLOW, olc::Pixel _activeColor = olc::BLUE) :
		scrollable_vh(_x, _y, _w, _h, _bActive, _mainColor, _mouseOnColor, _activeColor), header(_x, _y, _w, 10, { 0 }) {}

	void push_back(typename T::data&& _data) {
		rows.emplace_back(x, y + (rows.size() + 1) * 10, w, 10, _data);
	}
	void Render(olc::PixelGameEngine* pge) override {
		auto layer = pge->GetDrawTarget();
		pge->SetDrawTarget(&frame);

		header.RenderHeader(pge);
		for (auto& r : rows)
			r.Render(pge);
		scroll_h.Render(pge);
		scroll_v.Render(pge);

		pge->SetDrawTarget(layer);
		pge->DrawSprite(x, y, &frame);
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

	std::unordered_map<std::string, std::vector<std::shared_ptr<ui_element>>> uis;
	std::vector<std::shared_ptr<ui_element>> current_uis;
	std::vector<std::shared_ptr<button>> buttons;
	std::vector<std::shared_ptr<input>> inputs;


public:
	bool OnUserCreate() override {
		screens = {
			{ "main", (Screen)& Sniffer::MainScreen },
		};
		//uis = {
		//	{ "main", {
		//		std::make_shared<table<row_ip>>(0, 0, 500, 100)
		//	}},
		//};
		auto t1 = std::make_shared<table<row_ip>>(0, 0, 500, 100);
		t1.get()->push_back({ 0 });
		t1.get()->push_back({ 0 });
		t1.get()->push_back({ 0 });
		t1.get()->push_back({ 0 });
		t1.get()->push_back({ 0 });
		t1.get()->push_back({ 0 });
		t1.get()->push_back({ 0 });
		t1.get()->push_back({ 0 });
		t1.get()->push_back({ 0 });
		t1.get()->push_back({ 0 });
		t1.get()->push_back({ 0 });
		t1.get()->push_back({ 0 });
		t1.get()->push_back({ 0 });
		t1.get()->push_back({ 0 });
		t1.get()->push_back({ 0 });
		uis["main"].push_back(t1);
		ChangeScreen("main");

		return true;
	}

	bool OnUserUpdate(float fElapsedTime) override {
		if (!current_screen)
			return false;

		// Draw
		for (auto& ui : current_uis) {
			ui.get()->Render(this);
		}

		std::invoke(current_screen, *this, fElapsedTime);

		return true;
	}


private:
	template<typename T>
	void Extract(std::vector<std::shared_ptr<T>>& v, std::vector<std::shared_ptr<ui_element>>& uis) {
		v.clear();
		for (auto& ui : uis) {
			T* item = dynamic_cast<T*>(ui.get());
			if (item) {
				v.emplace_back(item);
			}
		}
	}

	void ChangeScreen(std::string screenName) {
		current_screen = screens[screenName];
		// Ќе круто, что происходит полное копирование
		current_uis = uis[screenName];
		Extract(buttons, current_uis);
		Extract(inputs, current_uis);
	}

	bool MainScreen(float fElapsedTime) {
		return true;
	}

	bool ErrorScreen(float fElapsedTime) {
		return true;
	}
};


int main(int argc, char* argv[]) {
	Sniffer client;
	if (client.Construct(600, 300, 2, 2))
		client.Start();
	return 0;
}
