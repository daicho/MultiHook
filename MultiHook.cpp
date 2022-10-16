#include <iostream>
#include <string.h>
#include <windows.h>

using namespace std;

LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK LowLevelKeyboardProc(int, WPARAM, LPARAM);

bool prevDevice = false; // 前回の入力デバイス
bool first = true;       // 最初のマウス入力か
int originX, originY;    // 最後のマウス座標
HHOOK hMyHook;           // キーフック

// メイン関数
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpszCmdLine, int nCmdShow) {
    // 多重起動チェック
    TCHAR szAppName[] = TEXT("MultiHook");
    HANDLE hMutex = CreateMutex(NULL, TRUE, szAppName);

    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        MessageBox(NULL, TEXT("既に起動されています"), szAppName, MB_OK);
        ReleaseMutex(hMutex);
        CloseHandle(hMutex);
        return -1;
    }

    // ウィンドウ作成
    WNDCLASS wc;
    HWND hWnd;
    MSG msg;

    wc.style         = 0;
    wc.lpfnWndProc   = WndProc;
    wc.cbClsExtra    = 0;
    wc.cbWndExtra    = 0;
    wc.hInstance     = hInstance;
    wc.hIcon         = LoadIcon(NULL, IDI_APPLICATION);
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszMenuName  = NULL;
    wc.lpszClassName = szAppName;

    if (!RegisterClass(&wc)) return -1;
    hWnd = CreateWindow(szAppName, TEXT(""), WS_OVERLAPPED, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, NULL, NULL, hInstance, NULL);
    if (!hWnd) return -1;

    // キーフック
    hMyHook = SetWindowsHookEx(WH_KEYBOARD_LL, (HOOKPROC)LowLevelKeyboardProc, hInstance, 0);

    if (hMyHook == NULL) {
        MessageBox(hWnd, TEXT("キーフック失敗"), szAppName, MB_OK);
        return -1;
    }

    // RawInput登録
    RAWINPUTDEVICE rid = {0x01, 0x02, RIDEV_INPUTSINK, hWnd};
    if (!RegisterRawInputDevices(&rid, 1, sizeof(RAWINPUTDEVICE))) {
        MessageBox(hWnd, TEXT("RawInput登録失敗"), szAppName, MB_OK);
        return -1;
    }

    // メッセージループ
    while (GetMessage(&msg, NULL, 0, 0) > 0) {
        DispatchMessage(&msg);
    }

    UnhookWindowsHookEx(hMyHook);
    ReleaseMutex(hMutex);
    CloseHandle(hMutex);

    return msg.wParam;
}

// メッセージ
LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        // デバイス入力
        case WM_INPUT: {
            HRAWINPUT hRawInput = (HRAWINPUT)lParam;

            // サイズを取得
            UINT dwSize;
            GetRawInputData(hRawInput, RID_INPUT, NULL, &dwSize, sizeof(RAWINPUTHEADER));

            // 入力内容を取得
            RAWINPUT rawInput;
            GetRawInputData(hRawInput, RID_INPUT, &rawInput, &dwSize, sizeof(RAWINPUTHEADER));

            // マウスの入力かどうか
            bool device = rawInput.header.hDevice != NULL;

            if (device) {
                // 最後のマウス座標を復元
                if (!prevDevice && !first)
                    SetCursorPos(originX, originY);

                if (first)
                    first = false;

                // マウス座標を記録
                POINT originPos;
                GetCursorPos(&originPos);
                originX = originPos.x;
                originY = originPos.y;
            }

            // 前回の入力デバイスを記録
            prevDevice = device;

            break;
        }

        case WM_DESTROY: {
            PostQuitMessage(0);
            return 0;
        }
    }

    return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

// キーフック
LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    KBDLLHOOKSTRUCT* kbs = (KBDLLHOOKSTRUCT*)lParam;

    switch (kbs->vkCode) {
        // Insert→Win+Esc変換
        case VK_INSERT: {
            if (wParam != WM_KEYDOWN)
                break;

            // Win+Escのキー入力を設定
            INPUT inputWinEsc[4] = {};

            inputWinEsc[0].type = INPUT_KEYBOARD;
            inputWinEsc[0].ki.wVk = VK_LWIN;

            inputWinEsc[1].type = INPUT_KEYBOARD;
            inputWinEsc[1].ki.wVk = VK_ESCAPE;

            inputWinEsc[2].type = INPUT_KEYBOARD;
            inputWinEsc[2].ki.wVk = VK_ESCAPE;
            inputWinEsc[2].ki.dwFlags = KEYEVENTF_KEYUP;

            inputWinEsc[3].type = INPUT_KEYBOARD;
            inputWinEsc[3].ki.wVk = VK_LWIN;
            inputWinEsc[3].ki.dwFlags = KEYEVENTF_KEYUP;

            // キーを送信
            SendInput(ARRAYSIZE(inputWinEsc), inputWinEsc, sizeof(INPUT));

            return -1;
        }

        // Windowsキー無効化
        case VK_LWIN:
        case VK_RWIN: {
            // 最前面のウィンドウのタイトルを取得
            HWND hForeWnd = GetForegroundWindow();
            int titleLength = GetWindowTextLength(hForeWnd);
            LPSTR winTitle = (LPSTR)malloc(titleLength + 1);
            GetWindowText(GetForegroundWindow(), winTitle, titleLength + 1);

            // Apexが最前面ならWindowsキーを無効化
            if (strcmp(winTitle, TEXT("Apex Legends")) == 0)
                return -1;
        }
    }

    return CallNextHookEx(hMyHook, nCode, wParam, lParam);
}
