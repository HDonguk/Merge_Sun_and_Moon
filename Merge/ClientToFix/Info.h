#pragma once
#include "stdafx.h"

struct Vertex
{
	XMFLOAT3 position;
	XMFLOAT3 normal;
	XMFLOAT2 uv;
	XMFLOAT4 weight;
	int boneIndex[4];
};

struct ObjectCB
{
    XMFLOAT4X4 world;
	XMFLOAT4X4 finalTransform[90];
	int isAnimate;
	int padding0[3];
	float powValue;
	float ambiantValue;
	float padding1[2];
};

struct CommonCB
{
    XMFLOAT4X4 view;
    XMFLOAT4X4 proj;
	XMFLOAT4X4 lightViewProj;
	XMFLOAT4X4 lightTexCoord;
};

struct SubMeshData
{
	UINT vertexCountPerInstance;
	UINT indexCountPerInstance;
	UINT startVertexLocation;
	UINT startIndexLocation;
	UINT baseVertexLocation;
};

enum CollisionState {
	//NONE,         // �浹 ����
	ENTER,         // �浹 ����
	STAY,          // �浹 ��
	EXIT           // �浹 ����
};

enum class ePass
{
	Shadow,
	Default
};

enum class eKeyTable
{
	Up,
	Down,
	Right,
	Left,
	Shift,
	SIZE
};

enum class eKeyState
{
	None,
	Pressed,
	Released,
	SIZE
};

enum class eEvent
{
	None,
	MoveKeyPressed,
	MoveKeyReleased,
	ShiftKeyPressed,
	ShiftKeyReleased,
};

enum class eBehavior
{
	Idle,
	Walk,
	Run,
	Die
};
