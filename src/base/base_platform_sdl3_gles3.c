#include <GL/gl.h>
#include "SDL3/SDL.h"
#include "SDL3/SDL_opengl_glext.h"

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

#if BASE_OS == BASE_OS_WINDOWS
    #pragma comment(lib, "opengl32.lib")
#endif

typedef enum {
    SP__UBER,
    SP__TEXTURE,
    SP__COUNT,
} Shader_Program__;

struct Platform {
    using(Platform_Common, common);

    SDL_Window *window;
    SDL_GLContext context;
    GLuint shader_program[SP__COUNT];
    GLuint vao;

    f32 font_base_scale;
    stbtt_bakedchar ascii[96];
    GLint text_u_tex;
    GLint text_u_screen_size;
    GLint text_u_color;
    GLuint text_vbo;
};

static void app_quit(Platform *);
static void app_start(Platform *);
static void app_update_and_render(Platform *);

static void platform_measure_text(Platform_Common *platform_, String text, f32 font_size, f32 out[2]) {
    Platform *platform = (Platform *)platform_;

    f32 scale = font_size / platform->font_base_scale;
    f32 x = 0;

    for (u64 i = 0; i < text.count; i += 1) {
        u8 c = text.data[i];
        if (c < 32 || c > 126) continue;

        stbtt_bakedchar *baked = &platform->ascii[c - 32];
        x += baked->xadvance * scale;
    }

    out[0] = x;
    out[1] = font_size;
}

static void sdl_ensure(const char *proc_name, bool ok) {
    if (!ok) {
        panic("SDL_%s error: '%s'", proc_name, SDL_GetError());
    }
}

#define GL__PROCS \
    X(CREATESHADER, CreateShader) \
    X(SHADERSOURCE, ShaderSource) \
    X(COMPILESHADER, CompileShader) \
    X(GETSHADERIV, GetShaderiv) \
    X(GETSHADERINFOLOG, GetShaderInfoLog) \
    X(CREATEPROGRAM, CreateProgram) \
    X(ATTACHSHADER, AttachShader) \
    X(LINKPROGRAM, LinkProgram) \
    X(GETPROGRAMIV, GetProgramiv) \
    X(GETPROGRAMINFOLOG, GetProgramInfoLog) \
    X(DETACHSHADER, DetachShader) \
    X(DELETESHADER, DeleteShader) \
    X(GENVERTEXARRAYS, GenVertexArrays) \
    X(BINDVERTEXARRAY, BindVertexArray) \
    X(GENBUFFERS, GenBuffers) \
    X(BINDBUFFER, BindBuffer) \
    X(BUFFERDATA, BufferData) \
    X(VERTEXATTRIBPOINTER, VertexAttribPointer) \
    X(ENABLEVERTEXATTRIBARRAY, EnableVertexAttribArray) \
    X(USEPROGRAM, UseProgram) \
    X(DELETEBUFFERS, DeleteBuffers) \
    X(DELETEVERTEXARRAYS, DeleteVertexArrays) \
    X(DELETEPROGRAM, DeleteProgram) \
    X(GETUNIFORMBLOCKINDEX, GetUniformBlockIndex) \
    X(UNIFORMBLOCKBINDING, UniformBlockBinding) \
    X(BINDBUFFERBASE, BindBufferBase) \
    X(BUFFERSUBDATA, BufferSubData) \
    X(BLENDFUNCSEPARATE, BlendFuncSeparate) \
    X(BLENDEQUATION, BlendEquation) \
    X(ACTIVETEXTURE, ActiveTexture) \
    X(UNIFORM1I, Uniform1i) \
    X(UNIFORM2F, Uniform2f) \
    X(UNIFORM4FV, Uniform4fv) \
    X(GETUNIFORMLOCATION, GetUniformLocation) \

#define X(NAME, name) static PFNGL##NAME##PROC gl##name;
GL__PROCS
#undef X

static App_Mouse_Button app_mouse_from_sdl(int sdl_button) {
    App_Mouse_Button button = 0;
    switch (sdl_button) {
        case SDL_BUTTON_LEFT: button = App_Mouse_Button_LEFT; break;
        case SDL_BUTTON_RIGHT: button = App_Mouse_Button_RIGHT; break;
        case SDL_BUTTON_MIDDLE: button = App_Mouse_Button_MIDDLE; break;
        default: {
            static bool logged_once = false;
            if (!logged_once) {
                log_info("unimplemented conversion from SDL_BUTTON <%d> (logging this ONCE for all buttons)", sdl_button);
                logged_once = true;
            }
        } break;
    }
    return button;
}

static App_Key app_key_from_sdl(SDL_Keycode sdl_key) {
    App_Key key = 0;
    bool lowercase_alpha = 'a' <= sdl_key && sdl_key <= 'z';
    bool visible_ascii = lowercase_alpha || sdl_key <= '`';
    if (visible_ascii) {
        if (lowercase_alpha) key = (App_Key)(sdl_key &~ 0x20); // lowercase -> uppercase
        else key = (App_Key)sdl_key;
    } else switch (sdl_key) {
        default: {
            static bool logged_once = false;
            if (!logged_once) {
                log_info("unimplemented conversion from SDLK <0x%x> (logging this ONCE for all keys)", sdl_key);
                logged_once = true;
            }
        } break;
    }
    return key;
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

    // TODO(felix): figure out SDL_SetHint for e.g. logging_level

    bool ok = false;

    ok = SDL_Init(SDL_INIT_VIDEO);
    sdl_ensure("Init", ok);

    ok = SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    sdl_ensure("GL_SetAttribute", ok);
    ok = SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    sdl_ensure("GL_SetAttribute", ok);
    ok = SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    sdl_ensure("GL_SetAttribute", ok);

    SDL_WindowFlags flags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY;
    platform.window = SDL_CreateWindow("app", 1260, 720, flags);
    sdl_ensure("CreateWindow", platform.window != 0);

    platform.context = SDL_GL_CreateContext(platform.window);
    sdl_ensure("GL_CreateContext", platform.context != 0);

    SDL_GL_MakeCurrent(platform.window, platform.context);
    SDL_GL_SetSwapInterval(1); // vsync

    #define X(NAME, name) gl##name = (PFNGL##NAME##PROC)SDL_GL_GetProcAddress("gl" #name);
    GL__PROCS
    #undef X

    enum { Shader_VS, Shader_FS, Shader_COUNT };
    const char *code[Shader_COUNT][SP__COUNT] = {
        [SP__UBER] = {
            [Shader_VS] = "#version 300 es\n" STRINGIFY(
                precision highp float;
                precision highp int;

                // NOTE(felix): mind WebGL2 / ES3 limits
                layout(std140) uniform Common_Uniforms {
                    mat4 model;           // NOTE(felix): mat4 for alignment
                    mat4 inverse_model;   // likewise
                    vec2 framebuffer_size;
                    vec4 color;           // also top_left_color

                    // 0 = rectangle
                    // 1 = quad
                    int kind;

                    // rectangle, quad
                    vec4 bottom_left_color;
                    vec4 bottom_right_color;
                    vec4 top_right_color;

                    // rectangle
                    vec2 top_left_pixels;
                    vec2 bottom_right_pixels;
                    vec4 border_color;
                    vec2 border_data; // .x = width, .y = radius

                    // arbitrary quadrilateral
                    vec2 quad_top_left;
                    vec2 quad_bottom_left;
                    vec2 quad_bottom_right;
                    vec2 quad_top_right;
                };

                vec2 ndc_from_pixels(vec2 pixels) {
                    vec2 ndc = (pixels / framebuffer_size) * 2.0 - 1.0;
                    ndc.y = -ndc.y;
                    return ndc;
                }

                vec2 pixels_from_ndc(vec2 ndc) {
                    ndc.y = -ndc.y;
                    vec2 pixels = (ndc + 1.0) * 0.5;
                    pixels *= framebuffer_size;
                    return pixels;
                }

                out vec2 v_quad_corner_uv;

                vec2 corner_from_vertex_index(int vertex_index) {
                    vec2 corners[4] = vec2[4](
                        vec2(0.0, 0.0), // bottom left
                        vec2(1.0, 0.0), // bottom right
                        vec2(1.0, 1.0), // top right
                        vec2(0.0, 1.0)  // top left
                    );
                    int indices[6] = int[6](0, 1, 2, 0, 2, 3);
                    vec2 corner = corners[indices[vertex_index]];
                    return corner;
                }

                void main() {
                    if (kind == 0) { // rectangle
                        vec2 half_size_pixels = (bottom_right_pixels - top_left_pixels) * 0.5;
                        vec2 centre_pixels = top_left_pixels + half_size_pixels;
                        float border_width = border_data.x;
                        float border_radius = border_data.y;
                        vec2 half_size_with_border_pixels = half_size_pixels + vec2(border_width);

                        vec2 corners_pixels[4] = vec2[4](
                            centre_pixels + vec2(-half_size_with_border_pixels.x, -half_size_with_border_pixels.y),
                            centre_pixels + vec2(+half_size_with_border_pixels.x, -half_size_with_border_pixels.y),
                            centre_pixels + vec2(+half_size_with_border_pixels.x, +half_size_with_border_pixels.y),
                            centre_pixels + vec2(-half_size_with_border_pixels.x, +half_size_with_border_pixels.y)
                        );

                        vec2 transformed_corners_pixels[4];
                        for (int i = 0; i < 4; i += 1) {
                            vec3 transformed_corner = mat3(model) * vec3(corners_pixels[i], 1.0);
                            transformed_corners_pixels[i] = transformed_corner.xy;
                        }

                        vec2 min_point = transformed_corners_pixels[0];
                        vec2 max_point = transformed_corners_pixels[0];
                        for (int i = 1; i < 4; i += 1) {
                            min_point = min(min_point, transformed_corners_pixels[i]);
                            max_point = max(max_point, transformed_corners_pixels[i]);
                        }

                        vec2 corner = corner_from_vertex_index(int(gl_VertexID));
                        vec2 position_ndc = mix(ndc_from_pixels(min_point), ndc_from_pixels(max_point), corner);
                        gl_Position = vec4(position_ndc, 0.0, 1.0);
                    }

                    if (kind == 1) { // quad
                        vec2 quad_corners[4] = vec2[4](
                            quad_top_left,
                            quad_bottom_left,
                            quad_bottom_right,
                            quad_top_right
                        );

                        int corner_index = 0;
                        vec2 corner = corner_from_vertex_index(int(gl_VertexID));
                        v_quad_corner_uv = corner;

                        if (corner.x < 0.5 && corner.y < 0.5) {
                            corner_index = 0;
                        } else if (corner.x > 0.5 && corner.y < 0.5) {
                            corner_index = 1;
                        } else if (corner.x > 0.5 && corner.y > 0.5) {
                            corner_index = 2;
                        } else {
                            corner_index = 3;
                        }

                        vec2 coordinate = quad_corners[corner_index];
                        vec3 transformed = mat3(model) * vec3(coordinate, 1.0);
                        vec2 position_pixels = transformed.xy;

                        vec2 position_ndc = ndc_from_pixels(position_pixels);
                        gl_Position = vec4(position_ndc, 0.0, 1.0);
                    }
                }
            ),
            [Shader_FS] = "#version 300 es\n" STRINGIFY(
                precision highp float;
                precision highp int;

                layout(std140) uniform Common_Uniforms {
                    mat4 model;
                    mat4 inverse_model;
                    vec2 framebuffer_size;
                    vec4 color;           // also acts as top_left_color

                    // 0 = rectangle
                    // 1 = quad
                    int kind;

                    // rectangle, quad
                    vec4 bottom_left_color;
                    vec4 bottom_right_color;
                    vec4 top_right_color;

                    // rectangle
                    vec2 top_left_pixels;
                    vec2 bottom_right_pixels;
                    vec4 border_color;
                    vec2 border_data; // .x = width, .y = radius

                    // arbitrary quadrilateral
                    vec2 quad_top_left;
                    vec2 quad_bottom_left;
                    vec2 quad_bottom_right;
                    vec2 quad_top_right;
                };

                vec2 ndc_from_pixels(vec2 pixels) {
                    vec2 ndc = (pixels / framebuffer_size) * 2.0 - 1.0;
                    ndc.y = -ndc.y;
                    return ndc;
                }

                vec2 pixels_from_ndc(vec2 ndc) {
                    ndc.y = -ndc.y;
                    vec2 pixels = (ndc + 1.0) * 0.5;
                    pixels *= framebuffer_size;
                    return pixels;
                }

                in vec2 v_quad_corner_uv;
                out vec4 out_color;

                void main() {
                    if (kind == 0) { // rectangle
                        vec2 size_pixels = bottom_right_pixels - top_left_pixels;
                        vec2 half_size_pixels = size_pixels * 0.5;
                        vec2 centre_pixels = top_left_pixels + half_size_pixels;

                        vec2 fragment_coordinate = gl_FragCoord.xy;
                        vec2 point_pixels = (mat3(inverse_model) * vec3(fragment_coordinate, 1.0)).xy;

                        vec2 point_relative_to_centre_pixels = point_pixels - centre_pixels;

                        float border_width = border_data.x;
                        float border_radius = border_data.y;

                        vec2 offset = abs(point_relative_to_centre_pixels) - (half_size_pixels - vec2(border_radius));
                        vec2 offset_above_0 = max(offset, vec2(0.0));
                        float outside_distance = length(offset_above_0);
                        float inside_distance  = min(max(offset.x, offset.y), 0.0);
                        float distance = outside_distance + inside_distance - border_radius;

                        float inner_distance = distance + border_width;
                        float anti_aliasing = max(1.0, fwidth(distance));
                        float outer_mask = smoothstep(anti_aliasing, -anti_aliasing, distance);
                        float inner_mask = smoothstep(anti_aliasing, -anti_aliasing, inner_distance);

                        vec2 uv = (point_pixels - top_left_pixels) / size_pixels;
                        uv = clamp(uv, 0.0, 1.0);
                        vec4 top_left_color = color;
                        vec4 top = mix(top_left_color, top_right_color, uv.x);
                        vec4 bottom = mix(bottom_left_color, bottom_right_color, uv.x);
                        vec4 fill_color = mix(bottom, top, uv.y);

                        vec4 border_mix = mix(vec4(0.0), border_color, outer_mask - inner_mask);
                        vec4 fill_mix   = mix(vec4(0.0), fill_color,  inner_mask);

                        vec4 final_color = vec4(
                            border_mix.rgb + fill_mix.rgb,
                            max(outer_mask * border_color.a, inner_mask * fill_color.a)
                        );

                        out_color = final_color;
                    }

                    if (kind == 1) { // quad
                        float u = clamp(v_quad_corner_uv.x, 0.0, 1.0);
                        float v = clamp(v_quad_corner_uv.y, 0.0, 1.0);

                        vec4 top_left_color = color;
                        vec4 c0 = top_left_color;
                        vec4 c1 = bottom_left_color;
                        vec4 c2 = bottom_right_color;
                        vec4 c3 = top_right_color;

                        vec4 bottom = mix(c0, c1, u);
                        vec4 top    = mix(c3, c2, u);
                        vec4 final_color  = mix(bottom, top, v);

                        out_color = final_color;
                    }
                }
            ),
        },
        [SP__TEXTURE] = {
            [Shader_VS] = "#version 300 es\n" STRINGIFY(
                precision highp float;
                layout (location = 0) in vec2 a_pos;
                layout (location = 1) in vec2 a_uv;
                uniform vec2 u_screen_size;
                out vec2 v_uv;
                void main() {
                    v_uv = a_uv;
                    vec2 ndc = (a_pos / u_screen_size) * 2.0 - 1.0;
                    ndc.y = -ndc.y;
                    gl_Position = vec4(ndc, 0.0, 1.0);
                }
            ),
            [Shader_FS] = "#version 300 es\n" STRINGIFY(
                precision highp float;
                in vec2 v_uv;
                uniform sampler2D u_tex;
                uniform vec4 u_color;
                out vec4 out_color;
                void main() {
                    float a = texture(u_tex, v_uv).r;
                    out_color = vec4(u_color.rgb, u_color.a * a);
                }
            ),
        },
    };
    int shader_type[Shader_COUNT] = { [Shader_VS] = GL_VERTEX_SHADER, [Shader_FS] = GL_FRAGMENT_SHADER };

    GLuint shaders[SP__COUNT][Shader_COUNT] = {0};
    for (Shader_Program__ sp = 0; sp < SP__COUNT; sp += 1) for (u64 type = 0; type < Shader_COUNT; type += 1) {
        GLuint *s = &shaders[sp][type];

        *s = glCreateShader(shader_type[type]);
        glShaderSource(*s, 1, &code[sp][type], 0);
        glCompileShader(*s);

        GLint shader_ok = 0;
        glGetShaderiv(*s, GL_COMPILE_STATUS, &shader_ok);
        if (!shader_ok) {
            char log[512];
            glGetShaderInfoLog(*s, sizeof log, 0, log);
            panic("OpenGL shader compilation failed: '%s'", log);
        }
    }

    for (Shader_Program__ sp = 0; sp < SP__COUNT; sp += 1) {
        GLuint *shader_program = &platform.shader_program[sp];

        *shader_program = glCreateProgram();
        for (u64 i = 0; i < Shader_COUNT; i += 1) glAttachShader(*shader_program, shaders[sp][i]);
        glLinkProgram(*shader_program);

        GLint gl_ok = 0;
        glGetProgramiv(*shader_program, GL_LINK_STATUS, &gl_ok);
        if (!gl_ok) {
            char log[512];
            glGetProgramInfoLog(*shader_program, sizeof log, 0, log);
            panic("OpenGL program link failed: '%s'", log);
        }

        for (u64 i = 0; i < Shader_COUNT; i += 1) glDetachShader(*shader_program, shaders[sp][i]);
        for (u64 i = 0; i < Shader_COUNT; i += 1) glDeleteShader(shaders[sp][i]);
    }

    glEnable(GL_BLEND);
    glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    glBlendEquation(GL_FUNC_ADD);

    struct {
        M4 model;
        M4 inverse_model;
        f32 framebuffer_size[2];
        f32 _pad0[2];

        f32 color[4];

        i32 kind;
        i32 _pad1[3];

        f32 bottom_left_color[4];
        f32 bottom_right_color[4];
        f32 top_right_color[4];

        f32 top_left_pixels[2];
        f32 bottom_right_pixels[2];

        f32 border_color[4];

        f32 border_data[2];
        f32 _pad2[2];

        f32 quad_top_left[2];
        f32 _pad3[2];

        f32 quad_bottom_left[2];
        f32 _pad4[2];

        f32 quad_bottom_right[2];
        f32 _pad5[2];

        f32 quad_top_right[2];
        f32 _pad6[2];
    } uniforms_cpu = {0};

    GLuint common_ubo = 0;
    glGenBuffers(1, &common_ubo);
    glBindBuffer(GL_UNIFORM_BUFFER, common_ubo);
    glBufferData(GL_UNIFORM_BUFFER, sizeof uniforms_cpu, 0, GL_DYNAMIC_DRAW);

    GLuint block_index = glGetUniformBlockIndex(platform.shader_program[SP__UBER], "Common_Uniforms");
    glUniformBlockBinding(platform.shader_program[SP__UBER], block_index, 0);

    glBindBufferBase(GL_UNIFORM_BUFFER, 0, common_ubo);

    glGenVertexArrays(1, &platform.vao);
    glBindVertexArray(platform.vao); // afaik must have one even if unused

    {
        GLuint text_program = platform.shader_program[SP__TEXTURE];
        platform.text_u_screen_size = glGetUniformLocation(text_program, "u_screen_size");
        platform.text_u_tex = glGetUniformLocation(text_program, "u_tex");
        platform.text_u_color = glGetUniformLocation(text_program, "u_color");
        glGenBuffers(1, &platform.text_vbo);
    }

    app_start(&platform);

    GLuint font_texture = 0;
    platform.font_base_scale = 48.f;
    int max_font_size = 512;
    {
        Scratch scratch = scratch_begin(platform.persistent_arena);

        u8 *bytes = arena_make(scratch.arena, max_font_size * max_font_size, u8);
        String ttf = os_read_entire_file(scratch.arena, "deps/Inter-4.1/InterVariable.ttf", 0);
        stbtt_BakeFontBitmap(ttf.data, 0, platform.font_base_scale, bytes, max_font_size, max_font_size, 32, 96, platform.ascii);

        glGenTextures(1, &font_texture);
        glBindTexture(GL_TEXTURE_2D, font_texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, max_font_size, max_font_size, 0, GL_RED, GL_UNSIGNED_BYTE, bytes);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

        scratch_end(scratch);
    }

    u64 frequency = SDL_GetPerformanceFrequency();
    u64 last_counter = SDL_GetPerformanceCounter();

    for (Scratch scratch = scratch_begin(platform.frame_arena); !platform.should_quit; scratch_end(scratch)) {
        u64 counter = SDL_GetPerformanceCounter();
        platform.seconds_since_last_frame = (f32)(counter - last_counter) / (f32)frequency;
        last_counter = counter;

        SDL_Event event = {0};
        while (SDL_PollEvent(&event)) switch (event.type) {
            case SDL_EVENT_QUIT: {
                platform.should_quit = true;
            } break;
            case SDL_EVENT_WINDOW_RESIZED: case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED: {
                int pw = 0, ph = 0;
                SDL_GetWindowSizeInPixels(platform.window, &pw, &ph);
                platform.window_size[0] = (f32)pw;
                platform.window_size[1] = (f32)ph;
                platform.dpi_scale = SDL_GetWindowDisplayScale(platform.window);
            } break;
            case SDL_EVENT_MOUSE_MOTION: {
                platform.mouse_position[0] = (f32)event.motion.x;
                platform.mouse_position[1] = (f32)event.motion.y;
            } break;
            case SDL_EVENT_MOUSE_BUTTON_DOWN: {
                App_Mouse_Button button = app_mouse_from_sdl(event.button.button);
                if (!platform.mouse_down[button]) platform.mouse_clicked[button] = true;
                platform.mouse_down[button] = true;
            } break;
            case SDL_EVENT_MOUSE_BUTTON_UP: {
                App_Mouse_Button button = app_mouse_from_sdl(event.button.button);
                platform.mouse_down[button] = false;
            } break;
            case SDL_EVENT_KEY_DOWN: {
                if (!event.key.repeat) {
                    App_Key key = app_key_from_sdl(event.key.key);
                    if (!platform.key_down[key]) platform.key_pressed[key] = true;
                    platform.key_down[key] = true;
                }
            } break;
            case SDL_EVENT_KEY_UP: {
                App_Key key = app_key_from_sdl(event.key.key);
                platform.key_down[key] = false;
            } break;
            default: break;
        }
        if (platform.should_quit) {
            scratch_end(scratch);
            break;
        }

        platform.draw_commands = (Array_Draw_Command){ .arena = platform.frame_arena };
        app_update_and_render(&platform);

        glViewport(0, 0, (int)platform.window_size[0], (int)platform.window_size[1]);
        {
            f32 *c = platform.clear_color;
            glClearColor(c[0], c[1], c[2], c[3]);
        }
        glClear(GL_COLOR_BUFFER_BIT);

        v_copy(uniforms_cpu.framebuffer_size, platform.window_size);

        glBindBuffer(GL_UNIFORM_BUFFER, common_ubo);
        glUseProgram(platform.shader_program[SP__UBER]);
        glBindVertexArray(platform.vao);

        for_slice (Draw_Command *, command, platform.draw_commands) switch (command->kind) {
            case Draw_Kind_TEXT: {
                f32 x = command->position[0];
                f32 y = command->position[1];
                f32 scale = command->text.font_size / platform.font_base_scale;

                glUseProgram(platform.shader_program[SP__TEXTURE]);
                glBindVertexArray(platform.vao);
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, font_texture);
                glUniform1i(platform.text_u_tex, 0);

                glUniform2f(platform.text_u_screen_size,
                            platform.window_size[0],
                            platform.window_size[1]);

                glUniform4fv(platform.text_u_color, 1, command->color[Draw_Color_SOLID]);

                for (u64 i = 0; i < command->text.string.count; i++) {
                    char c = command->text.string.data[i];
                    if (c < 32 || c > 126) continue;

                    stbtt_bakedchar *b = &platform.ascii[c - 32];

                    f32 x0 = x + b->xoff * scale;
                    f32 y0 = y + b->yoff * scale;
                    f32 x1 = x0 + (b->x1 - b->x0) * scale;
                    f32 y1 = y0 + (b->y1 - b->y0) * scale;

                    f32 max = 1.f / (f32)max_font_size;
                    f32 u0 = b->x0 * max;
                    f32 v0 = b->y0 * max;
                    f32 u1 = b->x1 * max;
                    f32 v1 = b->y1 * max;

                    f32 verts[] = {
                        x0, y0, u0, v0,
                        x1, y0, u1, v0,
                        x0, y1, u0, v1,
                        x1, y1, u1, v1,
                    };

                    glBindBuffer(GL_ARRAY_BUFFER, platform.text_vbo);
                    glBufferData(GL_ARRAY_BUFFER, sizeof verts, verts, GL_DYNAMIC_DRAW);

                    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(f32)*4, (void*)0);
                    glEnableVertexAttribArray(0);
                    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(f32)*4, (void*)(8));
                    glEnableVertexAttribArray(1);

                    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

                    x += b->xadvance * scale;
                }
            } break;
            case Draw_Kind_RECTANGLE: {
                assert(command->rectangle.size[0] >= 0);
                assert(command->rectangle.size[1] >= 0);

                f32 pivot[2] = {0};
                if (v_equals_(command->rectangle.pivot, (f32[2]){0}, 2)) {
                    f32 add[2]; v_copy(add, command->rectangle.size);
                    v_scale(add, 0.5f);
                    v_copy(pivot, command->position);
                    v_add(pivot, add);
                } else {
                    v_copy(pivot, command->rectangle.pivot);
                }

                M3 model_m3 = m3_from_rotation(command->rectangle.rotation_radians, pivot);
                M3 inv_m3   = m3_inverse(model_m3);

                uniforms_cpu.model = m4_from_top_left_m3(model_m3);
                uniforms_cpu.inverse_model = m4_from_top_left_m3(inv_m3);

                uniforms_cpu.kind = 0;

                v_copy(uniforms_cpu.top_left_pixels, command->position);

                v_copy(uniforms_cpu.bottom_right_pixels, command->position);
                v_add(uniforms_cpu.bottom_right_pixels, command->rectangle.size);

                uniforms_cpu.border_data[0] = command->rectangle.border_width;
                uniforms_cpu.border_data[1] = command->rectangle.border_radius;

                f32 (*c)[4] = command->color;

                v_copy_(uniforms_cpu.color, c[Draw_Color_SOLID], 4);
                v_copy_(uniforms_cpu.bottom_left_color,
                        command->gradient ? c[Draw_Color_BOTTOM_LEFT] : c[Draw_Color_SOLID],
                        4);
                v_copy_(uniforms_cpu.bottom_right_color,
                        command->gradient ? c[Draw_Color_BOTTOM_RIGHT] : c[Draw_Color_SOLID],
                        4);
                v_copy_(uniforms_cpu.top_right_color,
                        command->gradient ? c[Draw_Color_TOP_RIGHT] : c[Draw_Color_SOLID],
                        4);
                v_copy(uniforms_cpu.border_color, command->rectangle.border_color);

                glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof uniforms_cpu, &uniforms_cpu);
                glDrawArrays(GL_TRIANGLES, 0, 6);
            } break;

            case Draw_Kind_QUADRILATERAL: {
                uniforms_cpu.model = m4_fill_diagonal(1.f);
                uniforms_cpu.inverse_model = m4_fill_diagonal(1.f);
                uniforms_cpu.kind = 1;

                v_copy(uniforms_cpu.quad_top_left,     command->quadrilateral[Draw_Corner_TOP_LEFT]);
                v_copy(uniforms_cpu.quad_bottom_left,  command->quadrilateral[Draw_Corner_BOTTOM_LEFT]);
                v_copy(uniforms_cpu.quad_bottom_right, command->quadrilateral[Draw_Corner_BOTTOM_RIGHT]);
                v_copy(uniforms_cpu.quad_top_right,    command->quadrilateral[Draw_Corner_TOP_RIGHT]);

                f32 (*c)[4] = command->color;

                v_copy_(uniforms_cpu.color, c[Draw_Color_SOLID], 4);
                v_copy_(uniforms_cpu.bottom_left_color,
                        command->gradient ? c[Draw_Color_BOTTOM_LEFT] : c[Draw_Color_SOLID],
                        4);
                v_copy_(uniforms_cpu.bottom_right_color,
                        command->gradient ? c[Draw_Color_BOTTOM_RIGHT] : c[Draw_Color_SOLID],
                        4);
                v_copy_(uniforms_cpu.top_right_color,
                        command->gradient ? c[Draw_Color_TOP_RIGHT] : c[Draw_Color_SOLID],
                        4);

                v_copy(uniforms_cpu.border_color, command->rectangle.border_color);

                glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof uniforms_cpu, &uniforms_cpu);
                glDrawArrays(GL_TRIANGLES, 0, 6);
            } break;

            default: unreachable;
        }

        SDL_GL_SwapWindow(platform.window);
        memset(platform.mouse_clicked, 0, sizeof platform.mouse_clicked);
        memset(platform.key_pressed, 0, sizeof platform.key_pressed);
    }

    app_quit(&platform);
    glDeleteVertexArrays(1, &platform.vao);
    glDeleteBuffers(1, &common_ubo);
    for (Shader_Program__ sp = 0; sp < SP__COUNT; sp += 1) glDeleteProgram(platform.shader_program[sp]);
    SDL_DestroyWindow(platform.window);
    SDL_Quit();
}
