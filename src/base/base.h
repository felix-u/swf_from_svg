#if !defined(BASE_H)
#define BASE_H


#include "base_context.h"

#include <stdint.h> // TODO(felix): look into removing
#include <stddef.h> // TODO(felix): look into removing
#include <stdarg.h> // NOTE(felix): doesn't seem like anything can be done about this one

#if !defined(LINK_CRT)
    #define LINK_CRT 0
#endif

#if BASE_OS == BASE_OS_EMSCRIPTEN
    #define static_assert _Static_assert
    #include <stdlib.h>
    #include <stdio.h>
    #include <fcntl.h>
    #include <unistd.h>
    #include <sys/stat.h>
    #include <sys/wait.h>
    #define os_exit(code) _exit(code)
    #define os_abort() abort()
    extern int errno;
    char *realpath(const char *file_name, char *resolved_name);
#elif BASE_OS == BASE_OS_LINUX
    #define static_assert _Static_assert
    void exit(int); // TODO(felix): can remove?
    void abort(void); // TODO(felix): own implementation to not depend on libc
    void *calloc(size_t item_count, size_t item_size); // TODO(felix): remove once using virtual alloc arena
    void free(void *pointer); // TODO(felix): remove once using virtual alloc arena
    // TODO(felix): which of these can be removed in favour of doing direct syscalls?
        #include <fcntl.h>
        #include <unistd.h>
    #include <stdio.h> // TODO(felix): only needed for FILE. remove!
#elif BASE_OS == BASE_OS_MACOS
    #define static_assert _Static_assert
    // TODO(felix): same notes as above
    void exit(int);
    #define os_exit(code) exit(code)
    void abort(void);
    #define os_abort() abort()
    void *malloc(size_t bytes);
    void *calloc(size_t item_count, size_t item_size);
    void free(void *pointer);
    #include <fcntl.h>
    #include <unistd.h>
    #include <stdio.h>
    #include <sys/stat.h>
    char *realpath(const char *file_name, char *resolved_name);
    extern int errno;
    #include <sys/wait.h>
#elif BASE_OS == BASE_OS_WINDOWS
    #define WIN32_LEAN_AND_MEAN
    #define VC_EXTRALEAN
    #define os_abort() ExitProcess(1)
    #define os_exit(code) ExitProcess(code)
    #define NOGDI
    #define NOUSER
    #define NOMINMAX
    #include "windows.h"
    #include "shellapi.h"
    #define _CRT_SECURE_NO_WARNINGS
#endif // OS

#if COMPILER_CLANG || COMPILER_GCC // they share many builtins
    #define builtin_unreachable __builtin_unreachable()
    #define force_inline inline __attribute__((always_inline))
    // TODO(felix): this is only for memcmp, etc. fix!
    #include <string.h>
#endif

#if COMPILER_CLANG
    #define builtin_assume(expr) __builtin_assume(expr)
    #define breakpoint __builtin_debugtrap()

#elif COMPILER_GCC
    #define builtin_assume(expr) { if (!(expr)) unreachable; }
    #define breakpoint __builtin_trap()

#elif COMPILER_MSVC
    #define builtin_assume(expr) __assume(expr)
    #define breakpoint __debugbreak()
    #define builtin_unreachable assert(false)
    #define force_inline inline __forceinline

#elif COMPILER_STANDARD
    // NOTE(felix): I don't even know why I have this. Getting my base layer working with a non-major compiler is not a priority
    #include <assert.h>
    #define builtin_assume(expr) assert(expr)
    #define breakpoint builtin_assume(false)
    #define builtin_unreachable assert(false)
    #define force_inline inline

#endif // COMPILER

#if defined(unreachable)
    #undef unreachable
#endif

#if BUILD_DEBUG
    #define assert(expr) { if(!(expr)) panic("failed assertion `"#expr"`"); }
    #define ensure(expression) assert(expression)
    #define unreachable panic("reached unreachable code")
#else
    #define assert(expr) builtin_assume(expr)
    #define ensure(expression) { if (!(expression)) panic("fatal condition"); }
    #define unreachable builtin_unreachable
#endif // BUILD_DEBUG

// TODO(felix): no dependency on C stdlib!
    #include <math.h> // TODO(felix): look into

#if !defined(__bool_true_false_are_defined)
    #if defined(bool)
        #undef bool
    #endif
    typedef _Bool bool;
    #if !defined(true)
        #define true 1
    #endif
    #if !defined(false)
        #define false 0
    #endif
    #define __bool_true_false_are_defined 1
#endif

#define meta(...)

typedef  uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef   int8_t  i8;
typedef  int16_t i16;
typedef  int32_t i32;
typedef  int64_t i64;
typedef    float f32;
typedef   double f64;

typedef     uintptr_t upointer;
typedef      intptr_t ipointer;

#define cast(type) (type)
#define using(type, field_name) union { type; type field_name; }

#define Slice(type) struct { type *data; u64 count; }

struct Arena;
#define Array(type) struct { \
    using(Slice_##type, slice); \
    u64 capacity; \
    struct Arena *arena; \
}

typedef Slice(u8) String;
typedef Slice(String) Slice_String;
typedef Array(String) Array_String;

#define for_slice(ptr_type, name, slice)\
    for (ptr_type name = (slice).data; (name == 0 ? false : name < (slice).data + (slice).count); name += 1)

#define array_count(arr) (sizeof(arr) / sizeof(*(arr)))
#define array_size(arr) ((arr).capacity * sizeof(*((arr).data)))

#define discard (void)

typedef Slice(u64) Slice_u64;
#define MapU64(type) struct { \
    using(Array_##type, values); \
    u64 *keys; \
    u64 *value_index_from_key_hash; \
}

#define define_container_types(type) \
    typedef Slice(type) Slice_##type; \
    typedef Array(type) Array_##type; \
    typedef MapU64(type) Map_##type;

#define enumdef(Name, type)\
    typedef type Name;\
    define_container_types(Name)\
    enum

#define structdef(Name) \
    typedef struct Name Name; \
    define_container_types(Name)\
    struct Name

#define uniondef(Name) \
    typedef union Name Name; \
    define_container_types(Name)\
    union Name

define_container_types(void)
typedef Slice(bool) Slice_bool;
typedef Array(bool) Array_bool;
typedef MapU64(bool) Map_bool;
define_container_types(u8)
define_container_types(u16)
define_container_types(u32)
typedef Array(u64) Array_u64;
typedef MapU64(u64) Map_u64;
define_container_types(i8)
define_container_types(i16)
define_container_types(i32)
define_container_types(i64)
define_container_types(f32)
define_container_types(f64)
define_container_types(upointer)
define_container_types(ipointer)

#define string_constant(s)  { .data = (u8 *)s, .count = sizeof(s) - 1 }
#define string(s) (String)string_constant(s)

#define log_error(...) log_internal("error: " __VA_ARGS__)

#define panic(...) {\
    log_internal("panic: " __VA_ARGS__);\
    breakpoint; os_abort();\
}

#define statement_macro(...) do { __VA_ARGS__ } while (0)

#define slice_from_c_array(c_array) { .data = c_array, .count = array_count(c_array) }
#define slice_of(type, ...) slice_from_c_array(((type[]){ __VA_ARGS__ }))
#define slice_get_last(s) (assert_expression((s).count > 0) ? &(s).data[(s).count - 1] : 0)
#define pop(s) (*(assert_expression((s)->count > 0) ? &(s)->data[--(s)->count] : 0))
#define slice_range(s, begin, end) { .data = assert_expression((i64)(end) - (i64)(begin) >= 0 && (end) <= (s).count) ? (s).data + (begin) : 0, .count = (end) - (begin) }
#define slice_shift(s) ((assert_expression((s)->count > 0)) ? &((s)->data++)[(s)->count--, 0] : 0)
#define slice_swap_remove(s, i) (s)->data[i] = (s)->data[--(s)->count]
#define slice_as_bytes(s) (String){ .data = (u8 *)((s).data), .count = sizeof(*((s).data)) * (s).count }
#define slice_size(s) ((s).count * (sizeof *(s).data))

#define as_bytes(value) ((String){ .data = (u8 *)value, .count = sizeof(*(value)) })

#define array_from_c_array_c(type, capacity_) { .data = (type[capacity_]){0}, .capacity = capacity_ }
#define array_from_c_array(type, capacity_) ((Array_##type)array_from_c_array_c(type, capacity_))

#define reserve_non_zero(array_pointer, item_count) \
    reserve_explicit_item_size((Array_void *)(array_pointer), item_count, sizeof(*((array_pointer)->data)), true)
#define reserve(array_pointer, item_count) \
    reserve_explicit_item_size((Array_void *)(array_pointer), item_count, sizeof(*((array_pointer)->data)), false)
static void reserve_explicit_item_size(Array_void *array, u64 item_count, u64 item_size, bool non_zero);

#define array_from_slice(s) { .data = (s).data, .count = (s).count, .capacity = (s).count }

#define push(array_pointer, item) statement_macro( \
    reserve_explicit_item_size((Array_void *)array_pointer, (array_pointer)->count + 1, sizeof(*((array_pointer)->data)), false); \
    push_assume_capacity(array_pointer, item); \
)

#define push_assume_capacity(array_pointer, item) statement_macro( \
    assert((array_pointer)->count < (array_pointer)->capacity); \
    (array_pointer)->data[(array_pointer)->count++] = item; \
)

#define push_slice(array_pointer, slice) statement_macro( \
    reserve_explicit_item_size((Array_void *)array_pointer, (array_pointer)->count + (slice).count, sizeof(*((array_pointer)->data)), false); \
    push_slice_assume_capacity(array_pointer, slice); \
)

#define push_slice_assume_capacity(array_pointer, slice) statement_macro( \
    assert((array_pointer)->count + (slice).count <= (array_pointer)->capacity); \
    for (u64 psac_i__ = 0; psac_i__ < (slice).count; psac_i__ += 1) { \
        (array_pointer)->data[(array_pointer)->count + psac_i__] = (slice).data[psac_i__]; \
    } \
    (array_pointer)->count += (slice).count; \
)

#define array_unused_capacity(array) ((array).capacity - (array).count)

#define bit_cast(type) *(type *)&

// NOTE(felix): these are function-like macros and so I would've liked them to be lowercase, but windows.h defines lowercase min,max and it's too much of a hassle to work out, especially with more than one translation unit. This is the simpler solution by far.
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define CLAMP_LOW MAX
#define CLAMP_HIGH MIN
#define CLAMP(value, low, high) CLAMP_HIGH(CLAMP_LOW(value, low), high)

#define swap(type, a, b) statement_macro( \
    type temp_a__ = *(a); *(a) = *(b); *(b) = temp_a__; \
)

// TODO(felix): rename/replace
#if BASE_OS == BASE_OS_WINDOWS
	static inline int memcmp_(void *a_, void *b_, u64 byte_count);
	#if BASE_OS == BASE_OS_WINDOWS
	    #pragma function(memcpy)
	#endif
	void *memcpy(void *destination_, const void *source_, u64 byte_count);
	#if BASE_OS == BASE_OS_WINDOWS
	    #pragma function(memset)
	#endif
	extern void *memset(void *destination_, int byte_, u64 byte_count);
#endif

#define zero(pointer) zero_(pointer, sizeof *(pointer))
static force_inline void zero_(void *pointer, u64 byte_count) {
    memset(pointer, 0, byte_count);
}

struct Arena;
static Slice_String os_get_arguments(struct Arena *arena);

structdef(Arena) {
    void *mem;
    u64 offset;
    u64 capacity;
    u64 last_offset;
};

structdef(Scratch) {
    Arena *arena;
    u64 offset;
    u64 last_offset;
};

#define arena_make(a, i, T) (T *)arena_make_((a), (i), sizeof(T), __FILE__, __LINE__, __func__)
static void *arena_make_(Arena *arena, u64 item_count, u64 item_size, const char *file, u64 line, const char *function);
static void  arena_deinit(Arena *arena);

// TODO(felix): remove this function. A zeroed reserve+commit arena will be valid and will grow on demand
static Arena arena_init(u64 initial_size_bytes);

static String arena_push(Arena *arena, String bytes);

static Scratch scratch_begin(Arena *arena);
static void    scratch_end(Scratch scratch);

#if BUILD_ASAN
    #define asan_poison_memory_region(address, byte_count)   __asan_poison_memory_region(address, byte_count)
    #define asan_unpoison_memory_region(address, byte_count) __asan_unpoison_memory_region(address, byte_count)
    #include <sanitizer/asan_interface.h>
#else
    #define asan_poison_memory_region(address, byte_count)   statement_macro( discard(address); discard(byte_count); )
    #define asan_unpoison_memory_region(address, byte_count) statement_macro( discard(address); discard(byte_count); )
#endif // BUILD_ASAN

structdef(Map_Result) { u64 index; void *pointer; bool is_new; };

#define map_get(map, key, put) map_get_((Map_void *)(map), (key), (put), sizeof *(map)->values.data)
static Map_Result map_get_(Map_void *map, u64 key, void *put, u64 item_size);

static u64 hash_djb2(String bytes);
static u64 hash_lookup_msi(u64 hash, u64 exponent, u64 index);

// TODO(felix): square root, etc. via intrinsics, not math.h (avoid linking UCRT)

uniondef(V2) {
    struct { f32 x, y; };
    f32 v[2];
};

uniondef(V3) {
    struct { f32 x, y, z; };
    struct { f32 r, g, b; };

    struct { V2 xy; f32 _0; };
    struct { f32 _1; V2 yz; };
    struct { V2 uv; f32 _2; };
    struct { f32 _3; V2 vw; };
    struct { V2 rg; f32 _4; };
    struct { f32 _5; V2 gb; };

    f32 v[3];
};

uniondef(V4) {
    struct { f32 x, y, z, w; };
    struct { f32 r, g, b, a; };
    struct { V2 top_left, bottom_right; };
    struct { f32 left, top, right, bottom; };

    struct { V2 xy, zw; };
    struct { f32 _0; V2 yz; f32 _1; };
    struct { V2 rg, ba; };
    struct { f32 _2; V2 gb; f32 _3; };

    struct { V3 xyz; f32 _4; };
    struct { V3 rgb; f32 _5; };
    struct { f32 _6; V3 yzw; };
    struct { f32 _7; V3 gba; };

    f32 v[4];
};

typedef V4 Quat;

uniondef(M3) {
    f32 c[3][3];
    V3 columns[3];
};

uniondef(M4) {
    f32 c[4][4];
    V4 columns[4];
};

#define pi_f32 3.14159265358979f

// TODO(felix): use C11 generics for generic abs()
#if COMPILER_MSVC
    #pragma intrinsic(abs, _abs64, fabs)
    #define abs_i32(x) abs(x)
    #define abs_i64(x) _abs64(x)
    #define abs_f64(x) fabs(x)
    // NOTE(felix): because I got `warning C4163: 'fabsf': not available as an intrinsic function`
    // NOTE(felix): corecrt_math.h defines fabsf as an inline function casting fabs to f32. Might be good to benchmark that approach versus what I'm doing here
    static force_inline f32 abs_f32(f32 x) { return x < 0 ? -x : x; }
#elif COMPILER_CLANG || COMPILER_GCC
    #define abs_i32(x) __builtin_abs(x)
    #define abs_i64(x) __builtin_llabs(x)
    #define abs_f32(x) __builtin_fabsf(x)
    #define abs_f64(x) __builtin_fabs(x)
#endif

#if COMPILER_MSVC
    #define count_trailing_zeroes(x) (63 - __lzcnt64((i64)(x)))
#elif COMPILER_CLANG || COMPILER_GCC
    #define count_trailing_zeroes(x) (u64)(__builtin_ctzll(x))
#endif

static bool intersect_point_in_rectangle(V2 point, V4 rectangle);
static bool is_power_of_2(u64 x);

static force_inline f32 radians_from_degrees(f32 degrees);

static V4 rgba_from_hex(u32 hex);

static       inline f32 stable_lerp(f32 a, f32 b, f32 k, f32 delta_time_seconds);
static force_inline f32 lerp(f32 a, f32 b, f32 amount);

static force_inline V2   v2(f32 value);
static force_inline V2   v2_add(V2 a, V2 b);
static force_inline V2   v2_div(V2 a, V2 b);
static       inline f32  v2_dot(V2 a, V2 b);
static force_inline bool v2_equals(V2 a, V2 b);
static       inline f32  v2_len(V2 v);
static       inline f32  v2_len_squared(V2 v);
static       inline V2   v2_lerp(V2 a, V2 b, f32 amount);
static force_inline V2   v2_max(V2 a, V2 b);
static force_inline V2   v2_min(V2 a, V2 b);
static force_inline V2   v2_mul(V2 a, V2 b);
static       inline V2   v2_norm(V2 v);
static force_inline V2   v2_reciprocal(V2 v);
static       inline V2   v2_rotate(V2 v, f32 angle_radians);
static       inline V2   v2_round(V2 v);
static       inline V2   v2_round_down(V2 v);
static force_inline V2   v2_scale(V2 v, f32 s);
static       inline V2   v2_stable_lerp(V2 a, V2 b, f32 k, f32 delta_time_seconds);
static force_inline V2   v2_sub(V2 a, V2 b);

static force_inline V3   v3_add(V3 a, V3 b);
static       inline V3   v3_cross(V3 a, V3 b);
static       inline f32  v3_dot(V3 a, V3 b);
static force_inline bool v3_equal(V3 a, V3 b);
static force_inline V3   v3_forward_from_view(M4 view);
static       inline f32  v3_len(V3 v);
static       inline f32  v3_len_squared(V3 v);
static       inline V3   v3_lerp(V3 a, V3 b, f32 amount);
static force_inline V3   v3_neg(V3 v);
static       inline V3   v3_norm(V3 v);
static force_inline V3   v3_right_from_view(M4 view);
static force_inline V3   v3_scale(V3 v, f32 s);
static force_inline V3   v3_sub(V3 a, V3 b);
static       inline V3   v3_unproject(V3 pos, M4 view_projection);
static force_inline V3   v3_up_from_view(M4 view);

static force_inline V4   v4_add(V4 a, V4 b);
static       inline f32  v4_dot(V4 a, V4 b);
static force_inline bool v4_equal(V4 a, V4 b);
static       inline V4   v4_lerp(V4 a, V4 b, f32 amount);
static       inline V4   v4_round(V4 v);
static force_inline V4   v4_scale(V4 v, f32 s);
static       inline V4   v4_stable_lerp(V4 a, V4 b, f32 k, f32 delta_time_seconds);
static force_inline V4   v4_sub(V4 a, V4 b);
static force_inline V4   v4v(V3 xyz, f32 w);

static inline Quat quat_from_rotation(V3 axis, f32 angle);
static inline Quat quat_mul_quat(Quat a, Quat b);
static inline V3   quat_rotate_v3(Quat q, V3 v);

static force_inline M3 m3_fill_diagonal(f32 value);
static       inline M3 m3_from_rotation(f32 radians, V2 pivot);
static       inline M3 m3_inverse(M3 m);
static       inline M3 m3_model(V2 scale, f32 radians, V2 pivot, V2 post_translation);
static       inline V3 m3_mul_v3(M3 m, V3 v);
static       inline M3 m3_transpose(M3 m);

static force_inline M4 m4_fill_diagonal(f32 value);
static       inline M4 m4_from_rotation(V3 axis, f32 angle);
static       inline M4 m4_from_top_left_m3(M3 m);
static       inline M4 m4_from_translation(V3 translation);
static       inline M4 m4_inverse(M4 m);
static       inline M4 m4_look_at(V3 eye, V3 centre, V3 up_direction);
static       inline M4 m4_mul_m4(M4 a, M4 b);
static       inline V4 m4_mul_v4(M4 m, V4 v);
static       inline M4 m4_perspective_projection(f32 fov_vertical_radians, f32 width_over_height, f32 range_near, f32 range_far);
static force_inline M4 m4_transpose(M4 m);

uniondef(String_Builder) {
    using(Array_u8, bytes);
    struct { String string; u64 _capacity; struct Arena *_arena; };
};

#define _for_valid_hex_digit(action)\
    action('0', 0x0) action('1', 0x1) action('2', 0x2) action('3', 0x3)\
    action('4', 0x4) action('5', 0x5) action('6', 0x6) action('7', 0x7)\
    action('8', 0x8) action('9', 0x9) action('A', 0xa) action('B', 0xb)\
    action('C', 0xc) action('D', 0xd) action('E', 0xe) action('F', 0xf)\
    action('a', 0xa) action('b', 0xb) action('c', 0xc) action('d', 0xd)\
    action('e', 0xe) action('f', 0xf)

static const u8 decimal_from_hex_digit_table[256] = {
    #define _make_hex_digit_value_table(c, val) [c] = val,
    _for_valid_hex_digit(_make_hex_digit_value_table)
};

static u64 int_from_string_base(String s, u64 base);

static char *cstring_from_string(Arena *arena, String string);

static     bool string_equals(String s1, String s2);
static   String string_from_cstring(const char *s);
static   String string_from_int_base(Arena *arena, u64 _num, u8 base);
static   String string_print_(Arena *arena, const char *fmt, va_list arguments);
static   String string_print(Arena *arena, const char *fmt, ...);
static   String string_range(String string, u64 start, u64 end);
static     bool string_starts_with(String s, String start);

static void string_builder_print(String_Builder *builder, const char* format, ...);
static void string_builder_print_(String_Builder *builder, const char *fmt, va_list arguments);

static inline void string_builder_null_terminate(String_Builder *builder);

structdef(Os_File_Info) {
    bool exists;
    char full_path[4096];
    u64 size;
    bool is_directory;
    // TODO(felix): creation, modification, and access time
};

static Os_File_Info  os_file_info(const char *relative_path);
static         void *os_heap_allocate(u64 byte_count);
static         void  os_heap_free(void *pointer);
static         bool  os_make_directory(const char *relative_path, u32 mode);
static       String  os_read_entire_file(Arena *arena, const char *relative_path, u64 max_bytes);
static         void  os_remove_file(const char *relative_path);
static         void  os_write(String bytes);
static         bool  os_write_entire_file(const char *relative_path, String bytes);

#define log_info(...) log_internal("info: " __VA_ARGS__)
#define log_internal(...) log_internal_with_location(__FILE__, __LINE__, __func__, __VA_ARGS__)
static void log_internal_with_location(const char *file, u64 line, const char *func, const char *format, ...);

typedef enum Os_Process_Flags {
    Os_Process_Flag_PRINT_COMMAND_BEFORE_RUNNING = 1 << 0,
    Os_Process_Flag_PRINT_EXIT_CODE = 1 << 1,
} Os_Process_Flags;
static u32 os_process_run(Arena arena, Slice_String arguments, String directory, Os_Process_Flags flags);

static void print(const char *format, ...);
static void print_(const char *format, va_list arguments);

typedef enum {
    Build_Compiler_MSVC,
    Build_Compiler_CLANG,
    Build_Compiler_EMCC,
    Build_Compiler_COUNT,
} Build_Compiler;

typedef enum {
    Build_Mode_DEBUG,
    Build_Mode_RELEASE,
    Build_Mode_COUNT,
} Build_Mode;

static u32 build_default_everything(Arena arena, String program_name, u8 target_os);

enumdef(App_Key, u8) {
    App_Key_NIL = 0,
    // 32 -> 96 use ASCII values
    App_Key__ASCII_DELIMITER = 128,

    App_Key_CONTROL,

    App_Key_LEFT,
    App_Key_RIGHT,

    App_Key_MAX_VALUE,
};

enumdef(App_Mouse_Button, u8) {
    App_Mouse_Button_NIL = 0,
    App_Mouse_Button_LEFT,
    App_Mouse_Button_RIGHT,
    App_Mouse_Button_MIDDLE,
    App_Mouse_Button_MAX_VALUE,
};

structdef(App_Frame_Info) {
    V2 window_size;
    f32 dpi_scale;
    f32 seconds_since_last_frame;
    V2 mouse_position;
    bool mouse_clicked[App_Mouse_Button_MAX_VALUE];
    bool mouse_down[App_Mouse_Button_MAX_VALUE];
    bool key_down[App_Key_MAX_VALUE];
    bool key_pressed[App_Key_MAX_VALUE];
};

typedef enum Draw_Kind {
    Draw_Kind_NIL = 0,
    Draw_Kind_TEXT,
    Draw_Kind_RECTANGLE,
    Draw_Kind_QUADRILATERAL,
    Draw_Kind_MAX_VALUE,
} Draw_Kind;

typedef enum Draw_Color {
    Draw_Color_SOLID = 0,

    Draw_Color_TOP_LEFT = Draw_Color_SOLID,
    Draw_Color_BOTTOM_LEFT,
    Draw_Color_BOTTOM_RIGHT,
    Draw_Color_TOP_RIGHT,

    Draw_Color_MAX_VALUE,
} Draw_Color;

typedef enum Draw_Corner {
    Draw_Corner_TOP_LEFT,
    Draw_Corner_BOTTOM_LEFT,
    Draw_Corner_BOTTOM_RIGHT,
    Draw_Corner_TOP_RIGHT,

    Draw_Corner_COUNT,
} Draw_Corner;

structdef(Draw_Command) {
    Draw_Kind kind;
    using(V2, position);
    V4 color[Draw_Color_MAX_VALUE];
    bool gradient;
    union {
        struct {
            String string;
            f32 font_size;
        } text;
        struct {
            V2 pivot;
            f32 rotation_radians;
            V4 border_color;
            f32 border_width;
            f32 border_radius;
            V2 size;
        } rectangle;
        V2 quadrilateral[Draw_Corner_COUNT];
    };
};

structdef(Platform_Common) {
    using(App_Frame_Info, frame_info);
    Arena *persistent_arena;
    Arena *frame_arena;
    Array_Draw_Command draw_commands;
    bool should_quit;
    V4 clear_color;
};

#if PLATFORM_NONE
    typedef Platform_Common Platform;
    static V2 platform_measure_text(Platform_Common *platform, String text, f32 font_size) {
        discard platform;
        discard text;
        discard font_size;
        panic("PLATFORM=NONE");
        return (V2){0};
    }
#else
    struct Platform;
    typedef struct Platform Platform;

    static V2 platform_measure_text(Platform_Common *platform, String text, f32 font_size);
#endif

#define draw(platform, ...) draw_(&(platform)->common, (Draw_Command){ __VA_ARGS__ })
static void draw_(Platform_Common *platform, Draw_Command command);

enumdef(Ui_Axis, u8) { Ui_Axis_X, Ui_Axis_Y, Ui_Axis_COUNT };

enumdef(Ui_Box_Flags, u8) {
    Ui_Box_Flag_CHILD_AXIS = 1 << 0,
    Ui_Box_Flag_DRAW_BACKGROUND = 1 << 1,
    Ui_Box_Flag_DRAW_BORDER = 1 << 2,
    Ui_Box_Flag_DRAW_TEXT = 1 << 3,
    Ui_Box_Flag_ANIMATE = 1 << 4,
    Ui_Box_Flag_HOVERABLE = 1 << 5,
    Ui_Box_Flag_CLICKABLE = 1 << 6,
    Ui_Box_Flag__FIRST_FRAME = 1 << 7,
};
#define Ui_Box_Flag_ANY_VISIBLE (Ui_Box_Flag_DRAW_BACKGROUND | Ui_Box_Flag_DRAW_BORDER | Ui_Box_Flag_DRAW_TEXT)

structdef(Ui_Box_Rectangle) {
    V2 top_left;
    V2 size;
};

enumdef(Ui_Size_Kind, u8) {
    Ui_Size_Kind_NIL = 0,
    Ui_Size_Kind_TEXT,
    Ui_Size_Kind_SUM_OF_CHILDREN,
    Ui_Size_Kind_LARGEST_CHILD,
};

enumdef(Ui_Color, u8) {
    Ui_Color_BACKGROUND,
    Ui_Color_FOREGROUND,
    Ui_Color_BORDER,
    Ui_Color_COUNT,
};

structdef(Ui_Box_Style) {
    f32 font_size;
    V2 inner_padding;
    V2 child_gap;
    V4 color[Ui_Color_COUNT];
    f32 border_width;
    f32 border_radius;
    f32 animation_speed;
};

enumdef(Ui_Box_Style_Kind, u8) {
    Ui_Box_Style_Kind_INACTIVE,
    Ui_Box_Style_Kind_HOVERED,
    Ui_Box_Style_Kind_CLICKED,
    Ui_Box_Style_Kind_COUNT,
};

structdef(Ui_Box_Style_Set) {
    Ui_Box_Style kinds[Ui_Box_Style_Kind_COUNT];
};

typedef u32 Ui_Box_Id;
define_container_types(Ui_Box_Id)

structdef(Ui_Box) {
    using(struct {
        Ui_Box_Flags flags;
        String display_string;
        Ui_Size_Kind size_kind[Ui_Axis_COUNT];
        using(struct {
            Ui_Box *parent;
            Ui_Box *previous_sibling;
            Ui_Box *next_sibling;
            Ui_Box *first_child;
            Ui_Box *last_child;
        }, links);
    }, frame_data);

    using(struct {
        using(struct {
            bool hovered;
            bool clicked;
        }, interaction);
        Ui_Box_Rectangle target_rectangle;
        Ui_Box_Style_Set target_style_set;
    }, frame_data_computed);

    using(struct {
        String hash_string;
    }, cached_data);

    using(struct {
        Ui_Box_Rectangle display_rectangle;
        Ui_Box_Style display_style;
        Ui_Box_Style_Kind target_style_kind;
    }, cached_data_computed);
};

typedef Ui_Box *Ui_Box_Pointer;
define_container_types(Ui_Box_Pointer)

structdef(Ui) {
    Platform_Common *platform;

    u64 used_box_count;
    Ui_Box *boxes;

    f32 scale;

    Ui_Box *root;
    Ui_Box *current_parent;

    Array_Ui_Box_Style_Set style_stack;

    Array_Ui_Box_Pointer boxes_to_render;
};

static   void  ui_begin(Ui *ui);
static Ui_Box *ui_button(Ui *ui, const char *format, ...);
static   void  ui_create(Ui *ui);
static   void  ui_default_render_passthrough(Ui *ui);
static   void  ui_end(Ui *ui);
static   void  ui_pop_parent(Ui *ui);

#define ui_style(ui) ui_defer_loop(ui_push_style(ui), ui_pop_style(ui))

static             void  ui_pop_style(Ui *ui);
static Ui_Box_Style_Set *ui_push_style(Ui *ui);
static           Ui_Box *ui_push(Ui *ui, bool parent, Ui_Box_Flags flags, const char *format, ...);
static           Ui_Box *ui_pushv(Ui *ui, bool parent, Ui_Box_Flags flags, const char *format, va_list arguments);
static           Ui_Box *ui_text(Ui *ui, const char *format, ...);

#define ui_expand_index(line) i__##line##__
#define ui_defer_loop_index(line) ui_expand_index(line)
#define ui_defer_loop(begin, end) \
    for ( \
        bool ui_defer_loop_index(__LINE__) = ((begin), false); \
        !ui_defer_loop_index(__LINE__); \
        ui_defer_loop_index(__LINE__) += 1, (end) \
    )
#define ui_parent(ui, axis) ui_defer_loop(ui_push((ui), true, axis, 0), ui_pop_parent(ui))


#endif // !defined(BASE_H)


#if defined(BASE_IMPLEMENTATION)


static force_inline bool assert_expression(bool value) {
    assert(value);
    return true;
}

// TODO(felix): replace with metaprogram lib specifier
#if BASE_OS == BASE_OS_WINDOWS
    #pragma comment(lib, "kernel32.lib")
    #pragma comment(lib, "shell32.lib") // needed by os_get_arguments
#endif

// TODO(felix): add slice_equal/slice_compare

static void reserve_explicit_item_size(Array_void *array, u64 item_count, u64 item_size, bool non_zero) {
    if (array->capacity >= item_count) return;

    // TODO(felix): should this be a power of 2?
    u64 new_capacity = CLAMP_LOW(1, array->capacity * 2);
    while (new_capacity < item_count) new_capacity *= 2;

    u8 *new_memory = arena_make_(array->arena, new_capacity, item_size, __FILE__, __LINE__, __func__);
    if (array->count > 0) memcpy(new_memory, array->data, array->count * item_size);

    if (!non_zero) {
        u64 old_capacity = array->capacity;
        u64 growth_byte_count = item_size * (new_capacity - old_capacity);
        u8 *beginning_of_new_memory = new_memory + (item_size * old_capacity);
        memset(beginning_of_new_memory, 0, growth_byte_count);
    }

    array->data = new_memory;
    array->capacity = new_capacity;
}

// TODO(felix): rename/replace (see base_core.h)
static inline int memcmp_(void *a_, void *b_, u64 byte_count) {
    u8 *a = a_, *b = b_;
    assert(a != 0);
    assert(b != 0);
    for (u64 i = 0; i < byte_count; i += 1) {
        if (a[i] != b[i]) return a[i] - b[i];
    }
    return 0;
}

#if !LINK_CRT
    #if COMPILER_MSVC
        #pragma function(memcpy)
    #endif
    void *memcpy(void *destination_, const void *source_, u64 byte_count) {
        u8 *destination = destination_;
        const u8 *source = source_;
        if (byte_count != 0) {
            assert(destination != 0);
            assert(source != 0);
        }
        for (u64 i = 0; i < byte_count; i += 1) destination[i] = source[i];
        return destination;
    }

    #if COMPILER_MSVC
        #pragma function(memset)
    #endif
	extern void *memset(void *destination_, int byte_, u64 byte_count) {
	    assert(byte_ < 256);
	    u8 byte = (u8)byte_;
	    u8 *destination = destination_;
	    assert(destination != 0);
	    for (u64 i = 0; i < byte_count; i += 1) destination[i] = byte;
	    return destination;
	}
#endif

#if BUILD_DEBUG && COMPILER_MSVC
    #pragma section(".raddbg", read, write)
    #define raddbg_exe_data __declspec(allocate(".raddbg"))
    raddbg_exe_data unsigned char raddbg_is_attached_byte_marker[1];
    #define raddbg_glue_(a, b) a##b
    #define raddbg_glue(a, b) raddbg_glue_(a, b)
    #define raddbg_gen_data_id() raddbg_glue(raddbg_data__, __COUNTER__)

    #define raddbg_type_view(type, ...) raddbg_exe_data char raddbg_gen_data_id()[] = ("type_view: {type: ```" #type "```, expr: ```" #__VA_ARGS__ "```}");
    #define raddbg_entry_point(...) raddbg_exe_data char raddbg_gen_data_id()[] = ("entry_point: \"" #__VA_ARGS__ "\"");
#else
    #define raddbg_type_view(...)
    #define raddbg_entry_point(...)
#endif

raddbg_entry_point(program)
raddbg_type_view(Slice_?, $.slice())
raddbg_type_view(Array_?, $.slice())
raddbg_type_view(String, view:text($.data, size=count))

static void program(void);

#if LINK_CRT || (BASE_OS & BASE_OS_ANY_POSIX)
    // TODO(felix): replace with something that feels less hacky

    static int argument_count_;
    static char **arguments_;

    int main(int argument_count, char **arguments) {
        argument_count_ = argument_count;
        arguments_ = arguments;
        program();
        os_exit(0);
    }
#elif BASE_OS == BASE_OS_WINDOWS
    void entrypoint(void) {
        program();
        os_exit(0);
    }
#endif

static Slice_String os_get_arguments(Arena *arena) {
    Array_String arguments = { .arena = arena };

    #if BASE_OS == BASE_OS_WINDOWS
        int argument_count = 0;
        u16 **arguments_utf16 = CommandLineToArgvW(GetCommandLineW(), &argument_count);
        if (arguments_utf16 == 0) {
            log_error("unable to get command line arguments");
            return (Slice_String){0};
        }

        reserve(&arguments, (u64)argument_count);

        for (u64 i = 0; i < (u64)argument_count; i += 1) {
            u16 *argument_utf16 = arguments_utf16[i];

            u64 length = 0;
            while (argument_utf16[length] != 0) length += 1;

            // TODO(felix): convert to UTF-8, not ascii
            Array_u8 argument = { .arena = arena };
            reserve(&argument, length);
            for (u64 j = 0; j < length; j += 1) {
                u16 wide_character = argument_utf16[j];
                assert(wide_character < 128);
                push_assume_capacity(&argument, (u8)wide_character);
            }
            push_assume_capacity(&arguments, bit_cast(String) argument.slice);
        }
    #elif BASE_OS & BASE_OS_ANY_POSIX
        reserve(&arguments, (u64)argument_count_);
        for (u64 i = 0; i < (u64)argument_count_; i += 1) {
            String argument = string_from_cstring(arguments_[i]);
            push_assume_capacity(&arguments, argument);
        }
    #endif

    return arguments.slice;
}

#define STRINGIFY(...) #__VA_ARGS__

static inline bool ascii_is_alpha(u8 c) { return ('A' <= c && c <= 'Z') || ('a' <= c && c <= 'z'); }

static inline bool ascii_is_decimal(u8 c) { return '0' <= c && c <= '9'; }

static inline bool ascii_is_hexadecimal(u8 c) { return ('0' <= c && c <= '9') || ('A' <= c && c <= 'F') || ('a' <= c && c <= 'f'); }

static inline bool ascii_is_whitespace(u8 c) { return c == ' ' || c == '\n' || c == '\r' || c == '\t'; }

static Arena arena_init(u64 initial_size_bytes) {
    // TODO(felix): switch to reserve+commit with (virtually) no limit: reserve something like 64gb and commit pages as needed
    #if BASE_OS == BASE_OS_WINDOWS
        Arena arena = { .mem = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, initial_size_bytes) };
    #elif BASE_OS & BASE_OS_ANY_POSIX
        Arena arena = { .mem = calloc(initial_size_bytes, 1) };
    #else
        #error "unsupported OS"
    #endif

    if (arena.mem == 0) panic("arena_init failure (requested %llu bytes)", initial_size_bytes);
    arena.capacity = initial_size_bytes;
    asan_poison_memory_region(arena.mem, arena.capacity);
    return arena;
}

static void *arena_make_(Arena *arena, u64 item_count, u64 item_size, const char *file, u64 line, const char *function) {
    if (arena == 0) {
        panic("nil arena passed at %s:%llu:%s()", file, line, function);
    }
    assert(item_size > 0);
    u64 byte_count = item_count * item_size;
    if (byte_count == 0) return 0;

    // TODO(felix): asan_poison alignment bytes
    u64 alignment = 2 * sizeof(void *);
    u64 modulo = arena->offset & (alignment - 1);
    if (modulo != 0) arena->offset += alignment - modulo;

    if (arena->capacity == 0) *arena = arena_init(8 * 1024 * 1024);

    if (arena->offset + byte_count > arena->capacity) panic("allocation failure");

    void *mem = (u8 *)arena->mem + arena->offset;
    arena->last_offset = arena->offset;
    arena->offset += byte_count;
    asan_unpoison_memory_region(mem, byte_count);
    return mem;
}

static void arena_deinit(Arena *arena) {
    asan_poison_memory_region(arena->mem, arena->capacity);

    #if BASE_OS == BASE_OS_WINDOWS
        HeapFree(GetProcessHeap(), 0, arena->mem);
    #elif BASE_OS & BASE_OS_ANY_POSIX
        free(arena->mem);
    #else
        #error "unsupported OS"
    #endif

    arena->mem = 0;
    arena->offset = 0;
    arena->capacity = 0;
}

static String arena_push(Arena *arena, String bytes) {
    u8 *bytes_on_arena = arena_make(arena, bytes.count, u8);
    memcpy(bytes_on_arena, bytes.data, bytes.count);
    String result = { .data = bytes_on_arena, .count = bytes.count };
    return result;
}

static Scratch scratch_begin(Arena *arena) {
    return (Scratch){
        .arena = arena,
        .offset = arena->offset,
        .last_offset = arena->last_offset,
    };
}

static void scratch_end(Scratch scratch) {
    asan_poison_memory_region((u8 *)scratch.arena->mem + scratch.offset, scratch.arena->capacity - scratch.offset);
    scratch.arena->offset = scratch.offset;
    scratch.arena->last_offset = scratch.last_offset;
}

static u64 hash_djb2(String bytes) {
    u64 hash = 5381;
    for (u64 i = 0; i < bytes.count; i += 1) {
        hash += (hash << 5) + bytes.data[i];
    }
    return hash;
}

static u64 hash_lookup_msi(u64 hash, u64 exponent, u64 index) {
    u64 mask = ((u64)1 << exponent) - 1;
    u64 step = (hash >> (64 - exponent)) | 1;
    u64 new = (index + step) & mask;
    return new;
}

#define MAP_MAX_LOAD_FACTOR 70

#define map_make(arena, map, capacity) statement_macro( \
    static_assert(sizeof(*(map)) == sizeof(Map_void), "Parameter must be a Map"); \
    map_make_explicit_item_size((arena), (Map_void *)(map), (capacity), sizeof(*(map)->values.data)); \
)

static void map_make_explicit_item_size(Arena *arena, Map_void *map, u64 capacity, u64 item_size) {
    capacity *= 100;
    capacity /= MAP_MAX_LOAD_FACTOR;

    map->arena = arena;
    reserve_explicit_item_size(&map->values, capacity, item_size, false);

    map->count = 1;
    map->keys = arena_make_(map->arena, capacity, sizeof *map->keys, __FILE__, __LINE__, __func__);
    map->value_index_from_key_hash = arena_make_(map->arena, capacity, sizeof *map->value_index_from_key_hash, __FILE__, __LINE__, __func__);
}

static Map_Result map_get_(Map_void *map, u64 key, void *put, u64 item_size) {
    Map_Result result = {0};

    assert(is_power_of_2(map->capacity));
    u64 exponent = count_trailing_zeroes(map->capacity);
    u64 hash = hash_djb2(as_bytes(&key));
    u64 *value_index = 0;
    for (u64 index = hash;;) {
        index = hash_lookup_msi(hash, exponent, index);
        index += !index; // never 0

        value_index = &map->value_index_from_key_hash[index];

        if (*value_index == 0) break;

        u64 key_at_index = map->keys[*value_index];
        if (key == key_at_index) break;
    }

    if (put != 0) {
        result.is_new = *value_index == 0;
        if (result.is_new) {
            ensure(map->count + 1 < map->capacity * MAP_MAX_LOAD_FACTOR / 100);

            *value_index = map->values.count;
            map->count += 1;
        }

        map->keys[*value_index] = key;

        void *pointer = (u8 *)map->values.data + *value_index * item_size;
        memcpy(pointer, put, item_size);
    }

    result.index = *value_index;
    if (result.index != 0) {
        result.pointer = (u8 *)map->values.data + result.index * item_size;
    }
    return result;
}

static bool intersect_point_in_rectangle(V2 point, V4 rectangle) {
    bool result = true;
    result = result && rectangle.left < point.x && point.x < rectangle.right;
    result = result && rectangle.top < point.y && point.y < rectangle.bottom;
    return result;
}

static bool is_power_of_2(u64 x) {
    return (x & (x - 1)) == 0;
}

static force_inline f32 radians_from_degrees(f32 degrees) { return degrees * pi_f32 / 180.f; }

static V4 rgba_from_hex(u32 hex) {
    f32 pack = 1.f / 255.f;
    V4 result = {
        .r = pack * (hex >> 24),
        .g = pack * ((hex >> 16) & 0xff),
        .b = pack * ((hex >> 8) & 0xff),
        .a = pack * (hex & 0xff),
    };
    return result;
}

static inline f32 stable_lerp(f32 a, f32 b, f32 k, f32 delta_time_seconds) {
	// Courtesy of GingerBill's BSC talk, this is framerate-independent lerping:
	// a += (b - a) * (1.0 - exp(-k * dt))
    f32 lerp_amount = 1.f - expf(-k * delta_time_seconds);
    f32 result = a + (b - a) * lerp_amount;
    return result;
}

static force_inline f32 lerp(f32 a, f32 b, f32 amount) { return a + amount * (b - a); }

static force_inline V2 v2(f32 value) { return (V2){ .x = value, .y = value }; }

static force_inline V2 v2_add(V2 a, V2 b) { return (V2){ .x = a.x + b.x, .y = a.y + b.y }; }

static force_inline V2 v2_div(V2 a, V2 b) { return (V2){ .x = a.x / b.x, .y = a.y / b.y }; }

static inline f32 v2_dot(V2 a, V2 b) { return a.x * b.x + a.y * b.y; }

static force_inline bool v2_equals(V2 a, V2 b) { return a.x == b.x && a.y == b.y; }

static inline f32 v2_len(V2 v) { return sqrtf(v2_len_squared(v)); }

static inline f32 v2_len_squared(V2 v) { return v2_dot(v, v); }

static inline V2 v2_lerp(V2 a, V2 b, f32 amount) {
    V2 add = v2_scale(v2_sub(b, a), amount);
    return v2_add(a, add);
}

static force_inline V2 v2_max(V2 a, V2 b) { return (V2){ .x = MAX(a.x, b.x), .y = MAX(a.y, b.y) }; }

static force_inline V2 v2_min(V2 a, V2 b) { return (V2){ .x = MIN(a.x, b.x), .y = MIN(a.y, b.y) }; }

static force_inline V2 v2_mul(V2 a, V2 b) { return (V2){ .x = a.x * b.x, .y = a.y * b.y }; }

static inline V2 v2_norm(V2 v) {
    f32 length = v2_len(v);
    if (length == 0) return (V2){0};
    return (V2){ .x = v.x / length, .y = v.y / length };
}

static force_inline V2 v2_reciprocal(V2 v) { return (V2){ .x = 1.f / v.x, .y = 1.f / v.y }; }

static inline V2 v2_rotate(V2 v, f32 angle_radians) {
    f32 sin_angle = sinf(angle_radians);
    f32 cos_angle = cosf(angle_radians);
    V2 result = { .x = v.x * cos_angle - v.y * sin_angle, .y = v.x * sin_angle + v.y * cos_angle };
    return result;
}

static inline V2 v2_round(V2 v) { return (V2){ .x = roundf(v.x), .y = roundf(v.y) }; }

static inline V2 v2_round_down(V2 v) { return v2_round(v2_sub(v, (V2){ .x = 0.5f, .y = 0.5f })); }

static force_inline V2 v2_scale(V2 v, f32 s) { return (V2){ .x = v.x * s, .y = v.y * s }; }

static inline V2 v2_stable_lerp(V2 a, V2 b, f32 k, f32 delta_time_seconds) {
    for (u64 i = 0; i < 2; i += 1) {
        a.v[i] = stable_lerp(a.v[i], b.v[i], k, delta_time_seconds);
    }
    return a;
}

static force_inline V2 v2_sub(V2 a, V2 b) { return (V2){ .x = a.x - b.x, .y = a.y - b.y }; }

static force_inline V3 v3_add(V3 a, V3 b) { return (V3){ .x = a.x + b.x, .y = a.y + b.y, .z = a.z + b.z }; }

static inline V3 v3_cross(V3 a, V3 b) {
    return (V3) {
        .x = a.y * b.z - a.z * b.y,
        .y = a.z * b.x - a.x * b.z,
        .z = a.x * b.y - a.y * b.x,
    };
}

static inline f32 v3_dot(V3 a, V3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }

static force_inline bool v3_equal(V3 a, V3 b) { return (a.x == b.x) && (a.y == b.y) && (a.z == b.z); }

static force_inline V3 v3_forward_from_view(M4 view) {
    return (V3){ .x = -view.c[0][2], .y = -view.c[1][2], .z = -view.c[2][2] };
}

static inline f32 v3_len(V3 v) { return sqrtf(v3_len_squared(v)); }

static inline f32 v3_len_squared(V3 v) { return v3_dot(v, v); }

static inline V3 v3_lerp(V3 a, V3 b, f32 amount) {
    V3 add = v3_scale(v3_sub(b, a), amount);
    return v3_add(a, add);
}

static force_inline V3 v3_neg(V3 v) { return (V3){ .x = -v.x, .y = -v.y, .z = -v.z }; }

static inline V3 v3_norm(V3 v) {
    f32 length = v3_len(v);
    if (length == 0) return (V3){0};
    return (V3){
        .x = v.x / length,
        .y = v.y / length,
        .z = v.z / length,
    };
}

static force_inline V3 v3_right_from_view(M4 view) {
    return (V3){ .x = view.c[0][0], .y = view.c[1][0], .z = view.c[2][0] };
}

static force_inline V3 v3_scale(V3 v, f32 s) { return (V3){ .x = v.x * s, .y = v.y * s, .z = v.z * s }; }

static force_inline V3 v3_sub(V3 a, V3 b) { return (V3){ .x = a.x - b.x, .y = a.y - b.y, .z = a.z - b.z }; }

static inline V3 v3_unproject(V3 pos, M4 view_projection) {
    M4 view_projection_inv = m4_inverse(view_projection);
    Quat q = v4v(pos, 1.f);
    Quat q_trans = m4_mul_v4(view_projection_inv, q);
    return v3_scale(q_trans.xyz, 1.f / q_trans.w);
}

static force_inline V3 v3_up_from_view(M4 view) {
    return (V3){ .x = view.c[0][1], .y = view.c[1][1], .z = view.c[2][1] };
}

static force_inline V4 v4_add(V4 a, V4 b) { return (V4){ .x = a.x + b.x, .y = a.y + b.y, .z = a.z + b.z, .w = a.w + b.w }; }

static inline f32 v4_dot(V4 a, V4 b) { return v3_dot(a.xyz, b.xyz) + a.w * b.w; }

static force_inline bool v4_equal(V4 a, V4 b) {
    return a.x == b.x && a.y == b.y && a.z == b.z && a.w == b.w;
}

static inline V4 v4_lerp(V4 a, V4 b, f32 amount) {
    V4 add = v4_scale(v4_sub(b, a), amount);
    return v4_add(a, add);
}

static inline V4 v4_round(V4 v) {
    v.x = roundf(v.x);
    v.y = roundf(v.y);
    v.z = roundf(v.z);
    v.w = roundf(v.w);
    return v;
}

static force_inline V4 v4_scale(V4 v, f32 s) { return (V4){ .x = v.x * s, .y = v.y * s, .z = v.z * s, .w = v.w * s }; }

static inline V4 v4_stable_lerp(V4 a, V4 b, f32 k, f32 delta_time_seconds) {
    for (u64 i = 0; i < 4; i += 1) {
        a.v[i] = stable_lerp(a.v[i], b.v[i], k, delta_time_seconds);
    }
    return a;
}

static force_inline V4 v4_sub(V4 a, V4 b) { return (V4){ .x = a.x - b.x, .y = a.y - b.y, .z = a.z - b.z, .w = a.w - b.w }; }

static V4 v4v(V3 xyz, f32 w) { return (V4){ .x = xyz.x, .y = xyz.y, .z = xyz.z, .w = w }; }

static inline Quat quat_from_rotation(V3 axis, f32 angle) {
    f32 half_angle = angle / 2.f;
    f32 sin_half_angle = sinf(half_angle);
    return (Quat){
        .x = axis.x * sin_half_angle,
        .y = axis.y * sin_half_angle,
        .z = axis.z * sin_half_angle,
        .w = cosf(half_angle),
    };
}

static inline Quat quat_mul_quat(Quat a, Quat b) {
    return (Quat){
        .x = a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
        .y = a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
        .z = a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w,
        .w = a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z,
    };
}

static inline V3 quat_rotate_v3(Quat q, V3 v) {
    Quat qv = { .xyz = v };
    Quat q_conjugate = (Quat){ .x = -q.x, .y = -q.y, .z = -q.z, .w = q.w };
    return quat_mul_quat(quat_mul_quat(q, qv), q_conjugate).xyz;
}

static force_inline M3 m3_fill_diagonal(f32 value) {
    M3 result = { .c = { [0][0] = value, [1][1] = value, [2][2] = value } };
    return result;
}

static inline M3 m3_from_rotation(f32 radians, V2 pivot) {
    return m3_model((V2){ .x = 1.f, .y = 1.f }, radians, pivot, (V2){0});
}

static inline M3 m3_inverse(M3 m) {
    M3 cross = { .columns = {
        [0] = v3_cross(m.columns[1], m.columns[2]),
        [1] = v3_cross(m.columns[2], m.columns[0]),
        [2] = v3_cross(m.columns[0], m.columns[1]),
    } };

    float inverse_determinant = 1.f / v3_dot(cross.columns[2], m.columns[2]);

    M3 result = { .columns = {
        [0] = v3_scale(cross.columns[0], inverse_determinant),
        [1] = v3_scale(cross.columns[1], inverse_determinant),
        [2] = v3_scale(cross.columns[2], inverse_determinant),
    } };

    result = m3_transpose(result);
    return result;
}

static inline M3 m3_model(V2 scale, f32 radians, V2 pivot, V2 post_translation) {
    f32 cos_value = cosf(radians);
    f32 sin_value = sinf(radians);

    f32 a = cos_value * scale.x;
    f32 b = sin_value * scale.y;
    f32 c = -sin_value * scale.x;
    f32 d = cos_value * scale.y;

    V2 a_pivot = { .x = a * pivot.x + c * pivot.y, .y = b * pivot.x + d * pivot.y };
    V2 translation = v2_sub(pivot, a_pivot);
    translation = v2_add(translation, post_translation);

    M3 result = { .c = {
        [0][0] = a,
        [0][1] = b,

        [1][0] = c,
        [1][1] = d,

        [2][0] = translation.x,
        [2][1] = translation.y,
        [2][2] = 1.f,
    } };

    return result;
}

static inline V3 m3_mul_v3(M3 m, V3 v) {
    return (V3){
        .x = v3_dot(v, (V3){ .x = m.c[0][0], .y = m.c[0][1], .z = m.c[0][2] }),
        .y = v3_dot(v, (V3){ .x = m.c[1][0], .y = m.c[1][1], .z = m.c[1][2] }),
        .z = v3_dot(v, (V3){ .x = m.c[2][0], .y = m.c[2][1], .z = m.c[2][2] }),
    };
}

static inline M3 m3_transpose(M3 m) {
    M3 result = m;
    result.c[0][1] = m.c[1][0];
    result.c[0][2] = m.c[2][0];
    result.c[1][0] = m.c[0][1];
    result.c[1][2] = m.c[2][1];
    result.c[2][1] = m.c[1][2];
    result.c[2][0] = m.c[0][2];
    return result;
}

static force_inline M4 m4_fill_diagonal(f32 value) {
    return (M4){ .c = { [0][0] = value, [1][1] = value, [2][2] = value, [3][3] = value } };
}

static inline M4 m4_from_rotation(V3 axis, f32 angle) {
    axis = v3_norm(axis);

    f32 sin_theta = sinf(angle);
    f32 cos_theta = cosf(angle);
    f32 cos_value = 1.f - cos_theta;

    return (M4){ .c = {
        [0][0] = (axis.x * axis.x * cos_value) + cos_theta,
        [0][1] = (axis.x * axis.y * cos_value) + (axis.z * sin_theta),
        [0][2] = (axis.x * axis.z * cos_value) - (axis.y * sin_theta),

        [1][0] = (axis.y * axis.x * cos_value) - (axis.z * sin_theta),
        [1][1] = (axis.y * axis.y * cos_value) + cos_theta,
        [1][2] = (axis.y * axis.z * cos_value) + (axis.x * sin_theta),

        [2][0] = (axis.z * axis.x * cos_value) + (axis.y * sin_theta),
        [2][1] = (axis.z * axis.y * cos_value) - (axis.x * sin_theta),
        [2][2] = (axis.z * axis.z * cos_value) + cos_theta,

        [3][3] = 1.f,
    } };
}

static inline M4 m4_from_top_left_m3(M3 m) {
    M4 result = { .columns = {
        [0].xyz = m.columns[0],
        [1].xyz = m.columns[1],
        [2].xyz = m.columns[2],
    } };
    return result;
}

static inline M4 m4_from_translation(V3 translation) {
    M4 result = m4_fill_diagonal(1.f);
    result.c[3][0] = translation.x;
    result.c[3][1] = translation.y;
    result.c[3][2] = translation.z;
    return result;
}

static inline M4 m4_inverse(M4 m) {
    V4 *cols = m.columns;

    V3 cross_0_1 = v3_cross(cols[0].xyz, cols[1].xyz);
    V3 cross_2_3 = v3_cross(cols[2].xyz, cols[3].xyz);
    V3 sub_1_0 = v3_sub(v3_scale(cols[0].xyz, cols[1].w), v3_scale(cols[1].xyz, cols[0].w));
    V3 sub_3_2 = v3_sub(v3_scale(cols[2].xyz, cols[3].w), v3_scale(cols[3].xyz, cols[2].w));

    float inv_det = 1.0f / (v3_dot(cross_0_1, sub_3_2) + v3_dot(cross_2_3, sub_1_0));
    cross_0_1 = v3_scale(cross_0_1, inv_det);
    cross_2_3 = v3_scale(cross_2_3, inv_det);
    sub_1_0 = v3_scale(sub_1_0, inv_det);
    sub_3_2 = v3_scale(sub_3_2, inv_det);

    return m4_transpose((M4){ .columns = {
        [0] = v4v(v3_add(v3_cross(cols[1].xyz, sub_3_2), v3_scale(cross_2_3, cols[1].w)), -v3_dot(cols[1].xyz, cross_2_3)),
        [1] = v4v(v3_sub(v3_cross(sub_3_2, cols[0].xyz), v3_scale(cross_2_3, cols[0].w)),  v3_dot(cols[0].xyz, cross_2_3)),
        [2] = v4v(v3_add(v3_cross(cols[3].xyz, sub_1_0), v3_scale(cross_0_1, cols[3].w)), -v3_dot(cols[3].xyz, cross_0_1)),
        [3] = v4v(v3_sub(v3_cross(sub_1_0, cols[2].xyz), v3_scale(cross_0_1, cols[2].w)),  v3_dot(cols[2].xyz, cross_0_1)),
    } });
}

static inline M4 m4_look_at(V3 eye, V3 centre, V3 up_direction) {
    V3 forward = v3_norm(v3_sub(centre, eye));
    V3 right   = v3_norm(v3_cross(forward, up_direction));
    V3 up      = v3_cross(right, forward);

    return (M4){ .c = {
        [0][0] =    right.x,
        [0][1] =       up.x,
        [0][2] = -forward.x,

        [1][0] =    right.y,
        [1][1] =       up.y,
        [1][2] = -forward.y,

        [2][0] =    right.z,
        [2][1] =       up.z,
        [2][2] = -forward.z,

        [3][0] = -v3_dot(right,   eye),
        [3][1] = -v3_dot(up,      eye),
        [3][2] =  v3_dot(forward, eye),
        [3][3] =  1.f,
    } };
}

static inline M4 m4_mul_m4(M4 a, M4 b) {
    M4 result = {0};
    for (int col = 0; col < 4; col += 1) for (int row = 0; row < 4; row += 1) {
        f32 sum = 0;
        for (int pos = 0; pos < 4; pos += 1) sum += a.c[pos][row] * b.c[col][pos];
        result.c[col][row] = sum;
    }
    return result;
}

static inline V4 m4_mul_v4(M4 m, V4 v) {
    return (V4){
        .x = v4_dot(v, (V4){ .x = m.c[0][0], .y = m.c[0][1], .z = m.c[0][2], .w = m.c[0][3] }),
        .y = v4_dot(v, (V4){ .x = m.c[1][0], .y = m.c[1][1], .z = m.c[1][2], .w = m.c[1][3] }),
        .z = v4_dot(v, (V4){ .x = m.c[2][0], .y = m.c[2][1], .z = m.c[2][2], .w = m.c[2][3] }),
        .w = v4_dot(v, (V4){ .x = m.c[3][0], .y = m.c[3][1], .z = m.c[3][2], .w = m.c[3][3] }),
    };
}

static inline M4 m4_perspective_projection(f32 fov_vertical_radians, f32 width_over_height, f32 range_near, f32 range_far) {
    f32 cot = 1.f / tanf(fov_vertical_radians / 2.f);
    return (M4){ .c = {
        [0][0] = cot / width_over_height,
        [1][1] = cot,
        [2][2] = (range_near + range_far) / (range_near - range_far),
        [2][3] = -1.f,
        [3][2] = (2.f * range_near * range_far) / (range_near - range_far),
    } };
}

static force_inline M4 m4_transpose(M4 m) {
    M4 result = {0};
    for (int i = 0; i < 4; i += 1) for (int j = 0; j < 4; j += 1) {
        result.c[i][j] = m.c[j][i];
    }
    return result;
}

static u64 int_from_string_base(String s, u64 base) {
    u64 result = 0, magnitude = s.count;
    for (u64 i = 0; i < s.count; i += 1, magnitude -= 1) {
        result *= base;
        u64 digit = decimal_from_hex_digit_table[s.data[i]];
        if (digit >= base) {
            log_error("digit '%c' is invalid in base %llu", s.data[i], base);
            return 0;
        }
        result += digit;
    }
    return result;
}

static bool string_equals(String s1, String s2) {
    if (s1.count != s2.count) return false;
    return memcmp_(s1.data, s2.data, s1.count) == 0;
}

static String string_from_cstring(const char *s) {
    if (s == NULL) return (String){ 0 };
    u64 count = 0;
    while (s[count] != '\0') count += 1;
    return (String){ .data = (u8 *)s, .count = count };
}

static char *cstring_from_string(Arena *arena, String string) {
    char *cstring = arena_make(arena, string.count + 1, char);
    for (u64 i = 0; i < string.count; i += 1) cstring[i] = (char)string.data[i];
    cstring[string.count] = '\0';
    return cstring;
}

// Only bases <= 10
static String string_from_int_base(Arena *arena, u64 _num, u8 base) {
    String str = {0};
    u64 num = _num;

    do {
        num /= base;
        str.count += 1;
    } while (num > 0);

    str.data = arena_make(arena, str.count, u8);

    num = _num;
    for (i64 i = (i64)str.count - 1; i >= 0; i -= 1) {
        str.data[i] = (u8)((num % base) + '0');
        num /= base;
    }

    return str;
}

static String string_range(String string, u64 start, u64 end) {
    assert(end <= string.count);
    assert(start <= end);
    String result = { .data = string.data + start, .count = end - start };
    return result;
}

static String string_print(Arena *arena, const char *fmt, ...) {
    va_list arguments;
    va_start(arguments, fmt);
    String result = string_print_(arena, fmt, arguments);
    va_end(arguments);
    return result;
}

static String string_print_(Arena *arena, const char *fmt, va_list arguments) {
    String_Builder builder = { .arena = arena };
    string_builder_print_(&builder, fmt, arguments);
    return builder.string;
}

static bool string_starts_with(String s, String start) {
    if (s.count < start.count) return false;
    for (u64 i = 0; i < start.count; i += 1) if (s.data[i] != start.data[i]) return false;
    return true;
}

static void string_builder_print(String_Builder *builder, const char *fmt_c, ...) {
    va_list arguments;
    va_start(arguments, fmt_c);
    string_builder_print_(builder, fmt_c, arguments);
    va_end(arguments);
}

static void string_builder_print_(String_Builder *builder, const char *fmt_c, va_list arguments) {
    String fmt_str = string_from_cstring(fmt_c);
    assert(fmt_str.count > 0);

    for (u64 i = 0; i < fmt_str.count; i += 1) {
        u64 beg_i = i;
        while (fmt_str.data[i] != '%' && i < fmt_str.count) i += 1;

        if (i == fmt_str.count) {
            string_builder_print(builder, "%S", string_range(fmt_str, beg_i, i));
            return;
        }

        assert(fmt_str.data[i] == '%');
        assert(i + 1 < fmt_str.count);

        bool non_format_chars_before_this_specifier = i > beg_i;
        if (non_format_chars_before_this_specifier) {
            String in_between_specifiers = slice_range(fmt_str, beg_i, i);
            string_builder_print(builder, "%S", in_between_specifiers);
        }

        i += 1;
        u8 byte_length = 4;
        u8 type = 0;

        parse_type: {
            type = fmt_str.data[i];
            switch (type) {
                case 'l': {
                    if (i + 1 < fmt_str.count && fmt_str.data[i + 1] == 'l') {
                        byte_length = sizeof(long long);
                        i += 1;
                    } else byte_length = sizeof(long);

                    i += 1;
                    goto parse_type;
                } break;
                case 'z': {
                    byte_length = sizeof(size_t);
                    i += 1;
                    goto parse_type;
                } break;
                case 'u': case 'd': case 'i': case 'x': case 'X': case 'o': case 'b':
                case 'c':
                case 's': case 'S':
                case 'f':
                case '%':
                    break; // everything is handled below - these just need to not error
                default: panic("invalid format syntax '%c'", type);
            }
        }

        u64 value = 0;
        bool value_is_signed = false;
        u8 base = 10;
        const char *character_from_digit = "0123456789abcdef";

        switch (type) {
            case 'X': character_from_digit = "0123456789ABCDEF"; // fallthrough
            case 'x': base = 16; goto unsigned_int;
            case 'o': base = 8; goto unsigned_int;
            case 'b': base = 2; goto unsigned_int;
            case 'd': case 'i': {
                i64 signed_value = 0;
                switch (byte_length) {
                    case 4: signed_value = va_arg(arguments, i32); break;
                    case 8: signed_value = va_arg(arguments, i64); break;
                    default: unreachable;
                }

                if (signed_value < 0) {
                    push(builder, '-');
                    signed_value = -signed_value;
                }

                value = cast(u64) signed_value;
                value_is_signed = true;
            } // fallthrough
            case 'u': { unsigned_int:
                if (!value_is_signed) switch (byte_length) {
                    case 4: value = va_arg(arguments, u32); break;
                    case 8: value = va_arg(arguments, u64); break;
                    default: unreachable;
                }

                if (value == 0) {
                    push(builder, '0');
                    break;
                }

                u8 buf_mem[sizeof(u64) * 8];
                u64 buf_index = sizeof(buf_mem);

                for (; value > 0; value /= base) {
                    buf_index -= 1;
                    u8 digit = (u8)(value % base);
                    buf_mem[buf_index] = (u8)character_from_digit[digit];
                }

                String decimal = { .data = buf_mem + buf_index, .count = sizeof(buf_mem) - buf_index };
                push_slice(builder, decimal);
            } break;
            case 'c': {
                static_assert(sizeof(char) == sizeof(u8), ""); // I don't know where this wouldn't be true
                u32 c = va_arg(arguments, u32);
                assert(c < 256);
                push(builder, cast(u8) c);
            } break;
            case 's': {
                char *cstring = va_arg(arguments, char *);
                String as_string = string_from_cstring(cstring);
                push_slice(builder, as_string);
            } break;
            case 'S': {
                String string = va_arg(arguments, String);
                push_slice(builder, string);
            } break;
            case 'f': {
                // References:
                // https://github.com/nothings/stb/blob/master/stb_sprintf.h
                // https://en.wikipedia.org/wiki/Double-precision_floating-point_format
                // https://dl.acm.org/doi/pdf/10.1145/3360595
                //
                // Most valuable:
                // https://github.com/charlesnicholson/nanoprintf
                // http://0x80.pl/notesen/2015-12-29-float-to-string.html
                // TODO(felix): implement this properly! ^

                f64 f64_value = va_arg(arguments, f64);

                u64 bits = bit_cast(u64) f64_value;
                i64 biased_exponent = (bits >> 52) & 0x7ff;
                u64 mantissa_mask = UINT64_MAX >> 12;
                u64 mantissa = bits & mantissa_mask;

                u8 is_negative = (u8)(bits >> 63);
                if (is_negative) push(builder, '-');

                if (biased_exponent == 0x7ff) {
                    String string = (mantissa == 0) ? string("Infinity") : string("NaN");
                    push_slice(builder, string);
                    break;
                }

                if (biased_exponent == 0 && mantissa == 0) {
                    push(builder, '0');
                    break;
                }

                // NOTE(felix): this can only represent integer parts up to UINT64_MAX!
                f64 absolute_value = is_negative ? -f64_value : f64_value;
                if (absolute_value > (f64)UINT64_MAX) {
                    push_slice(builder, (string("(...)")));
                    break;
                }

                string_builder_print(builder, "%llu", (u64)absolute_value);

                push(builder, '.');

                f64 fraction = absolute_value;

                // TODO(felix): add ability to configure in format argument
                u64 precision = 2;

                for (u64 j = 0; j < precision; j += 1) {
                    fraction -= (f64)(u64)fraction;
                    fraction *= 10.0;
                    u8 fraction_as_char = (u8)fraction + '0';
                    push(builder, fraction_as_char);
                }
            } break;
            case '%': push(builder, '%'); break;
            default: panic("unreachable");
        }
    }
}

static inline void string_builder_null_terminate(String_Builder *builder) {
    // Avoid push(0) because we don't want to increase the string length
    reserve(builder, builder->count + 1);
    builder->data[builder->count] = 0;
}

static void *os_heap_allocate(u64 byte_count) {
    void *pointer = 0;

    #if BASE_OS == BASE_OS_WINDOWS
        pointer = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, byte_count);
    #elif BASE_OS & BASE_OS_ANY_POSIX
        pointer = malloc(byte_count);
    #else
        #error "unsupported OS"
    #endif

    ensure(pointer != 0);
    return pointer;
}

static void os_heap_free(void *pointer) {
    #if BASE_OS == BASE_OS_WINDOWS
        HeapFree(GetProcessHeap(), 0, pointer);
    #elif BASE_OS & BASE_OS_ANY_POSIX
        free(pointer);
    #else
        #error "unsupported OS"
    #endif
}

static Os_File_Info os_file_info(const char *relative_path) {
    Os_File_Info info = {0};

    #if BASE_OS == BASE_OS_WINDOWS
    {
        HANDLE handle = CreateFileA(
            relative_path,
            GENERIC_READ,
            FILE_SHARE_READ,
            0,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL | FILE_FLAG_BACKUP_SEMANTICS,
            0
        );
        info.exists = handle != INVALID_HANDLE_VALUE;
        if (info.exists) {
            u32 attributes = GetFileAttributesA(relative_path);
            info.is_directory = !!(attributes & FILE_ATTRIBUTE_DIRECTORY);
        }

        if (info.exists) {
            GetFinalPathNameByHandleA(handle, info.full_path, sizeof info.full_path, 0);
            GetFileSizeEx(handle, (PLARGE_INTEGER)&info.size);
            CloseHandle(handle);
        }
    }
    #elif BASE_OS & BASE_OS_ANY_POSIX
    {
        struct stat status = {0};
        info.exists = lstat(relative_path, &status) == 0;
        if (info.exists) {
            info.is_directory = S_ISDIR(status.st_mode) ? true : false;
            info.size = (u64)status.st_size;
            realpath(relative_path, info.full_path);
        }
    }
    #else
        discard arena;
        discard relative_path;
        panic("unimplemented");
    #endif

    return info;
}

static bool os_make_directory(const char *relative_path, u32 mode) {
    bool ok = false;

    #if BASE_OS == BASE_OS_WINDOWS
    {
        discard mode;
        BOOL win32_ok = CreateDirectoryA(relative_path, 0);
        ok = !!win32_ok;

        // TODO(felix): actually get error from win32
        log_error("failed to create directory '%S'", relative_path);
    }
    #elif BASE_OS & BASE_OS_ANY_POSIX
    {
        int result = mkdir(relative_path, (mode_t)mode);
        ok = result == 0;
        if (result != 0) {
            log_error("mkdir() failed to create directory '%s' (errno=%d)", relative_path, errno);
        }
    }
    #else
    {
        discard mode;
        discard path_as_cstring;
        panic("unimplemented");
    }
    #endif

    return ok;
}

static String os_read_entire_file(Arena *arena, const char *relative_path, u64 max_bytes) {
    if (max_bytes == 0) max_bytes = UINT32_MAX;

    if (relative_path == 0 || *relative_path == 0) return (String){0};

    Array_u8 bytes = { .arena = arena };

    #if BASE_OS == BASE_OS_WINDOWS
        // NOTE(felix): this is because of ReadFile() taking a DWORD to specify the number of bytes to read, which is a u32
        assert(max_bytes <= UINT32_MAX);

        // NOTE(felix): not sure about this. See https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-createfilea
        DWORD share_mode = FILE_SHARE_READ;

        HANDLE file = CreateFileA(relative_path, GENERIC_READ, share_mode, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
        bool file_ok = file != INVALID_HANDLE_VALUE;
        if (!file_ok) log_error("unable to open file '%s'", relative_path);

        bool file_size_ok = false;
        u64 file_size = 0;
        if (file_ok) {
            BOOL ok = GetFileSizeEx(file, (PLARGE_INTEGER)&file_size);
            file_size_ok = ok != false;
            if (!file_size_ok) log_error("unable to get size of file '%s' after opening", relative_path);
        }

        if (file_size > max_bytes) {
            log_error("file '%s' is %llu bytes, which is greater than the supplied maximum of %llu bytes", relative_path, file_size, max_bytes);
            file_size_ok = false;
        }

        bool read_ok = false;
        if (file_size_ok) {
            reserve(&bytes, file_size);

            u32 num_bytes_read = 0;
            BOOL ok = ReadFile(file, bytes.data, (u32)file_size, (LPDWORD)&num_bytes_read, 0);
            read_ok = ok != false;
            if (!read_ok) log_error("unable to read bytes of file '%s' after opening", relative_path);

            assert(file_size == num_bytes_read);
            bytes.count = file_size;
        }


        if (file_ok) CloseHandle(file);
    #elif BASE_OS & BASE_OS_ANY_POSIX
        // TODO(felix): use open & read instead of the libc filesystem API
        FILE *file_handle = fopen(relative_path, "rb");
        bool file_ok = file_handle != 0;
        if (!file_ok) log_error("unable to open file '%s'", relative_path);

        bool seek_ok = false;
        if (file_ok) {
            seek_ok = fseek(file_handle, 0, SEEK_END) == 0;
            if (!seek_ok) log_error("unable to seek file '%s'", relative_path);
        }

        bool file_size_ok = false;
        u64 file_size = 0;
        if (seek_ok) {
            i64 file_size_signed = ftell(file_handle);
            file_size_ok = file_size_signed != -1;
            if (!file_size_ok) log_error("error reading offset after seeking file '%s'", relative_path);
            else {
                file_size = (u64)file_size_signed;
                if (file_size > max_bytes) {
                    log_error("file '%s' is %llu bytes, which is greater than the supplied maximum of %llu bytes", relative_path, file_size, max_bytes);
                    // goto end; // TODO(felix): is it correct not to have a goto here?
                }
            }
        }

        bool read_ok = false;
        if (file_size_ok) {
            rewind(file_handle);

            reserve(&bytes, file_size);

            u64 num_bytes_read = fread(bytes.data, 1, file_size, file_handle);
            bytes.count = num_bytes_read;
            read_ok = num_bytes_read == file_size;
            if (!read_ok) log_error("unable to read entire file '%s'; could only read %llu/%llu bytes", relative_path, num_bytes_read, file_size);
        }

        if (file_ok) fclose(file_handle);
    #else
        #error "unsupported OS"
    #endif

    return bit_cast(String) bytes;
}

static void os_remove_file(const char *relative_path) {
    #if BASE_OS == BASE_OS_WINDOWS
    {
        BOOL ok = DeleteFileA(relative_path);
        if (!ok) {
            // TODO(felix): actually get win32 error
            log_error("failure removing file '%s'", relative_path);
        }
    }
    #elif BASE_OS & BASE_OS_ANY_POSIX
    {
        int result = remove(relative_path);
        if (result != 0) {
            log_error("failure removing file '%s' (errno=%d)", relative_path, errno);
        }
    }
    #else
        discard path_as_cstring;
        panic("unimplemented");
    #endif
}

static bool os_write_entire_file(const char *relative_path, String bytes) {
    #if BASE_OS == BASE_OS_WINDOWS
        u64 dword_max = UINT32_MAX;
        assert(bytes.count <= dword_max);

        // NOTE(felix): not sure about this. See https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-createfilea
        DWORD share_mode = 0;
        HANDLE file_handle = CreateFileA(relative_path, GENERIC_WRITE, share_mode, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
        if (file_handle == INVALID_HANDLE_VALUE) {
            log_error("unable to open file '%s'", relative_path);
            return false;
        }

        bool ok = WriteFile(file_handle, bytes.data, (DWORD)bytes.count, 0, 0);
        if (!ok) {
            log_error("error writing to file '%s'", relative_path);
        }

        CloseHandle(file_handle);
        return ok;
    #elif BASE_OS & BASE_OS_ANY_POSIX
        // TODO(felix): use syscalls instead of libc filesystem API

        int open_flags = O_WRONLY | O_CREAT | O_TRUNC;
        mode_t open_permissions = 0644;
        const char *path_as_cstring = relative_path;
        int file_handle = open(path_as_cstring, open_flags, open_permissions);
        if (file_handle == -1) {
            log_error("unable to open file '%s'", relative_path);
            return false;
        }

        bool ok = false;
        for (u64 written_bytes = 0; written_bytes < bytes.count;) {
            i64 wrote_this_time = write(file_handle, bytes.data + written_bytes, bytes.count - written_bytes);
            if (wrote_this_time == -1) {
                log_error("error writing to file '%s'", relative_path);
                goto end;
            }
            written_bytes += (u64)wrote_this_time;
        }
        ok = true;

        end:
        close(file_handle);
        return ok;
    #else
        #error "unsupported OS"
    #endif
}

static void log_internal_with_location(const char *file, u64 line, const char *func, const char *format, ...) {
    va_list arguments;
    va_start(arguments, format);
    print_(format, arguments);
    va_end(arguments);
    print("\n");

    #if BUILD_DEBUG
        print("%s:%llu:%s(): logged here\n", file, line, func);
    #else
        discard(file); discard(line); discard(func);
    #endif
}

static u32 os_process_run(Arena arena, Slice_String arguments, String directory, Os_Process_Flags flags) {
    Scratch scratch = scratch_begin(&arena);
    u32 exit_code = 1;

    const char *current_directory = directory.count != 0 ? cstring_from_string(scratch.arena, directory) : 0;

    #if BASE_OS == BASE_OS_WINDOWS
    {
        const char *application_name = 0;

        String_Builder command_line = { .arena = scratch.arena };
        for (u64 i = 0; i < arguments.count; i += 1) {
            String argument = arguments.data[i];
            if (i > 0) string_builder_print(&command_line, "%c", ' ');
            string_builder_print(&command_line, "%S", argument);
        }
        string_builder_null_terminate(&command_line);

        if (flags & Os_Process_Flag_PRINT_COMMAND_BEFORE_RUNNING) {
            if (directory.count > 0) print("[.\\%S] ", directory);
            print("%S\n", command_line.string);
        }

        SECURITY_ATTRIBUTES *process_attributes = 0;
        SECURITY_ATTRIBUTES *thread_attributes = 0;
        BOOL inherit_handles = false;
        u32 creation_flags = 0;
        void *environment = 0;
        STARTUPINFOA startup_info = {0};
        PROCESS_INFORMATION process_info = {0};

        BOOL ok = CreateProcessA(
            application_name,
            (char *)command_line.data,
            process_attributes,
            thread_attributes,
            inherit_handles,
            creation_flags,
            environment,
            current_directory,
            &startup_info,
            &process_info
        );

        if (!ok) {
            log_error("unable to start process '%S'; CreateProcessA() returned %u", arguments.data[0], GetLastError());
        } else {
            WaitForSingleObject(process_info.hProcess, INFINITE);
            GetExitCodeProcess(process_info.hProcess, (LPDWORD)&exit_code);
            CloseHandle(process_info.hProcess);
            CloseHandle(process_info.hThread);
        }
    }
    #elif BASE_OS & BASE_OS_ANY_POSIX
    {
        Slice_String args = arguments;
        char **argv = arena_make(scratch.arena, args.count + 1, char *);
        for (u64 i = 0; i < args.count; i += 1) argv[i] = cstring_from_string(scratch.arena, args.data[i]);

        if (flags & Os_Process_Flag_PRINT_COMMAND_BEFORE_RUNNING) {
            if (directory.count != 0) print("[./%S] ", directory);
            for (u64 i = 0; i < args.count; i += 1) print("%s ", argv[i]);
            print("\n");
        }

        pid_t pid = fork();
        if (pid < 0) {
            log_error("unable to fork() to start process '%s' (errno=%d)", argv[0], errno);
        } else if (pid == 0) { // child
            bool directory_ok = current_directory == 0;
            if (current_directory != 0) {
                int result = chdir(current_directory);
                directory_ok = result == 0;
                if (result != 0) log_error("unable to chdir('%s') (errno=%d)", current_directory, errno);
            }

            if (directory_ok) {
                execvp(argv[0], argv);
                log_error("unable to execvp '%s' (errno=%d)", argv[0], errno);
            }

            os_exit(1);
        } else { // parent
            int status = 0;
            pid_t result = waitpid(pid, &status, 0);
            bool wait_ok = result != -1;
            if (result == -1) log_error("unable to wait on '%s' (errno=%d)", argv[0], errno);

            if (wait_ok) {
                if (WIFEXITED(status)) exit_code = WEXITSTATUS(status);
                else if (WIFSIGNALED(status)) exit_code = 128 + WTERMSIG(status);
            }
        }
    }
    #else
        panic("unimplemented");
    #endif

    scratch_end(scratch);
    if (flags & Os_Process_Flag_PRINT_EXIT_CODE) print("'%S' exited: %u\n", arguments.data[0], exit_code);
    return exit_code;
}

static void os_write(String string) {
    // TODO(felix): stderr support
    // NOTE(felix): can't use assert in this function because panic() will call os_write, so we'll end up with a recursively failing assert and stack overflow. Instead, use `if (!condition) { breakpoint; abort(); }`
    #if BASE_OS == BASE_OS_WINDOWS
        #if WINDOWS_SUBSYSTEM_WINDOWS
            discard(string);
        #else
            if (string.count > UINT32_MAX) { breakpoint; os_abort(); }

            HANDLE console_handle = GetStdHandle(STD_OUTPUT_HANDLE);
            if (console_handle == INVALID_HANDLE_VALUE) { breakpoint; os_abort(); }

            u32 num_chars_written = 0;
            BOOL ok = WriteFile(console_handle, string.data, (u32)string.count, (LPDWORD)&num_chars_written, 0);
            if (!ok) { breakpoint; os_abort(); }
            if (num_chars_written != string.count) { breakpoint; os_abort(); }
        #endif

    #elif BASE_OS & BASE_OS_ANY_POSIX
        int stdout_handle = 1;
        i64 bytes_written = write(stdout_handle, string.data, string.count);
        discard(bytes_written);

    #else
        #error "unimplemented"

    #endif
}

static void print(const char *format, ...) {
    va_list arguments;
    va_start(arguments, format);
    print_(format, arguments);
    va_end(arguments);
}

static void print_(const char *format, va_list arguments) {
    // TODO(felix): this should use the thread_local arena once we add that system
    static Arena arena = {0};
    if (arena.mem == 0) arena = arena_init(8096);

    Scratch temp = scratch_begin(&arena);

    String_Builder output = { .arena = &arena };
    string_builder_print_(&output, format, arguments);

    string_builder_null_terminate(&output);

    #if (BASE_OS == BASE_OS_WINDOWS) && BUILD_DEBUG
        OutputDebugStringA((char *)output.data);
    #endif

    os_write(output.string);

    scratch_end(temp);
}

#define BUILD_FLAGS_MAX 16

static String build_compiler_initial_command[Build_Compiler_COUNT][BUILD_FLAGS_MAX] = {
    [Build_Compiler_MSVC] = {
        string_constant("cl"),
        string_constant("-nologo"),
        string_constant("-FC"),
        string_constant("-diagnostics:column"),
        string_constant("-std:c11"),
        string_constant("-Oi"),
    },
    [Build_Compiler_CLANG] = {
        string_constant("clang"),
        string_constant("-fdiagnostics-absolute-paths"),
        string_constant("-pedantic"),
        string_constant("-Wno-microsoft"),
        string_constant("-fms-extensions"),
        string_constant("-ferror-limit=1"),
        string_constant("-Wno-gnu-zero-variadic-macro-arguments"),
        string_constant("-Wno-dollar-in-identifier-extension"),
        string_constant("-Wno-double-promotion"),
        string_constant("-std=c11"),
    },
    [Build_Compiler_EMCC] = {
        string_constant("emcc"),
        string_constant("-fdiagnostics-absolute-paths"),
        string_constant("-Wno-microsoft"),
        string_constant("-fms-extensions"),
        string_constant("-ferror-limit=1"),
        string_constant("-Wno-gnu-zero-variadic-macro-arguments"),
        string_constant("-Wno-dollar-in-identifier-extension"),
        string_constant("-std=c11"),
        string_constant("--shell-file"), string_constant("shell.html"),
    },
};

static String build_compiler_link_flags[Build_Compiler_COUNT][BASE_OS_COUNT][BUILD_FLAGS_MAX] = {
    [Build_Compiler_MSVC] = { [BASE_OS_WINDOWS] = {
        string_constant("-link"),
        // string_constant("-entry:entrypoint"),
        string_constant("-subsystem:console"),
        string_constant("-incremental:no"),
        string_constant("-opt:ref"),
        string_constant("-fixed"),
        string_constant("libucrtd.lib"),
    } },
    [Build_Compiler_CLANG] = {
        [BASE_OS_MACOS] = {
            string_constant("-framework"), string_constant("CoreFoundation"),
            string_constant("-framework"), string_constant("Metal"),
            string_constant("-framework"), string_constant("Cocoa"),
            string_constant("-framework"), string_constant("Metalkit"),
            string_constant("-framework"), string_constant("Quartz"),
            string_constant("-framework"), string_constant("AudioToolbox"),
        },
    },
    [Build_Compiler_EMCC] = { [BASE_OS_EMSCRIPTEN] = {
        string_constant("-sUSE_WEBGL2=1"),
        // string_constant("--use-port=emdawnwebgpu"),
        string_constant("-sABORTING_MALLOC=0"),
        string_constant("-sALLOW_MEMORY_GROWTH=1"),
        string_constant("-sMAXIMUM_MEMORY=4GB"),
        string_constant("-sASSERTIONS=2"),
        string_constant("-sSAFE_HEAP=1"),
    } },
};

static String build_compiler_mode_flags[Build_Mode_COUNT][Build_Compiler_COUNT][BUILD_FLAGS_MAX] = {
    [Build_Mode_DEBUG] = {
        [Build_Compiler_MSVC] = {
            string_constant("-MTd"), // TODO(felix): raylib needs -MDd, otherwise -MTd for sokol. Maybe should take care via metaprogramming annotation
            string_constant("-W4"),
            string_constant("-wd4127"), // "conditional expression is constant"
            string_constant("-wd4709"), // "comma operator within array index expression"
            string_constant("-WX"),
            string_constant("-Z7"),
        },
        [Build_Compiler_CLANG] = {
            string_constant("-Wall"),
            string_constant("-Werror"),
            string_constant("-Wextra"),
            string_constant("-Wshadow"),
            string_constant("-Wconversion"),
            string_constant("-Wno-unused-function"),
            string_constant("-Wno-deprecated-declarations"),
            string_constant("-Wno-missing-field-initializers"),
            string_constant("-Wno-unused-local-typedef"),
            string_constant("-Wno-initializer-overrides"),
            string_constant("-fno-strict-aliasing"),
            string_constant("-g3"),
            string_constant("-fsanitize=undefined"),
            string_constant("-fsanitize-trap"),
        },
        [Build_Compiler_EMCC] = {
            string_constant("-Wno-unused-function"),
            string_constant("-Wno-deprecated-declarations"),
            string_constant("-Wno-missing-field-initializers"),
            string_constant("-Wno-unused-local-typedef"),
            string_constant("-Wno-initializer-overrides"),
            string_constant("-Wno-shorten-64-to-32"),
            string_constant("-Wno-limited-postlink-optimizations"),
            string_constant("-fno-strict-aliasing"),
            string_constant("-g3"),
            string_constant("-fsanitize=undefined"),
            string_constant("-fsanitize-trap"),
        },
    },
    [Build_Mode_RELEASE] = {
        [Build_Compiler_MSVC] = {
            string_constant("-MT"),
            string_constant("-O2"),
        },
        [Build_Compiler_CLANG] = {
            string_constant("-Wno-assume"),
            string_constant("-O3"),
            #if BASE_OS == BASE_OS_WINDOWS
                string_constant("-MT"),
            #endif
        },
    },
};

static String build_compiler_out[Build_Compiler_COUNT] = {
    [Build_Compiler_MSVC] = string_constant("-out:"),
    [Build_Compiler_CLANG] = string_constant("-o"),
    [Build_Compiler_EMCC] = string_constant("-o"),
};

static Build_Compiler build_default_compiler[BASE_OS_COUNT] = {
    [BASE_OS_WINDOWS] = Build_Compiler_MSVC,
    [BASE_OS_MACOS] = Build_Compiler_CLANG,
    [BASE_OS_EMSCRIPTEN] = Build_Compiler_EMCC,
};

static String build_object_extension[BASE_OS_COUNT] = {
    [BASE_OS_WINDOWS] = string_constant("obj"),
    [BASE_OS_MACOS] = string_constant("o"),
    [BASE_OS_EMSCRIPTEN] = string_constant("o"),
};

static String build_binary_extension[BASE_OS_COUNT] = {
    [BASE_OS_WINDOWS] = string_constant("exe"),
    [BASE_OS_MACOS] = string_constant("bin"),
    [BASE_OS_EMSCRIPTEN] = string_constant("html"),
};

static char *build_emscripten_html = STRINGIFY(
    <!doctype html>
    <html>
    <head>
        <meta charset="utf-8">
        <meta name="viewport" content="width=device-width,initial-scale=1">
        <title>%S</title>
        <style>
            html,body{height:100%%;margin:0;background:#000}
            canvas{width:100vw;height:100vh;display:block}
        </style>
    </head>
    <body>
        <canvas id="canvas"></canvas>
        {{{ SCRIPT }}}
    </body>
    </html>
);

static u32 build_default_everything(Arena arena, String program_name, u8 target_os) {
    Scratch scratch = scratch_begin(&arena);
    Slice_String arguments = os_get_arguments(&arena);
    const char *c_file_path = "../src/main.c";

    Build_Compiler compiler = build_default_compiler[target_os];
    String object_extension = build_object_extension[target_os];
    String binary_extension = build_binary_extension[target_os];

    u8 path_separator = (BASE_OS & BASE_OS_ANY_POSIX) ? '/' : '\\';
    String dependency_directory = string_print(&arena, "..%cdeps", path_separator);

    String platform = {0};
    String code = os_read_entire_file(scratch.arena, &c_file_path[3], 0);
    String needle = string("meta(platform = ");
    for (u64 i = 0; i < code.count; i += 1) {
        String range = slice_range(code, i, code.count);
        if (!string_starts_with(range, needle)) continue;

        bool commented = false;
        for (u64 j = i - 1; j > 1 && code.data[j] != '\n'; j -= 1) {
            commented = code.data[j] == '/' && code.data[j - 1] == '/';
            if (commented) break;
        }
        if (commented) continue;

        u64 start = i + needle.count;
        u64 end = start + 1;
        while (code.data[end] != ')') end += 1;
        platform = (String)slice_range(code, start, end);
        break;
    }

    bool is_raylib = string_equals(platform, string("base_platform_raylib"));
    bool is_sdl_gles = string_equals(platform, string("base_platform_sdl3_gles3"));
    bool is_wingdi = string_equals(platform, string("base_platform_wingdi"));

    bool compile_platform = platform.count != 0;
    String platform_c_file = string_print(scratch.arena, "../src/base/%S.c", platform);
    Array_String platform_objects = { .arena = scratch.arena };

    if (is_raylib) {
        push(&platform_objects, string_print(scratch.arena, "%S/raylib-5.5_win64_msvc16/lib/raylib.lib", dependency_directory));
    } else if (is_sdl_gles) {
        push(&platform_objects, string_print(scratch.arena, "%S/SDL3-3.2.28/lib/x64/SDL3.lib", dependency_directory));
    } else if (!is_wingdi) {
        push(&platform_objects, string_print(scratch.arena, "%S.%S", platform, object_extension));
    }

    // TODO(felix): actual embed system, and remove this
    bool temporary_hack_embed_font = target_os == BASE_OS_EMSCRIPTEN;
    if (temporary_hack_embed_font) {
        const char *inter_c_path = "src/inter.c";
        if (!os_file_info(inter_c_path).exists) {
            const char *inter_path = "deps/Inter-4.1/InterVariable.ttf";
            String bytes = os_read_entire_file(&arena, inter_path, 0);
            String_Builder builder = { .arena = &arena };
            string_builder_print(&builder, "static const char inter_ttf_bytes[] = {\n    ");
            u8 column = 0;
            for_slice (u8 *, byte, bytes) {
                string_builder_print(&builder, "0x");
                string_builder_print(&builder, "%x", *byte);
                string_builder_print(&builder, ", ");
                column += 1;
                if (column > 16) {
                    column = 0;
                    string_builder_print(&builder, "\n    ");
                }
            }
            string_builder_print(&builder, "\n};");

            os_write_entire_file(inter_c_path, builder.string);
        }
    }

    String shdc_per_os[BASE_OS_COUNT] = {
        [BASE_OS_WINDOWS] = string_print(&arena, "%S\\shdc\\sokol-shdc.exe\0", dependency_directory),
        [BASE_OS_MACOS] = string_print(&arena, "%S/shdc/sokol-shdc-macos.bin\0", dependency_directory),
    };

    const char *shdc = (const char *)shdc_per_os[BASE_OS].data;
    if (BASE_OS == BASE_OS_MACOS) {
        static Os_File_Info info;
        info = os_file_info(&shdc[1]);
        shdc = info.full_path;
    }

    Slice_String include_paths = slice_of(String,
        string_print(&arena, "-I%S", dependency_directory),
        // string_print(&arena, "-I%S/SDL3-3.2.28/include", dependency_directory),
        // string_print(&arena, "-I%S/wgpu-windows-x86_64-msvc-debug/include/webgpu", dependency_directory),
        string("-I../src")
    );

    Array_String common[Build_Compiler_COUNT] = {0};
    for (u64 c = 0; c < array_count(common); c += 1) {
        common[c].arena = &arena;

        if (BASE_OS == BASE_OS_WINDOWS) {
            push(&common[c], (string("cmd.exe")));
            push(&common[c], (string("/c")));
        }

        // TOOD(felix): this slice needs to end at the first 0-length member, because the C array is statically sized
        Slice_String common_initial = slice_from_c_array(build_compiler_initial_command[c]);

        push_slice(&common[c], common_initial);
        push_slice(&common[c], include_paths);
    }

    // bool link_crt = !is_wingdi;
    bool link_crt = true;

    Array_String link[Build_Compiler_COUNT] = {0};
    for (u64 c = 0; c < array_count(link); c += 1) {
        link[c].arena = &arena;

        if (link_crt) {
            String argument = string("-DLINK_CRT=1");
            push(&link[c], argument);
        }

        Slice_String link_initial = slice_from_c_array(build_compiler_link_flags[c][target_os]);
        push_slice(&link[c], link_initial);
        if (is_raylib) {
            push_slice(&link[c], platform_objects);
            push(&link[c], (string("/NODEFAULTLIB:msvcrt")));
        }
    }

    Array_String flags[Build_Mode_COUNT][Build_Compiler_COUNT] = {0};
    for (u64 mode = 0; mode < array_count(flags); mode += 1) {
        for (u64 c = 0; c < array_count(flags[0]); c += 1) {
            Array_String *f = &flags[mode][c];
            f->arena = &arena;

            Slice_String initial = slice_from_c_array(build_compiler_mode_flags[mode][c]);
            push_slice(f, initial);

            if (mode == Build_Mode_DEBUG) {
                Slice_String extra_debug = slice_of(String, string("-DBUILD_DEBUG=1"));
                push_slice(f, extra_debug);
                if (link_crt && compiler != Build_Compiler_EMCC) {
                    String argument = string("-fsanitize=address");
                    push(f, argument);
                }
            }
        }
    }

    Build_Mode mode = Build_Mode_DEBUG;
    for_slice (String *, argument, arguments) {
        if (string_equals(*argument, string("clang"))) compiler = Build_Compiler_CLANG;
        if (string_equals(*argument, string("release"))) mode = Build_Mode_RELEASE;
    }

    Array_String compile = { .arena = &arena };
    push_slice(&compile, common[compiler]);
    push_slice(&compile, flags[mode][compiler]);

    Array_String dependency_compile = { .arena = &arena };
    push_slice(&dependency_compile, compile);
    push(&compile, string_from_cstring(c_file_path));

    if (compile_platform) {
        if (target_os == BASE_OS_EMSCRIPTEN) push(&compile, platform_c_file);
        else if (!is_raylib) push_slice(&compile, platform_objects);
    }

    push_slice(&compile, link[compiler]);
    push(&compile, string_print(&arena, "%S%S.%S", build_compiler_out[compiler], program_name, binary_extension));

    String build_directory = string("build");
    bool build_directory_ok = os_file_info(cstring_from_string(&arena, build_directory)).exists;
    if (!build_directory_ok) {
        build_directory_ok = os_make_directory(cstring_from_string(&arena, build_directory), 0755);
    }

    bool dependencies_ok = false;
    if (target_os == BASE_OS_EMSCRIPTEN) {
        String shell = string_print(&arena, build_emscripten_html, program_name);
        const char *path = (const char *)string_print(&arena, "%S/shell.html\0", build_directory).data;
        dependencies_ok = os_write_entire_file(path, shell);
    } else if (is_raylib || is_sdl_gles || is_wingdi) {
        dependencies_ok = true;
    } else {
        dependencies_ok = !compile_platform || os_file_info((const char *)string_print(&arena, "build/%S.%S\0", platform, object_extension).data).exists;
        bool compile_dependencies = build_directory_ok && !dependencies_ok;
        if (compile_dependencies) {
            if (BASE_OS == BASE_OS_MACOS) {
                dependency_compile.count = 0;
                push_slice(&dependency_compile, common[Build_Compiler_CLANG]);
                push(&dependency_compile, (string("-x")));
                push(&dependency_compile, (string("objective-c")));
            }
            push(&dependency_compile, (string("-DBASE_PLATFORM_IMPLEMENTATION")));
            push(&dependency_compile, (string("-c")));
            push(&dependency_compile, platform_c_file);
            push(&dependency_compile, string_print(&arena, "%S%S.%S", build_compiler_out[compiler], platform, object_extension));
            u32 exit_code = os_process_run(arena, dependency_compile.slice, build_directory, Os_Process_Flag_PRINT_COMMAND_BEFORE_RUNNING | Os_Process_Flag_PRINT_EXIT_CODE);
            dependencies_ok = exit_code == 0;
        }
    }

    bool is_sokol = string_equals(platform, string("base_platform_sokol"));
    bool shaders_ok = !is_sokol;
    shaders_ok = shaders_ok || BASE_OS == BASE_OS_WINDOWS; // TODO(felix): remove
    if (is_sokol && dependencies_ok && BASE_OS != BASE_OS_WINDOWS /* TODO(felix): remove */) {
        const char *shader_file = "./src/shader.c";
        if (os_file_info(shader_file).exists) {
            os_remove_file(shader_file);
        }

        Array_String command = { .arena = &arena };

        String target_shader_language[BASE_OS_COUNT] = {
            [BASE_OS_WINDOWS] = string("hlsl5"),
            [BASE_OS_MACOS] = string("metal_macos"),
            // [BASE_OS_EMSCRIPTEN] = string("wgsl"),
            [BASE_OS_EMSCRIPTEN] = string("glsl300es"),
        };
        String shdc_error_format[BASE_OS_COUNT] = {
            [BASE_OS_WINDOWS] = string("msvc"),
            [BASE_OS_MACOS] = string("gcc"),
        };

        if (BASE_OS == BASE_OS_WINDOWS) {
            push(&command, (string("cmd.exe")));
            push(&command, (string("/c")));
        }

        Slice_String args = slice_of(String,
            string_from_cstring(shdc),
            string("-l"),
            target_shader_language[target_os],
            string("-i"),
            string("../src/shader.glsl"),
            string("-o"),
            string("../src/shader.c"),
            string("-e"),
            shdc_error_format[BASE_OS],
        );
        push_slice(&command, args);

        u32 exit_code = os_process_run(arena, command.slice, build_directory, Os_Process_Flag_PRINT_COMMAND_BEFORE_RUNNING | Os_Process_Flag_PRINT_EXIT_CODE);
        shaders_ok = exit_code == 0;
    }

    u32 exit_code = 1;
    if (shaders_ok) {
        exit_code = os_process_run(arena, compile.slice, build_directory, Os_Process_Flag_PRINT_COMMAND_BEFORE_RUNNING | Os_Process_Flag_PRINT_EXIT_CODE);
    }

    bool finally_ok = exit_code == 0;
    if (finally_ok && target_os == BASE_OS_EMSCRIPTEN) {
        print("Success! You can execute: emrun build/%S.html\n", program_name);
    }

    scratch_end(scratch);
    return exit_code;
}

static void draw_(Platform_Common *platform, Draw_Command command) {
    push(&platform->draw_commands, command);
}

#define for_ui_axis(variable) for (Ui_Axis variable = 0; variable < Ui_Axis_COUNT; variable += 1)

static void ui_begin(Ui *ui) {
    ui->root = 0;
    ui->current_parent = 0;
    ui->style_stack.count = 0;
    ui->boxes_to_render.count = 0;

    ui->scale = ui->platform->dpi_scale;

    Ui_Box_Style_Set default_style = {0};
    default_style.kinds[Ui_Box_Style_Kind_INACTIVE].color[Ui_Color_BACKGROUND] = rgba_from_hex(0xd8d8d8ff);
    default_style.kinds[Ui_Box_Style_Kind_INACTIVE].color[Ui_Color_FOREGROUND] = rgba_from_hex(0x000000ff);
    default_style.kinds[Ui_Box_Style_Kind_INACTIVE].color[Ui_Color_BORDER] = rgba_from_hex(0x6d6d6dff);
    default_style.kinds[Ui_Box_Style_Kind_HOVERED].color[Ui_Color_BACKGROUND] = rgba_from_hex(0x9b9b9bff);
    default_style.kinds[Ui_Box_Style_Kind_HOVERED].color[Ui_Color_FOREGROUND] = rgba_from_hex(0x000000ff);
    default_style.kinds[Ui_Box_Style_Kind_HOVERED].color[Ui_Color_BORDER] = rgba_from_hex(0x515151ff);
    default_style.kinds[Ui_Box_Style_Kind_CLICKED].color[Ui_Color_BACKGROUND] = rgba_from_hex(0x000000ff);
    default_style.kinds[Ui_Box_Style_Kind_CLICKED].color[Ui_Color_FOREGROUND] = rgba_from_hex(0x000000ff);
    default_style.kinds[Ui_Box_Style_Kind_CLICKED].color[Ui_Color_BORDER] = rgba_from_hex(0xffffffff);

	for (Ui_Box_Style_Kind kind = 0; kind < Ui_Box_Style_Kind_COUNT; kind += 1) {
		Ui_Box_Style *style = &default_style.kinds[kind];

        style->font_size = 14.f;
		style->inner_padding.x = 5.f;
		style->inner_padding.y = 3.f;
		style->child_gap.x = 3.f;
		style->child_gap.y = 3.f;
		style->border_width = 1.f;
		style->border_radius = 5.f;
		style->animation_speed = 20.f;
	}
    ui->style_stack = (Array_Ui_Box_Style_Set){ .arena = ui->platform->frame_arena };
    push(&ui->style_stack, default_style);
}

static Ui_Box *ui_button(Ui *ui, const char *format, ...) {
    Ui_Box_Flags flags = Ui_Box_Flag_DRAW_TEXT | Ui_Box_Flag_DRAW_BACKGROUND | Ui_Box_Flag_DRAW_BORDER | Ui_Box_Flag_HOVERABLE | Ui_Box_Flag_CLICKABLE | Ui_Box_Flag_ANIMATE;

    va_list arguments;
    va_start(arguments, format);
    Ui_Box *box = ui_pushv(ui, false, flags, format, arguments);
    va_end(arguments);

    for_ui_axis (axis) box->size_kind[axis] = Ui_Size_Kind_TEXT;
    return box;
}

#define UI_MAX_BOX_COUNT 512
static void ui_create(Ui *ui) {
    ui->boxes = arena_make(ui->platform->persistent_arena, UI_MAX_BOX_COUNT * 2, Ui_Box);

    ui->boxes_to_render.arena = ui->platform->persistent_arena;
    reserve(&ui->boxes_to_render, UI_MAX_BOX_COUNT);
}

static void ui_default_render_passthrough(Ui *ui) {
    for_slice (Ui_Box **, box_double_pointer, ui->boxes_to_render) {
        Ui_Box *box = *box_double_pointer;

        Ui_Box_Style style = box->display_style;
        V4 background_color = style.color[Ui_Color_BACKGROUND];
        V4 border_color = style.color[Ui_Color_BORDER];
        V4 foreground_color = style.color[Ui_Color_FOREGROUND];
        f32 border_width = style.border_width * ui->scale;
        f32 border_radius = style.border_radius * ui->scale;

        bool draw_rectangle = !!(box->flags & (Ui_Box_Flag_DRAW_BORDER | Ui_Box_Flag_DRAW_BACKGROUND));
        if (draw_rectangle) {
            Draw_Command command = {
                .kind = Draw_Kind_RECTANGLE,
                .rectangle = {
                    .border_width = border_width,
                    .border_radius = border_radius,
                },
            };
            command.position = box->display_rectangle.top_left;
            command.color[Draw_Color_SOLID] = background_color;
            command.rectangle.size = box->display_rectangle.size;
            command.rectangle.border_color = border_color;

            draw_(ui->platform, command);
        }

        bool draw_text = !!(box->flags & Ui_Box_Flag_DRAW_TEXT);
        if (draw_text) {
            assert(box->display_string.count > 0);

            V2 position = box->display_rectangle.top_left;
            for_ui_axis (axis) {
                f32 add = box->display_style.inner_padding.v[axis];
                add += !!(box->flags & Ui_Box_Flag_DRAW_BORDER) * box->display_style.border_width;
                add *= ui->scale;
                position.v[axis] += add;
            }

            Draw_Command command = {
                .kind = Draw_Kind_TEXT,
                .text = {
                    .string = box->display_string,
                    .font_size = box->display_style.font_size * ui->scale,
                },
            };
            command.position = position;
            command.color[Draw_Color_SOLID] = foreground_color;

            draw_(ui->platform, command);
        }
    }
}

static void ui_layout_standalone(Ui *ui, Ui_Box *box) {
    Ui_Box_Style_Kind old_style_kind = box->target_style_kind;
    if ((box->flags & Ui_Box_Flag_CLICKABLE) && box->clicked) box->target_style_kind = Ui_Box_Style_Kind_CLICKED;
    else if ((box->flags & Ui_Box_Flag_HOVERABLE) && box->hovered) box->target_style_kind = Ui_Box_Style_Kind_HOVERED;
    else box->target_style_kind = Ui_Box_Style_Kind_INACTIVE;

    Ui_Box_Style *target_style = &box->target_style_set.kinds[box->target_style_kind];
    Ui_Box_Style *style = &box->display_style;

    bool lerp_style = box->target_style_kind <= old_style_kind;
    lerp_style = lerp_style && !(box->flags & Ui_Box_Flag__FIRST_FRAME);
    lerp_style = lerp_style && !!(box->flags & Ui_Box_Flag_ANIMATE);
    if (lerp_style) {
        f32 dt = ui->platform->seconds_since_last_frame;
        style->animation_speed = target_style->animation_speed;

        style->font_size = stable_lerp(style->font_size, target_style->font_size, style->animation_speed, dt);

        style->inner_padding = v2_stable_lerp(style->inner_padding, target_style->inner_padding, style->animation_speed, dt);
        style->child_gap = v2_stable_lerp(style->child_gap, target_style->child_gap, style->animation_speed, dt);

        for (Ui_Color color = 0; color < Ui_Color_COUNT; color += 1) {
            V4 *current = &style->color[color];
            V4 *target = &target_style->color[color];
            *current = v4_stable_lerp(*current, *target, style->animation_speed, dt);
        }

        style->border_width = stable_lerp(style->border_width, target_style->border_width, style->animation_speed, dt);
        style->border_radius = stable_lerp(style->border_radius, target_style->border_radius, style->animation_speed, dt);
    } else *style = *target_style;

    for_ui_axis (axis) {
		box->target_rectangle.size.v[axis] += ui->scale * 2.f * style->border_width * !!(box->flags & Ui_Box_Flag_DRAW_BORDER);
		box->target_rectangle.size.v[axis] += ui->scale * 2.f * style->inner_padding.v[axis];

        switch (box->size_kind[axis]) {
            case Ui_Size_Kind_NIL: unreachable;
            default: break;
        }
    }

    for (Ui_Box *child = box->first_child; child != 0; child = child->next_sibling) {
        ui_layout_standalone(ui, child);
    }
}

static void ui_layout_dependent_descendant(Ui *ui, Ui_Box *box) {
    for (Ui_Box *child = box->first_child; child != 0; child = child->next_sibling) {
        ui_layout_dependent_descendant(ui, child);
    }

    for_ui_axis (axis) switch (box->size_kind[axis]) {
        case Ui_Size_Kind_TEXT: break;
        case Ui_Size_Kind_SUM_OF_CHILDREN: {
            for (Ui_Box *child = box->first_child; child != 0; child = child->next_sibling) {
                box->target_rectangle.size.v[axis] += child->target_rectangle.size.v[axis];
				box->target_rectangle.size.v[axis] += ui->scale * box->display_style.child_gap.v[axis];
            }

            bool has_at_least_one_child = box->first_child != 0;
            box->target_rectangle.size.v[axis] -= has_at_least_one_child * ui->scale * box->display_style.child_gap.v[axis];
        } break;
        case Ui_Size_Kind_LARGEST_CHILD: {
            f32 largest = 0;
            for (Ui_Box *child = box->first_child; child != 0; child = child->next_sibling) {
                f32 size = child->target_rectangle.size.v[axis];
                largest = MAX(largest, size);
            }
            box->target_rectangle.size.v[axis] += largest;
        } break;
        default: unreachable;
    }

    for_ui_axis (axis) assert(box->target_rectangle.size.v[axis] != 0);
}

static void ui_layout_relative_positions_and_rectangle(Ui *ui, Ui_Box *box) {
    if (box->parent != 0) {
        Ui_Axis layout_axis = cast(Ui_Axis) box->parent->flags & Ui_Box_Flag_CHILD_AXIS;

        if (box->previous_sibling != 0) {
            box->target_rectangle.top_left.v[layout_axis] = box->previous_sibling->target_rectangle.top_left.v[layout_axis];
            box->target_rectangle.top_left.v[layout_axis] += box->previous_sibling->target_rectangle.size.v[layout_axis];
            box->target_rectangle.top_left.v[layout_axis] += ui->scale * box->parent->display_style.child_gap.v[layout_axis];
        } else {
            box->target_rectangle.top_left.v[layout_axis] = box->parent->target_rectangle.top_left.v[layout_axis];
			box->target_rectangle.top_left.v[layout_axis] += ui->scale * box->parent->display_style.inner_padding.v[layout_axis];
			box->target_rectangle.top_left.v[layout_axis] += !!(box->parent->flags & Ui_Box_Flag_DRAW_BORDER) * box->parent->display_style.border_width * ui->scale;
        }

        Ui_Axis non_layout_axis = !layout_axis;
        box->target_rectangle.top_left.v[non_layout_axis] = box->parent->target_rectangle.top_left.v[non_layout_axis];
        box->target_rectangle.top_left.v[non_layout_axis] += ui->scale * box->parent->display_style.inner_padding.v[non_layout_axis];
        box->target_rectangle.top_left.v[non_layout_axis] += !!(box->parent->flags & Ui_Box_Flag_DRAW_BORDER) * box->parent->display_style.border_width * ui->scale;
    }

    bool animate = !!(box->flags & Ui_Box_Flag_ANIMATE);
    animate = animate && !(box->flags & Ui_Box_Flag__FIRST_FRAME);
    if (animate) {
        V4 display = bit_cast(V4) box->display_rectangle;
        display = v4_stable_lerp(display, bit_cast(V4) box->target_rectangle, box->display_style.animation_speed, ui->platform->seconds_since_last_frame);
        box->display_rectangle = bit_cast(Ui_Box_Rectangle) display;
    } else {
        box->display_rectangle = box->target_rectangle;
    }

    if (box->flags & Ui_Box_Flag_ANY_VISIBLE) {
        Ui_Box_Rectangle display = box->display_rectangle;
        Ui_Box_Rectangle target = box->target_rectangle;
        assert(display.top_left.x >= 0);
        assert(display.top_left.y >= 0);
        assert(target.top_left.x >= 0);
        assert(target.top_left.y >= 0);
        assert(display.size.x >= 0);
        assert(display.size.y >= 0);
        assert(target.size.x > 0);
        assert(target.size.y > 0);

        push_assume_capacity(&ui->boxes_to_render, box);
    }

    for (Ui_Box *child = box->first_child; child != 0; child = child->next_sibling) {
        ui_layout_relative_positions_and_rectangle(ui, child);
    }
}

static void ui_end(Ui *ui) {
    ui_layout_standalone(ui, ui->root);
    // ui_layout_dependent_ancestor(ui, ui->root); // TODO(felix)
    ui_layout_dependent_descendant(ui, ui->root);
    // ui_layout_solve_violations(ui, ui->root); // TODO(felix)
    ui_layout_relative_positions_and_rectangle(ui, ui->root);
}

static void ui_pop_parent(Ui *ui) {
    ui->current_parent = ui->current_parent->parent;
}

static void ui_pop_style(Ui *ui) {
    pop(&ui->style_stack);
}

static Ui_Box_Style_Set *ui_push_style(Ui *ui) {
    Ui_Box_Style_Set top = *slice_get_last(ui->style_stack);
    push(&ui->style_stack, top);
    return slice_get_last(ui->style_stack);
}

static Ui_Box *ui_push(Ui *ui, bool parent, Ui_Box_Flags flags, const char *format, ...) {
    va_list arguments;
    va_start(arguments, format);
    Ui_Box *box = ui_pushv(ui, parent, flags, format, arguments);
    va_end(arguments);
    return box;
}

static Ui_Box *ui_pushv(Ui *ui, bool parent, Ui_Box_Flags flags, const char *format_c, va_list arguments) {
    bool keyed = format_c != 0;

    String hash_string = {0};
    String display_string = {0};

    Ui_Box *box = 0;
    bool box_is_new = !keyed;
    if (!keyed) {
        box = arena_make(ui->platform->frame_arena, 1, Ui_Box);
    } else {
        static Arena temporary = {0};
        if (temporary.capacity == 0) temporary = arena_init(8096);
        Scratch scratch = scratch_begin(&temporary);

        String format = string_print_(scratch.arena, format_c, arguments);

        bool has_2_hash_marker = false;
        bool has_3_hash_marker = false;

        u64 marker_start_index = 0;
        for (u64 i = 0; i < format.count; i += 1) {
            if (format.data[i] != '#') continue;

            if (i + 1 == format.count) break;
            if (format.data[i + 1] != '#') continue;

            has_3_hash_marker = (i + 2 < format.count) && format.data[i + 2] == '#';
            has_2_hash_marker = !has_3_hash_marker;

            marker_start_index = i;
            break;
        }

        u64 display_and_first_hash_part_end_index = marker_start_index;
        u64 second_hash_part_start_index = marker_start_index;

        // before `##` displayed and hashed; after only hashed
        second_hash_part_start_index += 2 * has_2_hash_marker;

        // before `###` only displayed; after only hashed
        u64 first_hash_part_start_index = display_and_first_hash_part_end_index * has_3_hash_marker;
        second_hash_part_start_index += 3 * has_3_hash_marker;

        String first_hash_part = string_range(format, first_hash_part_start_index, display_and_first_hash_part_end_index);
        String second_hash_part = string_range(format, second_hash_part_start_index, format.count);

        bool has_marker = has_2_hash_marker || has_3_hash_marker;
        u64 display_part_end_index = has_marker * display_and_first_hash_part_end_index + !has_marker * format.count;
        String display_part = string_range(format, 0, display_part_end_index);

        hash_string = string_print(scratch.arena, "%S%S", first_hash_part, second_hash_part);
        display_string = arena_push(ui->platform->frame_arena, display_part);

        u64 hash = hash_djb2(hash_string);
        u64 exponent = count_trailing_zeroes(UI_MAX_BOX_COUNT * 2);
        for (u64 index = hash;;) {
            index = hash_lookup_msi(hash, exponent, index);
            index += !index; // never 0
            Ui_Box *this_box = &ui->boxes[index];

            if (this_box->hash_string.count == 0) {
                ui->used_box_count += 1;
                u64 hashmap_max_load = 70;
                bool hashmap_saturated = ui->used_box_count * hashmap_max_load / 100 > UI_MAX_BOX_COUNT * 2;
                assert(!hashmap_saturated);

                box_is_new = true;
                box = this_box;
                break;
            }

            if (string_equals(this_box->hash_string, hash_string)) {
                box = this_box;
                // NOTE(felix): debug check for same hash for different boxes in same frame can go here
                break;
            }
        }

        if (box_is_new) box->hash_string = arena_push(ui->platform->persistent_arena, hash_string);

        scratch_end(scratch);
    }

    zero(&box->frame_data);
    zero(&box->frame_data_computed);

    box->parent = ui->current_parent;
    if (ui->root == 0) {
        assert(ui->current_parent == 0);
        assert(parent);
        ui->root = box;
        ui->current_parent = box;
    }

    box->flags = flags;
    box->flags |= Ui_Box_Flag__FIRST_FRAME * box_is_new;
    {
        Ui_Axis child_layout_axis = !!(box->flags & Ui_Box_Flag_CHILD_AXIS);
        box->size_kind[child_layout_axis] = Ui_Size_Kind_SUM_OF_CHILDREN;
        box->size_kind[!child_layout_axis] = Ui_Size_Kind_LARGEST_CHILD;
    }

    box->target_style_set = *slice_get_last(ui->style_stack);
    if (box_is_new) box->display_style = box->target_style_set.kinds[Ui_Box_Style_Kind_INACTIVE];

    box->display_string = display_string;
    if (display_string.count > 0) {
        f32 font_size = box->display_style.font_size * ui->scale;
        box->target_rectangle.size = platform_measure_text(ui->platform, box->display_string, font_size);
        for_ui_axis (axis) assert(box->target_rectangle.size.v[axis] != 0);
    }

    if (box->parent != 0) {
        if (box->parent->first_child == 0) {
            assert(box->parent->last_child == 0);
            box->parent->first_child = box;
            box->parent->last_child = box;
        } else {
            assert(box->parent->last_child != 0);
            box->parent->last_child->next_sibling = box;
            box->previous_sibling = box->parent->last_child;
            box->parent->last_child = box;
        }
    }

    if (parent) ui->current_parent = box;

    box->target_style_set = *slice_get_last(ui->style_stack);

    V4 rectangle = bit_cast(V4) box->display_rectangle.top_left;
    rectangle.zw = v2_add(box->display_rectangle.top_left, box->display_rectangle.size);

    box->hovered = !!(box->flags & Ui_Box_Flag_HOVERABLE) && intersect_point_in_rectangle(ui->platform->mouse_position, rectangle);
    box->clicked = box->hovered && !!(box->flags & Ui_Box_Flag_CLICKABLE) && ui->platform->mouse_clicked[App_Mouse_Button_LEFT];

    return box;
}

static Ui_Box *ui_text(Ui *ui, const char *format, ...) {
    va_list arguments;
    va_start(arguments, format);
    Ui_Box *box = ui_pushv(ui, false, Ui_Box_Flag_DRAW_TEXT | Ui_Box_Flag_ANIMATE, format, arguments);
    va_end(arguments);

    for_ui_axis (axis) box->size_kind[axis] = Ui_Size_Kind_TEXT;
    return box;
}


#endif // defined(BASE_IMPLEMENTATION)
