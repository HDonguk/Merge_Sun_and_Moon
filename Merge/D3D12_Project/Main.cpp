#include "stdafx.h"
#include "Framework.h"

_Use_decl_annotations_
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow)
{
    Framework framework;
    framework.OnInit(hInstance, 1280, 720);
    ShowWindow(framework.GetHWnd(), nCmdShow);
    UpdateWindow(framework.GetHWnd());
    ShowCursor(false);
    SetCursor(nullptr); // 초기 커서 핸들 제거

    // 네트워크 활성화 (테스트용)
    // framework.EnableNetwork("127.0.0.1", 5000);

    // Main loop.
    MSG msg{};
    while (msg.message != WM_QUIT)
    {
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else
        {
            framework.OnUpdate();
            framework.OnProcessCollision();
            framework.LateUpdate();
            framework.OnRender();
            
            // 로그인 화면에서는 FPS 제한 (깜빡임 방지)
            if (framework.IsInLoginScreen()) {
                Sleep(16); // 약 60FPS
            }
        }
    }
    return static_cast<int>(msg.wParam);
}