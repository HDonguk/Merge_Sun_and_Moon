#include "Scene.h"
#include "DXSampleHelper.h"
#include "GameTimer.h"
#include "string"
#include "info.h"
#include <array>
#include "Framework.h"

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
    LoadMeshAnimationTexture();
    BuildProjMatrix();
    BuildBaseStage();
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
}

void Scene::BuildHuntingStage()
{
    m_current_stage = L"Hunting";

    Object* objectPtr = nullptr;
    {
        objectPtr = new CameraObject(this, AllocateId());
        objectPtr->AddComponent(new Transform{ {0.f, 0.0f, 0.f} });
        AddObj(objectPtr);
    }

    {
        float scale = 0.1f;
        objectPtr = new PlayerObject(this, AllocateId());
        objectPtr->AddComponent(new Transform{ {300.f, 0.0f, 300.f} });
        objectPtr->AddComponent(new AdjustTransform{ {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {scale, scale, scale} });
        objectPtr->AddComponent(new Mesh{ "1P(boy-idle).fbx" });
        objectPtr->AddComponent(new Texture{ L"boy" , 1.0f, 0.4f });
        objectPtr->AddComponent(new Animation{ "1P(boy-idle).fbx" });
        objectPtr->AddComponent(new Gravity);
        objectPtr->AddComponent(new Collider{ {0.0f, 8.0f, 0.0f}, {2.0f, 8.0f, 2.0f} });
        AddObj(objectPtr);
    }

    {
        objectPtr = new TerrainObject(this, AllocateId());
        objectPtr->AddComponent(new Transform{ {0.f, 0.0f, 0.f} });
        objectPtr->AddComponent(new Mesh{ "HeightMap.raw" });
        objectPtr->AddComponent(new Texture{ L"grass" , 5.0f, 0.4f });
        AddObj(objectPtr);
    }

    // 나무
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

    // 나무
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
                objectPtr->AddComponent(new AdjustTransform{ {-0.8f * scale, 0.3f * scale, -2.5f * scale}, {0.0f, 0.0f, 0.0f}, {scale, scale, scale} });
                objectPtr->AddComponent(new Mesh{ "long_tree.fbx" });
                objectPtr->AddComponent(new Texture{ L"longTree", 1.0f, 0.4f });
                objectPtr->AddComponent(new Collider{ {0.0f, 20.0f, 0.0f}, {4.0f, 20.0f, 4.0f} });
                AddObj(objectPtr);
            }
        }
    }

    // 호랑이
    {
        float scale = 0.2f;
        float basePosX = 500.0f;
        float basePosZ = 500.0f;
        float offset = 100.0f;
        int repeat = 4;
        for (int i = 0; i < repeat; ++i) {
            for (int j = 0; j < repeat; ++j) {
                objectPtr = new TigerObject(this, AllocateId());
                objectPtr->AddComponent(new Transform{ {basePosX + offset * j, 0.0f, basePosZ + offset * i} });
                objectPtr->AddComponent(new AdjustTransform{ {0.0f, 0.0f, -40.0f * scale}, {0.0f, 180.0f, 0.0f}, {scale, scale, scale} });
                objectPtr->AddComponent(new Mesh{ "0113_tiger.fbx" });
                objectPtr->AddComponent(new Texture{ L"tigercolor", 1.0f, 0.4f });
                objectPtr->AddComponent(new Animation{ "0113_tiger_walk.fbx" });
                objectPtr->AddComponent(new Gravity);
                objectPtr->AddComponent(new Collider{ {0.0f, 6.0f, 0.0f}, {2.0f, 6.0f, 10.0f} });
                AddObj(objectPtr);
            }
        }
    }
    ProcessObjectQueue();
}

void Scene::BuildBaseStage()
{
    m_current_stage = L"Base";
    Object* objectPtr = nullptr;

    // 카메라
    {
        objectPtr = new CameraObject(this, AllocateId());
        objectPtr->AddComponent(new Transform{ {0.0f, 0.0f, 0.0f} });
        AddObj(objectPtr);
    }

    // 플레이어
    {
        float scale = 0.1f;
        objectPtr = new PlayerObject(this, AllocateId());
        objectPtr->AddComponent(new Transform{ {430.f, 0.0f, 150.f} });
        objectPtr->AddComponent(new AdjustTransform{ {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {scale, scale, scale} });
        objectPtr->AddComponent(new Mesh{ "1P(boy-idle).fbx" });
        objectPtr->AddComponent(new Texture{ L"boy" , 1.0f, 0.4f });
        objectPtr->AddComponent(new Animation{ "1P(boy-idle).fbx" });
        objectPtr->AddComponent(new Gravity);
        objectPtr->AddComponent(new Collider{ {0.0f, 80.0f * scale, 0.0f}, {30.0f * scale, 80.0f * scale, 30.0f * scale} });
        AddObj(objectPtr);

        scale = 2.0f;
        objectPtr = new TestObject(this, AllocateId(), objectPtr->GetId());
        objectPtr->AddComponent(new Transform{ { 0.0f, 5.0f, -3.0f} });
        objectPtr->AddComponent(new AdjustTransform{ {-0.8f * scale, 0.3f * scale, -2.5f * scale}, {0.0f, 0.0f, 0.0f}, {scale, scale, scale} });
        objectPtr->AddComponent(new Mesh{ "long_tree.fbx" });
        objectPtr->AddComponent(new Texture{ L"longTree", 1.0f, 0.4f });
        AddObj(objectPtr);
    }

    // 평면
    {
        objectPtr = new TestObject(this, AllocateId());
        objectPtr->AddComponent(new Transform{ {0.0f, 0.0f, 0.0f} });
        objectPtr->AddComponent(new Mesh{ "Plane" });
        objectPtr->AddComponent(new Texture{ L"grass", 1.0f, 0.4f });
        AddObj(objectPtr);
    }

    // 집
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

    // 부서진 집
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

    //부서진 집2
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

    // 테이블
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

    // 우물
    {
        float scale = 0.4f;
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

    // 나무1
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

    // 나무2
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

    // 호랑이 모형
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
 
    // 펜스
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

    //경계벽
    {
        float scale = 44.0f;
        int repeat = 5;
        for (int i = 0; i < repeat; ++i)
        {
            objectPtr = new TestObject(this, AllocateId());
            objectPtr->AddComponent(new Transform{ {100.0f + 200.0f * i, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f} });
            objectPtr->AddComponent(new AdjustTransform{ {0.0f * scale, 0.0f * scale, 0.0f * scale}, {0.0f, 0.0f, 0.0f}, {scale, scale, scale} });
            objectPtr->AddComponent(new Collider{ {0.0f, 0.0f, 0.0f}, {2.3f * scale, 1.5f * scale, 1.3f * scale} });
            objectPtr->AddComponent(new Mesh{ "cloud1.fbx" });
            objectPtr->AddComponent(new Texture{ L"stone", 1.0f, 0.4f });
            objectPtr->AddComponent(new Gravity);
            AddObj(objectPtr);
        }

        for (int i = 0; i < repeat; ++i)
        {
            objectPtr = new TestObject(this, AllocateId());
            objectPtr->AddComponent(new Transform{ {100.0f + 200.0f * i, 0.0f, 1000.0f}, {0.0f, 0.0f, 0.0f} });
            objectPtr->AddComponent(new AdjustTransform{ {0.0f * scale, 0.0f * scale, 0.0f * scale}, {0.0f, 0.0f, 0.0f}, {scale, scale, scale} });
            objectPtr->AddComponent(new Collider{ {0.0f, 0.0f, 0.0f}, {2.3f * scale, 1.5f * scale, 1.3f * scale} });
            objectPtr->AddComponent(new Mesh{ "cloud1.fbx" });
            objectPtr->AddComponent(new Texture{ L"stone", 1.0f, 0.4f });
            objectPtr->AddComponent(new Gravity);
            AddObj(objectPtr);
        }

        for (int i = 0; i < repeat; ++i)
        {
            objectPtr = new TestObject(this, AllocateId());
            objectPtr->AddComponent(new Transform{ {0.0f, 0.0f, 100.0f + 200.0f * i}, {0.0f, 90.0f, 0.0f} });
            objectPtr->AddComponent(new AdjustTransform{ {0.0f * scale, 0.0f * scale, 0.0f * scale}, {0.0f, 0.0f, 0.0f}, {scale, scale, scale} });
            objectPtr->AddComponent(new Collider{ {0.0f, 0.0f, 0.0f}, {2.3f * scale, 1.5f * scale, 1.3f * scale} });
            objectPtr->AddComponent(new Mesh{ "cloud1.fbx" });
            objectPtr->AddComponent(new Texture{ L"stone", 1.0f, 0.4f });
            objectPtr->AddComponent(new Gravity);
            AddObj(objectPtr);
        }

        for (int i = 0; i < repeat; ++i)
        {
            objectPtr = new TestObject(this, AllocateId());
            objectPtr->AddComponent(new Transform{ {1000.0f, 0.0f, 100.0f + 200.0f * i}, {0.0f, 90.0f, 0.0f} });
            objectPtr->AddComponent(new AdjustTransform{ {0.0f * scale, 0.0f * scale, 0.0f * scale}, {0.0f, 0.0f, 0.0f}, {scale, scale, scale} });
            objectPtr->AddComponent(new Collider{ {0.0f, 0.0f, 0.0f}, {2.3f * scale, 1.5f * scale, 1.3f * scale} });
            objectPtr->AddComponent(new Mesh{ "cloud1.fbx" });
            objectPtr->AddComponent(new Texture{ L"stone", 1.0f, 0.4f });
            objectPtr->AddComponent(new Gravity);
            AddObj(objectPtr);
        }
    }

    //{
    //    float scale = 3.0f;
    //    objectPtr = new QuadObject(this, AllocateId());
    //    objectPtr->AddComponent(new Transform{ {0.0f, 0.0f, 1.0f}, {0.0, 0.0f, 0.0f} });
    //    objectPtr->AddComponent(new AdjustTransform{ {-0.8f * scale, 0.3f * scale, -2.5f * scale}, {0.0f, 0.0f, 0.0f}, {scale, scale, scale} });
    //    objectPtr->AddComponent(new Mesh{ "long_tree.fbx" });
    //    objectPtr->AddComponent(new Texture{ L"longTree", 1.0f, 0.4f });
    //    AddObj(objectPtr);
    //}

    ProcessObjectQueue();
}

void Scene::BuildGodStage()
{
    m_current_stage = L"God";
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
    // 현제 stage에 맞는 바운드 반환하기
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
    return m_texture_name_to_index.at(name);
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
    m_objects.erase(std::remove_if(m_objects.begin(), m_objects.end(),[](Object* obj) { return !(obj->GetValid()); }), m_objects.end());
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
    m_stage_queue = stage;
}

void Scene::ProcessStageQueue()
{
    if (m_stage_queue == L"Base")
    {
        DeleteCurrentObjects();
        BuildBaseStage();
    }
    else if (m_stage_queue == L"Hunting")
    {
        DeleteCurrentObjects();
        BuildHuntingStage();

    }
    else if (m_stage_queue == L"God")
    {
        DeleteCurrentObjects();
        BuildGodStage();
    }
    m_stage_queue = L"";
}

void Scene::DeleteCurrentObjects()
{
    for (Object* obj : m_objects) {
        delete obj;
    }
    m_objects.clear();
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
    descPtr->MaxAnisotropy = 0; // filter 의 type 이 anisotropy 일때만 사용
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
    descPtr->MaxAnisotropy = 0; // filter 의 type 이 anisotropy 일때만 사용
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
    psoDesc.VS = CD3DX12_SHADER_BYTECODE(m_shaders.at("VS_Opaque").Get());
    psoDesc.PS = CD3DX12_SHADER_BYTECODE(m_shaders.at("PS_Opaque").Get());
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
    psoDesc.VS = CD3DX12_SHADER_BYTECODE(m_shaders.at("VS_Shadow").Get());
    psoDesc.PS = CD3DX12_SHADER_BYTECODE(m_shaders.at("PS_Shadow").Get());
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
    HeapDesc.NumDescriptors = static_cast<UINT>(1 + m_DDSFileName.size() + 2); // 앞의 1은 cbv 뒤에 1은 shdowmap용
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

        // DDSTexture 를 사용하는 방식
        unique_ptr<uint8_t[]> ddsData;
        vector<D3D12_SUBRESOURCE_DATA> subresources;
        ThrowIfFailed(LoadDDSTextureFromFile(device, fileName.c_str(), defaultBuffer.GetAddressOf(), ddsData, subresources));

        //// DirectTex를 사용하는 방식
        //ScratchImage image;
        //TexMetadata metadata;

        //ThrowIfFailed(LoadFromDDSFile(L"./Textures/grass.dds", DDS_FLAGS_NONE, &metadata, image));
        ////metadata = image.GetMetadata(); // 이코드를 사용하고 위 코드의 3번째 인자를 nullptr로 해도 된다.

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
        hDescriptor.Offset(1 + i, m_cbvsrvuavDescriptorSize); // 1 + i  에서 1의 의미는 이전에 만들어진 constant buffer view의 수 이다. 아직 한 개만 있음 

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
    if (m_object_queue_index > MAX_QUEUE - 1) throw; // 공간부족
    m_object_queue[m_object_queue_index++] = object;
}

void Scene::BuildProjMatrix()
{
    XMMATRIX proj = XMMatrixPerspectiveFovLH(XM_PI * 0.25f, m_viewport.Width / m_viewport.Height, 0.1f, 1000.0f);
    XMStoreFloat4x4(&m_proj, proj);
}

std::unordered_map<std::string, ComPtr<ID3D12PipelineState>>& Scene::GetPSOs()
{
    return m_PSOs;
}

void Scene::ProcessInput()
{
    BYTE* keyState = m_parent->GetKeyState();
    if ((keyState[VK_F1] & 0x88) == 0x80) {
        m_stage_queue = L"Base";
    }
}

void Scene::LoadMeshAnimationTexture()
{
    m_resourceManager = make_unique<ResourceManager>();
    m_resourceManager->CreatePlane("Plane", 1000, 10);
    m_resourceManager->CreatePlane("HalfPlane", 500, 5);
    m_resourceManager->CreateTerrain("HeightMap.raw", 50, 5, 50);
    m_resourceManager->LoadFbx("1P(boy-idle).fbx", false, true);
    m_resourceManager->LoadFbx("boy_walk_fix.fbx", true, true);
    m_resourceManager->LoadFbx("boy_run_fix.fbx", true, true);
    m_resourceManager->LoadFbx("boy_pickup_fix.fbx", true, true);
    m_resourceManager->LoadFbx("boy_attack(45).fbx", true, true);
    m_resourceManager->LoadFbx("boy_hit.fbx", true, true);
    m_resourceManager->LoadFbx("boy_dying_fix.fbx", true, true);

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





    int i = 0;
    m_DDSFileName.push_back(L"./Textures/boy.dds");
    m_texture_name_to_index.insert({ L"boy", i++ });
    m_DDSFileName.push_back(L"./Textures/bricks3.dds");
    m_texture_name_to_index.insert({ L"bricks3", i++ });
    m_DDSFileName.push_back(L"./Textures/checkboard.dds");
    m_texture_name_to_index.insert({ L"checkboard", i++ });
    m_DDSFileName.push_back(L"./Textures/grass.dds");
    m_texture_name_to_index.insert({ L"grass", i++ });
    m_DDSFileName.push_back(L"./Textures/tile.dds");
    m_texture_name_to_index.insert({ L"tile", i++ });
    m_DDSFileName.push_back(L"./Textures/god.dds");
    m_texture_name_to_index.insert({ L"god", i++ });
    m_DDSFileName.push_back(L"./Textures/sister.dds");
    m_texture_name_to_index.insert({ L"sister", i++ });
    m_DDSFileName.push_back(L"./Textures/water1.dds");
    m_texture_name_to_index.insert({ L"water1", i++ });
    m_DDSFileName.push_back(L"./Textures/PP_Color_Palette.dds");
    m_texture_name_to_index.insert({ L"PP_Color_Palette", i++ });
    m_DDSFileName.push_back(L"./Textures/tigercolor.dds");
    m_texture_name_to_index.insert({ L"tigercolor", i++ });
    m_DDSFileName.push_back(L"./Textures/stone.dds");
    m_texture_name_to_index.insert({ L"stone", i++ });
    m_DDSFileName.push_back(L"./Textures/normaltree_texture.dds");
    m_texture_name_to_index.insert({ L"normalTree", i++ });
    m_DDSFileName.push_back(L"./Textures/longtree_texture.dds");
    m_texture_name_to_index.insert({ L"longTree", i++ });
    m_DDSFileName.push_back(L"./Textures/rock(smooth).dds");
    m_texture_name_to_index.insert({ L"rock", i++ });
    m_DDSFileName.push_back(L"./Textures/broken_house.dds");
    m_texture_name_to_index.insert({ L"broken_house", i++ });
    m_DDSFileName.push_back(L"./Textures/broken_house2.dds");
    m_texture_name_to_index.insert({ L"broken_house2", i++ });
    m_DDSFileName.push_back(L"./Textures/Brown.dds");
    m_texture_name_to_index.insert({ L"Brown", i++ });
    m_DDSFileName.push_back(L"./Textures/tiger.dds");
    m_texture_name_to_index.insert({ L"tigerLeather", i++ });
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

    //투영행렬 쉐이더로 전달
    memcpy(static_cast<UINT8*>(m_mappedData) + sizeof(XMMATRIX), &XMMatrixTranspose(XMLoadFloat4x4(&m_proj)), sizeof(XMMATRIX)); // 처음 매개변수는 시작주소
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
        commandList->SetPipelineState(m_PSOs.at("PSO_Opaque").Get());
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
    for (int i = 0; i < objCount - 1; ++i)
    {
        if (!m_objects[i]->GetValid()) continue;
        Object* obj = m_objects[i];
        Collider* collider = obj->GetComponent<Collider>();
        if (!collider) continue;
        auto& OBB = collider->GetOBB();
        for (int j = i + 1; j < objCount; ++j)
        {
            if (!m_objects[i]->GetValid()) break;
            if (!m_objects[j]->GetValid()) continue;
            Object* otherObj = m_objects[j];
            Collider* otherCollider = otherObj->GetComponent<Collider>();
            if (!otherCollider) continue;
            auto& otherOBB = otherCollider->GetOBB();
            if (!OBB.Intersects(otherOBB)) continue;
            auto [normal, penetration] = GetCollisionData(OBB, otherOBB);
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
    // TODO: 여기에 return 문을 삽입합니다.
    return m_mappedData;
}

ID3D12DescriptorHeap* Scene::GetDescriptorHeap()
{
    return m_descriptorHeap.Get();
}
