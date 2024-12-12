#define UNICODE
#define _UNICODE

#include <windows.h>
#include <shlobj.h>
#include <string>
#include <filesystem>

#pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#pragma comment(linker, "/MANIFESTUAC:\"level='requireAdministrator' uiAccess='false'\"")
#pragma comment(lib, "version.lib")


// Global variables
HWND hwndPath;
HWND hwndInstall;
HINSTANCE hInst;  // Add this
const wchar_t* DEFAULT_PATH = L"C:\\Program Files\\nudhi-lang";
const wchar_t* WINDOW_CLASS = L"NudhiInstaller";

bool IsRunAsAdmin() {
    BOOL isAdmin = FALSE;
    PSID adminGroup = NULL;
    SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;

    if (AllocateAndInitializeSid(&ntAuthority, 2,
        SECURITY_BUILTIN_DOMAIN_RID,
        DOMAIN_ALIAS_RID_ADMINS,
        0, 0, 0, 0, 0, 0,
        &adminGroup)) {
        if (!CheckTokenMembership(NULL, adminGroup, &isAdmin)) {
            isAdmin = FALSE;
        }
        FreeSid(adminGroup);
    }
    return isAdmin != 0;
}


// Window procedure
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            // Create path input field
            hwndPath = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", DEFAULT_PATH,
                WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                20, 20, 300, 25, hwnd, NULL, hInst, NULL);

            // Create browse button
            CreateWindowW(L"BUTTON", L"Browse...",
                WS_CHILD | WS_VISIBLE,
                330, 20, 80, 25, hwnd, (HMENU)1, hInst, NULL);

            // Create install button
            hwndInstall = CreateWindowW(L"BUTTON", L"Install",
                WS_CHILD | WS_VISIBLE,
                20, 60, 390, 30, hwnd, (HMENU)2, hInst, NULL);
            
            break;
        }

        case WM_COMMAND: {
            switch (LOWORD(wParam)) {
                case 1: // Browse button
                    {
                        wchar_t path[MAX_PATH];
                        BROWSEINFOW bi = {0};
                        bi.hwndOwner = hwnd;
                        bi.lpszTitle = L"Select Installation Directory";
                        bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
                        
                        LPITEMIDLIST pidl = SHBrowseForFolderW(&bi);
                        if (pidl) {
                            if (SHGetPathFromIDListW(pidl, path)) {
                                SetWindowTextW(hwndPath, path);
                            }
                            CoTaskMemFree(pidl);
                        }
                    }
                    break;

                case 2: // Install button
                    {
                        if (!IsRunAsAdmin()) {
                            MessageBoxW(hwnd, 
                                L"This installer requires administrator privileges.\n"
                                L"Please right-click and select 'Run as administrator'.",
                                L"Administrator Rights Required",
                                MB_ICONEXCLAMATION);
                            break;
                        }

                        wchar_t path[MAX_PATH];
                        GetWindowTextW(hwndPath, path, MAX_PATH);
                        
                        try {
                            // Create directory
                            std::filesystem::create_directories(path);
                            
                            // Copy file
                            std::wstring source = L"nudhi.exe";
                            std::wstring dest = std::wstring(path) + L"\\nudhi.exe";
                            
                            if (CopyFileW(source.c_str(), dest.c_str(), FALSE)) {
                                // Modify PATH
                                HKEY hKey;
                                if (RegOpenKeyExW(HKEY_LOCAL_MACHINE,
                                    L"System\\CurrentControlSet\\Control\\Session Manager\\Environment",
                                    0, KEY_ALL_ACCESS, &hKey) == ERROR_SUCCESS) {
                                    
                                    wchar_t currentPath[2048];
                                    DWORD pathSize = sizeof(currentPath);
                                    
                                    if (RegQueryValueExW(hKey, L"Path", NULL, NULL,
                                        (LPBYTE)currentPath, &pathSize) == ERROR_SUCCESS) {
                                        
                                        std::wstring newPath = std::wstring(currentPath) + L";" + path;
                                        if (RegSetValueExW(hKey, L"Path", 0, REG_EXPAND_SZ,
                                            (LPBYTE)newPath.c_str(),
                                            (newPath.length() + 1) * sizeof(wchar_t)) == ERROR_SUCCESS) {
                                            
                                            SendMessageTimeout(HWND_BROADCAST, WM_SETTINGCHANGE,
                                                0, (LPARAM)L"Environment",
                                                SMTO_ABORTIFHUNG, 5000, NULL);
                                                
                                            MessageBoxW(hwnd, L"Installation Complete! you can close the window now.", L"Success", MB_OK);
                                        }
                                    }
                                    RegCloseKey(hKey);
                                }
                            } else {
                                MessageBoxW(hwnd, L"Failed to copy nudhi.exe", L"Error", MB_ICONERROR);
                            }
                        } catch (const std::exception& e) {
                            MessageBoxW(hwnd, L"Installation failed", L"Error", MB_ICONERROR);
                        }
                    }
                    break;
            }
            break;
        }

        case WM_DESTROY:
            PostQuitMessage(0);
            break;

        default:
            return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
    return 0;
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow) {
    hInst = hInstance;  // Store instance handle

    WNDCLASSEXW wc = {0};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);
    wc.lpszClassName = WINDOW_CLASS;

    if (!RegisterClassExW(&wc)) {
        MessageBoxW(NULL, L"Window Registration Failed!", L"Error", MB_ICONERROR);
        return 1;
    }

    HWND hwnd = CreateWindowExW(
        0,
        WINDOW_CLASS,
        L"Nudhi Lang Installer",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 450, 150,
        NULL, NULL, hInstance, NULL);

    if (!hwnd) {
        MessageBoxW(NULL, L"Window Creation Failed!", L"Error", MB_ICONERROR);
        return 1;
    }

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return (int)msg.wParam;
}