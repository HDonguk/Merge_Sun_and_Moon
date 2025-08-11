#include "stdafx.h"
#include "Object.h"
#include "GameTimer.h"
#include "Scene.h"
#include "DXSampleHelper.h"
#include <random>
#include "Framework.h"
#include "NetworkManager.h"

std::random_device rd;  // ù ° rd ü
default_random_engine dre(rd());
uniform_int_distribution uid(-180,180);

Object::~Object()
{
    for (Component* component : m_components) {
        delete component;
    }
}

Object::Object(Scene* scene, uint32_t id, uint32_t parentId) : m_scene{ scene }, m_id{id}, m_parent_id{parentId}
{
    BuildConstantBuffer(scene->GetFramework()->GetDevice());
}

void Object::OnUpdate(GameTimer& gTimer)
{
    Transform* transform = GetComponent<Transform>();
    Gravity* gravity = GetComponent<Gravity>();
    if (gravity)
    {
        XMVECTOR newPos = gravity->ProcessGravity(transform->GetPosition(), gTimer.DeltaTime());
        transform->SetPosition(newPos);
    }

    XMMATRIX finalM = transform->GetTransformM();
    if (m_parent_id != -1) {
        Object* parentObj = m_scene->GetObjFromId(m_parent_id);
        if (parentObj) {
            Transform* parentTransform = parentObj->GetComponent<Transform>();
            finalM = finalM * parentTransform->GetFinalM();
        }

    }
    transform->SetFinalM(finalM);

    Collider* collider = GetComponent<Collider>();
    if (collider) {
        collider->UpdateOBB(finalM);
    }
}

void Object::OnProcessCollision(Object& other, XMVECTOR collisionNormal, float penetration)
{
    float similarity = XMVectorGetX(XMVector3Dot(XMVECTOR{ 0.0f, 1.0f, 0.0f, 0.0f }, -collisionNormal));
    Gravity* gravity = GetComponent<Gravity>();
    if (gravity && similarity > 0.80f) {
        gravity->ResetElapseTime();

        Transform* transform = GetComponent<Transform>();
        XMVECTOR pos = transform->GetPosition();
        pos -= collisionNormal * penetration;
        transform->SetPosition(pos);
    }
}

void Object::LateUpdate(GameTimer& gTimer)
{
    Transform* transform = GetComponent<Transform>();
    TerrainObject* terrainObj = dynamic_cast<TerrainObject*>(this);
    if (!terrainObj && m_parent_id == -1) {
        XMVECTOR pos = transform->GetPosition();
        char outstatus = m_scene->ClampToBounds(pos, { 0.0f, 0.0f, 0.0f });
        transform->SetPosition(pos);

        Gravity* gravity = GetComponent<Gravity>();
        if ((outstatus & 0x04) && gravity)
        {
            gravity->ResetElapseTime();
        }
    }

    XMMATRIX finalM = transform->GetTransformM();
    if (m_parent_id != -1) {
        Object* parentObj = m_scene->GetObjFromId(m_parent_id);
        if (parentObj)
        {
            Transform* parentTransform = parentObj->GetComponent<Transform>();
            finalM = finalM * parentTransform->GetFinalM();
        }
        else
        {
            Delete();
        }
    }
    transform->SetFinalM(finalM);

    XMMATRIX world = transform->GetFinalM();
    XMMATRIX adjustM = XMMatrixIdentity();
    AdjustTransform* adjustTrnasform = GetComponent<AdjustTransform>();
    if (adjustTrnasform) {
        adjustM = adjustTrnasform->GetTransformM();
    }
    memcpy(m_mappedData, &XMMatrixTranspose(adjustM * world), sizeof(XMMATRIX));

    ProcessAnimation(gTimer);

    Texture* texture = GetComponent<Texture>();
    float powValue = 1.0f;
    float ambiantValue = 0.4f;
    if (texture) {
        ambiantValue = texture->mAmbiantValue;
        powValue = texture->mPowValue;
    }
    memcpy(m_mappedData + sizeof(XMFLOAT4X4) * 91 + sizeof(int) * 4, &powValue, sizeof(float));
    memcpy(m_mappedData + sizeof(XMFLOAT4X4) * 91 + sizeof(int) * 4 + sizeof(float), &ambiantValue, sizeof(float));
}

void Object::OnRender(ID3D12Device* device, ID3D12GraphicsCommandList* commandList)
{
    Mesh* mesh = GetComponent<Mesh>();
    if (!mesh) return;
    
    Texture* texture = GetComponent<Texture>();
    int textureIndex = m_scene->GetTextureIndex(texture->mName);

    CD3DX12_GPU_DESCRIPTOR_HANDLE hDescriptor(m_scene->GetDescriptorHeap()->GetGPUDescriptorHandleForHeapStart());
    hDescriptor.Offset(1 + textureIndex, device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));
    commandList->SetGraphicsRootDescriptorTable(1, hDescriptor);
    commandList->SetGraphicsRootConstantBufferView(2, m_constantBuffer.Get()->GetGPUVirtualAddress());

    SubMeshData& data = m_scene->GetResourceManager().GetSubMeshData(mesh->mName);
    if (data.startIndexLocation == -1) {
        commandList->DrawInstanced(data.vertexCountPerInstance, 1, data.startVertexLocation, 0);
    }
    else {
        commandList->DrawIndexedInstanced(data.indexCountPerInstance, 1, data.startIndexLocation, data.baseVertexLocation, 0);
    }

}


void Object::BuildConstantBuffer(ID3D12Device* device)
{
    const UINT constantBufferSize = m_scene->CalcConstantBufferByteSize(sizeof(ObjectCB));    // CB size is required to be 256-byte aligned.

    ThrowIfFailed(device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer(constantBufferSize),
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&m_constantBuffer)));

    CD3DX12_RANGE readRange(0, 0);
    ThrowIfFailed(m_constantBuffer->Map(0, &readRange, reinterpret_cast<void**>(& m_mappedData)));
}

void Object::AddComponent(Component* component)
{
    m_components.push_back(component);
}

void Object::ProcessAnimation(GameTimer& gTimer)
{
    Animation* animation = GetComponent<Animation>();
    int isAnimate = false;
    if (animation) {
        isAnimate = true;
        vector<XMFLOAT4X4> finalTransforms{ 90 };
        SkinnedData& animData = m_scene->GetResourceManager().GetAnimationData(animation->mCurrentFileName);
        
        animation->mAnimationTime += gTimer.DeltaTime();
        string clipName = "Take 001";
        if (animation->mAnimationTime >= animData.GetClipEndTime(clipName)) animation->mAnimationTime = 0.0f;
        animData.GetFinalTransforms(clipName, animation->mAnimationTime, finalTransforms);
        memcpy(m_mappedData + sizeof(XMMATRIX), finalTransforms.data(), sizeof(XMMATRIX) * 90);
    }
    memcpy(m_mappedData + sizeof(XMMATRIX) * 91, &isAnimate, sizeof(int));
}

uint32_t Object::GetId()
{
    return m_id;
}

bool Object::GetValid()
{
    return m_valid;
}

void Object::Delete()
{
    m_valid = false;
}

void PlayerObject::OnUpdate(GameTimer& gTimer)
{
    Transform* transform = GetComponent<Transform>();
    XMVECTOR pos = transform->GetPosition();
    char outstatus = m_scene->ClampToBounds(pos, { 0.0f, 0.0f, 0.0f });
    if (outstatus & 0x04) mIsJumpping = false;

    // 네트워크 플레이어는 입력 처리하지 않음
    if (!m_isNetworkPlayer) {
        ProcessInput(gTimer);
    }

    Object::OnUpdate(gTimer);
    
    // 플레이어 생명력 상태 로그 (처음 몇 프레임만 출력)
    static int frameCount = 0;
    if (frameCount < 10) {
        wchar_t debugMsg[256];
        swprintf_s(debugMsg, L"[PlayerObject] Frame %d - Life: %d, IsNetworkPlayer: %s\n", 
                   frameCount, mLife, m_isNetworkPlayer ? L"true" : L"false");
        OutputDebugString(debugMsg);
        frameCount++;
    }
    
    // 플레이어 생명력이 0 이하로 설정되어 있으면 3으로 초기화 (안전장치)
    if (mLife <= 0 && frameCount == 0) {
        mLife = 3;
        OutputDebugString(L"[PlayerObject] Life was 0 or negative, resetting to 3\n");
    }
}

void PlayerObject::OnProcessCollision(Object& other, XMVECTOR collisionNormal, float penetration)
{
    Transform* transform = GetComponent<Transform>();

    PlayerAttackObject* pa = dynamic_cast<PlayerAttackObject*>(&other);
    if (pa) return;

    TigerAttackObject* ta = dynamic_cast<TigerAttackObject*>(&other);
    if (ta) // 호랑이 공격과 충돌했을 때...
    {
        // 위치 정보 로그 추가
        Transform* playerTransform = GetComponent<Transform>();
        Transform* attackTransform = ta->GetComponent<Transform>();
        
        XMFLOAT3 playerPos, attackPos;
        XMStoreFloat3(&playerPos, playerTransform->GetPosition());
        XMStoreFloat3(&attackPos, attackTransform->GetPosition());
        
        // 충돌 법선과 penetration 정보도 추가
        XMFLOAT3 normal;
        XMStoreFloat3(&normal, collisionNormal);
        
        wchar_t debugMsg[512];
        swprintf_s(debugMsg, L"[PlayerObject] TigerAttack collision detected!\n"
                               L"  Player pos: (%.1f, %.1f, %.1f)\n"
                               L"  Attack pos: (%.1f, %.1f, %.1f)\n"
                               L"  Collision normal: (%.3f, %.3f, %.3f)\n"
                               L"  Penetration: %.3f\n"
                               L"  Calling Hit() method...\n", 
                               playerPos.x, playerPos.y, playerPos.z, 
                               attackPos.x, attackPos.y, attackPos.z,
                               normal.x, normal.y, normal.z,
                               penetration);
        OutputDebugString(debugMsg);
        
        Hit();
        OutputDebugString(L"[PlayerObject] Hit() method completed\n");
        return;
    }

    TigerLeather* leather = dynamic_cast<TigerLeather*>(&other);
    if (leather)
    {
        m_scene->IncreaseLeatherCount();
        return;
    }


    AxeObject* axe = dynamic_cast<AxeObject*>(&other);
    if (axe)
    {
        return;
    }

    RicecakeObject* ricecake = dynamic_cast<RicecakeObject*>(&other);
    if (ricecake)
    {
        ++mRicecake;
        mRicecake = mRicecake > 4 ? 4 : mRicecake;
        return;
    }

    XMVECTOR pos = transform->GetPosition();
    pos -= collisionNormal * penetration;
    transform->SetPosition(pos);

    float similarity = XMVectorGetX(XMVector3Dot(XMVECTOR{ 0.0f, 1.0f, 0.0f, 0.0f }, -collisionNormal));
    Gravity* gravity = GetComponent<Gravity>();
    if (gravity && similarity > 0.80f) {
        gravity->ResetElapseTime();
        mIsJumpping = false;
    }
}

int PlayerObject::GetRicecakeCount()
{
    return mRicecake;
}

int PlayerObject::GetLifeCount()
{
    return mLife;
}

void PlayerObject::ProcessInput(const GameTimer& gTimer)
{
    CalcTime(gTimer.DeltaTime());
    BYTE* keyState = m_scene->GetFramework()->GetKeyState();
    Transform* transform = GetComponent<Transform>();

    XMVECTOR dir = XMVectorZero();
    if ((keyState[0x57] & 0x88) == 0x88) { dir += XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f); } // w
    if ((keyState[0x53] & 0x88) == 0x88) { dir -= XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f); } // s
    if ((keyState[0x41] & 0x88) == 0x88) { dir -= XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f); } // a
    if ((keyState[0x44] & 0x88) == 0x88) { dir += XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f); } // d

    if (!XMVector3Equal(dir, XMVectorZero())) {
        float speed = mWalkSpeed;
        if ((keyState[VK_SHIFT] & 0x88) == 0x88) {
            speed = mRunSpeed;
        }
        if ((keyState[VK_CONTROL] & 0x88) == 0x88) {
            speed = 100.0f;
        }
        Move(dir, speed, gTimer.DeltaTime());
    }
    else {
        Idle();
    }

    if ((keyState[VK_LBUTTON] & 0x88) == 0x80)  Attack();
    if ((keyState[VK_RBUTTON] & 0x88) == 0x80)  Throw();


    if ((keyState[VK_SPACE] & 0x88) == 0x80) {
        Jump();
    }
}

void PlayerObject::ChangeState(string fileName)
{
    Animation* anim = GetComponent<Animation>();
    if (anim->ResetAnim(fileName, 0.0f)) mElapseTime = 0.0f;
}

void PlayerObject::Move(XMVECTOR dir, float speed,float deltaTime)
{
    Animation* anim = GetComponent<Animation>();
    if (anim->mCurrentFileName == "boy_attack(45).fbx") return;
    if (anim->mCurrentFileName == "boy_throw.fbx") return;
    if (anim->mCurrentFileName == "boy_hit.fbx") return;
    if (anim->mCurrentFileName == "boy_dying_fix.fbx") return;
    if (mAttackTime < 1.0) return;

    if (speed >= mRunSpeed) {
        ChangeState("boy_run_fix.fbx");
    }
    else if (speed >= mWalkSpeed) {
        ChangeState("boy_walk_fix.fbx");
    }

    Transform* transform = GetComponent<Transform>();
    CameraObject* cameraObj = m_scene->GetObj<CameraObject>();
    Transform* cameraTransform = cameraObj->GetComponent<Transform>();
    dir = XMVector3TransformNormal(dir, cameraTransform->GetRotationM());

    dir = XMVector3Normalize(XMVectorSetY(dir, 0.0f));
    XMStoreFloat3(&mDir, dir);

    XMVECTOR pos = transform->GetPosition();
    pos += dir * speed * deltaTime;
    transform->SetPosition(pos);

    float yaw = atan2f(XMVectorGetX(dir), XMVectorGetZ(dir)) * 180 / 3.141592f;
    transform->SetRotation({ 0.0f, yaw, 0.0f });
}


void PlayerObject::Idle()
{
    Animation* anim = GetComponent<Animation>();
    if (anim->mCurrentFileName == "boy_attack(45).fbx") return;
    if (anim->mCurrentFileName == "boy_throw.fbx") return;
    if (anim->mCurrentFileName == "boy_hit.fbx") return;
    if (anim->mCurrentFileName == "boy_dying_fix.fbx") return;

    ChangeState("1P(boy-idle).fbx");
}

void PlayerObject::Jump()
{
    if (mIsJumpping) return;
    mIsJumpping = true;
    Gravity* gravity = GetComponent<Gravity>();
    Animation* anim = GetComponent<Animation>();
    if (!gravity) return;
    if (anim->mCurrentFileName == "boy_hit.fbx") return;
    if (anim->mCurrentFileName == "boy_dying_fix.fbx") return;
    gravity->SetVerticalSpeed(40.0f);
    gravity->ResetElapseTime();
}


void PlayerObject::Attack()
{
    Animation* anim = GetComponent<Animation>();
    if (anim->mCurrentFileName == "boy_throw.fbx") return;
    if (anim->mCurrentFileName == "boy_hit.fbx") return;
    if (anim->mCurrentFileName == "boy_dying_fix.fbx") return;

    ChangeState("boy_attack(45).fbx");
}

void PlayerObject::Throw()
{
    Animation* anim = GetComponent<Animation>();
    if (anim->mCurrentFileName == "boy_attack(45).fbx") return;
    if (anim->mCurrentFileName == "boy_hit.fbx") return;
    if (anim->mCurrentFileName == "boy_dying_fix.fbx") return;
    if (mAttackTime < 1.0) return;
    if (mRicecake < 1) return;
    ChangeState("boy_throw.fbx");
}

void PlayerObject::TimeOut()
{
    Animation* anim = GetComponent<Animation>();
    if (anim->mCurrentFileName == "boy_attack(45).fbx" || anim->mCurrentFileName == "boy_throw.fbx")
    {
        mIsFired = false;
        mAttackTime = 0.0f;
        ChangeState("1P(boy-idle).fbx");
        return;
    }

    if (anim->mCurrentFileName == "boy_hit.fbx")
    {
        mIsHitted = false;
        ChangeState("1P(boy-idle).fbx");
        return;
    }

    if (anim->mCurrentFileName == "boy_dying_fix.fbx")
    {
        m_scene->ResetLeatherCount();
        m_scene->SetStage(L"Base");
        return;
    }
}

void PlayerObject::Fire()
{
    if (mIsFired) return;

    mIsFired = true;

    Animation* anim = GetComponent<Animation>();
    if (anim->mCurrentFileName == "boy_attack(45).fbx")
    {
        Object* obj = new PlayerAttackObject(m_scene, m_scene->AllocateId(), m_id);
        obj->AddComponent(new Transform{ {0.0f, 8.0f, 8.0f} });
        obj->AddComponent(new Collider{ {0.0f, 0.0f, 0.0f}, {6.0f, 8.0f, 6.0f} });
        m_scene->AddObj(obj);
    }

    if (anim->mCurrentFileName == "boy_throw.fbx")
    {
        --mRicecake;

        Transform* transform = GetComponent<Transform>();
        XMVECTOR pos = transform->GetPosition();
        XMVECTOR offset = XMVector3TransformNormal(XMVECTOR{ 4.0f, 10.0f, 10.0f }, transform->GetRotationM());
        float scale = 0.03f;
        RicecakeObject* obj = new RicecakeObject(m_scene, m_scene->AllocateId());
        obj->SetDir(XMLoadFloat3(&mDir));
        obj->AddComponent(new Transform{ pos + offset });
        obj->AddComponent(new AdjustTransform{ {-20.0f * scale, 22.0f * scale, 0.0f}, {0.0f, 0.0f, -90.0f}, {scale, scale, scale} });
        obj->AddComponent(new Mesh{ "ricecake.fbx" });
        obj->AddComponent(new Texture{ L"ricecake", 1.0f, 0.4f });
        obj->AddComponent(new Gravity);
        obj->AddComponent(new Collider{ {0.0f, 30.0f * scale, 0.0f}, {25.0f * scale, 30.0f * scale, 25.0f * scale} });
        m_scene->AddObj(obj);
    }
}

void PlayerObject::Hit()
{
    if (mIsHitted) return;
    mIsHitted = true;
    --mLife;
    
    // 생명력 변화 디버그 로그 추가
    wchar_t debugMsg[256];
    swprintf_s(debugMsg, L"[PlayerObject] Hit() called - Life decreased to: %d (IsNetworkPlayer: %s)\n", 
               mLife, m_isNetworkPlayer ? L"true" : L"false");
    OutputDebugString(debugMsg);
    
    if (mLife == 0)
    {
        OutputDebugString(L"[PlayerObject] Life is 0 - calling Dead()\n");
        Dead();
        return;
    }
    ChangeState("boy_hit.fbx");
}

void PlayerObject::Dead()
{
    ChangeState("boy_dying_fix.fbx");
}

void PlayerObject::CalcTime(float deltaTime)
{
    Animation* anim = GetComponent<Animation>();
    if (anim->mCurrentFileName == "boy_attack(45).fbx") 
    {
        mElapseTime += deltaTime;
        if (mElapseTime > 0.5f) Fire();
        if (mElapseTime > 1.0f) TimeOut();
    }
    else if (anim->mCurrentFileName == "boy_throw.fbx")
    {
        mElapseTime += deltaTime;
        if (mElapseTime > 0.7f) Fire();
        if (mElapseTime > 1.0f) TimeOut();
    }
    else 
    {
        mAttackTime += deltaTime;
    }

    if (anim->mCurrentFileName == "boy_hit.fbx")
    {
        mElapseTime += deltaTime;
        // 플레이어의 hit 애니메이션은 애니메이션의 실제 길이에 맞춰서 처리
        // Scene을 통해 ResourceManager에 접근하여 실제 애니메이션 길이를 가져와서 사용
        ResourceManager& resourceManager = m_scene->GetResourceManager();
        SkinnedData& animData = resourceManager.GetAnimationData(anim->mCurrentFileName);
        float clipEndTime = animData.GetClipEndTime("Take 001");
        if (mElapseTime > clipEndTime) {
            OutputDebugString(L"[PlayerObject] Hit animation timeout - switching to idle\n");
            TimeOut();
        }
    }

    if (anim->mCurrentFileName == "boy_dying_fix.fbx")
    {
        mElapseTime += deltaTime;
        if (mElapseTime > 2.0f) TimeOut();
    }

}



void CameraObject::OnUpdate(GameTimer& gTimer)
{
    float x = mRadius * sinf(mPhi) * cosf(mTheta);
    float y = mRadius * cosf(mPhi);
    float z = mRadius * sinf(mPhi) * sinf(mTheta);

    Object* playerObj = m_scene->GetObj<PlayerObject>();
    Transform* playerTransform = playerObj->GetComponent<Transform>();
    XMVECTOR targetPos = playerTransform->GetPosition() + XMVECTOR{0.0f, 10.0f, 0.0f};

    Transform* myTransform = GetComponent<Transform>();
    XMVECTOR myPos = targetPos + XMVECTOR{ x, y, z, 0.f };
    char outstatus = m_scene->ClampToBounds(myPos, { 0.0f, 1.0f, 0.0f });
    myTransform->SetPosition(myPos);
    
    XMVECTOR dir = targetPos - myPos;

    XMFLOAT3 yawPitch{};
    XMStoreFloat3(&yawPitch, dir);

    float yaw = atan2f(yawPitch.x, yawPitch.z) * 180 / 3.141592f;
    float pitch = atan2f(yawPitch.y, sqrtf(yawPitch.x * yawPitch.x + yawPitch.z * yawPitch.z)) * 180 / 3.141592f;
    myTransform->SetRotation({ -pitch, yaw, 0.0f });

    Object::OnUpdate(gTimer);
}

void CameraObject::LateUpdate(GameTimer& gTimer)
{
    Transform* transform = GetComponent<Transform>();

    XMMATRIX transformM = transform->GetTransformM();
    XMMATRIX invtransformM = XMMatrixInverse(nullptr, transformM);
    memcpy(m_scene->GetConstantBufferMappedData(), &XMMatrixTranspose(invtransformM), sizeof(XMMATRIX)); // ó Ű ּ
}

void CameraObject::OnMouseInput(WPARAM wParam, HWND hWnd)
{
    //  wnd  ǥ ˾ƿ´
    RECT clientRect{};
    GetWindowRect(hWnd, &clientRect);
    int width = int(clientRect.right - clientRect.left);
    int height = int(clientRect.bottom - clientRect.top);
    int centerX = clientRect.left + width / 2;
    int centerY = clientRect.top + height / 2;

    POINT currentMousePos;
    GetCursorPos(&currentMousePos);
    float dx = static_cast<float>(currentMousePos.x - centerX);
    float dy = static_cast<float>(currentMousePos.y - centerY);
    mTheta -= XMConvertToRadians(dx * 0.02f);
    mPhi -= XMConvertToRadians(dy * 0.02f);

    //  clamp
    float min = 0.1f;
    float max = XM_PI - 0.1f;
    mPhi = mPhi < min ? min : (mPhi > max ? max : mPhi);

    SetCursorPos(centerX, centerY);
}

TigerObject::TigerObject(Scene* scene, uint32_t id, uint32_t parentId) : Object(scene, id, parentId)
{
}

void TigerObject::OnUpdate(GameTimer& gTimer)
{
    CalcTime(gTimer.DeltaTime());
    
    // 네트워크 호랑이는 서버에서 관리되므로 TigerBehavior 호출하지 않음
    if (!m_isNetworkTiger) {
        TigerBehavior(gTimer);
    }
    
    Object::OnUpdate(gTimer);
}

void TigerObject::OnProcessCollision(Object& other, XMVECTOR collisionNormal, float penetration)
{
    TigerAttackObject* ta = dynamic_cast<TigerAttackObject*>(&other);
    if (ta) return;

    PlayerAttackObject* pa = dynamic_cast<PlayerAttackObject*>(&other);
    if (pa)
    {
        Hit();
        return;
    }
    RicecakeObject* rc = dynamic_cast<RicecakeObject*>(&other);
    if (rc)
    {
        HitByRiceCake();
        return;
    }


    Transform* transform = GetComponent<Transform>();
    XMVECTOR pos = transform->GetPosition();
    pos += -collisionNormal * penetration;
    transform->SetPosition(pos);
}

int TigerObject::GetLife()
{
    return mLife;
}


void TigerObject::TigerBehavior(GameTimer& gTimer)
{
    Transform* transform = GetComponent<Transform>();
    Animation* anim = GetComponent<Animation>();

    XMVECTOR pos = transform->GetPosition();
    PlayerObject* player = m_scene->GetObj<PlayerObject>();
    Transform* playerTransform = player->GetComponent<Transform>();
    XMVECTOR playerPos = playerTransform->GetPosition();
    float result = XMVectorGetX(XMVector3Length(playerPos - pos));
    XMVECTOR dir = XMVector3Normalize(playerPos - pos);
    float yaw = atan2f(XMVectorGetX(dir), XMVectorGetZ(dir)) * 180 / 3.141592f;

    if (result < 200.f) // ÷̾ Ž  ȿ ... 
    {
        if (result < 17.0f) // Ž ȿ ÷̾ ְ, ſ ٸ....
        {
            Attack();
            if (anim->mCurrentFileName == "0208_tiger_attack.fbx" && mElapseTime == 0)
            {
                transform->SetRotation({ 0.0f, yaw, 0.0f });
            }
        }
        else // Ž ȿ ÷̾ , ſ  ʴٸ...
        {
            Run();
            if (anim->mCurrentFileName == "0722_tiger_run.fbx")
            {
                transform->SetPosition(pos + dir * mRunSpeed * gTimer.DeltaTime());
                transform->SetRotation({ 0.0f, yaw, 0.0f });
            }
        }
    }
    else // ÷̾ Ž  ۿ ִ.
    {
        Search(gTimer.DeltaTime());
    }
}

void TigerObject::ChangeState(string fileName)
{
    Animation* anim = GetComponent<Animation>();
    if (anim->ResetAnim(fileName, 0.0f)) mElapseTime = 0.0f;
}

void TigerObject::Search(float deltaTime)
{
    
    static float randYaw = uid(dre);
    Transform* transform = GetComponent<Transform>();
    Animation* anim = GetComponent<Animation>();
    
    if (mSearchTime > 2.0f)
    {
        mSearchTime = 0.0f;
        randYaw = uid(dre);
        transform->SetRotation({ 0.0f, randYaw, 0.0f });
    }

    XMVECTOR dir = XMVector3TransformNormal({ 0.0f, 0.0f, 1.0f }, transform->GetRotationM());
    dir = XMVector3Normalize(dir);
    XMVECTOR pos = transform->GetPosition();
    transform->SetPosition(pos + dir * mWalkSpeed * deltaTime);

    // 네트워크 호랑이의 hit 후 idle 보호
    if (m_isNetworkTiger && m_protectIdleAfterHit && anim->mCurrentFileName == "0722_tiger_idle2.fbx") {
        return;  // hit 후 idle 상태 보호 중에는 walk로 변경하지 않음
    }
    
    ChangeState("0113_tiger_walk.fbx");
}

void TigerObject::Run()
{
    Animation* anim = GetComponent<Animation>();
    if (anim->mCurrentFileName == "0208_tiger_attack.fbx") return;
    if (anim->mCurrentFileName == "0208_tiger_hit.fbx") return;
    if (anim->mCurrentFileName == "0208_tiger_dying.fbx") return;
    
    // 네트워크 호랑이의 hit 후 idle 보호
    if (m_isNetworkTiger && m_protectIdleAfterHit && anim->mCurrentFileName == "0722_tiger_idle2.fbx") {
        return;  // hit 후 idle 상태 보호 중에는 run으로 변경하지 않음
    }
    
    if (mAttackTime < 2.0f) return;
    ChangeState("0722_tiger_run.fbx");
}

void TigerObject::Attack()
{
    Animation* anim = GetComponent<Animation>();
    if (anim->mCurrentFileName == "0208_tiger_hit.fbx") return;
    if (anim->mCurrentFileName == "0208_tiger_dying.fbx") return;
    
    // 네트워크 호랑이의 hit 후 idle 보호
    if (m_isNetworkTiger && m_protectIdleAfterHit && anim->mCurrentFileName == "0722_tiger_idle2.fbx") {
        return;  // hit 후 idle 상태 보호 중에는 attack으로 변경하지 않음
    }
    
    if (mAttackTime < 2.0f) return;  // Original과 동일한 2초
    ChangeState("0208_tiger_attack.fbx");
}
void TigerObject::TimeOut()
{
    Animation* anim = GetComponent<Animation>();
    
    if (anim->mCurrentFileName == "0208_tiger_attack.fbx") 
    {
        mIsFired = false;
        mAttackTime = 0.0f;
        ChangeState("0722_tiger_idle2.fbx");
    }

    if (anim->mCurrentFileName == "0208_tiger_hit.fbx")
    {
        mIsHitted = false;
        ChangeState("0722_tiger_idle2.fbx");
        
        // 네트워크 호랑이의 경우 hit 애니메이션 후 idle 상태 보호 플래그 설정
        if (m_isNetworkTiger) {
            m_protectIdleAfterHit = true;
            m_hitProtectionTimer = 2.0f;  // 2초 동안 보호 (더 길게 설정)
        }
    }

    if (anim->mCurrentFileName == "0208_tiger_dying.fbx")
    {
        CreateLeather();
        Delete();
    }
}
void TigerObject::Fire()
{
    if (mIsFired) return;
    mIsFired = true;

    // 네트워크 호랑이도 실제 공격 오브젝트 생성 (0.4초 후에 호출됨)
    if (m_isNetworkTiger) {
        OutputDebugString(L"[TigerObject] Network tiger Fire() called - creating attack object\n");
    }

    // 로컬 호랑이만 실제 공격 오브젝트 생성
    OutputDebugString(L"[TigerObject] Fire() called - creating attack object\n");

    // 호랑이의 실제 위치를 고려한 공격 오브젝트 생성
    Transform* tigerTransform = GetComponent<Transform>();
    if (tigerTransform) {
        // Original과 동일하게 호랑이의 Transform을 기준으로 상대적 위치 사용
        Object* obj = new TigerAttackObject(m_scene, m_scene->AllocateId(), m_id);
        obj->AddComponent(new Transform{ {0.0f, 6.0f, 18.0f} });  // Original과 동일한 상대적 위치
        obj->AddComponent(new Collider{ {0.0f, 0.0f, 0.0f}, {4.0f, 6.0f, 8.0f} });
        m_scene->AddObj(obj);
        
        // 위치 정보 로그 추가 (플레이어 위치 포함)
        XMFLOAT3 tigerPosFloat;
        XMStoreFloat3(&tigerPosFloat, tigerTransform->GetPosition());
        
        // 플레이어 위치 가져오기
        PlayerObject* player = m_scene->GetObj<PlayerObject>();
        XMFLOAT3 playerPosFloat = {0.0f, 0.0f, 0.0f};
        if (player) {
            Transform* playerTransform = player->GetComponent<Transform>();
            if (playerTransform) {
                XMVECTOR playerPos = playerTransform->GetPosition();
                XMStoreFloat3(&playerPosFloat, playerPos);
            }
        }
        
        wchar_t debugMsg[256];
        swprintf_s(debugMsg, L"[TigerObject] Tiger pos: (%.1f, %.1f, %.1f), Player pos: (%.1f, %.1f, %.1f)\n", 
                   tigerPosFloat.x, tigerPosFloat.y, tigerPosFloat.z,
                   playerPosFloat.x, playerPosFloat.y, playerPosFloat.z);
        OutputDebugString(debugMsg);
        
        OutputDebugString(L"[TigerObject] Attack object created and added to scene\n");
        
        // 공격 오브젝트 생성 후 즉시 OBB 업데이트
        Collider* attackCollider = obj->GetComponent<Collider>();
        if (attackCollider) {
            Transform* attackTransform = obj->GetComponent<Transform>();
            if (attackTransform) {
                // 부모(호랑이)의 Transform과 결합된 최종 월드 매트릭스 계산
                XMMATRIX attackLocalM = attackTransform->GetTransformM();
                XMMATRIX tigerWorldM = tigerTransform->GetFinalM();
                XMMATRIX finalM = attackLocalM * tigerWorldM;
                
                attackTransform->SetFinalM(finalM);
                attackCollider->UpdateOBB(finalM);
                
                OutputDebugString(L"[TigerObject] Attack object OBB updated with parent transform\n");
            }
        }
    } else {
        // Transform이 없는 경우에도 Original과 동일한 상대적 위치 사용
        Object* obj = new TigerAttackObject(m_scene, m_scene->AllocateId(), m_id);
        obj->AddComponent(new Transform{ {0.0f, 6.0f, 18.0f} });  // Original과 동일한 상대적 위치
        obj->AddComponent(new Collider{ {0.0f, 0.0f, 0.0f}, {4.0f, 6.0f, 8.0f} });
        m_scene->AddObj(obj);
        
        OutputDebugString(L"[TigerObject] Attack object created (no transform)\n");
    }
}

void TigerObject::Hit()
{
    if (mIsHitted) return;
    mIsHitted = true;
    
    OutputDebugString(L"[TigerObject] Hit() called - mIsHitted set to true\n");
    
    // 네트워크 호랑이도 즉시 hit 애니메이션 재생 (서버 응답과 관계없이)
    if (m_isNetworkTiger) {
        OutputDebugString(L"[TigerObject] Network tiger hit - sending hit event to server\n");
        Framework* framework = m_scene->GetFramework();
        if (framework && framework->IsNetworkEnabled()) {
            NetworkManager& networkManager = framework->GetNetworkManager();
            if (networkManager.IsLoggedIn()) {
                networkManager.SendTigerHit(m_networkTigerID, mLife); // 현재 생명력 전송
                OutputDebugString(L"[Tiger] Network tiger hit event sent to server\n");
            }
        }
        // 네트워크 호랑이도 즉시 hit 애니메이션 재생
        ChangeState("0208_tiger_hit.fbx");
        OutputDebugString(L"[TigerObject] Network tiger hit animation state changed\n");
        return;
    }
    
    // 로컬 호랑이만 즉시 생명력 감소 및 애니메이션 재생
    OutputDebugString(L"[TigerObject] Local tiger hit - reducing life\n");
    --mLife;
    
    if (mLife == 0)
    {
        OutputDebugString(L"[TigerObject] Tiger life reached 0 - calling Dead()\n");
        Dead();
        return;
    }
    ChangeState("0208_tiger_hit.fbx");
    OutputDebugString(L"[TigerObject] Local tiger hit animation state changed\n");
}

void TigerObject::HitByRiceCake()
{
    if (mIsHitted) return;
    mIsHitted = true;
    
    OutputDebugString(L"[TigerObject] HitByRiceCake() called - mIsHitted set to true\n");
    
    // 네트워크 호랑이도 즉시 hit 애니메이션 재생 (서버 응답과 관계없이)
    if (m_isNetworkTiger) {
        OutputDebugString(L"[TigerObject] Network tiger hit by ricecake - sending hit event to server\n");
        Framework* framework = m_scene->GetFramework();
        if (framework && framework->IsNetworkEnabled()) {
            NetworkManager& networkManager = framework->GetNetworkManager();
            if (networkManager.IsLoggedIn()) {
                // 떡으로 맞으면 생명력 대폭 감소 (한방에 죽일 수 있도록)
                mLife -= 3;
                networkManager.SendTigerHit(m_networkTigerID, mLife); // 현재 생명력 전송
                OutputDebugString(L"[Tiger] Network tiger hit by ricecake event sent to server\n");
            }
        }
        // 네트워크 호랑이도 즉시 hit 애니메이션 재생
        ChangeState("0208_tiger_hit.fbx");
        OutputDebugString(L"[TigerObject] Network tiger hit by ricecake animation state changed\n");
        return;
    }
    
    // 로컬 호랑이만 즉시 생명력 감소 및 애니메이션 재생
    OutputDebugString(L"[TigerObject] Local tiger hit by ricecake - reducing life\n");
    // 떡으로 맞으면 생명력 대폭 감소 (한방에 죽일 수 있도록)
    mLife -= 3;
    
    if (mLife <= 0)
    {
        Dead();
        return;
    }
    ChangeState("0208_tiger_hit.fbx");
}

void TigerObject::Dead()
{
    ChangeState("0208_tiger_dying.fbx");
}

void TigerObject::CalcTime(float deltaTime) 
{
    Animation* anim = GetComponent<Animation>();

    // 네트워크 호랑이는 서버에서 관리되므로 타이머만 업데이트
    if (m_isNetworkTiger) {
        // hit 보호 타이머 업데이트
        if (m_hitProtectionTimer > 0.0f) {
            m_hitProtectionTimer -= deltaTime;
            if (m_hitProtectionTimer <= 0.0f) {
                m_protectIdleAfterHit = false;  // 보호 플래그 해제
            }
        }
        
        if (anim->mCurrentFileName == "0113_tiger_walk.fbx")
        {
            mSearchTime += deltaTime;
        }

        if (anim->mCurrentFileName == "0722_tiger_run.fbx")
        {
            // run 애니메이션 중에는 별도 타이머 업데이트 없음 (서버에서 관리)
        }

        if (anim->mCurrentFileName == "0208_tiger_attack.fbx") 
        {
            mElapseTime += deltaTime;
            // 네트워크 호랑이도 Original과 동일하게 0.4초 후에 Fire() 호출
            if (mElapseTime >= 0.4f) {
                // 네트워크 호랑이는 서버에서 공격 신호를 받았을 때만 Fire() 호출
                if (m_serverAttackSignal) {
                    Fire();
                    m_serverAttackSignal = false;  // 신호 소비
                }
            }
            if (mElapseTime >= 0.8f) TimeOut();
        }
        else
        {
            mAttackTime += deltaTime;
        }

            if (anim->mCurrentFileName == "0208_tiger_hit.fbx")
    {
        mElapseTime += deltaTime;
        if (mElapseTime > 0.8f) TimeOut();  // Original과 동일한 0.8초 타이머
    }

        if (anim->mCurrentFileName == "0208_tiger_dying.fbx")
        {
            mElapseTime += deltaTime;
            // 네트워크 호랑이의 죽는 애니메이션이 끝나면 가죽 생성 후 삭제
            if (mElapseTime > 1.9f) {
                CreateLeather();
                Delete();
            }
        }
        return;
    }

    // 로컬 호랑이만 Fire()와 TimeOut() 호출 (Original과 동일)
    if (anim->mCurrentFileName == "0113_tiger_walk.fbx")
    {
        mSearchTime += deltaTime;
    }

    if (anim->mCurrentFileName == "0208_tiger_attack.fbx") 
    {
        mElapseTime += deltaTime;
        if (mElapseTime >= 0.4f) Fire();
        if (mElapseTime >= 0.8f) TimeOut();
    }
    else
    {
        mAttackTime += deltaTime;
    }

    if (anim->mCurrentFileName == "0208_tiger_hit.fbx")
    {
        mElapseTime += deltaTime;
        if (mElapseTime > 0.8f) TimeOut();  // Original과 동일한 0.8초 타이머
    }

    if (anim->mCurrentFileName == "0208_tiger_dying.fbx")
    {
        mElapseTime += deltaTime;
        if (mElapseTime > 1.9f) TimeOut();
    }
}

void TigerObject::CreateLeather()
{
    Transform* transform = GetComponent<Transform>();
    XMVECTOR pos = transform->GetPosition();

    Object* objectPtr = nullptr;
    float scale = 0.1f;
    objectPtr = new TigerLeather(m_scene, m_scene->AllocateId());
    objectPtr->AddComponent(new Transform{ pos });
    objectPtr->AddComponent(new AdjustTransform{ {0.0f * scale, 100.0f * scale, 0.0f * scale}, {-90.0f, 0.0f, 0.0f}, {scale, scale, scale} });
    objectPtr->AddComponent(new Mesh{ "tiger_leather.fbx" });
    objectPtr->AddComponent(new Texture{ L"tigerLeather", 1.0f, 0.6f });
    objectPtr->AddComponent(new Collider{ {0.0f, 100.0f * scale, 0.0f}, {90.0f * scale, 100.0f * scale, 20.0f * scale} });
    objectPtr->AddComponent(new Gravity);
    m_scene->AddObj(objectPtr);
}






void PlayerAttackObject::OnUpdate(GameTimer& gTimer)
{
    mElapseTime += gTimer.DeltaTime();
    if (mElapseTime >= 0.1) Delete();
    Object::OnUpdate(gTimer);
}

void PlayerAttackObject::OnProcessCollision(Object& other, XMVECTOR collisionNormal, float penetration)
{
    TigerObject* tiger = dynamic_cast<TigerObject*>(&other);
    if (tiger)
    {
        tiger->Hit();
        Delete();  // 공격 오브젝트 삭제
        return;
    }

    // 다른 오브젝트와의 충돌 처리
    Transform* transform = GetComponent<Transform>();
    XMVECTOR pos = transform->GetPosition();
    pos += -collisionNormal * penetration;
    transform->SetPosition(pos);
}

void TigerMockup::OnUpdate(GameTimer& gTimer)
{
    static float randYaw = uid(dre);
    Transform* transform = GetComponent<Transform>();

    mSearchTime += gTimer.DeltaTime();

    if (mSearchTime > 2.0f)
    {
        mSearchTime = 0.0f;
        randYaw = uid(dre);
        transform->SetRotation({ 0.0f, randYaw, 0.0f });
    }

    XMVECTOR dir = XMVector3TransformNormal({ 0.0f, 0.0f, 1.0f }, transform->GetRotationM());
    dir = XMVector3Normalize(dir);
    XMVECTOR pos = transform->GetPosition();
    transform->SetPosition(pos + dir * mWalkSpeed * gTimer.DeltaTime());
    Object::OnUpdate(gTimer);
}

void TigerMockup::OnProcessCollision(Object& other, XMVECTOR collisionNormal, float penetration)
{
    PlayerObject* player = dynamic_cast<PlayerObject*>(&other);
    if (player && !m_hasCollided)  // 충돌 플래그 확인
    {
        // 위치 정보 로그 추가
        Transform* godTransform = GetComponent<Transform>();
        Transform* playerTransform = player->GetComponent<Transform>();
        
        XMFLOAT3 godPos, playerPos;
        XMStoreFloat3(&godPos, godTransform->GetPosition());
        XMStoreFloat3(&playerPos, playerTransform->GetPosition());
        
        wchar_t debugMsg[256];
        swprintf_s(debugMsg, L"[TigerMockup] GOD collision detected! GOD pos: (%.1f, %.1f, %.1f), Player pos: (%.1f, %.1f, %.1f)\n", 
                   godPos.x, godPos.y, godPos.z, playerPos.x, playerPos.y, playerPos.z);
        OutputDebugString(debugMsg);
        
        m_hasCollided = true;  // 충돌 플래그 설정
        
        // Hunting Stage로 전환
        m_scene->SetStage(L"Hunting");
        
        // 서버에 호랑이 재생성 요청을 보내기 위해 로그 출력
        OutputDebugString(L"[TigerMockup] GOD collision detected, requesting tiger respawn from server\n");
        
        // 네트워크 매니저를 통해 서버에 호랑이 재생성 요청
        Framework* framework = m_scene->GetFramework();
        if (framework) {
            OutputDebugString(L"[TigerMockup] Framework found\n");
            if (framework->IsNetworkEnabled()) {
                OutputDebugString(L"[TigerMockup] Network is enabled\n");
                NetworkManager& networkManager = framework->GetNetworkManager();
                if (networkManager.IsLoggedIn()) {
                    OutputDebugString(L"[TigerMockup] Network manager is logged in\n");
                    // 서버에 호랑이 재생성 요청 패킷 전송
                    networkManager.SendTigerRespawnRequest();
                    OutputDebugString(L"[TigerMockup] Tiger respawn request sent to server\n");
                } else {
                    OutputDebugString(L"[TigerMockup] Network manager is not logged in\n");
                }
            } else {
                OutputDebugString(L"[TigerMockup] Network is not enabled\n");
            }
        } else {
            OutputDebugString(L"[TigerMockup] Framework not found\n");
        }
    }

    Transform* transform = GetComponent<Transform>();
    XMVECTOR pos = transform->GetPosition();
    pos += -collisionNormal * penetration;
    transform->SetPosition(pos);
}

void TigerAttackObject::OnUpdate(GameTimer& gTimer)
{
    mElapseTime += gTimer.DeltaTime();
    if (mElapseTime >= 0.8f) Delete();  // 공격 지속 시간 (Original과 동일)
    Object::OnUpdate(gTimer);
}

void TigerAttackObject::OnProcessCollision(Object& other, XMVECTOR collisionNormal, float penetration)
{
    PlayerObject* player = dynamic_cast<PlayerObject*>(&other);
    if (player)
    {
        OutputDebugString(L"[TigerAttackObject] Collision with PlayerObject detected - calling player->Hit()\n");
        // 플레이어가 피격되었을 때의 처리
        // 여기서는 단순히 로그만 출력하고 공격 오브젝트는 삭제
        Delete();  // 공격 오브젝트 삭제
        return;
    }

    // 다른 오브젝트와의 충돌 처리
    Transform* transform = GetComponent<Transform>();
    XMVECTOR pos = transform->GetPosition();
    pos += -collisionNormal * penetration;
    transform->SetPosition(pos);
}

void TigerLeather::OnUpdate(GameTimer& gTimer)
{
    Transform* transform = GetComponent<Transform>();
    XMFLOAT3 rot{};
    XMStoreFloat3(&rot, transform->GetRotation());

    rot.y += 60.0f * gTimer.DeltaTime();

    transform->SetRotation(XMLoadFloat3(&rot));
    Object::OnUpdate(gTimer);
}

void TigerLeather::OnProcessCollision(Object& other, XMVECTOR collisionNormal, float penetration)
{
    PlayerObject* player = dynamic_cast<PlayerObject*>(&other);
    if (player) Delete();
}

void RotFenceObject::OnUpdate(GameTimer& gTimer)
{
    Transform* transform = GetComponent<Transform>();
    XMFLOAT3 rot{};
    XMStoreFloat3(&rot, transform->GetRotation());
    rot.z += 30.0f * gTimer.DeltaTime();
    transform->SetRotation(XMLoadFloat3(&rot));
    Object::OnUpdate(gTimer);
}

void AxeObject::OnUpdate(GameTimer& gTimer)
{
    Transform* transform = GetComponent<Transform>();
    if (m_parent_id != -1)
    {
        XMMATRIX finalM = transform->GetTransformM();
        if (m_parent_id != -1) {
            Object* parentObj = m_scene->GetObjFromId(m_parent_id);
            if (parentObj) {
                Transform* parentTransform = parentObj->GetComponent<Transform>();
                finalM = finalM * parentTransform->GetFinalM();
            }
        }
        transform->SetFinalM(finalM);

        Collider* collider = GetComponent<Collider>();
        if (collider) {
            collider->UpdateOBB(finalM);
        }
    }
    else
    {
        Object::OnUpdate(gTimer);
    }
}

void AxeObject::OnProcessCollision(Object& other, XMVECTOR collisionNormal, float penetration)
{
    PlayerObject* player = dynamic_cast<PlayerObject*>(&other);
    if (player)
    {
        m_scene->SetStage(L"End");
    }
    Object::OnProcessCollision(other, collisionNormal, penetration);
}

void RicecakeObject::SetDir(XMVECTOR dir)
{
    XMStoreFloat3(&mDir, dir);
}

void RicecakeObject::OnUpdate(GameTimer& gTimer)
{
    Transform* transform = GetComponent<Transform>();
    XMVECTOR pos = transform->GetPosition();
    pos += XMLoadFloat3(&mDir) * mSpeed * gTimer.DeltaTime();
    transform->SetPosition(pos);

    Object::OnUpdate(gTimer);
}

void RicecakeObject::OnProcessCollision(Object& other, XMVECTOR collisionNormal, float penetration)
{
    PlayerAttackObject* pa = dynamic_cast<PlayerAttackObject*>(&other);
    if (pa) return;
    TigerAttackObject* ta = dynamic_cast<TigerAttackObject*>(&other);
    if (ta) return;
    RicecakeObject* ricecake = dynamic_cast<RicecakeObject*>(&other);
    if (ricecake)
    {
        Transform* transform = GetComponent<Transform>();
        XMVECTOR pos = transform->GetPosition();
        pos -= collisionNormal * penetration;
        transform->SetPosition(pos);
        return;
    }
    Delete();
    Object::OnProcessCollision(other, collisionNormal, penetration);
}

void RicecakeObject::LateUpdate(GameTimer& gTimer)
{
    Transform* transform = GetComponent<Transform>();
    XMVECTOR pos = transform->GetPosition();
    char outstatus = m_scene->ClampToBounds(pos, { 0.0f, 0.0f, 0.0f });
    transform->SetPosition(pos);

    bool CreatedByTree = XMVector3Equal(XMLoadFloat3(&mDir), XMVectorZero());

    if (outstatus)
    {
        if (CreatedByTree)
        {
            Gravity* gravity = GetComponent<Gravity>();
            gravity->ResetElapseTime();
        }
        else
        {
            Delete();
        }
    }

    transform->SetFinalM(transform->GetTransformM());

    XMMATRIX world = transform->GetFinalM();
    XMMATRIX adjustM = XMMatrixIdentity();
    AdjustTransform* adjustTrnasform = GetComponent<AdjustTransform>();
    if (adjustTrnasform) {
        adjustM = adjustTrnasform->GetTransformM();
    }
    memcpy(m_mappedData, &XMMatrixTranspose(adjustM * world), sizeof(XMMATRIX));

    ProcessAnimation(gTimer);

    Texture* texture = GetComponent<Texture>();
    float powValue = 1.0f;
    float ambiantValue = 0.4f;
    if (texture) {
        ambiantValue = texture->mAmbiantValue;
        powValue = texture->mPowValue;
    }
    memcpy(m_mappedData + sizeof(XMFLOAT4X4) * 91 + sizeof(int) * 4, &powValue, sizeof(float));
    memcpy(m_mappedData + sizeof(XMFLOAT4X4) * 91 + sizeof(int) * 4 + sizeof(float), &ambiantValue, sizeof(float));
}

void TreeObject::OnUpdate(GameTimer& gTimer)
{
    mElapseTime += gTimer.DeltaTime();
    Object::OnUpdate(gTimer);
}

void TreeObject::OnProcessCollision(Object& other, XMVECTOR collisionNormal, float penetration)
{
    PlayerAttackObject* pa = dynamic_cast<PlayerAttackObject*>(&other);
    if (pa && mElapseTime > 1.0f)
    {
        mElapseTime = 0.0f;

        Transform* transform = GetComponent<Transform>();
        XMVECTOR pos = transform->GetPosition();
        float yaw = uid(dre);
        XMVECTOR offset = XMVector3TransformNormal(XMVECTOR{ 0.0f, 50.0f, 20.0f }, XMMatrixRotationY(yaw));
        float scale = 0.03f;
        RicecakeObject* obj = new RicecakeObject(m_scene, m_scene->AllocateId());
        obj->AddComponent(new Transform{ pos + offset });
        obj->AddComponent(new AdjustTransform{ {-20.0f * scale, 22.0f * scale, 0.0f}, {0.0f, 0.0f, -90.0f}, {scale, scale, scale} });
        obj->AddComponent(new Mesh{ "ricecake.fbx" });
        obj->AddComponent(new Texture{ L"ricecake", 1.0f, 0.4f });
        obj->AddComponent(new Gravity);
        obj->AddComponent(new Collider{ {0.0f, 30.0f * scale, 0.0f}, {25.0f * scale, 30.0f * scale, 25.0f * scale} });
        m_scene->AddObj(obj);
    }
    Object::OnProcessCollision(other, collisionNormal, penetration);
}

void GoToBaseObject::OnUpdate(GameTimer& gTimer)
{
    Texture* texture = GetComponent<Texture>();
    if (texture && m_scene->HasEnoughLeather())
    {
        texture->mAmbiantValue = fabs(sinf(mElapseTime) * 2.0f) + 0.4f;
        mElapseTime += gTimer.DeltaTime();
       
    }
    Object::OnUpdate(gTimer);
}

void GoToBaseObject::OnProcessCollision(Object& other, XMVECTOR collisionNormal, float penetration)
{
    PlayerObject* player = dynamic_cast<PlayerObject*>(&other);
    if (player && m_scene->HasEnoughLeather())
    {
        m_scene->SetStage(L"Base");
    }
    Object::OnProcessCollision(other, collisionNormal, penetration);
}


void GodObject::OnProcessCollision(Object& other, XMVECTOR collisionNormal, float penetration)
{
    PlayerObject* player = dynamic_cast<PlayerObject*>(&other);
    if (player && m_scene->HasEnoughLeather())
    {
        m_scene->SetStage(L"God");
    }
    Object::OnProcessCollision(other, collisionNormal, penetration);
}

void TitleQuadObject::OnUpdate(GameTimer& gTimer)
{
    CameraObject* camera = m_scene->GetObj<CameraObject>();
    m_parent_id = camera->GetId();

    BYTE* keyState = m_scene->GetFramework()->GetKeyState();
    if ((keyState[VK_RETURN] & 0x88) == 0x80)
    {
        m_scene->SetStage(L"Base");
    }
}

void SisterObject::OnProcessCollision(Object& other, XMVECTOR collisionNormal, float penetration)
{
    PlayerObject* player = dynamic_cast<PlayerObject*>(&other);
    if (player && !mIsQuadAble)
    {
        mIsQuadAble = true;

        m_scene->SetTigerQuestState(true);

        Object* obj = new SisterQuadObject(m_scene, m_scene->AllocateId(), m_id);
        obj->AddComponent(new Transform{ {-5.0f, 10.0f, 0.1f}, {-90.0f, 180.0f, 0.0f}, {30.0f, 0.0f, 25.0f} });
        obj->AddComponent(new Mesh{ "Quad" });
        obj->AddComponent(new Texture{ L"Quest", -1.0f, 0.4f });
        m_scene->AddObj(obj);


    }
    Object::OnProcessCollision(other, collisionNormal, penetration);
}

void EndQuadObject::OnUpdate(GameTimer& gTimer)
{
    CameraObject* camera = m_scene->GetObj<CameraObject>();
    m_parent_id = camera->GetId();
}

void SisterQuadObject::OnUpdate(GameTimer& gTimer)
{
    if (m_scene->HasEnoughLeather())
    {
        Texture* texture = GetComponent<Texture>();
        texture->mName = L"GoToGod";
    }

 
}

void LifeQuadObject::OnUpdate(GameTimer& gTimer)
{
    CameraObject* camera = m_scene->GetObj<CameraObject>();
    m_parent_id = camera->GetId();

    PlayerObject* player = m_scene->GetObj<PlayerObject>();
    int playerLifeCount = player->GetLifeCount();
    Texture* texture = GetComponent<Texture>();
    switch (playerLifeCount)
    {
    case 0:
        texture->mName = L"Life0";
        break;
    case 1:
        texture->mName = L"Life1";
        break;
    case 2:
        texture->mName = L"Life2";
        break;
    case 3:
        texture->mName = L"Life3";
        break;
    default:
        break;
    }

}

void BoyIconQuadObject::OnUpdate(GameTimer& gTimer)
{
    CameraObject* camera = m_scene->GetObj<CameraObject>();
    m_parent_id = camera->GetId();
}

void RiceCakeQuadObject::OnUpdate(GameTimer& gTimer)
{
    CameraObject* camera = m_scene->GetObj<CameraObject>();
    m_parent_id = camera->GetId();
    PlayerObject* player = m_scene->GetObj<PlayerObject>();

    Texture* texture = GetComponent<Texture>();

    int riceCakeCount = player->GetRicecakeCount();

    switch (riceCakeCount)
    {
    case 0:
        texture->mName = L"RiceCake0";
        break;
    case 1:
        texture->mName = L"RiceCake1";
        break;
    case 2:
        texture->mName = L"RiceCake2";
        break;
    case 3:
        texture->mName = L"RiceCake3";
        break;
    case 4:
        texture->mName = L"RiceCake4";
        break;
    default:
        break;
    }


}

void TigerLeatherQuadObject::OnUpdate(GameTimer& gTimer)
{
    CameraObject* camera = m_scene->GetObj<CameraObject>();
    m_parent_id = camera->GetId();
    Texture* texture = GetComponent<Texture>();

    int LeatherCount = m_scene->GetLeatherCount();
    switch (LeatherCount)
    {
    case 0:
        texture->mName = L"TigerLeather0";
        break;
    case 1:
        texture->mName = L"TigerLeather1";
        break;
    case 2:
        texture->mName = L"TigerLeather2";
        break;
    case 3:
        texture->mName = L"TigerLeather3";
        break;
    case 4:
        texture->mName = L"TigerLeather4";
        break;
    case 5:
        texture->mName = L"TigerLeather5";
        break;
    default:
        break;
    }

    if (!m_scene->IsTigerQuestAccepted())
    {
        texture->mName = L"White";
    }

}

void TestObject::OnProcessCollision(Object& other, XMVECTOR collisionNormal, float penetration)
{
}

GrassGroupObject::GrassGroupObject(Scene* scene, uint32_t id, uint32_t parentId) : Object(scene, id, parentId)
{
    Object* objectPtr = nullptr;
    float scale = 30.0f;
    float offset = 5.0f;
    objectPtr = new TestObject(scene, scene->AllocateId(), id);
    objectPtr->AddComponent(new Transform{ {0.0f, 0.0f, 0.0f} });
    objectPtr->AddComponent(new AdjustTransform{ {0.0f, 0.0f, 0.0f}, {-90.0f, 0.0f, 0.0f}, {scale, scale, scale} });
    objectPtr->AddComponent(new Mesh{ "grass_low.fbx" });
    objectPtr->AddComponent(new Texture{ L"Green", 1.0f, 0.4f });
    scene->AddObj(objectPtr);
    objectPtr = new TestObject(scene, scene->AllocateId(), id);
    objectPtr->AddComponent(new Transform{ {-offset, 0.0f, 0.0f} });
    objectPtr->AddComponent(new AdjustTransform{ {0.0f, 0.0f, 0.0f}, {-90.0f, 0.0f, 0.0f}, {scale, scale, scale} });
    objectPtr->AddComponent(new Mesh{ "grass_low.fbx" });
    objectPtr->AddComponent(new Texture{ L"Green", 1.0f, 0.4f });
    scene->AddObj(objectPtr);
    objectPtr = new TestObject(scene, scene->AllocateId(), id);
    objectPtr->AddComponent(new Transform{ {offset, 0.0f, 0.0f} });
    objectPtr->AddComponent(new AdjustTransform{ {0.0f, 0.0f, 0.0f}, {-90.0f, 0.0f, 0.0f}, {scale, scale, scale} });
    objectPtr->AddComponent(new Mesh{ "grass_low.fbx" });
    objectPtr->AddComponent(new Texture{ L"Green", 1.0f, 0.4f });
    scene->AddObj(objectPtr);
    objectPtr = new TestObject(scene, scene->AllocateId(), id);
    objectPtr->AddComponent(new Transform{ {0.0f, 0.0f, offset} });
    objectPtr->AddComponent(new AdjustTransform{ {0.0f, 0.0f, 0.0f}, {-90.0f, 0.0f, 0.0f}, {scale, scale, scale} });
    objectPtr->AddComponent(new Mesh{ "grass_low.fbx" });
    objectPtr->AddComponent(new Texture{ L"Green", 1.0f, 0.4f });
    scene->AddObj(objectPtr);
    objectPtr = new TestObject(scene, scene->AllocateId(), id);
    objectPtr->AddComponent(new Transform{ {0.0f, 0.0f, -offset} });
    objectPtr->AddComponent(new AdjustTransform{ {0.0f, 0.0f, 0.0f}, {-90.0f, 0.0f, 0.0f}, {scale, scale, scale} });
    objectPtr->AddComponent(new Mesh{ "grass_low.fbx" });
    objectPtr->AddComponent(new Texture{ L"Green", 1.0f, 0.4f });
    scene->AddObj(objectPtr);
}

void GrassGroupObject::RandomRot()
{
    Transform* transform = GetComponent<Transform>();
    float yaw = uid(dre);
    transform->SetRotation({ 0.0f, yaw, 0.0f });
}

void TigerObject::SetNetworkTransform(float x, float y, float z, float rotY)
{
    if (m_isNetworkTiger) {
        Transform* transform = GetComponent<Transform>();
        if (transform != nullptr) {
            // Y-위치는 Gravity 컴포넌트가 관리하므로 X, Z만 서버에서 동기화
            XMVECTOR currentPos = transform->GetPosition();
            XMVECTOR newPos = {x, XMVectorGetY(currentPos), z, 1.0f};
            transform->SetPosition(newPos);
            
            // Original 클라이언트처럼 거리에 따라 다른 회전 로직 적용
            PlayerObject* player = m_scene->GetObj<PlayerObject>();
            if (player) {
                Transform* playerTransform = player->GetComponent<Transform>();
                if (playerTransform) {
                    XMVECTOR playerPos = playerTransform->GetPosition();
                    float distance = XMVectorGetX(XMVector3Length(playerPos - newPos));
                    
                    // Original 클라이언트와 동일한 거리 기준 사용
                    if (distance < 200.0f) {
                        // 플레이어가 탐색 범위 내에 있을 때만 플레이어를 향해 회전
                        XMVECTOR dir = XMVector3Normalize(playerPos - newPos);
                        float yaw = atan2f(XMVectorGetX(dir), XMVectorGetZ(dir)) * 180.0f / 3.141592f;
                        
                        // 현재 회전과 목표 회전의 차이를 계산하여 자연스러운 회전 적용
                        XMVECTOR currentRotation = transform->GetRotation();
                        float currentYaw = XMVectorGetY(currentRotation);
                        
                        // 회전 차이를 계산하고 부드럽게 보간
                        float rotationDiff = yaw - currentYaw;
                        
                        // 180도 이상 차이나는 경우 반대 방향으로 회전
                        if (rotationDiff > 180.0f) rotationDiff -= 360.0f;
                        if (rotationDiff < -180.0f) rotationDiff += 360.0f;
                        
                        // 부드러운 회전을 위해 회전 속도 제한 (60fps 기준으로 3도/프레임)
                        float maxRotationPerFrame = 3.0f;
                        if (abs(rotationDiff) > maxRotationPerFrame) {
                            if (rotationDiff > 0) {
                                rotationDiff = maxRotationPerFrame;
                            } else {
                                rotationDiff = -maxRotationPerFrame;
                            }
                        }
                        
                        float newYaw = currentYaw + rotationDiff;
                        transform->SetRotation({0.0f, newYaw, 0.0f});
                    } else {
                        // 플레이어가 탐색 범위 밖에 있을 때는 이동 방향에 따른 자연스러운 회전
                        // 이전 위치와 현재 위치를 비교하여 이동 방향 계산
                        XMVECTOR zeroVector = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
                        if (!XMVector4Equal(m_previousNetworkPos, zeroVector)) {
                            XMVECTOR moveDir = newPos - m_previousNetworkPos;
                            float moveDistance = XMVectorGetX(XMVector3Length(moveDir));
                            
                            // 이동 거리가 충분할 때만 회전 적용 (떨림 방지)
                            if (moveDistance > 0.1f) {
                                // 이동 방향으로 회전 (Original 클라이언트의 Search 로직과 동일)
                                XMVECTOR normalizedDir = XMVector3Normalize(moveDir);
                                float yaw = atan2f(XMVectorGetX(normalizedDir), XMVectorGetZ(normalizedDir)) * 180.0f / 3.141592f;
                                
                                // 현재 회전과 목표 회전의 차이를 계산하여 자연스러운 회전 적용
                                XMVECTOR currentRotation = transform->GetRotation();
                                float currentYaw = XMVectorGetY(currentRotation);
                                
                                // 회전 차이를 계산하고 부드럽게 보간
                                float rotationDiff = yaw - currentYaw;
                                
                                // 180도 이상 차이나는 경우 반대 방향으로 회전
                                if (rotationDiff > 180.0f) rotationDiff -= 360.0f;
                                if (rotationDiff < -180.0f) rotationDiff += 360.0f;
                                
                                // 부드러운 회전을 위해 회전 속도 제한 (60fps 기준으로 3도/프레임)
                                float maxRotationPerFrame = 3.0f;
                                if (abs(rotationDiff) > maxRotationPerFrame) {
                                    if (rotationDiff > 0) {
                                        rotationDiff = maxRotationPerFrame;
                                    } else {
                                        rotationDiff = -maxRotationPerFrame;
                                    }
                                }
                                
                                float newYaw = currentYaw + rotationDiff;
                                transform->SetRotation({0.0f, newYaw, 0.0f});
                            }
                        }
                    }
                }
            }
            
            // 이전 위치 저장 (다음 프레임에서 사용)
            m_previousNetworkPos = newPos;
            
            // OBB 업데이트
            Collider* collider = GetComponent<Collider>();
            if (collider) {
                XMMATRIX finalM = transform->GetTransformM();
                if (m_parent_id != -1) {
                    Object* parentObj = m_scene->GetObjFromId(m_parent_id);
                    if (parentObj) {
                        Transform* parentTransform = parentObj->GetComponent<Transform>();
                        finalM = finalM * parentTransform->GetFinalM();
                    }
                }
                transform->SetFinalM(finalM);
                collider->UpdateOBB(finalM);
            }
        }
    }
}

void TigerObject::SetNetworkAnimation(const std::string& animationFile, float animationTime)
{
    if (m_isNetworkTiger) {
        Animation* anim = GetComponent<Animation>();
        if (anim) {
            // 새로운 애니메이션이 시작될 때만 상태 변경
            if (anim->mCurrentFileName != animationFile) {
                // 현재 애니메이션이 끝나기 전까지는 변경하지 않음 (버벅거림 방지)
                // 단, 공격 애니메이션은 즉시 변경 (반응성 향상)
                if (animationFile == "0208_tiger_attack.fbx" ||
                    animationFile == "0208_tiger_hit.fbx" ||
                    animationFile == "0208_tiger_dying.fbx") {
                    ChangeState(animationFile);
                    // Original 클라이언트와 동일하게 0으로 리셋
                    anim->ResetAnim(animationFile, 0.0f);
                    mElapseTime = 0.0f;
                    m_serverAttackSignal = false;  // 공격 신호 리셋
                } else {
                    // 일반 애니메이션은 현재 애니메이션이 끝날 때까지 기다림
                    // 애니메이션 시간이 0.9 이상이면 거의 끝난 것으로 간주
                    if (anim->mAnimationTime >= 0.9f) {
                        ChangeState(animationFile);
                        // Original 클라이언트와 동일하게 0으로 리셋
                        anim->ResetAnim(animationFile, 0.0f);
                        mElapseTime = 0.0f;
                    }
                }
            }
            
            // 서버 시간을 직접 설정하지 않음 (떨림 방지)
            // Original 클라이언트와 동일하게 클라이언트에서 자연스럽게 재생
        }
    }
}