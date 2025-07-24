#include "stdafx.h"
#include "LoginUI.h"
#include <commctrl.h>

#pragma comment(lib, "comctl32.lib")

LoginUI::LoginUI() {
    // Common Controls 초기화
    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_WIN95_CLASSES;
    InitCommonControlsEx(&icex);
}

LoginUI::~LoginUI() {
    // 컨트롤들은 부모 윈도우가 파괴될 때 자동으로 정리됨
}

void LoginUI::Initialize(HWND hwnd, HINSTANCE hInstance) {
    m_hwnd = hwnd;
    
    // 디버깅을 위한 로그
    OutputDebugString(L"[LoginUI] Initializing...\n");
    
    // 윈도우 스타일을 Windows 컨트롤에 적합하게 변경
    LONG_PTR style = GetWindowLongPtr(hwnd, GWL_STYLE);
    style |= WS_CLIPCHILDREN;  // 자식 윈도우 클리핑 활성화
    SetWindowLongPtr(hwnd, GWL_STYLE, style);
    
    CreateControls(hInstance);
    UpdateLayout();
    
    OutputDebugString(L"[LoginUI] Initialization complete\n");
}

void LoginUI::CreateControls(HINSTANCE hInstance) {
    OutputDebugString(L"[LoginUI] Creating controls...\n");
    
    // 게임 로고 (배경색 추가)
    m_gameLogo = CreateWindowEx(
        WS_EX_STATICEDGE, L"STATIC", L"D3D12 Network Game",
        WS_CHILD | WS_VISIBLE | SS_CENTER | SS_NOTIFY | SS_SUNKEN,
        0, 0, 0, 0, m_hwnd, (HMENU)1000, hInstance, nullptr
    );
    
    if (!m_gameLogo) {
        OutputDebugString(L"[LoginUI] Failed to create game logo\n");
    } else {
        OutputDebugString(L"[LoginUI] Game logo created successfully\n");
    }
    
    // 사용자명 입력
    m_editUsername = CreateWindowEx(
        WS_EX_CLIENTEDGE, L"EDIT", L"Player1",
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | WS_BORDER,
        0, 0, 0, 0, m_hwnd, (HMENU)1001, hInstance, nullptr
    );

    // 서버 IP 입력
    m_editServerIP = CreateWindowEx(
        WS_EX_CLIENTEDGE, L"EDIT", L"127.0.0.1",
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | WS_BORDER,
        0, 0, 0, 0, m_hwnd, (HMENU)1002, hInstance, nullptr
    );

    // 서버 포트 입력
    m_editServerPort = CreateWindowEx(
        WS_EX_CLIENTEDGE, L"EDIT", L"5000",
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | WS_BORDER,
        0, 0, 0, 0, m_hwnd, (HMENU)1003, hInstance, nullptr
    );

    // 연결 버튼
    m_buttonConnect = CreateWindowEx(
        WS_EX_STATICEDGE, L"BUTTON", L"Connect to Server",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_BORDER,
        0, 0, 0, 0, m_hwnd, (HMENU)1004, hInstance, nullptr
    );

    // 상태 표시 (배경색 추가)
    m_staticStatus = CreateWindowEx(
        WS_EX_STATICEDGE, L"STATIC", L"Enter your username and server information",
        WS_CHILD | WS_VISIBLE | SS_CENTER | SS_NOTIFY | SS_SUNKEN | WS_BORDER,
        0, 0, 0, 0, m_hwnd, (HMENU)1005, hInstance, nullptr
    );

    // 폰트 설정
    HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
    HFONT hLogoFont = CreateFont(48, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Arial");
    
    SendMessage(m_gameLogo, WM_SETFONT, (WPARAM)hLogoFont, TRUE);
    SendMessage(m_editUsername, WM_SETFONT, (WPARAM)hFont, TRUE);
    SendMessage(m_editServerIP, WM_SETFONT, (WPARAM)hFont, TRUE);
    SendMessage(m_editServerPort, WM_SETFONT, (WPARAM)hFont, TRUE);
    SendMessage(m_buttonConnect, WM_SETFONT, (WPARAM)hFont, TRUE);
    SendMessage(m_staticStatus, WM_SETFONT, (WPARAM)hFont, TRUE);
    
    // 모든 컨트롤 생성 확인
    if (m_editUsername && m_editServerIP && m_editServerPort && m_buttonConnect && m_staticStatus) {
        OutputDebugString(L"[LoginUI] All controls created successfully\n");
        
        // 컨트롤 배경색 설정 (더 잘 보이도록)
        SetBkColor(GetDC(m_gameLogo), RGB(255, 255, 255));  // 흰색 배경
        SetBkColor(GetDC(m_staticStatus), RGB(255, 255, 255));  // 흰색 배경
        
        // 텍스트 색상 설정
        SetTextColor(GetDC(m_gameLogo), RGB(0, 0, 0));  // 검은색 텍스트
        SetTextColor(GetDC(m_staticStatus), RGB(0, 0, 0));  // 검은색 텍스트
    } else {
        OutputDebugString(L"[LoginUI] Some controls failed to create\n");
    }
}

void LoginUI::UpdateLayout() {
    OutputDebugString(L"[LoginUI] Updating layout...\n");
    
    RECT rect;
    GetClientRect(m_hwnd, &rect);
    int width = rect.right - rect.left;
    int height = rect.bottom - rect.top;
    
    OutputDebugString(L"[LoginUI] Window size: ");
    OutputDebugString(std::to_wstring(width).c_str());
    OutputDebugString(L" x ");
    OutputDebugString(std::to_wstring(height).c_str());
    OutputDebugString(L"\n");

    int centerX = width / 2;
    int logoY = height / 4; // 로고를 위쪽에 배치
    int startY = height / 2; // 입력란들을 아래쪽으로 이동
    int controlHeight = 25;
    int spacing = 10;

    // 게임 로고 (더 크게 표시, 배경색 추가)
    SetWindowPos(m_gameLogo, nullptr, centerX - 250, logoY, 500, 80, SWP_NOZORDER);

    // 사용자명 라벨 (배경색 추가)
    HWND usernameLabel = CreateWindowEx(WS_EX_STATICEDGE, L"STATIC", L"Username:", 
        WS_CHILD | WS_VISIBLE | SS_RIGHT | SS_SUNKEN | WS_BORDER,
        centerX - 150, startY, 100, controlHeight, m_hwnd, nullptr, nullptr, nullptr);
    SetWindowPos(usernameLabel, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);

    // 사용자명 입력
    SetWindowPos(m_editUsername, nullptr, centerX - 50, startY, 200, controlHeight, SWP_NOZORDER);

    // 서버 IP 라벨 (배경색 추가)
    HWND serverIPLabel = CreateWindowEx(WS_EX_STATICEDGE, L"STATIC", L"Server IP:", 
        WS_CHILD | WS_VISIBLE | SS_RIGHT | SS_SUNKEN | WS_BORDER,
        centerX - 150, startY + controlHeight + spacing, 100, controlHeight, m_hwnd, nullptr, nullptr, nullptr);
    SetWindowPos(serverIPLabel, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);

    // 서버 IP 입력
    SetWindowPos(m_editServerIP, nullptr, centerX - 50, startY + controlHeight + spacing, 200, controlHeight, SWP_NOZORDER);

    // 서버 포트 라벨 (배경색 추가)
    HWND portLabel = CreateWindowEx(WS_EX_STATICEDGE, L"STATIC", L"Port:", 
        WS_CHILD | WS_VISIBLE | SS_RIGHT | SS_SUNKEN | WS_BORDER,
        centerX - 150, startY + (controlHeight + spacing) * 2, 100, controlHeight, m_hwnd, nullptr, nullptr, nullptr);
    SetWindowPos(portLabel, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);

    // 서버 포트 입력
    SetWindowPos(m_editServerPort, nullptr, centerX - 50, startY + (controlHeight + spacing) * 2, 200, controlHeight, SWP_NOZORDER);

    // 연결 버튼
    SetWindowPos(m_buttonConnect, nullptr, centerX - 50, startY + (controlHeight + spacing) * 3, 200, controlHeight, SWP_NOZORDER);

    // 상태 표시
    SetWindowPos(m_staticStatus, nullptr, centerX - 200, startY + (controlHeight + spacing) * 4, 400, controlHeight * 2, SWP_NOZORDER);
    
    // 라벨들에 배경색 설정
    SetBkColor(GetDC(usernameLabel), RGB(255, 255, 255));
    SetBkColor(GetDC(serverIPLabel), RGB(255, 255, 255));
    SetBkColor(GetDC(portLabel), RGB(255, 255, 255));
    SetTextColor(GetDC(usernameLabel), RGB(0, 0, 0));
    SetTextColor(GetDC(serverIPLabel), RGB(0, 0, 0));
    SetTextColor(GetDC(portLabel), RGB(0, 0, 0));
}

void LoginUI::ShowControls(bool show) {
    OutputDebugString(L"[LoginUI] Showing controls: ");
    OutputDebugString(show ? L"true" : L"false");
    OutputDebugString(L"\n");
    
    if (m_gameLogo) ShowWindow(m_gameLogo, show ? SW_SHOW : SW_HIDE);
    if (m_editUsername) ShowWindow(m_editUsername, show ? SW_SHOW : SW_HIDE);
    if (m_editServerIP) ShowWindow(m_editServerIP, show ? SW_SHOW : SW_HIDE);
    if (m_editServerPort) ShowWindow(m_editServerPort, show ? SW_SHOW : SW_HIDE);
    if (m_buttonConnect) ShowWindow(m_buttonConnect, show ? SW_SHOW : SW_HIDE);
    if (m_staticStatus) ShowWindow(m_staticStatus, show ? SW_SHOW : SW_HIDE);
    
    // 컨트롤들이 보이는지 확인
    if (show) {
        OutputDebugString(L"[LoginUI] Controls should be visible now\n");
    } else {
        OutputDebugString(L"[LoginUI] Controls should be hidden now\n");
    }
}

void LoginUI::Render() {
    // Windows 컨트롤은 자동으로 렌더링되므로 추가 작업 불필요
    // 깜빡임 방지를 위해 아무것도 하지 않음
}

bool LoginUI::HandleMessage(UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_COMMAND:
            if (LOWORD(wParam) == 1004 && HIWORD(wParam) == BN_CLICKED) {
                OnConnectButton();
                return true;
            }
            break;
        case WM_KEYDOWN:
            if (wParam == VK_RETURN) {
                OnConnectButton();
                return true;
            }
            break;
        case WM_SIZE:
            UpdateLayout();
            return true;
    }
    return false;
}

void LoginUI::SetState(UIState state) {
    OutputDebugString(L"[LoginUI] Setting state...\n");
    m_currentState = state;
    
    switch (state) {
        case UIState::LOGIN_SCREEN:
            OutputDebugString(L"[LoginUI] Setting state to LOGIN_SCREEN\n");
            ShowControls(true);
            SetWindowText(m_staticStatus, L"Enter your username and server information");
            SetWindowText(m_buttonConnect, L"Connect to Server");
            EnableWindow(m_buttonConnect, TRUE);
            break;
            
        case UIState::CONNECTING:
            ShowControls(false);
            SetWindowText(m_staticStatus, L"Connecting to server...");
            SetWindowText(m_buttonConnect, L"Connecting...");
            EnableWindow(m_buttonConnect, FALSE);
            break;
            
        case UIState::CONNECTED:
            // 모든 컨트롤 완전 제거
            DestroyControls();
            break;
            
        case UIState::ERROR_STATE:
            ShowControls(true);
            SetWindowText(m_staticStatus, m_errorMessage.c_str());
            SetWindowText(m_buttonConnect, L"Retry Connection");
            EnableWindow(m_buttonConnect, TRUE);
            break;
    }
}

void LoginUI::SetErrorMessage(const std::wstring& message) {
    m_errorMessage = message;
    if (m_currentState == UIState::ERROR_STATE) {
        SetWindowText(m_staticStatus, m_errorMessage.c_str());
    }
}

void LoginUI::SetLoginCallback(std::function<void(const std::wstring&, const std::wstring&)> callback) {
    m_loginCallback = callback;
}

void LoginUI::SetConnectCallback(std::function<void(const std::wstring&, int)> callback) {
    m_connectCallback = callback;
}

void LoginUI::OnConnectButton() {
    if (m_currentState == UIState::CONNECTING) {
        return; // 연결 중에는 버튼 비활성화
    }

    // 입력값 가져오기
    wchar_t username[256] = {0};
    wchar_t serverIP[256] = {0};
    wchar_t serverPort[16] = {0};

    GetWindowText(m_editUsername, username, 256);
    GetWindowText(m_editServerIP, serverIP, 256);
    GetWindowText(m_editServerPort, serverPort, 16);

    // 입력 검증
    if (wcslen(username) == 0) {
        SetErrorMessage(L"Please enter a username");
        SetState(UIState::ERROR_STATE);
        return;
    }

    if (wcslen(serverIP) == 0) {
        SetErrorMessage(L"Please enter server IP");
        SetState(UIState::ERROR_STATE);
        return;
    }

    int port = _wtoi(serverPort);
    if (port <= 0 || port > 65535) {
        SetErrorMessage(L"Please enter a valid port number (1-65535)");
        SetState(UIState::ERROR_STATE);
        return;
    }

    // 로그인 콜백 호출
    if (m_loginCallback) {
        m_loginCallback(username, username); // 사용자명을 닉네임으로도 사용
    }

    // 연결 콜백 호출
    if (m_connectCallback) {
        m_connectCallback(serverIP, port);
    }

    SetState(UIState::CONNECTING);
}

void LoginUI::OnKeyDown(WPARAM wParam) {
    if (wParam == VK_RETURN) {
        OnConnectButton();
    }
}

void LoginUI::DestroyControls() {
    OutputDebugString(L"[LoginUI] Destroying all controls\n");
    
    // 모든 컨트롤 파괴
    if (m_gameLogo) {
        DestroyWindow(m_gameLogo);
        m_gameLogo = nullptr;
    }
    if (m_editUsername) {
        DestroyWindow(m_editUsername);
        m_editUsername = nullptr;
    }
    if (m_editServerIP) {
        DestroyWindow(m_editServerIP);
        m_editServerIP = nullptr;
    }
    if (m_editServerPort) {
        DestroyWindow(m_editServerPort);
        m_editServerPort = nullptr;
    }
    if (m_buttonConnect) {
        DestroyWindow(m_buttonConnect);
        m_buttonConnect = nullptr;
    }
    if (m_staticStatus) {
        DestroyWindow(m_staticStatus);
        m_staticStatus = nullptr;
    }
    
    // 윈도우 핸들도 초기화
    m_hwnd = nullptr;
    
    // 상태 초기화
    m_currentState = UIState::LOGIN_SCREEN;
    
    OutputDebugString(L"[LoginUI] All controls destroyed\n");
} 