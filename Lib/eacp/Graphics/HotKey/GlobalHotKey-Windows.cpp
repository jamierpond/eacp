#include <eacp/Core/Utils/WinInclude.h>

#include "GlobalHotKey.h"
#include <eacp/Core/App/AppEnvironment.h>
#include <eacp/Core/Utils/Logging.h>

#include <atomic>
#include <string>
#include <utility>

namespace eacp::Graphics
{

// Defined in Keyboard-Windows.cpp; maps a framework KeyCode (a macOS virtual
// key value, see Keyboard.h) to the Windows virtual key RegisterHotKey wants.
int virtualKeyFromKeyCode(uint16_t keyCode);

namespace
{
constexpr wchar_t hotKeyWindowClass[] = L"EACPGlobalHotKeyWindow";

UINT toWinModifiers(ModifierKeys modifiers)
{
    auto flags = UINT {0};

    if (modifiers.shift)
        flags |= MOD_SHIFT;
    if (modifiers.control)
        flags |= MOD_CONTROL;
    if (modifiers.alt)
        flags |= MOD_ALT;
    if (modifiers.command)
        flags |= MOD_WIN;

    // Match the macOS behaviour: one press fires once, no auto-repeat while
    // the combo is held.
    flags |= MOD_NOREPEAT;

    return flags;
}

int nextHotKeyId()
{
    static auto next = std::atomic<int> {1};
    return next++;
}
} // namespace

struct GlobalHotKey::Native
{
    Native(ModifierKeys modifiers, uint16_t keyCode, Callback callback)
        : onPressed(std::move(callback))
    {
        if (eacp::Apps::getAppEnvironment().headless)
            return;

        auto vk = virtualKeyFromKeyCode(keyCode);
        if (vk == 0)
        {
            LOG("GlobalHotKey: no Windows virtual key for keyCode "
                + std::to_string(keyCode));
            return;
        }

        if (!createWindow())
            return;

        id = nextHotKeyId();

        if (!RegisterHotKey(
                window, id, toWinModifiers(modifiers), static_cast<UINT>(vk)))
        {
            LOG("GlobalHotKey registration failed: error "
                + std::to_string(GetLastError()));
            id = 0;
            destroyWindow();
        }
    }

    ~Native()
    {
        if (id != 0)
            UnregisterHotKey(window, id);

        destroyWindow();
    }

    // A WM_HOTKEY message lands here via the event loop's DispatchMessage, so
    // the callback runs on the main thread as the contract promises — and keeps
    // firing inside nested runFor pumps and foreign modal loops.
    static LRESULT CALLBACK wndProc(HWND hwnd,
                                    UINT msg,
                                    WPARAM wParam,
                                    LPARAM lParam)
    {
        if (msg == WM_HOTKEY)
        {
            auto* self =
                reinterpret_cast<Native*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

            if (self != nullptr && self->onPressed)
                self->onPressed();

            return 0;
        }

        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }

    bool createWindow()
    {
        registerClassOnce();

        // A message-only window: invisible, no taskbar/Z-order cost, but still
        // a valid target for RegisterHotKey and posted WM_HOTKEY messages.
        window = CreateWindowExW(0,
                                 hotKeyWindowClass,
                                 L"",
                                 0,
                                 0,
                                 0,
                                 0,
                                 0,
                                 HWND_MESSAGE,
                                 nullptr,
                                 GetModuleHandleW(nullptr),
                                 nullptr);

        if (window == nullptr)
        {
            LOG("GlobalHotKey: message window creation failed, error "
                + std::to_string(GetLastError()));
            return false;
        }

        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
        return true;
    }

    void destroyWindow()
    {
        if (window != nullptr)
        {
            DestroyWindow(window);
            window = nullptr;
        }
    }

    static void registerClassOnce()
    {
        static auto registered = false;
        if (registered)
            return;

        auto wc = WNDCLASSEXW {};
        wc.cbSize = sizeof(WNDCLASSEXW);
        wc.lpfnWndProc = wndProc;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.lpszClassName = hotKeyWindowClass;
        RegisterClassExW(&wc);
        registered = true;
    }

    HWND window = nullptr;
    int id = 0;
    Callback onPressed;
};

GlobalHotKey::GlobalHotKey(ModifierKeys modifiers,
                           uint16_t keyCode,
                           Callback onPressed)
    : impl(modifiers, keyCode, std::move(onPressed))
{
}

GlobalHotKey::~GlobalHotKey() = default;

} // namespace eacp::Graphics
