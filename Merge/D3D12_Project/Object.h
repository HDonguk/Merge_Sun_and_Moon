#pragma once
#include "stdafx.h"
#include "Component.h"

class GameTimer;
class Scene;
class Object
{
public:
	virtual ~Object();
	Object(Scene* scene, uint32_t id, uint32_t parentId = -1);
	virtual void OnUpdate(GameTimer& gTimer);
	virtual void OnProcessCollision(Object& other, XMVECTOR collisionNormal, float penetration);
	virtual void LateUpdate(GameTimer& gTimer);
	virtual void OnRender(ID3D12Device* device, ID3D12GraphicsCommandList * commandList);
	void ProcessAnimation(GameTimer& gTimer);
	void BuildConstantBuffer(ID3D12Device* device);
	void AddComponent(Component* component);
	Scene* GetScene() { return m_scene; }
	uint32_t GetId();
	bool GetValid();
	void Delete();

	template <typename T>
	T* GetComponent() 
	{
		T* temp = nullptr;
		for (Component* component : m_components){
			temp = dynamic_cast<T*>(component);
			if (temp) break;
		}
		return temp; 
	}

protected:
	Scene* m_scene = nullptr;
	uint32_t m_id = -1;
	uint32_t m_parent_id = -1;
	bool m_valid = true;
	vector<Component*> m_components;

	// ������Ʈ ���� �������� CB
	UINT8* m_mappedData = nullptr;
	ComPtr<ID3D12Resource> m_constantBuffer;
};

class PlayerObject : public Object
{
public:
	using Object::Object;
	void OnUpdate(GameTimer& gTimer) override;
	void OnProcessCollision(Object& other, XMVECTOR collisionNormal, float penetration) override;
	int GetRicecakeCount();
	int GetLifeCount();
	
	// 네트워크 플레이어 구분
	void SetIsNetworkPlayer(bool isNetwork) { m_isNetworkPlayer = isNetwork; }
	bool IsNetworkPlayer() const { return m_isNetworkPlayer; }
	
private:
	void ProcessInput(const GameTimer& gTimer);
public:
	// 네트워크 플레이어를 위해 ChangeState() 메서드를 public으로 변경
	void ChangeState(string fileName);
private:
	void Move(XMVECTOR dir, float speed, float deltatime);
	void Idle();
	void Jump();
	void Attack();
	void Throw();
	void TimeOut();
	void Fire();
public:
	void Hit();
	void Dead();
	void CalcTime(float deltaTime);

	float mWalkSpeed = 25.0f; // 20.0f * 0.75 = 15.0f
	float mRunSpeed = 60.0f; // 80.0f * 0.75 = 60.0f
	float mElapseTime = 0.0f;
	
	float mAttackTime = 0.0f;
	bool mIsFired = false;
	bool mIsHitted = false;
	
	bool mIsJumpping = false;

	int mLife = 3;
	XMFLOAT3 mDir{};
	int mRicecake = 0;

	bool m_isNetworkPlayer = false;  // 네트워크 플레이어 구분
	
	// 생명력 관리 메서드 추가
	void SetLife(int life) { mLife = life; }
	int GetLife() const { return mLife; }
};

class CameraObject : public Object
{
public:
	using Object::Object;
	void OnUpdate(GameTimer& gTimer) override;
	void LateUpdate(GameTimer& gTimer) override;
	void OnMouseInput(WPARAM wParam, HWND hWnd);
private:
	int mLastPosX = -1;
	int mLastPosY = -1;
	float mTheta = XMConvertToRadians(-90.0f);
	float mPhi = XMConvertToRadians(70.0f);
	float mRadius = 70.0f;
};

class TerrainObject : public Object
{
public:
	using Object::Object;
};

class TestObject : public Object
{
public:
	using Object::Object;
};

class TreeObject : public Object
{
public:
	using Object::Object;
	void OnUpdate(GameTimer& gTimer) override;
	void OnProcessCollision(Object& other, XMVECTOR collisionNormal, float penetration) override;


private:
	float mElapseTime = 0.0f;
};

class TigerObject : public Object
{
public:
	TigerObject(Scene* scene, uint32_t id, uint32_t parentId = -1);
	void OnUpdate(GameTimer& gTimer) override;
	void OnProcessCollision(Object& other, XMVECTOR collisionNormal, float penetration) override;
	int GetLife();
	
	// 네트워크 관련 메서드
	void SetIsNetworkTiger(bool isNetworkTiger) { m_isNetworkTiger = isNetworkTiger; }
	bool IsNetworkTiger() const { return m_isNetworkTiger; }
	void SetNetworkTigerID(int tigerID) { m_networkTigerID = tigerID; }
	int GetNetworkTigerID() const { return m_networkTigerID; }
	

	
	// 네트워크 호랑이 애니메이션 상태 설정
	
	// 네트워크 호랑이 위치 및 회전 설정
	void SetNetworkTransform(float x, float y, float z, float rotY);
	
	// 네트워크 호랑이 애니메이션 설정
	void SetNetworkAnimation(const std::string& animationFile, float animationTime);
	
	// 공격 받기 메서드 (public으로 변경)
	void Hit();
	void Dead();
	void SetLife(int life) { mLife = life; }
	int GetLife() const { return mLife; }
	void ResetHitTimer() { mElapseTime = 0.0f; }
	void ResetHitState() { mIsHitted = false; }
	void ResetAnimationTimer() { mElapseTime = 0.0f; }
	
	// 네트워크 호랑이를 위해 Fire() 메서드를 public으로 변경
	void Fire();
	// 네트워크 호랑이를 위해 ChangeState() 메서드를 public으로 변경
	void ChangeState(string fileName);
	// 서버 공격 신호 설정 메서드
	void SetServerAttackSignal(bool signal) { m_serverAttackSignal = signal; }
	
private:
	void TigerBehavior(GameTimer& gTimer);
	void Search(float deltaTime);
	void Run();
	void Attack();
	void TimeOut();
	void CalcTime(float deltaTime);
	void CreateLeather();
	float mWalkSpeed = 20.0f;  // 원본과 동일하게 조정
	float mRunSpeed = 35.0f;   // 원본과 동일하게 조정
	float mElapseTime = 0.0f;
	float mAttackTime = 0.0f;
	float mSearchTime = 0.0f;
	bool mIsFired = false;
	bool mIsHitted = false;
	int mLife = 3;
	
	// 네트워크 관련 멤버 변수 (단순화)
	bool m_isNetworkTiger = false;
	int m_networkTigerID = -1;
	
	// 이전 네트워크 위치를 저장하여 이동 방향 계산에 사용
	XMVECTOR m_previousNetworkPos = {0.0f, 0.0f, 0.0f, 1.0f};
	
	// 서버로부터 공격 신호를 받았는지 확인하는 플래그
	bool m_serverAttackSignal = false;
	
	// hit 애니메이션 후 idle 상태 보호를 위한 플래그
	bool m_protectIdleAfterHit = false;
	float m_hitProtectionTimer = 0.0f;  // hit 애니메이션 후 보호 타이머
};


class TigerAttackObject : public Object
{
public:
	using Object::Object;
	void OnUpdate(GameTimer& gTimer) override;
	void OnProcessCollision(Object& other, XMVECTOR collisionNormal, float penetration) override;
private:
	float mElapseTime = 0.0f;
};

class PlayerAttackObject : public Object
{
public:
	using Object::Object;
	void OnUpdate(GameTimer& gTimer) override;
	void OnProcessCollision(Object& other, XMVECTOR collisionNormal, float penetration) override;
private:
	float mElapseTime = 0.0f;
};


class TigerMockup : public Object
{
public:
	using Object::Object;
	void OnUpdate(GameTimer& gTimer) override;
	void OnProcessCollision(Object& other, XMVECTOR collisionNormal, float penetration) override;
private:
	float mSearchTime = 0.0f;
	float mWalkSpeed = 20.0f;
	bool m_hasCollided = false;  // 충돌 플래그 추가
};

class TigerLeather : public Object
{
public:
	Object::Object;
	void OnUpdate(GameTimer& gTimer) override;
	void OnProcessCollision(Object& other, XMVECTOR collisionNormal, float penetration) override;
private:
};

class SisterObject : public Object
{
public:
	Object::Object;
	void OnProcessCollision(Object& other, XMVECTOR collisionNormal, float penetration) override;
private:
	bool mIsQuadAble = false;
};

class GodObject : public Object
{
public:
	Object::Object;
	void OnProcessCollision(Object& other, XMVECTOR collisionNormal, float penetration) override;
private:
};

class RotFenceObject : public Object
{
public:
	Object::Object;
	void OnUpdate(GameTimer& gTimer) override;
private:
};

class AxeObject : public Object
{
public:
	Object::Object;
	void OnUpdate(GameTimer& gTimer) override;
	void OnProcessCollision(Object& other, XMVECTOR collisionNormal, float penetration) override;
private:
};

class RicecakeObject : public Object
{
public:
	Object::Object;
	void SetDir(XMVECTOR dir);
	void OnUpdate(GameTimer& gTimer) override;
	void OnProcessCollision(Object& other, XMVECTOR collisionNormal, float penetration) override;
	void LateUpdate(GameTimer& gTimer) override;

private:
	XMFLOAT3 mDir{};
	float mSpeed = 200.0f;
};

class GoToBaseObject : public Object
{
public:
	Object::Object;
	void OnUpdate(GameTimer& gTimer) override;
	void OnProcessCollision(Object& other, XMVECTOR collisionNormal, float penetration) override;

private:
	float mElapseTime = 0.0f;
};

class TitleQuadObject : public Object
{
public:
	Object::Object;
	void OnUpdate(GameTimer& gTimer) override;
};

class EndQuadObject : public Object
{
public:
	Object::Object;
	void OnUpdate(GameTimer& gTimer) override;
};

class SisterQuadObject : public Object
{
public:
	Object::Object;
	void OnUpdate(GameTimer& gTimer) override;
};

class LifeQuadObject : public Object
{
public:
	Object::Object;
	void OnUpdate(GameTimer& gTimer) override;
};

class BoyIconQuadObject : public Object
{
public:
	Object::Object;
	void OnUpdate(GameTimer& gTimer) override;
};

class RiceCakeQuadObject : public Object
{
	Object::Object;
	void OnUpdate(GameTimer& gTimer) override;
};

class TigerLeatherQuadObject : public Object
{
	Object::Object;
	void OnUpdate(GameTimer& gTimer) override;
};