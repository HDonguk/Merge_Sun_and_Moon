#pragma once
#include <unordered_map>
#include "Object.h"
#include "Scene.h"
#include "ResourceManager.h"
#include "GameTimer.h"
#include "NetworkManager.h"
#include <mutex>

class OtherPlayerManager {
private:
    static OtherPlayerManager* instance;
    Scene* m_currentScene{nullptr};
    NetworkManager* m_networkManager{nullptr};
    std::unordered_map<int, PlayerObject*> otherPlayers;
    std::mutex m_mutex;  // 스레드 안전성을 위한 뮤텍스 추가
    bool m_isStageTransitioning{false};  // 스테이지 전환 중 플래그

    OtherPlayerManager() {}

public:
    static OtherPlayerManager* GetInstance() {
        if (instance == nullptr) {
            instance = new OtherPlayerManager();
        }
        return instance;
    }

    void SetScene(Scene* scene) { m_currentScene = scene; }
    void SetNetworkManager(NetworkManager* networkManager) { m_networkManager = networkManager; }

    void SpawnOtherPlayer(int clientID);
    void UpdateOtherPlayer(int clientID, float x, float y, float z, float rotY, const std::string& animationFile = "", float animationTime = 0.0f);
    void RemoveOtherPlayer(int clientID);
    void ClearAllPlayers(); // 모든 다른 플레이어 제거
    void SetStageTransitioning(bool transitioning); // 스테이지 전환 상태 설정

    std::unordered_map<int, PlayerObject*>& GetPlayers() {
        return otherPlayers;
    }
}; 