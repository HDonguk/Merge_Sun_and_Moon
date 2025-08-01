#include "Object.h"
#include "GameTimer.h"
#include "Scene.h"
#include "DXSampleHelper.h"
#include <random>
#include "Framework.h"
#include "NetworkManager.h"

std::random_device rd;  // ù ��° rd ��ü
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
        
        // 모든 오브젝트의 애니메이션 시간 증가 (네트워크 호랑이도 포함)
        // 네트워크 호랑이의 경우 SetNetworkAnimation에서 서버 시간으로 동기화됨
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
    // 네트워크 플레이어는 입력 처리하지 않음
    if (!m_isNetworkPlayer) {
        ProcessInput(gTimer);
    }
    ProcessRicecakeMockUp();
    Object::OnUpdate(gTimer);
}

void PlayerObject::OnProcessCollision(Object& other, XMVECTOR collisionNormal, float penetration)
{
    Transform* transform = GetComponent<Transform>();

    PlayerAttackObject* pa = dynamic_cast<PlayerAttackObject*>(&other);
    if (pa) return;

    TigerAttackObject* ta = dynamic_cast<TigerAttackObject*>(&other);
    if (ta) // ȣ���� ���ݿ� ������...
    {
        Hit();
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
        return;
    }

    XMVECTOR pos = transform->GetPosition();
    pos -= collisionNormal * penetration;
    transform->SetPosition(pos);

    float similarity = XMVectorGetX(XMVector3Dot(XMVECTOR{ 0.0f, 1.0f, 0.0f, 0.0f }, -collisionNormal));
    Gravity* gravity = GetComponent<Gravity>();
    if (gravity && similarity > 0.80f) {
        gravity->ResetElapseTime();
    }
}

int PlayerObject::GetRicecakeCount()
{
    return mRicecake;
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
    if (mJumped) return;
    mJumped = true;
    mJumpTime = 0.0f;
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
    if (mLife == 0)
    {
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
        if (mElapseTime > 1.0f) TimeOut();
    }

    if (anim->mCurrentFileName == "boy_dying_fix.fbx")
    {
        mElapseTime += deltaTime;
        if (mElapseTime > 2.0f) TimeOut();
    }

    if (mJumped)
    {
        mJumpTime += deltaTime;
        if (mJumpTime > 1.2f)
        {
            mJumped = false;
        }
    }
}

void PlayerObject::ProcessRicecakeMockUp()
{
    if (mRicecake > 0 && !mHasRicecake)
    {
        mHasRicecake = true;
        float scale = 0.03f;
        Object* obj = new RicecakeMockup(m_scene, m_scene->AllocateId(), m_id);
        obj->AddComponent(new Transform{ {1.5f, 5.0f, -0.2f} });
        obj->AddComponent(new AdjustTransform{ {-20.0f * scale, 22.0f * scale, 0.0f}, {0.0f, 0.0f, -90.0f}, {scale, scale, scale} });
        obj->AddComponent(new Mesh{ "ricecake.fbx" });
        obj->AddComponent(new Texture{ L"ricecake", 1.0f, 0.4f });
        m_scene->AddObj(obj);
    }

    if (mRicecake <= 0) mHasRicecake = false;
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
    memcpy(m_scene->GetConstantBufferMappedData(), &XMMatrixTranspose(invtransformM), sizeof(XMMATRIX)); // ó�� �Ű������� �����ּ�
}

void CameraObject::OnMouseInput(WPARAM wParam, HWND hWnd)
{
    // ���� wnd�� ���� ��ǥ�� �˾ƿ´�
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

    // ���� clamp
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
    // 모든 호랑이(로컬, 네트워크)에 대해 CalcTime 호출
    CalcTime(gTimer.DeltaTime());
    
    // 네트워크 호랑이는 서버에서 관리되므로 클라이언트 AI 로직을 사용하지 않음
    if (!m_isNetworkTiger) {
        TigerBehavior(gTimer);
    } else {
        // 네트워크 호랑이는 서버에서 받은 위치로 보간 (Y-위치는 Gravity 컴포넌트가 관리)
        Transform* transform = GetComponent<Transform>();
        if (transform) {
            XMVECTOR currentPos = transform->GetPosition();
            XMVECTOR targetPos = m_targetPosition;
            
            // X, Z 위치만 보간 (Y는 Gravity 컴포넌트가 관리)
            XMVECTOR currentXZ = {XMVectorGetX(currentPos), 0.0f, XMVectorGetZ(currentPos), 1.0f};
            XMVECTOR targetXZ = {XMVectorGetX(targetPos), 0.0f, XMVectorGetZ(targetPos), 1.0f};
            
            float distance = XMVectorGetX(XMVector3Length(targetXZ - currentXZ));
            if (distance > 0.1f) {  // 충분히 가까우면 보간 중단
                XMVECTOR newXZ = XMVectorLerp(currentXZ, targetXZ, m_interpolationSpeed * gTimer.DeltaTime());
                XMVECTOR newPos = {XMVectorGetX(newXZ), XMVectorGetY(currentPos), XMVectorGetZ(newXZ), 1.0f};
                transform->SetPosition(newPos);
            } else {
                // Y-위치는 현재 값 유지, X, Z만 서버 값으로 설정
                XMVECTOR newPos = {XMVectorGetX(targetPos), XMVectorGetY(currentPos), XMVectorGetZ(targetPos), 1.0f};
                transform->SetPosition(newPos);
            }
            
            // 회전 보간 (가장 짧은 경로로)
            XMVECTOR currentRot = transform->GetRotation();
            float currentY = XMVectorGetY(currentRot);
            float targetY = m_targetRotationY;
            
            // 각도 보간 (가장 짧은 경로로)
            float angleDiff = targetY - currentY;
            if (angleDiff > 180.0f) angleDiff -= 360.0f;
            if (angleDiff < -180.0f) angleDiff += 360.0f;
            
            if (abs(angleDiff) > 1.0f) {  // 충분히 가까우면 회전 중단
                float newY = currentY + angleDiff * m_interpolationSpeed * gTimer.DeltaTime();
                transform->SetRotation({0.0f, newY, 0.0f});
            } else {
                transform->SetRotation({0.0f, targetY, 0.0f});
            }
        }
    }
    
    Object::OnUpdate(gTimer);
}

void TigerObject::OnProcessCollision(Object& other, XMVECTOR collisionNormal, float penetration)
{
    TigerAttackObject* ta = dynamic_cast<TigerAttackObject*>(&other);
    if (ta) return;

    PlayerAttackObject* pa = dynamic_cast<PlayerAttackObject*>(&other);
    RicecakeObject* rc = dynamic_cast<RicecakeObject*>(&other);
    if (pa || rc)
    {
        Hit();
        return;
    }

    Transform* transform = GetComponent<Transform>();
    XMVECTOR pos = transform->GetPosition();
    pos += -collisionNormal * penetration;
    transform->SetPosition(pos);
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

    if (result < 200.f) // �÷��̾ Ž�� ���� �ȿ� ������... 
    {
        if (result < 17.0f) // Ž������ �ȿ� �÷��̾ �ְ�, �ſ� �����ٸ�....
        {
            Attack();
            if (anim->mCurrentFileName == "0208_tiger_attack.fbx" && mElapseTime == 0)
            {
                transform->SetRotation({ 0.0f, yaw, 0.0f });
            }
        }
        else // Ž������ �ȿ� �÷��̾ ������, �ſ� ������ �ʴٸ�...
        {
            Run();
            if (anim->mCurrentFileName == "0722_tiger_run.fbx")
            {
                transform->SetPosition(pos + dir * mRunSpeed * gTimer.DeltaTime());
                transform->SetRotation({ 0.0f, yaw, 0.0f });
            }
        }
    }
    else // �÷��̾ Ž�� ���� �ۿ� �ִ�.
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
    // 네트워크 호랑이는 서버에서 관리되므로 Search 로직을 사용하지 않음
    if (m_isNetworkTiger) {
        return;
    }
    
    static float randYaw = uid(dre);
    Transform* transform = GetComponent<Transform>();
    
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

    ChangeState("0113_tiger_walk.fbx");
}

void TigerObject::Run()
{
    // 네트워크 호랑이는 서버에서 애니메이션을 관리하므로 클라이언트에서 변경하지 않음
    if (m_isNetworkTiger) return;
    
    Animation* anim = GetComponent<Animation>();
    if (anim->mCurrentFileName == "0208_tiger_attack.fbx") return;
    if (anim->mCurrentFileName == "0208_tiger_hit.fbx") return;
    if (anim->mCurrentFileName == "0208_tiger_dying.fbx") return;
    if (mAttackTime < 2.0f) return;
    ChangeState("0722_tiger_run.fbx");
}

void TigerObject::Attack()
{
    // 네트워크 호랑이는 서버에서 애니메이션을 관리하므로 클라이언트에서 변경하지 않음
    if (m_isNetworkTiger) return;
    
    Animation* anim = GetComponent<Animation>();
    if (anim->mCurrentFileName == "0208_tiger_hit.fbx") return;
    if (anim->mCurrentFileName == "0208_tiger_dying.fbx") return;
    if (mAttackTime < 2.0f) return;  // Original과 동일한 2초
    ChangeState("0208_tiger_attack.fbx");
}
void TigerObject::TimeOut()
{
    Animation* anim = GetComponent<Animation>();
    
    if (anim->mCurrentFileName == "0208_tiger_attack.fbx") 
    {
        // 로컬 호랑이만 mIsFired 플래그를 리셋 (네트워크 호랑이는 Fire()에서 처리)
        if (!m_isNetworkTiger) {
            mIsFired = false;
        }
        mAttackTime = 0.0f;
        ChangeState("0722_tiger_idle2.fbx");
    }

    if (anim->mCurrentFileName == "0208_tiger_hit.fbx")
    {
        mIsHitted = false;
        ChangeState("0722_tiger_idle2.fbx");
    }

    if (anim->mCurrentFileName == "0208_tiger_dying.fbx")
    {
        // 로컬 호랑이만 가죽 생성 및 삭제
        if (!m_isNetworkTiger) {
            CreateLeather();
            Delete();
        }
        // 네트워크 호랑이는 CalcTime에서 애니메이션 끝나면 처리
    }
}
void TigerObject::Fire()
{
    // 네트워크 호랑이는 서버에서 공격 패킷을 받을 때마다 공격 오브젝트를 생성해야 함
    if (!m_isNetworkTiger && mIsFired) return;
    
    if (!m_isNetworkTiger) {
        mIsFired = true;
    }

    // 호랑이의 실제 위치를 고려한 공격 오브젝트 생성
    Transform* tigerTransform = GetComponent<Transform>();
    if (tigerTransform) {
        // 호랑이의 회전 행렬을 사용하여 공격 오브젝트의 위치 계산
        XMVECTOR tigerPos = tigerTransform->GetPosition();
        XMVECTOR attackOffset = XMVector3TransformNormal(XMVECTOR{0.0f, 6.0f, 18.0f}, tigerTransform->GetRotationM());
        XMVECTOR attackPos = tigerPos + attackOffset;
        
        Object* obj = new TigerAttackObject(m_scene, m_scene->AllocateId(), m_id);
        obj->AddComponent(new Transform{ attackPos });
        obj->AddComponent(new Collider{ {0.0f, 0.0f, 0.0f}, {4.0f, 6.0f, 8.0f} });
        m_scene->AddObj(obj);
    } else {
        // Transform이 없는 경우 기본 위치 사용
        Object* obj = new TigerAttackObject(m_scene, m_scene->AllocateId(), m_id);
        obj->AddComponent(new Transform{ {0.0f, 6.0f, 18.0f} });
        obj->AddComponent(new Collider{ {0.0f, 0.0f, 0.0f}, {4.0f, 6.0f, 8.0f} });
        m_scene->AddObj(obj);
    }
    
    // 네트워크 호랑이의 경우 Fire() 호출 후 mIsFired를 즉시 리셋하여 다음 공격을 받을 수 있도록 함
    //if (m_isNetworkTiger) {
    //    mIsFired = false;
   // }
}

void TigerObject::Hit()
{
    if (mIsHitted) return;
    mIsHitted = true;
    
    // 네트워크 호랑이는 서버에만 히트 이벤트 전송 (생명력 감소는 서버에서 처리)
    if (m_isNetworkTiger) {
        Framework* framework = m_scene->GetFramework();
        if (framework && framework->IsNetworkEnabled()) {
            NetworkManager& networkManager = framework->GetNetworkManager();
            if (networkManager.IsLoggedIn()) {
                networkManager.SendTigerHit(m_networkTigerID, mLife); // 현재 생명력 전송
                OutputDebugString(L"[Tiger] Network tiger hit event sent to server\n");
            }
        }
        // 네트워크 호랑이는 서버 응답을 기다리므로 여기서 애니메이션도 재생하지 않음
        // 애니메이션은 NetworkManager에서 서버 응답을 받은 후 재생
        mElapseTime = 0.0f;
        return;
    }
    
    // 로컬 호랑이만 즉시 생명력 감소 및 애니메이션 재생
    --mLife;
    
    if (mLife == 0)
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
            // 네트워크 호랑이는 서버에서 공격 패킷을 받을 때만 공격 오브젝트 생성
            // TimeOut() 호출하지 않음 (서버에서 애니메이션 관리)
        }
        else
        {
            mAttackTime += deltaTime;
        }

        if (anim->mCurrentFileName == "0208_tiger_hit.fbx")
        {
            mElapseTime += deltaTime;
            // 네트워크 호랑이도 hit 애니메이션이 끝나면 TimeOut() 호출
            if (mElapseTime > 0.8f) TimeOut();
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
        if (mElapseTime > 0.8f) TimeOut();
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

void TigerAttackObject::OnUpdate(GameTimer& gTimer)
{
    mElapseTime += gTimer.DeltaTime();
    if (mElapseTime >= 0.2) Delete();  // 수명을 0.05초에서 0.2초로 연장
    Object::OnUpdate(gTimer);
}

void TigerAttackObject::OnProcessCollision(Object& other, XMVECTOR collisionNormal, float penetration)
{
    PlayerObject* player = dynamic_cast<PlayerObject*>(&other);
    if (player)
    {
        player->Hit();
        Delete();  // 공격 오브젝트 삭제
        return;
    }

    // 다른 오브젝트와의 충돌 처리
    Transform* transform = GetComponent<Transform>();
    XMVECTOR pos = transform->GetPosition();
    pos += -collisionNormal * penetration;
    transform->SetPosition(pos);
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
        m_parent_id = player->GetId();
        Transform* transform = GetComponent<Transform>();
        transform->SetPosition({ 0.0f, 6.0f, -2.0f });
        transform->SetRotation({ 0.0f, 90.0f, 0.0f });
        Object* obj = new QuadObject(m_scene, m_scene->AllocateId());
        obj->AddComponent(new Transform{ {-0.5f * 1.77f, -0.5f, 1.0f}, {-90.0f, 0.0f, 0.0f}, {1.77f, 1.0f, 1.0f} });
        obj->AddComponent(new Mesh{ "Quad" });
        obj->AddComponent(new Texture{ L"End", 1.0f, 0.4f });
        m_scene->AddObj(obj);
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

void RicecakeMockup::OnUpdate(GameTimer& gTimer)
{
    PlayerObject* player = m_scene->GetObj<PlayerObject>();
    int ricecakeCount = player->GetRicecakeCount();
    if (ricecakeCount <= 0) Delete();
    Object::OnUpdate(gTimer);
}

void GoToBaseObject::OnUpdate(GameTimer& gTimer)
{
    Texture* texture = GetComponent<Texture>();
    if (texture && m_scene->HasEnoughLeather())
    {
        mElapseTime += gTimer.DeltaTime();
        texture->mAmbiantValue = ((sinf(mElapseTime) + 1.0f) * 2.0f) + 0.4;
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

void TitleObject::OnUpdate(GameTimer& gTimer)
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
        QuadObject* obj = new QuadObject(m_scene, m_scene->AllocateId(), m_id);
        obj->AddComponent(new Transform{ {-5.0f, 10.0f, 0.0f}, {-90.0f, 180.0f, 0.0f}, {30.0f, 0.0f, 25.0f} });
        obj->AddComponent(new Mesh{ "Quad" });
        obj->AddComponent(new Texture{ L"Quest", 1.0f, 0.4f });
        m_scene->AddObj(obj);
    }
    Object::OnProcessCollision(other, collisionNormal, penetration);
}

void QuadObject::OnUpdate(GameTimer& gTimer)
{
    CameraObject* camera = m_scene->GetObj<CameraObject>();
    m_parent_id = camera->GetId();
}