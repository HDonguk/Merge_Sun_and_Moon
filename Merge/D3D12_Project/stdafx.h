#pragma once

#include <SDKDDKVer.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers.
#endif

// Prevent winsock.h from being included by windows.h
#define _WINSOCKAPI_
#define _WINSOCK_DEPRECATED_NO_WARNINGS

// Winsock2 must be included before windows.h to prevent conflicts
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "libfbxsdk.lib")
#pragma comment(lib, "ws2_32.lib")

#include <windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <D3Dcompiler.h>
#include <DirectXMath.h>
#include "d3dx12.h"

#include <string>
#include <wrl.h>

#include <unordered_map>
#include <vector>
#include <array>

#include "include/fbxsdk.h"
#include "DDSTextureLoader12.h"

using namespace std;
using namespace DirectX;
using Microsoft::WRL::ComPtr;

template<typename T> using ptr = T*;
