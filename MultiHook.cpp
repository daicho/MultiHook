#include <iostream>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

using namespace std;

LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK LowLevelKeyboardProc(int, WPARAM, LPARAM);
BOOL CALLBACK MonitorEnumProc(HMONITOR, HDC, LPRECT, LPARAM);

bool prevDevice = false; // �O��̓��̓f�o�C�X
bool first = true;       // �ŏ��̃}�E�X���͂�
int originX, originY;    // �Ō�̃}�E�X���W
HHOOK hMyHook;           // �L�[�t�b�N

// �f�B�X�v���C���
struct MONITOR {
    HMONITOR hMonitor;
    RECT rect;
};

// �}���`�f�B�X�v���C���
struct MONITORS {
    int count = 0;
    MONITOR* monitors;
};

// �f�B�X�v���C�\�[�g�p��r�֐�
int cmpdisp(const void *m1, const void *m2) {
    MONITOR* monitor1 = (MONITOR*)m1;
    MONITOR* monitor2 = (MONITOR*)m2;

    // �E����, �と���Ɉړ�
    if (monitor1->rect.left > monitor2->rect.left) {
        return -1;
    } else if (monitor1->rect.left < monitor2->rect.left) {
        return 1;
    } else {
        if (monitor1->rect.top > monitor2->rect.top) {
            return 1;
        } else if (monitor1->rect.top < monitor2->rect.top) {
            return -1;
        } else {
            return 0;
        }
    }
}

// ���C���֐�
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpszCmdLine, int nCmdShow) {
    // ���d�N���`�F�b�N
    TCHAR szAppName[] = TEXT("MultiHook");
    HANDLE hMutex = CreateMutex(NULL, TRUE, szAppName);

    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        MessageBox(NULL, TEXT("���ɋN������Ă��܂�"), szAppName, MB_OK);
        ReleaseMutex(hMutex);
        CloseHandle(hMutex);
        return -1;
    }

    // �E�B���h�E�쐬
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

    // �L�[�t�b�N
    hMyHook = SetWindowsHookEx(WH_KEYBOARD_LL, (HOOKPROC)LowLevelKeyboardProc, hInstance, 0);

    if (hMyHook == NULL) {
        MessageBox(hWnd, TEXT("�L�[�t�b�N���s"), szAppName, MB_OK);
        return -1;
    }

    // RawInput�o�^
    RAWINPUTDEVICE rid = {0x01, 0x02, RIDEV_INPUTSINK, hWnd};
    if (!RegisterRawInputDevices(&rid, 1, sizeof(RAWINPUTDEVICE))) {
        MessageBox(hWnd, TEXT("RawInput�o�^���s"), szAppName, MB_OK);
        return -1;
    }

    // ���b�Z�[�W���[�v
    while (GetMessage(&msg, NULL, 0, 0) > 0) {
        DispatchMessage(&msg);
    }

    UnhookWindowsHookEx(hMyHook);
    ReleaseMutex(hMutex);
    CloseHandle(hMutex);

    return msg.wParam;
}

// ���b�Z�[�W
LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        // �f�o�C�X����
        case WM_INPUT: {
            HRAWINPUT hRawInput = (HRAWINPUT)lParam;

            // �T�C�Y���擾
            UINT dwSize;
            GetRawInputData(hRawInput, RID_INPUT, NULL, &dwSize, sizeof(RAWINPUTHEADER));

            // ���͓��e���擾
            RAWINPUT rawInput;
            GetRawInputData(hRawInput, RID_INPUT, &rawInput, &dwSize, sizeof(RAWINPUTHEADER));

            // �}�E�X�̓��͂��ǂ���
            bool device = rawInput.header.hDevice != NULL;

            if (device) {
                // �Ō�̃}�E�X���W�𕜌�
                if (!prevDevice && !first)
                    SetCursorPos(originX, originY);

                if (first)
                    first = false;

                // �}�E�X���W���L�^
                POINT originPos;
                GetCursorPos(&originPos);
                originX = originPos.x;
                originY = originPos.y;
            }

            // �O��̓��̓f�o�C�X���L�^
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

// �L�[�t�b�N
LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    KBDLLHOOKSTRUCT* kbs = (KBDLLHOOKSTRUCT*)lParam;

    switch (kbs->vkCode) {
        // �}���`�f�B�X�v���C�ԃE�B���h�E�ړ�
        case VK_INSERT: {
            if (wParam != WM_KEYDOWN)
                break;

            // �e�f�B�X�v���C�̍��W���擾
            int n = GetSystemMetrics(SM_CMONITORS);
            MONITORS monitors = {0, new MONITOR[n]};
            EnumDisplayMonitors(NULL, NULL, (MONITORENUMPROC)MonitorEnumProc, (LPARAM)&monitors);

            // ���W���ɂȂ�悤�Ƀ\�[�g
            qsort(monitors.monitors, n, sizeof(MONITOR), cmpdisp);

            // ���݂̃f�B�X�v���C���擾
            HWND hWnd = GetForegroundWindow();
            HMONITOR hCurrentMonitor = MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST);

            // �ő剻����Ă��������
            bool isZoomed = IsZoomed(hWnd);
            if (isZoomed)
                ShowWindow(hWnd, SW_RESTORE);

            // �E�B���h�E�̍��W���擾
            RECT rcWindow;
            GetWindowRect(hWnd, &rcWindow);

            for (int i = 0; i < n; i++) {
                MONITOR *curMonitor = &monitors.monitors[i];
                MONITOR *nextMonitor = &monitors.monitors[(i + 1) % n];

                if (curMonitor->hMonitor == hCurrentMonitor) {
                    // ���̃E�B���h�E�̍��W���擾
                    int x = nextMonitor->rect.left + rcWindow.left - curMonitor->rect.left;
                    int y = nextMonitor->rect.top + rcWindow.top - curMonitor->rect.top;
                    int w = rcWindow.right - rcWindow.left;
                    int h = rcWindow.bottom - rcWindow.top;

                    // ���̃f�B�X�v���C�ֈړ�
                    MoveWindow(hWnd, x, y, w, h, TRUE);

                    // �ő剻�����ɖ߂�
                    if (isZoomed)
                        ShowWindow(hWnd, SW_MAXIMIZE);

                    break;
                }
            }

            return -1;
        }

        // Windows�L�[������
        case VK_LWIN:
        case VK_RWIN: {
            // �őO�ʂ̃E�B���h�E�̃^�C�g�����擾
            HWND hForeWnd = GetForegroundWindow();
            int titleLength = GetWindowTextLength(hForeWnd);
            LPSTR winTitle = (LPSTR)malloc(titleLength + 1);
            GetWindowText(GetForegroundWindow(), winTitle, titleLength + 1);

            // Apex���őO�ʂȂ�Windows�L�[�𖳌���
            if (strcmp(winTitle, TEXT("Apex Legends")) == 0)
                return -1;
        }
    }

    return CallNextHookEx(hMyHook, nCode, wParam, lParam);
}

// �}���`�f�B�X�v���C���擾
BOOL CALLBACK MonitorEnumProc(HMONITOR hMonitor, HDC hdc, LPRECT lprcMonitor, LPARAM dwData) {
    MONITORS* monitors = (MONITORS*)dwData;

    // �f�B�X�v���C�����i�[
    monitors->monitors[monitors->count].hMonitor = hMonitor;
    monitors->monitors[monitors->count].rect = *lprcMonitor;
    monitors->count++;

    return TRUE;
}
