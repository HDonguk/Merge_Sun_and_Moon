#pragma once
#include "stdafx.h"
#include "Object.h"
#include "ResourceManager.h"
#include <utility>
#include "Shadow.h"
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
    std::unordered_map<std::string, ComPtr<ID3D12PipelineState>>& GetPSOs();
    void RenderObjects(ID3D12Device* device, ID3D12GraphicsCommandList* commandList);
    char ClampToBounds(XMVECTOR& pos, XMVECTOR offset);
    std::tuple<float, float, float, float, float> GetBounds(float x, float z);
    int GetTextureIndex(wstring name);
    std::tuple<XMVECTOR, float> GetCollisionData(BoundingOrientedBox OBB1, BoundingOrientedBox OBB2);
    Object* GetObjFromId(uint32_t id);
    uint32_t AllocateId();
    void SetStage(wstring stage);

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
    void BuildBaseStage();
    void BuildHuntingStage();
    void BuildGodStage();
    void BuildShadow();
    void BuildShaders();
    void BuildInputElement();
    ComPtr<ID3DBlob> CompileShader(
        const std::wstring& fileName, const D3D_SHADER_MACRO* defines, const std::string& entryPoint, const std::string& target);
private:
    Framework* m_parent = nullptr;
    wstring m_current_stage = L"Base";
    wstring m_stage_queue = L"";
    vector<Object*> m_objects;
    uint32_t m_id_counter = 0;
    Object* m_object_queue[MAX_QUEUE]{};
    int m_object_queue_index = 0;
    //
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
