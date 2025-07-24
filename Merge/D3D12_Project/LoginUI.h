#pragma once
#include "stdafx.h"
#include <string>
#include <functional>

// UIState enum을 클래스 외부로 이동
enum class UIState {
    LOGIN_SCREEN,
    CONNECTING,
    CONNECTED,
    ERROR_STATE
};

class LoginUI {
public:
    LoginUI();
    ~LoginUI();

    void Initialize(HWND hwnd, HINSTANCE hInstance);
    void Render();
    bool HandleMessage(UINT message, WPARAM wParam, LPARAM lParam);
    void SetState(UIState state);
    void SetErrorMessage(const std::wstring& message);
    void SetLoginCallback(std::function<void(const std::wstring&, const std::wstring&)> callback);
    void SetConnectCallback(std::function<void(const std::wstring&, int)> callback);
    HWND GetHwnd() const { return m_hwnd; }
    HWND GetEditUsernameHwnd() const { return m_editUsername; }
    void ShowControls(bool show); // public으로 이동
    void DestroyControls(); // 컨트롤 완전 제거

private:
    HWND m_hwnd = nullptr;
    HWND m_gameLogo = nullptr; // 게임 로고
    HWND m_editUsername = nullptr;
    HWND m_editServerIP = nullptr;
    HWND m_editServerPort = nullptr;
    HWND m_buttonConnect = nullptr;
    HWND m_staticStatus = nullptr;

    UIState m_currentState = UIState::LOGIN_SCREEN;
    std::wstring m_errorMessage;
    std::function<void(const std::wstring&, const std::wstring&)> m_loginCallback;
    std::function<void(const std::wstring&, int)> m_connectCallback;

    void CreateControls(HINSTANCE hInstance);
    void UpdateLayout();
    void OnConnectButton();
    void OnKeyDown(WPARAM wParam);
}; 