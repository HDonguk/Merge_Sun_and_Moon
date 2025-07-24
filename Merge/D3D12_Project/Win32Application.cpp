#include "Win32Application.h"
#include "Framework.h"
#include <WindowsX.h>

Win32Application::Win32Application(HINSTANCE hInstance, UINT width, UINT height) :
    m_width{ width },
    m_height{ height },
    m_aspectRatio{ static_cast<float>(width) / static_cast<float>(height) }
{
    CreateWnd(hInstance);
}

void Win32Application::CreateWnd(HINSTANCE hInstance)
{
    // Initialize the window class.
    WNDCLASSEX windowClass = { 0 };
    windowClass.cbSize = sizeof(WNDCLASSEX);
    windowClass.style = CS_HREDRAW | CS_VREDRAW;
    windowClass.lpfnWndProc = WindowProc;
    windowClass.hInstance = hInstance;
    windowClass.hCursor = LoadCursor(NULL, IDC_ARROW);
    windowClass.lpszClassName = L"MyGameClass";
    RegisterClassEx(&windowClass);

    RECT windowRect = { 0, 0, static_cast<LONG>(m_width), static_cast<LONG>(m_height) };
    AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, FALSE);

    // Create the window and store a handle to it.
    m_hwnd = CreateWindow(
        windowClass.lpszClassName,
        GetTitle(),
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        windowRect.right - windowRect.left, // AdjustWindowRect() �Լ��� ���ؼ� ũ�Ⱑ �����Ʊ� ����
        windowRect.bottom - windowRect.top, // AdjustWindowRect() �Լ��� ���ؼ� ũ�Ⱑ �����Ʊ� ����
        nullptr,        // We have no parent window.
        nullptr,        // We aren't using menus.
        hInstance,
        nullptr);
}

void Win32Application::SetCustomWindowText(LPCWSTR text)
{
    std::wstring windowText = m_title + L" : " + text;
    SetWindowText(m_hwnd, windowText.c_str());
}

void Win32Application::OnResize(UINT width, UINT height)
{
    m_width = width;
    m_height = height;
    m_aspectRatio = static_cast<float>(width) / static_cast<float>(height);
}

LRESULT CALLBACK Win32Application::WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    Framework* pSample = reinterpret_cast<Framework*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));

    switch (message)
    {
    case WM_COMMAND:
        if (pSample) {
            // LoginUI로 메시지 전달
            if (pSample->IsInLoginScreen()) {
                if (pSample->GetLoginUI().HandleMessage(message, wParam, lParam)) {
                    return 0;
                }
            }
            // 게임 화면에서는 Windows 컨트롤 메시지 무시
            else {
                return 0;
            }
        }
        break;

    case WM_KEYDOWN:
        if (pSample) {
            // LoginUI로 메시지 전달
            if (pSample->IsInLoginScreen()) {
                if (pSample->GetLoginUI().HandleMessage(message, wParam, lParam)) {
                    return 0;
                }
            }
        }
        break;

    case WM_MOUSEMOVE:
        if (pSample && !pSample->IsInLoginScreen())
        {
            pSample->GetScene(pSample->GetCurrentSceneName()).GetObj<CameraObject>()->OnMouseInput(
                wParam, pSample->GetWin32App().GetHwnd());
        }
        break;

    case WM_SIZE:
        if (pSample)
        {
            // LoginUI로 메시지 전달
            if (pSample->IsInLoginScreen()) {
                if (pSample->GetLoginUI().HandleMessage(message, wParam, lParam)) {
                    return 0;
                }
            }
            
            RECT clientRect{};
            GetClientRect(hWnd, &clientRect);
            pSample->OnResize(clientRect.right - clientRect.left, clientRect.bottom - clientRect.top, wParam == SIZE_MINIMIZED);
        }
        break;

    case WM_DESTROY:
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }

    return 0;
}
