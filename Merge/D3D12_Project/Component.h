#pragma once
#include <variant>
#include <DirectXCollision.h>
#include "stdafx.h"
#include "Info.h"
#include "FbxExtractor.h"
#include <queue>

struct Component // 객체로 만들지 않는 클래스
{
	virtual ~Component() = default;
};

class Transform : public Component
{
public:
	//Transform(XMFLOAT3&& pos, XMFLOAT3&& rot = { 0.0f, 0.0f, 0.0f }, XMFLOAT3&& scale = { 1.0f, 1.0f, 1.0f });
	Transform(XMVECTOR pos, XMVECTOR rot = { 0.0f, 0.0f, 0.0f, 0.0f }, XMVECTOR scale = { 1.0f, 1.0f, 1.0f, 0.0f });
	XMVECTOR GetScale();
	XMVECTOR GetRotation();
	XMVECTOR GetQuaternion();
	XMVECTOR GetPosition();
	XMMATRIX GetTranslateM();
	XMMATRIX GetScaleM();
	XMMATRIX GetRotationM();
	XMMATRIX GetRotationQuaternionM();
	XMMATRIX GetTransformM();
	XMMATRIX GetFinalM();
	void SetPosition(XMVECTOR pos);
	void SetRotation(XMVECTOR rot);
	void SetQuaternion(XMVECTOR qua);
	void SetFinalM(XMMATRIX finalM);
private:
	XMVECTOR GetQuaternionFromRotation();
	XMFLOAT3 mScale{ 1.0f, 1.0f, 1.0f };
	XMFLOAT3 mRotation{ 0.0f, 0.0f, 0.0f };
	XMFLOAT4 mQuaternion{ 0.0f, 0.0f, 0.0f, 1.0f };
	XMFLOAT3 mPosition{ 0.0f, 0.0f, 0.0f };
	XMFLOAT4X4 mFinalM{
	1.0f, 0.0f, 0.0f, 0.0f,
	0.0f, 1.0f, 0.0f, 0.0f,
	0.0f, 0.0f, 1.0f, 0.0f,
	0.0f, 0.0f, 0.0f, 1.0f };
};

class AdjustTransform : public Component
{
public:
	AdjustTransform(XMVECTOR pos = { 0.0f, 0.0f, 0.0f, 0.0f }, XMVECTOR rot = { 0.0f, 0.0f, 0.0f, 0.0f }, XMVECTOR scale = { 1.0f, 1.0f, 1.0f, 0.0f });
	XMMATRIX GetScaleM();
	XMMATRIX GetRotationM();
	XMMATRIX GetTranslateM();
	XMMATRIX GetTransformM();
private:
	XMFLOAT3 mScale{ 1.0f, 1.0f, 1.0f };
	XMFLOAT3 mRotation{ 0.0f, 0.0f, 0.0f };
	XMFLOAT3 mPosition{ 0.0f, 0.0f, 0.0f };
};

struct Mesh : public Component
{ 
	Mesh(string name) : mName{ name } {}
	string mName = "";
};

struct Texture : public Component
{
	Texture(wstring name, float pow, float ambiant) : mName{ name }, mPowValue{ pow }, mAmbiantValue{ambiant} {}
	wstring mName = L"";
	float mPowValue = 0.0f;
	float mAmbiantValue = 0.0f;
};

struct Animation : public Component
{
	Animation(string initFileName);
	void ResetAnim(string fileName, float time);
	float mAnimationTime = 0.0f;
	string mCurrentFileName = "";
};

class Gravity : public Component
{
public:
	XMVECTOR ProcessGravity(XMVECTOR pos, float deltaTime);
	void ResetElapseTime();
	float GetElapseTime();
	void SetVerticalSpeed(float speed);
private:
	float mElapseTime = 0.0f;
	float mG = 80.0f;
	float mVerticalSpeed = 0.0f;
};

class Collider : public Component
{
public:
	Collider(XMFLOAT3&& center = { 0.0f, 0.0f, 0.0f }, XMFLOAT3&& extents = { 0.5f, 0.5f, 0.5f }, XMFLOAT4&& orientation = { 0.0f, 0.0f, 0.0f, 1.0f });
	void UpdateOBB(XMMATRIX M);
	BoundingOrientedBox& GetOBB();
private:
	BoundingOrientedBox mBaseOBB{};
	BoundingOrientedBox mOBB{};
};