#include "stdafx.h"
#include "Scene.h"
#include "Object.h"
#include "Component.h"
#include "NetworkManager.h"
#include "OtherPlayerManager.h"
#include "Framework.h"
#include "DXSampleHelper.h"
#include "GameTimer.h"
#include "string"
#include "info.h"
#include <array>

Scene::~Scene()
{
    OnDestroy();
    DeleteCurrentObjects();
}

Scene::Scene(Framework* parent, UINT width, UINT height) :
    m_parent{ parent },
    m_viewport(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)),
    m_scissorRect(0, 0, static_cast<LONG>(width), static_cast<LONG>(height))
{
}

void Scene::OnInit(ID3D12Device* device, ID3D12GraphicsCommandList* commandList)
{
    OutputDebugString(L"[Scene] OnInit() started\n");
    LoadMeshAnimationTexture();
    BuildProjMatrix();
    OutputDebugString(L"[Scene] About to call BuildBaseStage()\n");
    BuildBaseStage();

    OutputDebugString(L"[Scene] BuildBaseStage() completed\n");
    BuildRootSignature(device);
    BuildInputElement();
    BuildShaders();
    BuildPSO(device);
    BuildVertexBuffer(device, commandList);
    BuildIndexBuffer(device, commandList);
    BuildTextureBuffer(device, commandList);
    BuildConstantBuffer(device);
    BuildDescriptorHeap(device);
    BuildVertexBufferView();
    BuildIndexBufferView();
    BuildConstantBufferView(device);
    BuildTextureBufferView(device);
    BuildShadow();

    ProcessStageQueue();
    ProcessObjectQueue();

    OutputDebugString(L"[Scene] OnInit() completed\n");
}

void Scene::BuildHuntingStage()
{
    OutputDebugString(L"[Scene] BuildHuntingStage() called - Building Hunting Stage\n");
    m_current_stage = L"Hunting";
    OutputDebugString(L"[Scene] Current stage set to: Hunting\n");
    
    OutputDebugString(L"[Scene] Starting to create objects for Hunting Stage...\n");

    Object* objectPtr = nullptr;
    {
        mMainCameraId = AllocateId();
        objectPtr = new CameraObject(this, mMainCameraId);
        objectPtr->AddComponent(new Transform{ {0.f, 0.0f, 0.f} });
        AddObj(objectPtr);
    }

    {
        float scale = 0.1f;
        objectPtr = new PlayerObject(this, AllocateId());
        // 플레이어 생명력을 명시적으로 3으로 설정
        dynamic_cast<PlayerObject*>(objectPtr)->SetLife(3);
        objectPtr->AddComponent(new Transform{ {200.f, 0.0f, 300.f} });
        objectPtr->AddComponent(new AdjustTransform{ {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {scale, scale, scale} });
        objectPtr->AddComponent(new Mesh{ "1P(boy-idle).fbx" });
        objectPtr->AddComponent(new Texture{ L"boy" , 1.0f, 0.4f });
        objectPtr->AddComponent(new Animation{ "1P(boy-idle).fbx" });
        objectPtr->AddComponent(new Gravity);
        objectPtr->AddComponent(new Collider{ {0.0f, 8.0f, 0.0f}, {2.0f, 8.0f, 2.0f} });
        AddObj(objectPtr);
        
        OutputDebugString(L"[Scene] Created local player with life: 3\n");
    }

   

    {
        objectPtr = new TerrainObject(this, AllocateId());
        objectPtr->AddComponent(new Transform{ {0.f, 0.0f, 0.f} });
        objectPtr->AddComponent(new Mesh{ "HeightMap.raw" });
        objectPtr->AddComponent(new Texture{ L"grass" , 5.0f, 0.4f });
        AddObj(objectPtr);
    }

    // ������ ����
    {
        float scale = 0.4f;
        objectPtr = new GoToBaseObject(this, AllocateId());
        objectPtr->AddComponent(new Transform{ {550.0f, 0.0f, 550.0f} });
        objectPtr->AddComponent(new AdjustTransform{ {0.0f * scale, 13.0f * scale, 0.0f * scale}, {0.0f, 0.0f, 0.0f}, {scale, scale, scale} });
        objectPtr->AddComponent(new Mesh{ "well.fbx" });
        objectPtr->AddComponent(new Texture{ L"broken_house", 1.0f, 0.4f });
        objectPtr->AddComponent(new Collider{ {0.0f, 37.5f * scale, 0.0f}, {12.5f * scale, 37.5f * scale, 12.5f * scale} });
        objectPtr->AddComponent(new Gravity);
        AddObj(objectPtr);

        objectPtr = new GoToBaseObject(this, AllocateId());
        objectPtr->AddComponent(new Transform{ {950.0f, 0.0f, 550.0f} });
        objectPtr->AddComponent(new AdjustTransform{ {0.0f * scale, 13.0f * scale, 0.0f * scale}, {0.0f, 0.0f, 0.0f}, {scale, scale, scale} });
        objectPtr->AddComponent(new Mesh{ "well.fbx" });
        objectPtr->AddComponent(new Texture{ L"broken_house", 1.0f, 0.4f });
        objectPtr->AddComponent(new Collider{ {0.0f, 37.5f * scale, 0.0f}, {12.5f * scale, 37.5f * scale, 12.5f * scale} });
        objectPtr->AddComponent(new Gravity);
        AddObj(objectPtr);

        objectPtr = new GoToBaseObject(this, AllocateId());
        objectPtr->AddComponent(new Transform{ {550.0f, 0.0f, 950.0f} });
        objectPtr->AddComponent(new AdjustTransform{ {0.0f * scale, 13.0f * scale, 0.0f * scale}, {0.0f, 0.0f, 0.0f}, {scale, scale, scale} });
        objectPtr->AddComponent(new Mesh{ "well.fbx" });
        objectPtr->AddComponent(new Texture{ L"broken_house", 1.0f, 0.4f });
        objectPtr->AddComponent(new Collider{ {0.0f, 37.5f * scale, 0.0f}, {12.5f * scale, 37.5f * scale, 12.5f * scale} });
        objectPtr->AddComponent(new Gravity);
        AddObj(objectPtr);

        objectPtr = new GoToBaseObject(this, AllocateId());
        objectPtr->AddComponent(new Transform{ {950.0f, 0.0f, 950.0f} });
        objectPtr->AddComponent(new AdjustTransform{ {0.0f * scale, 13.0f * scale, 0.0f * scale}, {0.0f, 0.0f, 0.0f}, {scale, scale, scale} });
        objectPtr->AddComponent(new Mesh{ "well.fbx" });
        objectPtr->AddComponent(new Texture{ L"broken_house", 1.0f, 0.4f });
        objectPtr->AddComponent(new Collider{ {0.0f, 37.5f * scale, 0.0f}, {12.5f * scale, 37.5f * scale, 12.5f * scale} });
        objectPtr->AddComponent(new Gravity);
        AddObj(objectPtr);
    }

    // ����
    {
        float scale = 30.0f;
        float basePosX = 100.0f;
        float basePosZ = 100.0f;
        float offset = 200.0f;
        int repeat = 6;
        for (int i = 0; i < repeat; ++i) {
            for (int j = 0; j < repeat; ++j) {
                objectPtr = new TreeObject(this, AllocateId());
                objectPtr->AddComponent(new Transform{ {basePosX + offset * j, -100.f, basePosZ + offset * i} });
                objectPtr->AddComponent(new AdjustTransform{ {-0.8f * scale, 0.3f * scale, -2.5f * scale}, {0.0f, 0.0f, 0.0f}, {scale, scale, scale} });
                objectPtr->AddComponent(new Mesh{ "long_tree.fbx" });
                objectPtr->AddComponent(new Texture{ L"longTree", 1.0f, 0.4f });
                objectPtr->AddComponent(new Collider{ {0.0f, 20.0f, 0.0f}, {4.0f, 20.0f, 4.0f} });
                AddObj(objectPtr);
            }
        }
    }
    // ����
    {
        float scale = 30.0f;
        float basePosX = 200.0f;
        float basePosZ = 200.0f;
        float offset = 200.0f;
        int repeat = 6;
        for (int i = 0; i < repeat; ++i) {
            for (int j = 0; j < repeat; ++j) {
                objectPtr = new TreeObject(this, AllocateId());
                objectPtr->AddComponent(new Transform{ {basePosX + offset * j, -100.f, basePosZ + offset * i} });
                objectPtr->AddComponent(new AdjustTransform{ {-1.0f * scale, 0.0f * scale, 0.0f * scale}, {0.0f, 0.0f, 0.0f}, {scale, scale, scale} });
                objectPtr->AddComponent(new Mesh{ "normal_tree.fbx" });
                objectPtr->AddComponent(new Texture{ L"normalTree", 1.0f, 0.4f });
                objectPtr->AddComponent(new Collider{ {0.0f, 1.0f * scale, 0.0f}, {0.15f * scale, 1.0f * scale, 0.15f * scale} });
                AddObj(objectPtr);
            }
        }
    }

    //��躮
    {
        float scale = 44.0f;
        float scaleY = scale * 1.5f;
        int repeat = 7;
        for (int i = 0; i < repeat; ++i)
        {
            objectPtr = new TestObject(this, AllocateId());
            objectPtr->AddComponent(new Transform{ {100.0f + 200.0f * i, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f} });
            objectPtr->AddComponent(new AdjustTransform{ {0.0f, 0.0f, 0.0f }, {0.0f, 0.0f, 0.0f}, {scale, scaleY, scale} });
            objectPtr->AddComponent(new Collider{ {0.0f, 0.0f, 0.0f}, {2.3f * scale, 1.5f * scaleY, 1.3f * scale} });
            objectPtr->AddComponent(new Mesh{ "cloud1.fbx" });
            objectPtr->AddComponent(new Texture{ L"LightGray", 1.0f, 0.4f });
            objectPtr->AddComponent(new Gravity);
            AddObj(objectPtr);
        }

        for (int i = 0; i < repeat; ++i)
        {
            objectPtr = new TestObject(this, AllocateId());
            objectPtr->AddComponent(new Transform{ {100.0f + 200.0f * i, 0.0f, 2560.0f}, {0.0f, 0.0f, 0.0f} });
            objectPtr->AddComponent(new AdjustTransform{ {0.0f, 0.0f, 0.0f }, {0.0f, 0.0f, 0.0f}, {scale, scaleY, scale} });
            objectPtr->AddComponent(new Collider{ {0.0f, 0.0f, 0.0f}, {2.3f * scale, 1.5f * scaleY, 1.3f * scale} });
            objectPtr->AddComponent(new Mesh{ "cloud1.fbx" });
            objectPtr->AddComponent(new Texture{ L"LightGray", 1.0f, 0.4f });
            objectPtr->AddComponent(new Gravity);
            AddObj(objectPtr);
        }

        for (int i = 0; i < repeat; ++i)
        {
            objectPtr = new TestObject(this, AllocateId());
            objectPtr->AddComponent(new Transform{ {0.0f, 0.0f, 100.0f + 200.0f * i}, {0.0f, 90.0f, 0.0f} });
            objectPtr->AddComponent(new AdjustTransform{ {0.0f, 0.0f, 0.0f }, {0.0f, 0.0f, 0.0f}, {scale, scaleY, scale} });
            objectPtr->AddComponent(new Collider{ {0.0f, 0.0f, 0.0f}, {2.3f * scale, 1.5f * scaleY, 1.3f * scale} });
            objectPtr->AddComponent(new Mesh{ "cloud1.fbx" });
            objectPtr->AddComponent(new Texture{ L"LightGray", 1.0f, 0.4f });
            objectPtr->AddComponent(new Gravity);
            AddObj(objectPtr);
        }

        for (int i = 0; i < repeat; ++i)
        {
            objectPtr = new TestObject(this, AllocateId());
            objectPtr->AddComponent(new Transform{ {2560.0f, 0.0f, 100.0f + 200.0f * i}, {0.0f, 90.0f, 0.0f} });
            objectPtr->AddComponent(new AdjustTransform{ {0.0f, 0.0f, 0.0f }, {0.0f, 0.0f, 0.0f}, {scale, scaleY, scale} });
            objectPtr->AddComponent(new Collider{ {0.0f, 0.0f, 0.0f}, {2.3f * scale, 1.5f * scaleY, 1.3f * scale} });
            objectPtr->AddComponent(new Mesh{ "cloud1.fbx" });
            objectPtr->AddComponent(new Texture{ L"LightGray", 1.0f, 0.4f });
            objectPtr->AddComponent(new Gravity);
            AddObj(objectPtr);
        }
    }
    // 호랑이들은 서버에서 관리되므로 로컬에서는 생성하지 않음
    // 서버에서 PACKET_TIGER_SPAWN 패킷을 통해 호랑이들이 생성됨
    
    // 다른 플레이어들은 네트워크를 통해 자동으로 생성되므로 여기서는 생성하지 않음
    
    // 스테이지 전환 완료 - 다른 플레이어 생성 허용
    if (m_parent) {
        OtherPlayerManager::GetInstance()->SetStageTransitioning(false);
        OutputDebugString(L"[Scene] BuildHuntingStage: Stage transition completed\n");
    }
}

void Scene::BuildBaseStage()
{
    OutputDebugString(L"[Scene] BuildBaseStage() called - Building Base Stage\n");
    m_current_stage = L"Base";
    OutputDebugString(L"[Scene] Current stage set to: Base\n");
    
    Object* objectPtr = nullptr;
    {
        mMainCameraId = AllocateId();
        objectPtr = new CameraObject(this, mMainCameraId);
        objectPtr->AddComponent(new Transform{ {0.0f, 0.0f, 0.0f} });
        AddObj(objectPtr);
    }

    {
        float scale = 0.1f;
        objectPtr = new PlayerObject(this, AllocateId());
        // 플레이어 생명력을 명시적으로 3으로 설정
        dynamic_cast<PlayerObject*>(objectPtr)->SetLife(3);
        objectPtr->AddComponent(new Transform{ {430.f, 0.0f, 150.f} });
        objectPtr->AddComponent(new AdjustTransform{ {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {scale, scale, scale} });
        objectPtr->AddComponent(new Mesh{ "1P(boy-idle).fbx" });
        objectPtr->AddComponent(new Texture{ L"boy" , 1.0f, 0.4f });
        objectPtr->AddComponent(new Animation{ "1P(boy-idle).fbx" });
        objectPtr->AddComponent(new Gravity);
        objectPtr->AddComponent(new Collider{ {0.0f, 80.0f * scale, 0.0f}, {30.0f * scale, 80.0f * scale, 30.0f * scale} });
        AddObj(objectPtr);
        
        OutputDebugString(L"[Scene] Created local player in Base stage with life: 3\n");
    }
    // 
  

    {
        float scale = 0.1f;
        objectPtr = new SisterObject(this, AllocateId());
        objectPtr->AddComponent(new Transform{ {300.f, 20.0f, 350.f}, {0.0f, 90.0f, 0.0f} });
        objectPtr->AddComponent(new AdjustTransform{ {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {scale, scale, scale} });
        objectPtr->AddComponent(new Mesh{ "sister_idle_fix.fbx" });
        objectPtr->AddComponent(new Texture{ L"sister" , 1.0f, 0.4f });
        objectPtr->AddComponent(new Animation{ "sister_idle_fix.fbx" });
        objectPtr->AddComponent(new Gravity);
        objectPtr->AddComponent(new Collider{ {0.0f, 80.0f * scale, 0.0f}, {30.0f * scale, 80.0f * scale, 30.0f * scale} });
        AddObj(objectPtr);
    }

    // 
    {
        float scale = 0.1f;
        objectPtr = new GodObject(this, AllocateId());
        objectPtr->AddComponent(new Transform{ {500.f, 20.0f, 600.0f}, {0.0f, 180.0f, 0.0f} });
        objectPtr->AddComponent(new AdjustTransform{ {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {scale, scale, scale} });
        objectPtr->AddComponent(new Mesh{ "god_idle.fbx" });
        objectPtr->AddComponent(new Texture{ L"god" , 1.0f, 0.4f });
        objectPtr->AddComponent(new Animation{ "god_idle.fbx" });
        objectPtr->AddComponent(new Gravity);
        objectPtr->AddComponent(new Collider{ {0.0f, 80.0f * scale, 0.0f}, {30.0f * scale, 80.0f * scale, 30.0f * scale} });
        AddObj(objectPtr);
    }
    // 
    {
        objectPtr = new TestObject(this, AllocateId());
        objectPtr->AddComponent(new Transform{ {0.0f, 0.0f, 0.0f} });
        objectPtr->AddComponent(new Mesh{ "Plane" });
        objectPtr->AddComponent(new Texture{ L"grass", 1.0f, 0.4f });
        AddObj(objectPtr);
    }


    // 
    {
        float scale = 4.0f;
        objectPtr = new TestObject(this, AllocateId());

        objectPtr->AddComponent(new Transform{ {200.0f, 0.0f, 350.0f} });
        objectPtr->AddComponent(new AdjustTransform{ {85.0f * scale, 8.5f * scale, -291.5f * scale}, {0.0f, 0.0f, 0.0f}, {scale, scale, scale} });
        objectPtr->AddComponent(new Mesh{ "background_house.fbx" });
        objectPtr->AddComponent(new Texture{ L"broken_house", 1.0f, 0.4f });
        objectPtr->AddComponent(new Collider{ {0.0f, 6.0f * scale, 0.0f}, {7.0f * scale, 6.0f * scale, 19.0f * scale} });
        objectPtr->AddComponent(new Gravity);
        AddObj(objectPtr);

        objectPtr = new TestObject(this, AllocateId());

        objectPtr->AddComponent(new Transform{ {500.0f, 0.0f, 700.0f}, {0.0f, 90.0f, 0.0f} });
        objectPtr->AddComponent(new AdjustTransform{ {85.0f * scale, 8.5f * scale, -291.5f * scale}, {0.0f, 0.0f, 0.0f}, {scale, scale, scale} });
        objectPtr->AddComponent(new Mesh{ "background_house.fbx" });
        objectPtr->AddComponent(new Texture{ L"broken_house", 1.0f, 0.4f });
        objectPtr->AddComponent(new Collider{ {0.0f, 6.0f * scale, 0.0f}, {7.0f * scale, 6.0f * scale, 19.0f * scale} });
      
        objectPtr->AddComponent(new Gravity);
        AddObj(objectPtr);
    }

    // 
    {
        float scale = 0.1f;
        objectPtr = new TestObject(this, AllocateId());
        objectPtr->AddComponent(new Transform{ {300.0f, 0.0f, 250.0f} });
        objectPtr->AddComponent(new AdjustTransform{ {0.0f * scale, 0.0f * scale, -100.0f * scale}, {0.0f, 0.0f, 0.0f}, {scale, scale, scale} });
        objectPtr->AddComponent(new Mesh{ "broken_house.fbx" });
        objectPtr->AddComponent(new Texture{ L"broken_house", 1.0f, 0.4f });
        objectPtr->AddComponent(new Collider{ {0.0f, 300.0f * scale, 0.0f}, {400.0f * scale, 300.0f * scale, 200.0f * scale} });
        objectPtr->AddComponent(new Gravity);
        AddObj(objectPtr);
   
        objectPtr = new TestObject(this, AllocateId());

        objectPtr->AddComponent(new Transform{ {400.0f, 0.0f, 600.0f}, {0.0f, 90.0f, 0.0f} });
        objectPtr->AddComponent(new AdjustTransform{ {0.0f * scale, 0.0f * scale, -100.0f * scale}, {0.0f, 0.0f, 0.0f}, {scale, scale, scale} });

        objectPtr->AddComponent(new Mesh{ "broken_house.fbx" });
        objectPtr->AddComponent(new Texture{ L"broken_house", 1.0f, 0.4f });

        objectPtr->AddComponent(new Collider{ {0.0f, 300.0f * scale, 0.0f}, {400.0f * scale, 300.0f * scale, 200.0f * scale} });
        
        objectPtr->AddComponent(new Gravity);
        AddObj(objectPtr);
    }

    {
        float scale = 0.1f;
        objectPtr = new TestObject(this, AllocateId());
        objectPtr->AddComponent(new Transform{ {300.0f, 0.0f, 480.0f} });
        objectPtr->AddComponent(new AdjustTransform{ {0.0f * scale, 0.0f * scale, 0.0f * scale}, {0.0f, 0.0f, 0.0f}, {scale, scale, scale} });
        objectPtr->AddComponent(new Mesh{ "broken_house2.fbx" });
        objectPtr->AddComponent(new Texture{ L"broken_house2", 1.0f, 0.4f });
        objectPtr->AddComponent(new Collider{ {0.0f, 300.0f * scale, 0.0f}, {500.0f * scale, 300.0f * scale, 300.0f * scale} });
        objectPtr->AddComponent(new Gravity);
        AddObj(objectPtr);
 
        objectPtr = new TestObject(this, AllocateId());

        objectPtr->AddComponent(new Transform{ {600.0f, 0.0f, 600.0f}, {0.0f, 90.0f, 0.0f} });
        objectPtr->AddComponent(new AdjustTransform{ {0.0f * scale, 0.0f * scale, 0.0f * scale}, {0.0f, 0.0f, 0.0f}, {scale, scale, scale} });
        objectPtr->AddComponent(new Mesh{ "broken_house2.fbx" });
        objectPtr->AddComponent(new Texture{ L"broken_house2", 1.0f, 0.4f });
        objectPtr->AddComponent(new Collider{ {0.0f, 300.0f * scale, 0.0f}, {500.0f * scale, 300.0f * scale, 300.0f * scale} });
        objectPtr->AddComponent(new Gravity);
        AddObj(objectPtr);
    }
    {
        float scale = 4.0f;
        objectPtr = new TestObject(this, AllocateId());

        objectPtr->AddComponent(new Transform{ {300.0f, 0.0f, 350.0f} });
        objectPtr->AddComponent(new AdjustTransform{ {0.0f * scale, 1.0f * scale, 0.0f * scale}, {0.0f, 0.0f, 0.0f}, {scale, scale, scale} });
        objectPtr->AddComponent(new Mesh{ "table.fbx" });
        objectPtr->AddComponent(new Texture{ L"Brown", 1.0f, 0.4f });
        objectPtr->AddComponent(new Collider{ {0.0f, 1.2f * scale, 0.0f}, {6.5f * scale, 1.2f * scale, 5.0f * scale} });

        objectPtr->AddComponent(new Gravity);
        AddObj(objectPtr);
   
        objectPtr = new TestObject(this, AllocateId());
        objectPtr->AddComponent(new Transform{ {500.0f, 0.0f, 600.0f} });
        objectPtr->AddComponent(new AdjustTransform{ {0.0f * scale, 1.0f * scale, 0.0f * scale}, {0.0f, 0.0f, 0.0f}, {scale, scale, scale} });
        objectPtr->AddComponent(new Mesh{ "table.fbx" });
        objectPtr->AddComponent(new Texture{ L"Brown", 1.0f, 0.4f });
        objectPtr->AddComponent(new Collider{ {0.0f, 1.2f * scale, 0.0f}, {6.5f * scale, 1.2f * scale, 5.0f * scale} });
        objectPtr->AddComponent(new Gravity);
        AddObj(objectPtr);
    }
    //  
    {
        float scale = 0.04f;
        objectPtr = new TestObject(this, AllocateId());

        objectPtr->AddComponent(new Transform{ {430.0f, 0.0f, 300.0f} });
        objectPtr->AddComponent(new AdjustTransform{ {0.0f * scale, 13.0f * scale, 0.0f * scale}, {0.0f, 0.0f, 0.0f}, {scale, scale, scale} });
        objectPtr->AddComponent(new Mesh{ "well.fbx" });
        objectPtr->AddComponent(new Texture{ L"broken_house", 1.0f, 0.4f });
        objectPtr->AddComponent(new Collider{ {0.0f, 37.5f * scale, 0.0f}, {12.5f * scale, 37.5f * scale, 12.5f * scale} });

        objectPtr->AddComponent(new Gravity);
        AddObj(objectPtr);

        objectPtr = new TestObject(this, AllocateId());
        objectPtr->AddComponent(new Transform{ {600.0f, 0.0f, 500.0f} });
        objectPtr->AddComponent(new AdjustTransform{ {0.0f * scale, 13.0f * scale, 0.0f * scale}, {0.0f, 0.0f, 0.0f}, {scale, scale, scale} });
        objectPtr->AddComponent(new Mesh{ "well.fbx" });
        objectPtr->AddComponent(new Texture{ L"broken_house", 1.0f, 0.4f });
        objectPtr->AddComponent(new Collider{ {0.0f, 37.5f * scale, 0.0f}, {12.5f * scale, 37.5f * scale, 12.5f * scale} });
        objectPtr->AddComponent(new Gravity);
        AddObj(objectPtr);
    }
    {
        float scale = 20.0f;
        int repeat = 10;
        float baseX = 150.0f;
        float baseZ = 150.0f;
        float offset = 80.0f;
        for (int i = 0; i < repeat; ++i)
        {
            objectPtr = new TestObject(this, AllocateId());
            objectPtr->AddComponent(new Transform{ {baseX + offset * i, 0.0f, baseZ} });
            objectPtr->AddComponent(new AdjustTransform{ {-0.8f * scale, 0.3f * scale, -2.5f * scale}, {0.0f, 0.0f, 0.0f}, {scale, scale, scale} });
            objectPtr->AddComponent(new Mesh{ "long_tree.fbx" });
            objectPtr->AddComponent(new Texture{ L"longTree", 1.0f, 0.4f });
            objectPtr->AddComponent(new Collider{ {0.0f, 1.0f * scale, 0.0f}, {0.15f * scale, 1.0f * scale, 0.15f * scale} });
            objectPtr->AddComponent(new Gravity);
            AddObj(objectPtr);
        }
        for (int i = 0; i < repeat; ++i)
        {
            objectPtr = new TestObject(this, AllocateId());
            objectPtr->AddComponent(new Transform{ {baseX + offset * i, 0.0f, baseZ + offset * (repeat - 1)} });
            objectPtr->AddComponent(new AdjustTransform{ {-0.8f * scale, 0.3f * scale, -2.5f * scale}, {0.0f, 0.0f, 0.0f}, {scale, scale, scale} });
            objectPtr->AddComponent(new Mesh{ "long_tree.fbx" });
            objectPtr->AddComponent(new Texture{ L"longTree", 1.0f, 0.4f });
            objectPtr->AddComponent(new Collider{ {0.0f, 1.0f * scale, 0.0f}, {0.15f * scale, 1.0f * scale, 0.15f * scale} });
            objectPtr->AddComponent(new Gravity);
            AddObj(objectPtr);
        }
        for (int i = 0; i < repeat; ++i)
        {
            objectPtr = new TestObject(this, AllocateId());
            objectPtr->AddComponent(new Transform{ {baseX , 0.0f, baseZ + offset * i} });
            objectPtr->AddComponent(new AdjustTransform{ {-0.8f * scale, 0.3f * scale, -2.5f * scale}, {0.0f, 0.0f, 0.0f}, {scale, scale, scale} });
            objectPtr->AddComponent(new Mesh{ "long_tree.fbx" });
            objectPtr->AddComponent(new Texture{ L"longTree", 1.0f, 0.4f });
            objectPtr->AddComponent(new Collider{ {0.0f, 1.0f * scale, 0.0f}, {0.15f * scale, 1.0f * scale, 0.15f * scale} });
            objectPtr->AddComponent(new Gravity);
            AddObj(objectPtr);
        }
        for (int i = 0; i < repeat; ++i)
        {
            objectPtr = new TestObject(this, AllocateId());
            objectPtr->AddComponent(new Transform{ {baseX + offset * (repeat - 1) , 0.0f, baseZ + offset * i} });
            objectPtr->AddComponent(new AdjustTransform{ {-0.8f * scale, 0.3f * scale, -2.5f * scale}, {0.0f, 0.0f, 0.0f}, {scale, scale, scale} });
            objectPtr->AddComponent(new Mesh{ "long_tree.fbx" });
            objectPtr->AddComponent(new Texture{ L"longTree", 1.0f, 0.4f });
            objectPtr->AddComponent(new Collider{ {0.0f, 1.0f * scale, 0.0f}, {0.15f * scale, 1.0f * scale, 0.15f * scale} });
            objectPtr->AddComponent(new Gravity);
            AddObj(objectPtr);
        }
    }
    // 

    {
        float scale = 20.0f;
        float baseX = 0.0f;
        float baseZ = 0.0f;
        float offset = 80.0f;
        for (int i = 0; i < 2; ++i)
        {
            for (int j = 0; j < 2; ++j)
            {
                objectPtr = new TestObject(this, AllocateId());
                objectPtr->AddComponent(new Transform{ {200.0f + baseX + offset * j, 0.0f, 600.0f + baseZ + offset * i} });
                objectPtr->AddComponent(new AdjustTransform{ {-1.0f * scale, 0.0f * scale, 0.0f * scale}, {0.0f, 0.0f, 0.0f}, {scale, scale, scale} });
                objectPtr->AddComponent(new Mesh{ "normal_tree.fbx" });
                objectPtr->AddComponent(new Texture{ L"normalTree", 1.0f, 0.4f });
                objectPtr->AddComponent(new Collider{ {0.0f, 1.0f * scale, 0.0f}, {0.15f * scale, 1.0f * scale, 0.15f * scale} });
                objectPtr->AddComponent(new Gravity);
                AddObj(objectPtr);

            }
        }

        for (int i = 0; i < 2; ++i)
        {
            for (int j = 0; j < 2; ++j)
            {
                objectPtr = new TestObject(this, AllocateId());
                objectPtr->AddComponent(new Transform{ {700.0f + baseX + offset * j, 0.0f, 600.0f + baseZ + offset * i} });
                objectPtr->AddComponent(new AdjustTransform{ {-1.0f * scale, 0.0f * scale, 0.0f * scale}, {0.0f, 0.0f, 0.0f}, {scale, scale, scale} });
                objectPtr->AddComponent(new Mesh{ "normal_tree.fbx" });
                objectPtr->AddComponent(new Texture{ L"normalTree", 1.0f, 0.4f });
                objectPtr->AddComponent(new Collider{ {0.0f, 1.0f * scale, 0.0f}, {0.15f * scale, 1.0f * scale, 0.15f * scale} });
                objectPtr->AddComponent(new Gravity);
                AddObj(objectPtr);

            }
        }


    };

    // 
   

    {
        float scale = 0.2f;
        objectPtr = new TigerMockup(this, AllocateId());
        objectPtr->AddComponent(new Transform{ {550.0f, 0.0f, 250.0f} });
        objectPtr->AddComponent(new AdjustTransform{ {0.0f, 0.0f, -40.0f * scale}, {0.0f, 180.0f, 0.0f}, {scale, scale, scale} });
        objectPtr->AddComponent(new Mesh{ "0113_tiger.fbx" });
        objectPtr->AddComponent(new Texture{ L"tigercolor", 1.0f, 0.4f });
        objectPtr->AddComponent(new Animation{ "0113_tiger_walk.fbx" });
        objectPtr->AddComponent(new Gravity);
        objectPtr->AddComponent(new Collider{ {0.0f, 6.0f, 0.0f}, {2.0f, 6.0f, 10.0f} });
        AddObj(objectPtr);
        
        objectPtr = new TigerMockup(this, AllocateId());
        objectPtr->AddComponent(new Transform{ {650.0f, 0.0f, 350.0f} });
        objectPtr->AddComponent(new AdjustTransform{ {0.0f, 0.0f, -40.0f * scale}, {0.0f, 180.0f, 0.0f}, {scale, scale, scale} });
        objectPtr->AddComponent(new Mesh{ "0113_tiger.fbx" });
        objectPtr->AddComponent(new Texture{ L"tigercolor", 1.0f, 0.4f });
        objectPtr->AddComponent(new Animation{ "0113_tiger_walk.fbx" });
        objectPtr->AddComponent(new Gravity);
        objectPtr->AddComponent(new Collider{ {0.0f, 6.0f, 0.0f}, {2.0f, 6.0f, 10.0f} });
        AddObj(objectPtr);
    }

    // 
    {
        float scale = 0.05f;
        int repeat = 15;
        float baseX = 500.0f;
        float baseZ = 200.0f;
        float offset = 150.0f * scale * 2.0f;
        for (int i = 0; i < repeat; ++i)
        {
            objectPtr = new TestObject(this, AllocateId());
            objectPtr->AddComponent(new Transform{ {baseX + offset * i, 0.0f, baseZ} });
            objectPtr->AddComponent(new AdjustTransform{ {0.0f * scale, 0.0f * scale, 0.0f * scale}, {-90.0f, 0.0f, 0.0f}, {scale, scale, scale} });
            objectPtr->AddComponent(new Mesh{ "fence.fbx" });
            objectPtr->AddComponent(new Texture{ L"Brown", 1.0f, 0.4f });
            objectPtr->AddComponent(new Collider{ {150.0f * scale, 100.0f * scale, 0.0f}, {150.0f * scale, 100.0f * scale, 50.0f * scale } });
            objectPtr->AddComponent(new Gravity);
            AddObj(objectPtr);
        }
        for (int i = 0; i < repeat; ++i)
        {
            objectPtr = new TestObject(this, AllocateId());
            objectPtr->AddComponent(new Transform{ {baseX + offset * i, 0.0f, baseZ + offset * repeat} });
            objectPtr->AddComponent(new AdjustTransform{ {0.0f * scale, 0.0f * scale, 0.0f * scale}, {-90.0f, 0.0f, 0.0f}, {scale, scale, scale} });
            objectPtr->AddComponent(new Mesh{ "fence.fbx" });
            objectPtr->AddComponent(new Texture{ L"Brown", 1.0f, 0.4f });
            objectPtr->AddComponent(new Collider{ {150.0f * scale, 100.0f * scale, 0.0f}, {150.0f * scale, 100.0f * scale, 50.0f * scale} });
            objectPtr->AddComponent(new Gravity);
            AddObj(objectPtr);
        }
        for (int i = 0; i < repeat; ++i)
        {
            objectPtr = new TestObject(this, AllocateId());
            objectPtr->AddComponent(new Transform{ {baseX, 0.0f, baseZ + offset * i}, {0.0f, -90.0f, 0.0f} });
            objectPtr->AddComponent(new AdjustTransform{ {0.0f * scale, 0.0f * scale, 0.0f * scale}, {-90.0f, 0.0f, 0.0f}, {scale, scale, scale} });
            objectPtr->AddComponent(new Mesh{ "fence.fbx" });
            objectPtr->AddComponent(new Texture{ L"Brown", 1.0f, 0.4f });
            objectPtr->AddComponent(new Collider{ {150.0f * scale, 100.0f * scale, 0.0f}, {150.0f * scale, 100.0f * scale, 50.0f * scale} });
            objectPtr->AddComponent(new Gravity);
            AddObj(objectPtr);
        }
        for (int i = 0; i < repeat; ++i)
        {
            objectPtr = new TestObject(this, AllocateId());
            objectPtr->AddComponent(new Transform{ {baseX + offset * repeat, 0.0f, baseZ + offset * i}, {0.0f, -90.0f, 0.0f} });
            objectPtr->AddComponent(new AdjustTransform{ {0.0f * scale, 0.0f * scale, 0.0f * scale}, {-90.0f, 0.0f, 0.0f}, {scale, scale, scale} });
            objectPtr->AddComponent(new Mesh{ "fence.fbx" });
            objectPtr->AddComponent(new Texture{ L"Brown", 1.0f, 0.4f });
            objectPtr->AddComponent(new Collider{ {150.0f * scale, 100.0f * scale, 0.0f}, {150.0f * scale, 100.0f * scale, 50.0f * scale} });
            objectPtr->AddComponent(new Gravity);
            AddObj(objectPtr);
        }

    }
    //��躮
    {
        float scale = 44.0f;
        float scaleY = scale * 1.5f;
        int repeat = 5;
        for (int i = 0; i < repeat; ++i)
        {
            objectPtr = new TestObject(this, AllocateId());
            objectPtr->AddComponent(new Transform{ {100.0f + 200.0f * i, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f} });
            objectPtr->AddComponent(new AdjustTransform{ {0.0f, 0.0f, 0.0f }, {0.0f, 0.0f, 0.0f}, {scale, scaleY, scale} });
            objectPtr->AddComponent(new Collider{ {0.0f, 0.0f, 0.0f}, {2.3f * scale, 1.5f * scaleY, 1.3f * scale} });
            objectPtr->AddComponent(new Mesh{ "cloud1.fbx" });
            objectPtr->AddComponent(new Texture{ L"Gray", 1.0f, 0.4f });
            objectPtr->AddComponent(new Gravity);
            AddObj(objectPtr);
        }

        for (int i = 0; i < repeat; ++i)
        {
            objectPtr = new TestObject(this, AllocateId());
            objectPtr->AddComponent(new Transform{ {100.0f + 200.0f * i, 0.0f, 2560.0f}, {0.0f, 0.0f, 0.0f} });
            objectPtr->AddComponent(new AdjustTransform{ {0.0f, 0.0f, 0.0f }, {0.0f, 0.0f, 0.0f}, {scale, scaleY, scale} });
            objectPtr->AddComponent(new Collider{ {0.0f, 0.0f, 0.0f}, {2.3f * scale, 1.5f * scaleY, 1.3f * scale} });
            objectPtr->AddComponent(new Mesh{ "cloud1.fbx" });
            objectPtr->AddComponent(new Texture{ L"Gray", 1.0f, 0.4f });
            objectPtr->AddComponent(new Gravity);
            AddObj(objectPtr);
        }
        for (int i = 0; i < repeat; ++i)
        {
            objectPtr = new TestObject(this, AllocateId());
            objectPtr->AddComponent(new Transform{ {0.0f, 0.0f, 100.0f + 200.0f * i}, {0.0f, 90.0f, 0.0f} });
            objectPtr->AddComponent(new AdjustTransform{ {0.0f, 0.0f, 0.0f }, {0.0f, 0.0f, 0.0f}, {scale, scaleY, scale} });
            objectPtr->AddComponent(new Collider{ {0.0f, 0.0f, 0.0f}, {2.3f * scale, 1.5f * scaleY, 1.3f * scale} });
            objectPtr->AddComponent(new Mesh{ "cloud1.fbx" });
            objectPtr->AddComponent(new Texture{ L"LightGray", 1.0f, 0.4f });
            objectPtr->AddComponent(new Gravity);
            AddObj(objectPtr);
        }

        for (int i = 0; i < repeat; ++i)
        {
            objectPtr = new TestObject(this, AllocateId());
            objectPtr->AddComponent(new Transform{ {2560.0f, 0.0f, 100.0f + 200.0f * i}, {0.0f, 90.0f, 0.0f} });
            objectPtr->AddComponent(new AdjustTransform{ {0.0f, 0.0f, 0.0f }, {0.0f, 0.0f, 0.0f}, {scale, scaleY, scale} });
            objectPtr->AddComponent(new Collider{ {0.0f, 0.0f, 0.0f}, {2.3f * scale, 1.5f * scaleY, 1.3f * scale} });
            objectPtr->AddComponent(new Mesh{ "cloud1.fbx" });
            objectPtr->AddComponent(new Texture{ L"LightGray", 1.0f, 0.4f });
            objectPtr->AddComponent(new Gravity);
            AddObj(objectPtr);
        }
    }



    
    // Base 스테이지 생성 완료 후 네트워크 업데이트 전송
    if (m_parent) {
        // 플레이어가 Base 스테이지 시작 위치로 이동했음을 네트워크로 전송
        m_parent->GetNetworkManager().SendPlayerUpdate(
            430.0f, 0.0f, 150.0f,  // Base 스테이지 시작 위치
            0.0f  // 회전
        );
        
        OutputDebugString(L"[Scene] BuildBaseStage: Sent position update to Base stage start position\n");
        
        // 스테이지 전환 완료 - 다른 플레이어 생성 허용
        OtherPlayerManager::GetInstance()->SetStageTransitioning(false);
        OutputDebugString(L"[Scene] BuildBaseStage: Stage transition completed\n");
    }
}

void Scene::BuildGodStage()
{
    OutputDebugString(L"[Scene] BuildGodStage() called - Building God Stage\n");
    m_current_stage = L"God";
    OutputDebugString(L"[Scene] Current stage set to: God\n");
    
    Object* objectPtr = nullptr;

    // 카메라
    {
        mMainCameraId = AllocateId();
        objectPtr = new CameraObject(this, mMainCameraId);
        objectPtr->AddComponent(new Transform{ {0.0f, 0.0f, 0.0f} });
        AddObj(objectPtr);
    }

    // 플레이어
    {
        float scale = 0.1f;
        objectPtr = new PlayerObject(this, AllocateId());
        // 플레이어 생명력을 명시적으로 3으로 설정
        dynamic_cast<PlayerObject*>(objectPtr)->SetLife(3);
        objectPtr->AddComponent(new Transform{ {150.0f, 0.0f, 100.0f} });
        objectPtr->AddComponent(new AdjustTransform{ {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {scale, scale, scale} });
        objectPtr->AddComponent(new Mesh{ "1P(boy-idle).fbx" });
        objectPtr->AddComponent(new Texture{ L"boy" , 1.0f, 0.4f });
        objectPtr->AddComponent(new Animation{ "1P(boy-idle).fbx" });
        objectPtr->AddComponent(new Gravity);
        objectPtr->AddComponent(new Collider{ {0.0f, 80.0f * scale, 0.0f}, {30.0f * scale, 80.0f * scale, 30.0f * scale} });
        AddObj(objectPtr);
        
        OutputDebugString(L"[Scene] Created local player in God stage with life: 3\n");
    }

  

    // 기존 다른 플레이어들을 다시 생성
    OutputDebugString(L"[Scene] Recreating other players for God Stage...\n");
    if (m_parent) {
        OutputDebugString(L"[Scene] m_parent found, accessing OtherPlayerManager...\n");
        auto& otherPlayers = OtherPlayerManager::GetInstance()->GetPlayers();
        OutputDebugString(L"[Scene] Got other players map, size: ");
        
        wchar_t debugMsg[256];
        swprintf_s(debugMsg, L"%zu\n", otherPlayers.size());
        OutputDebugString(debugMsg);
        
        if (otherPlayers.empty()) {
            OutputDebugString(L"[Scene] No other players found in OtherPlayerManager\n");
            // 서버에 다른 플레이어들의 최신 정보 요청
            if (m_parent && m_parent->IsNetworkEnabled()) {
                OutputDebugString(L"[Scene] Requesting latest player info from server...\n");
            }
        } else {
            OutputDebugString(L"[Scene] Found other players, recreating them...\n");
            for (auto& pair : otherPlayers) {
                int clientID = pair.first;
                PlayerObject* existingPlayer = pair.second;
                
                swprintf_s(debugMsg, L"[Scene] Processing player ID: %d, existingPlayer: %p\n", clientID, existingPlayer);
                OutputDebugString(debugMsg);
                
                if (existingPlayer) {
                    // 기존 플레이어의 위치 정보 가져오기
                    Transform* transform = existingPlayer->GetComponent<Transform>();
                    if (transform) {
                        XMFLOAT3 position;
                        XMStoreFloat3(&position, transform->GetPosition());
                        
                        // 기존 플레이어 객체를 Scene에서 제거 (메모리 해제는 하지 않음)
                        existingPlayer->Delete();
                        
                        // 새로운 플레이어 객체 생성
                        float scale = 0.1f;
                        Object* newPlayer = new PlayerObject(this, AllocateId());
                        dynamic_cast<PlayerObject*>(newPlayer)->SetIsNetworkPlayer(true);  // 네트워크 플레이어로 설정
                        dynamic_cast<PlayerObject*>(newPlayer)->SetLife(3);  // 네트워크 플레이어 생명력도 3으로 설정
                        newPlayer->AddComponent(new Transform{ {position.x, position.y, position.z} });
                        newPlayer->AddComponent(new AdjustTransform{ {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {scale, scale, scale} });
                        newPlayer->AddComponent(new Mesh{ "1P(boy-idle).fbx" });
                        newPlayer->AddComponent(new Texture{ L"boy" , 1.0f, 0.4f });
                        newPlayer->AddComponent(new Animation{ "1P(boy-idle).fbx" });
                        newPlayer->AddComponent(new Gravity);
                        newPlayer->AddComponent(new Collider{ {0.0f, 8.0f, 0.0f}, {2.0f, 8.0f, 2.0f} });
                        AddObj(newPlayer);
                        
                        // OtherPlayerManager에서 참조 업데이트
                        pair.second = dynamic_cast<PlayerObject*>(newPlayer);
                        
                        swprintf_s(debugMsg, L"[Scene] Recreated other player ID: %d at position (%.1f, %.1f, %.1f)\n", 
                                  clientID, position.x, position.y, position.z);
                        OutputDebugString(debugMsg);
                    } else {
                        swprintf_s(debugMsg, L"[Scene] Transform component not found for player ID: %d\n", clientID);
                        OutputDebugString(debugMsg);
                    }
                } else {
                    // 플레이어가 nullptr이면 새로 생성 (기본 위치 + 기본 애니메이션)
                    swprintf_s(debugMsg, L"[Scene] Creating new player for ID: %d (was null)\n", clientID);
                    OutputDebugString(debugMsg);
                    
                    float scale = 0.1f;
                    Object* newPlayer = new PlayerObject(this, AllocateId());
                    dynamic_cast<PlayerObject*>(newPlayer)->SetIsNetworkPlayer(true);  // 네트워크 플레이어로 설정
                    dynamic_cast<PlayerObject*>(newPlayer)->SetLife(3);  // 네트워크 플레이어 생명력도 3으로 설정
                    newPlayer->AddComponent(new Transform{ {300.0f, 0.0f, 300.0f} });  // 기본 위치
                    newPlayer->AddComponent(new AdjustTransform{ {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {scale, scale, scale} });
                    newPlayer->AddComponent(new Mesh{ "1P(boy-idle).fbx" });
                    newPlayer->AddComponent(new Texture{ L"boy" , 1.0f, 0.4f });
                    
                    // 기본 애니메이션 설정 (idle 애니메이션)
                    Animation* anim = new Animation{ "1P(boy-idle).fbx" };
                    anim->mAnimationTime = 0.0f;  // 애니메이션 시작 시간
                    newPlayer->AddComponent(anim);
                    
                    newPlayer->AddComponent(new Gravity);
                    newPlayer->AddComponent(new Collider{ {0.0f, 8.0f, 0.0f}, {2.0f, 8.0f, 2.0f} });
                    AddObj(newPlayer);
                    
                    // OtherPlayerManager에서 참조 업데이트
                    pair.second = dynamic_cast<PlayerObject*>(newPlayer);
                    
                    swprintf_s(debugMsg, L"[Scene] Created new player ID: %d with default idle animation\n", clientID);
                    OutputDebugString(debugMsg);
                }
            }
        }
        OutputDebugString(L"[Scene] Other players recreation completed\n");
    } else {
        OutputDebugString(L"[Scene] m_parent is null, cannot access OtherPlayerManager\n");
    }

    // 지형
    {
        objectPtr = new TestObject(this, AllocateId());
        objectPtr->AddComponent(new Transform{ {0.0f, 0.0f, 0.0f} });
        objectPtr->AddComponent(new Mesh{ "HalfPlane" });
        objectPtr->AddComponent(new Texture{ L"grass", 1.0f, 0.4f });
        AddObj(objectPtr);
    }
    // ����
    {
        float scale = 30.0f;
        objectPtr = new TreeObject(this, AllocateId());
        objectPtr->AddComponent(new Transform{ {200.0f, 0.0f, 150.0f} });
        objectPtr->AddComponent(new AdjustTransform{ {-1.0f * scale, 0.0f * scale, 0.0f * scale}, {0.0f, 0.0f, 0.0f}, {scale, scale, scale} });
        objectPtr->AddComponent(new Mesh{ "normal_tree.fbx" });
        objectPtr->AddComponent(new Texture{ L"normalTree", 1.0f, 0.4f });
        objectPtr->AddComponent(new Collider{ {0.0f, 1.0f * scale, 0.0f}, {0.15f * scale, 1.0f * scale, 0.15f * scale} });
        AddObj(objectPtr);
    }

    // ������
    {
        objectPtr = new PuzzleFrameObject(this, AllocateId());
        objectPtr->AddComponent(new Transform{ {300.0f, 0.0f, 150.0f}, {-90.0f, 0.0f, 0.0f}, {100.0f, 1.0f, 100.0f} });
        objectPtr->AddComponent(new Mesh{ "Quad" });
        objectPtr->AddComponent(new Texture{ L"PuzzleFrame", -1.0f, 0.4f });
        AddObj(objectPtr);
    }

    // 나무
    {
        float scale = 5.0f;
        objectPtr = new TestObject(this, AllocateId());
        objectPtr->AddComponent(new Transform{ {250.0f, 0.0f, 250.0f} });
        objectPtr->AddComponent(new AdjustTransform{ {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {scale, scale, scale} });
        objectPtr->AddComponent(new Mesh{ "wood.fbx" });
        objectPtr->AddComponent(new Texture{ L"wood", 1.0f, 0.4f });
        objectPtr->AddComponent(new Gravity);
        objectPtr->AddComponent(new Collider{ {0.0f, 35.0f * scale, 0.0f}, {10.0f * scale, 35.0f * scale, 10.0f * scale} });
        AddObj(objectPtr);
    }

    // 도끼
    {
        float scale = 30.0f;
        objectPtr = new AxeObject(this, AllocateId());
        objectPtr->AddComponent(new Transform{ {250.0f, 500.0f, 250.0f} });
        objectPtr->AddComponent(new AdjustTransform{ {0.0f, 0.1f * scale, 0.0f}, {0.0f, -90.0f, 0.0f}, {scale, scale, scale} });
        objectPtr->AddComponent(new Mesh{ "axe.fbx" });
        objectPtr->AddComponent(new Texture{ L"axe", 1.0f, 0.4f });
        objectPtr->AddComponent(new Gravity);
        objectPtr->AddComponent(new Collider{ {0.0f, 0.2f * scale, 0.0f}, {0.1f * scale, 0.2f * scale, 0.05f * scale} });
        AddObj(objectPtr);
    }

    // 울타리들
    {
        float offsetZ = 100.0f;
        float offsetX = 100.0f;
        float offsetY = 50.0f;
        float scale = 0.3f;
        for (int i = 0; i < 4; ++i)
        {
            objectPtr = new TestObject(this, AllocateId());
            objectPtr->AddComponent(new Transform{ {100.0f, offsetY * i, 100.0f + offsetZ * i}, {-30.0f, 0.0f, 0.0f} });
            objectPtr->AddComponent(new AdjustTransform{ {100.0f * scale, 0.0f, -150.0f * scale }, {0.0f, -90.0f, 0.0f}, {scale, scale, scale} });
            objectPtr->AddComponent(new Mesh{ "fence.fbx" });
            objectPtr->AddComponent(new Texture{ L"Brown", 1.0f, 0.4f });
            objectPtr->AddComponent(new Collider{ {0.0f, 0.0f, 0.0f}, {100.0f * scale, 15.0f * scale, 150.0f * scale } });
            AddObj(objectPtr);
        }

        for (int i = 0; i < 3; ++i)
        {
            objectPtr = new TestObject(this, AllocateId());
            objectPtr->AddComponent(new Transform{ {200.0f + offsetX * i, 150.0f + offsetY * i, 400.0f }, {-30.0f, 90.0f, 0.0f} });
            objectPtr->AddComponent(new AdjustTransform{ {100.0f * scale, 0.0f, -150.0f * scale }, {0.0f, -90.0f, 0.0f}, {scale, scale, scale} });
            objectPtr->AddComponent(new Mesh{ "fence.fbx" });
            objectPtr->AddComponent(new Texture{ L"Brown", 1.0f, 0.4f });
            objectPtr->AddComponent(new Collider{ {0.0f, 0.0f, 0.0f}, {100.0f * scale, 15.0f * scale, 150.0f * scale } });
            AddObj(objectPtr);
        }

        for (int i = 0; i < 3; ++i)
        {
            objectPtr = new TestObject(this, AllocateId());
            objectPtr->AddComponent(new Transform{ {400.0f, 250.0f + offsetY * i, 300.0f - offsetZ * i  }, {30.0f, 00.0f, 0.0f} });
            objectPtr->AddComponent(new AdjustTransform{ {100.0f * scale, 0.0f, -150.0f * scale }, {0.0f, -90.0f, 0.0f}, {scale, scale, scale} });
            objectPtr->AddComponent(new Mesh{ "fence.fbx" });
            objectPtr->AddComponent(new Texture{ L"Brown", 1.0f, 0.4f });
            objectPtr->AddComponent(new Collider{ {0.0f, 0.0f, 0.0f}, {100.0f * scale, 15.0f * scale, 150.0f * scale } });
            AddObj(objectPtr);
        }

        objectPtr = new TestObject(this, AllocateId());
        objectPtr->AddComponent(new Transform{ {300.0f, 350.0f, 50.0f  }, {0.0f, 90.0f, 0.0f} });
        objectPtr->AddComponent(new AdjustTransform{ {100.0f * scale, 0.0f, -150.0f * scale }, {0.0f, -90.0f, 0.0f}, {scale, scale, scale} });
        objectPtr->AddComponent(new Mesh{ "fence.fbx" });
        objectPtr->AddComponent(new Texture{ L"Brown", 1.0f, 0.4f });
        objectPtr->AddComponent(new Collider{ {0.0f, 0.0f, 0.0f}, {100.0f * scale, 15.0f * scale, 150.0f * scale } });
        AddObj(objectPtr);

        objectPtr = new RotFenceObject(this, AllocateId());
        objectPtr->AddComponent(new Transform{ {250.0f, 350.0f, 150.0f  }, {0.0f, 0.0f, 0.0f} });
        objectPtr->AddComponent(new AdjustTransform{ {100.0f * scale, 0.0f, -150.0f * scale }, {0.0f, -90.0f, 0.0f}, {scale, scale, scale} });
        objectPtr->AddComponent(new Mesh{ "fence.fbx" });
        objectPtr->AddComponent(new Texture{ L"Brown", 1.0f, 0.4f });
        objectPtr->AddComponent(new Collider{ {0.0f, 0.0f, 0.0f}, {100.0f * scale, 15.0f * scale, 150.0f * scale } });
        AddObj(objectPtr);
    }
    //��躮
    {
        float scale = 44.0f;
        float scaleY = scale * 1.5f;
        int repeat = 3;
        for (int i = 0; i < repeat; ++i)
        {
            objectPtr = new TestObject(this, AllocateId());
            objectPtr->AddComponent(new Transform{ {100.0f + 200.0f * i, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f} });
            objectPtr->AddComponent(new AdjustTransform{ {0.0f, 0.0f, 0.0f }, {0.0f, 0.0f, 0.0f}, {scale, scaleY, scale} });
            objectPtr->AddComponent(new Collider{ {0.0f, 0.0f, 0.0f}, {2.3f * scale, 1.5f * scaleY, 1.3f * scale} });
            objectPtr->AddComponent(new Mesh{ "cloud1.fbx" });
            objectPtr->AddComponent(new Texture{ L"Gray", 1.0f, 0.4f });
            objectPtr->AddComponent(new Gravity);
            AddObj(objectPtr);
        }

        for (int i = 0; i < repeat; ++i)
        {
            objectPtr = new TestObject(this, AllocateId());
            objectPtr->AddComponent(new Transform{ {100.0f + 200.0f * i, 0.0f, 2560.0f}, {0.0f, 0.0f, 0.0f} });
            objectPtr->AddComponent(new AdjustTransform{ {0.0f, 0.0f, 0.0f }, {0.0f, 0.0f, 0.0f}, {scale, scaleY, scale} });
            objectPtr->AddComponent(new Collider{ {0.0f, 0.0f, 0.0f}, {2.3f * scale, 1.5f * scaleY, 1.3f * scale} });
            objectPtr->AddComponent(new Mesh{ "cloud1.fbx" });
            objectPtr->AddComponent(new Texture{ L"Gray", 1.0f, 0.4f });
            objectPtr->AddComponent(new Gravity);
            AddObj(objectPtr);
        }

        for (int i = 0; i < repeat; ++i)
        {
            objectPtr = new TestObject(this, AllocateId());
            objectPtr->AddComponent(new Transform{ {0.0f, 0.0f, 100.0f + 200.0f * i}, {0.0f, 90.0f, 0.0f} });
            objectPtr->AddComponent(new AdjustTransform{ {0.0f, 0.0f, 0.0f }, {0.0f, 0.0f, 0.0f}, {scale, scaleY, scale} });
            objectPtr->AddComponent(new Collider{ {0.0f, 0.0f, 0.0f}, {2.3f * scale, 1.5f * scaleY, 1.3f * scale} });
            objectPtr->AddComponent(new Mesh{ "cloud1.fbx" });
            objectPtr->AddComponent(new Texture{ L"Gray", 1.0f, 0.4f });
            objectPtr->AddComponent(new Gravity);
            AddObj(objectPtr);
        }

        for (int i = 0; i < repeat; ++i)
        {
            objectPtr = new TestObject(this, AllocateId());
            objectPtr->AddComponent(new Transform{ {2560.0f, 0.0f, 100.0f + 200.0f * i}, {0.0f, 90.0f, 0.0f} });
            objectPtr->AddComponent(new AdjustTransform{ {0.0f, 0.0f, 0.0f }, {0.0f, 0.0f, 0.0f}, {scale, scaleY, scale} });
            objectPtr->AddComponent(new Collider{ {0.0f, 0.0f, 0.0f}, {2.3f * scale, 1.5f * scaleY, 1.3f * scale} });
            objectPtr->AddComponent(new Mesh{ "cloud1.fbx" });
            objectPtr->AddComponent(new Texture{ L"Gray", 1.0f, 0.4f });
            objectPtr->AddComponent(new Gravity);
            AddObj(objectPtr);
        }
    }
    
    // 스테이지 전환 완료 - 다른 플레이어 생성 허용
    if (m_parent) {
        OtherPlayerManager::GetInstance()->SetStageTransitioning(false);
        OutputDebugString(L"[Scene] BuildGodStage: Stage transition completed\n");
    }
    
    OutputDebugString(L"[Scene] BuildGodStage completed successfully\n");

}

void Scene::BuildEndStage()
{
    m_current_stage = L"End";
    Object* objectPtr = nullptr;

    // ī�޶�
    {
        mMainCameraId = AllocateId();
        objectPtr = new CameraObject(this, mMainCameraId);
        objectPtr->AddComponent(new Transform{ {0.0f, 0.0f, 0.0f} });
        AddObj(objectPtr);
    }

    // �÷��̾�
    {
        float scale = 0.1f;
        objectPtr = new PlayerObject(this, AllocateId());
        objectPtr->AddComponent(new Transform{ {500.0f, 0.0f, 500.0f} });
        objectPtr->AddComponent(new AdjustTransform{ {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {scale, scale, scale} });
        objectPtr->AddComponent(new Mesh{ "1P(boy-idle).fbx" });
        objectPtr->AddComponent(new Texture{ L"boy" , 1.0f, 0.4f });
        objectPtr->AddComponent(new Animation{ "1P(boy-idle).fbx" });
        objectPtr->AddComponent(new Gravity);
        objectPtr->AddComponent(new Collider{ {0.0f, 80.0f * scale, 0.0f}, {30.0f * scale, 80.0f * scale, 30.0f * scale} });
        AddObj(objectPtr);
    }

    // End
    {
        objectPtr = new TitleQuadObject(this, AllocateId(), mMainCameraId);
        objectPtr->AddComponent(new Transform{ {-0.5f * 1.77f, -0.5f, 1.0f}, {-90.0f, 0.0f, 0.0f}, {1.77f, 1.0f, 1.0f} });
        objectPtr->AddComponent(new Mesh{ "Quad" });
        objectPtr->AddComponent(new Texture{ L"End", -1.0f, 0.4f });
        AddObj(objectPtr);
    }

}

void Scene::BuildUI()
{
    if (m_current_stage == L"Title") return;
    if (m_current_stage == L"End") return;
    // UI
    Object* objectPtr = nullptr;

    float depthFactor = 0.11f;
    float scale = 0.1f;
    float textureRatio = 420.0f / 112.0f; // �ؽ�ó ����
    objectPtr = new LifeQuadObject(this, AllocateId(), mMainCameraId);
    objectPtr->AddComponent(new Transform{ {-0.8f * depthFactor, 0.4f * depthFactor, 1.0f * depthFactor}, {-90.0f, 0.0f, 0.0f}, {depthFactor * textureRatio * scale, 1.0f, depthFactor * scale} });
    objectPtr->AddComponent(new Mesh{ "Quad" });
    objectPtr->AddComponent(new Texture{ L"Life3", -1.0f, 0.4f });
    AddObj(objectPtr);

    scale = 0.15;
    textureRatio = 256.0f / 256.0f; // �ؽ�ó ����
    objectPtr = new BoyIconQuadObject(this, AllocateId(), mMainCameraId);
    objectPtr->AddComponent(new Transform{ {-1.0f * depthFactor, 0.4f * depthFactor, 1.0f * depthFactor}, {-90.0f, 0.0f, 0.0f}, {depthFactor * textureRatio * scale, 1.0f, depthFactor * scale} });
    objectPtr->AddComponent(new Mesh{ "Quad" });
    objectPtr->AddComponent(new Texture{ L"BoyIcon", -1.0f, 0.4f });
    AddObj(objectPtr);

    scale = 0.25;
    textureRatio = 256.0f / 328.0f; // �ؽ�ó ����
    objectPtr = new RiceCakeQuadObject(this, AllocateId(), mMainCameraId);
    objectPtr->AddComponent(new Transform{ {-1.0f * depthFactor, -0.55f * depthFactor, 1.0f * depthFactor}, {-90.0f, 0.0f, 0.0f}, {depthFactor * textureRatio * scale, 1.0f, depthFactor * scale} });
    objectPtr->AddComponent(new Mesh{ "Quad" });
    objectPtr->AddComponent(new Texture{ L"RiceCake0", -1.0f, 0.4f });
    AddObj(objectPtr);

    scale = 0.25;
    textureRatio = 256.0f / 256.0f; // �ؽ�ó ����
    objectPtr = new TigerLeatherQuadObject(this, AllocateId(), mMainCameraId);
    objectPtr->AddComponent(new Transform{ {0.7f * depthFactor, -0.55f * depthFactor, 1.0f * depthFactor}, {-90.0f, 0.0f, 0.0f}, {depthFactor * textureRatio * scale, 1.0f, depthFactor * scale} });
    objectPtr->AddComponent(new Mesh{ "Quad" });
    objectPtr->AddComponent(new Texture{ L"White", -1.0f, 0.4f });
    AddObj(objectPtr);
}

void Scene::BuildTitleStage()
{
    m_current_stage = L"Title";
    Object* objectPtr = nullptr;

    // ī�޶�
    {
        mMainCameraId = AllocateId();
        objectPtr = new CameraObject(this, mMainCameraId);
        objectPtr->AddComponent(new Transform{ {0.0f, 0.0f, 0.0f} });
        AddObj(objectPtr);
    }

    // �÷��̾�
    {
        float scale = 0.1f;
        objectPtr = new PlayerObject(this, AllocateId());
        objectPtr->AddComponent(new Transform{ {500.0f, 0.0f, 500.0f} });
        objectPtr->AddComponent(new AdjustTransform{ {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {scale, scale, scale} });
        objectPtr->AddComponent(new Mesh{ "1P(boy-idle).fbx" });
        objectPtr->AddComponent(new Texture{ L"boy" , 1.0f, 0.4f });
        objectPtr->AddComponent(new Animation{ "1P(boy-idle).fbx" });
        objectPtr->AddComponent(new Gravity);
        objectPtr->AddComponent(new Collider{ {0.0f, 80.0f * scale, 0.0f}, {30.0f * scale, 80.0f * scale, 30.0f * scale} });
        AddObj(objectPtr);
    }

    // Title
    {
        float scale = 0.2f;
        float ratio = GetAspectRatio();
        objectPtr = new TitleQuadObject(this, AllocateId(), mMainCameraId);
        objectPtr->AddComponent(new Transform{ {-0.5f * ratio * scale, -0.5f * scale, scale}, {-90.0f, 0.0f, 0.0f}, {scale * ratio, scale, scale} });
        objectPtr->AddComponent(new Mesh{ "Quad" });
        objectPtr->AddComponent(new Texture{ L"Title", -1.0f, 0.4f });
        AddObj(objectPtr);
    }

}


void Scene::BuildShadow()
{
    m_shadow = make_unique<Shadow>(this, 2048, 2048);
}

void Scene::BuildShaders()
{
    m_shaders["VS_Opaque"] = CompileShader(L"Shaders/Opaque.hlsl", nullptr, "VS", "vs_5_1");
    m_shaders["PS_Opaque"] = CompileShader(L"Shaders/Opaque.hlsl", nullptr, "PS", "ps_5_1");
    m_shaders["VS_Shadow"] = CompileShader(L"Shaders/Shadow.hlsl", nullptr, "VS", "vs_5_1");
    m_shaders["PS_Shadow"] = CompileShader(L"Shaders/Shadow.hlsl", nullptr, "PS", "ps_5_1");
}

void Scene::BuildInputElement()
{
    // Define the vertex input layout.
    //m_inputElement.reserve(5);
    m_inputElement =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "WEIGHT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 32, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "BONEINDEX", 0, DXGI_FORMAT_R32G32B32A32_SINT, 0, 48, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
    };
}

ComPtr<ID3DBlob> Scene::CompileShader(
    const std::wstring& fileName, const D3D_SHADER_MACRO* defines, const std::string& entryPoint, const std::string& target)
{
    UINT compileFlags = 0;
#if defined(_DEBUG) || defined(DBG)
    compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    HRESULT hr;

    Microsoft::WRL::ComPtr<ID3DBlob> byteCode = nullptr;
    Microsoft::WRL::ComPtr<ID3DBlob> errors;
    hr = D3DCompileFromFile(fileName.c_str(), defines, D3D_COMPILE_STANDARD_FILE_INCLUDE,
        entryPoint.c_str(), target.c_str(), compileFlags, 0, &byteCode, &errors);

    if (errors != nullptr)
    {
        OutputDebugStringA((char*)errors->GetBufferPointer());
    }
    ThrowIfFailed(hr);

    return byteCode;
}

void Scene::RenderObjects(ID3D12Device* device, ID3D12GraphicsCommandList* commandList)
{
    for (Object* obj : m_objects)
    {
        if (!obj->GetValid()) continue;
        obj->OnRender(device, commandList);
    }
}

char Scene::ClampToBounds(XMVECTOR& pos, XMVECTOR offset)
{
    XMFLOAT3 p;
    XMStoreFloat3(&p, pos);
    auto [minX, minY, minZ, maxX, maxZ] = GetBounds(p.x, p.z);

    float offsetX = XMVectorGetX(offset);
    float offsetY = XMVectorGetY(offset);
    float offsetZ = XMVectorGetZ(offset);

    char outStatus = 0x00;

    if (p.x <= minX + offsetX)
    {
        outStatus |= 0x08;
        p.x = minX + offsetX;
    }
    else if (p.x >= maxX - offsetX)
    {
        outStatus |= 0x08;
        p.x = maxX - offsetX;
    }

    if (p.y <= minY + offsetY)
    {
        outStatus |= 0x04;
        p.y = minY + offsetY;
    }

    if (p.z <= minZ + offsetZ)
    {
        outStatus |= 0x02;
        p.z = minZ + offsetZ;
    }
    else if (p.z >= maxZ - offsetZ)
    {
        outStatus |= 0x02;
        p.z = maxZ - offsetZ;
    }

    pos = XMLoadFloat3(&p);

    return outStatus;
}

std::tuple<float, float, float, float, float> Scene::GetBounds(float x, float z)
{
    //   stage  ٿ ȯϱ
    float minX = 0.0f;
    float minY = 0.0f;
    float minZ = 0.0f;

    float maxX = 1000.0f;
    float maxZ = 1000.0f;

    if (m_current_stage == L"God")
    {
        maxX = 500.0f;
        maxZ = 500.0f;
    }

    if (m_current_stage == L"Hunting")
    {
        ResourceManager& rm = GetResourceManager();
        int width = rm.GetTerrainData().terrainWidth;
        int height = rm.GetTerrainData().terrainHeight;
        int terrainScale = rm.GetTerrainData().terrainScale;

        vector<Vertex>& vertexBuffer = rm.GetVertexBuffer();
        UINT startVertex = rm.GetSubMeshData("HeightMap.raw").baseVertexLocation;

        int indexX = (int)(x / terrainScale);
        int indexZ = (int)(z / terrainScale);
        if (indexX < 0) indexX = 0;
        if (indexZ < 0) indexZ = 0;

        float leftBottom = vertexBuffer[startVertex + indexZ * width + indexX].position.y;
        float rightBottom = vertexBuffer[startVertex + indexZ * width + indexX + 1].position.y;
        float leftTop = vertexBuffer[startVertex + (indexZ + 1) * width + indexX].position.y;
        float rightTop = vertexBuffer[startVertex + (indexZ + 1) * width + indexX + 1].position.y;

        float offsetX = x / terrainScale - indexX;
        float offsetZ = z / terrainScale - indexZ;

        float lerpXBottom = (1 - offsetX) * leftBottom + offsetX * rightBottom;
        float lerpXTop = (1 - offsetX) * leftTop + offsetX * rightTop;

        float lerpZ = (1 - offsetZ) * lerpXBottom + offsetZ * lerpXTop;

        minY = lerpZ;
        maxX = (width - 1) * terrainScale;
        maxZ = (height - 1) * terrainScale;
    }

    return { minX, minY, minZ, maxX, maxZ };
}

int Scene::GetTextureIndex(wstring name)
{
    auto it = m_texture_name_to_index.find(name);
    if (it != m_texture_name_to_index.end()) {
        return it->second;
    }
    // 텍스처를 찾을 수 없는 경우 기본값 반환
    OutputDebugString(L"[Scene] Texture not found: ");
    OutputDebugString(name.c_str());
    OutputDebugString(L"\n");
    return -1;
}

std::tuple<XMVECTOR, float> Scene::GetCollisionData(BoundingOrientedBox OBB1, BoundingOrientedBox OBB2)
{
    XMVECTOR Center1 = XMLoadFloat3(&OBB1.Center);
    XMVECTOR Center2 = XMLoadFloat3(&OBB2.Center);
    XMVECTOR centerToCenter = Center2 - Center1;

    XMVECTOR quaternion1 = XMLoadFloat4(&OBB1.Orientation);
    XMVECTOR axes1[3]{};
    axes1[0] = XMVector3Rotate({ 1.0f, 0.0f, 0.0f }, quaternion1);
    axes1[1] = XMVector3Rotate({ 0.0f, 1.0f, 0.0f }, quaternion1);
    axes1[2] = XMVector3Rotate({ 0.0f, 0.0f, 1.0f }, quaternion1);

    XMVECTOR quaternion2 = XMLoadFloat4(&OBB2.Orientation);
    XMVECTOR axes2[3]{};
    axes2[0] = XMVector3Normalize(XMVector3Rotate({ 1.0f, 0.0f, 0.0f }, quaternion2));
    axes2[1] = XMVector3Normalize(XMVector3Rotate({ 0.0f, 1.0f, 0.0f }, quaternion2));
    axes2[2] = XMVector3Normalize(XMVector3Rotate({ 0.0f, 0.0f, 1.0f }, quaternion2));

    const int testAxesCount = 15;
    XMVECTOR testAxes[testAxesCount] = {
        axes1[0], axes1[1], axes1[2],
        axes2[0], axes2[1], axes2[2]
    };

    int offset = 6;
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            testAxes[offset++] = XMVector3Normalize(XMVector3Cross(axes1[i], axes2[j]));
        }
    }

    auto GetProjValue = [](XMVECTOR axis, XMVECTOR* axes, XMFLOAT3 extents) {
        return fabs(XMVectorGetX(XMVector3Dot(axis, axes[0]))) * extents.x +
            fabs(XMVectorGetX(XMVector3Dot(axis, axes[1]))) * extents.y +
            fabs(XMVectorGetX(XMVector3Dot(axis, axes[2]))) * extents.z; };

    float penetration = FLT_MAX;
    XMVECTOR normal = XMVectorZero();
    for (int i = 0; i < testAxesCount; ++i) {
        float lengthSq = XMVectorGetX(XMVector3LengthSq(testAxes[i]));
        if (lengthSq < 0.001f) continue;

        XMVECTOR axis = testAxes[i];
        float projValue1 = GetProjValue(axis, axes1, OBB1.Extents);
        float projValue2 = GetProjValue(axis, axes2, OBB2.Extents);
        float distance = fabs(XMVectorGetX(XMVector3Dot(centerToCenter, axis)));
        float overlap = projValue1 + projValue2 - distance;

        if (overlap < penetration) {
            penetration = overlap;
            normal = axis;
        }
    }

    if (XMVectorGetX(XMVector3Dot(normal, centerToCenter)) < 0.0f) {
        normal = -normal;
    }

    return { normal, penetration };
}

Object* Scene::GetObjFromId(uint32_t id)
{
    Object* temp = nullptr;
    for (Object* obj : m_objects) {
        if (!obj->GetValid()) continue;
        if (id == obj->GetId()) {
            return obj;
        }
    }
    return nullptr;
}



void Scene::CompactObjects()
{
    auto func = [](Object* obj) -> bool
        {
            bool result = obj->GetValid();
            if (!result) delete obj;
            return !result;
        };

    auto it = std::remove_if(m_objects.begin(), m_objects.end(), func);
    m_objects.erase(it, m_objects.end());
}

void Scene::ProcessObjectQueue()
{
    for (int i = 0; i < m_object_queue_index; ++i) {
        m_objects.push_back(m_object_queue[i]);
    }
    m_object_queue_index = 0;
}

uint32_t Scene::AllocateId()
{
    return m_id_counter++;
}

void Scene::SetStage(wstring stage)
{
    //OutputDebugString(L"[Scene] SetStage: Starting stage transition to " + stage + L"\n");
    
    // 스테이지 전환 시 네트워크 업데이트 전송
    if (m_parent && stage == L"Base") {
        // Base 스테이지로 전환할 때 현재 플레이어 위치를 네트워크로 전송
        PlayerObject* player = GetLocalPlayer();
        if (player) {
            Transform* transform = player->GetComponent<Transform>();
            if (transform) {
                XMVECTOR pos = transform->GetPosition();
                XMFLOAT3 position;
                XMStoreFloat3(&position, pos);
                
                // 네트워크 매니저를 통해 위치 업데이트 전송
                XMVECTOR rotation = transform->GetRotation();
                XMFLOAT3 rotFloat;
                XMStoreFloat3(&rotFloat, rotation);
                
                m_parent->GetNetworkManager().SendPlayerUpdate(
                    position.x, position.y, position.z, 
                    rotFloat.y
                );
                
                OutputDebugString(L"[Scene] SetStage: Sent position update to Base stage\n");
            }
        }
    }
    
    // Hunting 스테이지로 전환할 때 서버에 알림
    if (m_parent && stage == L"Hunting") {
        OutputDebugString(L"[Scene] SetStage: Sending Hunting stage change to server\n");
        // 스테이지 변경 중임을 표시
        m_parent->GetNetworkManager().SetStageTransitioning(true);
        m_parent->GetNetworkManager().SendStageChange(L"Hunting");
    }
    
    // Title 스테이지로 전환할 때 네트워크 상태 확인
    if (m_parent && stage == L"Title") {
        OutputDebugString(L"[Scene] SetStage: Switching to Title stage, checking network status\n");
        // Title 스테이지에서는 네트워크 연결 상태만 유지
    }
    
    m_stage_queue = stage;
   // OutputDebugString(L"[Scene] SetStage: Stage transition queued for " + stage + L"\n");
}

void Scene::IncreaseLeatherCount()
{
    ++mLeatherCount;
    mLeatherCount = mLeatherCount > 5 ? 5 : mLeatherCount;
}

void Scene::ResetLeatherCount()
{
    mLeatherCount = 0;
}

bool Scene::HasEnoughLeather()
{

    return mLeatherCount >= 5 ? true : false;
}

float Scene::GetAspectRatio()
{
    return m_viewport.Width / m_viewport.Height;
}

int Scene::GetLeatherCount()
{
    return mLeatherCount;
}

bool Scene::IsTigerQuestAccepted()
{
    return mTigerQuest;
}

void Scene::SetTigerQuestState(bool state)
{
    mTigerQuest = state;
}

XMVECTOR Scene::GetInputDir()
{
    return XMLoadFloat3(&mInputDir);
}

void Scene::ProcessStageQueue()
{
    if (m_stage_queue == L"") return;

    DeleteCurrentObjects();

    if (m_stage_queue == L"Base")
    {
        OutputDebugString(L"[Scene] ProcessStageQueue: Switching to Base Stage\n");
        m_current_stage = L"Base";
        OtherPlayerManager::GetInstance()->SetCurrentStage("Base");
        
        // 서버에 Base 스테이지 변경 알림
        if (m_parent && m_parent->IsNetworkEnabled()) {
            m_parent->GetNetworkManager().SendStageChange(L"Base");
        }
        
        BuildBaseStage();
      
    }
    else if (m_stage_queue == L"Hunting")
    {
        OutputDebugString(L"[Scene] ProcessStageQueue: Switching to Hunting Stage\n");
        OutputDebugString(L"[Scene] About to delete current objects...\n");
       
        OutputDebugString(L"[Scene] Current objects deleted, building Hunting Stage...\n");
        
        // NetworkManager의 호랑이 정보 초기화
        OutputDebugString(L"[Scene] About to clear NetworkManager tiger info...\n");
        if (m_parent) {
            m_parent->GetNetworkManager().ClearTigerInfo();
            OutputDebugString(L"[Scene] NetworkManager tiger info cleared successfully\n");
        } else {
            OutputDebugString(L"[Scene] Warning: m_parent is null, cannot clear NetworkManager tiger info\n");
        }
        
        m_current_stage = L"Hunting";
        OtherPlayerManager::GetInstance()->SetCurrentStage("Hunting");
        
        // 서버에 Hunting 스테이지 변경 알림
        if (m_parent && m_parent->IsNetworkEnabled()) {
            m_parent->GetNetworkManager().SendStageChange(L"Hunting");
        }
        
        BuildHuntingStage();
        OutputDebugString(L"[Scene] Hunting Stage built successfully\n");
       
    }
    else if (m_stage_queue == L"God")
    {
        OutputDebugString(L"[Scene] ProcessStageQueue: Switching to God Stage\n");
        m_current_stage = L"God";
        OtherPlayerManager::GetInstance()->SetCurrentStage("God");
        OtherPlayerManager::GetInstance()->SetStageTransitioning(true);  // 스테이지 전환 시작
        
        // 서버에 God 스테이지 변경 알림
        if (m_parent && m_parent->IsNetworkEnabled()) {
            m_parent->GetNetworkManager().SendStageChange(L"God");
        }
        
        BuildGodStage();
        OutputDebugString(L"[Scene] God Stage built successfully\n");
    }
    else if (m_stage_queue == L"Title")
    {
        m_current_stage = L"Title";
        OtherPlayerManager::GetInstance()->SetCurrentStage("Title");
        
        // 서버에 Title 스테이지 변경 알림
        if (m_parent && m_parent->IsNetworkEnabled()) {
            m_parent->GetNetworkManager().SendStageChange(L"Title");
        }
        
        BuildTitleStage();
       
    }
     else if (m_stage_queue == L"End")
    {
        m_current_stage = L"End";
        OtherPlayerManager::GetInstance()->SetCurrentStage("End");
        
        // 서버에 End 스테이지 변경 알림
        if (m_parent && m_parent->IsNetworkEnabled()) {
            m_parent->GetNetworkManager().SendStageChange(L"End");
        }
        
        BuildEndStage();
       
    }
    BuildUI();
    
    // 스테이지 변경 완료 표시
    if (m_parent) {
        m_parent->GetNetworkManager().SetStageTransitioning(false);
    }
    
    m_stage_queue = L"";
    
    // 스테이지 전환 완료 후 네트워크 상태 확인
    if (m_parent && m_parent->IsNetworkEnabled()) {
        OutputDebugString(L"[Scene] ProcessStageQueue: Stage transition completed, checking network status\n");
        // NetworkManager의 Scene 포인터를 변경하지 않음 - 네트워크 연결 보존
        // m_parent->GetNetworkManager().SetScene(this);  // 이 줄 제거
    }
    
    OutputDebugString(L"[Scene] ProcessStageQueue: Stage transition fully completed\n");
}

int(*Scene::GetPuzzleStatus())[3]
{
        return mPuzzleStatus;
}

void Scene::UpdatePuzzleCellsFromStatus()
{
    // PuzzleFrameObject를 찾아서 퍼즐 셀들을 업데이트
    bool foundPuzzleFrame = false;
    for (Object* obj : m_objects) {
        PuzzleFrameObject* puzzleFrame = dynamic_cast<PuzzleFrameObject*>(obj);
        if (puzzleFrame) {
            OutputDebugString(L"[Scene] PuzzleFrameObject found, updating cells\n");
            puzzleFrame->UpdatePuzzleCellsFromStatus(mPuzzleStatus);
            foundPuzzleFrame = true;
            break;
        }
    }
    
    if (!foundPuzzleFrame) {
        OutputDebugString(L"[Scene] Warning: PuzzleFrameObject not found in scene\n");
    }
}

void Scene::UpdatePuzzleStatusFromCells()
{
    // PuzzleFrameObject를 찾아서 현재 퍼즐 셀들의 상태를 mPuzzleStatus 배열에 반영
    bool foundPuzzleFrame = false;
    for (Object* obj : m_objects) {
        PuzzleFrameObject* puzzleFrame = dynamic_cast<PuzzleFrameObject*>(obj);
        if (puzzleFrame) {
            OutputDebugString(L"[Scene] PuzzleFrameObject found, getting cell status\n");
            puzzleFrame->GetPuzzleCellStatus(mPuzzleStatus);
            foundPuzzleFrame = true;
            break;
        }
    }
    
    if (!foundPuzzleFrame) {
        OutputDebugString(L"[Scene] Warning: PuzzleFrameObject not found in UpdatePuzzleStatusFromCells\n");
    }
}

void Scene::SyncPuzzleStatus() {
    if (m_parent && m_parent->IsNetworkEnabled()) {
        UpdatePuzzleStatusFromCells();
        m_parent->GetNetworkManager().SendPuzzleUpdate(mPuzzleStatus);
    }
}

void Scene::DeleteCurrentObjects()
{
    OutputDebugString(L"[Scene] DeleteCurrentObjects: Starting cleanup\n");
    
    try {
        // 스테이지 전환 시작 - 다른 플레이어 생성 차단
        OutputDebugString(L"[Scene] DeleteCurrentObjects: Starting stage transition\n");
        if (m_parent) {
            OtherPlayerManager::GetInstance()->SetStageTransitioning(true);
            OtherPlayerManager::GetInstance()->ClearAllPlayers();
            OutputDebugString(L"[Scene] DeleteCurrentObjects: All other players cleared\n");
        }
        
        for (Object* obj : m_objects) {
            if (obj != nullptr) {
                delete obj;
            }
        }
        m_objects.clear();
        OutputDebugString(L"[Scene] DeleteCurrentObjects: All objects cleared\n");
    }
    catch (const std::exception& e) {
        OutputDebugString(L"[Scene] DeleteCurrentObjects: Exception occurred during cleanup\n");
    }
    catch (...) {
        OutputDebugString(L"[Scene] DeleteCurrentObjects: Unknown exception occurred during cleanup\n");
    }
}

void Scene::BuildRootSignature(ID3D12Device* device)
{
    // Create a root signature consisting of a descriptor table with a single CBV.
    D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData{};

    // This is the highest version the sample supports. If CheckFeatureSupport succeeds, the HighestVersion returned will not be greater than this.
    featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;

    if (FAILED(device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData))))
    {
        featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
    }

    CD3DX12_DESCRIPTOR_RANGE1 ranges[3] = {};
    ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0, 0);
    ranges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0);
    ranges[2].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1, 0);

    CD3DX12_ROOT_PARAMETER1 rootParameters[4] = {};
    rootParameters[0].InitAsDescriptorTable(1, &ranges[0], D3D12_SHADER_VISIBILITY_VERTEX);
    rootParameters[1].InitAsDescriptorTable(1, &ranges[1], D3D12_SHADER_VISIBILITY_PIXEL);
    rootParameters[2].InitAsConstantBufferView(1);
    rootParameters[3].InitAsDescriptorTable(1, &ranges[2], D3D12_SHADER_VISIBILITY_PIXEL);

    std::array<D3D12_STATIC_SAMPLER_DESC, 2> samplerDesc = {};
    D3D12_STATIC_SAMPLER_DESC* descPtr = nullptr;

    descPtr = &samplerDesc[0];
    descPtr->Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    descPtr->AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    descPtr->AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    descPtr->AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    descPtr->MipLODBias = 0;
    descPtr->MaxAnisotropy = 0; // filter  type  anisotropy ϶
    descPtr->ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    descPtr->BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
    descPtr->MinLOD = 0.0f;
    descPtr->MaxLOD = D3D12_FLOAT32_MAX;
    descPtr->ShaderRegister = 0;
    descPtr->RegisterSpace = 0;
    descPtr->ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    descPtr = &samplerDesc[1];
    descPtr->Filter = D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
    descPtr->AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
    descPtr->AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
    descPtr->AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
    descPtr->MipLODBias = 0;
    descPtr->MaxAnisotropy = 0; // filter  type  anisotropy ϶
    descPtr->ComparisonFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
    descPtr->BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE;
    descPtr->MinLOD = 0.0f;
    descPtr->MaxLOD = 0.0f;
    descPtr->ShaderRegister = 1;
    descPtr->RegisterSpace = 0;
    descPtr->ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    D3D12_ROOT_SIGNATURE_FLAGS flags = 
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc;
    rootSignatureDesc.Init_1_1(_countof(rootParameters), rootParameters, samplerDesc.size(), samplerDesc.data(), flags);

    ComPtr<ID3DBlob> signature;
    ComPtr<ID3DBlob> error;
    ThrowIfFailed(D3DX12SerializeVersionedRootSignature(&rootSignatureDesc, featureData.HighestVersion, &signature, &error));
    ThrowIfFailed(device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature)));
}

void Scene::BuildPSO(ID3D12Device* device)
{
    // Describe and create the graphics pipeline state object (PSO).
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.InputLayout = { m_inputElement.data(), static_cast<UINT>(m_inputElement.size())};
    psoDesc.pRootSignature = m_rootSignature.Get();
    auto vsIt = m_shaders.find("VS_Opaque");
    auto psIt = m_shaders.find("PS_Opaque");
    if (vsIt != m_shaders.end() && psIt != m_shaders.end()) {
        psoDesc.VS = CD3DX12_SHADER_BYTECODE(vsIt->second.Get());
        psoDesc.PS = CD3DX12_SHADER_BYTECODE(psIt->second.Get());
    } else {
        OutputDebugString(L"[Scene] Required shaders not found for PSO_Opaque\n");
        return;
    }
    psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    psoDesc.SampleDesc.Count = 1;
    psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
    ThrowIfFailed(device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(m_PSOs["PSO_Opaque"].GetAddressOf())));

    psoDesc.RasterizerState.DepthBias = 10000;
    psoDesc.RasterizerState.DepthBiasClamp = 0.0f;
    psoDesc.RasterizerState.SlopeScaledDepthBias = 1.2f;
    auto vsShadowIt = m_shaders.find("VS_Shadow");
    auto psShadowIt = m_shaders.find("PS_Shadow");
    if (vsShadowIt != m_shaders.end() && psShadowIt != m_shaders.end()) {
        psoDesc.VS = CD3DX12_SHADER_BYTECODE(vsShadowIt->second.Get());
        psoDesc.PS = CD3DX12_SHADER_BYTECODE(psShadowIt->second.Get());
    } else {
        OutputDebugString(L"[Scene] Required shaders not found for PSO_Shadow\n");
        return;
    }
    psoDesc.NumRenderTargets = 0;
    psoDesc.RTVFormats[0] = DXGI_FORMAT_UNKNOWN;
    ThrowIfFailed(device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(m_PSOs["PSO_Shadow"].GetAddressOf())));
}

void Scene::BuildVertexBuffer(ID3D12Device* device, ID3D12GraphicsCommandList* commandList)
{
    const UINT vertexBufferSize = m_resourceManager->GetVertexBuffer().size() * sizeof(Vertex);
    // Create the vertex buffer.
    ThrowIfFailed(device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize),
        D3D12_RESOURCE_STATE_COMMON,
        nullptr,
        IID_PPV_ARGS(m_vertexBuffer_default.GetAddressOf())));

    ThrowIfFailed(device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize),
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(m_vertexBuffer_upload.GetAddressOf())));

    D3D12_SUBRESOURCE_DATA subResourceData{};
    subResourceData.pData = m_resourceManager->GetVertexBuffer().data();
    subResourceData.RowPitch = vertexBufferSize;
    subResourceData.SlicePitch = subResourceData.RowPitch;

    commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_vertexBuffer_default.Get(),
        D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST));
    UpdateSubresources(commandList, m_vertexBuffer_default.Get(), m_vertexBuffer_upload.Get(), 0, 0, 1, &subResourceData);
    commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_vertexBuffer_default.Get(),
        D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ));
}

void Scene::BuildIndexBuffer(ID3D12Device* device, ID3D12GraphicsCommandList* commandList)
{
    // Create the index buffer.
    const UINT indexBufferSize = m_resourceManager->GetIndexBuffer().size() * sizeof(uint32_t);

    ThrowIfFailed(device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer(indexBufferSize),
        D3D12_RESOURCE_STATE_COMMON,
        nullptr,
        IID_PPV_ARGS(m_indexBuffer_default.GetAddressOf())));

    ThrowIfFailed(device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer(indexBufferSize),
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(m_indexBuffer_upload.GetAddressOf())));

    D3D12_SUBRESOURCE_DATA subResourceData = {};
    subResourceData.pData = m_resourceManager->GetIndexBuffer().data();
    subResourceData.RowPitch = indexBufferSize;
    subResourceData.SlicePitch = subResourceData.RowPitch;

    commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_indexBuffer_default.Get(),
        D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST));
    UpdateSubresources(commandList, m_indexBuffer_default.Get(), m_indexBuffer_upload.Get(), 0, 0, 1, &subResourceData);
    commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_indexBuffer_default.Get(),
        D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ));
}

void Scene::BuildVertexBufferView()
{
    // Initialize the vertex buffer view.
    m_vertexBufferView.BufferLocation = m_vertexBuffer_default->GetGPUVirtualAddress();
    m_vertexBufferView.StrideInBytes = sizeof(Vertex);
    m_vertexBufferView.SizeInBytes = m_resourceManager->GetVertexBuffer().size() * sizeof(Vertex);
}

void Scene::BuildIndexBufferView()
{
    m_indexBufferView.BufferLocation = m_indexBuffer_default->GetGPUVirtualAddress();
    m_indexBufferView.Format = DXGI_FORMAT_R32_UINT;
    m_indexBufferView.SizeInBytes = m_resourceManager->GetIndexBuffer().size() * sizeof(uint32_t);
}

void Scene::BuildDescriptorHeap(ID3D12Device* device)
{
    D3D12_DESCRIPTOR_HEAP_DESC HeapDesc = {};
    HeapDesc.NumDescriptors = static_cast<UINT>(1 + m_DDSFileName.size() + 2); //  1 cbv  1 shdowmap
    HeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    HeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    ThrowIfFailed(device->CreateDescriptorHeap(&HeapDesc, IID_PPV_ARGS(m_descriptorHeap.GetAddressOf())));

    m_cbvsrvuavDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
}

void Scene::BuildConstantBuffer(ID3D12Device* device)
{
    const UINT constantBufferSize = CalcConstantBufferByteSize(sizeof(CommonCB));    // CB size is required to be 256-byte aligned.

    ThrowIfFailed(device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer(constantBufferSize),
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&m_constantBuffer)));

    // Map and initialize the constant buffer. We don't unmap this until the
    // app closes. Keeping things mapped for the lifetime of the resource is okay.
    CD3DX12_RANGE readRange(0, 0);        // We do not intend to read from this resource on the CPU.
    ThrowIfFailed(m_constantBuffer->Map(0, &readRange, &m_mappedData));
}

void Scene::BuildConstantBufferView(ID3D12Device* device)
{
    // Describe and create a constant buffer view.
    D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc{};
    cbvDesc.BufferLocation = m_constantBuffer->GetGPUVirtualAddress();
    cbvDesc.SizeInBytes = CalcConstantBufferByteSize(sizeof(CommonCB));

    CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptor(m_descriptorHeap->GetCPUDescriptorHandleForHeapStart());
    hDescriptor.Offset(0, m_cbvsrvuavDescriptorSize);

    device->CreateConstantBufferView(&cbvDesc, hDescriptor);

}

void Scene::BuildTextureBuffer(ID3D12Device* device, ID3D12GraphicsCommandList* commandList)
{
    // Create the texture.
    for(auto& fileName: m_DDSFileName)
    {
        ComPtr<ID3D12Resource> defaultBuffer;
        ComPtr<ID3D12Resource> uploadBuffer;

        // DDSTexture  
        unique_ptr<uint8_t[]> ddsData;
        vector<D3D12_SUBRESOURCE_DATA> subresources;
        ThrowIfFailed(LoadDDSTextureFromFile(device, fileName.c_str(), defaultBuffer.GetAddressOf(), ddsData, subresources));

        //// DirectTex  
        //ScratchImage image;
        //TexMetadata metadata;

        //ThrowIfFailed(LoadFromDDSFile(L"./Textures/grass.dds", DDS_FLAGS_NONE, &metadata, image));
        ////metadata = image.GetMetadata(); //  ڵ带 ڰ ڵ 3  nullptrص ȴ.

        //ThrowIfFailed(CreateTexture(device, metadata, m_textureBuffer_default.GetAddressOf()));
        //ThrowIfFailed(PrepareUpload(device, image.GetImages(), image.GetImageCount(), metadata, subresources));

        const UINT64 uploadBufferSize = GetRequiredIntermediateSize(defaultBuffer.Get(), 0, subresources.size());
        
        OutputDebugStringA(string{ "current texture subresource size = " + to_string(subresources.size()) + "\n"}.c_str());
        OutputDebugStringA(string{ "current texture mip level = " + to_string(defaultBuffer->GetDesc().MipLevels) + "\n"}.c_str());
        OutputDebugStringA(string{ "current texture format = " + to_string(defaultBuffer->GetDesc().Format) + "\n"}.c_str());

        // Create the GPU upload buffer.
        ThrowIfFailed(device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
            D3D12_HEAP_FLAG_NONE,
            &CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize),
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(uploadBuffer.GetAddressOf())));
        
        UpdateSubresources(commandList, defaultBuffer.Get(), uploadBuffer.Get(), 0, 0, static_cast<UINT>(subresources.size()), subresources.data());
        commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(defaultBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));

        m_textureBuffer_defaults.push_back(move(defaultBuffer));
        m_textureBuffer_uploads.push_back(move(uploadBuffer));
    }
}

void Scene::BuildTextureBufferView(ID3D12Device* device)
{
    // Describe and create a SRV for the texture.
    for (int i = 0; i < m_DDSFileName.size(); ++i)
    {
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Format = m_textureBuffer_defaults[i]->GetDesc().Format;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = m_textureBuffer_defaults[i]->GetDesc().MipLevels;

        CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptor(m_descriptorHeap->GetCPUDescriptorHandleForHeapStart());
        hDescriptor.Offset(1 + i, m_cbvsrvuavDescriptorSize); // 1 + i  1  constant buffer view   .  

        device->CreateShaderResourceView(m_textureBuffer_defaults[i].Get(), &srvDesc, hDescriptor);
    }
}

UINT Scene::CalcConstantBufferByteSize(UINT byteSize)
{
    return (byteSize + 255) & ~255;
}

Framework* Scene::GetFramework()
{
    return m_parent;
}

UINT Scene::GetNumOfTexture()
{
    return static_cast<UINT>(m_DDSFileName.size());
}

void Scene::AddObj(Object* object)
{
    if (m_object_queue_index > MAX_QUEUE - 1) throw; // 
    m_object_queue[m_object_queue_index++] = object;
}

void Scene::RemoveObj(Object* object)
{
    if (!object) return;
    
    // 오브젝트를 무효화하고 나중에 제거되도록 표시
    object->SetValid(false);
    
    // m_objects 컬렉션에서도 제거
    auto it = std::find(m_objects.begin(), m_objects.end(), object);
    if (it != m_objects.end()) {
        m_objects.erase(it);
    }
    
    OutputDebugString(L"[Scene] Object marked for removal\n");
}

void Scene::BuildProjMatrix()
{
    XMMATRIX proj = XMMatrixPerspectiveFovLH(XMConvertToRadians(60.0f), m_viewport.Width / m_viewport.Height, 0.1f, 1500.0f);
    XMStoreFloat4x4(&m_proj, proj);
}

std::unordered_map<std::string, ComPtr<ID3D12PipelineState>>& Scene::GetPSOs()
{
    return m_PSOs;
}

void Scene::ProcessInput()
{
    BYTE* keyState = m_parent->GetKeyState();
    if ((keyState[VK_F1] & 0x88) == 0x80) { m_stage_queue = L"Base"; }
    if ((keyState[VK_F2] & 0x88) == 0x80) { m_stage_queue = L"God"; }
    if ((keyState[VK_F3] & 0x88) == 0x80) { m_stage_queue = L"Title"; }
    if ((keyState[VK_F10] & 0x88) == 0x80) { mLeatherCount = 5; }

    if ((keyState[0x57] & 0x88) == 0x80) { mInputDir.z += 1.0f; } // w down
    if ((keyState[0x53] & 0x88) == 0x80) { mInputDir.z -= 1.0f; } // s down
    if ((keyState[0x41] & 0x88) == 0x80) { mInputDir.x -= 1.0f; } // a down
    if ((keyState[0x44] & 0x88) == 0x80) { mInputDir.x += 1.0f; } // d down

    if ((keyState[0x57] & 0x88) == 0x08) { mInputDir.z -= 1.0f; } // w up
    if ((keyState[0x53] & 0x88) == 0x08) { mInputDir.z += 1.0f; } // s up
    if ((keyState[0x41] & 0x88) == 0x08) { mInputDir.x += 1.0f; } // a up
    if ((keyState[0x44] & 0x88) == 0x08) { mInputDir.x -= 1.0f; } // d up
}

void Scene::LoadMeshAnimationTexture()
{
    m_resourceManager = make_unique<ResourceManager>();
    m_resourceManager->CreatePlane("Plane", 1000, 30);
    m_resourceManager->CreatePlane("HalfPlane", 500, 5);
    m_resourceManager->CreatePlane("Quad", 1, 1);
    m_resourceManager->CreateTerrain("HeightMap.raw", 50, 5, 50);
    m_resourceManager->LoadFbx("1P(boy-idle).fbx", false, true);
    m_resourceManager->LoadFbx("boy_walk_fix.fbx", true, true);
    m_resourceManager->LoadFbx("boy_run_fix.fbx", true, true);
    m_resourceManager->LoadFbx("boy_pickup_fix.fbx", true, true);
    m_resourceManager->LoadFbx("boy_attack(45).fbx", true, true);
    m_resourceManager->LoadFbx("boy_hit.fbx", true, true);
    m_resourceManager->LoadFbx("boy_dying_fix.fbx", true, true);
    m_resourceManager->LoadFbx("boy_throw.fbx", true, true);

    m_resourceManager->LoadFbx("god_idle.fbx", false, true);
    m_resourceManager->LoadFbx("sister_idle_fix.fbx", false, true);

    m_resourceManager->LoadFbx("0113_tiger.fbx", false, true);
    m_resourceManager->LoadFbx("0722_tiger_idle2.fbx", true, true);
    m_resourceManager->LoadFbx("0113_tiger_walk.fbx", true, true);
    m_resourceManager->LoadFbx("0722_tiger_run.fbx", true, true);
    m_resourceManager->LoadFbx("0208_tiger_attack.fbx", true, true);
    m_resourceManager->LoadFbx("0208_tiger_hit.fbx", true, true);
    m_resourceManager->LoadFbx("0208_tiger_dying.fbx", true, true);

    m_resourceManager->LoadFbx("long_tree.fbx", false, true);
    m_resourceManager->LoadFbx("normal_tree.fbx", false, true);

    m_resourceManager->LoadFbx("broken_house.fbx", false, true);
    m_resourceManager->LoadFbx("broken_house2.fbx", false, true);
    m_resourceManager->LoadFbx("background_house.fbx", false, true);

    m_resourceManager->LoadFbx("table.fbx", false, true);
    m_resourceManager->LoadFbx("well.fbx", false, true);
    m_resourceManager->LoadFbx("fence.fbx", false, true);

    m_resourceManager->LoadFbx("cloud1.fbx", false, true);
    m_resourceManager->LoadFbx("cloud2.fbx", false, true);
    m_resourceManager->LoadFbx("cloud3.fbx", false, true);
    m_resourceManager->LoadFbx("cloud4.fbx", false, true);

    m_resourceManager->LoadFbx("tiger_leather.fbx", false, true);

    m_resourceManager->LoadFbx("axe.fbx", false, true);
    m_resourceManager->LoadFbx("wood.fbx", false, true);
    m_resourceManager->LoadFbx("ricecake.fbx", false, true);

    m_resourceManager->LoadFbx("grass_low.fbx", false, true);

    m_resourceManager->LoadFbx("god.fbx", false, true);  // god 모델 로드 추가(테스트)




    int i = 0;
    m_DDSFileName.push_back(L"./Textures/boy.dds");
    m_texture_name_to_index.insert({ L"boy", i++ });
    m_DDSFileName.push_back(L"./Textures/grass.dds");
    m_texture_name_to_index.insert({ L"grass", i++ });

    m_DDSFileName.push_back(L"./Textures/god.dds");
    m_texture_name_to_index.insert({ L"god", i++ });
    m_DDSFileName.push_back(L"./Textures/sister.dds");
    m_texture_name_to_index.insert({ L"sister", i++ });
    m_DDSFileName.push_back(L"./Textures/PP_Color_Palette.dds");
    m_texture_name_to_index.insert({ L"PP_Color_Palette", i++ });
    m_DDSFileName.push_back(L"./Textures/tigercolor.dds");
    m_texture_name_to_index.insert({ L"tigercolor", i++ });

    m_DDSFileName.push_back(L"./Textures/normaltree_texture.dds");
    m_texture_name_to_index.insert({ L"normalTree", i++ });
    m_DDSFileName.push_back(L"./Textures/longtree_texture.dds");
    m_texture_name_to_index.insert({ L"longTree", i++ });

    m_DDSFileName.push_back(L"./Textures/broken_house.dds");
    m_texture_name_to_index.insert({ L"broken_house", i++ });
    m_DDSFileName.push_back(L"./Textures/broken_house2.dds");
    m_texture_name_to_index.insert({ L"broken_house2", i++ });

    m_DDSFileName.push_back(L"./Textures/Brown.dds");
    m_texture_name_to_index.insert({ L"Brown", i++ });

    m_DDSFileName.push_back(L"./Textures/Gray.dds");
    m_texture_name_to_index.insert({ L"Gray", i++ });
    m_DDSFileName.push_back(L"./Textures/LightGray.dds");
    m_texture_name_to_index.insert({ L"LightGray", i++ });
    m_DDSFileName.push_back(L"./Textures/Green.dds");
    m_texture_name_to_index.insert({ L"Green", i++ });
    m_DDSFileName.push_back(L"./Textures/White.dds");
    m_texture_name_to_index.insert({ L"White", i++ });
    m_DDSFileName.push_back(L"./Textures/Black.dds");
    m_texture_name_to_index.insert({ L"Black", i++ });
    m_DDSFileName.push_back(L"./Textures/Red.dds");
    m_texture_name_to_index.insert({ L"Red", i++ });
    m_DDSFileName.push_back(L"./Textures/Yellow.dds");
    m_texture_name_to_index.insert({ L"Yellow", i++ });

    m_DDSFileName.push_back(L"./Textures/tiger.dds");
    m_texture_name_to_index.insert({ L"tigerLeather", i++ });
    m_DDSFileName.push_back(L"./Textures/axe.dds");
    m_texture_name_to_index.insert({ L"axe", i++ });
    m_DDSFileName.push_back(L"./Textures/wood.dds");
    m_texture_name_to_index.insert({ L"wood", i++ });
    m_DDSFileName.push_back(L"./Textures/RiceCake.dds");
    m_texture_name_to_index.insert({ L"RiceCake", i++ });
    m_DDSFileName.push_back(L"./Textures/RiceCakePink.dds");
    m_texture_name_to_index.insert({ L"RiceCakePink", i++ });
    m_DDSFileName.push_back(L"./Textures/Title.dds");
    m_texture_name_to_index.insert({ L"Title", i++ });

    m_DDSFileName.push_back(L"./Textures/Quest.dds");
    m_texture_name_to_index.insert({ L"Quest", i++ });
    m_DDSFileName.push_back(L"./Textures/End.dds");
    m_texture_name_to_index.insert({ L"End", i++ });
    m_DDSFileName.push_back(L"./Textures/GoToGod.dds");
    m_texture_name_to_index.insert({ L"GoToGod", i++ });

    m_DDSFileName.push_back(L"./Textures/BoyIcon.dds");
    m_texture_name_to_index.insert({ L"BoyIcon", i++ });
    m_DDSFileName.push_back(L"./Textures/Life3.dds");
    m_texture_name_to_index.insert({ L"Life3", i++ });
    m_DDSFileName.push_back(L"./Textures/Life2.dds");
    m_texture_name_to_index.insert({ L"Life2", i++ });
    m_DDSFileName.push_back(L"./Textures/Life1.dds");
    m_texture_name_to_index.insert({ L"Life1", i++ });
    m_DDSFileName.push_back(L"./Textures/Life0.dds");
    m_texture_name_to_index.insert({ L"Life0", i++ });

    m_DDSFileName.push_back(L"./Textures/RiceCake0.dds");
    m_texture_name_to_index.insert({ L"RiceCake0", i++ });
    m_DDSFileName.push_back(L"./Textures/RiceCake1.dds");
    m_texture_name_to_index.insert({ L"RiceCake1", i++ });
    m_DDSFileName.push_back(L"./Textures/RiceCake2.dds");
    m_texture_name_to_index.insert({ L"RiceCake2", i++ });
    m_DDSFileName.push_back(L"./Textures/RiceCake3.dds");
    m_texture_name_to_index.insert({ L"RiceCake3", i++ });
    m_DDSFileName.push_back(L"./Textures/RiceCake4.dds");
    m_texture_name_to_index.insert({ L"RiceCake4", i++ });

    m_DDSFileName.push_back(L"./Textures/TigerLeather0.dds");
    m_texture_name_to_index.insert({ L"TigerLeather0", i++ });
    m_DDSFileName.push_back(L"./Textures/TigerLeather1.dds");
    m_texture_name_to_index.insert({ L"TigerLeather1", i++ });
    m_DDSFileName.push_back(L"./Textures/TigerLeather2.dds");
    m_texture_name_to_index.insert({ L"TigerLeather2", i++ });
    m_DDSFileName.push_back(L"./Textures/TigerLeather3.dds");
    m_texture_name_to_index.insert({ L"TigerLeather3", i++ });
    m_DDSFileName.push_back(L"./Textures/TigerLeather4.dds");
    m_texture_name_to_index.insert({ L"TigerLeather4", i++ });
    m_DDSFileName.push_back(L"./Textures/TigerLeather5.dds");
    m_texture_name_to_index.insert({ L"TigerLeather5", i++ });

    m_DDSFileName.push_back(L"./Textures/PuzzleFrame.dds");
    m_texture_name_to_index.insert({ L"PuzzleFrame", i++ });
    m_DDSFileName.push_back(L"./Textures/PuzzleFrameComplete.dds");
    m_texture_name_to_index.insert({ L"PuzzleFrameComplete", i++ });
    m_DDSFileName.push_back(L"./Textures/PuzzleO.dds");
    m_texture_name_to_index.insert({ L"PuzzleO", i++ });
    m_DDSFileName.push_back(L"./Textures/PuzzleX.dds");
    m_texture_name_to_index.insert({ L"PuzzleX", i++ });
}

// Update frame-based values.
void Scene::OnUpdate(GameTimer& gTimer)
{
    ProcessInput();
    ProcessStageQueue();
    CompactObjects();
    ProcessObjectQueue();
    for (Object* obj : m_objects)
    {
        if (!obj->GetValid()) continue;
        obj->OnUpdate(gTimer);
    }

    m_shadow->UpdateShadow();

    // 네트워크 업데이트 추가
    if (m_parent && m_parent->IsNetworkEnabled()) {
        NetworkManager& networkManager = m_parent->GetNetworkManager();
        networkManager.Update(gTimer, this);
    }

    // 
    memcpy(static_cast<UINT8*>(m_mappedData) + sizeof(XMMATRIX), &XMMatrixTranspose(XMLoadFloat4x4(&m_proj)), sizeof(XMMATRIX));
}

// Render the scene.
void Scene::OnRender(ID3D12Device* device, ID3D12GraphicsCommandList* commandList, ePass pass)
{
    switch (pass)
    {
    case ePass::Shadow:
    {
        commandList->SetGraphicsRootSignature(m_rootSignature.Get());
        ID3D12DescriptorHeap* ppHeaps[] = { m_descriptorHeap.Get() };
        commandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
        commandList->SetGraphicsRootDescriptorTable(3, m_shadow->GetGpuDescHandleForNullShadow());
        CD3DX12_GPU_DESCRIPTOR_HANDLE hDescriptor(m_descriptorHeap->GetGPUDescriptorHandleForHeapStart());
        commandList->SetGraphicsRootDescriptorTable(0, hDescriptor);
        commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        commandList->IASetVertexBuffers(0, 1, &m_vertexBufferView);
        commandList->IASetIndexBuffer(&m_indexBufferView);
        m_shadow->DrawShadowMap();
        break;
    }
    case ePass::Default:
    {
        commandList->RSSetViewports(1, &m_viewport);
        commandList->RSSetScissorRects(1, &m_scissorRect);
        auto psoIt = m_PSOs.find("PSO_Opaque");
        if (psoIt != m_PSOs.end()) {
            commandList->SetPipelineState(psoIt->second.Get());
        } else {
            OutputDebugString(L"[Scene] PSO_Opaque not found\n");
            return;
        }
        commandList->SetGraphicsRootDescriptorTable(3, m_shadow->GetGpuDescHandleForShadow());
        RenderObjects(device, commandList);
        break;
    }
    default:
        break;
    }
}

void Scene::OnResize(UINT width, UINT height)
{
    m_viewport = { CD3DX12_VIEWPORT(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f, 1.0f) };
    m_scissorRect = { CD3DX12_RECT(0, 0, static_cast<LONG>(width), static_cast<LONG>(height)) };
    BuildProjMatrix();
}

void Scene::OnDestroy()
{
    m_constantBuffer->Unmap(0, nullptr);
}

void Scene::OnProcessCollision()
{
    size_t objCount = m_objects.size();
    if (objCount < 2) return; // 최소 2개 이상의 객체가 있어야 충돌 검사 가능
    
    for (size_t i = 0; i < objCount - 1; ++i)
    {
        // 배열 범위 검사
        if (i >= m_objects.size()) break;
        
        if (!m_objects[i] || !m_objects[i]->GetValid()) continue;
        Object* obj = m_objects[i];
        Collider* collider = obj->GetComponent<Collider>();
        if (!collider) continue;
        auto& OBB = collider->GetOBB();
        for (size_t j = i + 1; j < objCount; ++j)
        {
            // 배열 범위 검사
            if (j >= m_objects.size()) break;
            
            if (!m_objects[j] || !m_objects[j]->GetValid()) continue;
            Object* otherObj = m_objects[j];
            Collider* otherCollider = otherObj->GetComponent<Collider>();
            if (!otherCollider) continue;
            auto& otherOBB = otherCollider->GetOBB();
            
            // TigerAttackObject와 PlayerObject 조합에 대한 상세 디버그
            TigerAttackObject* tigerAttack = dynamic_cast<TigerAttackObject*>(obj);
            PlayerObject* player = dynamic_cast<PlayerObject*>(otherObj);
            
            if (tigerAttack && player) {
                Transform* attackTransform = tigerAttack->GetComponent<Transform>();
                Transform* playerTransform = player->GetComponent<Transform>();
                
                XMFLOAT3 attackPos, playerPos;
                XMStoreFloat3(&attackPos, attackTransform->GetPosition());
                XMStoreFloat3(&playerPos, playerTransform->GetPosition());
                
                // OBB 정보 출력
                XMFLOAT3 attackCenter, playerCenter;
                XMStoreFloat3(&attackCenter, XMLoadFloat3(&OBB.Center));
                XMStoreFloat3(&playerCenter, XMLoadFloat3(&otherOBB.Center));
                
                wchar_t debugMsg[512];
                swprintf_s(debugMsg, L"[Scene] Checking TigerAttack vs Player collision:\n"
                                   L"  Attack pos: (%.1f, %.1f, %.1f), OBB center: (%.1f, %.1f, %.1f), extents: (%.1f, %.1f, %.1f)\n"
                                   L"  Player pos: (%.1f, %.1f, %.1f), OBB center: (%.1f, %.1f, %.1f), extents: (%.1f, %.1f, %.1f)\n",
                                   attackPos.x, attackPos.y, attackPos.z,
                                   attackCenter.x, attackCenter.y, attackCenter.z,
                                   OBB.Extents.x, OBB.Extents.y, OBB.Extents.z,
                                   playerPos.x, playerPos.y, playerPos.z,
                                   playerCenter.x, playerCenter.y, playerCenter.z,
                                   otherOBB.Extents.x, otherOBB.Extents.y, otherOBB.Extents.z);
                OutputDebugString(debugMsg);
            }
            
            if (!OBB.Intersects(otherOBB)) {
                // TigerAttackObject와 PlayerObject 조합에서 충돌이 감지되지 않은 경우
                if (tigerAttack && player) {
                    OutputDebugString(L"[Scene] TigerAttack and Player OBBs do NOT intersect\n");
                }
                continue;
            }
            
            // TigerAttackObject와 PlayerObject 조합에서 충돌이 감지된 경우
            if (tigerAttack && player) {
                OutputDebugString(L"[Scene] TigerAttack and Player OBBs DO intersect - processing collision!\n");
            }
            
            auto [normal, penetration] = GetCollisionData(OBB, otherOBB);
            
            // TigerAttackObject와 PlayerObject 충돌 감지 로그
            if (tigerAttack && player) {
                OutputDebugString(L"[Scene] TigerAttackObject collision with Player detected!\n");
            }
            
            obj->OnProcessCollision(*otherObj, normal, penetration);
            otherObj->OnProcessCollision(*obj, -normal, penetration);
        }
    }
}

void Scene::LateUpdate(GameTimer& gTimer)
{
    for (Object* obj : m_objects)
    {
        if (!obj->GetValid()) continue;
        obj->LateUpdate(gTimer);
    }
}

ResourceManager& Scene::GetResourceManager()
{
    return *(m_resourceManager.get());
}

void* Scene::GetConstantBufferMappedData()
{
    // TODO:  մϴ.
    return m_mappedData;
}

ID3D12DescriptorHeap* Scene::GetDescriptorHeap()
{
    return m_descriptorHeap.Get();
}
