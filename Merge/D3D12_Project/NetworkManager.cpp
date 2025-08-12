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
        if (m_logFile.is_open()) {
            LogToFile("NetworkManager initialized");
        }
        else {
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
        LogToFile("[Error] Scene is null");
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
                LogToFile("[Info] Found Local PlayerObject in scene after " + std::to_string(retryCount * 100) + "ms");
                break;
            } else {
                LogToFile("[Warning] Local PlayerObject not found in scene, attempt " + std::to_string(retryCount + 1) + "/" + std::to_string(MAX_RETRIES));
                Sleep(100); // 100ms 대기
                retryCount++;
            }
        }
        catch (const std::exception& e) {
            LogToFile("[Error] Failed to find PlayerObject: " + std::string(e.what()));
            Sleep(100);
            retryCount++;
        }
    }
    
    if (retryCount >= MAX_RETRIES) {
        LogToFile("[Error] PlayerObject not found after all retries");
        return false;
    }
    
    // OtherPlayerManager 초기화
    OtherPlayerManager::GetInstance()->SetScene(scene);
    OtherPlayerManager::GetInstance()->SetNetworkManager(this);
    LogToFile("[Info] OtherPlayerManager initialized with scene and network manager");
    
    std::string logMsg = "[Client] Connecting to server " + std::string(serverIP) + ":" + std::to_string(port);
    LogToFile(logMsg);
    
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        LogToFile("[Error] WSAStartup failed");
        return false;
    }

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) {
        LogToFile("[Error] Socket creation failed");
        return false;
    }

    SOCKADDR_IN serverAddr = { 0 };
    serverAddr.sin_family = AF_INET;
    inet_pton(AF_INET, serverIP, &serverAddr.sin_addr);
    serverAddr.sin_port = htons(port);

    if (connect(sock, (SOCKADDR*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        LogToFile("[Error] Connection failed");
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

    LogToFile("[Client] Successfully connected to server");

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
            LogToFile("[Login] Sent login request for user: " + username);
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
        LogToFile("[Disconnect] Sent disconnect packet for user: " + m_username);
    }
    catch (const std::exception& e) {
        LogToFile("[Error] Failed to send disconnect packet: " + std::string(e.what()));
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
            LogToFile("[Error] Failed to send tiger respawn request: " + std::to_string(error));
        } else {
            LogToFile("[Tiger] Sent tiger respawn request to server");
        }
    }
    catch (const std::exception& e) {
        LogToFile("[Error] Exception in SendTigerRespawnRequest: " + std::string(e.what()));
    }
}

void NetworkManager::SendTigerHit(int tigerID, int life) {
    if (!m_isRunning || !m_isLoggedIn) return;

    try {
        PacketTigerHit pkt;
        pkt.header.size = sizeof(PacketTigerHit);
        pkt.header.type = PACKET_TIGER_HIT;
        pkt.tigerID = tigerID;
        pkt.life = life;

        int sendResult = send(sock, (char*)&pkt, sizeof(pkt), 0);
        if (sendResult == SOCKET_ERROR) {
            int error = WSAGetLastError();
            LogToFile("[Error] Failed to send tiger hit: " + std::to_string(error));
        } else {
            LogToFile("[Tiger] Sent tiger hit to server, tigerID: " + std::to_string(tigerID) + ", life: " + std::to_string(life));
        }
    }
    catch (const std::exception& e) {
        LogToFile("[Error] Exception in SendTigerHit: " + std::string(e.what()));
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
            LogToFile("[Error] Failed to send tiger attack: " + std::to_string(error));
        } else {
            LogToFile("[Tiger] Sent tiger attack to server, tigerID: " + std::to_string(tigerID));
        }
    }
    catch (const std::exception& e) {
        LogToFile("[Error] Exception in SendTigerAttack: " + std::string(e.what()));
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
            LogToFile("[Error] Failed to send stage change: " + std::to_string(error));
        } else {
            LogToFile("[Stage] Sent stage change to server: " + stageNameStr);
        }
    }
    catch (const std::exception& e) {
        LogToFile("[Error] Exception in SendStageChange: " + std::string(e.what()));
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
    network->LogToFile("[Thread] Network thread started");
    
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
                network->LogToFile("[Error] Select failed with error: " + std::to_string(error));
                if (++errorCount >= MAX_ERRORS) {
                    network->LogToFile("[Warning] Many select errors, but continuing...");
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
                    network->LogToFile("[Warning] Connection issue detected: " + std::to_string(error));
                    // 연결 문제가 있어도 계속 시도 (재연결 로직으로 처리)
                    continue;
                }
                network->LogToFile("[Error] Receive failed: " + std::to_string(error));
                if (++errorCount >= MAX_ERRORS) {
                    network->LogToFile("[Warning] Many receive errors, but continuing...");
                    // 에러가 많아도 연결 유지
                    continue;
                }
                continue;
            }
            
            // 수신된 데이터 로그 (디버깅용) - 빈도 감소
            static int packetCount = 0;
            if (++packetCount % 50 == 0) { // 50개 패킷마다 로그 (로그 부하 감소)
                network->LogToFile("[Network] Received " + std::to_string(recvBytes) + " bytes");
            }

            if (recvBytes > sizeof(network->m_recvBuffer)) {
                network->LogToFile("[Error] Receive buffer overflow: " + std::to_string(recvBytes) + " bytes");
                return 1;
            }

            // 수신된 데이터를 패킷 버퍼에 추가
            if (network->m_packetBufferSize + recvBytes > sizeof(network->m_packetBuffer)) {
                network->LogToFile("[Error] Packet buffer overflow, clearing buffer");
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
                    network->LogToFile("[Error] Invalid packet size: " + std::to_string(header->size));
                    network->m_packetBufferSize = 0;
                    memset(network->m_packetBuffer, 0, sizeof(network->m_packetBuffer));
                    if (++errorCount >= MAX_ERRORS) {
                        network->LogToFile("[Warning] Many invalid packets, but continuing...");
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
                    network->LogToFile("[Error] Exception during packet processing: " + std::string(e.what()));
                } catch (...) {
                    // 패킷 처리 중 예외 발생 시 무시하고 계속 진행
                    network->LogToFile("[Error] Unknown exception during packet processing");
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
            network->LogToFile("[Error] Exception in network thread: " + std::string(e.what()));
            if (++errorCount >= MAX_ERRORS) {
                network->LogToFile("[Warning] Many exceptions, but continuing...");
                // 에러가 많아도 연결 유지
                continue;
            }
            Sleep(1000);
        }
    }
    
    network->LogToFile("[Thread] Network thread ended normally");
    return 0;
}

void NetworkManager::HandleError(const std::string& description) {
    DWORD currentTime = GetTickCount();
    
    m_errorCount++;
    m_lastErrorTime = currentTime;
    
    std::string errorMsg = "[Error] Count: " + std::to_string(m_errorCount) + 
                          ", Description: " + description;
    LogToFile(errorMsg);
    
    // 5번 연속 에러 시 재연결 시도
    if (m_errorCount >= 5) {
        LogToFile("[Warning] Too many errors, considering reconnection");
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
    
    LogToFile("[Reconnect] Attempting reconnection #" + std::to_string(m_reconnectAttempts));
    
    // 기존 소켓 정리
    if (sock != INVALID_SOCKET) {
        closesocket(sock);
        sock = INVALID_SOCKET;
    }
    
    // WSA 정리 후 재시작
    WSACleanup();
    
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        LogToFile("[Error] WSAStartup failed during reconnection");
        return false;
    }

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) {
        LogToFile("[Error] Socket creation failed during reconnection");
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
        LogToFile("[Error] Reconnection failed with error: " + std::to_string(error));
        return false;
    }

    LogToFile("[Reconnect] Successfully reconnected to server");
    
    // 재연결 성공 시 상태 초기화
    m_shouldReconnect = false;
    m_reconnectAttempts = 0;
    ResetErrorInfo();
    
    // 로그인 상태 복구 시도 (스테이지 변경 중이 아닐 때만)
    if (!m_username.empty() && !m_isStageChanging) {
        LogToFile("[Reconnect] Attempting to restore login session");
        SendLoginRequest(m_username);
    } else if (m_isStageChanging) {
        LogToFile("[Reconnect] Skipping login restore during stage change");
    }
    
    return true;
}

void NetworkManager::ResetErrorInfo() {
    m_errorCount = 0;
    m_packetBufferSize = 0;
    memset(m_packetBuffer, 0, sizeof(m_packetBuffer));
    LogToFile("[Info] Error info and packet buffer reset");
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
            
            LogToFile("[Warning] Normalized rotation value from " + std::to_string(rotY) + " to " + std::to_string(safeRotY));
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
                LogToFile("[Warning] Send buffer full, packet dropped");
            } else {
                // 기타 에러는 로그만 남기고 계속 진행
                LogToFile("[Warning] Send error: " + std::to_string(error) + ", continuing...");
            }
        } else {
            // Player 테스트를 위해 위치 업데이트 로그 추가
            static float lastX = 0.0f, lastY = 0.0f, lastZ = 0.0f;
            if (abs(x - lastX) > 1.0f || abs(y - lastY) > 1.0f || abs(z - lastZ) > 1.0f) {
                LogToFile("[Player] Sent position update: (" + std::to_string(x) + ", " + std::to_string(y) + ", " + std::to_string(z) + ")");
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
                        char memBuffer[256];
                        sprintf_s(memBuffer, "[Memory] High usage: %.1f%% (%llu MB)", 
                            memoryUsagePercent, usedMemory / (1024*1024));
                        LogToFile(memBuffer);
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
                LogToFile("[Login] Received login response - Success: " + std::to_string(loginRespPkt->success));
                
                if (loginRespPkt->success) {
                    m_myClientID = loginRespPkt->clientID;
                    m_isLoggedIn = true;
                    LogToFile("[Login] Login successful - Client ID: " + std::to_string(m_myClientID));
                    
                    // 로그인 성공 후 준비 완료 신호 전송
                    PacketClientReady readyPacket;
                    readyPacket.header.type = PACKET_CLIENT_READY;
                    readyPacket.header.size = sizeof(PacketClientReady);
                    readyPacket.clientID = m_myClientID;
                    
                    int sendResult = send(sock, (char*)&readyPacket, sizeof(readyPacket), 0);
                    if (sendResult == SOCKET_ERROR) {
                        int error = WSAGetLastError();
                        LogToFile("[Error] Failed to send ready packet: " + std::to_string(error));
                    } else {
                        LogToFile("[Login] Sent client ready packet");
                    }
                    
                    if (m_loginSuccessCallback) {
                        m_loginSuccessCallback(m_myClientID, m_username);
                    }
                } else {
                    m_isLoggedIn = false;
                    std::string errorMsg = loginRespPkt->message;
                    LogToFile("[Login] Login failed: " + errorMsg);
                    
                    if (m_loginFailedCallback) {
                        m_loginFailedCallback(errorMsg);
                    }
                }
                break;
            }
            
            case PACKET_PLAYER_SPAWN: {
                PacketPlayerSpawn* spawnPkt = (PacketPlayerSpawn*)buffer;
                LogToFile("[Spawn] Processing spawn packet for ID: " + std::to_string(spawnPkt->playerID));

                // 로그인 상태 확인
                if (!m_isLoggedIn) {
                    LogToFile("[Spawn] Ignoring player spawn packet - not logged in yet");
                    break;
                }

                if (m_myClientID == 0) {
                    m_myClientID = spawnPkt->playerID;
                    LogToFile("[Spawn] Set my client ID to: " + std::to_string(m_myClientID));
                }
                else if (spawnPkt->playerID != m_myClientID) {
                    try {
                        OtherPlayerManager::GetInstance()->SpawnOtherPlayer(spawnPkt->playerID);
                        LogToFile("[Spawn] Successfully spawned other player: " + std::to_string(spawnPkt->playerID) + " (" + spawnPkt->username + ")");
                    }
                    catch (const std::exception& e) {
                        LogToFile("[Error] Failed to spawn other player: " + std::string(e.what()));
                    }
                }
                break;
            }
            
            case PACKET_PLAYER_DISCONNECT: {
                PacketPlayerDisconnect* disconnectPkt = (PacketPlayerDisconnect*)buffer;
                LogToFile("[Disconnect] Player disconnected: " + std::to_string(disconnectPkt->playerID) + " (" + disconnectPkt->username + ")");
                
                if (disconnectPkt->playerID != m_myClientID) {
                    try {
                        OtherPlayerManager::GetInstance()->RemoveOtherPlayer(disconnectPkt->playerID);
                        LogToFile("[Disconnect] Removed other player: " + std::to_string(disconnectPkt->playerID));
                    }
                    catch (const std::exception& e) {
                        LogToFile("[Error] Failed to remove other player: " + std::string(e.what()));
                    }
                }
                break;
            }

            case PACKET_PLAYER_UPDATE: {
                PacketPlayerUpdate* updatePkt = (PacketPlayerUpdate*)buffer;
                
                // 로그인 상태 확인
                if (!m_isLoggedIn) {
                    LogToFile("[Update] Ignoring player update packet - not logged in yet");
                    break;
                }
                
                if (updatePkt->clientID == m_myClientID) {
                    LogToFile("[Update] Ignoring own update packet");
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
                LogToFile("[Update] Successfully updated player: " + std::to_string(updatePkt->clientID) + " in stage: " + stageName);
                break;
            }

            case PACKET_TIGER_SPAWN: {
                PacketTigerSpawn* tigerSpawnPkt = (PacketTigerSpawn*)buffer;
                wchar_t debugMsg[256];
                swprintf_s(debugMsg, L"[Tiger] Received spawn packet for tiger ID: %d at position (%.1f, %.1f, %.1f)\n", 
                          tigerSpawnPkt->tigerID, tigerSpawnPkt->x, tigerSpawnPkt->y, tigerSpawnPkt->z);
                OutputDebugString(debugMsg);
                
                // 로그인 상태 확인 - 로그인 전에 받은 호랑이 스폰 패킷은 무시
                if (!m_isLoggedIn) {
                    OutputDebugString(L"[Tiger] Ignoring tiger spawn packet - not logged in yet\n");
                    break;
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
                
                // 첫 번째 호랑이 스폰 패킷을 받으면 Hunting Stage로 전환 (주석 처리)
                // if (m_tigers.size() == 1) {
                //     LogToFile("[Tiger] First tiger received, switching to Hunting Stage");
                //     if (m_scene) {
                //         m_scene->SetStage(L"Hunting");
                //     }
                // }
                
                // Scene에 Tiger 생성 요청 (Hunting Stage일 때만)
                if (m_scene) {
                    OutputDebugString(L"[Tiger] Scene found, checking current stage...\n");
                    // 현재 스테이지가 Hunting일 때만 호랑이 생성
                    if (m_scene->GetCurrentStage() == L"Hunting") {
                        OutputDebugString(L"[Tiger] Current stage is Hunting, creating tiger object...\n");
                        try {
                            float scale = 0.2f;
                            TigerObject* tigerObj = new TigerObject(m_scene, m_scene->AllocateId());
                            tigerObj->SetIsNetworkTiger(true);  // 네트워크 호랑이로 설정
                            tigerObj->AddComponent(new Transform{ {tigerSpawnPkt->x, tigerSpawnPkt->y, tigerSpawnPkt->z} });
                            tigerObj->AddComponent(new AdjustTransform{ {0.0f, 0.0f, -40.0f * scale}, {0.0f, 180.0f, 0.0f}, {scale, scale, scale} });
                            tigerObj->AddComponent(new Mesh{ "0113_tiger.fbx" });
                            tigerObj->AddComponent(new Texture{ L"tigercolor", 1.0f, 0.4f });
                            tigerObj->AddComponent(new Animation{ "0113_tiger_walk.fbx" });
                            tigerObj->AddComponent(new Gravity);
                            tigerObj->AddComponent(new Collider{ {0.0f, 6.0f, 0.0f}, {2.0f, 6.0f, 10.0f} });
                            
                            m_scene->AddObj(tigerObj);
                            
                            // 호랑이 ID를 저장하여 나중에 업데이트할 때 사용
                            tigerObj->SetNetworkTigerID(tigerSpawnPkt->tigerID);
                            
                            // 초기 위치 설정 (보간 제거로 인해 즉시 설정)
                            Transform* transform = tigerObj->GetComponent<Transform>();
                            if (transform) {
                                transform->SetPosition({tigerSpawnPkt->x, tigerSpawnPkt->y, tigerSpawnPkt->z, 1.0f});
                                transform->SetRotation({0.0f, 0.0f, 0.0f});
                            }
                            
                            // 초기 애니메이션 설정
                            Animation* anim = tigerObj->GetComponent<Animation>();
                            if (anim) {
                                anim->ResetAnim("0722_tiger_idle2.fbx", 0.0f);
                            }
                            
                            swprintf_s(debugMsg, L"[Tiger] Successfully created tiger object for ID: %d\n", tigerSpawnPkt->tigerID);
                            OutputDebugString(debugMsg);
                        }
                        catch (const std::exception& e) {
                            OutputDebugString(L"[Error] Failed to create tiger object\n");
                        }
                    } else {
                        // wstring을 안전하게 string으로 변환
                        std::wstring currentStage = m_scene->GetCurrentStage();
                        std::string stageStr(currentStage.begin(), currentStage.end());
                        swprintf_s(debugMsg, L"[Tiger] Ignoring tiger spawn - not in Hunting Stage (current stage: %s)\n", 
                                 std::wstring(stageStr.begin(), stageStr.end()).c_str());
                        OutputDebugString(debugMsg);
                    }
                } else {
                    OutputDebugString(L"[Tiger] Scene is null, cannot create tiger object\n");
                }
                break;
            }
            
            case PACKET_TIGER_UPDATE: {
                PacketTigerUpdate* tigerUpdatePkt = (PacketTigerUpdate*)buffer;
                
                // 로그인 상태 확인
                if (!m_isLoggedIn) {
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
                // 로그인 상태 확인
                if (!m_isLoggedIn) {
                    break;
                }
                // Scene에서 해당 호랑이를 찾아서 공격 신호만 설정 (공격 오브젝트는 0.4초 후 CalcTime에서 생성됨)
                if (m_scene) {
                    for (Object* obj : m_scene->GetObjects()) {
                        TigerObject* tigerObj = dynamic_cast<TigerObject*>(obj);
                        if (tigerObj && tigerObj->IsNetworkTiger() && tigerObj->GetNetworkTigerID() == tigerAttackPkt->tigerID) {
                            // 서버 공격 신호 설정 (공격 오브젝트는 0.4초 후 CalcTime에서 생성됨)
                            tigerObj->SetServerAttackSignal(true);
                            LogToFile("[Tiger] Network tiger " + std::to_string(tigerAttackPkt->tigerID) + " received attack signal from server");
                            break;
                        }
                    }
                }
                break;
            }
            
            case PACKET_TIGER_HIT: {
                PacketTigerHit* tigerHitPkt = (PacketTigerHit*)buffer;
                
                // 로그인 상태 확인
                if (!m_isLoggedIn) {
                    break;
                }
                
                // Scene에서 해당 호랑이를 찾아서 Hit 상태 동기화
                if (m_scene) {
                    for (Object* obj : m_scene->GetObjects()) {
                        TigerObject* tigerObj = dynamic_cast<TigerObject*>(obj);
                        if (tigerObj && tigerObj->IsNetworkTiger() && tigerObj->GetNetworkTigerID() == tigerHitPkt->tigerID) {
                            // 서버에서 받은 생명력으로 업데이트
                            int currentLife = tigerObj->GetLife();
                            if (currentLife != tigerHitPkt->life) {  // 생명력이 변경된 경우에만 처리
                                tigerObj->SetLife(tigerHitPkt->life);
                                
                                if (tigerHitPkt->life <= 0) {
                                    tigerObj->Dead();
                                } else {
                                    // 클라이언트에서 이미 hit 애니메이션을 재생했으므로 생명력만 업데이트
                                    // 애니메이션은 클라이언트에서 관리하므로 서버에서 재생하지 않음
                                    LogToFile("[Tiger] Hit animation already playing for tiger " + std::to_string(tigerHitPkt->tigerID));
                                }
                                
                                LogToFile("[Tiger] Tiger " + std::to_string(tigerHitPkt->tigerID) + " hit, remaining life: " + std::to_string(tigerHitPkt->life));
                            }
                            break;
                        }
                    }
                }
                break;
            }

            default:
                LogToFile("[Warning] Unknown packet type: " + std::to_string(header->type));
                break;
        }
    }
    catch (const std::exception& e) {
        LogToFile("[Error] Failed to process packet: " + std::string(e.what()));
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
            LogToFile("[Update] Successfully reconnected");
        } else {
            LogToFile("[Update] Reconnection attempt failed");
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
        LogToFile("[Error] Local PlayerObject not found in scene");
        return;
    }
    
    auto* transform = player->GetComponent<Transform>();
    if (!transform) {
        LogToFile("[Error] Transform component not found on Local PlayerObject");
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
        
        LogToFile("[Warning] Normalized rotation value in Update from " + std::to_string(XMVectorGetY(rot)) + " to " + std::to_string(rotY));
    }
    
    m_updateTimer += gTimer.DeltaTime();
    const float UPDATE_INTERVAL = 0.016667f;  // 60fps (16.67ms)마다 업데이트

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
    LogToFile("[Stage] Stage transition state set to: " + std::string(transitioning ? "true" : "false"));
} 