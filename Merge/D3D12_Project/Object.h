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
	void ProcessRicecakeMockUp();
	float mWalkSpeed = 25.0f; // 20.0f * 0.75 = 15.0f
	float mRunSpeed = 60.0f; // 80.0f * 0.75 = 60.0f
	float mElapseTime = 0.0f;
	float mJumpTime = 0.0f;
	float mAttackTime = 0.0f;
	bool mIsFired = false;
	bool mIsHitted = false;
	bool mJumped = false;
	int mLife = 3;
	XMFLOAT3 mDir{};
	int mRicecake = 0;
	bool mHasRicecake = false;
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
	
	// 네트워크 관련 메서드
	void SetIsNetworkTiger(bool isNetworkTiger) { m_isNetworkTiger = isNetworkTiger; }
	bool IsNetworkTiger() const { return m_isNetworkTiger; }
	void SetNetworkTigerID(int tigerID) { m_networkTigerID = tigerID; }
	int GetNetworkTigerID() const { return m_networkTigerID; }
	

	
	// 네트워크 호랑이 애니메이션 상태 설정
	void SetNetworkAnimation(const std::string& animationFile, float animationTime) {
		if (m_isNetworkTiger) {
			Animation* anim = GetComponent<Animation>();
			if (anim) {
				// hit 애니메이션 중에는 서버 업데이트를 무시하여 애니메이션이 완료되도록 함
				if (anim->mCurrentFileName == "0208_tiger_hit.fbx") {
					return;  // hit 애니메이션 중에는 서버 애니메이션 업데이트 무시
				}
				
				// hit 보호 타이머가 활성화된 경우 모든 서버 애니메이션 업데이트 무시
				if (m_hitProtectionTimer > 0.0f) {
					return;  // 보호 타이머가 활성화된 동안 서버 업데이트 무시
				}
				
				// hit 애니메이션 후 idle 상태 보호 플래그가 설정된 경우 서버 업데이트 무시
				if (m_protectIdleAfterHit && anim->mCurrentFileName == "0722_tiger_idle2.fbx") {
					// 보호 타이머가 만료된 후에만 다른 애니메이션 허용
					if (m_hitProtectionTimer <= 0.0f) {
						m_protectIdleAfterHit = false;  // 보호 플래그 해제
					} else {
						return;  // idle 상태 보호 중에는 서버 업데이트 무시
					}
				}
				
				// 애니메이션 파일이 변경된 경우에만 리셋 (원본 클라이언트처럼 단순하게)
				if (anim->mCurrentFileName != animationFile) {
					anim->ResetAnim(animationFile, 0.0f);
					mElapseTime = 0.0f;
				}
				// 서버 시간 동기화 로직 제거 - 원본 클라이언트처럼 단순하게 유지
			}
		}
	}
	
	// 네트워크 호랑이 위치 및 회전 설정
	void SetNetworkTransform(float x, float y, float z, float rotY);
	
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
	
	// hit 애니메이션 후 idle 상태 보호를 위한 플래그
	bool m_protectIdleAfterHit = false;
	float m_hitProtectionTimer = 0.0f;  // hit 애니메이션 후 보호 타이머
};


class TigerAttackObject : public Object
{
public:
	using Object::Object;
	void OnUpdate(GameTimer& gTimer) override;
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

class RicecakeMockup : public Object
{
public:
	Object::Object;
	void OnUpdate(GameTimer& gTimer) override;
private:
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

class TitleObject : public Object
{
public:
	Object::Object;
	void OnUpdate(GameTimer& gTimer) override;
};

class QuadObject : public Object
{
public:
	Object::Object;
	void OnUpdate(GameTimer& gTimer) override;
};