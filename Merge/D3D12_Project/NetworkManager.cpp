#include "stdafx.h"
#include "NetworkManager.h"
#include "OtherPlayerManager.h"
#include <iostream>
#include "ResourceManager.h"
#include <ctime>
#include <cstdio>
#include <mutex>

std::ofstream NetworkManager::m_logFile;
std::mutex NetworkManager::m_logMutex;

NetworkManager::NetworkManager() : sock(INVALID_SOCKET), m_networkThread(NULL), m_isRunning(false), m_myClientID(0) {
    
    // 에러 정보 초기화
    m_errorCount = 0;
    m_lastErrorTime = 0;
    m_shouldReconnect = false;
    m_reconnectAttempts = 0;
    m_lastReconnectTime = 0;
    
    try {
        CreateDirectory(L"logs", NULL);
        
        time_t now = time(0);
        tm ltm;
        localtime_s(&ltm, &now);
        char filename[100];
        sprintf_s(filename, "logs/network_log_%d%02d%02d_%02d%02d%02d.txt",
            ltm.tm_year + 1900, ltm.tm_mon + 1, ltm.tm_mday,
            ltm.tm_hour, ltm.tm_min, ltm.tm_sec);
        
        m_logFile.open(filename);
        if (!m_logFile.is_open()) {
            std::cerr << "Failed to open log file: " << filename << std::endl;
        }
    }
    catch (const std::exception& e) {
        std::cerr << "Failed to initialize NetworkManager: " << e.what() << std::endl;
    }
    catch (...) {
        std::cerr << "Unknown error in NetworkManager initialization" << std::endl;
    }
    
}

NetworkManager::~NetworkManager() {
    Shutdown();
}

bool NetworkManager::Initialize(const char* serverIP, int port, Scene* scene) {
            if (!scene) {
            // LogToFile 제거 - 디버그 메시지로 대체
            return false;
        }
    m_scene = scene;
    
    // PlayerObject가 생성될 때까지 대기 (최대 10초)
    int retryCount = 0;
    const int MAX_RETRIES = 100; // 100번 시도 (10초)
    
    while (retryCount < MAX_RETRIES) {
        try {
            auto* player = m_scene->GetLocalPlayer();
            if (player) {
                // LogToFile 제거 - 디버그 메시지로 대체
                break;
            } else {
                // LogToFile 제거 - 디버그 메시지로 대체
                Sleep(100); // 100ms 대기
                retryCount++;
            }
        }
        catch (const std::exception& e) {
            // LogToFile 제거 - 디버그 메시지로 대체
            Sleep(100);
            retryCount++;
        }
    }
    
    if (retryCount >= MAX_RETRIES) {
        // LogToFile 제거 - 디버그 메시지로 대체
        return false;
    }
    
    // OtherPlayerManager 초기화
    OtherPlayerManager::GetInstance()->SetScene(scene);
    OtherPlayerManager::GetInstance()->SetNetworkManager(this);
    
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        // LogToFile 제거 - 디버그 메시지로 대체
        return false;
    }

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) {
        // LogToFile 제거 - 디버그 메시지로 대체
        return false;
    }

    SOCKADDR_IN serverAddr = { 0 };
    serverAddr.sin_family = AF_INET;
    inet_pton(AF_INET, serverIP, &serverAddr.sin_addr);
    serverAddr.sin_port = htons(port);

    if (connect(sock, (SOCKADDR*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        // LogToFile 제거 - 디버그 메시지로 대체
        return false;
    }

    // recv 버퍼 초기화
    memset(m_recvBuffer, 0, sizeof(m_recvBuffer));
    memset(m_packetBuffer, 0, sizeof(m_packetBuffer));
    m_packetBufferSize = 0;

    m_isRunning = true;
    m_networkThread = CreateThread(NULL, 0, NetworkThread, this, 0, NULL);
    if (m_networkThread == NULL) {
        std::cout << "[Error] Failed to create network thread" << std::endl;
        return false;
    }

        // LogToFile 제거 - 디버그 메시지로 대체
    
    return true;
}

void NetworkManager::SendLoginRequest(const std::string& username) {
    if (!m_isRunning) return;

    m_username = username;
    
    try {
        PacketLoginRequest pkt;
        pkt.header.size = sizeof(PacketLoginRequest);
        pkt.header.type = PACKET_LOGIN_REQUEST;
        strncpy_s(pkt.username, username.c_str(), sizeof(pkt.username) - 1);

        int sendResult = send(sock, (char*)&pkt, sizeof(pkt), 0);
        if (sendResult == SOCKET_ERROR) {
            int error = WSAGetLastError();
            HandleError("Login request failed: " + std::to_string(error));
        } else {
            // LogToFile 제거 - 디버그 메시지로 대체
        }
    }
    catch (const std::exception& e) {
        HandleError("Exception in SendLoginRequest: " + std::string(e.what()));
    }
}

void NetworkManager::SendPlayerDisconnect() {
    if (!m_isRunning || !m_isLoggedIn) return;

    try {
        PacketPlayerDisconnect pkt;
        pkt.header.size = sizeof(PacketPlayerDisconnect);
        pkt.header.type = PACKET_PLAYER_DISCONNECT;
        pkt.playerID = m_myClientID;
        strncpy_s(pkt.username, m_username.c_str(), sizeof(pkt.username) - 1);

        send(sock, (char*)&pkt, sizeof(pkt), 0);
        // LogToFile 제거 - 디버그 메시지로 대체
    }
    catch (const std::exception& e) {
        // LogToFile 제거 - 디버그 메시지로 대체
    }
}

void NetworkManager::SendTigerRespawnRequest() {
    if (!m_isRunning || !m_isLoggedIn) return;

    try {
        PacketTigerRespawnRequest pkt;
        pkt.header.size = sizeof(PacketTigerRespawnRequest);
        pkt.header.type = PACKET_TIGER_RESPAWN_REQUEST;
        pkt.clientID = m_myClientID;

        int sendResult = send(sock, (char*)&pkt, sizeof(pkt), 0);
        if (sendResult == SOCKET_ERROR) {
            int error = WSAGetLastError();
            // LogToFile 제거 - 디버그 메시지로 대체
        } else {
            // LogToFile 제거 - 디버그 메시지로 대체
        }
    }
    catch (const std::exception& e) {
        // LogToFile 제거 - 디버그 메시지로 대체
    }
}

void NetworkManager::SendTigerHit(int tigerID, int life, int damage) {
    if (!m_isRunning || !m_isLoggedIn) return;

    try {
        PacketTigerHit pkt;
        pkt.header.size = sizeof(PacketTigerHit);
        pkt.header.type = PACKET_TIGER_HIT;
        pkt.tigerID = tigerID;
        pkt.life = life;
        pkt.damage = damage;

        int sendResult = send(sock, (char*)&pkt, sizeof(pkt), 0);
        if (sendResult == SOCKET_ERROR) {
            int error = WSAGetLastError();
            // LogToFile 제거 - 디버그 메시지로 대체
        } else {
            // LogToFile 제거 - 디버그 메시지로 대체
        }
    }
    catch (const std::exception& e) {
        // LogToFile 제거 - 디버그 메시지로 대체
    }
}

void NetworkManager::SendTigerAttack(int tigerID) {
    if (!m_isRunning || !m_isLoggedIn) return;

    try {
        PacketTigerAttack pkt;
        pkt.header.size = sizeof(PacketTigerAttack);
        pkt.header.type = PACKET_TIGER_ATTACK;
        pkt.tigerID = tigerID;

        int sendResult = send(sock, (char*)&pkt, sizeof(pkt), 0);
        if (sendResult == SOCKET_ERROR) {
            int error = WSAGetLastError();
            // LogToFile 제거 - 디버그 메시지로 대체
        } else {
            // LogToFile 제거 - 디버그 메시지로 대체
        }
    }
    catch (const std::exception& e) {
        // LogToFile 제거 - 디버그 메시지로 대체
    }
}

void NetworkManager::SendStageChange(const std::wstring& stageName) {
    if (!m_isRunning || !m_isLoggedIn) return;

    try {
        PacketStageChange pkt;
        pkt.header.size = sizeof(PacketStageChange);
        pkt.header.type = PACKET_STAGE_CHANGE;
        pkt.clientID = m_myClientID;
        
        // wstring을 char 배열로 변환
        std::string stageNameStr = std::string(stageName.begin(), stageName.end());
        strncpy_s(pkt.stageName, sizeof(pkt.stageName), stageNameStr.c_str(), sizeof(pkt.stageName) - 1);
        pkt.stageName[sizeof(pkt.stageName) - 1] = '\0';  // null 종료 보장

        int sendResult = send(sock, (char*)&pkt, sizeof(pkt), 0);
        if (sendResult == SOCKET_ERROR) {
            int error = WSAGetLastError();
            // LogToFile 제거 - 디버그 메시지로 대체
        } else {
            // LogToFile 제거 - 디버그 메시지로 대체
        }
    }
    catch (const std::exception& e) {
        // LogToFile 제거 - 디버그 메시지로 대체
    }
}

void NetworkManager::SendPuzzleUpdate(int puzzleStatus[3][3]) {
    if (!m_isRunning || !m_isLoggedIn) return;

    try {
        PacketPuzzleUpdate pkt;
        pkt.header.size = sizeof(PacketPuzzleUpdate);
        pkt.header.type = PACKET_PUZZLE_UPDATE;
        pkt.clientID = m_myClientID;
        
        // 퍼즐 상태 복사
        for (int i = 0; i < 3; ++i) {
            for (int j = 0; j < 3; ++j) {
                pkt.puzzleStatus[i][j] = puzzleStatus[i][j];
            }
        }

        int sendResult = send(sock, (char*)&pkt, sizeof(pkt), 0);
        if (sendResult == SOCKET_ERROR) {
            int error = WSAGetLastError();
            // LogToFile 제거 - 디버그 메시지로 대체
        } else {
            // LogToFile 제거 - 디버그 메시지로 대체
        }
    }
    catch (const std::exception& e) {
        // LogToFile 제거 - 디버그 메시지로 대체
    }
}

void NetworkManager::SetLoginSuccessCallback(std::function<void(int, const std::string&)> callback) {
    m_loginSuccessCallback = callback;
}

void NetworkManager::SetLoginFailedCallback(std::function<void(const std::string&)> callback) {
    m_loginFailedCallback = callback;
}

DWORD WINAPI NetworkManager::NetworkThread(LPVOID arg) {
    NetworkManager* network = (NetworkManager*)arg;
    // LogToFile 제거 - 디버그 메시지로 대체
    
    int errorCount = 0;
    const int MAX_ERRORS = 10;  // 최대 연속 에러 허용 횟수 증가 (더 관대하게)
    const int ERROR_RESET_TIME = 10000;  // 에러 카운트 리셋 시간 (ms) - 더 길게
    DWORD lastErrorTime = GetTickCount();
    
    while (network->m_isRunning) {
        try {
            fd_set readSet;
            FD_ZERO(&readSet);
            FD_SET(network->sock, &readSet);
            
            timeval timeout;
            timeout.tv_sec = 0;
            timeout.tv_usec = 50000;  // 50ms (더 짧은 타임아웃)
            
            int selectResult = select(0, &readSet, nullptr, nullptr, &timeout);
            if (selectResult == SOCKET_ERROR) {
                int error = WSAGetLastError();
                if (error == WSAEWOULDBLOCK) {
                    continue;  // 일시적 에러, 무시
                }
                // LogToFile 제거 - 디버그 메시지로 대체
                if (++errorCount >= MAX_ERRORS) {
                    // LogToFile 제거 - 디버그 메시지로 대체
                    // 에러가 많아도 연결 유지
                    continue;
                }
                continue;
            }
            
            if (selectResult == 0) continue;  // 타임아웃

            int recvBytes = recv(network->sock, network->m_recvBuffer, sizeof(network->m_recvBuffer), 0);
            if (recvBytes <= 0) {
                int error = WSAGetLastError();
                if (error == WSAEWOULDBLOCK) {
                    continue;  // 일시적 에러, 무시
                }
                if (error == WSAECONNRESET || error == WSAECONNABORTED || error == WSAENOTSOCK) {
                    // 실제 연결 문제가 있는 경우에만 로그 출력
                    // LogToFile 제거 - 디버그 메시지로 대체
                    // 연결 문제가 있어도 계속 시도 (재연결 로직으로 처리)
                    continue;
                }
                // LogToFile 제거 - 디버그 메시지로 대체
                if (++errorCount >= MAX_ERRORS) {
                    // LogToFile 제거 - 디버그 메시지로 대체
                    // 에러가 많아도 연결 유지
                    continue;
                }
                continue;
            }
            
            // 수신된 데이터 로그 (디버깅용) - 빈도 감소
            static int packetCount = 0;
            if (++packetCount % 50 == 0) { // 50개 패킷마다 로그 (로그 부하 감소)
                // LogToFile 제거 - 디버그 메시지로 대체
            }

            if (recvBytes > sizeof(network->m_recvBuffer)) {
                // LogToFile 제거 - 디버그 메시지로 대체
                return 1;
            }

            // 수신된 데이터를 패킷 버퍼에 추가
            if (network->m_packetBufferSize + recvBytes > sizeof(network->m_packetBuffer)) {
                // LogToFile 제거 - 디버그 메시지로 대체
                network->m_packetBufferSize = 0;
                memset(network->m_packetBuffer, 0, sizeof(network->m_packetBuffer));
                continue;
            }

            memcpy(network->m_packetBuffer + network->m_packetBufferSize, network->m_recvBuffer, recvBytes);
            network->m_packetBufferSize += recvBytes;

            // 패킷 버퍼에서 완전한 패킷들을 처리
            int processedBytes = 0;
            while (network->m_packetBufferSize - processedBytes >= sizeof(PacketHeader)) {
                PacketHeader* header = (PacketHeader*)(network->m_packetBuffer + processedBytes);
                
                // 패킷 크기 검증
                if (header->size < sizeof(PacketHeader) || header->size > sizeof(network->m_packetBuffer)) {
                    // LogToFile 제거 - 디버그 메시지로 대체
                    network->m_packetBufferSize = 0;
                    memset(network->m_packetBuffer, 0, sizeof(network->m_packetBuffer));
                    if (++errorCount >= MAX_ERRORS) {
                        // LogToFile 제거 - 디버그 메시지로 대체
                        // 에러가 많아도 연결 유지
                        continue;
                    }
                    break;
                }

                // 완전한 패킷이 있는지 확인
                if (network->m_packetBufferSize - processedBytes < header->size) {
                    break;  // 완전한 패킷이 없음
                }

                // 패킷 처리 (안전한 접근)
                try {
                    network->ProcessPacket(network->m_packetBuffer + processedBytes);
                } catch (const std::exception& e) {
                    // 패킷 처리 중 예외 발생 시 무시하고 계속 진행
                    // LogToFile 제거 - 디버그 메시지로 대체
                } catch (...) {
                    // 패킷 처리 중 예외 발생 시 무시하고 계속 진행
                    // LogToFile 제거 - 디버그 메시지로 대체
                }
                processedBytes += header->size;
            }

            // 처리된 데이터를 버퍼에서 제거
            if (processedBytes > 0) {
                if (processedBytes < network->m_packetBufferSize) {
                    memmove(network->m_packetBuffer, network->m_packetBuffer + processedBytes, 
                           network->m_packetBufferSize - processedBytes);
                }
                network->m_packetBufferSize -= processedBytes;
            }

            // 성공적인 패킷 수신 시 에러 카운트 리셋
            errorCount = 0;
            lastErrorTime = GetTickCount();
        }
        catch (const std::exception& e) {
            // LogToFile 제거 - 디버그 메시지로 대체
            if (++errorCount >= MAX_ERRORS) {
                // LogToFile 제거 - 디버그 메시지로 대체
                // 에러가 많아도 연결 유지
                continue;
            }
            Sleep(1000);
        }
    }
    
    // LogToFile 제거 - 디버그 메시지로 대체
    return 0;
}

void NetworkManager::HandleError(const std::string& description) {
    DWORD currentTime = GetTickCount();
    
    m_errorCount++;
    m_lastErrorTime = currentTime;
    
    // LogToFile 제거 - 디버그 메시지로 대체
    
    // 5번 연속 에러 시 재연결 시도
    if (m_errorCount >= 5) {
        // LogToFile 제거 - 디버그 메시지로 대체
        m_shouldReconnect = true;
    }
}

bool NetworkManager::ShouldReconnect() const {
    return m_shouldReconnect && m_reconnectAttempts < 5;
}

bool NetworkManager::AttemptReconnect() {
    DWORD currentTime = GetTickCount();
    
    if (currentTime - m_lastReconnectTime < 5000) {
        return false;
    }
    
    m_lastReconnectTime = currentTime;
    m_reconnectAttempts++;
    
    // LogToFile 제거 - 디버그 메시지로 대체
    
    // 기존 소켓 정리
    if (sock != INVALID_SOCKET) {
        closesocket(sock);
        sock = INVALID_SOCKET;
    }
    
    // WSA 정리 후 재시작
    WSACleanup();
    
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        // LogToFile 제거 - 디버그 메시지로 대체
        return false;
    }

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) {
        // LogToFile 제거 - 디버그 메시지로 대체
        return false;
    }

    // 소켓 옵션 설정 (더 안정적인 연결을 위해)
    int optval = 1;
    setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, (char*)&optval, sizeof(optval));
    
    // 연결 시도
    SOCKADDR_IN serverAddr = { 0 };
    serverAddr.sin_family = AF_INET;
    inet_pton(AF_INET, "127.0.0.1", &serverAddr.sin_addr);
    serverAddr.sin_port = htons(5000);

        if (connect(sock, (SOCKADDR*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        int error = WSAGetLastError();
        // LogToFile 제거 - 디버그 메시지로 대체
        return false;
    }
    
    // LogToFile 제거 - 디버그 메시지로 대체
    
    // 재연결 성공 시 상태 초기화
    m_shouldReconnect = false;
    m_reconnectAttempts = 0;
    ResetErrorInfo();
    
    // 로그인 상태 복구 시도 (스테이지 변경 중이 아닐 때만)
    if (!m_username.empty() && !m_isStageChanging) {
        // LogToFile 제거 - 디버그 메시지로 대체
        SendLoginRequest(m_username);
    } else if (m_isStageChanging) {
        // LogToFile 제거 - 디버그 메시지로 대체
    }
    
    return true;
}

void NetworkManager::ResetErrorInfo() {
    m_errorCount = 0;
    m_packetBufferSize = 0;
    memset(m_packetBuffer, 0, sizeof(m_packetBuffer));
    // LogToFile 제거 - 디버그 메시지로 대체
}

void NetworkManager::SendPlayerUpdate(float x, float y, float z, float rotY) {
    if (!m_isRunning || !m_isLoggedIn) return;

    try {
        // 회전값 안전성 검증: 로컬 플레이어의 회전값이 올바른 범위 내에 있는지 확인
        float safeRotY = rotY;
        if (rotY < -180.0f || rotY > 180.0f) {
            // 유효하지 않은 회전값인 경우 정규화
            while (safeRotY < -180.0f) safeRotY += 360.0f;
            while (safeRotY > 180.0f) safeRotY -= 360.0f;
            
            // LogToFile 제거 - 디버그 메시지로 대체
        }
        
        PacketPlayerUpdate pkt;
        pkt.header.size = sizeof(PacketPlayerUpdate);
        pkt.header.type = PACKET_PLAYER_UPDATE;
        pkt.clientID = m_myClientID;
        pkt.x = x;
        pkt.y = y;
        pkt.z = z;
        pkt.rotY = safeRotY;
        
        // 애니메이션 정보 추가
        if (m_scene) {
            auto* player = m_scene->GetLocalPlayer();
            if (player) {
                Animation* anim = player->GetComponent<Animation>();
                if (anim) {
                    strncpy_s(pkt.animationFile, anim->mCurrentFileName.c_str(), sizeof(pkt.animationFile) - 1);
                    pkt.animationTime = anim->mAnimationTime;
                } else {
                    strncpy_s(pkt.animationFile, "1P(boy-idle).fbx", sizeof(pkt.animationFile) - 1);
                    pkt.animationTime = 0.0f;
                }
            }
        }

        int sendResult = send(sock, (char*)&pkt, sizeof(pkt), 0);
        if (sendResult == SOCKET_ERROR) {
            int error = WSAGetLastError();
            if (error == WSAECONNRESET || error == WSAECONNABORTED) {
                HandleError("Connection lost during send: " + std::to_string(error));
            } else if (error == WSAEWOULDBLOCK) {
                // 일시적 에러, 무시하고 계속 진행
                // LogToFile 제거 - 디버그 메시지로 대체
            } else {
                // 기타 에러는 로그만 남기고 계속 진행
                // LogToFile 제거 - 디버그 메시지로 대체
            }
        } else {
            // Player 테스트를 위해 위치 업데이트 로그 추가
            static float lastX = 0.0f, lastY = 0.0f, lastZ = 0.0f;
            if (abs(x - lastX) > 1.0f || abs(y - lastY) > 1.0f || abs(z - lastZ) > 1.0f) {
                // LogToFile 제거 - 디버그 메시지로 대체
                lastX = x; lastY = y; lastZ = z;
            }
        }
    }
    catch (const std::exception& e) {
        HandleError("Exception in SendPlayerUpdate: " + std::string(e.what()));
    }
}

void NetworkManager::ProcessPacket(char* buffer) {
    PacketHeader* header = (PacketHeader*)buffer;
    // 패킷 처리 시작 로그 제거 (로그 출력 최소화)

    // 메모리 사용량 모니터링 (최적화)
    static int packetCount = 0;
    packetCount++;
    if (packetCount % 50 == 0) {  // 50개 패킷마다 메모리 상태 체크 (빈도 감소)
        try {
            MEMORYSTATUSEX memInfo;
            memInfo.dwLength = sizeof(MEMORYSTATUSEX);
            if (GlobalMemoryStatusEx(&memInfo)) {
                DWORDLONG usedMemory = memInfo.ullTotalPhys - memInfo.ullAvailPhys;
                DWORDLONG totalMemory = memInfo.ullTotalPhys;
                if (totalMemory > 0) {
                    double memoryUsagePercent = (double)usedMemory / totalMemory * 100.0;
                    
                    // 85% 초과 시에만 경고
                    if (memoryUsagePercent > 85.0) {
                        // LogToFile 제거 - 디버그 메시지로 대체
                    }
                }
            }
        } catch (...) {
            // 메모리 상태 확인 실패 시 무시
        }
    }

    try {
        switch (header->type) {
            case PACKET_LOGIN_RESPONSE: {
                PacketLoginResponse* loginRespPkt = (PacketLoginResponse*)buffer;
                // LogToFile 제거 - 디버그 메시지로 대체
                
                if (loginRespPkt->success) {
                    m_myClientID = loginRespPkt->clientID;
                    m_isLoggedIn = true;
                    // LogToFile 제거 - 디버그 메시지로 대체
                    
                    // 로그인 성공 후 준비 완료 신호 전송
                    PacketClientReady readyPacket;
                    readyPacket.header.type = PACKET_CLIENT_READY;
                    readyPacket.header.size = sizeof(PacketClientReady);
                    readyPacket.clientID = m_myClientID;
                    
                    int sendResult = send(sock, (char*)&readyPacket, sizeof(readyPacket), 0);
                    if (sendResult == SOCKET_ERROR) {
                        int error = WSAGetLastError();
                        // LogToFile 제거 - 디버그 메시지로 대체
                    } else {
                        // LogToFile 제거 - 디버그 메시지로 대체
                    }
                    
                    if (m_loginSuccessCallback) {
                        m_loginSuccessCallback(m_myClientID, m_username);
                    }
                } else {
                    m_isLoggedIn = false;
                    std::string errorMsg = loginRespPkt->message;
                    // LogToFile 제거 - 디버그 메시지로 대체
                    
                    if (m_loginFailedCallback) {
                        m_loginFailedCallback(errorMsg);
                    }
                }
                break;
            }
            
            case PACKET_PLAYER_SPAWN: {
                PacketPlayerSpawn* spawnPkt = (PacketPlayerSpawn*)buffer;
                // LogToFile 제거 - 디버그 메시지로 대체

                // 로그인 상태 확인
                if (!m_isLoggedIn) {
                    // LogToFile 제거 - 디버그 메시지로 대체
                    break;
                }

                if (m_myClientID == 0) {
                    m_myClientID = spawnPkt->playerID;
                    // LogToFile 제거 - 디버그 메시지로 대체
                }
                else if (spawnPkt->playerID != m_myClientID) {
                    try {
                        OtherPlayerManager::GetInstance()->SpawnOtherPlayer(spawnPkt->playerID);
                        // LogToFile 제거 - 디버그 메시지로 대체
                    }
                    catch (const std::exception& e) {
                        // LogToFile 제거 - 디버그 메시지로 대체
                    }
                }
                break;
            }
            
            case PACKET_PLAYER_DISCONNECT: {
                PacketPlayerDisconnect* disconnectPkt = (PacketPlayerDisconnect*)buffer;
                // LogToFile 제거 - 디버그 메시지로 대체
                
                if (disconnectPkt->playerID != m_myClientID) {
                    try {
                        OtherPlayerManager::GetInstance()->RemoveOtherPlayer(disconnectPkt->playerID);
                        // LogToFile 제거 - 디버그 메시지로 대체
                    }
                    catch (const std::exception& e) {
                        // LogToFile 제거 - 디버그 메시지로 대체
                    }
                }
                break;
            }

            case PACKET_PLAYER_UPDATE: {
                PacketPlayerUpdate* updatePkt = (PacketPlayerUpdate*)buffer;
                
                // 로그인 상태 확인
                if (!m_isLoggedIn) {
                    // LogToFile 제거 - 디버그 메시지로 대체
                    break;
                }
                
                if (updatePkt->clientID == m_myClientID) {
                    // LogToFile 제거 - 디버그 메시지로 대체
                    return;
                }

                // 애니메이션 정보 추출
                std::string animationFile = updatePkt->animationFile;
                float animationTime = updatePkt->animationTime;
                
                // 스테이지 정보 추출
                std::string stageName = updatePkt->stageName;

                OtherPlayerManager::GetInstance()->UpdateOtherPlayer(
                    updatePkt->clientID, updatePkt->x, updatePkt->y, updatePkt->z, updatePkt->rotY, 
                    animationFile, animationTime, stageName);
                // LogToFile 제거 - 디버그 메시지로 대체
                break;
            }

            case PACKET_TIGER_SPAWN: {
                PacketTigerSpawn* tigerSpawnPkt = (PacketTigerSpawn*)buffer;
                wchar_t debugMsg[256];
                swprintf_s(debugMsg, L"[Tiger] Received spawn packet for tiger ID: %d at position (%.1f, %.1f, %.1f)\n", 
                          tigerSpawnPkt->tigerID, tigerSpawnPkt->x, tigerSpawnPkt->y, tigerSpawnPkt->z);
                OutputDebugString(debugMsg);
                
                // 로그인 상태 확인 - 로그인 전에도 호랑이 정보는 저장해두기
                if (!m_isLoggedIn) {
                    OutputDebugString(L"[Tiger] Not logged in yet, but storing tiger info for later\n");
                }
                
                // 이미 스폰된 호랑이인지 확인
                if (m_tigers.find(tigerSpawnPkt->tigerID) != m_tigers.end()) {
                    swprintf_s(debugMsg, L"[Tiger] Tiger ID %d already spawned, ignoring duplicate spawn\n", tigerSpawnPkt->tigerID);
                    OutputDebugString(debugMsg);
                    break;
                }
                
                // Tiger 정보 저장
                TigerInfo tigerInfo;
                tigerInfo.tigerID = tigerSpawnPkt->tigerID;
                tigerInfo.x = tigerSpawnPkt->x;
                tigerInfo.y = tigerSpawnPkt->y;
                tigerInfo.z = tigerSpawnPkt->z;
                tigerInfo.rotY = 0.0f;
                m_tigers[tigerSpawnPkt->tigerID] = tigerInfo;
                
                swprintf_s(debugMsg, L"[Tiger] Successfully stored tiger info for ID: %d\n", tigerSpawnPkt->tigerID);
                OutputDebugString(debugMsg);
                
                // 호랑이 카운터 업데이트 및 자동 생성 체크
                static int expectedTigerCount = 16; // 총 16마리
                static int receivedTigerCount = 0;
                
                receivedTigerCount++;
                swprintf_s(debugMsg, L"[Tiger] Received %d/%d tigers\n", receivedTigerCount, expectedTigerCount);
                OutputDebugString(debugMsg);
                
                // 모든 호랑이를 받았고 현재 Hunting 스테이지에 있다면 자동 생성
                if (receivedTigerCount >= expectedTigerCount) {
                    if (m_scene && m_scene->GetCurrentStage() == L"Hunting") {
                        OutputDebugString(L"[Tiger] All tigers received, auto-creating in Hunting Stage\n");
                        CreateStoredTigers();
                    } else {
                        OutputDebugString(L"[Tiger] All tigers received but not in Hunting Stage, will create later\n");
                    }
                    receivedTigerCount = 0; // 리셋
                }
                break;
            }
            
            case PACKET_TIGER_UPDATE: {
                PacketTigerUpdate* tigerUpdatePkt = (PacketTigerUpdate*)buffer;
                
                // 로그인 상태 확인 - 로그인 전에는 무시
                if (!m_isLoggedIn) {
                    OutputDebugString(L"[Tiger] Ignoring tiger update packet - not logged in yet\n");
                    break;
                }
                
                // Scene의 Tiger 오브젝트 업데이트
                if (m_scene) {
                    // 모든 TigerObject를 순회하여 해당 ID의 호랑이를 찾아 업데이트
                    for (Object* obj : m_scene->GetObjects()) {
                        TigerObject* tigerObj = dynamic_cast<TigerObject*>(obj);
                        if (tigerObj && tigerObj->IsNetworkTiger() && tigerObj->GetNetworkTigerID() == tigerUpdatePkt->tigerID) {
                            // 새로운 메서드 사용으로 개선
                            tigerObj->SetNetworkTransform(tigerUpdatePkt->x, tigerUpdatePkt->y, tigerUpdatePkt->z, tigerUpdatePkt->rotY);
                            tigerObj->SetNetworkAnimation(tigerUpdatePkt->animationFile, tigerUpdatePkt->animationTime);
                            break;
                        }
                    }
                }
                break;
            }


            
            case PACKET_TIGER_ATTACK: {
                PacketTigerAttack* tigerAttackPkt = (PacketTigerAttack*)buffer;
                
                // 로그인 상태 확인 - 로그인 전에는 무시
                if (!m_isLoggedIn) {
                    OutputDebugString(L"[Tiger] Ignoring tiger attack packet - not logged in yet\n");
                    break;
                }
                // Scene에서 해당 호랑이를 찾아서 공격 신호만 설정 (공격 오브젝트는 0.4초 후 CalcTime에서 생성됨)
                if (m_scene) {
                    for (Object* obj : m_scene->GetObjects()) {
                        TigerObject* tigerObj = dynamic_cast<TigerObject*>(obj);
                        if (tigerObj && tigerObj->IsNetworkTiger() && tigerObj->GetNetworkTigerID() == tigerAttackPkt->tigerID) {
                            // 서버 공격 신호 설정 (공격 오브젝트는 0.4초 후 CalcTime에서 생성됨)
                            tigerObj->SetServerAttackSignal(true);
                            // LogToFile 제거 - 디버그 메시지로 대체
                            break;
                        }
                    }
                }
                break;
            }
            
            case PACKET_TIGER_HIT: {
                PacketTigerHit* tigerHitPkt = (PacketTigerHit*)buffer;
                
                // 로그인 상태 확인 - 로그인 전에는 무시
                if (!m_isLoggedIn) {
                    OutputDebugString(L"[Tiger] Ignoring tiger hit packet - not logged in yet\n");
                    break;
                }
                
                // Scene에서 해당 호랑이를 찾아서 Hit 상태 동기화
                if (m_scene) {
                    for (Object* obj : m_scene->GetObjects()) {
                        TigerObject* tigerObj = dynamic_cast<TigerObject*>(obj);
                        if (tigerObj && tigerObj->IsNetworkTiger() && tigerObj->GetNetworkTigerID() == tigerHitPkt->tigerID) {
                            // 서버에서 받은 생명력으로 업데이트 (항상 처리)
                            int currentLife = tigerObj->GetLife();
                            wchar_t debugMsg[256];
                            swprintf_s(debugMsg, L"[NetworkManager] Tiger ID %d life: %d -> %d\n", 
                                     tigerHitPkt->tigerID, currentLife, tigerHitPkt->life);
                            OutputDebugString(debugMsg);
                            
                            // 서버의 생명력을 정확히 반영
                            tigerObj->SetLife(tigerHitPkt->life);
                            
                            if (tigerHitPkt->life <= 0) {
                                // 호랑이 사망 - 즉시 Dead() 호출
                                if (!tigerObj->IsDead()) { // 이미 죽지 않은 경우에만
                                    tigerObj->Dead();
                                    OutputDebugString(L"[NetworkManager] Tiger died, calling Dead()\n");
                                    
                                    // Dead() 호출 후 애니메이션 상태 확인
                                    Animation* anim = tigerObj->GetComponent<Animation>();
                                    if (anim) {
                                        wchar_t debugMsg[256];
                                        swprintf_s(debugMsg, L"[NetworkManager] Tiger animation after Dead(): %s\n", 
                                                   anim->mCurrentFileName.c_str());
                                        OutputDebugString(debugMsg);
                                    }
                                } else {
                                    OutputDebugString(L"[NetworkManager] Tiger already dead, skipping Dead() call\n");
                                }
                            } else {
                                // 호랑이 피격 - hit 애니메이션 재생
                                tigerObj->ChangeState("0208_tiger_hit.fbx");
                                OutputDebugString(L"[NetworkManager] Tiger hit, playing hit animation\n");
                            }
                            break;
                        }
                    }
                }
                break;
            }
            
            case PACKET_PUZZLE_SYNC: {
                PacketPuzzleSync* puzzleSyncPkt = (PacketPuzzleSync*)buffer;
                
                if (!m_isLoggedIn) {
                    break;
                }
                
                if (m_scene) {
                    // Scene의 퍼즐 상태를 서버에서 받은 값으로 업데이트
                    int(*puzzleStatus)[3] = m_scene->GetPuzzleStatus();
                    for (int i = 0; i < 3; ++i) {
                        for (int j = 0; j < 3; ++j) {
                            puzzleStatus[i][j] = puzzleSyncPkt->puzzleStatus[i][j];
                        }
                    }
                    
                    // 목표 퍼즐 패턴도 서버에서 받은 값으로 업데이트
                    int(*targetPattern)[3] = m_scene->GetTargetPattern();
                    for (int i = 0; i < 3; ++i) {
                        for (int j = 0; j < 3; ++j) {
                            targetPattern[i][j] = puzzleSyncPkt->targetPattern[i][j];
                        }
                    }
                    
                    // 퍼즐 셀들의 시각적 표현을 업데이트
                    m_scene->UpdatePuzzleCellsFromStatus();
                    
                    // 목표 퍼즐 패턴도 UI에 업데이트
                    m_scene->UpdatePuzzleQuestTargetPattern();
                    
                    OutputDebugString(L"[NetworkManager] Received puzzle status and target pattern from server and updated local state\n");
                }
                break;
            }
            
            case PACKET_RICE_CAKE_SPAWN: {
                PacketRiceCakeSpawn* spawnPkt = (PacketRiceCakeSpawn*)buffer;
                
                if (!m_isLoggedIn || spawnPkt->clientID == m_myClientID) {
                    break;  // 자신이 발사한 떡은 처리하지 않음
                }
                
                if (m_scene) {
                    // 다른 플레이어가 발사한 떡 발사체 생성
                    m_scene->CreateOtherPlayerRiceCakeProjectile(
                        spawnPkt->projectileID,
                        spawnPkt->x, spawnPkt->y, spawnPkt->z,
                        spawnPkt->dirX, spawnPkt->dirY, spawnPkt->dirZ,
                        spawnPkt->speed
                    );
                }
                break;
            }
            
            case PACKET_RICE_CAKE_UPDATE: {
                PacketRiceCakeUpdate* updatePkt = (PacketRiceCakeUpdate*)buffer;
                
                if (!m_isLoggedIn || updatePkt->clientID == m_myClientID) {
                    break;  // 자신이 발사한 떡은 처리하지 않음
                }
                
                if (m_scene) {
                    // 다른 플레이어의 떡 발사체 위치 업데이트
                    m_scene->UpdateOtherPlayerRiceCakeProjectile(
                        updatePkt->projectileID,
                        updatePkt->x, updatePkt->y, updatePkt->z
                    );
                }
                break;
            }
            
            case PACKET_LEATHER_COUNT_SYNC: {
                PacketLeatherCountSync* syncPkt = (PacketLeatherCountSync*)buffer;
                
                if (m_scene) {
                    // 서버에서 전송된 호랑이 가죽 개수로 동기화
                    m_scene->SetLeatherCount(syncPkt->leatherCount);
                    
                    wchar_t debugMsg[256];
                    swprintf_s(debugMsg, L"[NetworkManager] Received leather count sync: %d\n", syncPkt->leatherCount);
                    OutputDebugString(debugMsg);
                }
                break;
            }

            default:
                // LogToFile 제거 - 디버그 메시지로 대체
                break;
        }
    }
    catch (const std::exception& e) {
        // LogToFile 제거 - 디버그 메시지로 대체
        // 예외를 상위로 전파하지 않고 여기서 처리
    }

    // 패킷 처리 완료 로그 제거
}

void NetworkManager::Shutdown() {
    m_isRunning = false;
    if (m_networkThread) {
        WaitForSingleObject(m_networkThread, INFINITE);
        CloseHandle(m_networkThread);
        m_networkThread = NULL;
    }
    if (sock != INVALID_SOCKET) {
        closesocket(sock);
        sock = INVALID_SOCKET;
    }
    WSACleanup();
}

void NetworkManager::LogToFile(const std::string& message) {
    // 로그 기능 다시 활성화 (디버깅을 위해)
    std::lock_guard<std::mutex> lock(m_logMutex);
    if (m_logFile.is_open()) {
        time_t now = time(0);
        tm ltm;
        localtime_s(&ltm, &now);
        
        char timeStr[20];
        sprintf_s(timeStr, "[%02d:%02d:%02d] ", ltm.tm_hour, ltm.tm_min, ltm.tm_sec);
        
        m_logFile << timeStr << message << std::endl;
        m_logFile.flush(); // 즉시 파일에 쓰기
    }
}



void NetworkManager::Update(GameTimer& gTimer, Scene* scene) {
    if (!m_scene) return;

    // 재연결 시도가 필요한 경우
    if (ShouldReconnect()) {
        if (AttemptReconnect()) {
            // LogToFile 제거 - 디버그 메시지로 대체
        } else {
            // LogToFile 제거 - 디버그 메시지로 대체
        }
        return;
    }

    if (!m_isRunning) return;

    // 로그 출력 최소화로 성능 개선
    // static int updateCount = 0;
    // updateCount++;
    // if (updateCount % 300 == 0) { // 5초마다 로그 (성능 개선)
    //     LogToFile("[Update] NetworkManager::Update called - count: " + std::to_string(updateCount));
    // }



    auto* player = scene->GetLocalPlayer();
    if (!player) {
        // LogToFile 제거 - 디버그 메시지로 대체
        return;
    }
    
    auto* transform = player->GetComponent<Transform>();
    if (!transform) {
        // LogToFile 제거 - 디버그 메시지로 대체
        return;
    }
    
    XMVECTOR pos = transform->GetPosition();
    XMVECTOR rot = transform->GetRotation();
    
    // 회전값 안전성 검증: 로컬 플레이어의 회전값이 올바른 범위 내에 있는지 확인
    float rotY = XMVectorGetY(rot);
    if (rotY < -180.0f || rotY > 180.0f) {
        // 유효하지 않은 회전값인 경우 정규화
        while (rotY < -180.0f) rotY += 360.0f;
        while (rotY > 180.0f) rotY -= 360.0f;
        
        // LogToFile 제거 - 디버그 메시지로 대체
    }
    
    m_updateTimer += gTimer.DeltaTime();
    const float UPDATE_INTERVAL = 0.004167f;  // 240fps (4.17ms)마다 업데이트

    if (m_updateTimer >= UPDATE_INTERVAL) {
        // 로그 제거로 성능 개선
        SendPlayerUpdate(
            XMVectorGetX(pos), 
            XMVectorGetY(pos), 
            XMVectorGetZ(pos),
            rotY
        );
        m_updateTimer = 0.0f;
    }
} 

void NetworkManager::ClearTigerInfo() {
    m_tigers.clear();
    OutputDebugString(L"[NetworkManager] Tiger info cleared\n");
}

void NetworkManager::SetStageTransitioning(bool transitioning) {
    m_isStageChanging = transitioning;
    // LogToFile 제거 - 디버그 메시지로 대체
}

// 떡 발사체 동기화 메서드들 구현
void NetworkManager::SendRiceCakeSpawn(int projectileID, float x, float y, float z, float dirX, float dirY, float dirZ, float speed) {
    if (!m_isRunning || !m_isLoggedIn) return;

    try {
        PacketRiceCakeSpawn pkt;
        pkt.header.size = sizeof(PacketRiceCakeSpawn);
        pkt.header.type = PACKET_RICE_CAKE_SPAWN;
        pkt.clientID = m_myClientID;
        pkt.projectileID = projectileID;
        pkt.x = x;
        pkt.y = y;
        pkt.z = z;
        pkt.dirX = dirX;
        pkt.dirY = dirY;
        pkt.dirZ = dirZ;
        pkt.speed = speed;

        int sendResult = send(sock, (char*)&pkt, sizeof(pkt), 0);
        if (sendResult == SOCKET_ERROR) {
            int error = WSAGetLastError();
            OutputDebugString(L"[NetworkManager] Failed to send rice cake spawn packet\n");
        } else {
            OutputDebugString(L"[NetworkManager] Rice cake spawn packet sent successfully\n");
        }
    }
    catch (const std::exception& e) {
        OutputDebugString(L"[NetworkManager] Exception in SendRiceCakeSpawn\n");
    }
}

void NetworkManager::SendRiceCakeUpdate(int projectileID, float x, float y, float z) {
    if (!m_isRunning || !m_isLoggedIn) return;

    try {
        PacketRiceCakeUpdate pkt;
        pkt.header.size = sizeof(PacketRiceCakeUpdate);
        pkt.header.type = PACKET_RICE_CAKE_UPDATE;
        pkt.clientID = m_myClientID;
        pkt.projectileID = projectileID;
        pkt.x = x;
        pkt.y = y;
        pkt.z = z;

        int sendResult = send(sock, (char*)&pkt, sizeof(pkt), 0);
        if (sendResult == SOCKET_ERROR) {
            int error = WSAGetLastError();
            OutputDebugString(L"[NetworkManager] Failed to send rice cake update packet\n");
        }
    }
    catch (const std::exception& e) {
        OutputDebugString(L"[NetworkManager] Exception in SendRiceCakeUpdate\n");
    }
}

void NetworkManager::CreateStoredTigers() {
    if (!m_scene) {
        OutputDebugString(L"[NetworkManager] Cannot create stored tigers - scene is null\n");
        return;
    }
    
    OutputDebugString(L"[NetworkManager] Creating stored tigers...\n");
    
    // 스테이지 체크는 호출하는 쪽에서 이미 처리됨
    // 여기서는 항상 호랑이 생성 진행
    
    // 저장된 호랑이 정보를 기반으로 호랑이 오브젝트 생성
    for (const auto& tigerPair : m_tigers) {
        const auto& tigerInfo = tigerPair.second;
        
        // 이미 생성된 호랑이가 있는지 확인 (더 정확한 체크)
        bool tigerExists = false;
        for (Object* obj : m_scene->GetObjects()) {
            TigerObject* existingTiger = dynamic_cast<TigerObject*>(obj);
            if (existingTiger && existingTiger->IsNetworkTiger() && 
                existingTiger->GetNetworkTigerID() == tigerInfo.tigerID) {
                tigerExists = true;
                wchar_t debugMsg[256];
                swprintf_s(debugMsg, L"[NetworkManager] Tiger ID %d already exists, skipping creation\n", tigerInfo.tigerID);
                OutputDebugString(debugMsg);
                break;
            }
        }
        
        if (tigerExists) {
            continue;
        }
        
        try {
            float scale = 0.2f;
            TigerObject* tigerObj = new TigerObject(m_scene, m_scene->AllocateId());
            tigerObj->SetIsNetworkTiger(true);
            tigerObj->AddComponent(new Transform{ {tigerInfo.x, tigerInfo.y, tigerInfo.z} });
            tigerObj->AddComponent(new AdjustTransform{ {0.0f, 0.0f, -40.0f * scale}, {0.0f, 180.0f, 0.0f}, {scale, scale, scale} });
            tigerObj->AddComponent(new Mesh{ "0113_tiger.fbx" });
            tigerObj->AddComponent(new Texture{ L"tigercolor", 1.0f, 0.4f });
            tigerObj->AddComponent(new Animation{ "0113_tiger_walk.fbx" });
            tigerObj->AddComponent(new Gravity);
            tigerObj->AddComponent(new Collider{ {0.0f, 6.0f, 0.0f}, {2.0f, 6.0f, 10.0f} });
            
            m_scene->AddObj(tigerObj);
            tigerObj->SetNetworkTigerID(tigerInfo.tigerID);
            
            // 초기 위치 설정
            Transform* transform = tigerObj->GetComponent<Transform>();
            if (transform) {
                transform->SetPosition({tigerInfo.x, tigerInfo.y, tigerInfo.z, 1.0f});
                transform->SetRotation({0.0f, 0.0f, 0.0f});
            }
            
            // 초기 애니메이션 설정 (idle 상태로 시작)
            Animation* anim = tigerObj->GetComponent<Animation>();
            if (anim) {
                anim->ResetAnim("0722_tiger_idle2.fbx", 0.0f);
            }
            
            // 초기 생명력 설정 (기본값 3)
            tigerObj->SetLife(3);
            
            wchar_t debugMsg[256];
            swprintf_s(debugMsg, L"[NetworkManager] Successfully created stored tiger ID: %d at position (%.1f, %.1f, %.1f)\n", 
                      tigerInfo.tigerID, tigerInfo.x, tigerInfo.y, tigerInfo.z);
            OutputDebugString(debugMsg);
        }
        catch (const std::exception& e) {
            OutputDebugString(L"[NetworkManager] Failed to create stored tiger object\n");
        }
    }
    
    OutputDebugString(L"[NetworkManager] Finished creating stored tigers\n");
}

void NetworkManager::SendLeatherCountUpdate(int leatherCount) {
    if (!m_isRunning || !m_isLoggedIn) return;

    try {
        PacketLeatherCountUpdate pkt;
        pkt.header.size = sizeof(PacketLeatherCountUpdate);
        pkt.header.type = PACKET_LEATHER_COUNT_UPDATE;
        pkt.clientID = m_myClientID;
        pkt.leatherCount = leatherCount;

        int sendResult = send(sock, (char*)&pkt, sizeof(pkt), 0);
        if (sendResult == SOCKET_ERROR) {
            int error = WSAGetLastError();
            OutputDebugString(L"[NetworkManager] Failed to send leather count update packet\n");
        } else {
            wchar_t debugMsg[256];
            swprintf_s(debugMsg, L"[NetworkManager] Sent leather count update: %d\n", leatherCount);
            OutputDebugString(debugMsg);
        }
    }
    catch (const std::exception& e) {
        OutputDebugString(L"[NetworkManager] Exception in SendLeatherCountUpdate\n");
    }
} 