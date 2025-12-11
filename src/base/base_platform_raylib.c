#include "raylib-5.5_win64_msvc16/include/raylib.h"

#if BASE_OS == BASE_OS_WINDOWS
    #pragma comment(lib, "winmm.lib")
    #pragma comment(lib, "user32.lib")
    #pragma comment(lib, "gdi32.lib")
#endif

struct Platform {
    using(Platform_Common, common);
    Font rl_font;
};

static void app_quit(Platform *);
static void app_start(Platform *);
static void app_update_and_render(Platform *);

static void platform_measure_text(Platform_Common *platform_, String text, f32 font_size, f32 out[2]) {
    Platform *platform = (Platform *)platform_;

    Scratch scratch = scratch_begin(platform->frame_arena);
    const char *text_cstring = cstring_from_string(scratch.arena, text);
    Vector2 measure = MeasureTextEx(platform->rl_font, text_cstring, font_size, 1.f);
    scratch_end(scratch);

    out[0] = measure.x;
    out[1] = measure.y;
}

static Color rl_color_from_v4(f32 c[4]) {
    Color result = {0};
    u8 *as_bytes = (u8 *)&result.r;
    for (u64 i = 0; i < 4; i += 1) as_bytes[i] = (u8)(c[i] * 255.f + 0.5f);
    return result;
}

static int rl_from_app_key(App_Key key) {
    if (key <= '`') return (int)key;
    static bool logged_once = false;
    if (!logged_once) {
        log_info("unimplemented translation: app key %d to equivalent raylib keycode (logging this ONCE for all keys)", key);
        logged_once = true;
    }
    return 0;
}

static int rl_from_app_mouse(App_Mouse_Button button) {
    static const MouseButton table[App_Mouse_Button_MAX_VALUE] = {
        [App_Mouse_Button_LEFT] = MOUSE_BUTTON_LEFT,
        [App_Mouse_Button_RIGHT] = MOUSE_BUTTON_RIGHT,
        [App_Mouse_Button_MIDDLE] = MOUSE_BUTTON_MIDDLE,
    };
    return table[button];
}

static void rl_populate_frame_info(App_Frame_Info *info) {
    info->dpi_scale = GetWindowScaleDPI().x;
    info->window_size[0] = (f32)GetScreenWidth();
    info->window_size[1] = (f32)GetScreenHeight();
    info->seconds_since_last_frame = GetFrameTime();

    for (App_Key i = 0; i < array_count(info->key_pressed); i += 1) {
        int rl_key = rl_from_app_key(i);
        if (rl_key == 0) continue;
        info->key_pressed[i] = IsKeyPressed(rl_key);
        info->key_down[i] = IsKeyDown(rl_key);
    }

    for (App_Mouse_Button i = 0; i < array_count(info->mouse_down); i += 1) {
        int rl_mouse = rl_from_app_mouse(i);
        if (rl_mouse == 0) continue;
        info->mouse_down[i] = IsMouseButtonDown(rl_mouse);
        info->mouse_clicked[i] = IsMouseButtonPressed(rl_mouse);
    }

    info->mouse_position[0] = (f32)GetMouseX();
    info->mouse_position[1] = (f32)GetMouseY();
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

    SetTraceLogLevel(LOG_WARNING);
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT | FLAG_WINDOW_HIGHDPI | FLAG_MSAA_4X_HINT);
    int monitor = GetCurrentMonitor();
    InitWindow(0, 0, "app");
    SetWindowSize(GetMonitorWidth(monitor) * 3 / 4, GetMonitorHeight(monitor) * 3 / 4);
    SetTargetFPS(GetMonitorRefreshRate(monitor));

    // TODO(felix): can the ttf bytes live on a scratch just until the font is loaded?
    String font_file = os_read_entire_file(platform.persistent_arena, "deps/Inter-4.1/InterVariable.ttf", 0);
    platform.rl_font = LoadFontFromMemory(".ttf", font_file.data, (int)font_file.count, 256, 0, 0);
    assert(IsFontValid(platform.rl_font));

    rl_populate_frame_info(&platform.frame_info);
    app_start(&platform);
    Color clear = rl_color_from_v4(platform.clear_color);

    while (!WindowShouldClose()) {
        rl_populate_frame_info(&platform.frame_info);
        platform.draw_commands = (Array_Draw_Command){ .arena = platform.frame_arena };
        Scratch scratch = scratch_begin(platform.frame_arena);

        app_update_and_render(&platform);
        clear = rl_color_from_v4(platform.clear_color);

        BeginDrawing();
        ClearBackground(clear);

        for_slice (Draw_Command *, c, platform.draw_commands) switch (c->kind) {
            case Draw_Kind_TEXT: {
                const char *as_cstring = cstring_from_string(scratch.arena, c->text.string);
                Color color = rl_color_from_v4(*c->color);
                DrawTextEx(platform.rl_font, as_cstring, *(Vector2 *)c->position, c->text.font_size, 1.f, color);
            } break;
            case Draw_Kind_RECTANGLE: {
                // TODO(felix): support border

                assert(c->rectangle.size[0] >= 0);
                assert(c->rectangle.size[1] >= 0);

                Rectangle rectangle = {
                    .x = c->position[0],
                    .y = c->position[1],
                    .width = c->rectangle.size[0],
                    .height = c->rectangle.size[1],
                };
                Color top_left_color = rl_color_from_v4(c->color[Draw_Color_TOP_LEFT]);
                Color bottom_left_color = c->gradient ? rl_color_from_v4(c->color[Draw_Color_BOTTOM_LEFT]) : top_left_color;
                Color top_right_color = c->gradient ? rl_color_from_v4(c->color[Draw_Color_TOP_RIGHT]) : top_left_color;
                Color bottom_right_color = c->gradient ? rl_color_from_v4(c->color[Draw_Color_BOTTOM_RIGHT]) : top_left_color;
                DrawRectangleGradientEx(rectangle, top_left_color, bottom_left_color, top_right_color, bottom_right_color);
            } break;
            case Draw_Kind_QUADRILATERAL: {
                // TODO(felix): support actual quad, not just rectangle

                Rectangle rectangle = {
                    .x = c->quadrilateral[Draw_Corner_TOP_LEFT][0],
                    .y = c->quadrilateral[Draw_Corner_TOP_LEFT][1],
                };
                rectangle.width = c->quadrilateral[Draw_Corner_BOTTOM_RIGHT][0] - rectangle.x;
                rectangle.height = c->quadrilateral[Draw_Corner_BOTTOM_RIGHT][1] - rectangle.y;

                Color top_left_color = rl_color_from_v4(c->color[Draw_Color_TOP_LEFT]);
                Color bottom_left_color = c->gradient ? rl_color_from_v4(c->color[Draw_Color_BOTTOM_LEFT]) : top_left_color;
                Color top_right_color = c->gradient ? rl_color_from_v4(c->color[Draw_Color_TOP_RIGHT]) : top_left_color;
                Color bottom_right_color = c->gradient ? rl_color_from_v4(c->color[Draw_Color_BOTTOM_RIGHT]) : top_left_color;
                DrawRectangleGradientEx(rectangle, top_left_color, bottom_left_color, top_right_color, bottom_right_color);
            } break;
            default: unreachable;
        }

        EndDrawing();

        memset(platform.mouse_clicked, 0, sizeof platform.mouse_clicked);
        memset(platform.key_pressed, 0, sizeof platform.key_pressed);
        scratch_end(scratch);
        if (platform.should_quit) break;
    }

    app_quit(&platform);
    UnloadFont(platform.rl_font);
    CloseWindow();
}
