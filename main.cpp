#pragma warning (disable : 4715)
#pragma warning (disable : 4005)
#pragma warning (disable : 4305)
#pragma warning (disable : 4244)
#define NOMINMAX
#include "main.h"
#include <random>
#include <map>
#include <fstream>
#include <atomic>
#include <string>
#include <Windows.h>
#include <sstream>
#include <ctime>
#include <algorithm>
#include <regex>
#include <filesystem>
#include "Players.h"
bool use_nvidia = true; 

struct Config {
    std::string logFilePath;
    std::string userName;
} g_config;

void static LoadConfig(const std::string& filename) {
    std::ifstream file(filename);
    std::string line;
    while (std::getline(file, line)) {
        if (line.rfind("LogFilePath=", 0) == 0) {
            std::string path = line.substr(12);
            if (!path.empty() && path.front() == '"' && path.back() == '"') {
                path = path.substr(1, path.size() - 2);
            }
            g_config.logFilePath = path;
            std::cout << "Loaded log at: " << g_config.logFilePath << std::endl;
        }
        if (line.rfind("UserName=", 0) == 0) {
            std::string name = line.substr(9);
            if (!name.empty() && name.front() == '"' && name.back() == '"') {
                name = name.substr(1, name.size() - 2);
            }
            g_config.userName = name;
            std::cout << "Loaded username: " << g_config.userName << std::endl;
        }
    }
}
void static SendAltZ()
{
    INPUT inputs[4] = {};

        inputs[0].type = INPUT_KEYBOARD;
    inputs[0].ki.wVk = VK_MENU; 
        inputs[1].type = INPUT_KEYBOARD;
    inputs[1].ki.wVk = 'Z';

        inputs[2].type = INPUT_KEYBOARD;
    inputs[2].ki.wVk = 'Z';
    inputs[2].ki.dwFlags = KEYEVENTF_KEYUP;

        inputs[3].type = INPUT_KEYBOARD;
    inputs[3].ki.wVk = VK_MENU;
    inputs[3].ki.dwFlags = KEYEVENTF_KEYUP;

    SendInput(4, inputs, sizeof(INPUT));
}
void static ToggleNvidiaOverlayTwice()
{
    SendAltZ();
    Sleep(200);
    SendAltZ();
}
bool IsKeyDown(int vk)
{
    return (GetAsyncKeyState(vk) & 0x8000) != 0;
}
std::atomic<int64_t> totalDamage{ 0 };
std::atomic<bool> parserRunning{ true };
std::atomic<int64_t> totalExperience{ 0 };
std::atomic<int64_t> totalKills{ 0 };
std::atomic<int64_t> xpHrAccumulated{ 0 };
int currentXP = 0;
std::chrono::steady_clock::time_point serverRestartTime;
bool serverRestartActive = false;
std::vector<std::string> playerList;
std::chrono::steady_clock::time_point GSTimer;
bool GSTimerActive = false;
std::chrono::steady_clock::time_point TimeStopTimer;
bool TimeStopTimerActive = false;
int TimeStopCooldown = 20;
std::chrono::steady_clock::time_point GSShieldTimer;
bool GSShieldTimerActive = false;
int GSShieldCooldown = 45;
int totalGold = 0;
int currentGold = 0;
int goldHrAccumulated = 0;
auto goldStartTime = std::chrono::steady_clock::now();
#define GREEN ImColor(0, 255, 0)
#define RED ImColor(255, 0, 0)
#define BLUE ImColor(0, 0, 255)
#define ORANGE ImColor(255, 165, 0)
#define WHITE ImColor(255, 255, 255)
#define TEAL ImColor(0, 128, 128)
#define YELLOW ImColor(255, 255, 0)
#define GOLD ImColor(255, 215, 0)
#define PURPLE ImColor(128, 0, 128)
bool seenGSCast = false;
namespace fs = std::filesystem;
static std::string GetLatestLogFile(const std::string& logDir)
{
    namespace fs = std::filesystem;

    std::string latestLog;
    fs::file_time_type latestTime;

    for (int i = 1; i <= 10; i++) {
        std::string logPath = logDir + "\\nwclientLog" + std::to_string(i) + ".txt";

        if (fs::exists(logPath)) {
            auto writeTime = fs::last_write_time(logPath);

            if (latestLog.empty() || writeTime > latestTime) {
                latestTime = writeTime;
                latestLog = logPath;
            }
        }
    }

    return latestLog;
}
std::map<std::string, PlayerDPS> players;
void static LogParserThread(const std::string& logDir)
{
    std::string currentLog = GetLatestLogFile(logDir);
    if (currentLog.empty())
    {
        printf("No log files found in: %s\n", logDir.c_str());
        return;
    }

    std::ifstream logFile(currentLog);
    if (!logFile.is_open())
    {
        printf("Failed to open log file: %s\n", currentLog.c_str());
        return;
    }
    printf("Opened log file: %s\n", currentLog.c_str());  // ✅ print it here

    logFile.seekg(0, std::ios::end);

    std::string line;
    while (parserRunning)
    {
        if (std::getline(logFile, line))
        {
            size_t dmgPos = line.find(" damages ");
            if (dmgPos != std::string::npos)
            {
                // Skip the "[CHAT WINDOW TEXT] [timestamp]" part
                size_t rightBracket = line.find("]", line.find("]") + 1); // second ']'
                if (rightBracket != std::string::npos) {
                    size_t nameStart = rightBracket + 2; // skip "] "

                    // Attacker name is from here up until " damages "
                    std::string attacker = line.substr(nameStart, dmgPos - nameStart);
                    attacker.erase(0, attacker.find_first_not_of(" ")); // trim left
                    attacker.erase(attacker.find_last_not_of(" ") + 1); // trim right

                    // Damage value is after ':'
                    size_t colonPos = line.find(':', dmgPos);
                    if (colonPos != std::string::npos) {
                        size_t startNum = colonPos + 1;
                        while (startNum < line.size() && line[startNum] == ' ') startNum++;

                        size_t endNum = startNum;
                        while (endNum < line.size() && isdigit(line[endNum])) endNum++;

                        if (endNum > startNum) {
                            try {
                                int dmg = std::stoi(line.substr(startNum, endNum - startNum));

                                auto& p = players[attacker];
                                p.name = attacker;
                                if (p.totalDamage == 0) {
                                    p.firstHit = std::chrono::steady_clock::now();
                                }
                                p.totalDamage += dmg;
                                totalDamage += dmg;
                                p.AddDamage(dmg);
                                p.lastHit = std::chrono::steady_clock::now();
                            }
                            catch (...) {}
                        }
                    }
                }
            }
            size_t xpPos = line.find("Experience Points Gained:");
            if (xpPos != std::string::npos)
            {
                size_t startNum = xpPos + strlen("Experience Points Gained:");
                while (startNum < line.size() && line[startNum] == ' ') startNum++;

                size_t endNum = startNum;
                while (endNum < line.size() && isdigit(line[endNum])) endNum++;

                if (endNum > startNum)
                {
                    try
                    {
                        int xp = std::stoi(line.substr(startNum, endNum - startNum));
                        totalExperience += xp;
                        currentXP += xp;
                        xpHrAccumulated += xp;
                    }
                    catch (...) {}
                }
            }
            size_t killPos = line.find(" killed ");
            if (killPos != std::string::npos)
            {
                size_t killPos = line.find(" killed ");
                if (killPos != std::string::npos)
                {
                    totalKills += 1;
                }
            }
            size_t chatPos = line.find("currentxp is ");
            if (chatPos != std::string::npos)
            {
                std::string xpStr = line.substr(chatPos + strlen("currentxp is "));
                try {
                    currentXP = std::stoi(xpStr);
                }
                catch (std::exception&)
                {
                    printf("Failed to parse XP from chat input\n");
                }
            }
            size_t shoutPos = line.find("SERVER : [Shout]");
            if (shoutPos != std::string::npos && line.find("until server restart!") != std::string::npos)
            {
                std::smatch match;
                std::regex timeRegex(R"((\d+)\s+(minute|minutes|hour|hours)\s+until server restart!)");
                if (std::regex_search(line, match, timeRegex))
                {
                    int value = std::stoi(match[1].str());
                    std::string unit = match[2].str();

                    int totalSeconds = 0;
                    if (unit.find("hour") != std::string::npos)
                        totalSeconds = value * 3600;
                    else
                        totalSeconds = value * 60;

                    serverRestartTime = std::chrono::steady_clock::now() + std::chrono::seconds(totalSeconds);
                    serverRestartActive = true;
                }
            }
            size_t restart = line.find("The server will restart in");
            if (restart != std::string::npos) {
                int hrs = 0, mins = 0, secs = 0;
                std::sscanf(line.c_str() + restart, "The server will restart in %d hours, %d minutes, %d seconds", &hrs, &mins, &secs);

                auto now = std::chrono::steady_clock::now();
                serverRestartTime = now + std::chrono::hours(hrs) + std::chrono::minutes(mins) + std::chrono::seconds(secs);
                serverRestartActive = true;
            }
            std::string joinText = " has joined as a";
            size_t joined = line.find(joinText);
            if (joined != std::string::npos)
            {
                size_t lastBracketPos = line.rfind("] ");
                if (lastBracketPos != std::string::npos)
                {
                    size_t nameStart = lastBracketPos + 2;
                    std::string playerName = line.substr(nameStart, joined - nameStart);
                    playerName.erase(0, playerName.find_first_not_of(" \t"));
                    playerName.erase(playerName.find_last_not_of(" \t") + 1);

                    if (std::find(playerList.begin(), playerList.end(), playerName) == playerList.end())
                    {
                        playerList.push_back(playerName);
                    }
                }
            }
            size_t firstBracketPos = line.find("] ");
            if (firstBracketPos != std::string::npos)
            {
                size_t secondBracketPos = line.find("] ", firstBracketPos + 1);
                if (secondBracketPos != std::string::npos)
                {
                    size_t nameStart = secondBracketPos + 2;
                    size_t leavePos = line.find(" has left as a");
                    if (leavePos != std::string::npos && leavePos > nameStart)
                    {
                        std::string playerName = line.substr(nameStart, leavePos - nameStart);
                        auto trim = [](std::string& s) {
                            size_t start = s.find_first_not_of(' ');
                            size_t end = s.find_last_not_of(' ');
                            if (start == std::string::npos || end == std::string::npos)
                                s = "";
                            else
                                s = s.substr(start, end - start + 1);
                            };
                        trim(playerName);
                        auto it = std::find(playerList.begin(), playerList.end(), playerName);
                        if (it != playerList.end())
                        {
                            playerList.erase(it);
                        }
                    }
                }
            }
            size_t lostPos = line.find("Experience Points Lost:");
            if (lostPos != std::string::npos)
            {
                size_t startNum = lostPos + strlen("Experience Points Lost:");
                while (startNum < line.size() && line[startNum] == ' ') startNum++;

                size_t endNum = startNum;
                while (endNum < line.size() && isdigit(line[endNum])) endNum++;

                if (endNum > startNum)
                {
                    try
                    {
                        int xpLost = std::stoi(line.substr(startNum, endNum - startNum));
                        totalExperience -= xpLost;
                        currentXP -= xpLost;
                        if (totalExperience < 0) totalExperience = 0;
                        if (currentXP < 0) currentXP = 0;
                    }
                    catch (...) {}
                }
            }
            size_t TS = line.find("casts Time Stop");
            if (TS != std::string::npos)
            {
                TimeStopTimer = std::chrono::steady_clock::now() + std::chrono::seconds(TimeStopCooldown);
                TimeStopTimerActive = true;
            }

            size_t lostGoldPos = line.find("Lost ");
            size_t acquiredGoldPos = line.find("Acquired ");

            if (lostGoldPos != std::string::npos) {
                int goldLost = 0;
                if (sscanf(line.c_str() + lostGoldPos, "Lost %dGP", &goldLost) == 1) {
                    totalGold -= goldLost;
                    currentGold -= goldLost;
                    goldHrAccumulated -= goldLost;
                }
            }
            else if (acquiredGoldPos != std::string::npos) {
                int goldGained = 0;
                if (sscanf(line.c_str() + acquiredGoldPos, "Acquired %dGP", &goldGained) == 1) {
                    totalGold += goldGained;
                    currentGold += goldGained;
                    goldHrAccumulated += goldGained;
                }
            }
            size_t GS = line.find("casts Greater Sanctuary");
            size_t GSCancel = line.find("You have used Greater Sanctuary too recently, the effect has been cancelled");

            if (GS != std::string::npos)
            {
                seenGSCast = true;
            }
            else if (seenGSCast)
            {
                if (GSCancel != std::string::npos)
                {
                    seenGSCast = false;
                }
                else
                {
                    GSTimer = std::chrono::steady_clock::now() + std::chrono::seconds(240);
                    GSTimerActive = true;
                    GSShieldTimer = std::chrono::steady_clock::now() + std::chrono::seconds(GSShieldCooldown);
                    GSShieldTimerActive = true;

                    seenGSCast = false;
                }
            }
            
        }
        else
        {
            // Check if a new log appeared
            std::string latest = GetLatestLogFile(logDir);
            if (latest != currentLog)
            {
                printf("Switching to new log: %s\n", latest.c_str());
                logFile.close();
                currentLog = latest;
                logFile.open(currentLog);
                if (!logFile.is_open())
                {
                    printf("Failed to open new log file: %s\n", currentLog.c_str());
                    return;
                }
                // Start reading from the beginning of the new log
                logFile.seekg(0, std::ios::beg);
            }
            else
            {
                logFile.clear();
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
        }
    }

}

int main(int argc, char** argv)
{
    LoadConfig("config.txt");
    Overlay ov1;
    ov1.Start();

    
    printf(XorStr("Press Enter to exit...\n"));
	std::thread parserThread(LogParserThread, g_config.logFilePath);
    getchar();

    ov1.Clear();
	parserRunning = false;
	if (parserThread.joinable())
		parserThread.join();
    ToggleNvidiaOverlayTwice();

    return 0;
}