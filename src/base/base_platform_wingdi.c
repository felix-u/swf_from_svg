#include <windowsx.h>
#include <dwmapi.h>

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")

struct Platform {
    using(Platform_Common, common);

    u32 *pixels;
    HWND window;
    f32 real_window_size[2];
    f32 real_mouse_position[2];
    HDC memory_dc;
    HBITMAP bitmap;
};

static void app_quit(Platform *);
static void app_start(Platform *);
static void app_update_and_render(Platform *);

static void platform_measure_text(Platform_Common *platform_, String text, f32 font_size, f32 out[2]) {
    // TODO(felix)
    discard text;
    discard font_size;
    Platform *platform = (Platform *)platform_;
    out[0] = platform->mouse_position[0];
    out[1] = platform->mouse_position[1];
}

static App_Key app_key_from_win32(WPARAM winkey) {
    App_Key key = 0;

    bool same = ' ' <= winkey && winkey <= 'Z';
    if (same) key = (App_Key)winkey;
    else switch (winkey) {
        default: {
            static bool once = false;
            if (!once) {
                log_info("unimplemented translation from win32 keycode <0x%x> (logging ONCE for all keys)", winkey);
                once = true;
            }
        } break;
    }

    return key;
}

static Platform *window_procedure__platform;
static LRESULT window_procedure__(HWND window, u32 message, WPARAM w, LPARAM l) {
    Platform *platform = window_procedure__platform;
    switch (message) {
        case WM_CLOSE: platform->should_quit = true; break;
        case WM_DESTROY: PostQuitMessage(0); break;
        case WM_DISPLAYCHANGE: {
            HMONITOR monitor = MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST);
            MONITORINFO info = { .cbSize = sizeof info };
            BOOL ok = GetMonitorInfoA(monitor, &info);
            ensure(ok);
            platform->real_window_size[0] = (f32)(info.rcMonitor.right - info.rcMonitor.left);
            platform->real_window_size[1] = (f32)(info.rcMonitor.bottom - info.rcMonitor.top);
        } break;
        case WM_DPICHANGED: {
            f32 dpi_1x = 96.f;
            platform->dpi_scale = (f32)GetDpiForWindow(window) / dpi_1x;
        } break;
        case WM_GETMINMAXINFO: { // TODO(felix): not working. The goal is to not allow the window to become smaller than a certain size
            MINMAXINFO *info = (MINMAXINFO *)l;
            int min_width = 100, min_height = 100;
            info->ptMinTrackSize.x = min_width;
            info->ptMinTrackSize.y = min_height;
        } break;
        case WM_KEYDOWN: {
            App_Key key = app_key_from_win32(w);
            platform->key_pressed[key] = !platform->key_down[key];
            platform->key_down[key] = true;
        } break;
        case WM_KEYUP: {
            App_Key key = app_key_from_win32(w);
            platform->key_down[key] = false;
        } break;
        case WM_LBUTTONDOWN: {
            App_Mouse_Button button = App_Mouse_Button_LEFT;
            platform->mouse_clicked[button] = !platform->mouse_down[button];
            platform->mouse_down[button] = true;
        } break;
        case WM_LBUTTONUP: {
            App_Mouse_Button button = App_Mouse_Button_LEFT;
            platform->mouse_down[button] = false;
        } break;
        case WM_MOUSEMOVE: {
            // NOTE(felix): we include windowsx.h just for these two lparam macros. Can we remove that and do something else?
            platform->real_mouse_position[0] = (f32)GET_X_LPARAM(l);
            platform->real_mouse_position[1] = (f32)GET_Y_LPARAM(l);
        } break;
        case WM_RBUTTONDOWN: {
            App_Mouse_Button button = App_Mouse_Button_RIGHT;
            platform->mouse_clicked[button] = !platform->mouse_down[button];
            platform->mouse_down[button] = true;
        } break;
        case WM_RBUTTONUP: {
            App_Mouse_Button button = App_Mouse_Button_RIGHT;
            platform->mouse_down[button] = false;
        } break;
        case WM_SIZE: {
            platform->real_window_size[0] = (f32)LOWORD(l);
            platform->real_window_size[1] = (f32)HIWORD(l);
        } break;
        case WM_SIZING: {
            // TODO(felix): handle redrawing here so that we're not blocked while resizing
            // NOTE(felix): apparently there might be some problems with this and the better way to do it is for the thread with window events to be different than the application thread that submits draw calls and updates the program
        } break;
    }
    return DefWindowProcA(window, message, w, l);
}

static u32 rgb__from_v4(f32 c[4]) {
    u32 rgb = (
        ((u32)(c[0] * 255.f + 0.5f) << 16) |
        ((u32)(c[1] * 255.f + 0.5f) << 8) |
        ((u32)(c[2] * 255.f + 0.5f))
    );
    return rgb;
}

static void draw__rectangle(u32 *pixels, u64 stride, u64 left, u64 top, u64 right, u64 bottom, u32 rgb) {
    for (u64 y = top; y < bottom; y += 1) {
        for (u64 x = left; x < right; x += 1) {
            pixels[y * stride + x] = rgb;
        }
    }
}

static void program(void) {
    static Arena persistent_arena;
    persistent_arena = arena_init(32 * 1024 * 1024);
    static Arena frame_arena;
    frame_arena = arena_init(32 * 1024 * 1024);

    static Platform platform = {
        .persistent_arena = &persistent_arena,
        .frame_arena = &frame_arena,
    };

    app_start(&platform);

    if (platform.window_size[0] == 0) platform.window_size[0] = 640.f;
    if (platform.window_size[1] == 0) platform.window_size[1] = 360.f;

    const char *window_name = "app";

    window_procedure__platform = &platform;
    WNDCLASSA window_class = {
        .lpfnWndProc = window_procedure__,
        .hCursor = LoadCursorA(0, IDC_ARROW),
        .lpszClassName = window_name,
    };
    ATOM atom = RegisterClassA(&window_class);
    ensure(atom != 0);

    BOOL bool_ok = SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    ensure(bool_ok);

    {
        DWORD extended_style = 0;
        const char *class_name = window_name;
        int x = CW_USEDEFAULT, y = CW_USEDEFAULT, window_width = CW_USEDEFAULT, window_height = CW_USEDEFAULT;
        platform.window = CreateWindowExA(extended_style, class_name, window_name, WS_OVERLAPPEDWINDOW, x, y, window_width, window_height, 0, 0, 0, 0);
        ensure(platform.window != 0);
    }

    ShowWindow(platform.window, SW_SHOWDEFAULT);

    {
        HDC window_dc = GetDC(platform.window);
        ensure(window_dc != 0);

        i32 virtual_width = (i32)platform.window_size[0];
        i32 virtual_height = (i32)platform.window_size[1];

        BITMAPINFOHEADER bitmap_info_header = {
            .biSize = sizeof bitmap_info_header,
            .biWidth = virtual_width,
            .biHeight = -virtual_height,
            .biPlanes = 1,
            .biBitCount = sizeof(*platform.pixels) * 8,
        };

        platform.memory_dc = CreateCompatibleDC(window_dc);
        ensure(platform.memory_dc != 0);

        platform.bitmap = CreateDIBSection(platform.memory_dc, (BITMAPINFO *)&bitmap_info_header, DIB_RGB_COLORS, (void **)&platform.pixels, 0, 0);
        ensure(platform.bitmap != 0);

        SelectObject(platform.memory_dc, platform.bitmap);

        ReleaseDC(platform.window, window_dc);
    }

    // TODO(felix): init font

    while (!platform.should_quit) {
        platform.draw_commands = (Array_Draw_Command){ .arena = platform.frame_arena };
        Scratch scratch = scratch_begin(platform.frame_arena);

        for (MSG message = {0}; PeekMessageA(&message, 0, 0, 0, PM_REMOVE);) {
            if (message.message == WM_QUIT) {
                platform.should_quit = true;
            }
            TranslateMessage(&message);
            DispatchMessageA(&message);
        }
        if (platform.should_quit) break;

        for (u64 i = 0; i < 2; i += 1) {
            platform.mouse_position[i] = platform.real_mouse_position[i] * (platform.window_size[i] / platform.real_window_size[i]);
        }

        app_update_and_render(&platform);

        i32 virtual_width = (i32)platform.window_size[0];
        i32 virtual_height = (i32)platform.window_size[1];

        {
            u32 clear = rgb__from_v4(platform.clear_color);
            u64 row = 0;
            for (u64 y = 0; y < virtual_height; y += 1, row += virtual_width) {
                for (u64 x = 0; x < virtual_width; x += 1) {
                    platform.pixels[row + x] = clear;
                }
            }
        }

        for_slice (Draw_Command *, c, platform.draw_commands) switch (c->kind) {
            case Draw_Kind_TEXT: {
                // TODO(felix)
            } break;
            case Draw_Kind_RECTANGLE: {
                u32 rgb = rgb__from_v4(c->color[Draw_Color_SOLID]);

                i32 left = (i32)c->position[0];
                i32 top = (i32)c->position[1];
                i32 width = (i32)c->rectangle.size[0];
                i32 height = (i32)c->rectangle.size[1];

                i32 bottom = CLAMP(top + height, 0, virtual_height);
                i32 right = CLAMP(left + width, 0, virtual_width);
                top = CLAMP(top, 0, virtual_height - 1);
                left = CLAMP(left, 0, virtual_width - 1);

                draw__rectangle(platform.pixels, virtual_width, left, top, right, bottom, rgb);
            } break;
            case Draw_Kind_QUADRILATERAL: {
                // TODO(felix)
            } break;
            default: unreachable;
        }

        HDC window_dc = GetDC(platform.window);
        ensure(window_dc != 0);
        {
            i32 real_width = (i32)platform.real_window_size[0];
            i32 real_height = (i32)platform.real_window_size[1];

            BOOL ok = StretchBlt(
                window_dc,
                0, 0, real_width, real_height,
                platform.memory_dc,
                0, 0, virtual_width, virtual_height,
                SRCCOPY
            );
            ensure(ok);

            #pragma comment(lib, "dwmapi.lib")
            DwmFlush(); // TODO(felix): better way to "vsync"
        }
        ReleaseDC(platform.window, window_dc);

        memset(platform.mouse_clicked, 0, sizeof platform.mouse_clicked);
        memset(platform.key_pressed, 0, sizeof platform.key_pressed);
        scratch_end(scratch);
        if (platform.should_quit) break;
    }

    DestroyWindow(platform.window);
    DeleteObject(platform.bitmap);
    DeleteDC(platform.memory_dc);

    app_quit(&platform);
}
