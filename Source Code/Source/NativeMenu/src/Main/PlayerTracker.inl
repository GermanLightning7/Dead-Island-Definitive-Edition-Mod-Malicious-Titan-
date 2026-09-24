#pragma once
#include <atomic>
#include <vector>
#include <cmath>
#include <algorithm>
#include <mutex>

namespace PlayerTracker {
inline std::atomic_bool enabled{false};

struct PlayerInfo {
    uintptr_t address{0};
    bool isLocal{false};
    V pos{0.f, 0.f, 0.f};
    unsigned long long lastSeen{0};
};

inline std::mutex playerMutex;
inline std::vector<PlayerInfo> trackedPlayers;

inline void Toggle(){
    bool next = !enabled.load();
    enabled = next;
    Log(next ? "PLAYER_TRACKER_ENABLED" : "PLAYER_TRACKER_DISABLED");
    SetStatus(next ? "PLAYER TRACKER ENABLED" : "PLAYER TRACKER DISABLED");
}

inline void UpdatePlayer(uintptr_t addr, bool local, V position){
    if (!addr || !Finite(position)) return;
    std::lock_guard<std::mutex> lock(playerMutex);
    auto now = GetTickCount64();
    for (auto& p : trackedPlayers) {
        if (p.address == addr) {
            p.pos = position;
            p.isLocal = local;
            p.lastSeen = now;
            return;
        }
    }
    trackedPlayers.push_back({addr, local, position, now});
}

inline void PruneStale(){
    std::lock_guard<std::mutex> lock(playerMutex);
    auto now = GetTickCount64();
    trackedPlayers.erase(
        std::remove_if(trackedPlayers.begin(), trackedPlayers.end(),
            [now](const PlayerInfo& p){ return now - p.lastSeen > 2000; }),
        trackedPlayers.end()
    );
}

inline bool GetLocalPlayerCoords(char* buf, size_t size){
    if (!buf || size == 0) return false;
    uintptr_t local = 0;
    if (TrueGodPlayer(local) && local) {
        V pos{};
        if (Read(local + 0x578, pos) && Finite(pos)) {
            sprintf_s(buf, size, "%.1f, %.1f, %.1f", pos.x, pos.y, pos.z);
            return true;
        }
    }
    strcpy_s(buf, size, "SEARCHING...");
    return false;
}

inline void Draw(std::vector<D3DVertex>& verts, float w, float h){
    if (!enabled.load() || w <= 0.f || h <= 0.f) return;

    // Refresh local player position directly each frame
    uintptr_t local = 0;
    if (TrueGodPlayer(local) && local) {
        V localPos{};
        if (Read(local + 0x578, localPos) && Finite(localPos)) {
            UpdatePlayer(local, true, localPos);
        }
    }

    PruneStale();

    V camPos{}, camRight{}, camUp{}, camBack{};
    float sx = 0, sy = 0;
    if (!ReadD3DCamera(camPos, camRight, camUp, camBack, sx, sy)) return;

    std::vector<PlayerInfo> currentList;
    {
        std::lock_guard<std::mutex> lock(playerMutex);
        currentList = trackedPlayers;
    }

    for (const auto& player : currentList) {
        // Place the marker 2.0 meters above player root (above head)
        V markerWorld = player.pos;
        markerWorld.y += 2.05f;

        float screenX = 0, screenY = 0;
        if (!ProjectD3D(markerWorld, camPos, camRight, camUp, camBack, sx, sy, w, h, screenX, screenY)) {
            continue;
        }

        // Clip to screen bounds with margin
        if (screenX < 10.f || screenX > w - 10.f || screenY < 25.f || screenY > h - 10.f) {
            continue;
        }

        float dist = Distance(camPos, player.pos);

        // 1. Draw distinct Purple Dot / Indicator with dark shadow
        // Outer dark circle / shadow
        currentLineThickness = 2.6f;
        AddCircle(verts, screenX, screenY, 6.0f, w, h, 0.0f, 0.0f, 0.0f);
        // Inner vibrant purple ring
        AddCircle(verts, screenX, screenY, 4.5f, w, h, 0.72f, 0.22f, 1.0f);
        // Bright purple center dot
        AddRect(verts, screenX - 2.0f, screenY - 2.0f, 4.0f, 4.0f, w, h, 0.90f, 0.65f, 1.0f, 1.0f);

        // 2. Draw Text Label above the purple dot
        char text[128]{};
        sprintf_s(text, "%s [%.1f, %.1f, %.1f] (%.0fm)",
            player.isLocal ? "YOU" : "PLAYER",
            player.pos.x, player.pos.y, player.pos.z,
            dist
        );

        float textScale = 0.85f;
        float textW = SmoothFont::WidthOf(text, textScale);
        float boxX = screenX - textW * 0.5f - 4.0f;
        float boxY = screenY - 21.0f;
        float boxW = textW + 8.0f;
        float boxH = 15.0f;

        // Dark background plate for clear text readability
        AddRect(verts, boxX, boxY, boxW, boxH, w, h, 0.08f, 0.04f, 0.12f, 0.88f);
        // Purple top border line on the tag
        AddRect(verts, boxX, boxY, boxW, 1.5f, w, h, 0.75f, 0.30f, 1.0f, 0.95f);

        // Text label
        AddText(verts, screenX - textW * 0.5f, boxY + 2.0f, text, textScale, w, h,
            player.isLocal ? 0.95f : 0.85f,
            player.isLocal ? 0.88f : 0.75f,
            1.0f
        );
    }
}

}
