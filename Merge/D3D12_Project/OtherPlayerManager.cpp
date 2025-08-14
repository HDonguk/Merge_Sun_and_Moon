#include "stdafx.h"
#include "OtherPlayerManager.h"
#include "GameTimer.h"

OtherPlayerManager* OtherPlayerManager::instance = nullptr;

void OtherPlayerManager::SpawnOtherPlayer(int clientID) {
    // 스테이지 전환 중이면 플레이어 생성 차단
    if (m_isStageTransitioning) {
        OutputDebugString(L"[OtherPlayerManager] Stage transitioning, blocking spawn for player\n");
        return;
    }
    
    // 이미 존재하는 플레이어인지 확인
    if (otherPlayers.find(clientID) != otherPlayers.end()) {
        wchar_t debugMsg[256];
        swprintf_s(debugMsg, L"[OtherPlayerManager] Player already exists: %d, skipping spawn\n", clientID);
        OutputDebugString(debugMsg);
        return;
    }

    try {
        wchar_t debugMsg[256];
        swprintf_s(debugMsg, L"[OtherPlayerManager] Spawning new player: %d\n", clientID);
        OutputDebugString(debugMsg);
        
        // Scene이 없으면 생성 불가
        if (!m_currentScene) {
            OutputDebugString(L"[OtherPlayerManager] Scene is null, cannot spawn player\n");
            return;
        }
        
        // Scene이 유효한지 추가 검사 (null 포인터가 아닌지만 확인)
        if (m_currentScene == nullptr) {
            OutputDebugString(L"[OtherPlayerManager] Scene is null, cannot spawn player\n");
            return;
        }
        
        // 실제 PlayerObject 생성
        float scale = 0.1f;
        uint32_t newId = m_currentScene->AllocateId();
        if (newId == 0) {
            OutputDebugString(L"[OtherPlayerManager] Failed to allocate ID for new player\n");
            return;
        }
        
        PlayerObject* playerObj = new PlayerObject(m_currentScene, newId);
        if (!playerObj) {
            OutputDebugString(L"[OtherPlayerManager] Failed to create PlayerObject\n");
            return;
        }
        
        playerObj->SetIsNetworkPlayer(true);  // 네트워크 플레이어로 설정
        playerObj->SetLife(3);  // 네트워크 플레이어 생명력도 3으로 설정
        
        // 컴포넌트 추가 시 예외 처리
        try {
            playerObj->AddComponent(new Transform{ {0.0f, 0.0f, 0.0f} });
            playerObj->AddComponent(new AdjustTransform{ {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {scale, scale, scale} });
            playerObj->AddComponent(new Mesh{ "1P(boy-idle).fbx" });
            playerObj->AddComponent(new Texture{ L"boy" , 1.0f, 0.4f });
            playerObj->AddComponent(new Animation{ "1P(boy-idle).fbx" });
            playerObj->AddComponent(new Gravity);
            playerObj->AddComponent(new Collider{ {0.0f, 8.0f, 0.0f}, {2.0f, 8.0f, 2.0f} });
        }
        catch (const std::exception& e) {
            OutputDebugString(L"[OtherPlayerManager] Exception during component addition\n");
            delete playerObj;
            return;
        }
        catch (...) {
            OutputDebugString(L"[OtherPlayerManager] Unknown exception during component addition\n");
            delete playerObj;
            return;
        }
        
        // Scene에 추가
        try {
            m_currentScene->AddObj(playerObj);
        }
        catch (const std::exception& e) {
            OutputDebugString(L"[OtherPlayerManager] Exception during adding player to scene\n");
            delete playerObj;
            return;
        }
        catch (...) {
            OutputDebugString(L"[OtherPlayerManager] Unknown exception during adding player to scene\n");
            delete playerObj;
            return;
        }
        
        // 맵에 저장
        otherPlayers[clientID] = playerObj;
        
        swprintf_s(debugMsg, L"[OtherPlayerManager] Successfully created and registered player %d\n", clientID);
        OutputDebugString(debugMsg);
    }
    catch (const std::exception& e) {
        OutputDebugString(L"[OtherPlayerManager] Exception during spawn\n");
    }
    catch (...) {
        OutputDebugString(L"[OtherPlayerManager] Unknown exception during spawn\n");
    }
}

void OtherPlayerManager::UpdateOtherPlayer(int clientID, float x, float y, float z, float rotY, const std::string& animationFile, float animationTime, const std::string& stageName) {
    // 스테이지 전환 중이면 업데이트도 차단
    if (m_isStageTransitioning) {
        OutputDebugString(L"[OtherPlayerManager] Stage transitioning, blocking update for player\n");
        if (m_networkManager) {
            // LogToFile 제거 - 디버그 메시지로 대체
        }
        return;
    }
    
    // 현재 스테이지와 다른 스테이지에 있는 플레이어는 업데이트만 차단 (God Stage에서는 모든 플레이어 허용)
    if (m_currentStage != stageName && m_currentStage != "God") {
        OutputDebugString(L"[OtherPlayerManager] Player from different stage, skipping update\n");
        if (m_networkManager) {
            // LogToFile 제거 - 디버그 메시지로 대체
        }
        return;
    }
    
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
            
            // 회전값 안전성 검증: 로컬 플레이어의 회전값과 섞이지 않도록
            // rotY가 유효한 범위(-180 ~ 180도) 내에 있는지 확인
            if (rotY >= -180.0f && rotY <= 180.0f) {
                transform->SetRotation({0.0f, rotY, 0.0f});
            } else {
                // 유효하지 않은 회전값인 경우 현재 회전값 유지
                XMVECTOR currentRot = transform->GetRotation();
                float currentRotY = XMVectorGetY(currentRot);
                transform->SetRotation({0.0f, currentRotY, 0.0f});
                
                if (m_networkManager) {
                    // LogToFile 제거 - 디버그 메시지로 대체
                }
            }
            
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
                // LogToFile 제거 - 디버그 메시지로 대체
            }
        } else {
            if (m_networkManager) {
                // LogToFile 제거 - 디버그 메시지로 대체
            }
        }
    } else {
        if (m_networkManager) {
            // LogToFile 제거 - 디버그 메시지로 대체
        }
    }
}

void OtherPlayerManager::RemoveOtherPlayer(int clientID) {
    auto it = otherPlayers.find(clientID);
    if (it != otherPlayers.end()) {
        if (m_networkManager) {
            // LogToFile 제거 - 디버그 메시지로 대체
        }
        
        // 실제 PlayerObject 제거
        PlayerObject* playerObj = it->second;
        if (playerObj) {
            // Scene에서 제거
            if (m_currentScene) {
                playerObj->Delete();
            }
            
            if (m_networkManager) {
                // LogToFile 제거 - 디버그 메시지로 대체
            }
        }
        
        // 맵에서 제거
        otherPlayers.erase(it);
        
        if (m_networkManager) {
            // LogToFile 제거 - 디버그 메시지로 대체
        }
    }
}

void OtherPlayerManager::ClearAllPlayers() {
    OutputDebugString(L"[OtherPlayerManager] Clearing all other players\n");
    
    if (m_networkManager) {
        // LogToFile 제거 - 디버그 메시지로 대체
    }
    
    // 모든 플레이어 객체 삭제
    for (auto& pair : otherPlayers) {
        PlayerObject* playerObj = pair.second;
        if (playerObj) {
            // Scene에서 제거
            if (m_currentScene) {
                playerObj->Delete();
            }
            
                    // LogToFile 제거 - 디버그 메시지로 대체
        }
    }
    
    // 맵 비우기
    otherPlayers.clear();
    
    OutputDebugString(L"[OtherPlayerManager] All other players cleared\n");
    
    // LogToFile 제거 - 디버그 메시지로 대체
}

void OtherPlayerManager::SetStageTransitioning(bool transitioning) {
    m_isStageTransitioning = transitioning;
    OutputDebugString(transitioning ? L"[OtherPlayerManager] Stage transition started\n" : L"[OtherPlayerManager] Stage transition ended\n");
    
    // LogToFile 제거 - 디버그 메시지로 대체
} 