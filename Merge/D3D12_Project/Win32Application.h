#pragma once
#include "stdafx.h"

class Win32Application
{
public:
    Win32Application(HINSTANCE hInstance, UINT width, UINT height);
    void SetCustomWindowText(LPCWSTR text);
    HWND GetHwnd() { return m_hwnd; }
    const WCHAR* GetTitle() const { return m_title.c_str(); }
    UINT GetWidth() const { return m_width; }
    UINT GetHeight() const { return m_height; }
    bool GetWindowVisible() { return m_windowVisible; }
    void SetWindowVisible(bool visible) { m_windowVisible = visible; }
    void OnResize(UINT width, UINT height);

protected:
    static LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

private:
    void CreateWnd(HINSTANCE hInstance);
    HWND m_hwnd = nullptr;
    std::wstring m_title = L"Game";
    UINT m_width = 0;
    UINT m_height = 0;
    float m_aspectRatio = 0.0f;
    bool m_windowVisible = true;
};
