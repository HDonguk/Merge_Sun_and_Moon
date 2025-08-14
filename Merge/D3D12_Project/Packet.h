#pragma once
#include "stdafx.h"
#include <vector>

#pragma pack(push, 1)
struct PacketHeader {
    unsigned short size;
    unsigned short type;
};

enum PacketType {
    PACKET_PLAYER_UPDATE = 1,
    PACKET_PLAYER_SPAWN = 2,
    PACKET_TIGER_SPAWN = 3,    // 호랑이 스폰 패킷
    PACKET_TIGER_UPDATE = 4,   // 호랑이 업데이트 패킷
    PACKET_LOGIN_REQUEST = 6,  // 로그인 요청
    PACKET_LOGIN_RESPONSE = 7, // 로그인 응답
    PACKET_PLAYER_DISCONNECT = 8, // 플레이어 연결 해제
    PACKET_CLIENT_READY = 9,   // 클라이언트 준비 완료 신호
    PACKET_TIGER_ATTACK = 10,  // 호랑이 공격 패킷
    PACKET_TIGER_RESPAWN_REQUEST = 11,  // 호랑이 재생성 요청
    PACKET_TIGER_HIT = 12,     // 호랑이 Hit 패킷
    PACKET_STAGE_CHANGE = 13,  // 스테이지 변경 패킷
    PACKET_PUZZLE_UPDATE = 14, // 퍼즐 상태 업데이트 패킷
    PACKET_PUZZLE_SYNC = 15    // 퍼즐 상태 동기화 패킷
};

struct PacketPlayerUpdate {
    PacketHeader header;
    int clientID;
    float x, y, z;    // Position
    float rotY;       // Rotation
    char animationFile[64];  // 현재 애니메이션 파일명
    float animationTime;     // 애니메이션 시간
    char stageName[32];      // 현재 스테이지 이름 추가
};

struct PacketPlayerSpawn {
    PacketHeader header;
    int playerID;     // 클라이언트 ID
    char username[32]; // 사용자명 (최대 31자 + null)
};

struct PacketTigerSpawn {
    PacketHeader header;
    int tigerID;
    float x, y, z;
};

struct PacketTigerUpdate {
    PacketHeader header;
    int tigerID;
    float x, y, z;
    float rotY;
    char animationFile[64];  // 현재 애니메이션 파일명
    float animationTime;     // 애니메이션 시간
};



struct PacketLoginRequest {
    PacketHeader header;
    char username[32]; // 사용자명 (최대 31자 + null)
};

struct PacketLoginResponse {
    PacketHeader header;
    int clientID;
    bool success;
    char message[128]; // 응답 메시지
};

struct PacketPlayerDisconnect {
    PacketHeader header;
    int playerID;
    char username[32];
};

struct PacketClientReady {
    PacketHeader header;
    int clientID;
};

struct PacketTigerAttack {
    PacketHeader header;
    int tigerID;
    float x, y, z;  // 공격 위치
    float rotY;     // 공격 방향
};

struct PacketTigerRespawnRequest {
    PacketHeader header;
    int clientID;
    // 추가 데이터가 필요하면 여기에 추가
};

struct PacketTigerHit {
    PacketHeader header;
    int tigerID;
    int life;  // 남은 체력
};

struct PacketStageChange {
    PacketHeader header;
    int clientID;
    char stageName[32];  // 스테이지 이름 (예: "Hunting", "God")
};

struct PacketPuzzleUpdate {
    PacketHeader header;
    int clientID;
    int puzzleStatus[3][3];  // 퍼즐 상태 (0: O, 1: X)
};

struct PacketPuzzleSync {
    PacketHeader header;
    int puzzleStatus[3][3];  // 서버에서 전송하는 퍼즐 상태
};
#pragma pack(pop) 