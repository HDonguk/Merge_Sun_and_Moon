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
        }
    }
    return static_cast<int>(msg.wParam);
}