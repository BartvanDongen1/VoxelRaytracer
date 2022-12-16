#include <cassert>
#include <stdlib.h>
#include "rendering/imgui-docking/imgui_impl_win32.h"

#include <vector>

// ------------------------------------------------------------------

#include "window.h"
#include "engine\inputManager.h"

// ------------------------------------------------------------------

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

static int width;
static int height;

static const UINT windowStyle;

static bool fullscreen = false;
static bool shouldClose = false;

static std::vector<BYTE> rawBuffer;

static RECT windowRect;
static HWND hwnd;

static bool isWindowRegistered = false;

static InputManager::InputDispatcher dispatcher;

#define wndMax(a, b) (((a) > (b)) ? (a) : (b))

static LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hwnd, message, wParam, lParam))
        return true;

    GetWindowLongPtr(hwnd, GWLP_USERDATA);

    switch (message)
    {
    case WM_SIZE:
    {
        RECT clientRect;
        ::GetClientRect(hwnd, &clientRect);

        unsigned int myWidth = clientRect.right - clientRect.left;
        unsigned int myHeight = clientRect.bottom - clientRect.top;

        if (width != myWidth || height != myHeight)
        {
            width = wndMax(1u, myWidth);
            height = wndMax(1u, myHeight);
        }
    } break;

    case WM_KEYDOWN:
    {
        dispatcher.updateKey(static_cast<int>(wParam), true);
    } break;

    case WM_KEYUP:
    {
        dispatcher.updateKey(static_cast<int>(wParam), false);
    } break;

    case WM_INPUT:
    {
        UINT size = 0;
        // first get the size of the input data
        if (GetRawInputData(
            reinterpret_cast<HRAWINPUT>(lParam),
            RID_INPUT,
            nullptr,
            &size,
            sizeof(RAWINPUTHEADER)) == -1)
        {
            // bail msg processing if error
            break;
        }
        rawBuffer.resize(size);
        // read in the input data
        if (GetRawInputData(
            reinterpret_cast<HRAWINPUT>(lParam),
            RID_INPUT,
            rawBuffer.data(),
            &size,
            sizeof(RAWINPUTHEADER)) != size)
        {
            // bail msg processing if error
            break;
        }
        // process the raw input data
        auto& ri = reinterpret_cast<const RAWINPUT&>(*rawBuffer.data());
        if (ri.header.dwType == RIM_TYPEMOUSE &&
            (ri.data.mouse.lLastX != 0 || ri.data.mouse.lLastY != 0))
        {
            dispatcher.updateMouse(static_cast<float>(ri.data.mouse.lLastX), static_cast<float>(ri.data.mouse.lLastY));
        }
    } break;

    case WM_MOUSEWHEEL:
    {
        if (HIWORD(wParam) == 120)
        {
            dispatcher.updateScroll(1);
        }
        else
        {
            dispatcher.updateScroll(-1);
        }

    } break;

    case WM_DESTROY:
    {
        ::PostQuitMessage(0);
    } break;

    default:
    {
        return ::DefWindowProcW(hwnd, message, wParam, lParam);
    } break;
    }

    return 0;
}

static void RegisterWindowClass(HINSTANCE hInst, const wchar_t* windowClassName)
{
    // Register a window class for creating our render window with.
    WNDCLASSEXW windowClass = {};

    windowClass.cbSize = sizeof(WNDCLASSEX);
    windowClass.style = CS_HREDRAW | CS_VREDRAW;
    windowClass.lpfnWndProc = WndProc;
    windowClass.cbClsExtra = 0;
    windowClass.cbWndExtra = 0;
    windowClass.hInstance = hInst;
    windowClass.hIcon = ::LoadIcon(hInst, IDI_WINLOGO);
    windowClass.hCursor = ::LoadCursor(NULL, IDC_ARROW);
    windowClass.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    windowClass.lpszMenuName = NULL;
    windowClass.lpszClassName = windowClassName;
    windowClass.hIconSm = ::LoadIcon(hInst, IDI_WINLOGO);

    static ATOM atom = ::RegisterClassExW(&windowClass);
    assert(atom > 0);

    isWindowRegistered = true;
}

static HWND myCreateWindow(const wchar_t* windowClassName, HINSTANCE hInst,
    const wchar_t* windowTitle, unsigned int width, unsigned int height)
{
    int screenWidth = ::GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = ::GetSystemMetrics(SM_CYSCREEN);

    windowRect = { 0, 0, static_cast<LONG>(width), static_cast<LONG>(height) };
    ::AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, FALSE);

    int windowWidth = windowRect.right - windowRect.left;
    int windowHeight = windowRect.bottom - windowRect.top;

    // Center the window within the screen. Clamp to 0, 0 for the top-left corner.
    int windowX = wndMax(0, (screenWidth - windowWidth) / 2);
    int windowY = wndMax(0, (screenHeight - windowHeight) / 2);

    HWND hWnd = ::CreateWindowExW(
        NULL,
        windowClassName,
        windowTitle,
        WS_OVERLAPPEDWINDOW,
        windowX,
        windowY,
        windowWidth,
        windowHeight,
        NULL,
        NULL,
        hInst,
        nullptr
    );

    assert(hWnd && "Failed to create window");

    return hWnd;
}

void Window::init(int aWidth, int aHeight, const char* aTitle)
{
    if (!isWindowRegistered)
    {
        RegisterWindowClass(NULL, L"Window");
    }

    size_t newsize = strlen(aTitle) + 1;
    wchar_t* wcstring = new wchar_t[newsize];

    // Convert char* string to a wchar_t* string.
    size_t convertedChars = 0;
    mbstowcs_s(&convertedChars, wcstring, newsize, aTitle, _TRUNCATE);

    width = aWidth;
    height = aHeight;
    hwnd = myCreateWindow(L"Window", 0, wcstring, width, height);


    SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)nullptr);

    ShowWindow(hwnd, SW_NORMAL);

    // register mouse raw input device
    RAWINPUTDEVICE rid;
    rid.usUsagePage = 0x01; // mouse page
    rid.usUsage = 0x02; // mouse usage
    rid.dwFlags = 0;
    rid.hwndTarget = nullptr;
    if (RegisterRawInputDevices(&rid, 1, sizeof(rid)) == FALSE)
    {
        throw GetLastError();
    }
}

void Window::confineCursor() noexcept
{
    RECT rect;
    GetClientRect(hwnd, &rect);
    MapWindowPoints(hwnd, nullptr, reinterpret_cast<POINT*>(&rect), 2);
    ClipCursor(&rect);
}

void Window::freeCursor() noexcept
{
    ClipCursor(nullptr);
}

void Window::showCursor() noexcept
{
    while (::ShowCursor(TRUE) < 0);
}

void Window::hideCursor() noexcept
{
    while (::ShowCursor(FALSE) >= 0);
}

int Window::getWidth()
{
    return width;
}

int Window::getHeight()
{
    return height;
}

HWND Window::getHwnd()
{
    return hwnd;
}

void Window::processMessages()
{
    dispatcher.update();

    MSG msg;
    while (::PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE))
    {
        ::TranslateMessage(&msg);
        ::DispatchMessage(&msg);
        if (msg.message == WM_QUIT)
            shouldClose = true;
    }
}

bool Window::getShouldClose()
{
    return shouldClose;
}
