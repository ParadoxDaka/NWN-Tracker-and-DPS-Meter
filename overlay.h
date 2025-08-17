#pragma once
#define NOMINMAX
#include <Windows.h>
#include <WinUser.h>
#include <Dwmapi.h> 
#pragma comment(lib, "dwmapi.lib")
#include <stdlib.h>
#include <vector>
#include <chrono>
#include <cwchar>
#include <thread>
#include <string>
#include "XorString.h"
#include "imgui/imgui.h"
#include "imgui/imgui_impl_dx11.h"
#include "imgui/imgui_impl_win32.h"

#pragma comment(lib, "d3d11.lib")

#define GREEN ImColor(0, 255, 0)
#define RED ImColor(255, 0, 0)
#define BLUE ImColor(0, 0, 255)
#define ORANGE ImColor(255, 165, 0)
#define WHITE ImColor(255, 255, 255)
#define TEAL ImColor(0, 128, 128)
#define YELLOW ImColor(255, 255, 0)
#define GOLD ImColor(255, 215, 0)
#define PURPLE ImColor(128, 0, 128)


class Overlay
{
public:
	void Start();
	DWORD CreateOverlay();
	void Clear();
	int getWidth();
	int getHeight();
	void RenderInfo();
	void RenderMenu();
	void Cooldowns();
	void RenderDPSMeter();
	void PlayerList();
	void ClickThrough(bool v);
	void DrawLine(ImVec2 a, ImVec2 b, ImColor color, float width);
	ImVec2 infoWindowPos = ImVec2(0, 25); // default position
	void Text(ImVec2 pos, ImColor color, const char* text_begin, const char* text_end, float wrap_width, const ImVec4* cpu_fine_clip_rect);

	void String(ImVec2 pos, ImColor color, const char* text);
	
private:
	bool running;
	HWND overlayHWND;
};
