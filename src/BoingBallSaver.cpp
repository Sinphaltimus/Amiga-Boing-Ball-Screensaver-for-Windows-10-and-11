// BoingBallSaver.cpp — Windows screensaver with bouncing red/white checkered ball similar to Amiga BoingBall Demo.
// Created by Sinphaltimus Exmortus aka AirTwerx and Copilot AI.

#include <windows.h>
#include <GL/gl.h>
#include <GL/glu.h>
#include <cmath>
#include <string>
#include <mmsystem.h>
#pragma comment(lib, "winmm.lib")
#include "resource.h"

#pragma comment(lib, "opengl32.lib")
#pragma comment(lib, "glu32.lib")
#pragma comment(lib, "gdi32.lib")

HINSTANCE g_hInst;
HWND g_hWnd;
HDC g_hDC;
HGLRC g_hGL;
bool g_running = false;
bool g_preview = false;

GLuint g_checkerTex;

// Playback speed
float g_timeScale = 0.5f;  // 0.5 = half speed, 0.25 = quarter speed, 1.0 = normal

// World/physics
float BALL_RADIUS = 0.25f;
float RESTITUTION = 1.0f;     // perfect bounce
float GRAVITY = -9.8f;        // gravity strength
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
float g_spinAngle = 0.0f;     // current rotation angle
float g_spinSpeed = 120.0f;   // degrees per second (base)
int   g_spinDir = 1;          // +1 or -1

LARGE_INTEGER g_freq, g_prev;

void QuitSaver() { g_running = false; PostMessage(g_hWnd, WM_CLOSE, 0, 0); }

void MakeCheckerTexture() {
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

    // Texture filtering (nicer)
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
}

void SetupGL(int w, int h) {
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

    // Enable blending for transparency
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    GLfloat lightDir[] = { -0.5f, 0.8f, 0.6f, 0.0f };
    glLightfv(GL_LIGHT0, GL_POSITION, lightDir);

    // Global ambient light
    GLfloat globalAmbient[] = { 0.3f, 0.3f, 0.3f, 1.0f };
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, globalAmbient);

    // Per-light ambient boost
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

void CleanupGL() {
    if (g_checkerTex) glDeleteTextures(1, &g_checkerTex);
    if (g_hGL) { wglMakeCurrent(NULL, NULL); wglDeleteContext(g_hGL); }
    if (g_hDC) ReleaseDC(g_hWnd, g_hDC);
}

// Reflect ball at side walls
inline void reflectXAtWall() {
    if (g_ballX > WALL_X - BALL_RADIUS) {
        g_ballX = WALL_X - BALL_RADIUS;
        g_vx = -fabsf(g_vx);
        g_spinDir *= -1;
        PlaySound(MAKEINTRESOURCE(BOINGW), g_hInst, SND_RESOURCE | SND_ASYNC);
    }
    else if (g_ballX < -WALL_X + BALL_RADIUS) {
        g_ballX = -WALL_X + BALL_RADIUS;
        g_vx = +fabsf(g_vx);
        g_spinDir *= -1;
        PlaySound(MAKEINTRESOURCE(BOINGW), g_hInst, SND_RESOURCE | SND_ASYNC);
    }
}

void UpdatePhysics(float dt) {
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
        PlaySound(MAKEINTRESOURCE(BOINGF), g_hInst, SND_RESOURCE | SND_ASYNC);
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

void DrawSphere(float r) {
    GLUquadric* quad = gluNewQuadric();
    gluQuadricTexture(quad, GL_TRUE);
    glBindTexture(GL_TEXTURE_2D, g_checkerTex);
    gluSphere(quad, r, 32, 32);
    gluDeleteQuadric(quad);
}

void RenderFrame() {
    LARGE_INTEGER now; QueryPerformanceCounter(&now);
    float dt = (float)(now.QuadPart - g_prev.QuadPart) / (float)g_freq.QuadPart;
    g_prev = now;
    if (dt > 0.05f) dt = 0.05f;
    UpdatePhysics(dt * g_timeScale);

    glClearColor(0.75f, 0.75f, 0.75f, 1.0f);  // soft grey
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glTranslatef(0, 0, -2.0f);

    glDisable(GL_LIGHTING);
    glColor3f(0.3f, 0.6f, 1.0f);  // cyan grid lines
    glLineWidth(2.0f);            // thicker lines

    // Grid floor
    glBegin(GL_LINES);
    for (float i = -1.0f; i <= 1.0f; i += 0.2f) {
        // X lines
        glVertex3f(i, FLOOR_Y, -1.0f);
        glVertex3f(i, FLOOR_Y, 1.0f);

        // Z lines
        glVertex3f(-1.0f, FLOOR_Y, i);
        glVertex3f(1.0f, FLOOR_Y, i);
    }
    glEnd();

    // Grid wall
    glBegin(GL_LINES);

    // Vertical lines (X direction)
    for (float x = -1.0f; x <= 1.0f; x += 0.2f) {
        glVertex3f(x, FLOOR_Y, -1.0f);          // bottom at floor
        glVertex3f(x, FLOOR_Y + 2.0f, -1.0f);   // top of wall (2 units tall)
    }

    // Horizontal lines (Y direction)
    for (float y = FLOOR_Y; y <= FLOOR_Y + 2.0f; y += 0.2f) {
        glVertex3f(-1.0f, y, -1.0f);            // left edge
        glVertex3f(1.0f, y, -1.0f);             // right edge
    }

    glEnd();

    // Floor shadow
    glDisable(GL_LIGHTING);
    glColor4f(0.0f, 0.0f, 0.0f, 0.4f);  // semi-transparent black

    glPushMatrix();
    glTranslatef(g_ballX, FLOOR_Y + 0.001f, g_ballZ); // place just above floor
    glScalef(1.0f, 0.1f, 1.0f);                       // flatten into ellipse
    DrawSphere(BALL_RADIUS);                          // reuse sphere geometry
    glPopMatrix();

    // Wall shadow
    glColor4f(0.0f, 0.0f, 0.0f, 0.3f);  // softer shadow
    glPushMatrix();
    glTranslatef(g_ballX, g_ballY, -1.0f);   // position on wall
    glScalef(1.0f, 1.0f, 0.1f);              // flatten into disk
    DrawSphere(BALL_RADIUS);                 // reuse sphere geometry
    glPopMatrix();

    glEnable(GL_LIGHTING);

    // Ball
    glPushMatrix();
    glTranslatef(g_ballX, g_ballY, g_ballZ);
    // Initial orientation
    glRotatef(90.0f, 1, 0, 0);   // rotate 90° around X
    glRotatef(-15.0f, 0, 1, 0);  // rotate 15° around Y
    glRotatef(g_spinAngle, 0, 0, 1);   // dynamic spin around Z
    DrawSphere(BALL_RADIUS);
    glPopMatrix();

    SwapBuffers(g_hDC);
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: {
        // Startup test sound (optional)
        PlaySound(MAKEINTRESOURCE(BOINGF), g_hInst, SND_RESOURCE | SND_ASYNC);

        RECT r; GetClientRect(hWnd, &r);
        g_hWnd = hWnd;
        SetupGL(r.right, r.bottom);

        // Set initial ball position based on dynamic bounds
        g_ballX = -WALL_X + BALL_RADIUS;
        g_ballY = FLOOR_Y + BALL_RADIUS;

        g_running = true;
    } return 0;

    case WM_PAINT:
        if (g_running) RenderFrame();
        ValidateRect(hWnd, NULL);
        return 0;
    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE) {  // only ESC quits
            QuitSaver();
        }
        return 0;
    case WM_MOUSEMOVE:
        // ignore mouse movement completely
        return 0;
    case WM_DESTROY:
        CleanupGL(); PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProc(hWnd, msg, wParam, lParam);
    }
}

HWND CreateSaverWindow(HINSTANCE hInst, HWND hParent, bool preview) {
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
    std::wstring args(cmd ? cmd : L"");
    bool run = false;
    if (args.empty()) run = true;
    if (args.find(L"/s") != std::wstring::npos || args.find(L"-s") != std::wstring::npos) run = true;

    // Config dialog
    if (args.find(L"/c") != std::wstring::npos || args.find(L"-c") != std::wstring::npos) {
        MessageBox(nullptr,
            L"Boing Ball Screensaver\n\nNo configuration options yet.\n"
            L"Tip: Rename the EXE to .scr and place in System32.",
            L"BoingBallSaver", MB_OK | MB_ICONINFORMATION);
        return 0;
    }

    // Parse preview handle (/p <HWND>)
    HWND hWndParent = nullptr;
    size_t ppos = args.find(L"/p");
    if (ppos != std::wstring::npos) {
        size_t start = args.find_first_not_of(L" \t", ppos + 2);
        if (start != std::wstring::npos) {
            std::wstring num = args.substr(start);
            hWndParent = (HWND)(uintptr_t)_wtoi(num.c_str());
        }
    }

    if (!run && !hWndParent) run = true;

    g_hWnd = CreateSaverWindow(hInst, hWndParent, hWndParent != nullptr);
    if (!g_hWnd) {
        MessageBox(nullptr, L"Failed to create window!", L"Debug", MB_OK);
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
