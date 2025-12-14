// BoingBallSaver.cpp — Windows screensaver with bouncing red/white checkered ball similar to Amiga BoingBall Demo
// Created by Sinphaltimus Exmortus aka AirTwerx and Copilot AI

#include <windows.h>
#include <GL/gl.h>
#include <GL/glu.h>
#include <cmath>
#include <string>
#include <mmsystem.h>
#include <cstdlib>   // for _wtoi
#include <cwchar>    // for wcslen
#include <commdlg.h> // ChooseColor

#pragma comment(lib, "winmm.lib")
#include "resource.h"

#pragma comment(lib, "opengl32.lib")
#pragma comment(lib, "glu32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "Comdlg32.lib")

HINSTANCE g_hInst;
HWND g_hWnd;
HDC g_hDC;
HGLRC g_hGL;
bool g_running = false;
bool g_preview = false;

GLuint g_checkerTex;

// Playback speed
float g_timeScale = 0.5f;

// World/physics
float BALL_RADIUS = 0.25f;
float RESTITUTION = 1.0f;
float GRAVITY = -9.8f;
float WALL_X;
float WALL_Z;
float FLOOR_Y;

// Position and velocities
float g_ballX;
float g_ballY;
float g_ballZ = 0.0f;
float g_vx = 0.8f;
float g_vy = 4.5f;
float g_vz = 0.0f;

// Spin control
float g_spinAngle = 0.0f;
float g_spinSpeed = 120.0f;
int   g_spinDir = 1;

LARGE_INTEGER g_freq, g_prev;

// Settings (state)
bool     g_floorShadowEnabled = true;
bool     g_wallShadowEnabled = true;
bool     g_gridEnabled = true;
bool     g_soundEnabled = true;
COLORREF g_bgColor = RGB(192, 192, 192);  // default gray
int      g_geometryMode = 1; // 1 = Classic, 0 = Smooth
bool     g_ballLightingEnabled = true;


// Defaults
const bool     DEFAULT_FLOOR_SHADOW = true;
const bool     DEFAULT_WALL_SHADOW = true;
const bool     DEFAULT_GRID = true;
const bool     DEFAULT_SOUND = true;
const COLORREF DEFAULT_BG_COLOR = RGB(192, 192, 192);
const int      DEFAULT_GEOMETRY_MODE = 1;  // Classic by default
const bool DEFAULT_BALL_LIGHTING = true;

// Registry path
static const wchar_t* kRegPath = L"Software\\AirTwerx\\BoingBallSaver";

// Registry helpers
static bool ReadBoolSetting(const wchar_t* name, bool defaultVal) {
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kRegPath, 0, KEY_READ, &hKey) != ERROR_SUCCESS) return defaultVal;
    DWORD val = 0, type = 0, size = sizeof(DWORD);
    LONG res = RegQueryValueExW(hKey, name, nullptr, &type, reinterpret_cast<LPBYTE>(&val), &size);
    RegCloseKey(hKey);
    if (res == ERROR_SUCCESS && type == REG_DWORD) return val != 0;
    return defaultVal;
}

static int ReadIntSetting(const wchar_t* name, int defaultVal) {
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kRegPath, 0, KEY_READ, &hKey) != ERROR_SUCCESS) return defaultVal;
    DWORD val = 0, type = 0, size = sizeof(DWORD);
    LONG res = RegQueryValueExW(hKey, name, nullptr, &type, reinterpret_cast<LPBYTE>(&val), &size);
    RegCloseKey(hKey);
    if (res == ERROR_SUCCESS && type == REG_DWORD) return static_cast<int>(val);
    return defaultVal;
}

static COLORREF ReadColorSetting(const wchar_t* name, COLORREF defaultVal) {
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kRegPath, 0, KEY_READ, &hKey) != ERROR_SUCCESS) return defaultVal;
    DWORD val = 0, type = 0, size = sizeof(DWORD);
    LONG res = RegQueryValueExW(hKey, name, nullptr, &type, reinterpret_cast<LPBYTE>(&val), &size);
    RegCloseKey(hKey);
    if (res == ERROR_SUCCESS && type == REG_DWORD) return static_cast<COLORREF>(val);
    return defaultVal;
}

static void WriteBoolSetting(const wchar_t* name, bool value) {
    HKEY hKey; DWORD disp = 0;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, kRegPath, 0, nullptr, 0, KEY_WRITE, nullptr, &hKey, &disp) != ERROR_SUCCESS) return;
    DWORD v = value ? 1 : 0;
    RegSetValueExW(hKey, name, 0, REG_DWORD, reinterpret_cast<const BYTE*>(&v), sizeof(DWORD));
    RegCloseKey(hKey);
}

static void WriteIntSetting(const wchar_t* name, int value) {
    HKEY hKey; DWORD disp = 0;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, kRegPath, 0, nullptr, 0, KEY_WRITE, nullptr, &hKey, &disp) != ERROR_SUCCESS) return;
    RegSetValueExW(hKey, name, 0, REG_DWORD, reinterpret_cast<const BYTE*>(&value), sizeof(DWORD));
    RegCloseKey(hKey);
}

static void WriteColorSetting(const wchar_t* name, COLORREF value) {
    HKEY hKey; DWORD disp = 0;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, kRegPath, 0, nullptr, 0, KEY_WRITE, nullptr, &hKey, &disp) != ERROR_SUCCESS) return;
    RegSetValueExW(hKey, name, 0, REG_DWORD, reinterpret_cast<const BYTE*>(&value), sizeof(DWORD));
    RegCloseKey(hKey);
}

static void QuitSaver() { g_running = false; PostMessage(g_hWnd, WM_CLOSE, 0, 0); }

static void CleanupGL() {
    if (g_checkerTex) { glDeleteTextures(1, &g_checkerTex); g_checkerTex = 0; }
    if (g_hGL) { wglMakeCurrent(NULL, NULL); wglDeleteContext(g_hGL); g_hGL = NULL; }
    if (g_hDC) { ReleaseDC(g_hWnd, g_hDC); g_hDC = NULL; }
}

static void MakeCheckerTexture() {
    const int TEX_SIZE = 128;
    unsigned char* data = new unsigned char[TEX_SIZE * TEX_SIZE * 3];
    for (int y = 0; y < TEX_SIZE; ++y) {
        for (int x = 0; x < TEX_SIZE; ++x) {
            int cx = x / (TEX_SIZE / 16);
            int cy = y / (TEX_SIZE / 8);
            bool red = ((cx + cy) % 2) == 0;
            unsigned char r = red ? 220 : 240;
            unsigned char g = red ? 30 : 240;
            unsigned char b = red ? 30 : 240;
            int i = (y * TEX_SIZE + x) * 3;
            data[i + 0] = r; data[i + 1] = g; data[i + 2] = b;
        }
    }
    glGenTextures(1, &g_checkerTex);
    glBindTexture(GL_TEXTURE_2D, g_checkerTex);
    gluBuild2DMipmaps(GL_TEXTURE_2D, GL_RGB, TEX_SIZE, TEX_SIZE, GL_RGB, GL_UNSIGNED_BYTE, data);
    delete[] data;

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
}

static void SetupGL(int w, int h) {
    PIXELFORMATDESCRIPTOR pfd = { sizeof(PIXELFORMATDESCRIPTOR), 1 };
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 24; pfd.cDepthBits = 24;
    g_hDC = GetDC(g_hWnd);
    int pf = ChoosePixelFormat(g_hDC, &pfd);
    SetPixelFormat(g_hDC, pf, &pfd);
    g_hGL = wglCreateContext(g_hDC);
    if (!g_hGL) {
        MessageBox(nullptr, L"Failed to create OpenGL context!", L"BoingBallSaver", MB_OK | MB_ICONERROR);
        return;
    }
    wglMakeCurrent(g_hDC, g_hGL);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    GLfloat lightDir[] = { -0.5f, 0.8f, 0.6f, 0.0f };
    glLightfv(GL_LIGHT0, GL_POSITION, lightDir);

    GLfloat globalAmbient[] = { 0.3f, 0.3f, 0.3f, 1.0f };
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, globalAmbient);

    GLfloat ambient[] = { 0.4f, 0.4f, 0.4f, 1.0f };
    glLightfv(GL_LIGHT0, GL_AMBIENT, ambient);

    MakeCheckerTexture();

    glViewport(0, 0, w, h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(45.0, (float)w / (float)h, 0.1, 50.0);

    // Compute dynamic bounds based on FOV and aspect ratio
    float fovRadians = 45.0f * (3.14159265f / 180.0f);
    float camDist = 2.0f; // matches glTranslatef(0,0,-2.0f)
    float aspect = (float)w / (float)h;

    float halfHeight = tanf(fovRadians / 2.0f) * camDist;
    float halfWidth = halfHeight * aspect;

    WALL_X = halfWidth;
    WALL_Z = halfWidth;
    FLOOR_Y = -halfHeight;

    QueryPerformanceFrequency(&g_freq);
    QueryPerformanceCounter(&g_prev);
}

// Reflect ball at side walls
static inline void reflectXAtWall() {
    if (g_ballX > WALL_X - BALL_RADIUS) {
        g_ballX = WALL_X - BALL_RADIUS;
        g_vx = -fabsf(g_vx);
        g_spinDir *= -1;
        if (g_soundEnabled) {
            PlaySound(MAKEINTRESOURCE(BOINGW), g_hInst, SND_RESOURCE | SND_ASYNC);
        }
    }
    else if (g_ballX < -WALL_X + BALL_RADIUS) {
        g_ballX = -WALL_X + BALL_RADIUS;
        g_vx = +fabsf(g_vx);
        g_spinDir *= -1;
        if (g_soundEnabled) {
            PlaySound(MAKEINTRESOURCE(BOINGW), g_hInst, SND_RESOURCE | SND_ASYNC);
        }
    }
}

static void UpdatePhysics(float dt) {
    g_spinAngle += g_spinDir * g_spinSpeed * dt;
    if (g_spinAngle > 360.0f) g_spinAngle -= 360.0f;
    if (g_spinAngle < 0.0f)   g_spinAngle += 360.0f;

    g_vy += GRAVITY * dt;
    g_ballX += g_vx * dt;
    g_ballY += g_vy * dt;
    g_ballZ += g_vz * dt;

    // Floor bounce
    if (g_ballY < FLOOR_Y + BALL_RADIUS) {
        g_ballY = FLOOR_Y + BALL_RADIUS;
        g_vy = 4.5f;
        if (g_soundEnabled) {
            PlaySound(MAKEINTRESOURCE(BOINGF), g_hInst, SND_RESOURCE | SND_ASYNC);
        }
    }

    reflectXAtWall();

    // Optional Z walls
    if (g_ballZ > WALL_Z - BALL_RADIUS) {
        g_ballZ = WALL_Z - BALL_RADIUS;
        g_vz = -fabsf(g_vz);
    }
    else if (g_ballZ < -WALL_Z + BALL_RADIUS) {
        g_ballZ = -WALL_Z + BALL_RADIUS;
        g_vz = +fabsf(g_vz);
    }
}

static void DrawSphere(float r) {
    GLUquadric* quad = gluNewQuadric();
    gluQuadricTexture(quad, GL_TRUE);
    glBindTexture(GL_TEXTURE_2D, g_checkerTex);

    int slices, stacks;
    if (g_geometryMode == 1) {
        // Classic Amiga tessellation: 16 longitude × 8 latitude
        slices = 16;
        stacks = 8;
    }
    else {
        // Smooth sphere
        slices = 64;
        stacks = 32;
    }

    gluSphere(quad, r, slices, stacks);
    gluDeleteQuadric(quad);
}

static void RenderFrame() {
    LARGE_INTEGER now; QueryPerformanceCounter(&now);
    float dt = (float)(now.QuadPart - g_prev.QuadPart) / (float)g_freq.QuadPart;
    g_prev = now;
    if (dt > 0.05f) dt = 0.05f;
    UpdatePhysics(dt * g_timeScale);

    glClearColor(
        GetRValue(g_bgColor) / 255.0f,
        GetGValue(g_bgColor) / 255.0f,
        GetBValue(g_bgColor) / 255.0f,
        1.0f
    );

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glTranslatef(0, 0, -2.0f);

    glDisable(GL_LIGHTING);
    glColor3f(0.3f, 0.6f, 1.0f);
    glLineWidth(2.0f);

    // Grid floor + wall
    if (g_gridEnabled) {
        // Floor grid
        glBegin(GL_LINES);
        for (float i = -1.0f; i <= 1.0f; i += 0.2f) {
            glVertex3f(i, FLOOR_Y, -1.0f);
            glVertex3f(i, FLOOR_Y, 1.0f);
            glVertex3f(-1.0f, FLOOR_Y, i);
            glVertex3f(1.0f, FLOOR_Y, i);
        }
        glEnd();

        // Wall grid
        glBegin(GL_LINES);
        for (float x = -1.0f; x <= 1.0f; x += 0.2f) {
            glVertex3f(x, FLOOR_Y, -1.0f);
            glVertex3f(x, FLOOR_Y + 2.0f, -1.0f);
        }
        for (float y = FLOOR_Y; y <= FLOOR_Y + 2.0f; y += 0.2f) {
            glVertex3f(-1.0f, y, -1.0f);
            glVertex3f(1.0f, y, -1.0f);
        }
        glEnd();
    }

    // Floor shadow
    if (g_floorShadowEnabled) {
        glDisable(GL_LIGHTING);
        glColor4f(0.0f, 0.0f, 0.0f, 0.4f);
        glPushMatrix();
        glTranslatef(g_ballX, FLOOR_Y + 0.001f, g_ballZ);
        glScalef(1.0f, 0.1f, 1.0f);
        DrawSphere(BALL_RADIUS);
        glPopMatrix();
    }

    // Wall shadow
    if (g_wallShadowEnabled) {
        glDisable(GL_LIGHTING);
        glColor4f(0.0f, 0.0f, 0.0f, 0.3f);
        glPushMatrix();
        glTranslatef(g_ballX, g_ballY, -1.0f);
        glScalef(1.0f, 1.0f, 0.1f);
        DrawSphere(BALL_RADIUS);
        glPopMatrix();
    }

    glEnable(GL_LIGHTING);

    // Ball
    glPushMatrix();
    glTranslatef(g_ballX, g_ballY, g_ballZ);
    glRotatef(90.0f, 1, 0, 0);
    glRotatef(-15.0f, 0, 1, 0);
    glRotatef(g_spinAngle, 0, 0, 1);
    if (!g_ballLightingEnabled) {
        glDisable(GL_LIGHTING);
        glColor3f(1.0f, 1.0f, 1.0f); // full bright texture
    }

    DrawSphere(BALL_RADIUS);

    if (!g_ballLightingEnabled) {
        glEnable(GL_LIGHTING);
    }

    glPopMatrix();

    SwapBuffers(g_hDC);
}

// Config dialog (single, with Restore Defaults)
static INT_PTR CALLBACK ConfigDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_INITDIALOG:
        // Load persisted settings (or defaults if missing)
        g_floorShadowEnabled = ReadBoolSetting(L"FloorShadow", DEFAULT_FLOOR_SHADOW);
        g_wallShadowEnabled = ReadBoolSetting(L"WallShadow", DEFAULT_WALL_SHADOW);
        g_gridEnabled = ReadBoolSetting(L"Grid", DEFAULT_GRID);
        g_soundEnabled = ReadBoolSetting(L"Sound", DEFAULT_SOUND);
        g_bgColor = ReadColorSetting(L"BackgroundColor", DEFAULT_BG_COLOR);
        g_geometryMode = ReadIntSetting(L"GeometryMode", DEFAULT_GEOMETRY_MODE);
        g_ballLightingEnabled = ReadBoolSetting(L"BallLighting", DEFAULT_BALL_LIGHTING);
        CheckDlgButton(hDlg, IDC_BALLLIGHTING, g_ballLightingEnabled ? BST_CHECKED : BST_UNCHECKED);


        // Reflect in UI
        CheckDlgButton(hDlg, IDC_FLOORSHADOW, g_floorShadowEnabled ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(hDlg, IDC_WALLSHADOW, g_wallShadowEnabled ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(hDlg, IDC_GRID, g_gridEnabled ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(hDlg, IDC_SOUND, g_soundEnabled ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(hDlg, IDC_GEOMETRY, g_geometryMode ? BST_CHECKED : BST_UNCHECKED);
        return TRUE;

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDC_BGCOLOR: {
            static COLORREF customColors[16] = { 0 };  // persists across opens
            CHOOSECOLOR cc = { sizeof(CHOOSECOLOR) };
            cc.hwndOwner = hDlg;
            cc.lpCustColors = customColors;
            cc.rgbResult = g_bgColor;
            cc.Flags = CC_RGBINIT | CC_FULLOPEN;

            if (ChooseColor(&cc)) {
                g_bgColor = cc.rgbResult;
                // Optional: live preview
                if (g_hWnd) InvalidateRect(g_hWnd, nullptr, FALSE);
            }
            return TRUE;
        }

        case IDC_RESTORE:
            // Revert to coded defaults
            g_floorShadowEnabled = DEFAULT_FLOOR_SHADOW;
            g_wallShadowEnabled = DEFAULT_WALL_SHADOW;
            g_gridEnabled = DEFAULT_GRID;
            g_soundEnabled = DEFAULT_SOUND;
            g_bgColor = DEFAULT_BG_COLOR;
            g_geometryMode = DEFAULT_GEOMETRY_MODE;
            g_ballLightingEnabled = DEFAULT_BALL_LIGHTING;
            CheckDlgButton(hDlg, IDC_BALLLIGHTING, g_ballLightingEnabled ? BST_CHECKED : BST_UNCHECKED);

            // Update UI
            CheckDlgButton(hDlg, IDC_FLOORSHADOW, g_floorShadowEnabled ? BST_CHECKED : BST_UNCHECKED);
            CheckDlgButton(hDlg, IDC_WALLSHADOW, g_wallShadowEnabled ? BST_CHECKED : BST_UNCHECKED);
            CheckDlgButton(hDlg, IDC_GRID, g_gridEnabled ? BST_CHECKED : BST_UNCHECKED);
            CheckDlgButton(hDlg, IDC_SOUND, g_soundEnabled ? BST_CHECKED : BST_UNCHECKED);
            CheckDlgButton(hDlg, IDC_GEOMETRY, g_geometryMode ? BST_CHECKED : BST_UNCHECKED);
            return TRUE;

        case IDOK:
            // Read UI state
            g_floorShadowEnabled = (IsDlgButtonChecked(hDlg, IDC_FLOORSHADOW) == BST_CHECKED);
            g_wallShadowEnabled = (IsDlgButtonChecked(hDlg, IDC_WALLSHADOW) == BST_CHECKED);
            g_gridEnabled = (IsDlgButtonChecked(hDlg, IDC_GRID) == BST_CHECKED);
            g_soundEnabled = (IsDlgButtonChecked(hDlg, IDC_SOUND) == BST_CHECKED);
            g_geometryMode = (IsDlgButtonChecked(hDlg, IDC_GEOMETRY) == BST_CHECKED) ? 1 : 0;
            g_ballLightingEnabled = (IsDlgButtonChecked(hDlg, IDC_BALLLIGHTING) == BST_CHECKED);
            WriteBoolSetting(L"BallLighting", g_ballLightingEnabled);

            // Persist
            WriteBoolSetting(L"FloorShadow", g_floorShadowEnabled);
            WriteBoolSetting(L"WallShadow", g_wallShadowEnabled);
            WriteBoolSetting(L"Grid", g_gridEnabled);
            WriteBoolSetting(L"Sound", g_soundEnabled);
            WriteColorSetting(L"BackgroundColor", g_bgColor);
            WriteIntSetting(L"GeometryMode", g_geometryMode);

            EndDialog(hDlg, IDOK);
            return TRUE;

        case IDCANCEL:
            EndDialog(hDlg, IDCANCEL);
            return TRUE;
        }
        break;
    }
    return FALSE;
}

static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: {
        // Load all settings at startup so runtime matches dialog state
        g_floorShadowEnabled = ReadBoolSetting(L"FloorShadow", DEFAULT_FLOOR_SHADOW);
        g_wallShadowEnabled = ReadBoolSetting(L"WallShadow", DEFAULT_WALL_SHADOW);
        g_gridEnabled = ReadBoolSetting(L"Grid", DEFAULT_GRID);
        g_soundEnabled = ReadBoolSetting(L"Sound", DEFAULT_SOUND);
        g_bgColor = ReadColorSetting(L"BackgroundColor", DEFAULT_BG_COLOR);
        g_geometryMode = ReadIntSetting(L"GeometryMode", DEFAULT_GEOMETRY_MODE);
        g_ballLightingEnabled = ReadBoolSetting(L"BallLighting", DEFAULT_BALL_LIGHTING);

        if (g_soundEnabled) {
            PlaySound(MAKEINTRESOURCE(BOINGF), g_hInst, SND_RESOURCE | SND_ASYNC);
        }

        RECT r; GetClientRect(hWnd, &r);
        g_hWnd = hWnd;
        SetupGL(r.right, r.bottom);

        g_ballX = -WALL_X + BALL_RADIUS;
        g_ballY = FLOOR_Y + BALL_RADIUS;

        g_running = true;
        if (!g_preview) ShowCursor(FALSE);
    } return 0;

    case WM_PAINT:
        if (g_running) RenderFrame();
        ValidateRect(hWnd, NULL);
        return 0;

    case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN:
    case WM_MBUTTONDOWN:
    case WM_MOUSEWHEEL:
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:   // catches Alt, F10, etc.
        if (!g_preview) QuitSaver();
        return 0;

    case WM_DESTROY:
        CleanupGL(); 
        if (!g_preview) ShowCursor(TRUE);
        PostQuitMessage(0);
        return 0;

    default:
        return DefWindowProc(hWnd, msg, wParam, lParam);
    }
}

static HWND CreateSaverWindow(HINSTANCE hInst, HWND hParent, bool preview) {
    WNDCLASS wc = {};
    wc.style = CS_OWNDC;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.lpszClassName = L"BoingBallSaver";
    RegisterClass(&wc);

    RECT rc;
    if (preview && hParent) {
        GetClientRect(hParent, &rc);
        g_preview = true;
        return CreateWindowEx(0, wc.lpszClassName, L"", WS_CHILD | WS_VISIBLE,
            rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top,
            hParent, NULL, hInst, NULL);
    }
    else {
        GetWindowRect(GetDesktopWindow(), &rc);
        HWND hWnd = CreateWindowEx(WS_EX_TOPMOST, wc.lpszClassName, L"", WS_POPUP,
            rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top,
            NULL, NULL, hInst, NULL);
        if (!hWnd) {
            MessageBox(nullptr, L"Failed to create window!", L"BoingBallSaver", MB_OK | MB_ICONERROR);
        }
        return hWnd;
    }
}

int APIENTRY wWinMain(HINSTANCE hInst, HINSTANCE, LPWSTR cmd, int) {
    g_hInst = hInst;

    // Settings dialog
    if (cmd && (wcsstr(cmd, L"/c") || wcsstr(cmd, L"-c"))) {
        DialogBoxParamW(hInst, MAKEINTRESOURCE(IDD_CONFIG), nullptr, ConfigDlgProc, 0);
        return 0;
    }

    // Parse preview handle (/p <HWND>)
    HWND hWndParent = nullptr;
    if (cmd && (wcsstr(cmd, L"/p") || wcsstr(cmd, L"-p"))) {
        wchar_t* hwndStr = wcsstr(cmd, L"/p");
        if (!hwndStr) hwndStr = wcsstr(cmd, L"-p");
        if (hwndStr && wcslen(hwndStr) > 2) {
            hwndStr += 2;
            while (*hwndStr == L' ') hwndStr++;
            hWndParent = (HWND)(uintptr_t)_wtoi(hwndStr);
        }
    }

    g_hWnd = CreateSaverWindow(hInst, hWndParent, hWndParent != nullptr);
    if (!g_hWnd) {
        MessageBox(nullptr, L"Failed to create window!", L"BoingBallSaver", MB_OK | MB_ICONERROR);
        return 1;
    }

    ShowWindow(g_hWnd, SW_SHOW);
    UpdateWindow(g_hWnd);

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
        if (!g_running) break;
        InvalidateRect(g_hWnd, nullptr, FALSE);
    }
    return 0;
}

// END OF FILE
