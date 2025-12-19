// BoingBallSaver.cpp — Windows screensaver with bouncing red/white checkered ball similar to Amiga BoingBall Demo
// Created by Sinphaltimus Exmortus aka AirTwerx aka Lenny Rivera and Copilot AI
// Clean, consistent, multi-monitor GL context management, no WM_PAINT rendering, no wglShareLists, no hidden master — per-window resources only.

#include <windows.h>
#include <GL/gl.h>
#include <GL/glu.h>
#include <cmath>
#include <string>
#include <mmsystem.h>
#include <cstdlib>
#include <cwchar>
#include <commdlg.h>
#include <cstdint>
#include <vector>

#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif

#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "opengl32.lib")
#pragma comment(lib, "glu32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "Comdlg32.lib")
#pragma comment(lib, "Advapi32.lib")

#include "resource.h"

// Class name (versioned and constant for this build)
static const wchar_t* kSaverClassName = L"BoingBallSaver_v3";

// Global handles and state
HINSTANCE g_hInst = nullptr;
HWND      g_hWnd = nullptr;   // Primary window (first created), for messages and Quit
HDC       g_hDC = nullptr;    // DC for primary window (no rendering here)
bool      g_running = false;
bool      g_preview = false;
bool      g_soundPlayedThisFrame = false;
bool      g_cursorHidden = false;

// Physics constants
float g_timeScale = 0.5f;
float BALL_RADIUS = 0.25f;
float GRAVITY = -9.8f;

// Global ball state (used in Single and Replicated modes)
float g_ballX = 0.0f, g_ballY = 0.0f, g_ballZ = 0.0f;
float g_vx = 0.8f, g_vy = 4.5f, g_vz = 0.0f;
float g_spinAngle = 0.0f, g_spinSpeed = 120.0f;
int   g_spinDir = 1;

// Global bounds used for global physics (replicated/single)
float g_WALL_X = 1.0f, g_WALL_Z = 1.0f, g_FLOOR_Y = -1.0f;

// Timing
LARGE_INTEGER g_freq = {}, g_prev = {};

// User settings
bool     g_floorShadowEnabled = true;
bool     g_wallShadowEnabled = true;
bool     g_gridEnabled = true;
bool     g_soundEnabled = true;
COLORREF g_bgColor = RGB(192, 192, 192);
int      g_geometryMode = 1;  // 1 = low, 0 = high (kept same semantics as prior)
bool     g_ballLightingEnabled = true;
int      g_multiMonitorMode = 0;  // 0=Single, 1=Extended, 2=Replicated, 3=Unified

// Defaults
const bool     DEFAULT_FLOOR_SHADOW = true;
const bool     DEFAULT_WALL_SHADOW = true;
const bool     DEFAULT_GRID = true;
const bool     DEFAULT_SOUND = true;
const COLORREF DEFAULT_BG_COLOR = RGB(192, 192, 192);
const int      DEFAULT_GEOMETRY_MODE = 1;
const bool     DEFAULT_BALL_LIGHTING = true;
const int      DEFAULT_MULTI_MONITOR_MODE = 0;

// Registry path
static const wchar_t* kRegPath = L"Software\\AirTwerx\\BoingBallSaver";

static bool ReadBoolSetting(const wchar_t* name, bool defaultVal) {
    HKEY hKey;
    DWORD val = 0, type = 0, size = sizeof(DWORD);

    // Try 64‑bit hive first
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kRegPath, 0,
        KEY_READ | KEY_WOW64_64KEY, &hKey) == ERROR_SUCCESS) {
        if (RegQueryValueExW(hKey, name, nullptr, &type,
            reinterpret_cast<LPBYTE>(&val), &size) == ERROR_SUCCESS && type == REG_DWORD) {
            RegCloseKey(hKey);
            return val != 0;
        }
        RegCloseKey(hKey);
    }

    // Fall back to 32‑bit hive
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kRegPath, 0,
        KEY_READ | KEY_WOW64_32KEY, &hKey) == ERROR_SUCCESS) {
        if (RegQueryValueExW(hKey, name, nullptr, &type,
            reinterpret_cast<LPBYTE>(&val), &size) == ERROR_SUCCESS && type == REG_DWORD) {
            RegCloseKey(hKey);
            return val != 0;
        }
        RegCloseKey(hKey);
    }

    return defaultVal;
}

static int ReadIntSetting(const wchar_t* name, int defaultVal) {
    HKEY hKey;
    DWORD val = 0, type = 0, size = sizeof(DWORD);

    // Try 64‑bit hive first
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kRegPath, 0,
        KEY_READ | KEY_WOW64_64KEY, &hKey) == ERROR_SUCCESS) {
        if (RegQueryValueExW(hKey, name, nullptr, &type,
            reinterpret_cast<LPBYTE>(&val), &size) == ERROR_SUCCESS && type == REG_DWORD) {
            RegCloseKey(hKey);
            return static_cast<int>(val);
        }
        RegCloseKey(hKey);
    }

    // Fall back to 32‑bit hive
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kRegPath, 0,
        KEY_READ | KEY_WOW64_32KEY, &hKey) == ERROR_SUCCESS) {
        if (RegQueryValueExW(hKey, name, nullptr, &type,
            reinterpret_cast<LPBYTE>(&val), &size) == ERROR_SUCCESS && type == REG_DWORD) {
            RegCloseKey(hKey);
            return static_cast<int>(val);
        }
        RegCloseKey(hKey);
    }

    return defaultVal;
}


static COLORREF ReadColorSetting(const wchar_t* name, COLORREF defaultVal) {
    HKEY hKey;
    DWORD val = 0, type = 0, size = sizeof(DWORD);

    // Try 64‑bit hive first
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kRegPath, 0,
        KEY_READ | KEY_WOW64_64KEY, &hKey) == ERROR_SUCCESS) {
        if (RegQueryValueExW(hKey, name, nullptr, &type,
            reinterpret_cast<LPBYTE>(&val), &size) == ERROR_SUCCESS && type == REG_DWORD) {
            RegCloseKey(hKey);
            return static_cast<COLORREF>(val);
        }
        RegCloseKey(hKey);
    }

    // Fall back to 32‑bit hive
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kRegPath, 0,
        KEY_READ | KEY_WOW64_32KEY, &hKey) == ERROR_SUCCESS) {
        if (RegQueryValueExW(hKey, name, nullptr, &type,
            reinterpret_cast<LPBYTE>(&val), &size) == ERROR_SUCCESS && type == REG_DWORD) {
            RegCloseKey(hKey);
            return static_cast<COLORREF>(val);
        }
        RegCloseKey(hKey);
    }

    return defaultVal;
}

static void WriteBoolSetting(const wchar_t* name, bool value) {
    HKEY hKey; DWORD disp = 0;
    DWORD v = value ? 1 : 0;

    // Write to 64‑bit hive
    if (RegCreateKeyExW(HKEY_CURRENT_USER, kRegPath, 0, nullptr, 0,
        KEY_WRITE | KEY_WOW64_64KEY, nullptr, &hKey, &disp) == ERROR_SUCCESS) {
        RegSetValueExW(hKey, name, 0, REG_DWORD,
            reinterpret_cast<const BYTE*>(&v), sizeof(DWORD));
        RegCloseKey(hKey);
    }

    // Write to 32‑bit hive
    if (RegCreateKeyExW(HKEY_CURRENT_USER, kRegPath, 0, nullptr, 0,
        KEY_WRITE | KEY_WOW64_32KEY, nullptr, &hKey, &disp) == ERROR_SUCCESS) {
        RegSetValueExW(hKey, name, 0, REG_DWORD,
            reinterpret_cast<const BYTE*>(&v), sizeof(DWORD));
        RegCloseKey(hKey);
    }
}

static void WriteIntSetting(const wchar_t* name, int value) {
    HKEY hKey; DWORD disp = 0;

    // Write to 64‑bit hive
    if (RegCreateKeyExW(HKEY_CURRENT_USER, kRegPath, 0, nullptr, 0,
        KEY_WRITE | KEY_WOW64_64KEY, nullptr, &hKey, &disp) == ERROR_SUCCESS) {
        RegSetValueExW(hKey, name, 0, REG_DWORD,
            reinterpret_cast<const BYTE*>(&value), sizeof(DWORD));
        RegCloseKey(hKey);
    }

    // Write to 32‑bit hive
    if (RegCreateKeyExW(HKEY_CURRENT_USER, kRegPath, 0, nullptr, 0,
        KEY_WRITE | KEY_WOW64_32KEY, nullptr, &hKey, &disp) == ERROR_SUCCESS) {
        RegSetValueExW(hKey, name, 0, REG_DWORD,
            reinterpret_cast<const BYTE*>(&value), sizeof(DWORD));
        RegCloseKey(hKey);
    }
}


static void WriteColorSetting(const wchar_t* name, COLORREF value) {
    HKEY hKey; DWORD disp = 0;

    // Write to 64‑bit hive
    if (RegCreateKeyExW(HKEY_CURRENT_USER, kRegPath, 0, nullptr, 0,
        KEY_WRITE | KEY_WOW64_64KEY, nullptr, &hKey, &disp) == ERROR_SUCCESS) {
        RegSetValueExW(hKey, name, 0, REG_DWORD,
            reinterpret_cast<const BYTE*>(&value), sizeof(DWORD));
        RegCloseKey(hKey);
    }

    // Write to 32‑bit hive
    if (RegCreateKeyExW(HKEY_CURRENT_USER, kRegPath, 0, nullptr, 0,
        KEY_WRITE | KEY_WOW64_32KEY, nullptr, &hKey, &disp) == ERROR_SUCCESS) {
        RegSetValueExW(hKey, name, 0, REG_DWORD,
            reinterpret_cast<const BYTE*>(&value), sizeof(DWORD));
        RegCloseKey(hKey);
    }
}

// Explicitly set monitor mode radios without relying on resource grouping
static void SetMonitorModeRadios(HWND hDlg, int mode) {
    // Uncheck all
    SendDlgItemMessage(hDlg, IDC_MONITOR_SINGLE, BM_SETCHECK, BST_UNCHECKED, 0);
    SendDlgItemMessage(hDlg, IDC_MONITOR_EXTENDED, BM_SETCHECK, BST_UNCHECKED, 0);
    SendDlgItemMessage(hDlg, IDC_MONITOR_REPLICATED, BM_SETCHECK, BST_UNCHECKED, 0);
    SendDlgItemMessage(hDlg, IDC_MONITOR_UNIFIED, BM_SETCHECK, BST_UNCHECKED, 0);

    // Check the selected one
    int id =
        (mode == 0) ? IDC_MONITOR_SINGLE :
        (mode == 1) ? IDC_MONITOR_EXTENDED :
        (mode == 2) ? IDC_MONITOR_REPLICATED :
        (mode == 3) ? IDC_MONITOR_UNIFIED : IDC_MONITOR_SINGLE;

    SendDlgItemMessage(hDlg, id, BM_SETCHECK, BST_CHECKED, 0);
}

/* Debug helper : popup the current monitor mode with a label********************************************************
static void DebugMode(const wchar_t* label) {
    wchar_t buf[128];
    swprintf(buf, 128, L"%s: g_multiMonitorMode=%d", label, g_multiMonitorMode);
    MessageBoxW(nullptr, buf, L"BoingBallSaver Debug", MB_OK | MB_ICONINFORMATION);
}
*/
// Load all settings once, before any window creation
static void LoadSettingsFromRegistry() {
    g_floorShadowEnabled = ReadBoolSetting(L"FloorShadow", DEFAULT_FLOOR_SHADOW);
    g_wallShadowEnabled = ReadBoolSetting(L"WallShadow", DEFAULT_WALL_SHADOW);
    g_gridEnabled = ReadBoolSetting(L"Grid", DEFAULT_GRID);
    g_soundEnabled = ReadBoolSetting(L"Sound", DEFAULT_SOUND);
    g_bgColor = ReadColorSetting(L"BackgroundColor", DEFAULT_BG_COLOR);
    g_geometryMode = ReadIntSetting(L"GeometryMode", DEFAULT_GEOMETRY_MODE);
    g_ballLightingEnabled = ReadBoolSetting(L"BallLighting", DEFAULT_BALL_LIGHTING);
    g_multiMonitorMode = ReadIntSetting(L"MultiMonitorMode", DEFAULT_MULTI_MONITOR_MODE);
}

static void QuitSaver() {
    g_running = false;
    if (!g_preview && g_cursorHidden) { ShowCursor(TRUE); g_cursorHidden = false; }
    PostMessage(g_hWnd, WM_CLOSE, 0, 0);
}

// Pixel format helper
static void SetWindowPixelFormat(HDC hdc) {
    PIXELFORMATDESCRIPTOR pfd = {};
    pfd.nSize = sizeof(PIXELFORMATDESCRIPTOR);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 24;
    pfd.cDepthBits = 24;
    pfd.cStencilBits = 8;

    int pf = ChoosePixelFormat(hdc, &pfd);
    if (pf == 0) return;
    SetPixelFormat(hdc, pf, &pfd);
}

// Per-monitor window structure (per-context resources)
struct MonitorWindow {
    HWND   hWnd = nullptr;
    HDC    hDC = nullptr;
    HGLRC  hGL = nullptr;

    // Per-window GL resources
    GLuint     checkerTex = 0;
    GLUquadric* quadric = nullptr;

    // Per-window world bounds (derived from viewport)
    float wallX = 1.0f, wallZ = 1.0f, floorY = -1.0f;

    // Per-window ball state
    float ballX = -0.5f, ballY = 0.0f, ballZ = 0.0f;
    float vx = 0.8f, vy = 4.5f, vz = 0.0f;
    float spinAngle = 0.0f;
    int   spinDir = 1;
};

std::vector<MonitorWindow> g_monitorWindows;

// Texture creation (for current context)
static GLuint MakeCheckerTexture() {
    const int TEX_SIZE = 128;
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    unsigned char* data = new unsigned char[TEX_SIZE * TEX_SIZE * 3];
    for (int y = 0; y < TEX_SIZE; ++y) {
        for (int x = 0; x < TEX_SIZE; ++x) {
            int cx = x / (TEX_SIZE / 16), cy = y / (TEX_SIZE / 8);
            bool red = ((cx + cy) % 2) == 0;
            unsigned char r = red ? 220 : 240;
            unsigned char g = red ? 30 : 240;
            unsigned char b = red ? 30 : 240;
            int i = (y * TEX_SIZE + x) * 3;
            data[i] = r; data[i + 1] = g; data[i + 2] = b;
        }
    }
    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    gluBuild2DMipmaps(GL_TEXTURE_2D, GL_RGB, TEX_SIZE, TEX_SIZE, GL_RGB, GL_UNSIGNED_BYTE, data);
    delete[] data;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    return tex;
}

// Apply viewport/projection and compute per-window bounds
static void ApplyViewportAndProjection(MonitorWindow& mw, int w, int h) {
    if (w <= 0) w = 1;
    if (h <= 0) h = 1;

    glViewport(0, 0, w, h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(45.0, (float)w / (float)h, 0.1, 50.0);

    float fovRadians = 45.0f * (3.14159265f / 180.0f);
    float camDist = 2.0f;
    float aspect = (float)w / (float)h;

    float halfHeight = tanf(fovRadians / 2.0f) * camDist;
    float halfWidth = halfHeight * aspect;

    mw.wallX = halfWidth;
    mw.wallZ = halfWidth;
    mw.floorY = -halfHeight;
}

// Setup GL state for a window/context and compute bounds
static void SetupGL(MonitorWindow& mw, int w, int h) {
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

    // Texture (create per-context)
    mw.checkerTex = MakeCheckerTexture();
    glBindTexture(GL_TEXTURE_2D, mw.checkerTex);

    // Quadric (per-context)
    mw.quadric = gluNewQuadric();
    gluQuadricTexture(mw.quadric, GL_TRUE);
    gluQuadricNormals(mw.quadric, GLU_SMOOTH);

    // Projection and bounds
    ApplyViewportAndProjection(mw, w, h);
}

// Initialize high-resolution timer
static void InitTimer() {
    QueryPerformanceFrequency(&g_freq);
    QueryPerformanceCounter(&g_prev);
}

// Timing
static float ComputeDeltaTime() {
    LARGE_INTEGER now; QueryPerformanceCounter(&now);
    float dt = (float)(now.QuadPart - g_prev.QuadPart) / (float)g_freq.QuadPart;
    g_prev = now;
    if (dt > 0.05f) dt = 0.05f;
    return dt;
}

// Global physics helpers (use global bounds)
static inline void reflectXAtWallGlobal() {
    if (g_ballX > g_WALL_X - BALL_RADIUS) {
        g_ballX = g_WALL_X - BALL_RADIUS;
        g_vx = -fabsf(g_vx);
        g_spinDir *= -1;
        if (g_soundEnabled && !g_soundPlayedThisFrame) {
            PlaySound(MAKEINTRESOURCE(BOINGW), g_hInst, SND_RESOURCE | SND_ASYNC);
            g_soundPlayedThisFrame = true;
        }
    }
    else if (g_ballX < -g_WALL_X + BALL_RADIUS) {
        g_ballX = -g_WALL_X + BALL_RADIUS;
        g_vx = +fabsf(g_vx);
        g_spinDir *= -1;
        if (g_soundEnabled && !g_soundPlayedThisFrame) {
            PlaySound(MAKEINTRESOURCE(BOINGW), g_hInst, SND_RESOURCE | SND_ASYNC);
            g_soundPlayedThisFrame = true;
        }
    }
}

static void UpdatePhysicsGlobal(float dt) {
    g_spinAngle += g_spinDir * g_spinSpeed * dt;
    if (g_spinAngle > 360.0f) g_spinAngle -= 360.0f;
    if (g_spinAngle < 0.0f)   g_spinAngle += 360.0f;

    g_vy += GRAVITY * dt;
    g_ballX += g_vx * dt;
    g_ballY += g_vy * dt;
    g_ballZ += g_vz * dt;

    if (g_ballY < g_FLOOR_Y + BALL_RADIUS) {
        g_ballY = g_FLOOR_Y + BALL_RADIUS;
        g_vy = 4.5f;
        if (g_soundEnabled && !g_soundPlayedThisFrame) {
            PlaySound(MAKEINTRESOURCE(BOINGF), g_hInst, SND_RESOURCE | SND_ASYNC);
            g_soundPlayedThisFrame = true;
        }
    }

    reflectXAtWallGlobal();

    if (g_ballZ > g_WALL_Z - BALL_RADIUS) {
        g_ballZ = g_WALL_Z - BALL_RADIUS;
        g_vz = -fabsf(g_vz);
    }
    else if (g_ballZ < -g_WALL_Z + BALL_RADIUS) {
        g_ballZ = -g_WALL_Z + BALL_RADIUS;
        g_vz = +fabsf(g_vz);
    }
}

// Per-window physics (use per-window bounds)
static void UpdatePhysicsMW(MonitorWindow& mw, float dt) {
    mw.spinAngle += mw.spinDir * g_spinSpeed * dt;
    if (mw.spinAngle > 360.0f) mw.spinAngle -= 360.0f;
    if (mw.spinAngle < 0.0f)   mw.spinAngle += 360.0f;

    mw.vy += GRAVITY * dt;
    mw.ballX += mw.vx * dt;
    mw.ballY += mw.vy * dt;
    mw.ballZ += mw.vz * dt;

    if (mw.ballY < mw.floorY + BALL_RADIUS) {
        mw.ballY = mw.floorY + BALL_RADIUS;
        mw.vy = 4.5f;
        if (g_soundEnabled && !g_soundPlayedThisFrame) {
            PlaySound(MAKEINTRESOURCE(BOINGF), g_hInst, SND_RESOURCE | SND_ASYNC);
            g_soundPlayedThisFrame = true;
        }
    }

    if (mw.ballX > mw.wallX - BALL_RADIUS) {
        mw.ballX = mw.wallX - BALL_RADIUS;
        mw.vx = -fabsf(mw.vx);
        mw.spinDir *= -1;
        if (g_soundEnabled && !g_soundPlayedThisFrame) {
            PlaySound(MAKEINTRESOURCE(BOINGW), g_hInst, SND_RESOURCE | SND_ASYNC);
            g_soundPlayedThisFrame = true;
        }
    }
    else if (mw.ballX < -mw.wallX + BALL_RADIUS) {
        mw.ballX = -mw.wallX + BALL_RADIUS;
        mw.vx = +fabsf(mw.vx);
        mw.spinDir *= -1;
        if (g_soundEnabled && !g_soundPlayedThisFrame) {
            PlaySound(MAKEINTRESOURCE(BOINGW), g_hInst, SND_RESOURCE | SND_ASYNC);
            g_soundPlayedThisFrame = true;
        }
    }

    if (mw.ballZ > mw.wallZ - BALL_RADIUS) {
        mw.ballZ = mw.wallZ - BALL_RADIUS;
        mw.vz = -fabsf(mw.vz);
    }
    else if (mw.ballZ < -mw.wallZ + BALL_RADIUS) {
        mw.ballZ = -mw.wallZ + BALL_RADIUS;
        mw.vz = +fabsf(mw.vz);
    }
}

// Sphere draw (per-window resources)
static void DrawSphere(const MonitorWindow& mw, float r) {
    glBindTexture(GL_TEXTURE_2D, mw.checkerTex);
    int slices = (g_geometryMode == 1) ? 16 : 64;
    int stacks = (g_geometryMode == 1) ? 8 : 32;
    gluSphere(mw.quadric, r, slices, stacks);
}

// Per-monitor render
static void RenderFrameMonitor(MonitorWindow& mw, bool useGlobalState, float dt) {
    if (!wglMakeCurrent(mw.hDC, mw.hGL)) {
        return; // Skip this monitor this frame if context couldn't be made current
    }

    // Ensure per-context resources are alive and bound
    if (mw.checkerTex == 0 || !glIsTexture(mw.checkerTex)) {
        // Recreate texture in this context if it was lost
        mw.checkerTex = MakeCheckerTexture();
    }
    glBindTexture(GL_TEXTURE_2D, mw.checkerTex);

    if (mw.quadric == nullptr) {
        mw.quadric = gluNewQuadric();
        gluQuadricTexture(mw.quadric, GL_TRUE);
        gluQuadricNormals(mw.quadric, GLU_SMOOTH);
    }

    // Re-apply viewport/projection to ensure bounds match current size
    RECT rc; GetClientRect(mw.hWnd, &rc);
    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;
    ApplyViewportAndProjection(mw, w, h);

    if (!useGlobalState) {
        UpdatePhysicsMW(mw, dt * g_timeScale);
    }

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

    if (g_gridEnabled) {
        glBegin(GL_LINES);
        for (float i = -1.0f; i <= 1.0f; i += 0.2f) {
            glVertex3f(i, mw.floorY, -1.0f);
            glVertex3f(i, mw.floorY, 1.0f);
            glVertex3f(-1.0f, mw.floorY, i);
            glVertex3f(1.0f, mw.floorY, i);
        }
        glEnd();

        glBegin(GL_LINES);
        for (float x = -1.0f; x <= 1.0f; x += 0.2f) {
            glVertex3f(x, mw.floorY, -1.0f);
            glVertex3f(x, mw.floorY + 2.0f, -1.0f);
        }
        for (float y = mw.floorY; y <= mw.floorY + 2.0f; y += 0.2f) {
            glVertex3f(-1.0f, y, -1.0f);
            glVertex3f(1.0f, y, -1.0f);
        }
        glEnd();
    }

    if (g_floorShadowEnabled) {
        glDisable(GL_LIGHTING);
        glColor4f(0.0f, 0.0f, 0.0f, 0.4f);
        glPushMatrix();
        if (useGlobalState) glTranslatef(g_ballX, g_FLOOR_Y + 0.001f, g_ballZ);
        else                glTranslatef(mw.ballX, mw.floorY + 0.001f, mw.ballZ);
        glScalef(1.0f, 0.1f, 1.0f);
        DrawSphere(mw, BALL_RADIUS);
        glPopMatrix();
    }

    if (g_wallShadowEnabled) {
        glDisable(GL_LIGHTING);
        glColor4f(0.0f, 0.0f, 0.0f, 0.3f);
        glPushMatrix();
        if (useGlobalState) glTranslatef(g_ballX, g_ballY, -1.0f);
        else                glTranslatef(mw.ballX, mw.ballY, -1.0f);
        glScalef(1.0f, 1.0f, 0.1f);
        DrawSphere(mw, BALL_RADIUS);
        glPopMatrix();
    }

    glEnable(GL_LIGHTING);

    glPushMatrix();
    if (useGlobalState) {
        glTranslatef(g_ballX, g_ballY, g_ballZ);
        glRotatef(90.0f, 1, 0, 0);
        glRotatef(-15.0f, 0, 1, 0);
        glRotatef(g_spinAngle, 0, 0, 1);
    }
    else {
        glTranslatef(mw.ballX, mw.ballY, mw.ballZ);
        glRotatef(90.0f, 1, 0, 0);
        glRotatef(-15.0f, 0, 1, 0);
        glRotatef(mw.spinAngle, 0, 0, 1);
    }
    if (!g_ballLightingEnabled) {
        glDisable(GL_LIGHTING);
        glColor3f(1.0f, 1.0f, 1.0f);
    }
    DrawSphere(mw, BALL_RADIUS);
    if (!g_ballLightingEnabled) glEnable(GL_LIGHTING);
    glPopMatrix();

    SwapBuffers(mw.hDC);
}

// Config dialog
// Config dialog
static INT_PTR CALLBACK ConfigDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM) {
    switch (msg) {
    case WM_INITDIALOG: {
        g_floorShadowEnabled = ReadBoolSetting(L"FloorShadow", DEFAULT_FLOOR_SHADOW);
        g_wallShadowEnabled = ReadBoolSetting(L"WallShadow", DEFAULT_WALL_SHADOW);
        g_gridEnabled = ReadBoolSetting(L"Grid", DEFAULT_GRID);
        g_soundEnabled = ReadBoolSetting(L"Sound", DEFAULT_SOUND);
        g_bgColor = ReadColorSetting(L"BackgroundColor", DEFAULT_BG_COLOR);
        g_geometryMode = ReadIntSetting(L"GeometryMode", DEFAULT_GEOMETRY_MODE);
        g_ballLightingEnabled = ReadBoolSetting(L"BallLighting", DEFAULT_BALL_LIGHTING);
        g_multiMonitorMode = ReadIntSetting(L"MultiMonitorMode", DEFAULT_MULTI_MONITOR_MODE);

        /*Debugger*************************************************************************************************************************************
        DebugMode(L"Config WM_INITDIALOG after reads");
        */
        CheckDlgButton(hDlg, IDC_FLOORSHADOW, g_floorShadowEnabled ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(hDlg, IDC_WALLSHADOW, g_wallShadowEnabled ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(hDlg, IDC_GRID, g_gridEnabled ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(hDlg, IDC_SOUND, g_soundEnabled ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(hDlg, IDC_GEOMETRY, g_geometryMode ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(hDlg, IDC_BALLLIGHTING, g_ballLightingEnabled ? BST_CHECKED : BST_UNCHECKED);

        // Explicit radio set (do not rely on CheckRadioButton grouping)
        SetMonitorModeRadios(hDlg, g_multiMonitorMode);
        return TRUE;
    }

    case WM_COMMAND: {
        switch (LOWORD(wParam)) {

        case IDC_BGCOLOR: {
            static COLORREF customColors[16] = { 0 };
            CHOOSECOLOR cc = { sizeof(CHOOSECOLOR) };
            cc.hwndOwner = hDlg;
            cc.lpCustColors = customColors;
            cc.rgbResult = g_bgColor;
            cc.Flags = CC_RGBINIT | CC_FULLOPEN;
            if (ChooseColor(&cc)) {
                g_bgColor = cc.rgbResult;
                if (g_hWnd) InvalidateRect(g_hWnd, nullptr, FALSE);
            }
            return TRUE;
        }

        case IDC_RESTORE: {
            g_floorShadowEnabled = DEFAULT_FLOOR_SHADOW;
            g_wallShadowEnabled = DEFAULT_WALL_SHADOW;
            g_gridEnabled = DEFAULT_GRID;
            g_soundEnabled = DEFAULT_SOUND;
            g_bgColor = DEFAULT_BG_COLOR;
            g_geometryMode = DEFAULT_GEOMETRY_MODE;
            g_ballLightingEnabled = DEFAULT_BALL_LIGHTING;
            g_multiMonitorMode = DEFAULT_MULTI_MONITOR_MODE;

			/*Debugger****************************************************************************************************************************
            g_multiMonitorMode = DEFAULT_MULTI_MONITOR_MODE;
            DebugMode(L"Config RESTORE after reset");
            */
            // Explicit radio set
            SetMonitorModeRadios(hDlg, g_multiMonitorMode);

            CheckDlgButton(hDlg, IDC_FLOORSHADOW, g_floorShadowEnabled ? BST_CHECKED : BST_UNCHECKED);
            CheckDlgButton(hDlg, IDC_WALLSHADOW, g_wallShadowEnabled ? BST_CHECKED : BST_UNCHECKED);
            CheckDlgButton(hDlg, IDC_GRID, g_gridEnabled ? BST_CHECKED : BST_UNCHECKED);
            CheckDlgButton(hDlg, IDC_SOUND, g_soundEnabled ? BST_CHECKED : BST_UNCHECKED);
            CheckDlgButton(hDlg, IDC_GEOMETRY, g_geometryMode ? BST_CHECKED : BST_UNCHECKED);
            CheckDlgButton(hDlg, IDC_BALLLIGHTING, g_ballLightingEnabled ? BST_CHECKED : BST_UNCHECKED);
            return TRUE;
        }

        case IDOK: {
            g_floorShadowEnabled = (IsDlgButtonChecked(hDlg, IDC_FLOORSHADOW) == BST_CHECKED);
            g_wallShadowEnabled = (IsDlgButtonChecked(hDlg, IDC_WALLSHADOW) == BST_CHECKED);
            g_gridEnabled = (IsDlgButtonChecked(hDlg, IDC_GRID) == BST_CHECKED);
            g_soundEnabled = (IsDlgButtonChecked(hDlg, IDC_SOUND) == BST_CHECKED);
            g_geometryMode = (IsDlgButtonChecked(hDlg, IDC_GEOMETRY) == BST_CHECKED) ? 1 : 0;
            g_ballLightingEnabled = (IsDlgButtonChecked(hDlg, IDC_BALLLIGHTING) == BST_CHECKED);

            // Read explicit radio checks
            if (IsDlgButtonChecked(hDlg, IDC_MONITOR_SINGLE) == BST_CHECKED) g_multiMonitorMode = 0;
            else if (IsDlgButtonChecked(hDlg, IDC_MONITOR_EXTENDED) == BST_CHECKED) g_multiMonitorMode = 1;
            else if (IsDlgButtonChecked(hDlg, IDC_MONITOR_REPLICATED) == BST_CHECKED) g_multiMonitorMode = 2;
            else if (IsDlgButtonChecked(hDlg, IDC_MONITOR_UNIFIED) == BST_CHECKED) g_multiMonitorMode = 3;
            else g_multiMonitorMode = DEFAULT_MULTI_MONITOR_MODE;
			
            /*Debugger****************************************************************************************************************************
            DebugMode(L"Config IDOK before write");
            */
            WriteBoolSetting(L"FloorShadow", g_floorShadowEnabled);
            WriteBoolSetting(L"WallShadow", g_wallShadowEnabled);
            WriteBoolSetting(L"Grid", g_gridEnabled);
            WriteBoolSetting(L"Sound", g_soundEnabled);
            WriteColorSetting(L"BackgroundColor", g_bgColor);
            WriteIntSetting(L"GeometryMode", g_geometryMode);
            WriteBoolSetting(L"BallLighting", g_ballLightingEnabled);
            WriteIntSetting(L"MultiMonitorMode", g_multiMonitorMode);

            EndDialog(hDlg, IDOK);
            return TRUE;
        }

        case IDCANCEL:
            EndDialog(hDlg, IDCANCEL);
            return TRUE;
        }
        break;
    }
    }
    return FALSE;
}

// WndProc — no GL context creation, no rendering in WM_PAINT
static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: {
        // Re‑read settings if you want to be extra safe
        LoadSettingsFromRegistry();

        g_hWnd = hWnd;
        g_hDC = GetDC(hWnd);
        SetWindowPixelFormat(g_hDC);

        g_running = true;
        return 0;
    }


    case WM_PAINT: {
        // No rendering here; single source of truth is the main loop.
        ValidateRect(hWnd, NULL);
        return 0;
    }

    case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN:
    case WM_MBUTTONDOWN:
    case WM_MOUSEWHEEL:
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
        if (!g_preview) QuitSaver();
        return 0;

    case WM_DESTROY: {
        PostQuitMessage(0);
        return 0;
    }

    default:
        return DefWindowProc(hWnd, msg, wParam, lParam);
    }
}

// Guarded class registration (WNDCLASSEX) to avoid collisions
static void EnsureRegisteredClass(HINSTANCE hInst, const wchar_t* className, WNDPROC proc) {
    WNDCLASSEX wcx = { sizeof(WNDCLASSEX) };
    WNDCLASSEX existing = { sizeof(WNDCLASSEX) };
    if (GetClassInfoEx(hInst, className, &existing)) {
        return; // Already registered
    }
    wcx.style = CS_OWNDC;
    wcx.lpfnWndProc = proc;
    wcx.hInstance = hInst;
    wcx.hCursor = LoadCursor(NULL, IDC_ARROW);
    wcx.lpszClassName = className;
    wcx.hbrBackground = NULL;
    RegisterClassEx(&wcx);
}

// Per-monitor window creation callback
static BOOL CALLBACK EnumMonitorsProc(HMONITOR, HDC, LPRECT lprcMonitor, LPARAM lParam) {
    HINSTANCE hInstLocal = (HINSTANCE)lParam;

    const int w = lprcMonitor->right - lprcMonitor->left;
    const int h = lprcMonitor->bottom - lprcMonitor->top;

    HWND hWnd = CreateWindowEx(
        WS_EX_TOPMOST, kSaverClassName, L"", WS_POPUP,
        lprcMonitor->left, lprcMonitor->top, w, h,
        NULL, NULL, hInstLocal, NULL
    );
    if (!hWnd) return TRUE;

    // Enforce true topmost across all monitors
    SetWindowPos(hWnd, HWND_TOPMOST, lprcMonitor->left, lprcMonitor->top, w, h,
        SWP_SHOWWINDOW | SWP_NOACTIVATE);

    ShowWindow(hWnd, SW_SHOW);
    UpdateWindow(hWnd);

    MonitorWindow mw{};
    mw.hWnd = hWnd;
    mw.hDC = GetDC(hWnd);
    if (!mw.hDC) return TRUE;

    SetWindowPixelFormat(mw.hDC);

    mw.hGL = wglCreateContext(mw.hDC);
    if (!mw.hGL) {
        ReleaseDC(hWnd, mw.hDC);
        return TRUE;
    }

    if (!wglMakeCurrent(mw.hDC, mw.hGL)) {
        wglDeleteContext(mw.hGL);
        ReleaseDC(hWnd, mw.hDC);
        return TRUE;
    }

    // Setup GL and resources per window (texture + quadric + projection)
    SetupGL(mw, w, h);

    // Initialize per-window ball to a sensible starting point
    mw.ballX = -0.5f;
    mw.ballY = mw.floorY + BALL_RADIUS;
    mw.ballZ = 0.0f;
    mw.vx = 0.8f;
    mw.vy = 4.5f;
    mw.vz = 0.0f;
    mw.spinAngle = 0.0f;
    mw.spinDir = 1;

    // Apply an offset in Extended mode so balls don't sync
    if (g_multiMonitorMode == 1) {
        int idx = (int)g_monitorWindows.size(); // 0 for first, 1 for second, etc.
        mw.ballX += 0.5f * idx;   // shift starting X
        mw.ballY += 0.2f * idx;   // shift starting Y
        mw.vx += 0.1f * idx;      // tweak velocity slightly
    }

	/*debugger******************************************************************************************************************************
    if (g_multiMonitorMode == 1) {
        DebugMode(L"EnumMonitorsProc Extended offset");
        int idx = (int)g_monitorWindows.size();
     }
    */
    g_monitorWindows.push_back(mw);
    if (!g_hWnd) g_hWnd = hWnd;

    return TRUE;
}

// Create saver windows for all modes
static HWND CreateSaverWindow(HINSTANCE hInst, HWND hParent, bool preview) {
    EnsureRegisteredClass(hInst, kSaverClassName, WndProc);

    RECT rc;
    if (preview && hParent) {
        GetClientRect(hParent, &rc);
        g_preview = true;

        HWND hWnd = CreateWindowEx(0, kSaverClassName, L"", WS_CHILD | WS_VISIBLE,
            rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top,
            hParent, NULL, hInst, NULL);
        if (!hWnd) return nullptr;

        ShowWindow(hWnd, SW_SHOW);
        UpdateWindow(hWnd);

        MonitorWindow mw{};
        mw.hWnd = hWnd;
        mw.hDC = GetDC(hWnd);
        SetWindowPixelFormat(mw.hDC);
        mw.hGL = wglCreateContext(mw.hDC);
        wglMakeCurrent(mw.hDC, mw.hGL);

        // Setup GL and resources per window
        int w = rc.right - rc.left;
        int h = rc.bottom - rc.top;
        SetupGL(mw, w, h);

        // Initialize ball for preview (start near center to avoid floor intersection)
        g_ballX = 0.0f;
        g_ballY = (mw.floorY + BALL_RADIUS) + (fabs(mw.floorY) * 0.5f);
        g_ballZ = 0.0f;
        g_vx = 0.8f;
        g_vy = 0.0f;
        g_vz = 0.0f;
        g_spinAngle = 0.0f;
        g_spinDir = 1;

        mw.ballX = g_ballX;
        mw.ballY = g_ballY;
        mw.ballZ = g_ballZ;
        mw.vx = g_vx;
        mw.vy = g_vy;
        mw.vz = g_vz;
        mw.spinAngle = g_spinAngle;
        mw.spinDir = g_spinDir;

        g_monitorWindows.push_back(mw);
        if (!g_hWnd) g_hWnd = hWnd;

        return hWnd;
    }
    else {
        g_monitorWindows.clear();

		/*Debugger*************************************************************************************************************************************
        DebugMode(L"CreateSaverWindow before switch");
        */
        switch (g_multiMonitorMode) {
        case 1: // Extended (independent physics per monitor) — intentional fallthrough to case 2 for window creation
            // (No break here: both Extended and Replicated enumerate one popup per monitor)
        case 2: // Replicated (global physics rendered on all monitors)
        {
            g_monitorWindows.clear();
            EnumDisplayMonitors(NULL, NULL, EnumMonitorsProc, (LPARAM)hInst);

            // Initialize global ball state for replicated mode; bounds derived from first window per-frame
            g_ballX = -0.5f;
            g_ballY = 0.0f;
            g_ballZ = 0.0f;
            g_vx = 0.8f;
            g_vy = 4.5f;
            g_vz = 0.0f;
            g_spinAngle = 0.0f;
            g_spinDir = 1;

            return g_hWnd;
        }

        case 3: // Unified (single window spanning virtual screen)
        {
            int x = GetSystemMetrics(SM_XVIRTUALSCREEN);
            int y = GetSystemMetrics(SM_YVIRTUALSCREEN);
            int w = GetSystemMetrics(SM_CXVIRTUALSCREEN);
            int h = GetSystemMetrics(SM_CYVIRTUALSCREEN);

            HWND hWnd = CreateWindowEx(WS_EX_TOPMOST, kSaverClassName, L"", WS_POPUP,
                x, y, w, h, NULL, NULL, hInst, NULL);
            if (!hWnd) return nullptr;

            SetWindowPos(hWnd, HWND_TOPMOST, x, y, w, h, SWP_SHOWWINDOW | SWP_NOACTIVATE);
            ShowWindow(hWnd, SW_SHOW);
            UpdateWindow(hWnd);

            MonitorWindow mw{};
            mw.hWnd = hWnd;
            mw.hDC = GetDC(hWnd);
            SetWindowPixelFormat(mw.hDC);
            mw.hGL = wglCreateContext(mw.hDC);
            wglMakeCurrent(mw.hDC, mw.hGL);

            SetupGL(mw, w, h);

            mw.ballX = -0.5f;
            mw.ballY = mw.floorY + BALL_RADIUS;
            mw.ballZ = 0.0f;
            mw.vx = 0.8f;
            mw.vy = 4.5f;
            mw.vz = 0.0f;
            mw.spinAngle = 0.0f;
            mw.spinDir = 1;

            g_monitorWindows.push_back(mw);
            g_hWnd = hWnd;
            return g_hWnd;
        }

        default: // 0 — Single
        {
            RECT rcDesk; GetWindowRect(GetDesktopWindow(), &rcDesk);
            const int w = rcDesk.right - rcDesk.left;
            const int h = rcDesk.bottom - rcDesk.top;

            HWND hWnd = CreateWindowEx(WS_EX_TOPMOST, kSaverClassName, L"", WS_POPUP,
                rcDesk.left, rcDesk.top, w, h, NULL, NULL, hInst, NULL);
            if (!hWnd) return nullptr;

            SetWindowPos(hWnd, HWND_TOPMOST, rcDesk.left, rcDesk.top, w, h, SWP_SHOWWINDOW | SWP_NOACTIVATE);
            ShowWindow(hWnd, SW_SHOW);
            UpdateWindow(hWnd);

            MonitorWindow mw{};
            mw.hWnd = hWnd;
            mw.hDC = GetDC(hWnd);
            SetWindowPixelFormat(mw.hDC);
            mw.hGL = wglCreateContext(mw.hDC);
            wglMakeCurrent(mw.hDC, mw.hGL);

            SetupGL(mw, w, h);

            // Seed both global and per-window state near center (avoid immediate floor clamp)
            g_ballX = 0.0f;
            g_ballY = (mw.floorY + BALL_RADIUS) + (fabs(mw.floorY) * 0.5f);
            g_ballZ = 0.0f;
            g_vx = 0.8f;
            g_vy = 0.0f;
            g_vz = 0.0f;
            g_spinAngle = 0.0f;
            g_spinDir = 1;

            mw.ballX = g_ballX;
            mw.ballY = g_ballY;
            mw.ballZ = g_ballZ;
            mw.vx = g_vx;
            mw.vy = g_vy;
            mw.vz = g_vz;
            mw.spinAngle = g_spinAngle;
            mw.spinDir = g_spinDir;

            g_monitorWindows.push_back(mw);
            g_hWnd = hWnd;
            return g_hWnd;
        }
        }

    }
    return nullptr;
}

// Cleanup: per-monitor resources and contexts — no sharing, no master
static void CleanupGL() {
    for (auto& mw : g_monitorWindows) {
        if (mw.hDC && mw.hGL) {
            if (wglMakeCurrent(mw.hDC, mw.hGL)) {
                if (mw.checkerTex) { glDeleteTextures(1, &mw.checkerTex); mw.checkerTex = 0; }
                if (mw.quadric) { gluDeleteQuadric(mw.quadric); mw.quadric = nullptr; }
                wglMakeCurrent(NULL, NULL);
            }
        }
        if (mw.hGL) { wglDeleteContext(mw.hGL); mw.hGL = nullptr; }
        if (mw.hDC) { ReleaseDC(mw.hWnd, mw.hDC); mw.hDC = nullptr; }
        if (mw.hWnd) { DestroyWindow(mw.hWnd); mw.hWnd = nullptr; }
    }
    g_monitorWindows.clear();

    if (g_hDC) { ReleaseDC(g_hWnd, g_hDC); g_hDC = nullptr; }
    g_hWnd = nullptr;

    if (!g_preview && g_cursorHidden) { ShowCursor(TRUE); g_cursorHidden = false; }
}

// Entry point
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR lpCmdLine, int) {
    g_hInst = hInstance;

    const wchar_t* cmd = lpCmdLine;

    // Settings dialog
    if (cmd && (wcsstr(cmd, L"/c") || wcsstr(cmd, L"-c"))) {
        DialogBoxParamW(hInstance, MAKEINTRESOURCE(IDD_CONFIG), nullptr, ConfigDlgProc, 0);
        return 0;
    }

    // Preview (/p hwnd)
    HWND hWndParent = nullptr;
    if (cmd && (wcsstr(cmd, L"/p") || wcsstr(cmd, L"-p"))) {
        const wchar_t* hwndStr = wcsstr(cmd, L"/p"); if (!hwndStr) hwndStr = wcsstr(cmd, L"-p");
        if (hwndStr && wcslen(hwndStr) > 2) {
            hwndStr += 2;
            while (*hwndStr == L' ') hwndStr++;
            hWndParent = (HWND)(uintptr_t)_wtoi(hwndStr);
        }
    }

    // Load settings before creating any windows so mode is correct for CreateSaverWindow
    LoadSettingsFromRegistry();

    g_hWnd = CreateSaverWindow(hInstance, hWndParent, hWndParent != nullptr);

    if (!g_hWnd) {
        MessageBox(nullptr, L"Failed to create window!", L"BoingBallSaver", MB_OK | MB_ICONERROR);
        return 1;
    }

    ShowWindow(g_hWnd, SW_SHOW);
    UpdateWindow(g_hWnd);
    if (!g_preview && !g_cursorHidden) { ShowCursor(FALSE); g_cursorHidden = true; }

    InitTimer();

    MSG msg;
    while (g_running) {
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) g_running = false;
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        g_soundPlayedThisFrame = false;
        float dt = ComputeDeltaTime();

		/*debugger*********************************************************************************************************************************
        DebugMode(L"Main loop top");
        */
        // Refresh global bounds and advance global physics for Replicated and Single
        if (g_multiMonitorMode == 2 || g_multiMonitorMode == 0) {
			/*debugger*************************************************************************************************************************
            DebugMode(L"Main loop bounds update (Single/Replicated path)");
            */
            if (!g_monitorWindows.empty()) {
                RECT rc; GetClientRect(g_monitorWindows[0].hWnd, &rc);
                MonitorWindow& first = g_monitorWindows[0];
                // Make current so projection-related state and bounds are valid
                wglMakeCurrent(first.hDC, first.hGL);
                ApplyViewportAndProjection(first, rc.right - rc.left, rc.bottom - rc.top);
                g_WALL_X = first.wallX;
                g_WALL_Z = first.wallZ;
                g_FLOOR_Y = first.floorY;
            }
            UpdatePhysicsGlobal(dt * g_timeScale);
        }

        for (auto& mw : g_monitorWindows) {
            switch (g_multiMonitorMode) { //debuggers below**************************************************************
            case 1: // Extended
                /*DebugMode(L"Render loop Extended");*/
                RenderFrameMonitor(mw, false, dt);
                break;
            case 2: // Replicated
                /*DebugMode(L"Render loop Replicated");*/
                RenderFrameMonitor(mw, true, dt);
                break;
            case 3: // Unified
                /*DebugMode(L"Render loop Unified");*/
                RenderFrameMonitor(mw, false, dt);
                break;
            default: // Single
                /*DebugMode(L"Render loop Single");*/
                RenderFrameMonitor(mw, true, dt);
                break;
            }
        }

        Sleep(1);
    }

    // Final cleanup
    CleanupGL();
    return 0;
}

// END OF FILE
