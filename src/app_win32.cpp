// app_win32.cpp - live viewer. All drawing still happens in our own Framebuffer
// (the BACK buffer). Win32 is used only to open a window, read the keyboard and
// copy the finished frame to the screen with one block transfer (StretchDIBits).
//
// Animation loop (as in the lecture):
//   WHILE running: CALCULATE dt -> UPDATE state with dt -> DRAW into back buffer -> PRESENT
#include "app.h"

#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define Polygon Win32GdiPolygon  // GDI has a Polygon() function; keep our Polygon type usable
#include <windows.h>
#include <dwmapi.h>
#undef Polygon

#include "animation.h"
#include "demos.h"
#include "font.h"
#include "raster.h"
#include "scene.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <vector>

namespace {

struct Viewer {
    // ---- buffers
    Framebuffer back{PAGE_W, PAGE_H};      // every frame is completed here first
    Framebuffer canvas{CANVAS_W, CANVAS_H};
    std::vector<uint32_t> bgra;            // back buffer in the 32-bit layout GDI expects
    BITMAPINFO bmi{};
    std::vector<Framebuffer> staticPages;  // pages that never change are rendered once

    // ---- window
    HWND hwnd = nullptr;
    bool running = true, fullscreen = false;
    WINDOWPLACEMENT savedPlacement{};

    // ---- state
    int page = 0;                  // 0 = scene, 1..N = technique pages
    bool paused = false, doubleBuffer = true, vsync = true, construction = false, help = true;
    double story = 0, animTime = 0;  // seconds along the story timeline / idle animation clock
    double manual = -1;              // >= 0 while the user scrubs the distortion by hand
    double distortion = 0;
    KeyframeTrack timeline;
    FrameClock clock;
    double dt = 0;
    std::string toast;
    double toastTimer = 0;
    std::string outDir;
    int shots = 0;
};
Viewer* V = nullptr;

void showToast(const std::string& msg) {
    V->toast = msg;
    V->toastTimer = 2.5;
}

// ------------------------------------------------------------------ present
// Copies the back buffer to the window in one block transfer, scaled to fit
// while keeping the aspect ratio (letterboxing the rest).
void present() {
    const Color* src = V->back.data();
    for (size_t i = 0; i < V->bgra.size(); ++i)
        V->bgra[i] = (uint32_t)src[i].r << 16 | (uint32_t)src[i].g << 8 | src[i].b;

    HDC dc = GetDC(V->hwnd);
    RECT rc;
    GetClientRect(V->hwnd, &rc);
    int cw = rc.right, ch = rc.bottom;
    double s = std::min(cw / (double)PAGE_W, ch / (double)PAGE_H);
    int dw = (int)(PAGE_W * s), dh = (int)(PAGE_H * s);
    int dx = (cw - dw) / 2, dy = (ch - dh) / 2;
    if (dx > 0) { PatBlt(dc, 0, 0, dx, ch, BLACKNESS); PatBlt(dc, dx + dw, 0, cw - dx - dw, ch, BLACKNESS); }
    if (dy > 0) { PatBlt(dc, 0, 0, cw, dy, BLACKNESS); PatBlt(dc, 0, dy + dh, cw, ch - dy - dh, BLACKNESS); }
    SetStretchBltMode(dc, COLORONCOLOR);  // nearest neighbour: keeps pixels crisp
    StretchDIBits(dc, dx, dy, dw, dh, 0, 0, PAGE_W, PAGE_H, V->bgra.data(), &V->bmi, DIB_RGB_COLORS, SRCCOPY);
    ReleaseDC(V->hwnd, dc);
}

// ------------------------------------------------------------------ HUD
void drawSceneHud(Framebuffer& c) {
    // bottom panel: alpha-blended over the scene
    const int y0 = CANVAS_H - 30;
    fillRectAlpha(c, {0, y0, CANVAS_W, 30}, {10, 8, 16}, 0.65);
    drawText(c, 10, y0 + 6, "DISTORTION", {200, 200, 215});
    IRect bar{76, y0 + 5, 150, 9};
    fillRect(c, bar, {50, 46, 62});
    int fillW = (int)std::lround(bar.w * V->distortion);
    for (int x = 0; x < fillW; ++x)  // colour interpolation along the bar
        for (int y = bar.y; y < bar.y + bar.h; ++y)
            c.setPixel(bar.x + x, y, lerpColor({60, 190, 200}, {210, 40, 60}, x / (double)bar.w));
    char buf[64];
    std::snprintf(buf, sizeof buf, "%.2f", V->distortion);
    drawText(c, 232, y0 + 6, buf, {240, 240, 245});
    drawText(c, 10, y0 + 18, V->manual >= 0 ? "MANUAL (PRESS R FOR AUTOPLAY)" : "AUTOPLAY STORY", {130, 130, 150});

    // timeline with keyframe markers and playhead
    const double dur = V->timeline.duration();
    const int tx0 = 300, tx1 = CANVAS_W - 14, ty = y0 + 10;
    drawText(c, tx0, y0 + 18, "KEYFRAMES", {130, 130, 150});
    fillRect(c, {tx0, ty, tx1 - tx0, 2}, {90, 86, 110});
    for (const Keyframe& k : V->timeline.keys()) {
        int x = tx0 + (int)std::lround((tx1 - tx0) * k.time / dur);
        Color col = lerpColor({60, 190, 200}, {210, 40, 60}, k.value);
        fillRect(c, {x - 3, ty - 2, 7, 6}, col);  // marker coloured by its value
    }
    int px = tx0 + (int)std::lround((tx1 - tx0) * V->story / dur);
    fillRect(c, {px - 1, ty - 6, 3, 14}, {255, 255, 255});

    if (V->help) {
        const char* lines[] = {
            "SPACE   PAUSE / PLAY",   "< >     SCRUB DISTORTION", "R       RESTART STORY",
            "C       CONSTRUCTION LINES", "B       DOUBLE BUFFERING", "V       VSYNC",
            "1-8     TECHNIQUE PAGES", "TAB     NEXT PAGE",        "F11     FULLSCREEN",
            "P       SCREENSHOT",     "H       HIDE HELP",        "ESC     QUIT"};
        const int n = sizeof lines / sizeof lines[0], w = 190, h = 34 + n * 10;
        const int x0 = CANVAS_W - w - 8, yy = 8;
        fillRectAlpha(c, {x0, yy, w, h}, {10, 8, 16}, 0.7);
        drawText(c, x0 + 8, yy + 7, "CONTROLS", {120, 200, 210});
        for (int i = 0; i < n; ++i) drawText(c, x0 + 8, yy + 22 + i * 10, lines[i], {225, 225, 235});
    }
}

void drawToast(Framebuffer& c) {
    if (V->toastTimer <= 0) return;
    double a = std::min(1.0, V->toastTimer / 0.5);  // fade out during the last half second
    int w = textWidth(V->toast, 2) + 24;
    IRect r{(CANVAS_W - w) / 2, CANVAS_H - 76, w, 30};
    fillRectAlpha(c, r, {10, 8, 16}, 0.8 * a);
    Framebuffer txt(w, 30, {0, 0, 0});
    drawText(txt, 12, 8, V->toast, {255, 255, 255}, 2);
    Mask m = makeColorKeyMask(txt, {0, 0, 0});
    blitAlpha(c, r.x, r.y, txt, {0, 0, w, 30}, a, &m);
}

// ------------------------------------------------------------------ frame
void renderFrame() {
    const auto& pages = demoPages();
    char sub[128], right[64];
    std::snprintf(right, sizeof right, "%.0f FPS   DT %.1f MS", V->clock.fps(), V->dt * 1000);

    if (V->page == 0) {
        SceneOptions opt;
        opt.time = V->animTime;
        opt.construction = V->construction;
        renderScene(V->canvas, V->distortion, opt);
        drawSceneHud(V->canvas);
        bool recovering = V->manual < 0 && V->timeline.sample(V->story + 0.1) < V->timeline.sample(V->story) - 1e-4;
        std::string title = std::string("STAGE: ") + (recovering ? "RECOVERING" : stageName(V->distortion));
        std::snprintf(sub, sizeof sub, "STORY %4.1f / %.0f S%s%s%s", V->story, V->timeline.duration(),
                      V->paused ? "   PAUSED" : "", V->doubleBuffer ? "" : "   SINGLE BUFFER",
                      V->vsync ? "" : "   NO VSYNC");
        drawToast(V->canvas);
        drawCaptionBar(V->back, title, sub, right);
    } else {
        const DemoPage& p = pages[V->page - 1];
        if (p.animated) {
            V->canvas.clear({245, 245, 248});
            p.render(V->canvas, V->animTime);
        } else {
            V->canvas = V->staticPages[V->page - 1];
        }
        drawToast(V->canvas);
        std::snprintf(sub, sizeof sub, "%s   [%d/%zu  TAB: NEXT  0: SCENE]", p.subtitle, V->page, pages.size());
        drawCaptionBar(V->back, p.title, sub, right);
    }
    blit(V->back, 0, CAPTION_H, V->canvas, {0, 0, CANVAS_W, CANVAS_H});
}

void update(double dt) {
    V->dt = dt;
    if (V->toastTimer > 0) V->toastTimer -= dt;
    if (!V->paused) {
        V->animTime += dt;
        if (V->manual < 0) V->story = std::fmod(V->story + dt, V->timeline.duration());
    }
    // hold LEFT / RIGHT to scrub the distortion smoothly (rate is per second, so frame-rate independent)
    if (GetForegroundWindow() == V->hwnd && V->page == 0) {
        int dir = ((GetAsyncKeyState(VK_RIGHT) & 0x8000) ? 1 : 0) - ((GetAsyncKeyState(VK_LEFT) & 0x8000) ? 1 : 0);
        if (dir != 0) {
            if (V->manual < 0) V->manual = V->distortion;
            V->manual = clamp01(V->manual + dir * 0.45 * dt);
        }
    }
    V->distortion = V->manual >= 0 ? V->manual : V->timeline.sample(V->story);
}

void toggleFullscreen() {
    DWORD style = GetWindowLong(V->hwnd, GWL_STYLE);
    if (!V->fullscreen) {
        MONITORINFO mi{};
        mi.cbSize = sizeof(mi);
        V->savedPlacement.length = sizeof(WINDOWPLACEMENT);
        if (GetWindowPlacement(V->hwnd, &V->savedPlacement) &&
            GetMonitorInfo(MonitorFromWindow(V->hwnd, MONITOR_DEFAULTTOPRIMARY), &mi)) {
            SetWindowLong(V->hwnd, GWL_STYLE, style & ~WS_OVERLAPPEDWINDOW);
            SetWindowPos(V->hwnd, HWND_TOP, mi.rcMonitor.left, mi.rcMonitor.top,
                         mi.rcMonitor.right - mi.rcMonitor.left, mi.rcMonitor.bottom - mi.rcMonitor.top,
                         SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
            V->fullscreen = true;
        }
    } else {
        SetWindowLong(V->hwnd, GWL_STYLE, style | WS_OVERLAPPEDWINDOW);
        SetWindowPlacement(V->hwnd, &V->savedPlacement);
        SetWindowPos(V->hwnd, nullptr, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
        V->fullscreen = false;
    }
}

void screenshot() {
    std::string dir = V->outDir + "/screenshots";
    std::filesystem::create_directories(dir);
    char name[64];
    do {  // never overwrite screenshots from an earlier run
        std::snprintf(name, sizeof name, "/shot_%03d.png", ++V->shots);
    } while (std::filesystem::exists(dir + name));
    if (V->back.savePNG(dir + name)) showToast(std::string("SAVED SCREENSHOTS") + name);
}

void onKey(WPARAM key) {
    const int pageCount = (int)demoPages().size();
    bool shift = GetKeyState(VK_SHIFT) & 0x8000;
    switch (key) {
        case VK_ESCAPE:
            if (V->fullscreen) toggleFullscreen();
            else DestroyWindow(V->hwnd);
            break;
        case VK_SPACE:
            V->paused = !V->paused;
            showToast(V->paused ? "PAUSED" : "PLAYING");
            break;
        case 'R':
            V->story = 0;
            V->manual = -1;
            V->paused = false;
            showToast("STORY RESTARTED");
            break;
        case 'C':
            V->construction = !V->construction;
            showToast(V->construction ? "CONSTRUCTION LINES ON" : "CONSTRUCTION LINES OFF");
            break;
        case 'B':
            V->doubleBuffer = !V->doubleBuffer;
            showToast(V->doubleBuffer ? "DOUBLE BUFFERING ON" : "SINGLE BUFFER: WATCH IT FLICKER");
            break;
        case 'V':
            V->vsync = !V->vsync;
            showToast(V->vsync ? "VSYNC ON" : "VSYNC OFF: UNCAPPED FPS");
            break;
        case 'H': V->help = !V->help; break;
        case 'P': screenshot(); break;
        case VK_F11:
        case 'F': toggleFullscreen(); break;
        case VK_TAB: V->page = (V->page + (shift ? pageCount : 1)) % (pageCount + 1); break;
        case 'S':
        case '0': V->page = 0; break;
        default:
            if (key >= '1' && key <= (WPARAM)('0' + pageCount)) V->page = (int)(key - '0');
            break;
    }
}

LRESULT CALLBACK wndProc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
            if (!(lp & (1 << 30))) onKey(wp);  // bit 30 set = auto-repeat: ignore
            return 0;
        case WM_ERASEBKGND: return 1;  // we repaint everything ourselves; erasing would flicker
        case WM_PAINT: {
            PAINTSTRUCT ps;
            BeginPaint(h, &ps);
            EndPaint(h, &ps);
            if (V) present();
            return 0;
        }
        case WM_DESTROY:
            if (V) V->running = false;
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcW(h, msg, wp, lp);
}

}  // namespace

bool liveViewerAvailable() { return true; }

int runLiveViewer(const std::string& outDir) {
    SetProcessDPIAware();  // 1 window pixel = 1 screen pixel, so the image is not blurred by Windows
    Viewer viewer;
    V = &viewer;
    viewer.outDir = outDir;
    viewer.bgra.resize((size_t)PAGE_W * PAGE_H);
    viewer.bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    viewer.bmi.bmiHeader.biWidth = PAGE_W;
    viewer.bmi.bmiHeader.biHeight = -PAGE_H;  // negative = rows stored top-down, like our framebuffer
    viewer.bmi.bmiHeader.biPlanes = 1;
    viewer.bmi.bmiHeader.biBitCount = 32;
    viewer.bmi.bmiHeader.biCompression = BI_RGB;

    // The story: keyframes of the distortion value; tweening with ease-in-out fills the gaps.
    viewer.timeline.add(0, 0).add(3, 0).add(9, 0.35).add(15, 0.7).add(21, 1.0).add(26, 1.0).add(31, 0).add(34, 0);

    std::printf("Preparing technique pages...\n");
    for (const DemoPage& p : demoPages()) {
        Framebuffer f(CANVAS_W, CANVAS_H, {245, 245, 248});
        if (!p.animated) p.render(f, 0);
        viewer.staticPages.push_back(f);
    }

    HINSTANCE inst = GetModuleHandleW(nullptr);
    WNDCLASSW wc{};
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = wndProc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    wc.lpszClassName = L"ShatteredRealitiesViewer";
    RegisterClassW(&wc);

    // Largest window that fits comfortably on the screen
    RECT work;
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
    double s = std::min((work.right - work.left) * 0.9 / PAGE_W, (work.bottom - work.top) * 0.88 / PAGE_H);
    s = std::max(1.0, std::floor(s * 4) / 4);
    RECT r{0, 0, (LONG)(PAGE_W * s), (LONG)(PAGE_H * s)};
    AdjustWindowRect(&r, WS_OVERLAPPEDWINDOW, FALSE);
    viewer.hwnd = CreateWindowExW(0, wc.lpszClassName, L"Shattered Realities - live viewer",
                                  WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, r.right - r.left,
                                  r.bottom - r.top, nullptr, nullptr, inst, nullptr);
    if (!viewer.hwnd) {
        std::printf("Could not create a window.\n");
        V = nullptr;
        return 1;
    }
    ShowWindow(viewer.hwnd, SW_SHOW);
    std::printf("Live viewer running. Press H in the window for controls, Esc to quit.\n");

    viewer.clock.tick();
    int slowFlushes = 0;
    while (viewer.running) {
        MSG msg;
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) viewer.running = false;
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        if (!viewer.running) break;

        update(viewer.clock.tick());  // 1. elapsed time  2. update state

        if (viewer.doubleBuffer) {
            renderFrame();  // 3. draw the complete frame off-screen ...
            present();      // 4. ... then show it in one go
        } else {
            // Without a back buffer the screen shows each stage of drawing: first the
            // cleared screen, then the finished picture -> flicker. Our drawing is so fast
            // that we let one display refresh happen while the screen is still blank, as
            // it would when drawing directly into a visible buffer takes longer.
            viewer.back.clear({0, 0, 0});
            present();
            if (viewer.vsync) DwmFlush();
            else Sleep(8);
            renderFrame();
            present();
        }
        if (viewer.vsync) {
            // wait for the compositor's next refresh (no tearing). Some setups (remote
            // desktop, virtual machines) have no real refresh: then fall back to uncapped.
            auto t0 = std::chrono::steady_clock::now();
            HRESULT hr = DwmFlush();
            double waited = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
            slowFlushes = (FAILED(hr) || waited > 0.05) ? slowFlushes + 1 : 0;
            if (slowFlushes >= 10) {
                viewer.vsync = false;
                slowFlushes = 0;
                showToast("VSYNC UNAVAILABLE HERE: UNCAPPED");
            }
        } else {
            Sleep(0);
        }
    }
    V = nullptr;
    return 0;
}

#else  // ------------------------------------------------------------ other platforms

bool liveViewerAvailable() { return false; }
int runLiveViewer(const std::string&) { return 1; }

#endif
