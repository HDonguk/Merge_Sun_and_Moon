#include "stdafx.h"
#include "OtherPlayerManager.h"
#include "GameTimer.h"

OtherPlayerManager* OtherPlayerManager::instance = nullptr;

void OtherPlayerManager::SpawnOtherPlayer(int clientID) {
    if (otherPlayers.find(clientID) != otherPlayers.end()) {
        if (m_networkManager) {
            m_networkManager->LogToFile("[OtherPlayerManager] Player already exists: " + std::to_string(clientID));
        }
        return;
    }

    try {
        if (m_networkManager) {
            m_networkManager->LogToFile("[OtherPlayerManager] Spawning new player: " + std::to_string(clientID));
        }
        
        // Scene이 없으면 생성 불가
        if (!m_currentScene) {
            if (m_networkManager) {
                m_networkManager->LogToFile("[OtherPlayerManager] Scene is null, cannot spawn player");
            }
            return;
        }
        
        // 실제 PlayerObject 생성
        float scale = 0.1f;
        PlayerObject* playerObj = new PlayerObject(m_currentScene, m_currentScene->AllocateId());
        playerObj->SetIsNetworkPlayer(true);  // 네트워크 플레이어로 설정
        playerObj->AddComponent(new Transform{ {0.0f, 0.0f, 0.0f} });
        playerObj->AddComponent(new AdjustTransform{ {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {scale, scale, scale} });
        playerObj->AddComponent(new Mesh{ "1P(boy-idle).fbx" });
        playerObj->AddComponent(new Texture{ L"boy" , 1.0f, 0.4f });
        playerObj->AddComponent(new Animation{ "1P(boy-idle).fbx" });
        playerObj->AddComponent(new Gravity);
        playerObj->AddComponent(new Collider{ {0.0f, 8.0f, 0.0f}, {2.0f, 8.0f, 2.0f} });
        
        // Scene에 추가
        m_currentScene->AddObj(playerObj);
        
        // 맵에 저장
        otherPlayers[clientID] = playerObj;
        
        if (m_networkManager) {
            m_networkManager->LogToFile("[OtherPlayerManager] Successfully created and registered player " + std::to_string(clientID));
        }
    }
    catch (const std::exception& e) {
        if (m_networkManager) {
            m_networkManager->LogToFile("[OtherPlayerManager] Exception during spawn: " + std::string(e.what()));
        }
    }
    catch (...) {
        if (m_networkManager) {
            m_networkManager->LogToFile("[OtherPlayerManager] Unknown exception during spawn");
        }
    }
}

void OtherPlayerManager::UpdateOtherPlayer(int clientID, float x, float y, float z, float rotY, const std::string& animationFile, float animationTime) {
    auto it = otherPlayers.find(clientID);
    if (it == otherPlayers.end()) {
        // 플레이어가 없으면 새로 생성
        SpawnOtherPlayer(clientID);
        it = otherPlayers.find(clientID);
        if (it == otherPlayers.end()) {
            return; // 생성 실패
        }
    }

    // 실제 PlayerObject 업데이트
    PlayerObject* playerObj = it->second;
    if (playerObj) {
        auto* transform = playerObj->GetComponent<Transform>();
        if (transform) {
            // 위치 업데이트
            transform->SetPosition({x, y, z, 1.0f});
            transform->SetRotation({0.0f, rotY, 0.0f});
            
            // 애니메이션 업데이트
            Animation* anim = playerObj->GetComponent<Animation>();
            if (anim && !animationFile.empty()) {
                // 애니메이션 파일이 변경되었으면 리셋
                if (anim->mCurrentFileName != animationFile) {
                    anim->ResetAnim(animationFile, animationTime);
                } else {
                    // 같은 애니메이션이면 시간만 업데이트
                    anim->mAnimationTime = animationTime;
                }
            }
            
            if (m_networkManager) {
                m_networkManager->LogToFile("[OtherPlayerManager] Updated player " + std::to_string(clientID) + 
                    " to position (" + std::to_string(x) + ", " + std::to_string(y) + ", " + std::to_string(z) + ")");
            }
        } else {
            if (m_networkManager) {
                m_networkManager->LogToFile("[OtherPlayerManager] Transform component not found for player " + std::to_string(clientID));
            }
        }
    } else {
        if (m_networkManager) {
            m_networkManager->LogToFile("[OtherPlayerManager] Player object is null for ID " + std::to_string(clientID));
        }
    }
}

void OtherPlayerManager::RemoveOtherPlayer(int clientID) {
    auto it = otherPlayers.find(clientID);
    if (it != otherPlayers.end()) {
        if (m_networkManager) {
            m_networkManager->LogToFile("[OtherPlayerManager] Remove request for player: " + std::to_string(clientID));
        }
        
        // 실제 PlayerObject 제거
        PlayerObject* playerObj = it->second;
        if (playerObj) {
            // Scene에서 제거
            if (m_currentScene) {
                playerObj->Delete();
            }
            
            if (m_networkManager) {
                m_networkManager->LogToFile("[OtherPlayerManager] Deleted player object: " + std::to_string(clientID));
            }
        }
        
        // 맵에서 제거
        otherPlayers.erase(it);
        
        if (m_networkManager) {
            m_networkManager->LogToFile("[OtherPlayerManager] Removed player: " + std::to_string(clientID));
        }
    }
} 