#include "Server.h"
#include <iostream>
#include <format>
#include <random>
#include <algorithm>
#define NOMINMAX
#include <windows.h>
#include <chrono>

GameServer::GameServer()
    : m_nextClientID(1)
    , m_nextTigerID(1)
    , m_isRunning(false)
    , m_hIOCP(NULL)
    , m_listenSocket(INVALID_SOCKET)
    , m_port(5000)
    , m_huntingStageActive(false)
    , m_randomEngine(std::random_device{}())
    , m_puzzleInitialized(false)
{
    // 퍼즐 상태 초기화
    InitializePuzzle();
}

GameServer::~GameServer() {
    Cleanup();
}

bool GameServer::Initialize(int port) {
    m_port = port;
    std::cout << "[Server] Starting server on port " << m_port << std::endl;

    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cout << "[Error] WSAStartup failed" << std::endl;
        return false;
    }

    m_listenSocket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
    if (m_listenSocket == INVALID_SOCKET) {
        std::cout << "[Error] Failed to create listen socket" << std::endl;
        return false;
    }

    SOCKADDR_IN serverAddr = { 0 };
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serverAddr.sin_port = htons(m_port);

    if (bind(m_listenSocket, (SOCKADDR*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cout << "[Error] Bind failed" << std::endl;
        return false;
    }

    if (listen(m_listenSocket, SOMAXCONN) == SOCKET_ERROR) {
        std::cout << "[Error] Listen failed" << std::endl;
        return false;
    }

    m_hIOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
    if (m_hIOCP == NULL) {
        std::cout << "[Error] CreateIoCompletionPort failed" << std::endl;
        return false;
    }

    // 호랑이 초기화는 Hunting 스테이지 활성화 시에 수행
    std::cout << "[Server] Tiger initialization delayed until Hunting Stage activation" << std::endl;
    return true;
}

void GameServer::Start() {
    m_isRunning = true;
    
    // Create worker threads
    for (int i = 0; i < 2; ++i) {
        HANDLE hThread = CreateThread(NULL, 0, WorkerThreadProc, this, 0, NULL);
        if (hThread) {
            m_workerThreads.push_back(hThread);
        }
    }

    // Main accept loop
    while (m_isRunning) {
        SOCKET clientSocket = accept(m_listenSocket, NULL, NULL);
        if (clientSocket != INVALID_SOCKET) {
            ProcessNewClient(clientSocket);
        }
    }
}

void GameServer::Stop() {
    m_isRunning = false;
    Cleanup();
}

DWORD WINAPI GameServer::WorkerThreadProc(LPVOID lpParam) {
    GameServer* server = static_cast<GameServer*>(lpParam);
    return server->WorkerThread();
}

DWORD GameServer::WorkerThread() {
    DWORD lastTime = GetTickCount();
    
    while (m_isRunning) {
        DWORD currentTime = GetTickCount();
        float deltaTime = (currentTime - lastTime) / 1000.0f;
        lastTime = currentTime;
        
        // 호랑이 업데이트
        UpdateTigers(deltaTime);
        
        // 클라이언트 연결 상태 모니터링
        MonitorClientConnections();
        
        // 기존 IOCP 처리 코드
        DWORD bytesTransferred;
        ULONG_PTR completionKey;
        OVERLAPPED* pOverlapped;
        
        BOOL result = GetQueuedCompletionStatus(m_hIOCP, &bytesTransferred, 
            &completionKey, &pOverlapped, 8); // 타임아웃을 8ms로 설정 (120fps 업데이트 주기와 맞춤)
        
        if (!m_isRunning) break;
        
        // 타임아웃 발생 시 (pOverlapped가 NULL)
        if (!pOverlapped) {
            continue;  // 타임아웃은 정상적인 상황이므로 계속 진행
        }
        
        IOContext* ioContext = CONTAINING_RECORD(pOverlapped, IOContext, overlapped);
        int clientID = static_cast<int>(completionKey);
        
        if (m_clients.find(clientID) == m_clients.end()) {
            delete ioContext;
            continue;
        }
        
        // 연결 해제 감지: result가 FALSE이고 bytesTransferred가 0인 경우만
        if (!result && bytesTransferred == 0) {
            int error = WSAGetLastError();
            
            // 실제 연결 해제인 경우만 처리
            if (error == WSAECONNRESET || error == WSAECONNABORTED || error == WSAENETDOWN) {
                // 클라이언트 제거
                auto clientIt = m_clients.find(clientID);
                if (clientIt != m_clients.end()) {
                    std::cout << "[Connection] Client " << clientID << " disconnected (Error: " << error << ")" << std::endl;
                    closesocket(clientIt->second.socket);
                    m_clients.erase(clientIt);
                }
                delete ioContext;
                continue;
            }
            
            // 일시적 에러는 무시하고 계속 진행
            if (error == WSAEWOULDBLOCK || error == ERROR_IO_PENDING) {
                // 다음 수신 준비
                auto clientIt = m_clients.find(clientID);
                if (clientIt != m_clients.end() && clientIt->second.socket != INVALID_SOCKET) {
                    if (StartReceive(clientIt->second.socket, clientID, ioContext)) {
                        continue;
                    }
                }
            }
            
            delete ioContext;
            continue;
        }
        
        // 정상적인 데이터 수신이지만 bytesTransferred가 0인 경우는 무시
        if (bytesTransferred == 0) {
            delete ioContext;
            continue;
        }

        HandlePacket(ioContext, clientID, bytesTransferred);

        // 다음 수신 준비 - 클라이언트 존재 여부 확인
        auto clientIt = m_clients.find(clientID);
        if (clientIt == m_clients.end()) {
            std::cout << "[Error] Client " << clientID << " not found for next receive" << std::endl;
            delete ioContext;
            continue;
        }
        
        if (clientIt->second.socket == INVALID_SOCKET) {
            std::cout << "[Error] Invalid socket for client " << clientID << " for next receive" << std::endl;
            delete ioContext;
            continue;
        }
        
        // 다음 수신 준비
        if (!StartReceive(clientIt->second.socket, clientID, ioContext)) {
            std::cout << "[Error] Failed to start next receive for client " << clientID << std::endl;
            
            // StartReceive 실패 시 클라이언트 상태 확인
            if (clientIt->second.socket == INVALID_SOCKET) {
                // 소켓이 이미 무효화된 경우 클라이언트 제거
                m_clients.erase(clientIt);
                std::cout << "[Info] Removed invalid client " << clientID << std::endl;
            } else {
                // 소켓은 유효하지만 StartReceive 실패한 경우 재시도
                Sleep(100); // 잠시 대기 후 재시도
                if (StartReceive(clientIt->second.socket, clientID, ioContext)) {
                    continue;
                } else {
                    // 재시도 실패 시 클라이언트 제거
                    std::cout << "[Warning] StartReceive retry failed for client " << clientID << ", removing client" << std::endl;
                    closesocket(clientIt->second.socket);
                    m_clients.erase(clientIt);
                }
            }
            
            delete ioContext;
            continue;
        }
    }
    return 0;
}

void GameServer::HandlePacket(IOContext* ioContext, int clientID, DWORD bytesTransferred) {
    // 클라이언트 정보 가져오기
    auto clientIt = m_clients.find(clientID);
    if (clientIt == m_clients.end()) {
        std::cout << "[Error] Client " << clientID << " not found in HandlePacket" << std::endl;
        return;
    }
    
    ClientInfo& client = clientIt->second;
    
    // 수신된 데이터를 패킷 버퍼에 추가
    if (client.packetBufferSize + bytesTransferred > sizeof(client.packetBuffer)) {
        std::cout << "[Error] Packet buffer overflow for client " << clientID << std::endl;
        client.packetBufferSize = 0;  // 버퍼 초기화
        return;
    }
    
    memcpy(client.packetBuffer + client.packetBufferSize, ioContext->buffer, bytesTransferred);
    client.packetBufferSize += bytesTransferred;
    
    // 패킷 버퍼에서 완전한 패킷들을 처리
    int processedBytes = 0;
    while (client.packetBufferSize - processedBytes >= sizeof(PacketHeader)) {
        PacketHeader* header = (PacketHeader*)(client.packetBuffer + processedBytes);
        
        // 패킷 헤더 유효성 검사
        if (header->size < sizeof(PacketHeader) || header->size > MAX_PACKET_SIZE || 
            header->type <= 0 || header->type > 19) {
            std::cout << "[Error] Invalid packet header - Size: " << header->size 
                      << ", Type: " << header->type << ", Client: " << clientID << std::endl;
            
            // 잘못된 패킷의 첫 몇 바이트를 출력하여 디버깅
            std::cout << "[Debug] First 16 bytes: ";
            for (int i = 0; i < std::min(16, static_cast<int>(client.packetBufferSize - processedBytes)); ++i) {
                printf("%02X ", static_cast<unsigned char>(client.packetBuffer[processedBytes + i]));
            }
            std::cout << std::endl;
            
            // 버퍼 초기화
            client.packetBufferSize = 0;
            return;
        }
        
        // 완전한 패킷이 있는지 확인
        if (client.packetBufferSize - processedBytes < header->size) {
            break;  // 완전한 패킷이 없음
        }
        
        // 패킷 처리
        ProcessSinglePacket(client.packetBuffer + processedBytes, clientID, header->size);
        processedBytes += header->size;
        
        // 성공적인 패킷 수신 시 연결 에러 카운트와 로그 상태 리셋
        client.connectionErrorCount = 0;
        client.connectionErrorLogged = false;
    }
    
    // 처리된 데이터를 버퍼에서 제거
    if (processedBytes > 0) {
        if (processedBytes < client.packetBufferSize) {
            memmove(client.packetBuffer, client.packetBuffer + processedBytes, 
                   client.packetBufferSize - processedBytes);
        }
        client.packetBufferSize -= processedBytes;
    }
}

void GameServer::ProcessSinglePacket(char* buffer, int clientID, int packetSize) {
    PacketHeader* header = (PacketHeader*)buffer;
    
    // Receive 로그 제거 (주기적으로 나오므로)

    switch (header->type) {
        case PACKET_LOGIN_REQUEST: {
            if (header->size != sizeof(PacketLoginRequest)) {
                std::cout << "[Error] Invalid LOGIN_REQUEST packet size" << std::endl;
                break;
            }
            PacketLoginRequest* pkt = (PacketLoginRequest*)buffer;
            
            // 현재 클라이언트가 이미 로그인되어 있는지 확인
            auto currentClientIt = m_clients.find(clientID);
            if (currentClientIt != m_clients.end() && currentClientIt->second.isLoggedIn) {
                // 이미 로그인된 클라이언트의 재로그인 요청
                std::cout << "[Login] Client " << clientID << " already logged in, sending success response" << std::endl;
                
                PacketLoginResponse response;
                response.header.type = PACKET_LOGIN_RESPONSE;
                response.header.size = sizeof(PacketLoginResponse);
                response.clientID = clientID;
                response.success = true;
                strncpy_s(response.message, "Already logged in", sizeof(response.message) - 1);
                
                auto clientIt = m_clients.find(clientID);
                if (clientIt != m_clients.end()) {
                    SendPacket(clientIt->second.socket, &response, sizeof(response));
                }
                break;
            }
            
            // 사용자명 중복 체크 (다른 클라이언트와의 중복)
            bool usernameExists = false;
            for (const auto& clientPair : m_clients) {
                const auto& client = clientPair.second;
                if (client.isLoggedIn && client.username == pkt->username) {
                    usernameExists = true;
                    break;
                }
            }
            
            PacketLoginResponse response;
            response.header.type = PACKET_LOGIN_RESPONSE;
            response.header.size = sizeof(PacketLoginResponse);
            response.clientID = clientID;
            
            if (usernameExists) {
                response.success = false;
                strncpy_s(response.message, "Username already exists", sizeof(response.message) - 1);
                std::cout << "[Login] Failed for client " << clientID << " - Username already exists: " << pkt->username << std::endl;
            } else {
                response.success = true;
                strncpy_s(response.message, "Login successful", sizeof(response.message) - 1);
                
                // 클라이언트 정보 업데이트
                auto clientIt = m_clients.find(clientID);
                if (clientIt != m_clients.end()) {
                    clientIt->second.username = pkt->username;
                    clientIt->second.isLoggedIn = true;
                }
                
                std::cout << "[Login] Success for client " << clientID << " - Username: " << pkt->username << std::endl;
                
                // 로그인 성공 후 플레이어 스폰 패킷 전송
                PacketPlayerSpawn spawnPacket;
                spawnPacket.header.type = PACKET_PLAYER_SPAWN;
                spawnPacket.header.size = sizeof(PacketPlayerSpawn);
                spawnPacket.playerID = clientID;
                strncpy_s(spawnPacket.username, pkt->username, sizeof(spawnPacket.username) - 1);
                
                BroadcastPacket(&spawnPacket, sizeof(spawnPacket));
                
                // 로그인 성공 후 클라이언트 준비 완료 신호를 기다림
                std::cout << "[Login] Waiting for client " << clientID << " to send ready signal" << std::endl;
            }
            
            auto clientIt = m_clients.find(clientID);
            if (clientIt != m_clients.end()) {
                SendPacket(clientIt->second.socket, &response, sizeof(response));
            }
            break;
        }
        
        case PACKET_PLAYER_DISCONNECT: {
            if (header->size != sizeof(PacketPlayerDisconnect)) {
                std::cout << "[Error] Invalid PLAYER_DISCONNECT packet size" << std::endl;
                break;
            }
            PacketPlayerDisconnect* pkt = (PacketPlayerDisconnect*)buffer;
            
            std::cout << "[Disconnect] Player " << pkt->username << " (ID: " << pkt->playerID << ") disconnected" << std::endl;
            
            // 다른 클라이언트들에게 연결 해제 알림
            BroadcastPacket(pkt, sizeof(PacketPlayerDisconnect), clientID);
            
            // 클라이언트 제거
            auto clientIt = m_clients.find(clientID);
            if (clientIt != m_clients.end()) {
                if (clientIt->second.socket != INVALID_SOCKET) {
                    closesocket(clientIt->second.socket);
                    clientIt->second.socket = INVALID_SOCKET;
                }
                m_clients.erase(clientIt);
                std::cout << "[Disconnect] Client " << clientID << " removed. Remaining clients: " << m_clients.size() << std::endl;
            } else {
                std::cout << "[Disconnect] Client " << clientID << " not found in client list" << std::endl;
            }
            break;
        }
        
        case PACKET_PLAYER_UPDATE: {
            if (header->size != sizeof(PacketPlayerUpdate)) {
                std::cout << "[Error] Invalid PLAYER_UPDATE packet size" << std::endl;
                break;
            }
            
            // 클라이언트 존재 여부 확인
            auto clientIt = m_clients.find(clientID);
            if (clientIt == m_clients.end()) {
                std::cout << "[Error] Client " << clientID << " not found for PLAYER_UPDATE" << std::endl;
                break;
            }
            
            // 소켓 유효성 확인
            if (clientIt->second.socket == INVALID_SOCKET) {
                std::cout << "[Error] Invalid socket for client " << clientID << " in PLAYER_UPDATE" << std::endl;
                break;
            }
            
            PacketPlayerUpdate* pkt = (PacketPlayerUpdate*)buffer;
            pkt->clientID = clientID;
            clientIt->second.lastUpdate = *pkt;
            
            // 스테이지 정보 추가
            strcpy_s(pkt->stageName, sizeof(pkt->stageName), clientIt->second.currentStage.c_str());
            
            // PlayerUpdate 로그 제거 (주기적으로 나오므로)
            
            // 같은 스테이지에 있는 클라이언트들에게만 전송
            BroadcastToStage(pkt, sizeof(PacketPlayerUpdate), clientIt->second.currentStage, clientID);
            break;
        }
        case PACKET_PLAYER_SPAWN: {
            if (header->size != sizeof(PacketPlayerSpawn)) {
                std::cout << "[Error] Invalid PLAYER_SPAWN packet size" << std::endl;
                break;
            }
            PacketPlayerSpawn* pkt = (PacketPlayerSpawn*)buffer;
            
            // 클라이언트 존재 여부 확인
            auto clientIt = m_clients.find(clientID);
            if (clientIt != m_clients.end()) {
                // 같은 스테이지에 있는 클라이언트들에게만 전송
                BroadcastToStage(pkt, sizeof(PacketPlayerSpawn), clientIt->second.currentStage, clientID);
            }
            break;
        }
        case PACKET_TIGER_SPAWN: {
            if (header->size != sizeof(PacketTigerSpawn)) {
                std::cout << "[Error] Invalid TIGER_SPAWN packet size" << std::endl;
                break;
            }
            PacketTigerSpawn* pkt = (PacketTigerSpawn*)buffer;
            
            // 호랑이 스폰 패킷을 모든 클라이언트에게 브로드캐스트
            std::cout << "[TigerSpawn] Broadcasting tiger spawn packet for ID: " << pkt->tigerID 
                      << " at position (" << pkt->x << ", " << pkt->y << ", " << pkt->z << ")" << std::endl;
            BroadcastPacket(pkt, sizeof(PacketTigerSpawn));
            break;
        }
        case PACKET_TIGER_UPDATE: {
            if (header->size != sizeof(PacketTigerUpdate)) {
                std::cout << "[Error] Invalid TIGER_UPDATE packet size" << std::endl;
                break;
            }
            PacketTigerUpdate* pkt = (PacketTigerUpdate*)buffer;
            BroadcastPacket(pkt, sizeof(PacketTigerUpdate));
            break;
        }
        case PACKET_TIGER_ATTACK: {
            if (header->size != sizeof(PacketTigerAttack)) {
                std::cout << "[Error] Invalid TIGER_ATTACK packet size" << std::endl;
                break;
            }
            PacketTigerAttack* pkt = (PacketTigerAttack*)buffer;
            BroadcastPacket(pkt, sizeof(PacketTigerAttack));
            break;
        }
        
        case PACKET_TIGER_HIT: {
            if (header->size != sizeof(PacketTigerHit)) {
                std::cout << "[Error] Invalid TIGER_HIT packet size" << std::endl;
                break;
            }
            PacketTigerHit* pkt = (PacketTigerHit*)buffer;
            
            // 해당 호랑이의 상태 업데이트
            auto tigerIt = m_tigers.find(pkt->tigerID);
            if (tigerIt != m_tigers.end()) {
                TigerInfo& tiger = tigerIt->second;
                
                // 이미 죽은 호랑이는 처리하지 않음
                if (tiger.isDead) {
                    std::cout << "[TigerHit] Tiger " << pkt->tigerID << " already dead, ignoring hit" << std::endl;
                    break;
                }
                
                // 서버에서 직접 생명력 감소 (클라이언트 생명력 신뢰하지 않음)
                if (tiger.life > 0) {
                    // 클라이언트에서 전송한 데미지 정보 사용
                    int damage = pkt->damage; // 패킷에서 데미지 정보 가져오기
                    
                    // 데미지 유효성 검사
                    if (damage <= 0 || damage > 10) { // 비정상적인 데미지 값 방지
                        damage = 1;
                        std::cout << "[TigerHit] Invalid damage value, using default: 1" << std::endl;
                    }
                    
                    std::cout << "[TigerHit] Tiger " << pkt->tigerID << " hit with damage: " << damage << std::endl;
                    
                    tiger.life -= damage;
                    if (tiger.life < 0) tiger.life = 0; // 음수가 되지 않도록 보호
                    
                    tiger.isHitted = true;
                    
                    if (tiger.life <= 0) {
                        // 호랑이 사망 - 완전히 죽은 상태로 표시
                        tiger.isDead = true;
                        tiger.currentAnimation = "0208_tiger_dying.fbx";
                        tiger.animationTime = 0.0f;
                        tiger.elapseTime = 0.0f;
                        std::cout << "[TigerHit] Tiger " << pkt->tigerID << " died permanently" << std::endl;
                    } else {
                        // 호랑이 피격
                        tiger.currentAnimation = "0208_tiger_hit.fbx";
                        tiger.animationTime = 0.0f;
                        tiger.elapseTime = 0.0f;
                        std::cout << "[TigerHit] Tiger " << pkt->tigerID << " hit, remaining life: " << tiger.life << std::endl;
                    }
                    
                    // 서버에서 계산된 생명력으로 패킷 업데이트
                    pkt->life = tiger.life;
                    
                    // 다른 클라이언트들에게 호랑이 상태 업데이트 브로드캐스트
                    BroadcastPacket(pkt, sizeof(PacketTigerHit));
                } else {
                    std::cout << "[TigerHit] Tiger " << pkt->tigerID << " already dead, ignoring hit" << std::endl;
                }
            } else {
                std::cout << "[Error] Tiger " << pkt->tigerID << " not found for hit update" << std::endl;
            }
            break;
        }
        
        case PACKET_TIGER_RESPAWN_REQUEST: {
            std::cout << "[TigerRespawn] Received PACKET_TIGER_RESPAWN_REQUEST from client " << clientID << std::endl;
            std::cout << "[TigerRespawn] Packet size: " << header->size << ", Expected: " << sizeof(PacketTigerRespawnRequest) << std::endl;
            if (header->size != sizeof(PacketTigerRespawnRequest)) {
                std::cout << "[Error] Invalid TIGER_RESPAWN_REQUEST packet size" << std::endl;
                break;
            }
            PacketTigerRespawnRequest* pkt = (PacketTigerRespawnRequest*)buffer;
            std::cout << "[TigerRespawn] Client " << clientID << " requested tiger respawn" << std::endl;
            
            // 클라이언트에게 모든 호랑이 스폰 패킷을 다시 전송
            for (const auto& [tigerID, tiger] : m_tigers) {
                // 죽은 호랑이는 제외
                if (tiger.isDead) {
                    std::cout << "[TigerRespawn] Skipping dead tiger " << tiger.tigerID << " for respawn" << std::endl;
                    continue;
                }
                
                PacketTigerSpawn tigerPacket;
                tigerPacket.header.type = PACKET_TIGER_SPAWN;
                tigerPacket.header.size = sizeof(PacketTigerSpawn);
                tigerPacket.tigerID = tiger.tigerID;
                tigerPacket.x = tiger.x;
                tigerPacket.y = tiger.y;
                tigerPacket.z = tiger.z;
                
                auto clientIt = m_clients.find(clientID);
                if (clientIt != m_clients.end()) {
                    if (!SendPacket(clientIt->second.socket, &tigerPacket, sizeof(PacketTigerSpawn))) {
                    std::cout << "[Error] Failed to send tiger respawn packet for ID: " << tiger.tigerID << std::endl;
                    break;
                }
                std::cout << "[Success] Sent tiger respawn packet for ID: " << tiger.tigerID << std::endl;
                    } else {
                        std::cout << "[Error] Client " << clientID << " not found for tiger respawn" << std::endl;
                        break;
                    }
                }
                
                // 각 호랑이 스폰 사이에 짧은 지연 추가
                Sleep(50);
         }
            
            std::cout << "[TigerRespawn] Completed sending all tiger respawn packets to client " << clientID << std::endl;
            break;
        
        case PACKET_CLIENT_READY: {
            if (header->size != sizeof(PacketClientReady)) {
                std::cout << "[Error] Invalid CLIENT_READY packet size" << std::endl;
                break;
            }
            PacketClientReady* pkt = (PacketClientReady*)buffer;
            std::cout << "[ClientReady] Client " << clientID << " is ready to receive game data" << std::endl;
            
            // 클라이언트가 준비되었으므로 기존 플레이어 정보 전송
            std::cout << "[ClientReady] Client " << clientID << " ready, waiting for stage change to Hunting" << std::endl;
            
            // 클라이언트 상태 최종 확인
            if (m_clients.find(clientID) != m_clients.end() && 
                m_clients[clientID].socket != INVALID_SOCKET) {
                std::cout << "[ClientReady] Client " << clientID << " successfully ready" << std::endl;
                
                // 기존 플레이어 정보 전송
                BroadcastNewPlayer(clientID);
                
                // 호랑이 스테이지가 활성화된 경우 호랑이 정보도 전송
                if (m_huntingStageActive && !m_tigers.empty()) {
                    std::cout << "[ClientReady] Hunting stage active, sending tiger info to client " << clientID << std::endl;
                    SendExistingTigersToClient(clientID);
                }
            } else {
                std::cout << "[Error] Client " << clientID << " disconnected after ready" << std::endl;
            }
            break;
        }
        
        case PACKET_STAGE_CHANGE: {
            if (header->size != sizeof(PacketStageChange)) {
                std::cout << "[Error] Invalid STAGE_CHANGE packet size" << std::endl;
                break;
            }
            PacketStageChange* pkt = (PacketStageChange*)buffer;
            
            // 클라이언트 존재 여부 확인
            auto clientIt = m_clients.find(clientID);
            if (clientIt == m_clients.end()) {
                std::cout << "[Error] Client " << clientID << " not found for STAGE_CHANGE" << std::endl;
                break;
            }
            
            std::cout << "[StageChange] Client " << clientID << " (" << clientIt->second.username << ") changed to stage: " << pkt->stageName << std::endl;
            std::cout << "[StageChange] Total clients before stage change: " << m_clients.size() << std::endl;
            
            // 클라이언트의 스테이지 정보 업데이트
            UpdateClientStage(clientID, pkt->stageName);
            
            // Hunting 스테이지로 변경된 경우 호랑이 초기화
            if (strcmp(pkt->stageName, "Hunting") == 0) {
                ActivateHuntingStage();
                
                // 새로 Hunting Stage에 진입한 클라이언트에게 기존 호랑이 정보 전송
                if (m_huntingStageActive && !m_tigers.empty()) {
                    std::cout << "[StageChange] Sending existing tiger info to client " << clientID << " entering Hunting Stage" << std::endl;
                    SendExistingTigersToClient(clientID);
                }
            }
            
            // God 스테이지로 변경된 경우 퍼즐 상태 전송
            if (strcmp(pkt->stageName, "God") == 0) {
                // 서버에서 생성된 랜덤 퍼즐 패턴을 클라이언트에게 전송
                SendPuzzleStatusToClient(clientID);
                std::cout << "[StageChange] Client " << clientID << " entered God stage - sending server-generated puzzle pattern" << std::endl;
            }
            
            std::cout << "[StageChange] Total clients after stage change: " << m_clients.size() << std::endl;
            break;
        }
        
        case PACKET_PUZZLE_UPDATE: {
            if (header->size != sizeof(PacketPuzzleUpdate)) {
                std::cout << "[Error] Invalid PUZZLE_UPDATE packet size" << std::endl;
                break;
            }
            PacketPuzzleUpdate* pkt = (PacketPuzzleUpdate*)buffer;
            
            std::cout << "[Puzzle] Client " << clientID << " updated puzzle status" << std::endl;
            
            // 서버의 퍼즐 상태 업데이트 및 다른 클라이언트들과 동기화
            UpdatePuzzleStatus(clientID, pkt->puzzleStatus);
            break;
        }
        
        case PACKET_RICE_CAKE_SPAWN: {
            if (header->size != sizeof(PacketRiceCakeSpawn)) {
                std::cout << "[Error] Invalid RICE_CAKE_SPAWN packet size" << std::endl;
                break;
            }
            PacketRiceCakeSpawn* pkt = (PacketRiceCakeSpawn*)buffer;
            
            std::cout << "[RiceCake] Client " << clientID << " spawned rice cake projectile " << pkt->projectileID << std::endl;
            
            // 다른 클라이언트들에게 떡 발사체 스폰 정보 브로드캐스트
            BroadcastRiceCakeSpawn(clientID, pkt->projectileID, pkt->x, pkt->y, pkt->z, 
                                 pkt->dirX, pkt->dirY, pkt->dirZ, pkt->speed);
            break;
        }
        
        case PACKET_RICE_CAKE_UPDATE: {
            if (header->size != sizeof(PacketRiceCakeUpdate)) {
                std::cout << "[Error] Invalid RICE_CAKE_UPDATE packet size" << std::endl;
                break;
            }
            PacketRiceCakeUpdate* pkt = (PacketRiceCakeUpdate*)buffer;
            
            // 다른 클라이언트들에게 떡 발사체 위치 업데이트 브로드캐스트
            BroadcastRiceCakeUpdate(clientID, pkt->projectileID, pkt->x, pkt->y, pkt->z);
            break;
        }
        
        case PACKET_LEATHER_COUNT_UPDATE: {
            if (header->size != sizeof(PacketLeatherCountUpdate)) {
                std::cout << "[Error] Invalid LEATHER_COUNT_UPDATE packet size" << std::endl;
                break;
            }
            PacketLeatherCountUpdate* pkt = (PacketLeatherCountUpdate*)buffer;
            
            // 클라이언트의 호랑이 가죽 개수 업데이트 및 동기화
            UpdateClientLeatherCount(clientID, pkt->leatherCount);
            break;
        }
        
        default:
            std::cout << "  -> Unknown packet type" << std::endl;
            break;
    }
}

void GameServer::Cleanup() {
    m_isRunning = false;

    for (auto& [id, client] : m_clients) {
        closesocket(client.socket);
    }
    m_clients.clear();

    if (m_listenSocket != INVALID_SOCKET) {
        closesocket(m_listenSocket);
        m_listenSocket = INVALID_SOCKET;
    }

    for (HANDLE hThread : m_workerThreads) {
        WaitForSingleObject(hThread, INFINITE);
        CloseHandle(hThread);
    }
    m_workerThreads.clear();

    if (m_hIOCP) {
        CloseHandle(m_hIOCP);
        m_hIOCP = NULL;
    }

    WSACleanup();
}

// [Broadcast] 관련 반복 로그 주석 처리
void GameServer::BroadcastPacket(const void* packet, int size, int excludeID) {
    // Player 테스트를 위해 간소화된 브로드캐스트
    for (auto& [id, client] : m_clients) {
        if (!client.isLoggedIn || client.socket == INVALID_SOCKET || id == excludeID)
            continue;
            
        if (!SendPacket(client.socket, packet, size)) {
            // 주기적 로그 제거 - 에러 상황에서만 로그 출력
            continue;
        }
    }
}

void GameServer::ProcessNewClient(SOCKET clientSocket) {
    int clientID = m_nextClientID++;
    std::cout << "[Info] ProcessNewClient" << std::endl;
    
    ClientInfo newClient;
    newClient.socket = clientSocket;
    newClient.clientID = clientID;
    newClient.username = "";
    newClient.isLoggedIn = false;
    newClient.lastUpdate = { 0 };
    newClient.packetBufferSize = 0;  // 패킷 버퍼 초기화
    memset(newClient.packetBuffer, 0, sizeof(newClient.packetBuffer));
    
    // 1. IOCP 설정
    if (CreateIoCompletionPort((HANDLE)clientSocket, m_hIOCP, clientID, 0) == NULL) {
        std::cout << "[Error] Failed to associate with IOCP" << std::endl;
        closesocket(clientSocket);
        return;
    }
    
    // 2. 수신 시작 - 먼저 시작
    IOContext* ioContext = new IOContext();
    if (!StartReceive(clientSocket, clientID, ioContext)) {
        std::cout << "[Error] Failed to start receive" << std::endl;
        delete ioContext;
        closesocket(clientSocket);
        return;
    }
    
    // 3. 클라이언트 맵에 추가
    m_clients[clientID] = newClient;
    std::cout << "[ProcessNewClient] " << clientID << " added to map. Total clients: " << m_clients.size() << std::endl;
    
    // 4. 로그인 대기 상태로 설정 (호랑이 스폰 패킷은 로그인 성공 후에 전송)
    std::cout << "[ProcessNewClient] Client " << clientID << " waiting for login..." << std::endl;
}

bool GameServer::SendPacket(SOCKET socket, const void* packet, int size) {
    if (socket == INVALID_SOCKET) return false;
    
    // Player 테스트를 위해 간소화된 send
    int ret = send(socket, (const char*)packet, size, 0);
    if (ret == SOCKET_ERROR) {
        int err = WSAGetLastError();
        
        // 연결 관련 에러는 즉시 false 반환
        if (err == WSAECONNRESET || err == WSAECONNABORTED || err == WSAENOTSOCK) {
            // 클라이언트 ID를 찾아서 연결 에러 로그 상태 확인
            for (auto& [id, client] : m_clients) {
                if (client.socket == socket) {
                    if (!client.connectionErrorLogged) {
                        std::cout << "[SendPacket] Connection error (Client: " << id << ", Error: " << err << ")" << std::endl;
                        client.connectionErrorLogged = true;
                    }
                    break;
                }
            }
            return false;
        }
        
        // 기타 에러도 false 반환
        std::cout << "[SendPacket] Send failed (socket: " << socket << ", Error: " << err << ")" << std::endl;
        return false;
    }
    
    return true;
}

bool GameServer::StartReceive(SOCKET clientSocket, int clientID, IOContext* ioContext) {
    memset(&ioContext->overlapped, 0, sizeof(OVERLAPPED));
    ioContext->wsaBuf.buf = ioContext->buffer;
    ioContext->wsaBuf.len = sizeof(ioContext->buffer);
    ioContext->flags = 0;

    DWORD recvBytes;
    if (WSARecv(clientSocket, &ioContext->wsaBuf, 1, &recvBytes,
        &ioContext->flags, &ioContext->overlapped, NULL) == SOCKET_ERROR) {
        int error = WSAGetLastError();
        if (error != ERROR_IO_PENDING && error != WSAEWOULDBLOCK) {
            std::cout << "[Error] Initial WSARecv failed with error: " << error << std::endl;
            return false;
        }
    }
    return true;
}

void GameServer::BroadcastNewPlayer(int newClientID) {
    std::cout << "\n[BroadcastNewPlayer] New client ID: " << newClientID << std::endl;
    
    // 새 클라이언트가 로그인되었는지 확인
    if (m_clients.find(newClientID) == m_clients.end() || !m_clients[newClientID].isLoggedIn) {
        std::cout << "[BroadcastNewPlayer] Client " << newClientID << " is not logged in yet, skipping" << std::endl;
        return;
    }
    
    // 새 클라이언트 소켓 상태 확인
    if (m_clients[newClientID].socket == INVALID_SOCKET) {
        std::cout << "[BroadcastNewPlayer] Client " << newClientID << " socket is invalid, skipping" << std::endl;
        return;
    }
    
    try {
        // Player 테스트를 위해 간소화된 브로드캐스트
        // 기존 플레이어 정보를 새 클라이언트에게 전송
        for (const auto& [id, client] : m_clients) {
            if (id != newClientID && client.isLoggedIn && client.socket != INVALID_SOCKET) {
                PacketPlayerSpawn existingClientPacket;
                existingClientPacket.header.type = PACKET_PLAYER_SPAWN;
                existingClientPacket.header.size = sizeof(PacketPlayerSpawn);
                existingClientPacket.playerID = id;
                strncpy_s(existingClientPacket.username, client.username.c_str(), sizeof(existingClientPacket.username) - 1);
                
                // 새 클라이언트 상태 재확인
                if (m_clients.find(newClientID) == m_clients.end() || 
                    m_clients[newClientID].socket == INVALID_SOCKET) {
                    std::cout << "[BroadcastNewPlayer] Client " << newClientID << " disconnected during broadcast, stopping" << std::endl;
                    return;
                }
                
                if (!SendPacket(m_clients[newClientID].socket, &existingClientPacket, sizeof(existingClientPacket))) {
                    std::cout << "[Error] Failed to send existing player info for ID: " << id << std::endl;
                    continue;
                }
                std::cout << "[BroadcastNewPlayer] Sent existing player " << id << " info to new client" << std::endl;
            }
        }
        
        // 새 클라이언트 정보를 다른 로그인된 클라이언트들에게 전송
        PacketPlayerSpawn newClientPacket;
        newClientPacket.header.type = PACKET_PLAYER_SPAWN;
        newClientPacket.header.size = sizeof(PacketPlayerSpawn);
        newClientPacket.playerID = newClientID;
        auto newClientIt = m_clients.find(newClientID);
        if (newClientIt != m_clients.end()) {
            strncpy_s(newClientPacket.username, newClientIt->second.username.c_str(), sizeof(newClientPacket.username) - 1);
        } else {
            std::cout << "[Error] Client " << newClientID << " not found for username copy" << std::endl;
            return;
        }
        
        // 간단한 브로드캐스트 (에러 처리 개선)
        int broadcastCount = 0;
        for (auto& [id, client] : m_clients) {
            if (id != newClientID && client.isLoggedIn && client.socket != INVALID_SOCKET) {
                if (SendPacket(client.socket, &newClientPacket, sizeof(newClientPacket))) {
                    broadcastCount++;
                    std::cout << "[BroadcastNewPlayer] Sent new player info to client " << id << std::endl;
                } else {
                    std::cout << "[BroadcastNewPlayer] Failed to send new player info to client " << id << std::endl;
                }
            }
        }
        
        // 호랑이 스테이지가 활성화된 경우 호랑이 정보도 전송
        if (m_huntingStageActive && !m_tigers.empty()) {
            std::cout << "[BroadcastNewPlayer] Hunting stage active, sending tiger info to new client " << newClientID << std::endl;
            std::cout << "[BroadcastNewPlayer] Total tigers in server: " << m_tigers.size() << std::endl;
            SendExistingTigersToClient(newClientID);
        } else {
            std::cout << "[BroadcastNewPlayer] Hunting stage not active or no tigers. Active: " << m_huntingStageActive << ", Tigers: " << m_tigers.size() << std::endl;
        }
        
        std::cout << "[BroadcastNewPlayer] Completed sending info to new client. Broadcasted to " << broadcastCount << " clients" << std::endl;
    }
    catch (const std::exception& e) {
        std::cout << "[Error] Exception in BroadcastNewPlayer: " << e.what() << std::endl;
    }
    catch (...) {
        std::cout << "[Error] Unknown exception in BroadcastNewPlayer" << std::endl;
    }
}

void GameServer::InitializeTigers() {
    std::cout << "\n[InitializeTigers] Starting tiger initialization..." << std::endl;
    
    // Original과 동일한 호랑이 생성 위치
    float basePosX = 500.0f;
    float basePosZ = 500.0f;
    float offset = 100.0f;
    int repeat = 4;
    
    // 4x4 그리드로 16마리 호랑이 생성 (Original과 동일)
    for (int i = 0; i < repeat; ++i) {
        for (int j = 0; j < repeat; ++j) {
            TigerInfo tiger;
            tiger.tigerID = m_nextTigerID++;
            
            // Original과 동일한 위치에 배치
            tiger.x = basePosX + offset * j;
            tiger.y = 0.0f;
            tiger.z = basePosZ + offset * i;
            tiger.rotY = 0.0f;  // 초기 회전값
            tiger.moveTimer = 0.0f;
            tiger.isChasing = false;
            tiger.targetClientID = -1;  // 초기에는 추적 중인 클라이언트 없음
            tiger.currentAnimation = "0113_tiger_walk.fbx";  // Original과 동일한 초기 애니메이션
            tiger.animationTime = 0.0f;
            tiger.attackTime = 0.0f;
            tiger.searchTime = 0.0f;
            tiger.elapseTime = 0.0f;
            tiger.isFired = false;
            tiger.isHitted = false;  // Original과 동일
            tiger.life = 3;          // Original과 동일
            tiger.isDead = false;    // 초기에는 살아있는 상태
            tiger.hitProtectionTimer = 0.0f;  // hit 보호 타이머 초기화
            tiger.attackDelayTimer = 0.0f;    // 공격 후 딜레이 타이머 초기화
            
            // 초기 목표 위치 설정
            tiger.targetX = tiger.x;
            tiger.targetZ = tiger.z;
            
            m_tigers[tiger.tigerID] = tiger;

            std::cout << "[Tiger] Created tiger ID: " << tiger.tigerID 
                      << " at position (" << tiger.x << ", " << tiger.y << ", " << tiger.z << ")"
                      << " with rotation " << tiger.rotY << " degrees" << std::endl;
        }
    }
    
    std::cout << "[InitializeTigers] Completed. Total tigers created: " << m_tigers.size() << std::endl;
    std::cout << "[InitializeTigers] Note: Tiger spawn packets will be sent when clients log in" << std::endl;
}



void GameServer::UpdateTigerBehavior(TigerInfo& tiger, float deltaTime) {
    // Original과 동일한 상수값 사용
    const float CHASE_RADIUS = 200.0f;
    const float ATTACK_RADIUS = 17.0f;
    const float WALK_SPEED = 20.0f;  // Original과 동일하게 수정
    const float RUN_SPEED = 35.0f;   // Original과 동일하게 수정
    
    // 공격 후 딜레이 타이머가 활성화된 동안에는 위치 업데이트를 완전히 차단
    if (tiger.attackDelayTimer > 0.0f) {
        // 딜레이 타이머만 업데이트하고 즉시 반환
        tiger.attackDelayTimer -= deltaTime;
        return;
    }
    
    // 애니메이션 시간 업데이트 (모든 애니메이션에 대해)
    tiger.animationTime += deltaTime;
    
    // 타이머 업데이트 (Original CalcTime과 동일)
    if (tiger.currentAnimation == "0113_tiger_walk.fbx") {
        tiger.searchTime += deltaTime;
    }
    
    if (tiger.currentAnimation == "0208_tiger_attack.fbx") {
        tiger.elapseTime += deltaTime;
        // 공격 발사 타이밍 (Original과 동일)
        if (tiger.elapseTime >= 0.4f && !tiger.isFired) {
            tiger.isFired = true;
            // 공격 패킷 전송
            PacketTigerAttack attackPacket;
            attackPacket.header.type = PACKET_TIGER_ATTACK;
            attackPacket.header.size = sizeof(PacketTigerAttack);
            attackPacket.tigerID = tiger.tigerID;
            attackPacket.x = tiger.x;
            attackPacket.y = tiger.y;
            attackPacket.z = tiger.z;
            attackPacket.rotY = tiger.rotY;
            BroadcastPacket(&attackPacket, sizeof(attackPacket));
        }
        // 공격 종료 타이밍 (Original과 동일)
        if (tiger.elapseTime >= 0.8f) {
            tiger.currentAnimation = "0722_tiger_idle2.fbx";
            tiger.animationTime = 0.0f;
            tiger.elapseTime = 0.0f;
            tiger.isFired = false;
            tiger.attackTime = 0.0f;
            // 공격 후 딜레이 타이머 시작
            tiger.attackDelayTimer = 1.5f;
        }
    } else {
        tiger.attackTime += deltaTime;
    }
    
    if (tiger.currentAnimation == "0208_tiger_hit.fbx") {
        tiger.elapseTime += deltaTime;
        if (tiger.elapseTime > 0.8f) {
            tiger.currentAnimation = "0722_tiger_idle2.fbx";
            tiger.animationTime = 0.0f;
            tiger.elapseTime = 0.0f;
            tiger.isHitted = false;
            tiger.hitProtectionTimer = 2.0f;  // 2초 동안 보호
            // hit 후에도 딜레이 타이머 설정
            tiger.attackDelayTimer = 1.5f;
        }
    }
    
    // hit 보호 타이머 업데이트
    if (tiger.hitProtectionTimer > 0.0f) {
        tiger.hitProtectionTimer -= deltaTime;
    }
    
    // 공격 후 딜레이 타이머는 함수 시작 부분에서 처리됨
    
    if (tiger.currentAnimation == "0208_tiger_dying.fbx") {
        tiger.elapseTime += deltaTime;
        if (tiger.elapseTime > 1.9f) {
            // 호랑이 사망 처리 (가죽 생성 등은 클라이언트에서 처리)
            tiger.life = 0;
        }
    }
    
    // 가장 가까운 플레이어 찾기 (수정된 로직)
    float nearestDist = FLT_MAX;
    float targetX = tiger.x, targetZ = tiger.z;
    int nearestClientID = -1;
    
    // 이미 추적 중인 클라이언트가 있는지 확인
    if (tiger.targetClientID != -1 && m_clients.find(tiger.targetClientID) != m_clients.end()) {
        auto targetClientIt = m_clients.find(tiger.targetClientID);
        if (targetClientIt != m_clients.end()) {
            const auto& targetClient = targetClientIt->second;
        if (targetClient.isLoggedIn) {
            // 추적 중인 클라이언트가 여전히 로그인되어 있음
            
            // 추적 중인 클라이언트가 Hunting 스테이지에 있는지 확인
            if (targetClient.currentStage != "Hunting") {
                // Hunting 스테이지가 아니면 추적 중단
                int oldTargetID = tiger.targetClientID;
                tiger.targetClientID = -1;
                tiger.isChasing = false;
                OutputDebugStringA(("[Tiger] " + std::to_string(tiger.tigerID) + " stopped chasing client " + std::to_string(oldTargetID) + " (left Hunting stage)\n").c_str());
            } else {
                float dx = targetClient.lastUpdate.x - tiger.x;
                float dz = targetClient.lastUpdate.z - tiger.z;
                float distSq = dx * dx + dz * dz;
                float dist = sqrt(distSq);
                
                // 추적 중인 클라이언트가 탐색 범위 밖으로 나갔는지 확인
                if (dist > CHASE_RADIUS * 1.5f) {  // 여유를 두어 탐색 범위를 약간 넘어서도 추적 유지
                    // 추적 중인 클라이언트가 너무 멀리 갔으면 추적 중단
                    int oldTargetID = tiger.targetClientID;
                    tiger.targetClientID = -1;
                    tiger.isChasing = false;
                    OutputDebugStringA(("[Tiger] " + std::to_string(tiger.tigerID) + " stopped chasing client " + std::to_string(oldTargetID) + " (too far)\n").c_str());
                } else {
                    // 추적 중인 클라이언트 계속 추적
                    nearestDist = distSq;
                    targetX = targetClient.lastUpdate.x;
                    targetZ = targetClient.lastUpdate.z;
                    nearestClientID = tiger.targetClientID;
                }
            }
        } else {
            // 추적 중인 클라이언트가 로그아웃했으면 추적 중단
            int oldTargetID = tiger.targetClientID;
            tiger.targetClientID = -1;
            tiger.isChasing = false;
            OutputDebugStringA(("[Tiger] " + std::to_string(tiger.tigerID) + " stopped chasing client " + std::to_string(oldTargetID) + " (logged out)\n").c_str());
        }
        }
    } else if (tiger.targetClientID != -1) {
        // 추적 중인 클라이언트가 더 이상 존재하지 않음 (연결 해제 등)
        int oldTargetID = tiger.targetClientID;
        OutputDebugStringA(("[Tiger] " + std::to_string(tiger.tigerID) + " stopped chasing client " + std::to_string(oldTargetID) + " (client disconnected)\n").c_str());
        tiger.targetClientID = -1;
        tiger.isChasing = false;
    }
    
    // 추적 중인 클라이언트가 없거나 추적을 중단한 경우, 새로운 타겟 찾기
    if (nearestClientID == -1) {
        for (const auto& [id, client] : m_clients) {
            if (!client.isLoggedIn) continue;
            
            // Hunting 스테이지에 있는 클라이언트만을 대상으로 함
            if (client.currentStage != "Hunting") continue;
            
            float dx = client.lastUpdate.x - tiger.x;
            float dz = client.lastUpdate.z - tiger.z;
            float distSq = dx * dx + dz * dz;
            if (distSq < nearestDist) {
                nearestDist = distSq;
                targetX = client.lastUpdate.x;
                targetZ = client.lastUpdate.z;
                nearestClientID = id;
            }
        }
        
        // 새로운 타겟을 찾았고 탐색 범위 안에 있으면 추적 시작
        if (nearestClientID != -1) {
            float dist = sqrt(nearestDist);
            if (dist < CHASE_RADIUS) {
                tiger.targetClientID = nearestClientID;
                auto newTargetClientIt = m_clients.find(nearestClientID);
                if (newTargetClientIt != m_clients.end()) {
                    const auto& newTargetClient = newTargetClientIt->second;
                OutputDebugStringA(("[Tiger] " + std::to_string(tiger.tigerID) + " started chasing client " + std::to_string(nearestClientID) + 
                    " at distance " + std::to_string(dist) + " (pos: " + std::to_string(newTargetClient.lastUpdate.x) + 
                    ", " + std::to_string(newTargetClient.lastUpdate.z) + ")\n").c_str());
                }
            }
        }
    }
    
    float dist = sqrt(nearestDist);
    
    // Original TigerBehavior 로직과 동일하게 구현
    if (dist < CHASE_RADIUS) {  // 플레이어가 탐색 범위 안에 있을 때
        tiger.isChasing = true;
        
        // 공격 범위 내에 있는지 확인 (추적 중인 클라이언트 기준)
        if (tiger.targetClientID != -1 && dist < ATTACK_RADIUS) {  // 공격 범위 안이면
            // Attack() 함수 로직 (Original과 동일)
            if (tiger.currentAnimation != "0208_tiger_attack.fbx" && 
                tiger.currentAnimation != "0208_tiger_hit.fbx" && 
                tiger.attackTime >= 2.0f &&
                tiger.hitProtectionTimer <= 0.0f) {  // hit 보호 타이머가 만료된 후에만 attack 허용
                
                if (tiger.currentAnimation != "0208_tiger_attack.fbx") {
                    tiger.currentAnimation = "0208_tiger_attack.fbx";
                    tiger.animationTime = 0.0f;
                    tiger.elapseTime = 0.0f;
                    tiger.isFired = false;
                    
                    // 공격 시작 시 로그 출력
                    OutputDebugStringA(("[Tiger] " + std::to_string(tiger.tigerID) + " started attacking client " + std::to_string(tiger.targetClientID) + "\n").c_str());
                }
            }
            
            // 공격 애니메이션 중 방향 설정 (추적 중인 클라이언트를 향해)
            if (tiger.currentAnimation == "0208_tiger_attack.fbx" && tiger.elapseTime == 0.0f) {
                float dx = targetX - tiger.x;
                float dz = targetZ - tiger.z;
                tiger.rotY = atan2(dx, dz) * (180.0f / 3.141592f);
            }
        } else if (tiger.targetClientID != -1) {  // 공격 범위 밖이지만 추적 중인 클라이언트가 있으면
            // Run() 함수 로직 (추적 중인 클라이언트를 향해 달리기)
            if (tiger.currentAnimation != "0208_tiger_attack.fbx" && 
                tiger.currentAnimation != "0208_tiger_hit.fbx" && 
                tiger.currentAnimation != "0208_tiger_dying.fbx" && 
                tiger.attackTime >= 2.0f &&
                tiger.hitProtectionTimer <= 0.0f) {  // hit 보호 타이머가 만료된 후에만 run 허용
                
                if (tiger.currentAnimation != "0722_tiger_run.fbx") {
                    tiger.currentAnimation = "0722_tiger_run.fbx";
                    tiger.animationTime = 0.0f;
                    
                    // 달리기 시작 시 로그 출력
                    OutputDebugStringA(("[Tiger] " + std::to_string(tiger.tigerID) + " started running towards client " + std::to_string(tiger.targetClientID) + "\n").c_str());
                }
            }
            
            // 달리기 애니메이션 중일 때만 이동 (추적 중인 클라이언트를 향해)
            if (tiger.currentAnimation == "0722_tiger_run.fbx" && tiger.attackDelayTimer <= 0.0f) {
                float dx = targetX - tiger.x;
                float dz = targetZ - tiger.z;
                float moveDist = sqrt(dx * dx + dz * dz);
                if (moveDist > 0.1f) {
                    tiger.x += (dx / moveDist) * RUN_SPEED * deltaTime;
                    tiger.z += (dz / moveDist) * RUN_SPEED * deltaTime;
                    tiger.rotY = atan2(dx, dz) * (180.0f / 3.141592f);
                }
            }
        } else {  // 추적 중인 클라이언트가 없거나 공격 범위 밖이면
            // idle 상태일 때는 움직이지 않음 (공격 후 딜레이 애니메이션이 끝날 때까지)
            if (tiger.currentAnimation == "0722_tiger_idle2.fbx") {
                // idle 애니메이션이 1.5초 재생된 후에만 walk 상태로 전환
                if (tiger.animationTime >= 1.5f && tiger.hitProtectionTimer <= 0.0f) {
                    tiger.currentAnimation = "0113_tiger_walk.fbx";
                    tiger.animationTime = 0.0f;
                }
            } else if (tiger.currentAnimation != "0113_tiger_walk.fbx" && tiger.hitProtectionTimer <= 0.0f) {
                tiger.currentAnimation = "0113_tiger_walk.fbx";
                tiger.animationTime = 0.0f;
            }
        }
    } else {  // 플레이어가 탐색 범위 밖에 있을 때
        // Search() 함수 로직 (수정된 로직)
        tiger.isChasing = false;
        
        // 추적 중인 클라이언트가 탐색 범위 밖으로 나갔으면 추적 중단
        if (tiger.targetClientID != -1) {
            int oldTargetID = tiger.targetClientID;
            tiger.targetClientID = -1;
            OutputDebugStringA(("[Tiger] " + std::to_string(tiger.tigerID) + " stopped chasing client " + std::to_string(oldTargetID) + " (out of range)\n").c_str());
        }
        
        if (tiger.searchTime > 2.0f) {  // Original과 동일한 2초
            tiger.searchTime = 0.0f;
            // 랜덤 방향 설정
            float randYaw = GetRandomFloat(-180.0f, 180.0f);
            tiger.rotY = randYaw;
            
            // 탐색 방향 변경 시 로그 출력
            OutputDebugStringA(("[Tiger] " + std::to_string(tiger.tigerID) + " changed search direction to " + std::to_string(randYaw) + " degrees\n").c_str());
        }
        
        // idle 상태일 때는 움직이지 않음 (공격 후 딜레이 애니메이션이 끝날 때까지)
        if (tiger.currentAnimation == "0722_tiger_idle2.fbx") {
            // idle 애니메이션이 1.5초 재생된 후에만 walk 상태로 전환
            if (tiger.animationTime >= 1.5f && tiger.hitProtectionTimer <= 0.0f) {
                tiger.currentAnimation = "0113_tiger_walk.fbx";
                tiger.animationTime = 0.0f;
            }
        } else {
            // 현재 방향으로 이동 (Original과 동일한 로직) - 딜레이 타이머가 만료된 후에만
            if (tiger.attackDelayTimer <= 0.0f) {
                float angleRad = tiger.rotY * (3.141592f / 180.0f);
                float dirX = sin(angleRad);
                float dirZ = cos(angleRad);
                
                tiger.x += dirX * WALK_SPEED * deltaTime;
                tiger.z += dirZ * WALK_SPEED * deltaTime;
            }
            
            if (tiger.currentAnimation != "0113_tiger_walk.fbx" && tiger.hitProtectionTimer <= 0.0f) {
                tiger.currentAnimation = "0113_tiger_walk.fbx";
                tiger.animationTime = 0.0f;
            }
        }
    }
}

void GameServer::UpdateTigers(float deltaTime) {
    // Hunting 스테이지가 활성화된 경우에만 호랑이 업데이트
    if (!m_huntingStageActive) return;
    
    // deltaTime을 더 세밀하게 처리하여 부드러운 움직임 보장
    // 매우 작은 deltaTime 값도 정확하게 처리
    if (deltaTime > 0.0f && deltaTime < 0.1f) { // 100ms 이상의 큰 점프는 무시
        for (auto& tigerPair : m_tigers) {
            auto& tiger = tigerPair.second;
            UpdateTigerBehavior(tiger, deltaTime); // 실제 deltaTime 사용
        }
        
        BroadcastTigerUpdates();
    }
}

void GameServer::MonitorClientConnections() {
    static DWORD lastCheckTime = GetTickCount();
    DWORD currentTime = GetTickCount();
    
    if (currentTime - lastCheckTime > 5000) { // 5초마다 체크
        lastCheckTime = currentTime;
        
        for (auto it = m_clients.begin(); it != m_clients.end();) {
            if (it->second.socket == INVALID_SOCKET) {
                std::cout << "[Monitor] Removing invalid client " << it->first << " (" << it->second.username << ")" << std::endl;
                it = m_clients.erase(it);
            } else {
                std::cout << "[Monitor] Client " << it->first << " (" << it->second.username << ") - Socket: " << it->second.socket << ", LoggedIn: " << (it->second.isLoggedIn ? "Yes" : "No") << std::endl;
                ++it;
            }
        }
        
        std::cout << "[Monitor] Total active clients: " << m_clients.size() << std::endl;
    }
}

void GameServer::BroadcastTigerUpdates() {
    // Hunting 스테이지가 활성화되지 않았으면 업데이트 전송하지 않음
    if (!m_huntingStageActive) return;
    
    // 로그인된 클라이언트가 없으면 업데이트 전송하지 않음
    int loggedInCount = 0;
    for (const auto& clientPair : m_clients) {
        const auto& client = clientPair.second;
        if (client.isLoggedIn && client.socket != INVALID_SOCKET) {
            loggedInCount++;
        }
    }
    
    if (loggedInCount == 0) {
        return;
    }
    
    // 호랑이 업데이트 전송
    for (const auto& tigerPair : m_tigers) {
        const auto& tiger = tigerPair.second;
        PacketTigerUpdate updatePacket;
        updatePacket.header.type = PACKET_TIGER_UPDATE;
        updatePacket.header.size = sizeof(PacketTigerUpdate);
        updatePacket.tigerID = tiger.tigerID;
        updatePacket.x = tiger.x;
        updatePacket.y = tiger.y;
        updatePacket.z = tiger.z;
        updatePacket.rotY = tiger.rotY;
        
        // 애니메이션 정보 추가 (Original과 동일한 애니메이션 파일명)
        strcpy_s(updatePacket.animationFile, sizeof(updatePacket.animationFile), tiger.currentAnimation.c_str());
        updatePacket.animationTime = tiger.animationTime;
        
        BroadcastPacket(&updatePacket, sizeof(updatePacket));
    }
}



float GameServer::GetRandomFloat(float min, float max) {
    std::uniform_real_distribution<float> dist(min, max);
    return dist(m_randomEngine);
}

bool GameServer::IsPlayerNearby(const TigerInfo& tiger, float radius) {
    for (const auto& [id, client] : m_clients) {
        float dx = client.lastUpdate.x - tiger.x;
        float dz = client.lastUpdate.z - tiger.z;
        float distSq = dx * dx + dz * dz;
        if (distSq < radius * radius) {
            return true;
        }
    }
    return false;
}

void GameServer::GetNearestPlayerPosition(const TigerInfo& tiger, float& targetX, float& targetZ) {
    float nearestDist = FLT_MAX;
    targetX = tiger.x;
    targetZ = tiger.z;
    
    for (const auto& [id, client] : m_clients) {
        if (!client.isLoggedIn) continue;
        
        float dx = client.lastUpdate.x - tiger.x;
        float dz = client.lastUpdate.z - tiger.z;
        float distSq = dx * dx + dz * dz;
        if (distSq < nearestDist) {
            nearestDist = distSq;
            targetX = client.lastUpdate.x;
            targetZ = client.lastUpdate.z;
        }
    }
}

void GameServer::ActivateHuntingStage() {
    if (!m_huntingStageActive) {
        m_huntingStageActive = true;
        std::cout << "[Server] Hunting Stage activated - initializing tigers" << std::endl;
        
        // 호랑이 초기화
        InitializeTigers();
        
        // 모든 클라이언트에게 호랑이 스폰 패킷 전송
        std::cout << "[Server] Broadcasting tiger spawn packets to all clients..." << std::endl;
        std::cout << "[Server] Total tigers to broadcast: " << m_tigers.size() << std::endl;
        
        int broadcastCount = 0;
        for (const auto& tigerPair : m_tigers) {
            const auto& tiger = tigerPair.second;
            PacketTigerSpawn spawnPacket;
            spawnPacket.header.type = PACKET_TIGER_SPAWN;
            spawnPacket.header.size = sizeof(PacketTigerSpawn);
            spawnPacket.tigerID = tiger.tigerID;
            spawnPacket.x = tiger.x;
            spawnPacket.y = tiger.y;
            spawnPacket.z = tiger.z;
            
            BroadcastPacket(&spawnPacket, sizeof(spawnPacket));
            std::cout << "[Server] Sent tiger spawn packet for ID: " << tiger.tigerID << std::endl;
            broadcastCount++;
        }
        
        std::cout << "[Server] Tigers initialized and spawn packets sent to all clients. Total broadcasted: " << broadcastCount << std::endl;
    } else {
        std::cout << "[Server] Hunting Stage already active, skipping initialization" << std::endl;
    }
}

// 스테이지별 플레이어 관리 메서드들 추가
void GameServer::UpdateClientStage(int clientID, const std::string& stageName) {
    auto clientIt = m_clients.find(clientID);
    if (clientIt != m_clients.end()) {
        std::string oldStage = clientIt->second.currentStage;
        clientIt->second.currentStage = stageName;
        std::cout << "[StageChange] Client " << clientID << " (" << clientIt->second.username 
                  << ") changed from " << oldStage << " to " << stageName << std::endl;
    }
}

void GameServer::BroadcastToStage(const void* packet, int size, const std::string& stageName, int excludeID) {
    for (auto& [id, client] : m_clients) {
        if (!client.isLoggedIn || client.socket == INVALID_SOCKET || id == excludeID)
            continue;
            
        if (client.currentStage == stageName) {
            if (!SendPacket(client.socket, packet, size)) {
                continue;
            }
        }
    }
}

std::vector<int> GameServer::GetClientsInStage(const std::string& stageName) {
    std::vector<int> clientsInStage;
    for (const auto& [id, client] : m_clients) {
        if (client.isLoggedIn && client.currentStage == stageName) {
            clientsInStage.push_back(id);
        }
    }
    return clientsInStage;
}

// 퍼즐 관련 메서드들 구현
void GameServer::InitializePuzzle() {
    // 게임 시작 시 서버에서 랜덤 퍼즐 상태 생성
    std::random_device rd;
    std::default_random_engine dre(rd());
    std::uniform_int_distribution<int> uid(0, 1);
    
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            m_puzzleStatus[i][j] = uid(dre);
        }
    }
    
    m_puzzleInitialized = true;
    std::cout << "[Server] Puzzle initialized with random state:" << std::endl;
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            std::cout << m_puzzleStatus[i][j] << " ";
        }
        std::cout << std::endl;
    }
}

void GameServer::UpdatePuzzleStatus(int clientID, int puzzleStatus[3][3]) {
    // 클라이언트의 퍼즐 상태 변경을 추적 (로깅용)
    std::cout << "[Server] Client " << clientID << " updated puzzle status:" << std::endl;
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            std::cout << puzzleStatus[i][j] << " ";
        }
        std::cout << std::endl;
    }
    
    // 각 클라이언트는 자신만의 퍼즐 패턴을 가져야 하므로 다른 클라이언트들에게 브로드캐스트하지 않음
    // BroadcastPuzzleStatus(clientID); // 이 줄을 주석 처리
}

void GameServer::BroadcastPuzzleStatus(int excludeID) {
    PacketPuzzleSync syncPacket;
    syncPacket.header.type = PACKET_PUZZLE_SYNC;
    syncPacket.header.size = sizeof(PacketPuzzleSync);
    
    // 현재 퍼즐 상태 복사
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            syncPacket.puzzleStatus[i][j] = m_puzzleStatus[i][j];
        }
    }
    
    // God 스테이지에 있는 클라이언트 수 확인
    int clientsInGodStage = 0;
    for (const auto& [id, client] : m_clients) {
        if (client.isLoggedIn && client.currentStage == "God" && id != excludeID) {
            clientsInGodStage++;
        }
    }
    
    std::cout << "[Server] Clients in God stage: " << clientsInGodStage << " (excluding client " << excludeID << ")" << std::endl;
    
    // God 스테이지에 있는 모든 클라이언트에게 전송
    BroadcastToStage(&syncPacket, sizeof(syncPacket), "God", excludeID);
    
    std::cout << "[Server] Puzzle status broadcasted to all clients in God stage" << std::endl;
}

void GameServer::SendPuzzleStatusToClient(int clientID) {
    auto clientIt = m_clients.find(clientID);
    if (clientIt == m_clients.end() || !clientIt->second.isLoggedIn) {
        return;
    }
    
    PacketPuzzleSync syncPacket;
    syncPacket.header.type = PACKET_PUZZLE_SYNC;
    syncPacket.header.size = sizeof(PacketPuzzleSync);
    
    // 현재 퍼즐 상태 복사
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            syncPacket.puzzleStatus[i][j] = m_puzzleStatus[i][j];
        }
    }
    
    if (SendPacket(clientIt->second.socket, &syncPacket, sizeof(syncPacket))) {
        std::cout << "[Server] Puzzle status sent to client " << clientID << std::endl;
    }
}

// 떡 발사체 관련 메서드들 구현
void GameServer::BroadcastRiceCakeSpawn(int clientID, int projectileID, float x, float y, float z, float dirX, float dirY, float dirZ, float speed) {
    PacketRiceCakeSpawn spawnPacket;
    spawnPacket.header.type = PACKET_RICE_CAKE_SPAWN;
    spawnPacket.header.size = sizeof(PacketRiceCakeSpawn);
    spawnPacket.clientID = clientID;
    spawnPacket.projectileID = projectileID;
    spawnPacket.x = x;
    spawnPacket.y = y;
    spawnPacket.z = z;
    spawnPacket.dirX = dirX;
    spawnPacket.dirY = dirY;
    spawnPacket.dirZ = dirZ;
    spawnPacket.speed = speed;
    
    // 발사한 클라이언트를 제외한 모든 클라이언트에게 전송
    BroadcastPacket(&spawnPacket, sizeof(spawnPacket), clientID);
    
    std::cout << "[Server] Rice cake spawn broadcasted - Client: " << clientID 
              << ", Projectile: " << projectileID << std::endl;
}

void GameServer::BroadcastRiceCakeUpdate(int clientID, int projectileID, float x, float y, float z) {
    PacketRiceCakeUpdate updatePacket;
    updatePacket.header.type = PACKET_RICE_CAKE_UPDATE;
    updatePacket.header.size = sizeof(PacketRiceCakeUpdate);
    updatePacket.clientID = clientID;
    updatePacket.projectileID = projectileID;
    updatePacket.x = x;
    updatePacket.y = y;
    updatePacket.z = z;
    
    // 발사한 클라이언트를 제외한 모든 클라이언트에게 전송
    BroadcastPacket(&updatePacket, sizeof(updatePacket), clientID);
    
    std::cout << "[Server] Rice cake update broadcasted - Client: " << clientID 
              << ", Projectile: " << projectileID << std::endl;
}

void GameServer::SendExistingTigersToClient(int clientID) {
    auto clientIt = m_clients.find(clientID);
    if (clientIt == m_clients.end() || !clientIt->second.isLoggedIn) {
        std::cout << "[Error] Client " << clientID << " not found or not logged in for tiger info" << std::endl;
        return;
    }
    
    std::cout << "[Server] Sending existing tiger info to client " << clientID << " (total tigers: " << m_tigers.size() << ")" << std::endl;
    
    int sentCount = 0;
    int skippedCount = 0;
    
    // 모든 기존 호랑이 정보를 새 클라이언트에게 전송
    for (const auto& tigerPair : m_tigers) {
        const auto& tiger = tigerPair.second;
        
        // 죽은 호랑이는 전송하지 않음
        if (tiger.isDead) {
            std::cout << "[Server] Skipping dead tiger " << tiger.tigerID << " for client " << clientID << std::endl;
            skippedCount++;
            continue;
        }
        
        PacketTigerSpawn spawnPacket;
        spawnPacket.header.type = PACKET_TIGER_SPAWN;
        spawnPacket.header.size = sizeof(PacketTigerSpawn);
        spawnPacket.tigerID = tiger.tigerID;
        spawnPacket.x = tiger.x;
        spawnPacket.y = tiger.y;
        spawnPacket.z = tiger.z;
        
        if (SendPacket(clientIt->second.socket, &spawnPacket, sizeof(spawnPacket))) {
            std::cout << "[Server] Sent tiger " << tiger.tigerID << " info to client " << clientID 
                      << " at position (" << tiger.x << ", " << tiger.y << ", " << tiger.z << ")" << std::endl;
            sentCount++;
        } else {
            std::cout << "[Error] Failed to send tiger " << tiger.tigerID << " info to client " << clientID << std::endl;
        }
    }
    
    std::cout << "[Server] Completed sending tiger info to client " << clientID 
              << " - Sent: " << sentCount << ", Skipped: " << skippedCount << std::endl;
}

// 호랑이 가죽 관련 메서드들 구현
void GameServer::UpdateClientLeatherCount(int clientID, int leatherCount) {
    auto clientIt = m_clients.find(clientID);
    if (clientIt == m_clients.end()) {
        std::cout << "[Error] Client " << clientID << " not found for leather count update" << std::endl;
        return;
    }
    
    // 클라이언트의 호랑이 가죽 개수 업데이트
    clientIt->second.leatherCount = leatherCount;
    std::cout << "[Server] Updated client " << clientID << " leather count to " << leatherCount << std::endl;
    
    // 모든 클라이언트에게 동기화 패킷 전송
    BroadcastLeatherCountSync(leatherCount, clientID);
}

void GameServer::BroadcastLeatherCountSync(int leatherCount, int excludeID) {
    PacketLeatherCountSync syncPacket;
    syncPacket.header.type = PACKET_LEATHER_COUNT_SYNC;
    syncPacket.header.size = sizeof(PacketLeatherCountSync);
    syncPacket.leatherCount = leatherCount;
    
    // excludeID를 제외한 모든 클라이언트에게 전송
    BroadcastPacket(&syncPacket, sizeof(syncPacket), excludeID);
    
    std::cout << "[Server] Leather count sync broadcasted - Count: " << leatherCount 
              << " (excluded client: " << excludeID << ")" << std::endl;
}

void GameServer::SendLeatherCountToClient(int clientID) {
    auto clientIt = m_clients.find(clientID);
    if (clientIt == m_clients.end()) {
        std::cout << "[Error] Client " << clientID << " not found for leather count sync" << std::endl;
        return;
    }
    
    PacketLeatherCountSync syncPacket;
    syncPacket.header.type = PACKET_LEATHER_COUNT_SYNC;
    syncPacket.header.size = sizeof(PacketLeatherCountSync);
    syncPacket.leatherCount = clientIt->second.leatherCount;
    
    if (SendPacket(clientIt->second.socket, &syncPacket, sizeof(syncPacket))) {
        std::cout << "[Server] Leather count " << clientIt->second.leatherCount 
                  << " sent to client " << clientID << std::endl;
    } else {
        std::cout << "[Error] Failed to send leather count to client " << clientID << std::endl;
    }
}

int main() {
    GameServer server;
    
    if (!server.Initialize(5000)) {  // 포트 번호 지정 가능
        std::cout << "[Error] Server initialization failed" << std::endl;
        return 1;
    }

    server.Start();
    return 0;
}