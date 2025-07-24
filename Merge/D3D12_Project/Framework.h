#pragma once
#include "stdafx.h"
#include "Scene.h"
#include "Win32Application.h"
#include "GameTimer.h"
#include "NetworkManager.h"
#include "LoginUI.h"

class Framework
{
public:
	~Framework();	
	void OnInit(HINSTANCE hInstance, UINT width, UINT height);
	void OnUpdate();
	void OnProcessCollision();
	void LateUpdate();
	void OnRender();
	void OnResize(UINT width, UINT height, bool minimized);
	void OnDestroy();

	GameTimer& GetTimer();
	Scene& GetScene(const wstring& name);
	const wstring& GetCurrentSceneName();
	Win32Application& GetWin32App();
	ID3D12Device* GetDevice();
	ID3D12GraphicsCommandList* GetCommandList();
	ID3D12DescriptorHeap* GetDsvDescHeap();
	BYTE* GetKeyState();
	HWND GetHWnd();
	
	// 네트워크 관련
	void EnableNetwork(const char* serverIP, int port);
	void DisableNetwork();
	bool IsNetworkEnabled() const { return m_isNetworkEnabled; }
	NetworkManager& GetNetworkManager() { return m_networkManager; }
	
	// 로그인 관련
	bool IsInLoginScreen() const { return m_isInLoginScreen; }
	LoginUI& GetLoginUI() { return m_loginUI; }
	
	// 로그인 콜백 함수들
	void OnLoginSuccess(int clientID, const std::string& username);
	void OnLoginFailed(const std::string& errorMessage);
	void OnConnectToServer(const std::wstring& serverIP, int port);

private:
	void GetHardwareAdapter(
		IDXGIFactory1* pFactory,
		IDXGIAdapter1** ppAdapter,
		bool requestHighPerformanceAdapter = false
	);
	void BuildFactoryAndDevice();
	void BuildCommandQueueAndSwapChain();
	void BuildCommandListAndAllocator();
	void BuildRtvDescriptorHeap();
	void BuildRtv();
	void BuildDsvDescriptorHeap();
	void BuildDepthStencilBuffer(UINT width, UINT height);
	void BuildDsv();
	void BuildFence();

	void CalculateFrame();
	void PopulateCommandList();
	void BuildScenes(ID3D12Device* device, ID3D12GraphicsCommandList* commandList);
	void WaitForPreviousFrame();

	void ProcessInput();

	unique_ptr<Win32Application> m_win32App;

	GameTimer m_Timer;

	// Adapter info.
	bool m_useWarpDevice = false;

	static const UINT FrameCount = 2;

	// Pipeline objects.
	ComPtr<IDXGIFactory4> m_factory;
	ComPtr<ID3D12Device> m_device;
	ComPtr<ID3D12CommandQueue> m_commandQueue;
	ComPtr<IDXGISwapChain3> m_swapChain;
	ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
	ComPtr<ID3D12DescriptorHeap> m_dsvHeap;
	ComPtr<ID3D12Resource> m_renderTargets[FrameCount];
	ComPtr<ID3D12Resource> m_depthStencilBuffer;
	ComPtr<ID3D12CommandAllocator> m_commandAllocator;
	ComPtr<ID3D12GraphicsCommandList> m_commandList;

	UINT m_rtvDescriptorSize;
	UINT m_dsvDescriptorSize;

	// Synchronization objects.
	UINT m_frameIndex;
	HANDLE m_fenceEvent;
	ComPtr<ID3D12Fence> m_fence;
	UINT64 m_fenceValue;

	unordered_map<wstring, Scene*> m_scenes;
	wstring m_currentSceneName;

	BYTE mKeyState[256]{};
	
	// 네트워크 관련
	NetworkManager m_networkManager;
	bool m_isNetworkEnabled = false;
	
	// 로그인 UI 관련
	LoginUI m_loginUI;
	bool m_isInLoginScreen = true;
};

