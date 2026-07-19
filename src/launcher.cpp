#include "launcher.h"

#include <cctype>
#include <string>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <windowsx.h>
#include <commdlg.h>
#include <shellapi.h>

#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "msimg32.lib")

namespace scaler
{
namespace
{
constexpr int kWinW = 920;
constexpr int kWinH = 560;

struct UiState
{
    LaunchConfig* cfg       = nullptr;
    HWND          hwnd      = nullptr;
    std::wstring  romDisplay;
    int           hoverBtn  = -1; // 0 browse, 1 play, 2 quit; 10+ mode; 20+ res
    bool          playArmed = false;
    HFONT         fontBrand = nullptr;
    HFONT         fontHead  = nullptr;
    HFONT         fontBody  = nullptr;
    HFONT         fontSmall = nullptr;
    HBITMAP       backBmp   = nullptr;
    HDC           backDc    = nullptr;
    int           backW     = 0;
    int           backH     = 0;
};

UiState* g_ui = nullptr;

RECT R(int x, int y, int w, int h)
{
    return RECT { x, y, x + w, y + h };
}

bool hit(const RECT& r, int x, int y)
{
    return x >= r.left && x < r.right && y >= r.top && y < r.bottom;
}

std::wstring utf8ToWide(const std::string& s)
{
    if (s.empty())
        return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
    std::wstring out(n, 0);
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), out.data(), n);
    return out;
}

std::string wideToUtf8(const std::wstring& s)
{
    if (s.empty())
        return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0, nullptr, nullptr);
    std::string out(n, 0);
    WideCharToMultiByte(CP_UTF8, 0, s.c_str(), (int)s.size(), out.data(), n, nullptr, nullptr);
    return out;
}

std::wstring fileNameOnly(const std::wstring& path)
{
    auto p = path.find_last_of(L"\\/");
    return p == std::wstring::npos ? path : path.substr(p + 1);
}

void ensureBackBuffer(HDC hdc, int w, int h)
{
    if (!g_ui)
        return;
    if (g_ui->backDc && g_ui->backW == w && g_ui->backH == h)
        return;
    if (g_ui->backBmp)
        DeleteObject(g_ui->backBmp);
    if (g_ui->backDc)
        DeleteDC(g_ui->backDc);
    g_ui->backDc  = CreateCompatibleDC(hdc);
    g_ui->backBmp = CreateCompatibleBitmap(hdc, w, h);
    SelectObject(g_ui->backDc, g_ui->backBmp);
    g_ui->backW = w;
    g_ui->backH = h;
}

void fillGradientV(HDC hdc, const RECT& rc, COLORREF top, COLORREF bot)
{
    TRIVERTEX v[2] {};
    v[0].x     = rc.left;
    v[0].y     = rc.top;
    v[0].Red  = (COLOR16)(GetRValue(top) << 8);
    v[0].Green = (COLOR16)(GetGValue(top) << 8);
    v[0].Blue  = (COLOR16)(GetBValue(top) << 8);
    v[0].Alpha = 0;
    v[1].x     = rc.right;
    v[1].y     = rc.bottom;
    v[1].Red  = (COLOR16)(GetRValue(bot) << 8);
    v[1].Green = (COLOR16)(GetGValue(bot) << 8);
    v[1].Blue  = (COLOR16)(GetBValue(bot) << 8);
    v[1].Alpha = 0;
    GRADIENT_RECT gr { 0, 1 };
    GradientFill(hdc, v, 2, &gr, 1, GRADIENT_FILL_RECT_V);
}

void fillRound(HDC hdc, const RECT& rc, int rad, COLORREF fill, COLORREF border)
{
    HBRUSH br = CreateSolidBrush(fill);
    HPEN   pn = CreatePen(PS_SOLID, 1, border);
    HGDIOBJ ob = SelectObject(hdc, br);
    HGDIOBJ op = SelectObject(hdc, pn);
    RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, rad, rad);
    SelectObject(hdc, ob);
    SelectObject(hdc, op);
    DeleteObject(br);
    DeleteObject(pn);
}

void drawText(HDC hdc, HFONT font, const RECT& rc, const wchar_t* text, COLORREF color, UINT fmt)
{
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, color);
    HGDIOBJ old = SelectObject(hdc, font);
    DrawTextW(hdc, text, -1, const_cast<RECT*>(&rc), fmt);
    SelectObject(hdc, old);
}

// Layout regions
RECT brandRect() { return R(48, 40, 520, 56); }
RECT tagRect() { return R(52, 100, 500, 28); }
RECT romBox() { return R(48, 200, 560, 48); }
RECT browseBtn() { return R(620, 200, 120, 48); }
RECT modeLabel() { return R(48, 280, 200, 24); }
RECT resLabel() { return R(48, 360, 200, 24); }
RECT playBtn() { return R(48, 450, 220, 56); }
RECT quitBtn() { return R(288, 450, 120, 56); }
RECT sidePanel() { return R(620, 280, 250, 200); }

RECT modeChip(int i)
{
    // Clarity, Smooth, Neural in a row
    return R(48 + i * 150, 308, 140, 36);
}

RECT resChip(int i)
{
    return R(48 + i * 120, 388, 108, 36);
}

const wchar_t* modeChipName(int i)
{
    static const wchar_t* n[] = { L"Clarity", L"Smooth", L"Neural" };
    return n[i];
}

ScaleMode modeFromChip(int i)
{
    switch (i)
    {
    case 0:
        return ScaleMode::TinySR;
    case 1:
        return ScaleMode::Smooth;
    default:
        return ScaleMode::Neural;
    }
}

int chipFromMode(ScaleMode m)
{
    if (m == ScaleMode::TinySR)
        return 0;
    if (m == ScaleMode::Neural)
        return 2;
    return 1; // Smooth default for other modes in UI
}

OutputRes resFromChip(int i)
{
    switch (i)
    {
    case 0:
        return OutputRes::Res1080p;
    case 1:
        return OutputRes::Res1440p;
    default:
        return OutputRes::Res4K;
    }
}

int chipFromRes(OutputRes r)
{
    if (r == OutputRes::Res1080p)
        return 0;
    if (r == OutputRes::Res1440p)
        return 1;
    return 2;
}

const wchar_t* resChipName(int i)
{
    static const wchar_t* n[] = { L"1080p", L"1440p", L"4K" };
    return n[i];
}

bool browseRom(HWND hwnd)
{
    wchar_t file[MAX_PATH] = {};
    OPENFILENAMEW ofn {};
    ofn.lStructSize  = sizeof(ofn);
    ofn.hwndOwner    = hwnd;
    ofn.lpstrFilter  = L"NES ROMs (*.nes)\0*.nes\0All files\0*.*\0";
    ofn.lpstrFile    = file;
    ofn.nMaxFile     = MAX_PATH;
    ofn.Flags        = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_EXPLORER;
    ofn.lpstrTitle   = L"Choose a NES ROM";
    if (!GetOpenFileNameW(&ofn))
        return false;
    g_ui->cfg->romPath = wideToUtf8(file);
    g_ui->romDisplay   = fileNameOnly(file);
    g_ui->playArmed    = !g_ui->cfg->romPath.empty();
    InvalidateRect(hwnd, nullptr, FALSE);
    return true;
}

void paint(HWND hwnd)
{
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);
    RECT client {};
    GetClientRect(hwnd, &client);
    ensureBackBuffer(hdc, client.right, client.bottom);
    HDC dc = g_ui->backDc;

    // Atmosphere: deep ink ? slightly warmer base (not flat, not purple)
    fillGradientV(dc, client, RGB(14, 18, 28), RGB(28, 22, 18));

    // Subtle scanline texture
    for (int y = 0; y < client.bottom; y += 3)
    {
        HPEN pen = CreatePen(PS_SOLID, 1, RGB(20, 24, 34));
        HGDIOBJ op = SelectObject(dc, pen);
        MoveToEx(dc, 0, y, nullptr);
        LineTo(dc, client.right, y);
        SelectObject(dc, op);
        DeleteObject(pen);
    }

    // Soft amber glow blob (left)
    {
        RECT glow = R(-40, 120, 420, 320);
        fillRound(dc, glow, 200, RGB(42, 28, 16), RGB(42, 28, 16));
    }

    // Brand
    drawText(dc, g_ui->fontBrand, brandRect(), L"RetroNNetScaler", RGB(245, 236, 220),
             DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    drawText(dc,
             g_ui->fontHead,
             tagRect(),
             L"Sharp pixels. Clean diagonals. Hard palette.",
             RGB(180, 150, 110),
             DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    // ROM field
    const bool romHover = g_ui->hoverBtn == 0;
    fillRound(dc, romBox(), 10, RGB(22, 26, 36), romHover ? RGB(220, 160, 70) : RGB(55, 50, 45));
    std::wstring romTxt = g_ui->romDisplay.empty() ? L"Drop a .nes ROM here, or browse..." : g_ui->romDisplay;
    RECT romText = romBox();
    InflateRect(&romText, -16, 0);
    drawText(dc,
             g_ui->fontBody,
             romText,
             romTxt.c_str(),
             g_ui->romDisplay.empty() ? RGB(120, 110, 95) : RGB(235, 225, 210),
             DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    // Browse
    fillRound(dc,
              browseBtn(),
              10,
              romHover ? RGB(55, 42, 28) : RGB(36, 32, 28),
              RGB(220, 160, 70));
    drawText(dc, g_ui->fontBody, browseBtn(), L"Browse", RGB(245, 220, 170),
             DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    // Mode chips
    drawText(dc, g_ui->fontSmall, modeLabel(), L"SCALE", RGB(140, 125, 100), DT_LEFT | DT_SINGLELINE);
    const int selMode = chipFromMode(g_ui->cfg->prefs.scaleMode);
    for (int i = 0; i < 3; ++i)
    {
        RECT c    = modeChip(i);
        bool sel  = i == selMode;
        bool hov  = g_ui->hoverBtn == 10 + i;
        COLORREF fill = sel ? RGB(210, 140, 50) : (hov ? RGB(40, 34, 28) : RGB(26, 28, 34));
        COLORREF border = sel ? RGB(240, 190, 100) : RGB(60, 55, 48);
        COLORREF text = sel ? RGB(20, 16, 12) : RGB(210, 200, 185);
        fillRound(dc, c, 8, fill, border);
        drawText(dc, g_ui->fontBody, c, modeChipName(i), text, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }

    // Res chips
    drawText(dc, g_ui->fontSmall, resLabel(), L"OUTPUT", RGB(140, 125, 100), DT_LEFT | DT_SINGLELINE);
    const int selRes = chipFromRes(g_ui->cfg->prefs.outputRes);
    for (int i = 0; i < 3; ++i)
    {
        RECT c    = resChip(i);
        bool sel  = i == selRes;
        bool hov  = g_ui->hoverBtn == 20 + i;
        COLORREF fill = sel ? RGB(70, 110, 120) : (hov ? RGB(40, 34, 28) : RGB(26, 28, 34));
        COLORREF border = sel ? RGB(120, 180, 190) : RGB(60, 55, 48);
        COLORREF text = sel ? RGB(240, 248, 250) : RGB(210, 200, 185);
        fillRound(dc, c, 8, fill, border);
        drawText(dc, g_ui->fontBody, c, resChipName(i), text, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }

    // Side note
    RECT side = sidePanel();
    drawText(dc,
             g_ui->fontSmall,
             side,
             L"In-game\n"
             L"F1  cycle scale\n"
             L"F3  cycle res\n"
             L"[ ] sharpness\n"
             L"F11 fullscreen\n"
             L"Esc quit",
             RGB(110, 100, 90),
             DT_LEFT | DT_TOP | DT_WORDBREAK);

    // Play CTA
    const bool canPlay = !g_ui->cfg->romPath.empty();
    const bool playHov = g_ui->hoverBtn == 1 && canPlay;
    COLORREF playFill = !canPlay ? RGB(50, 45, 40)
                                 : (playHov ? RGB(235, 165, 55) : RGB(210, 140, 50));
    fillRound(dc, playBtn(), 12, playFill, canPlay ? RGB(255, 200, 120) : RGB(70, 65, 60));
    drawText(dc,
             g_ui->fontHead,
             playBtn(),
             L"Play",
             canPlay ? RGB(22, 16, 10) : RGB(100, 90, 80),
             DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    fillRound(dc, quitBtn(), 12, RGB(30, 32, 38), RGB(70, 65, 60));
    drawText(dc, g_ui->fontBody, quitBtn(), L"Quit", RGB(180, 170, 160),
             DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    BitBlt(hdc, 0, 0, client.right, client.bottom, dc, 0, 0, SRCCOPY);
    EndPaint(hwnd, &ps);
}

int hitTest(int x, int y)
{
    if (hit(browseBtn(), x, y) || hit(romBox(), x, y))
        return 0;
    if (hit(playBtn(), x, y))
        return 1;
    if (hit(quitBtn(), x, y))
        return 2;
    for (int i = 0; i < 3; ++i)
        if (hit(modeChip(i), x, y))
            return 10 + i;
    for (int i = 0; i < 3; ++i)
        if (hit(resChip(i), x, y))
            return 20 + i;
    return -1;
}

LRESULT CALLBACK LauncherProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_CREATE:
        DragAcceptFiles(hwnd, TRUE);
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT:
        paint(hwnd);
        return 0;
    case WM_MOUSEMOVE:
    {
        int x = GET_X_LPARAM(lParam);
        int y = GET_Y_LPARAM(lParam);
        int h = hitTest(x, y);
        if (h != g_ui->hoverBtn)
        {
            g_ui->hoverBtn = h;
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return 0;
    }
    case WM_LBUTTONUP:
    {
        int x = GET_X_LPARAM(lParam);
        int y = GET_Y_LPARAM(lParam);
        int h = hitTest(x, y);
        if (h == 0)
            browseRom(hwnd);
        else if (h == 1 && !g_ui->cfg->romPath.empty())
        {
            g_ui->cfg->cancelled = false;
            applyOutputRes(g_ui->cfg->prefs);
            DestroyWindow(hwnd);
        }
        else if (h == 2)
        {
            g_ui->cfg->cancelled = true;
            DestroyWindow(hwnd);
        }
        else if (h >= 10 && h < 13)
        {
            g_ui->cfg->prefs.scaleMode = modeFromChip(h - 10);
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        else if (h >= 20 && h < 23)
        {
            g_ui->cfg->prefs.outputRes = resFromChip(h - 20);
            applyOutputRes(g_ui->cfg->prefs);
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return 0;
    }
    case WM_DROPFILES:
    {
        HDROP drop = (HDROP)wParam;
        wchar_t path[MAX_PATH] {};
        if (DragQueryFileW(drop, 0, path, MAX_PATH))
        {
            std::wstring p(path);
            if (p.size() > 4)
            {
                auto ext = p.substr(p.size() - 4);
                for (auto& c : ext)
                    if (c >= L'A' && c <= L'Z')
                        c = (wchar_t)(c - L'A' + L'a');
                if (ext == L".nes")
                {
                    g_ui->cfg->romPath = wideToUtf8(p);
                    g_ui->romDisplay   = fileNameOnly(p);
                    g_ui->playArmed    = true;
                    InvalidateRect(hwnd, nullptr, FALSE);
                }
            }
        }
        DragFinish(drop);
        return 0;
    }
    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE)
        {
            g_ui->cfg->cancelled = true;
            DestroyWindow(hwnd);
        }
        else if (wParam == VK_RETURN && !g_ui->cfg->romPath.empty())
        {
            g_ui->cfg->cancelled = false;
            applyOutputRes(g_ui->cfg->prefs);
            DestroyWindow(hwnd);
        }
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

HFONT makeFont(int h, int weight, const wchar_t* face)
{
    return CreateFontW(-h,
                       0,
                       0,
                       0,
                       weight,
                       FALSE,
                       FALSE,
                       FALSE,
                       DEFAULT_CHARSET,
                       OUT_DEFAULT_PRECIS,
                       CLIP_DEFAULT_PRECIS,
                       CLEARTYPE_QUALITY,
                       DEFAULT_PITCH | FF_DONTCARE,
                       face);
}
} // namespace

bool showLauncher(LaunchConfig& cfg)
{
    UiState ui;
    ui.cfg = &cfg;
    g_ui   = &ui;

    if (!cfg.romPath.empty())
        ui.romDisplay = fileNameOnly(utf8ToWide(cfg.romPath));
    ui.playArmed = !cfg.romPath.empty();

    // Prefer distinctive faces available on Win10/11
    ui.fontBrand = makeFont(42, FW_BOLD, L"Bahnschrift");
    if (!ui.fontBrand)
        ui.fontBrand = makeFont(42, FW_BOLD, L"Segoe UI");
    ui.fontHead  = makeFont(22, 600, L"Bahnschrift");
    ui.fontBody  = makeFont(16, FW_NORMAL, L"Segoe UI");
    ui.fontSmall = makeFont(13, FW_BOLD, L"Segoe UI");

    HINSTANCE hInst = GetModuleHandleW(nullptr);
    WNDCLASSW wc {};
    wc.style         = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    wc.lpfnWndProc   = LauncherProc;
    wc.hInstance     = hInst;
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;
    wc.lpszClassName = L"RetroNNetLauncher";
    RegisterClassW(&wc);

    const DWORD style = WS_POPUP | WS_VISIBLE | WS_SYSMENU;
    RECT rc { 0, 0, kWinW, kWinH };
    AdjustWindowRect(&rc, style, FALSE);
    int sw = GetSystemMetrics(SM_CXSCREEN);
    int sh = GetSystemMetrics(SM_CYSCREEN);
    int ww = rc.right - rc.left;
    int wh = rc.bottom - rc.top;

    HWND hwnd = CreateWindowExW(WS_EX_APPWINDOW | WS_EX_ACCEPTFILES,
                                wc.lpszClassName,
                                L"RetroNNetScaler",
                                style,
                                (sw - ww) / 2,
                                (sh - wh) / 2,
                                ww,
                                wh,
                                nullptr,
                                nullptr,
                                hInst,
                                nullptr);
    ui.hwnd = hwnd;
    if (!hwnd)
    {
        g_ui = nullptr;
        return false;
    }

    // Soft rounded region for the shell
    HRGN rgn = CreateRoundRectRgn(0, 0, ww + 1, wh + 1, 18, 18);
    SetWindowRgn(hwnd, rgn, TRUE);

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    MSG msg {};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0)
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    if (ui.backBmp)
        DeleteObject(ui.backBmp);
    if (ui.backDc)
        DeleteDC(ui.backDc);
    DeleteObject(ui.fontBrand);
    DeleteObject(ui.fontHead);
    DeleteObject(ui.fontBody);
    DeleteObject(ui.fontSmall);
    UnregisterClassW(L"RetroNNetLauncher", hInst);
    g_ui = nullptr;
    return !cfg.cancelled && !cfg.romPath.empty();
}
}
