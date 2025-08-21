#pragma once
#include <string>
#include <chrono>

struct PlayerDPS {
    std::string name;
    int64_t totalDamage = 0;

    std::chrono::steady_clock::time_point firstHit;
    std::chrono::steady_clock::time_point lastHit;
    int64_t combatDamage = 0;
    // For fluid DPS calculation
    int64_t damageAccum = 0;
    std::chrono::steady_clock::time_point dpsStartTime;
    std::chrono::steady_clock::time_point lastDamageTime;
    bool dpsActive = false;

    // For animated bar
    float displayedDPS = 0.0f;

    static constexpr int DPS_RESET_SECONDS = 10; // reset after 10s of no damage

    double GetDPS() const {
        if (!dpsActive) return 0.0;
        auto now = std::chrono::steady_clock::now();
        double elapsedSec = std::chrono::duration_cast<std::chrono::milliseconds>(now - dpsStartTime).count() / 1000.0;
        if (elapsedSec <= 0.0) elapsedSec = 1.0;
        return static_cast<double>(damageAccum) / elapsedSec;
    }

    void AddDamage(int dmg) {
        auto now = std::chrono::steady_clock::now();
        if (!dpsActive) {
            dpsStartTime = now;
            damageAccum = 0;
            dpsActive = true;
            combatDamage = 0;
        }

        damageAccum += dmg;
        combatDamage += dmg;
        lastDamageTime = now;
        totalDamage += dmg;

        if (firstHit == lastHit) firstHit = now;
        lastHit = now;
    }
    
    void Update() {
        auto now = std::chrono::steady_clock::now();

        

        // Animate displayed DPS towards actual DPS smoothly
        float targetDPS = static_cast<float>(GetDPS());
        float speed = 5.0f; // higher = faster animation
        displayedDPS += (targetDPS - displayedDPS) * ImGui::GetIO().DeltaTime * speed;
    }
};