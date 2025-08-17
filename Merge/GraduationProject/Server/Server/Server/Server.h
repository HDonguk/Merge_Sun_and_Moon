//#pragma once

#pragma comment(lib, "ws2_32.lib")

#define NOMINMAX
#include <winsock2.h>
#include <windows.h>
#include <mswsock.h>
#include <unordered_map>
#include <vector>
#include <random>
#include "Packet.h"

class GameServer {
public:
    GameServer();
    ~GameServer();

    bool Initialize(int port = 5000);
    void Start();
    void Stop();

private:
    static constexpr int MAX_CLIENTS = 2;
    static constexpr int MAX_PACKET_SIZE = 1024;
    static constexpr int MAX_TIGERS = 5;   // 성능 개선을 위해 5마리로 줄임

    struct ClientInfo {
        SOCKET socket;
        int clientID;
        std::string username;
        bool isLoggedIn;
        PacketPlayerUpdate lastUpdate;
        int sendFailCount = 0; // 송신 실패 횟수
        int connectionErrorCount = 0; // 연결 에러 횟수
        bool connectionErrorLogged = false; // 연결 에러 로그 출력 여부
        std::string currentStage = "Base"; // 현재 스테이지 정보 추가
        int leatherCount = 0; // 호랑이 가죽 개수 추가
        
        // 패킷 버퍼링을 위한 추가 필드
        char packetBuffer[MAX_PACKET_SIZE * 4];  // 여러 패킷을 저장할 수 있는 버퍼
        int packetBufferSize = 0;  // 현재 패킷 버퍼에 저장된 데이터 크기
    };

    struct TigerInfo {
        int tigerID;
        float x, y, z;
        float rotY;
        float targetX, targetZ;  // 목표 위치
        float moveTimer;         // 이동 타이머
        bool isChasing;         // 플레이어 추적 여부
        int targetClientID;     // 추적 중인 클라이언트 ID (-1이면 추적 중이 아님)
        std::string currentAnimation;  // 현재 애니메이션 파일명
        float animationTime;     // 애니메이션 시간
        float attackTime;        // 공격 타이머 (원본과 동일)
        float searchTime;        // 탐색 타이머
        float elapseTime;        // 애니메이션 경과 시간
        bool isFired;           // 공격 발사 여부
        bool isHitted;          // 피격 상태 (Original과 동일)
        int life;               // 생명력 (Original과 동일)
        bool isDead;            // 영구적인 사망 상태 (한번 죽으면 부활하지 않음)
        float hitProtectionTimer;  // hit 애니메이션 후 보호 타이머
        float attackDelayTimer;    // 공격 후 딜레이 타이머
    };



    struct IOContext {
        OVERLAPPED overlapped;
        WSABUF wsaBuf;
        char buffer[MAX_PACKET_SIZE];
        DWORD flags;
    };

private:
    // 멤버 변수
    HANDLE m_hIOCP;
    int m_nextClientID;
    int m_nextTigerID;
    std::unordered_map<int, ClientInfo> m_clients;
    std::unordered_map<int, TigerInfo> m_tigers;
    SOCKET m_listenSocket;
    std::vector<HANDLE> m_workerThreads;
    bool m_isRunning;
    int m_port;
    std::mt19937 m_randomEngine;
    bool m_huntingStageActive;  // Hunting 스테이지 활성화 여부

    // 퍼즐 상태 관리 추가
    int m_puzzleStatus[3][3];      // God 스테이지 퍼즐 상태
    int m_targetPattern[3][3];     // 목표 퍼즐 패턴 (클라이언트가 맞춰야 할 패턴)
    bool m_puzzleInitialized;      // 퍼즐 초기화 여부

    // 내부 메서드
    static DWORD WINAPI WorkerThreadProc(LPVOID lpParam);
    DWORD WorkerThread();
    void Cleanup();
    void BroadcastPacket(const void* packet, int size, int excludeID = -1);
    void ProcessNewClient(SOCKET clientSocket);
    bool SendPacket(SOCKET socket, const void* packet, int size);
    bool StartReceive(SOCKET clientSocket, int clientID, IOContext* ioContext);
    void BroadcastNewPlayer(int newClientID);
    void HandlePacket(IOContext* ioContext, int clientID, DWORD bytesTransferred);
    void ProcessSinglePacket(char* buffer, int clientID, int packetSize);
    
    // 새로 연결된 클라이언트에게 기존 호랑이 정보 전송
    void SendExistingTigersToClient(int clientID);
    
    // 호랑이 관련 메서드
    void InitializeTigers();
    void UpdateTigers(float deltaTime);
    void BroadcastTigerUpdates();
    void UpdateTigerBehavior(TigerInfo& tiger, float deltaTime);
    void ActivateHuntingStage();
    void MonitorClientConnections();
    float GetRandomFloat(float min, float max);
    bool IsPlayerNearby(const TigerInfo& tiger, float radius);
    void GetNearestPlayerPosition(const TigerInfo& tiger, float& targetX, float& targetZ);
    
    // 스테이지별 플레이어 관리 메서드 추가
    void UpdateClientStage(int clientID, const std::string& stageName);
    void BroadcastToStage(const void* packet, int size, const std::string& stageName, int excludeID = -1);
    std::vector<int> GetClientsInStage(const std::string& stageName);
    
    // 퍼즐 관련 메서드 추가
    void InitializePuzzle();
    void UpdatePuzzleStatus(int clientID, int puzzleStatus[3][3]);
    void BroadcastPuzzleStatus(int excludeID = -1);
    void SendPuzzleStatusToClient(int clientID);
    void PeriodicPuzzleSync();  // 주기적 퍼즐 동기화 추가

    // 떡 발사체 관련 메서드 추가
    void BroadcastRiceCakeSpawn(int clientID, int projectileID, float x, float y, float z, float dirX, float dirY, float dirZ, float speed);
    void BroadcastRiceCakeUpdate(int clientID, int projectileID, float x, float y, float z);

    // 호랑이 가죽 관련 메서드 추가
    void UpdateClientLeatherCount(int clientID, int leatherCount);
    void BroadcastLeatherCountSync(int leatherCount, int excludeID = -1);
    void SendLeatherCountToClient(int clientID);

}; 