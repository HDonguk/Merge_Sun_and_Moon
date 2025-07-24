#include "Shadow.h"
#include "Scene.h"
#include "Framework.h"
#include "DXSampleHelper.h"

Shadow::Shadow(Scene* parent, UINT width, UINT height) :
	mParent{ parent },
	mWidth{ width },
	mHeight{ height },
	mViewport{ 0.f, 0.f, static_cast<float>(width), static_cast<float>(height), 0.f, 1.f },
	mScissorRect{ 0, 0, static_cast<long>(width), static_cast<long>(height) },
	mSceneSphere{ XMFLOAT3{0.f,0.f,0.f}, 500.f }
{
	XMStoreFloat3(&mLightDirection, XMVector3Normalize(XMVECTOR{ -1.f, -1.f, 0.f }));

	ID3D12DescriptorHeap* cbvSrvUavDescHeap = mParent->GetDescriptorHeap();
	D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = cbvSrvUavDescHeap->GetCPUDescriptorHandleForHeapStart();
	UINT descriptorHeapIndex = 1 + mParent->GetNumOfTexture();
	ID3D12Device* device = mParent->GetFramework()->GetDevice();
	UINT incrementSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	mSrvCpuHandle.ptr = cpuHandle.ptr + descriptorHeapIndex * incrementSize;
	
	D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = cbvSrvUavDescHeap->GetGPUDescriptorHandleForHeapStart();
	mSrvGpuHandle.ptr = gpuHandle.ptr + descriptorHeapIndex * incrementSize;

	mNullSrvCpuHandle.ptr = mSrvCpuHandle.ptr + 1 * incrementSize;
	mNullSrvGpuHandle.ptr = mSrvGpuHandle.ptr + 1 * incrementSize;

	cpuHandle = mParent->GetFramework()->GetDsvDescHeap()->GetCPUDescriptorHandleForHeapStart();
	incrementSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
	mDsvCpuHandle.ptr = cpuHandle.ptr + 1 * incrementSize;
	
	BuildResource();
	BuildDescView();
}

void Shadow::UpdateShadow()
{
	PlayerObject* player = mParent->GetObj<PlayerObject>();
	Transform* transform = player->GetComponent<Transform>();
	XMVECTOR pos = transform->GetPosition();
	float posX = XMVectorGetX(pos);
	float posZ = XMVectorGetZ(pos);
	mSceneSphere.Center = { posX, 0.0f, posZ };
	//기회가 되면 Framework 로 부터 시간 가져와서 조명의 위치에 따라 그림자가 생성되게 구현.
	XMVECTOR lightDir = XMLoadFloat3(&mLightDirection);
	XMVECTOR lightPos = -5.f * mSceneSphere.Radius * lightDir;
	XMVECTOR target = XMLoadFloat3(&mSceneSphere.Center);
	XMVECTOR up = { 0.f, 1.f, 0.f , 0.f};
	XMMATRIX lightViewMatrix = XMMatrixLookAtLH(lightPos, target, up);
	//XMMATRIX lightViewMatrix = XMMatrixLookToLH(lightPos, target, up);
	XMStoreFloat4x4(&mViewMatrix, lightViewMatrix);

	XMFLOAT3 targetInLightViewCoord;
	XMStoreFloat3(&targetInLightViewCoord, XMVector3TransformCoord(target, lightViewMatrix));

	float left = targetInLightViewCoord.x - mSceneSphere.Radius;
	float right = targetInLightViewCoord.x + mSceneSphere.Radius;
	float bottom = targetInLightViewCoord.y - mSceneSphere.Radius;
	float top = targetInLightViewCoord.y + mSceneSphere.Radius;
	float n = targetInLightViewCoord.z - mSceneSphere.Radius;
	float f = targetInLightViewCoord.z + mSceneSphere.Radius;

	XMMATRIX lightProjMatrix = XMMatrixOrthographicOffCenterLH(left, right, bottom, top, n, f);
	XMStoreFloat4x4(&mProjMatrix, lightProjMatrix);

	XMMATRIX textureMatrix{
		0.5f, 0.0f, 0.0f, 0.0f,
		0.0f, -0.5f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.5f, 0.5f, 0.0f, 1.0f
	};
	XMStoreFloat4x4(&mTextureMatrix, textureMatrix);


	XMMATRIX finalTransformMatrix = lightViewMatrix * lightProjMatrix * textureMatrix;
	XMStoreFloat4x4(&mFinalMatrix, finalTransformMatrix);

	UINT8* mappedData = static_cast<UINT8*>(mParent->GetConstantBufferMappedData());
	memcpy(mappedData + 2 * sizeof(XMFLOAT4X4), &XMMatrixTranspose(lightViewMatrix * lightProjMatrix), sizeof(XMMATRIX));
	memcpy(mappedData + 3 * sizeof(XMFLOAT4X4), &XMMatrixTranspose(textureMatrix), sizeof(XMMATRIX));
}

void Shadow::DrawShadowMap()
{
	ID3D12Device* device = mParent->GetFramework()->GetDevice();
	ID3D12GraphicsCommandList* commandList = mParent->GetFramework()->GetCommandList();

	commandList->RSSetViewports(1, &mViewport);
	commandList->RSSetScissorRects(1, &mScissorRect);

	D3D12_RESOURCE_BARRIER barrier{};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier.Transition.pResource = mShadowMap.Get();
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_GENERIC_READ;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_DEPTH_WRITE;
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	commandList->ResourceBarrier(1, &barrier);

	commandList->ClearDepthStencilView(mDsvCpuHandle, D3D12_CLEAR_FLAG_DEPTH| D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);
	commandList->OMSetRenderTargets(0, nullptr, false, &mDsvCpuHandle);
	
	auto& PSOs = mParent->GetPSOs();
	commandList->SetPipelineState(PSOs["PSO_Shadow"].Get());

	mParent->RenderObjects(device, commandList);

	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_DEPTH_WRITE;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_GENERIC_READ;
	commandList->ResourceBarrier(1, &barrier);
}

Scene* Shadow::GetScene()
{
	return mParent;
}

D3D12_GPU_DESCRIPTOR_HANDLE& Shadow::GetGpuDescHandleForShadow()
{
	return mSrvGpuHandle;
}

D3D12_GPU_DESCRIPTOR_HANDLE& Shadow::GetGpuDescHandleForNullShadow()
{
	return mNullSrvGpuHandle;
}

void Shadow::BuildDescView()
{
	ID3D12Device* device = mParent->GetFramework()->GetDevice();

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = 1;
	srvDesc.Texture2D.PlaneSlice = 0;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
	device->CreateShaderResourceView(mShadowMap.Get(), &srvDesc, mSrvCpuHandle);

	device->CreateShaderResourceView(nullptr, &srvDesc, mNullSrvCpuHandle);

	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
	dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	dsvDesc.Texture2D.MipSlice = 0;
	device->CreateDepthStencilView(mShadowMap.Get(), &dsvDesc, mDsvCpuHandle);
}

void Shadow::BuildResource()
{

	D3D12_HEAP_PROPERTIES heapProp{};
	heapProp.Type = D3D12_HEAP_TYPE_DEFAULT;
	heapProp.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN; // heapType이 custom일 경우에 사용
	heapProp.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN; // heapType이 custom일 경우에 사용
	heapProp.CreationNodeMask = 0x01; // 멀티 GPU 일경우 특정 디바이스 선택용
	heapProp.VisibleNodeMask = 0x01; // 멀티 GPU 일경우 특정 디바이스 선택용
	
	D3D12_RESOURCE_DESC resourceDesc{};
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	resourceDesc.Alignment = 0;
	resourceDesc.Width = mWidth;
	resourceDesc.Height = mHeight;
	resourceDesc.DepthOrArraySize = 1;
	resourceDesc.MipLevels = 1;
	resourceDesc.Format = DXGI_FORMAT_R24G8_TYPELESS;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.SampleDesc.Quality = 0;
	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	D3D12_CLEAR_VALUE clearValue{};
	clearValue.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	clearValue.DepthStencil.Depth = 1.f;
	clearValue.DepthStencil.Stencil = 0;

	ID3D12Device* device = GetScene()->GetFramework()->GetDevice();

	// Depth stencil 과 Shader resource 로 사용될 리소스 생성
	ThrowIfFailed(device->CreateCommittedResource(
		&heapProp,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		&clearValue,
		IID_PPV_ARGS(mShadowMap.GetAddressOf())
	));
}