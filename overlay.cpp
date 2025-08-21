#pragma warning (disable : 4715)
#pragma warning (disable : 4005)
#pragma warning (disable : 4305)
#pragma warning (disable : 4244)
#include "overlay.h"
#include <sstream>
#include <fstream>
#include <iomanip>
#include <d3d11.h>
#include <iostream>
#include <algorithm>
#include "Players.h"
#include <map>
#include "LevelUtils.h"

int width;
int height;
extern bool use_nvidia;
LONG nv_default = WS_POPUP | WS_CLIPSIBLINGS;
LONG nv_default_in_game = nv_default | WS_DISABLED;
LONG nv_edit = nv_default_in_game | WS_VISIBLE;

LONG nv_ex_default = WS_EX_TOOLWINDOW;
LONG nv_ex_edit = nv_ex_default | WS_EX_LAYERED | WS_EX_TRANSPARENT;
LONG nv_ex_edit_menu = nv_ex_default | WS_EX_TRANSPARENT;
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
		return true;

	return DefWindowProc(hWnd, msg, wParam, lParam);
}
static DWORD WINAPI StaticMessageStart(void* Param)
{
	Overlay* ov = (Overlay*)Param;
	ov->CreateOverlay();
	return 0;
}

BOOL CALLBACK EnumWindowsCallback(HWND hwnd, LPARAM lParam)
{
	wchar_t className[255] = L"";
	GetClassName(hwnd, className, 255);
	if (use_nvidia)
	{
		if (wcscmp(XorStrW(L"CEF-OSC-WIDGET"), className) == 0) 		{
			HWND* w = (HWND*)lParam;
			if (GetWindowLong(hwnd, GWL_STYLE) != nv_default && GetWindowLong(hwnd, GWL_STYLE) != nv_default_in_game)
				return TRUE;
			*w = hwnd;
			return TRUE;
		}
	}
	else
	{
		if (wcscmp(XorStrW(L"overlay"), className) == 0) 		{
			HWND* w = (HWND*)lParam;
			*w = hwnd;
			return TRUE;
		}
	}
	return TRUE;
}

static ID3D11Device* g_pd3dDevice = NULL;
static ID3D11DeviceContext* g_pd3dDeviceContext = NULL;
static IDXGISwapChain* g_pSwapChain = NULL;
static ID3D11RenderTargetView* g_mainRenderTargetView = NULL;
bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void CreateRenderTarget();
void CleanupRenderTarget();

void Overlay::DrawLine(ImVec2 a, ImVec2 b, ImColor color, float width)
{
	ImGui::GetWindowDrawList()->AddLine(a, b, color, width);
}
void Overlay::Text(ImVec2 pos, ImColor color, const char* text_begin, const char* text_end, float wrap_width, const ImVec4* cpu_fine_clip_rect)
{
	ImGui::GetWindowDrawList()->AddText(ImGui::GetFont(), ImGui::GetFontSize(), pos, color, text_begin, text_end, wrap_width, cpu_fine_clip_rect);
}

void Overlay::String(ImVec2 pos, ImColor color, const char* text)
{
	Text(pos, color, text, text + strlen(text), 200, 0);
}
extern std::atomic<int64_t> totalDamage;
extern std::atomic<int64_t> totalExperience;
extern std::atomic<int64_t> totalKills;
extern std::atomic<int64_t> xpHrAccumulated;
bool test = false;
ImVec2 infoWindowPos;
ImVec2 infoWindowPos2;
ImVec2 infoWindowPos3;
ImVec2 DPSWindowPos;
ImVec2 BuffWindowPos;
bool load = true;
static int64_t damageAccum = 0;       static std::chrono::steady_clock::time_point dpsStartTime;
static std::chrono::steady_clock::time_point lastDamageTime;
static bool dpsActive = false;
static float currentDPS = 0.0f;
extern int currentXP;
bool Buffwindow = false;
bool Cooldownwindow = false;
bool Stattrackerwindow = false;
bool DPSwindow = false;
bool Playerlistwindow = false;
extern std::chrono::steady_clock::time_point serverRestartTime;
extern bool serverRestartActive;

const int XP_TABLE[NWN_LEVEL_MAX + 1] = {
	0,
	0, 
	1000, 
	3000,   
	6000,  
	10000, 
	15000,  
	21000,  
	28000, 
	36000,  
	45000,  
	55000,  
	66000,  
	78000,  
	91000,  
	105000, 
	120000,  
	136000,   
	153000,  
	171000, 
	190000, 
	210000, 
	231000,  
	253000,  
	276000,  
	300000,   
	325000,  
	351000,  
	378000,  
	406000,  
	435000,  
	465000,  
	496000,  
	528000,  
	561000,  
	595000,  
	630000,  
	666000,  
	703000,  
	741000,  
	780000    };

LevelInfo GetLevelFromXP(int currentXP)
{
	LevelInfo info = { 1, 0, 0 };

	for (int lvl = 1; lvl < NWN_LEVEL_MAX; ++lvl)
	{
		if (currentXP >= XP_TABLE[lvl] && currentXP < XP_TABLE[lvl + 1])
		{
			info.currentLevel = lvl;
			info.xpCurrentLevelStart = XP_TABLE[lvl];
			info.xpToNextLevel = XP_TABLE[lvl + 1] - currentXP;
			return info;
		}
	}

	if (currentXP >= XP_TABLE[NWN_LEVEL_MAX])
	{
		info.currentLevel = NWN_LEVEL_MAX;
		info.xpCurrentLevelStart = XP_TABLE[NWN_LEVEL_MAX];
		info.xpToNextLevel = 0;
	}
	return info;
}
std::string static FormatWithCommas(int64_t value)
{
	std::stringstream ss;
	ss.imbue(std::locale(""));
	ss << std::fixed << value;
	return ss.str();
}
extern int TimeStopCooldown;
static float bgAlpha = 1.0f;
extern std::vector<std::string> playerList;
extern bool GSTimerActive;
constexpr int GSTimerActiveCooldown = 45;
extern int totalGold;
extern int currentGold;
extern int goldHrAccumulated;
extern std::chrono::steady_clock::time_point goldStartTime;
extern std::chrono::steady_clock::time_point RestReadyTime;
extern bool RestTimerActive;
int tsCooldown = 9;
extern bool bastion;
extern bool lostsoulsreborn;
void Overlay::RenderInfo()
{
	ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, bgAlpha));
	ImGui::SetNextWindowPos(infoWindowPos);

	LevelInfo lvlInfo = GetLevelFromXP(currentXP);
	auto now2 = std::chrono::steady_clock::now();
	auto remaining = std::chrono::duration_cast<std::chrono::seconds>(serverRestartTime - now2).count();

	int hours = remaining / 3600;
	int minutes = (remaining % 3600) / 60;
	int seconds = remaining % 60;

	std::string dmgStr = "Total Damage: " + FormatWithCommas(totalDamage.load());
	std::string expStr = "Total Experience: " + FormatWithCommas(totalExperience.load());
	std::string killStr = "Total Kills: " + FormatWithCommas(totalKills.load());
	std::string dpsStr = "DPS: " + std::to_string(static_cast<int>(currentDPS));
	std::string currentLevelStr = "Current Level: " + std::to_string(lvlInfo.currentLevel);
	std::string xpLeftStr = "XP to Next Level: " + FormatWithCommas(lvlInfo.xpToNextLevel);
	char buf[64];
	snprintf(buf, sizeof(buf), "Server Restart In: %02d:%02d:%02d", hours, minutes, seconds);

	ImVec2 dmgSize = ImGui::CalcTextSize(dmgStr.c_str());
	ImVec2 expSize = ImGui::CalcTextSize(expStr.c_str());
	ImVec2 killSize = ImGui::CalcTextSize(killStr.c_str());
	ImVec2 dpsSize = ImGui::CalcTextSize(dpsStr.c_str());
	ImVec2 xpLeftSize = ImGui::CalcTextSize(xpLeftStr.c_str());
	ImVec2 currentLevelSize = ImGui::CalcTextSize(currentLevelStr.c_str());
	ImVec2 restartSize = ImGui::CalcTextSize(buf);

	float maxLeftWidth = std::max({ dmgSize.x, expSize.x, killSize.x, dpsSize.x, xpLeftSize.x, currentLevelSize.x, restartSize.x }) + 20.0f;

	static std::string xpPerHourStr;

	ImVec2 goldSize = ImGui::CalcTextSize("Gold: 00000000000000");
	ImVec2 goldPerHourSize = ImGui::CalcTextSize("Gold/hr: 000000000000.0");


	float maxRightWidth = std::max({ goldSize.x, goldPerHourSize.x }) + 20.0f;


	int numLines = 6;
	float lineHeight = dmgSize.y;
	float totalHeight = (lineHeight * numLines) + 20.0f;


	static int64_t damageLast = 0;
	int64_t damageNow = totalDamage.load();
	int64_t damageDiff = damageNow - damageLast;

	if (damageDiff > 0)
	{
		auto now = std::chrono::steady_clock::now();

		if (!dpsActive)
		{
			dpsStartTime = now;
			damageAccum = 0;
			dpsActive = true;
		}

		damageAccum += damageDiff;
		lastDamageTime = now;
	}
	else
	{
		if (dpsActive)
		{
			auto now = std::chrono::steady_clock::now();
			auto timeSinceLastDamage = std::chrono::duration_cast<std::chrono::seconds>(now - lastDamageTime).count();
			if (timeSinceLastDamage >= 5)
			{
				dpsActive = false;
				damageAccum = 0;
				currentDPS = 0.0f;
			}
		}
	}

	damageLast = damageNow;

	if (dpsActive)
	{
		auto now = std::chrono::steady_clock::now();
		auto elapsedSec = std::chrono::duration_cast<std::chrono::milliseconds>(now - dpsStartTime).count() / 1000.0f;
		if (elapsedSec > 0.0f)
			currentDPS = damageAccum / elapsedSec;
	}

	float currentLevelXPStart = XP_TABLE[lvlInfo.currentLevel];
	float nextLevelXPStart = XP_TABLE[lvlInfo.currentLevel + 1];
	float progress = (float)(currentXP - currentLevelXPStart) /
		(float)(nextLevelXPStart - currentLevelXPStart);
	progress = std::clamp(progress, 0.0f, 1.0f);

	static float displayedProgress = 0.0f;
	float speed = 5.0f;
	displayedProgress += (progress - displayedProgress) * ImGui::GetIO().DeltaTime * speed;

	static auto xpHrStartTime = std::chrono::steady_clock::now();
	auto now = std::chrono::steady_clock::now();
	float elapsedHours = std::chrono::duration_cast<std::chrono::seconds>(now - xpHrStartTime).count() / 3600.0f;
	float xpPerHour = (elapsedHours > 0.0f) ? (xpHrAccumulated / elapsedHours) : 0.0f;
	xpPerHourStr = "XP/hr: " + FormatWithCommas((long long)xpPerHour);

	ImVec2 xpPerHourSize = ImGui::CalcTextSize(xpPerHourStr.c_str());
	maxRightWidth = std::max({ xpPerHourSize.x, goldSize.x, goldPerHourSize.x }) + 20.0f;

	float totalWidth = maxLeftWidth + maxRightWidth + 10.0f;
	ImGui::SetNextWindowSize(ImVec2(totalWidth, totalHeight), ImGuiCond_Always);
	if (load)
	{
		std::ifstream file("Config.txt");
		if (file.is_open())
		{
			std::string dummy;
			std::getline(file, dummy); // skip line 0
			std::getline(file, dummy); // skip line 1
			file >> infoWindowPos.x >> infoWindowPos.y;
			file >> infoWindowPos2.x >> infoWindowPos2.y;
			file >> bgAlpha;
			file >> infoWindowPos3.x >> infoWindowPos3.y;
			file >> DPSWindowPos.x >> DPSWindowPos.y;
			file >> BuffWindowPos.x >> BuffWindowPos.y;
			file >> bastion;
			file >> lostsoulsreborn;
			file >> tsCooldown;
			file >> TimeStopCooldown;
			file >> Buffwindow;
			file >> Cooldownwindow;
			file >> Stattrackerwindow;
			file >> DPSwindow;
			file >> Playerlistwindow;
			file.close();
		}
		else
		{
			printf("Failed to open Config.txt for reading\n");
		}
		load = false;
	}
	if (Stattrackerwindow)
	{
		ImGui::Begin("##info", nullptr,
			ImGuiWindowFlags_NoTitleBar |
			ImGuiWindowFlags_NoResize |
			ImGuiWindowFlags_NoScrollbar);

		ImGui::Columns(2, nullptr, false); 	ImGui::SetColumnWidth(0, maxLeftWidth);
		ImGui::SetColumnWidth(1, maxRightWidth);

		

		const int warningThreshold = 60 * 10;
		const int alertThreshold = 60 * 2;

		ImVec4 color;
		if (remaining <= alertThreshold) {
			color = ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
		}
		else if (remaining <= warningThreshold) {
			color = ImVec4(1.0f, 1.0f, 0.0f, 1.0f);
		}
		else {
			color = ImVec4(0.5f, 1.0f, 0.5f, 1.0f);
		}

		auto nowgold = std::chrono::steady_clock::now();
		double elapsedSeconds = std::chrono::duration_cast<std::chrono::seconds>(nowgold - goldStartTime).count();

		double goldPerHour = 0.0;
		if (elapsedSeconds > 0) {
			goldPerHour = goldHrAccumulated / (elapsedSeconds / 3600.0);
		}
		if (lostsoulsreborn)
		{
			ImGui::TextColored(color, "%s", buf);
		}
		ImGui::TextColored(WHITE, "%s", expStr.c_str());

		ImGui::TextColored(WHITE, "%s", killStr.c_str());




		ImGui::TextColored(WHITE, "%s", currentLevelStr.c_str());
		ImGui::TextColored(WHITE, "%s", xpLeftStr.c_str());

		if (RestTimerActive && bastion) {
			auto now = std::chrono::steady_clock::now();
			auto restRemaining = std::chrono::duration_cast<std::chrono::seconds>(RestReadyTime - now).count();
			if (restRemaining <= 0) {
				RestTimerActive = false;
				restRemaining = 0;
			}

			float t = 1.0f - std::clamp(restRemaining / 120.0f, 0.0f, 1.0f);

			ImVec4 color;
			if (t < 0.5f) {
				color = ImVec4(1.0f, t * 2.0f, 0.0f, 1.0f);
			}
			else {
				color = ImVec4(1.0f - (t - 0.5f) * 2.0f, 1.0f, 0.0f, 1.0f);
			}

			ImGui::TextColored(color, "Rest ready in: %d:%02d", restRemaining / 60, restRemaining % 60);
		}

		if (!RestTimerActive && bastion)
		{
			color = ImVec4(0.0f, 1.0f, 0.0f, 1.0f);
			ImGui::TextColored(color, "Rest ready");
		}
		ImGui::NextColumn();
		ImGui::Text("%s", xpPerHourStr.c_str());
		ImGui::ProgressBar(displayedProgress, ImVec2(-1.0f, 0.0f));
		ImGui::Text("Gold: %d", totalGold);
		ImGui::Text("Gold/hr: %.1f", goldPerHour);

		ImGui::Columns(1);
		ImGui::End();
		ImGui::PopStyleColor();
	}
}
void Overlay::PlayerList()
{
	ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, bgAlpha));

	float rightEdgeX = infoWindowPos2.x;
	float posY = infoWindowPos2.y;

	std::string NumPlayer = "Number of players: " + FormatWithCommas(playerList.size());
	ImVec2 playersTextSize = ImGui::CalcTextSize(NumPlayer.c_str());
	float maxTextWidth = playersTextSize.x;
	float lineHeight = playersTextSize.y;
	for (const auto& player : playerList)
	{
		ImVec2 size = ImGui::CalcTextSize(player.c_str());
		if (size.x > maxTextWidth)
			maxTextWidth = size.x;
	}

	float totalWidth = maxTextWidth + 20.0f;
	int numLines = static_cast<int>(playerList.size()) + 1;
	float totalHeight = (lineHeight * numLines) + 40.0f;
	float windowPosX = rightEdgeX - totalWidth;
	ImGui::SetNextWindowPos(ImVec2(windowPosX, posY));
	ImGui::SetNextWindowSize(ImVec2(totalWidth, totalHeight), ImGuiCond_Always);
	if (Playerlistwindow)
	{
		ImGui::Begin("##playerlist", nullptr,
			ImGuiWindowFlags_NoTitleBar |
			ImGuiWindowFlags_NoResize |
			ImGuiWindowFlags_NoScrollbar);

		ImGui::Text("%s", NumPlayer.c_str());
		for (const auto& player : playerList)
		{
			ImGui::Text("%s", player.c_str());
		}

		ImGui::End();
		ImGui::PopStyleColor();
	}
}
extern std::chrono::steady_clock::time_point GSTimer;
extern std::chrono::steady_clock::time_point TimeStopTimer;
extern bool TimeStopTimerActive;


extern std::chrono::steady_clock::time_point GSShieldTimer;
extern bool GSShieldTimerActive;
extern int GSShieldCooldown;
extern float CombatTimer;
extern std::chrono::steady_clock::time_point LastDamageTime;
void Overlay::Cooldowns()
{
	ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, bgAlpha));

	float rightEdgeX = infoWindowPos3.x;
	float posY = infoWindowPos3.y;

	std::string header = "Spell Cooldowns:";
	ImVec2 headerSize = ImGui::CalcTextSize(header.c_str());
	float maxTextWidth = headerSize.x;
	float lineHeight = headerSize.y;

	auto now = std::chrono::steady_clock::now();

	std::string tsText = "Time Stop: ";
	bool tsActive = TimeStopTimerActive && now < TimeStopTimer;
	int tsRemaining = tsActive ? std::chrono::duration_cast<std::chrono::seconds>(TimeStopTimer - now).count() : 0;
	float tsT = tsActive ? 1.0f - (float)tsRemaining / tsCooldown : 1.0f;
	if (!TimeStopTimerActive || now >= TimeStopTimer)
		tsText += "Ready";
	else
		tsText += std::to_string(tsRemaining) + "s";

	ImVec4 tsColor = (tsText == "Ready") ? ImVec4(0, 1, 0, 1) : ImVec4(1.0f - tsT, tsT, 0.0f, 1.0f);

	ImVec2 tsTextSize = ImGui::CalcTextSize(tsText.c_str());
	if (tsTextSize.x > maxTextWidth)
		maxTextWidth = tsTextSize.x;

	std::string gsText = "Greater Sanctuary: ";
	constexpr int gsCooldown = 240;
	bool gsActive = GSTimerActive && now < GSTimer;
	int gsRemaining = gsActive ? std::chrono::duration_cast<std::chrono::seconds>(GSTimer - now).count() : 0;
	float gsT = gsActive ? 1.0f - (float)gsRemaining / gsCooldown : 1.0f;
	if (!GSTimerActive || now >= GSTimer)
		gsText += "Ready";
	else
		gsText += std::to_string(gsRemaining) + "s";

	ImVec4 gsColor = (gsText == "Ready") ? ImVec4(0, 1, 0, 1) : ImVec4(1.0f - gsT, gsT, 0.0f, 1.0f);

	ImVec2 gsTextSize = ImGui::CalcTextSize(gsText.c_str());
	if (gsTextSize.x > maxTextWidth)
		maxTextWidth = gsTextSize.x;

	std::string fsText = "Greater Sanctuary Time: ";
	auto nowgs = std::chrono::steady_clock::now();
	bool fsActive = GSShieldTimerActive && nowgs < GSShieldTimer;
	int fsRemaining = fsActive ? std::chrono::duration_cast<std::chrono::seconds>(GSShieldTimer - nowgs).count() : 0;
	float fsT = fsActive ? 1.0f - (float)fsRemaining / GSShieldCooldown : 1.0f;
	if (!GSShieldTimerActive || nowgs >= GSShieldTimer)
		fsText += "Ready";
	else
		fsText += std::to_string(fsRemaining) + "s";

	ImVec4 fsColor = (fsText == "Ready") ? ImVec4(0, 1, 0, 1) : ImVec4(1.0f - fsT, fsT, 0.0f, 1.0f);

	ImVec2 fsTextSize = ImGui::CalcTextSize(fsText.c_str());
	if (fsTextSize.x > maxTextWidth)
		maxTextWidth = fsTextSize.x;

	int numLines = 4;
	float totalHeight = (lineHeight * numLines) + 25.0f;
	float totalWidth = maxTextWidth + 20.0f;
	float windowPosX = rightEdgeX - totalWidth;

	ImGui::SetNextWindowPos(ImVec2(windowPosX, posY));
	ImGui::SetNextWindowSize(ImVec2(totalWidth, totalHeight), ImGuiCond_Always);
	if (Cooldownwindow)
	{
		ImGui::Begin("##Cooldowns", nullptr,
			ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar);

		ImGui::Text("%s", header.c_str());
		ImGui::TextColored(gsColor, "%s", gsText.c_str());
		ImGui::TextColored(fsColor, "%s", fsText.c_str());
		ImGui::TextColored(tsColor, "%s", tsText.c_str());

		ImGui::End();

		ImGui::PopStyleColor();
	}
}
static inline std::string trim(const std::string& s) {
	size_t start = s.find_first_not_of(" \t\n\r");
	if (start == std::string::npos) return "";
	size_t end = s.find_last_not_of(" \t\n\r");
	return s.substr(start, end - start + 1);
}
void Overlay::RenderMenu()
{
	
	ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 1.0f));

	ImGui::SetNextWindowPos(ImVec2(100, 0));
	ImGui::SetNextWindowSize(ImVec2(500, 450));
	static bool menuOpen = true;
	ImGui::Begin(XorStr("Config"), (bool*)true, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar);
	
	
	ImGui::DragFloat("Tracker X", &infoWindowPos.x, 1.0f);
	ImGui::DragFloat("Tracker Y", &infoWindowPos.y, 1.0f);
	ImGui::DragFloat("Player List X", &infoWindowPos2.x, 1.0f);
	ImGui::DragFloat("Player List Y", &infoWindowPos2.y, 1.0f);
	ImGui::DragFloat("Cooldown List X", &infoWindowPos3.x, 1.0f);
	ImGui::DragFloat("Cooldown List Y", &infoWindowPos3.y, 1.0f);
	ImGui::DragFloat("DPS X", &DPSWindowPos.x, 1.0f);
	ImGui::DragFloat("DPS Y", &DPSWindowPos.y, 1.0f);
	ImGui::DragFloat("Buff's X", &BuffWindowPos.x, 1.0f);
	ImGui::DragFloat("Buff's Y", &BuffWindowPos.y, 1.0f);
	ImGui::SliderFloat("Background Alpha", &bgAlpha, 0.0f, 1.0f, "%.2f");
	ImGui::Sliderbox("Buffs", &Buffwindow);
	ImGui::Sliderbox("Cooldowns", &Cooldownwindow);
	ImGui::Sliderbox("Stats", &Stattrackerwindow);
	ImGui::Sliderbox("DPS Meter", &DPSwindow);
	ImGui::Sliderbox("Player List", &Playerlistwindow);
	if (ImGui::Sliderbox("Bastion", &bastion))
	{
		lostsoulsreborn = false;
		TimeStopCooldown = 9;
		tsCooldown = 9;
		bastion = true;
	}
	if (ImGui::Sliderbox("Lost Souls Reborn", &lostsoulsreborn))
	{
		bastion = false;
		TimeStopCooldown = 20;
		tsCooldown = 20;
		lostsoulsreborn = true;
	}
	if (ImGui::Button("Save Position"))
	{
		std::ifstream inFile("Config.txt");
		std::vector<std::string> lines;
		if (inFile.is_open()) {
			std::string line;
			while (std::getline(inFile, line)) {
				lines.push_back(line);
			}
			inFile.close();
		}
		while (lines.size() < 17) {
			lines.push_back(""); 
		}

		lines[2] = std::to_string(static_cast<int>(infoWindowPos.x)) + " " + std::to_string(static_cast<int>(infoWindowPos.y));
		lines[3] = std::to_string(static_cast<int>(infoWindowPos2.x)) + " " + std::to_string(static_cast<int>(infoWindowPos2.y));
		lines[4] = std::to_string(static_cast<float>(bgAlpha));
		lines[5] = std::to_string(static_cast<int>(infoWindowPos3.x)) + " " + std::to_string(static_cast<int>(infoWindowPos3.y));
		lines[6] = std::to_string(static_cast<int>(DPSWindowPos.x)) + " " + std::to_string(static_cast<int>(DPSWindowPos.y));
		lines[7] = std::to_string(static_cast<int>(BuffWindowPos.x)) + " " + std::to_string(static_cast<int>(BuffWindowPos.y));
		lines[8] = std::to_string(static_cast<bool>(bastion));
		lines[9] = std::to_string(static_cast<bool>(lostsoulsreborn));
		lines[10] = std::to_string(static_cast<int>(tsCooldown));
		lines[11] = std::to_string(static_cast<int>(TimeStopCooldown));

		lines[12] = std::to_string(static_cast<bool>(Buffwindow));
		lines[13] = std::to_string(static_cast<bool>(Cooldownwindow));
		lines[14] = std::to_string(static_cast<bool>(Stattrackerwindow));
		lines[15] = std::to_string(static_cast<bool>(DPSwindow));
		lines[16] = std::to_string(static_cast<bool>(Playerlistwindow));

		std::ofstream outFile("Config.txt");
		if (outFile.is_open()) {
			for (const auto& line : lines) {
				outFile << line << "\n";
			}
			outFile.close();
		}
		else {
			std::cerr << "Failed to open Config.txt for writing\n";
		}
	}

	ImGui::End();
	ImGui::PopStyleColor();
}
extern std::map<std::string, PlayerDPS> players;

std::string FormatInt(double value) {
	return FormatWithCommas(static_cast<int64_t>(std::round(value)));
}
void Overlay::RenderDPSMeter() {
	ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, bgAlpha));

	ImGui::SetNextWindowPos(DPSWindowPos, ImGuiCond_Always);
	ImGui::SetNextWindowSize(ImVec2(400, 150), ImGuiCond_Always);
	if (DPSwindow)
	{
		ImGui::Begin("##DPS", nullptr,
			ImGuiWindowFlags_NoTitleBar |
			ImGuiWindowFlags_NoResize |
			ImGuiWindowFlags_NoScrollbar |
			ImGuiWindowFlags_NoCollapse);

		for (auto& [name, p] : players) {
			p.Update();
		}
		static auto prevTime = std::chrono::steady_clock::now();
		auto now = std::chrono::steady_clock::now();
		float deltaTime = std::chrono::duration<float>(now - prevTime).count();
		prevTime = now;

		// decrease timer smoothly
		if (CombatTimer > 0.0f) {
			CombatTimer -= deltaTime;
			if (CombatTimer < 0.0f) CombatTimer = 0.0f;
		}
		if (CombatTimer == 0.0f) {
			for (auto& [name, p] : players) {
				p.combatDamage = 0;
				p.damageAccum = 0;
				p.dpsActive = false;
			}
		}
		std::vector<std::pair<std::string, PlayerDPS>> sorted(players.begin(), players.end());
		std::sort(sorted.begin(), sorted.end(),
			[](auto& a, auto& b) {
				return a.second.totalDamage > b.second.totalDamage;
			});

		if (ImGui::BeginTable("dpsTable", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable)) {
			ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
			ImGui::TableSetupColumn("Total Damage", ImGuiTableColumnFlags_WidthStretch);
			ImGui::TableSetupColumn("Combat Damage", ImGuiTableColumnFlags_WidthStretch);
			ImGui::TableSetupColumn("DPS", ImGuiTableColumnFlags_WidthStretch);
			ImGui::TableHeadersRow();

			int showCount = std::min(5, (int)sorted.size());
			for (int i = 0; i < showCount; i++) {
				auto& [name, p] = sorted[i];
				ImGui::TableNextRow();

				ImVec4 color;
				switch (i) {
				case 0: color = ImVec4(1.0f, 0.84f, 0.0f, 1.0f); break;   // Gold
				case 1: color = ImVec4(0.75f, 0.75f, 0.75f, 1.0f); break; // Silver
				case 2: color = ImVec4(0.8f, 0.5f, 0.2f, 1.0f); break;    // Bronze
				case 3: color = ImVec4(0.5f, 0.7f, 1.0f, 1.0f); break;    // Light Blue
				case 4: color = ImVec4(0.6f, 0.8f, 0.6f, 1.0f); break;    // Light Green
				default: color = ImVec4(1.0f, 1.0f, 1.0f, 1.0f); break;   // fallback
				}

				ImGui::PushStyleColor(ImGuiCol_Text, color);

				ImGui::TableSetColumnIndex(0);
				ImGui::TextUnformatted(name.c_str());

				ImGui::TableSetColumnIndex(1);
				ImGui::TextUnformatted(FormatWithCommas(p.totalDamage).c_str());
				ImGui::TableSetColumnIndex(2);
				ImGui::TextUnformatted(FormatWithCommas(p.combatDamage).c_str());

				ImGui::TableSetColumnIndex(3);
				ImGui::TextUnformatted(FormatInt(p.GetDPS()).c_str());

				ImGui::PopStyleColor();
			}

			ImGui::EndTable();
			ImGui::Text("Combat Timer: %.0f", CombatTimer);
		}

		ImGui::End();
		ImGui::PopStyleColor();
	}
}
extern int spellLevelsRemaining;
extern bool mantleActive;
extern bool trackingSpellMantle;
extern std::chrono::steady_clock::time_point mantleExpireTime;
void Overlay::BuffWindow() {
	ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, bgAlpha));

	ImGui::SetNextWindowPos(BuffWindowPos, ImGuiCond_Always);
	ImGui::SetNextWindowSize(ImVec2(300, 100), ImGuiCond_Always);
	if (mantleActive && std::chrono::steady_clock::now() >= mantleExpireTime)
	{
		spellLevelsRemaining = 0;
		trackingSpellMantle = false;
		mantleActive = false;
	}
	if (Buffwindow)
	{
		ImGui::Begin("##Buffs", nullptr,
			ImGuiWindowFlags_NoTitleBar |
			ImGuiWindowFlags_NoResize |
			ImGuiWindowFlags_NoScrollbar |
			ImGuiWindowFlags_NoCollapse);
		ImGui::Text("Buffs");
		ImGui::Dummy(ImVec2(0.0f, 3.0f));
		if (mantleActive)
		{

			ImGui::Text("Greater Spell Mantle | Min 11 | Max 22");
			float frac = spellLevelsRemaining / 22.0f;
			ImGui::ProgressBar(frac, ImVec2(200, 20),
				(std::to_string(spellLevelsRemaining) + " left").c_str());
			if (spellLevelsRemaining <= 0)
			{
				mantleActive = false;
				trackingSpellMantle = false;
			}
			else
			{
				auto now = std::chrono::steady_clock::now();
				int totalSeconds = std::chrono::duration_cast<std::chrono::seconds>(mantleExpireTime - now).count();

				if (totalSeconds <= 0)
				{
					mantleActive = false;
					spellLevelsRemaining = 0;
					trackingSpellMantle = false;
				}
				else
				{
					LevelInfo lvlInfo = GetLevelFromXP(currentXP);
					float frac = float(totalSeconds) / float(lvlInfo.currentLevel * 6);

					// Interpolate color from green to red
					ImVec4 barColor = ImVec4(1.0f - frac, frac, 0.0f, 1.0f); // R,G,B,A

					ImGui::PushStyleColor(ImGuiCol_PlotHistogram, barColor);
					ImGui::ProgressBar(frac, ImVec2(200, 20), std::to_string(totalSeconds).c_str());
					ImGui::PopStyleColor();
				}
			}
		}

		ImGui::End();
		ImGui::PopStyleColor();
	}
}


void Overlay::ClickThrough (bool v)
{
	if (v)
	{
		nv_edit = nv_default_in_game | WS_VISIBLE;
		if (GetWindowLong(overlayHWND, GWL_EXSTYLE) != nv_ex_edit)
			SetWindowLong(overlayHWND, GWL_EXSTYLE, nv_ex_edit);
	}
	else
	{
		nv_edit = nv_default | WS_VISIBLE;
		if (GetWindowLong(overlayHWND, GWL_EXSTYLE) != nv_ex_edit_menu)
			SetWindowLong(overlayHWND, GWL_EXSTYLE, nv_ex_edit_menu);
	}
}
bool k_ins = false;
bool show_menu = false;
extern bool IsKeyDown(int vk);

DWORD Overlay::CreateOverlay()
{
	EnumWindows(EnumWindowsCallback, (LPARAM)&overlayHWND);
	Sleep(300);
	if (overlayHWND == 0)
	{
		printf(XorStr("Can't find the overlay, HIT ALT-Z twice while in game and try again\n"));
		Sleep(5000);
		exit(0);
	}

	HDC hDC = ::GetWindowDC(NULL);
	width = ::GetDeviceCaps(hDC, HORZRES);
	height = ::GetDeviceCaps(hDC, VERTRES);

	running = true;

	if (!CreateDeviceD3D(overlayHWND))
	{
		CleanupDeviceD3D();
		return 1;
	}
	::ShowWindow(overlayHWND, SW_SHOWDEFAULT);
	::UpdateWindow(overlayHWND);

		IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	
		ImGui::StyleColorsDark();
	ImGui::GetStyle().WindowMinSize = ImVec2(1, 1);

		ImGui_ImplWin32_Init(overlayHWND);
	ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

	ImVec4 clear_color = ImVec4(0.0f, 0.0f, 0.0f, 0.00f);

		MSG msg;
	ZeroMemory(&msg, sizeof(msg));

	ClickThrough(false);
	while (running)
	{
		HWND wnd = GetWindow(GetForegroundWindow(), GW_HWNDPREV);
		if (use_nvidia)
		{
			if (GetWindowLong(overlayHWND, GWL_STYLE) != nv_edit)
				SetWindowLong(overlayHWND, GWL_STYLE, nv_edit);
			if (show_menu)
			{
				ClickThrough(false);
			}
			else
			{
				if (GetWindowLong(overlayHWND, GWL_EXSTYLE) != nv_ex_edit)
					SetWindowLong(overlayHWND, GWL_EXSTYLE, nv_ex_edit);
				ClickThrough(true);
			}
		}
		if (wnd != overlayHWND)
		{
			SetWindowPos(overlayHWND, wnd, 0, 0, 0, 0, SWP_ASYNCWINDOWPOS | SWP_NOMOVE | SWP_NOSIZE);
			::UpdateWindow(overlayHWND);
		}

		if (::PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE))
		{
			::TranslateMessage(&msg);
			::DispatchMessage(&msg);
			continue;
		}

				ImGui_ImplDX11_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();
		ImGuiIO& io = ImGui::GetIO();

		io.MousePos = ImVec2(io.MousePos.x, io.MousePos.y);

				io.MouseDown[0] = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
				if (IsKeyDown(VK_INSERT) && !k_ins)
		{
			show_menu = !show_menu;
			ClickThrough(!show_menu);
			k_ins = true;
		}
		else if (!IsKeyDown(VK_INSERT) && k_ins)
		{
			k_ins = false;
		}
		if (show_menu)
			RenderMenu();

		RenderInfo();
		PlayerList();
		Cooldowns();
		RenderDPSMeter();
		BuffWindow();
		
		


		ImGui::EndFrame();
		ImGui::Render();
		const float clear_color_with_alpha[4] = { clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w };
		g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, NULL);
		g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color_with_alpha);
		ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

		g_pSwapChain->Present(1, 0); 
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
	ClickThrough(true);

	CleanupDeviceD3D();
	::DestroyWindow(overlayHWND);
	return 0;
}

void Overlay::Start()
{
	DWORD ThreadID;
	CreateThread(NULL, 0, StaticMessageStart, (void*)this, 0, &ThreadID);
}

void Overlay::Clear()
{
	running = 0;
	Sleep(50);
	if (use_nvidia)
	{
		SetWindowLong(overlayHWND, GWL_STYLE, nv_default);
		SetWindowLong(overlayHWND, GWL_EXSTYLE, nv_ex_default);
	}
}

int Overlay::getWidth()
{
	return width;
}

int Overlay::getHeight()
{
	return height;
}


void CreateRenderTarget()
{
	ID3D11Texture2D* pBackBuffer;
	g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
	if (pBackBuffer)
	{
		g_pd3dDevice->CreateRenderTargetView(pBackBuffer, NULL, &g_mainRenderTargetView);
		pBackBuffer->Release();
	}
}

bool CreateDeviceD3D(HWND hWnd)
{
		DXGI_SWAP_CHAIN_DESC sd;
	ZeroMemory(&sd, sizeof(sd));
	sd.BufferCount = 2;
	sd.BufferDesc.Width = 0;
	sd.BufferDesc.Height = 0;
	sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	sd.BufferDesc.RefreshRate.Numerator = 60;
	sd.BufferDesc.RefreshRate.Denominator = 1;
	sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.OutputWindow = hWnd;
	sd.SampleDesc.Count = 1;
	sd.SampleDesc.Quality = 0;
	sd.Windowed = TRUE;
	sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

	UINT createDeviceFlags = 0;
		D3D_FEATURE_LEVEL featureLevel;
	const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };
	if (D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext) != S_OK)
		return false;

	CreateRenderTarget();
	return true;
}

void CleanupRenderTarget()
{
	if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = NULL; }
}

void CleanupDeviceD3D()
{
	CleanupRenderTarget();
	if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = NULL; }
	if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = NULL; }
	if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = NULL; }
}



