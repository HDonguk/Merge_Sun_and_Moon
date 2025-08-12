#pragma once
#include "stdafx.h"
#include "Object.h"
#include "ResourceManager.h"
#include <utility>
#include "Shadow.h"
#include "NetworkManager.h"
#define MAX_QUEUE 700
class GameTimer;
class Framework;

class Scene
{
public:
    ~Scene();
    Scene(Framework* parent, UINT width, UINT height);
    void OnInit(ID3D12Device* device, ID3D12GraphicsCommandList* commandList);
    void OnUpdate(GameTimer& gTimer);
    void OnProcessCollision();
    void LateUpdate(GameTimer& gTimer);
    void OnRender(ID3D12Device* device, ID3D12GraphicsCommandList* commandList, ePass pass);
    void OnResize(UINT width, UINT height);
    void OnDestroy();
    ResourceManager& GetResourceManager();
    void* GetConstantBufferMappedData();
    ID3D12DescriptorHeap* GetDescriptorHeap();
    UINT CalcConstantBufferByteSize(UINT byteSize);
    Framework* GetFramework();
    UINT GetNumOfTexture();
    void AddObj(Object* object);
    void RemoveObj(Object* object);  // 특정 오브젝트 제거 메서드 추가
    std::unordered_map<std::string, ComPtr<ID3D12PipelineState>>& GetPSOs();
    void RenderObjects(ID3D12Device* device, ID3D12GraphicsCommandList* commandList);
    char ClampToBounds(XMVECTOR& pos, XMVECTOR offset);
    std::tuple<float, float, float, float, float> GetBounds(float x, float z);
    int GetTextureIndex(wstring name);
    std::tuple<XMVECTOR, float> GetCollisionData(BoundingOrientedBox OBB1, BoundingOrientedBox OBB2);
    Object* GetObjFromId(uint32_t id);
    uint32_t AllocateId();
    void SetStage(wstring stage);
    void IncreaseLeatherCount();
    void ResetLeatherCount();
    bool HasEnoughLeather();
    float GetAspectRatio();
    int GetLeatherCount();
    bool IsTigerQuestAccepted();
    void SetTigerQuestState(bool state);
    XMVECTOR GetInputDir();
    int(*GetPuzzleStatus())[3];

    wstring GetCurrentStage() const { return m_current_stage; }
    const vector<Object*>& GetObjects() const { return m_objects; }

    template<typename T>
    T* GetObj()
    {
        T* temp = nullptr;
        for (Object* obj : m_objects) {
            if (!obj->GetValid()) continue;
            temp = dynamic_cast<T*>(obj);
            if (temp) break;
        }
        return temp;
    }

    // 로컬 플레이어(직접 조작하는 플레이어)를 반환하는 메서드 추가
    PlayerObject* GetLocalPlayer()
    {
        PlayerObject* localPlayer = nullptr;
        int playerCount = 0;
        
        for (Object* obj : m_objects) {
            if (!obj->GetValid()) continue;
            PlayerObject* player = dynamic_cast<PlayerObject*>(obj);
            if (player) {
                playerCount++;
                if (!player->IsNetworkPlayer()) {
                    localPlayer = player;  // 네트워크 플레이어가 아닌 것이 로컬 플레이어
                }
            }
        }
        
        // 안전장치: 로컬 플레이어가 여러 개 있거나 없는 경우 로그
        if (playerCount == 0) {
            OutputDebugString(L"[Scene] No PlayerObject found in scene\n");
            return nullptr;
        } else if (playerCount > 1 && localPlayer == nullptr) {
            OutputDebugString(L"[Scene] Multiple players found but no local player identified\n");
        }
        
        return localPlayer;
    }
private:
    void ProcessStageQueue();
    void CompactObjects();
    void ProcessObjectQueue();
    void DeleteCurrentObjects();
    void ProcessInput();
    void LoadMeshAnimationTexture();
    void BuildRootSignature(ID3D12Device* device);
    void BuildPSO(ID3D12Device* device);
    void BuildVertexBuffer(ID3D12Device* device, ID3D12GraphicsCommandList* commandList);
    void BuildIndexBuffer(ID3D12Device* device, ID3D12GraphicsCommandList* commandList);
    void BuildVertexBufferView();
    void BuildIndexBufferView();
    void BuildConstantBuffer(ID3D12Device* device);
    void BuildConstantBufferView(ID3D12Device* device);
    void BuildTextureBuffer(ID3D12Device* device, ID3D12GraphicsCommandList* commandList);
    void BuildTextureBufferView(ID3D12Device* device);
    void BuildDescriptorHeap(ID3D12Device* device);
    void BuildProjMatrix();
    void BuildTitleStage();
    void BuildBaseStage();
    void BuildHuntingStage();
    void BuildGodStage();
    void BuildEndStage();
    void BuildUI();
    void BuildShadow();
    void BuildShaders();
    void BuildInputElement();
    ComPtr<ID3DBlob> CompileShader(
        const std::wstring& fileName, const D3D_SHADER_MACRO* defines, const std::string& entryPoint, const std::string& target);

    Framework* m_parent = nullptr;
    wstring m_current_stage = L"";
    wstring m_stage_queue = L"God";
    vector<Object*> m_objects;
    uint32_t m_id_counter = 0;
    Object* m_object_queue[MAX_QUEUE]{};
    int m_object_queue_index = 0;
    int mLeatherCount = 0;
    //
    bool mTigerQuest = false;

    XMFLOAT3 mInputDir{};
    uint32_t mMainCameraId = -1;

    int mPuzzleStatus[3][3] = { {0,0,0},{1,1,1},{0,0,0} };

    unique_ptr<ResourceManager> m_resourceManager;
    //
    CD3DX12_VIEWPORT m_viewport;
    CD3DX12_RECT m_scissorRect;
    ComPtr<ID3D12RootSignature> m_rootSignature;
  
    std::unordered_map<std::string, ComPtr<ID3D12PipelineState>> m_PSOs;
    std::unordered_map<std::string, ComPtr<ID3DBlob>> m_shaders;
    //
    ComPtr<ID3D12DescriptorHeap> m_descriptorHeap;
    UINT m_cbvsrvuavDescriptorSize;
    //
    ComPtr<ID3D12Resource> m_vertexBuffer_default;
    ComPtr<ID3D12Resource> m_indexBuffer_default;
    ComPtr<ID3D12Resource> m_vertexBuffer_upload;
    ComPtr<ID3D12Resource> m_indexBuffer_upload;
    D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView;
    D3D12_INDEX_BUFFER_VIEW m_indexBufferView;
    //
    unordered_map<wstring, int> m_texture_name_to_index;
    vector<wstring> m_DDSFileName;
    vector<ComPtr<ID3D12Resource>> m_textureBuffer_defaults;
    vector<ComPtr<ID3D12Resource>> m_textureBuffer_uploads;
    //
    ComPtr<ID3D12Resource> m_constantBuffer;
    void* m_mappedData = nullptr;
    //
    XMFLOAT4X4 m_proj;
    //
    unique_ptr<Shadow> m_shadow = nullptr;

    std::vector<D3D12_INPUT_ELEMENT_DESC> m_inputElement;
};
