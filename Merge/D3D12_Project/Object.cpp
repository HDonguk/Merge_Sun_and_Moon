#include "Object.h"
#include "GameTimer.h"
#include "Scene.h"
#include "DXSampleHelper.h"
#include <random>
#include "Framework.h"

std::random_device rd;  // 첫 번째 rd 객체
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
        else {
            Delete();
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
    if (gravity && similarity > 0.95f) {
        gravity->ResetElapseTime();

        Transform* transform = GetComponent<Transform>();
        XMVECTOR pos = transform->GetPosition();
        pos += -collisionNormal * penetration;
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
        memcpy(m_mappedData + sizeof(XMMATRIX), finalTransforms.data(), sizeof(XMMATRIX) * 90); // 처음 매개변수는 시작주소
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
    ProcessInput(gTimer);
    Object::OnUpdate(gTimer);
}

void PlayerObject::OnProcessCollision(Object& other, XMVECTOR collisionNormal, float penetration)
{
    Transform* transform = GetComponent<Transform>();
    TigerAttackObject* ta = dynamic_cast<TigerAttackObject*>(&other);
    if (ta) // 호랑이 공격에 맞으면...
    {
        Hit();
    }
    else
    {
        XMVECTOR pos = transform->GetPosition();
        pos -= collisionNormal * penetration;
        transform->SetPosition(pos);
    }

    float similarity = XMVectorGetX(XMVector3Dot(XMVECTOR{ 0.0f, 1.0f, 0.0f, 0.0f }, -collisionNormal));
    Gravity* gravity = GetComponent<Gravity>();
    if (gravity && similarity > 0.95f) {
        gravity->ResetElapseTime();
    }
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

    if ((keyState[VK_LBUTTON] & 0x88) == 0x80) { Attack(); }


    Gravity* gravity = GetComponent<Gravity>();
    if ((keyState[VK_SPACE] & 0x88) == 0x80 && gravity) {
        gravity->SetVerticalSpeed(20.0f);
    }
}

void PlayerObject::Move(XMVECTOR dir, float speed,float deltaTime)
{
    Animation* anim = GetComponent<Animation>();
    if (anim->mCurrentFileName == "boy_attack(45).fbx") return;
    if (anim->mCurrentFileName == "boy_hit.fbx") return;
    if (anim->mCurrentFileName == "boy_dying_fix.fbx") return;

    if (speed >= mRunSpeed) {
        anim->ResetAnim("boy_run_fix.fbx", 0.0f);
    }
    else if (speed >= mWalkSpeed) {
        anim->ResetAnim("boy_walk_fix.fbx", 0.0f);
    }

    Transform* transform = GetComponent<Transform>();
    CameraObject* cameraObj = m_scene->GetObj<CameraObject>();
    Transform* cameraTransform = cameraObj->GetComponent<Transform>();
    dir = XMVector3TransformNormal(dir, cameraTransform->GetRotationM());

    dir = XMVector3Normalize(XMVectorSetY(dir, 0.0f));

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
    if (anim->mCurrentFileName == "boy_hit.fbx") return;
    if (anim->mCurrentFileName == "boy_dying_fix.fbx") return;

    anim->ResetAnim("1P(boy-idle).fbx", 0.0f);
}

void PlayerObject::Attack()
{
    Animation* anim = GetComponent<Animation>();
    if (anim->mCurrentFileName == "boy_hit.fbx") return;
    if (anim->mCurrentFileName == "boy_dying_fix.fbx") return;

    anim->ResetAnim("boy_attack(45).fbx", 0.0f);
}

void PlayerObject::TimeOut()
{
    mElapseTime = 0.0f;
    Animation* anim = GetComponent<Animation>();
    if (anim->mCurrentFileName == "boy_attack(45).fbx") 
    {
        anim->ResetAnim("1P(boy-idle).fbx", 0.0f);
        mIsFired = false;
    }

    if (anim->mCurrentFileName == "boy_hit.fbx")
    {
        anim->ResetAnim("1P(boy-idle).fbx", 0.0f);
        mIsHitted = false;
    }

    if (anim->mCurrentFileName == "boy_dying_fix.fbx")
    {
        anim->ResetAnim("1P(boy-idle).fbx", 0.0f);
        mIsHitted = false;
        mLife = 3;
    }
}

void PlayerObject::Fire()
{
    if (mIsFired) return;

    // 투사체 생성
    mIsFired = true;
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
    Animation* anim = GetComponent<Animation>();
    anim->ResetAnim("boy_hit.fbx", 0.0f);
}

void PlayerObject::Dead()
{
    Animation* anim = GetComponent<Animation>();
    anim->ResetAnim("boy_dying_fix.fbx", 0.0f);    
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
    myTransform->SetPosition(targetPos + XMVECTOR{ x, y, z, 0.f });
    
    XMVECTOR myPos = myTransform->GetPosition();
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
    XMVECTOR pos = transform->GetPosition();
    char outstatus = m_scene->ClampToBounds(pos, {0.0f, 1.0f, 0.0f});
    transform->SetPosition(pos);

    XMMATRIX transformM = transform->GetTransformM();
    XMMATRIX invtransformM = XMMatrixInverse(nullptr, transformM);
    memcpy(m_scene->GetConstantBufferMappedData(), &XMMatrixTranspose(invtransformM), sizeof(XMMATRIX)); // 처음 매개변수는 시작주소
}

void CameraObject::OnMouseInput(WPARAM wParam, HWND hWnd)
{
    // 현재 wnd의 센터 좌표를 알아온다
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

    // 각도 clamp
    float min = 0.1f;
    float max = XM_PI - 0.1f;
    mPhi = mPhi < min ? min : (mPhi > max ? max : mPhi);

    SetCursorPos(centerX, centerY);
}

void TigerObject::OnUpdate(GameTimer& gTimer)
{
    // RandomVelocity(gTimer);
    CalcTime(gTimer.DeltaTime());
    TigerBehavior(gTimer);
    Object::OnUpdate(gTimer);
}

void TigerObject::OnProcessCollision(Object& other, XMVECTOR collisionNormal, float penetration)
{
    TigerAttackObject* ta = dynamic_cast<TigerAttackObject*>(&other);
    if (ta) return;

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

    if (result < 200.f) // 플레이어가 탐색 범위 안에 들어오면... 
    {
        if (result < 17.0f) // 탐색범위 안에 플레이어가 있고, 매우 가깝다면....
        {
            if (anim->mCurrentFileName != "0208_tiger_attack.fbx") 
            {
                // 공격 모션 중에는 회전하지 않는다.
                transform->SetRotation({ 0.0f, yaw, 0.0f });
            }
            Attack();
        }
        else // 탐색범위 안에 플레이어가 있지만, 매우 가깝지 않다면...
        {
            if (anim->mCurrentFileName != "0208_tiger_attack.fbx") 
            {
                // 공격 모션 중에는 움직이면 안된다.
                transform->SetPosition(pos + dir * mSpeed * gTimer.DeltaTime());
                transform->SetRotation({ 0.0f, yaw, 0.0f });
            }
        }
    }
    else // 플레이어가 탐색 범위 밖에 있다.
    {
        Search(gTimer.DeltaTime());
    }
}

void TigerObject::Search(float deltaTime)
{
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
    transform->SetPosition(pos + dir * mSpeed * deltaTime);
}

void TigerObject::Attack()
{
    Animation* anim = GetComponent<Animation>();
    if (mAttackTime < 2.0f) return;
    anim->ResetAnim("0208_tiger_attack.fbx", 0.0f);
}
void TigerObject::TimeOut()
{
    mElapseTime = 0.0f;
    Animation* anim = GetComponent<Animation>();
    
    if (anim->mCurrentFileName == "0208_tiger_attack.fbx") 
    {
        anim->ResetAnim("0113_tiger_walk.fbx", 0.0f);
        mIsFired = false;
        mAttackTime = 0.0f;
    }
}
void TigerObject::Fire()
{
    if (mIsFired) return;
    mIsFired = true;

    Object* obj = new TigerAttackObject(m_scene, m_scene->AllocateId(), m_id);
    obj->AddComponent(new Transform{ {0.0f, 6.0f, 16.0f} });
    obj->AddComponent(new Collider{ {0.0f, 0.0f, 0.0f}, {2.0f, 6.0f, 6.0f} });
    m_scene->AddObj(obj);
}

void TigerObject::CalcTime(float deltaTime) 
{
    Animation* anim = GetComponent<Animation>();
    if (anim->mCurrentFileName == "0208_tiger_attack.fbx") 
    {
        mElapseTime += deltaTime;
        if (mElapseTime >= 0.4f) Fire();
        if (mElapseTime >= 0.8f) TimeOut();
    }
    else
    {
        mAttackTime += deltaTime;
        mSearchTime += deltaTime;
    }
}

void TigerAttackObject::OnUpdate(GameTimer& gTimer)
{
    mElapseTime += gTimer.DeltaTime();
    if (mElapseTime >= 0.05) Delete();
    Object::OnUpdate(gTimer);
}

void QuadObject::OnUpdate(GameTimer& gTimer)
{
    CameraObject* camera = m_scene->GetObj<CameraObject>();
    m_parent_id = camera->GetId();
    Object::OnUpdate(gTimer);
}
