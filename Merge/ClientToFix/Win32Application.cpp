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
        windowRect.right - windowRect.left, // AdjustWindowRect() 함수에 의해서 크기가 조정됐기 때문
        windowRect.bottom - windowRect.top, // AdjustWindowRect() 함수에 의해서 크기가 조정됐기 때문
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
    case WM_MOUSEMOVE:
        if (pSample)
        {
            pSample->GetScene(pSample->GetCurrentSceneName()).GetObj<CameraObject>()->OnMouseInput(
                wParam, pSample->GetWin32App().GetHwnd());
        }
        break;

    case WM_SIZE:
        if (pSample)
        {
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
