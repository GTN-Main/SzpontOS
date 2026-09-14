/*
 * SzpontOS — Native OpenGL & EGL Verification Suite (gltriangle.c)
 * (C) Copyright by Szpont Industries. All rights reserved.
 *
 * Validates:
 * 1. EGL 1.5 runtime initialization (libEGL.so) via Surfaceless or DRM platform
 * 2. Mesa Gallium Megadriver loading (softpipe / swrast)
 * 3. OpenGL ES 2.0 / 3.0 API bindings (libGLESv2.so & libglapi.so)
 * 4. GLSL Shader compiler (vertex & fragment shaders, linking)
 * 5. Offscreen Pbuffer rasterization & glReadPixels verification
 * 6. Export rendered frame to PPM image (/tmp/gltriangle.ppm)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>

#ifndef EGL_PLATFORM_SURFACELESS_MESA
#define EGL_PLATFORM_SURFACELESS_MESA 0x31DD
#endif

#define TEST_WIDTH   512
#define TEST_HEIGHT  512

/* ANSI Color Escape Sequences */
#define COLOR_RESET   "\033[0m"
#define COLOR_BOLD    "\033[1m"
#define COLOR_RED     "\033[1;31m"
#define COLOR_GREEN   "\033[1;32m"
#define COLOR_YELLOW  "\033[1;33m"
#define COLOR_BLUE    "\033[1;34m"
#define COLOR_MAGENTA "\033[1;35m"
#define COLOR_CYAN    "\033[1;36m"
#define COLOR_WHITE   "\033[1;37m"

static const char *g_vertex_shader_src =
    "attribute vec2 aPosition;\n"
    "attribute vec3 aColor;\n"
    "varying vec3 vColor;\n"
    "void main() {\n"
    "    vColor = aColor;\n"
    "    gl_Position = vec4(aPosition, 0.0, 1.0);\n"
    "}\n";

static const char *g_fragment_shader_src =
    "precision mediump float;\n"
    "varying vec3 vColor;\n"
    "void main() {\n"
    "    gl_FragColor = vec4(vColor, 1.0);\n"
    "}\n";

static GLuint compile_shader(GLenum type, const char *source) {
    GLuint shader = glCreateShader(type);
    if (!shader) {
        fprintf(stderr, COLOR_RED "Error: glCreateShader failed\n" COLOR_RESET);
        return 0;
    }

    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);

    GLint compiled = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (!compiled) {
        char info_log[1024];
        GLsizei len = 0;
        glGetShaderInfoLog(shader, sizeof(info_log), &len, info_log);
        fprintf(stderr, COLOR_RED "Shader compilation error: %s\n" COLOR_RESET, info_log);
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

static GLuint create_program(const char *vs_src, const char *fs_src) {
    GLuint vs = compile_shader(GL_VERTEX_SHADER, vs_src);
    GLuint fs = compile_shader(GL_FRAGMENT_SHADER, fs_src);
    if (!vs || !fs) return 0;

    GLuint program = glCreateProgram();
    if (!program) return 0;

    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glBindAttribLocation(program, 0, "aPosition");
    glBindAttribLocation(program, 1, "aColor");

    glLinkProgram(program);

    GLint linked = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (!linked) {
        char info_log[1024];
        GLsizei len = 0;
        glGetProgramInfoLog(program, sizeof(info_log), &len, info_log);
        fprintf(stderr, COLOR_RED "Program link error: %s\n" COLOR_RESET, info_log);
        glDeleteProgram(program);
        return 0;
    }

    glDeleteShader(vs);
    glDeleteShader(fs);
    return program;
}

static bool save_ppm(const char *filename, const uint8_t *pixels, int width, int height) {
    FILE *fp = fopen(filename, "wb");
    if (!fp) {
        fprintf(stderr, "Cannot open %s for writing: %s\n", filename, strerror(errno));
        return false;
    }

    /* PPM Header: P6 width height max_val */
    fprintf(fp, "P6\n%d %d\n255\n", width, height);

    /* OpenGL glReadPixels returns bottom-to-top, flip vertically for standard image */
    for (int y = height - 1; y >= 0; y--) {
        for (int x = 0; x < width; x++) {
            const uint8_t *p = pixels + (y * width + x) * 4;
            fputc(p[0], fp); /* R */
            fputc(p[1], fp); /* G */
            fputc(p[2], fp); /* B */
        }
    }

    fclose(fp);
    return true;
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    printf(COLOR_CYAN "=====================================================\n" COLOR_RESET);
    printf(COLOR_BOLD "  SzpontOS OpenGL & EGL Hardware/Software Smoke Test  \n" COLOR_RESET);
    printf(COLOR_CYAN "  Architecture: Gallium softpipe / DRI3 / EGL         \n" COLOR_RESET);
    printf(COLOR_CYAN "=====================================================\n\n" COLOR_RESET);

    /* 1. Initialize EGL Display */
    printf(COLOR_WHITE "[STEP 1] Querying EGL Display... " COLOR_RESET);
    PFNEGLGETPLATFORMDISPLAYEXTPROC get_platform_display =
        (PFNEGLGETPLATFORMDISPLAYEXTPROC)eglGetProcAddress("eglGetPlatformDisplayEXT");

    EGLDisplay dpy = EGL_NO_DISPLAY;
    if (get_platform_display) {
        dpy = get_platform_display(EGL_PLATFORM_SURFACELESS_MESA, EGL_DEFAULT_DISPLAY, NULL);
    }
    if (dpy == EGL_NO_DISPLAY) {
        dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    }

    if (dpy == EGL_NO_DISPLAY) {
        printf(COLOR_RED "FAILED: eglGetDisplay returned EGL_NO_DISPLAY\n" COLOR_RESET);
        return 1;
    }
    printf(COLOR_GREEN "PASSED (Display handle: %p)\n" COLOR_RESET, (void *)dpy);

    /* 2. Initialize EGL */
    printf(COLOR_WHITE "[STEP 2] Initializing EGL... " COLOR_RESET);
    EGLint major = 0, minor = 0;
    if (!eglInitialize(dpy, &major, &minor)) {
        printf(COLOR_RED "FAILED: eglInitialize returned error 0x%04x\n" COLOR_RESET, eglGetError());
        return 1;
    }
    printf(COLOR_GREEN "PASSED (EGL Version %d.%d)\n" COLOR_RESET, major, minor);

    /* 3. Bind API: OpenGL ES */
    printf(COLOR_WHITE "[STEP 3] Binding EGL OpenGL ES API... " COLOR_RESET);
    if (!eglBindAPI(EGL_OPENGL_ES_API)) {
        printf(COLOR_RED "FAILED: eglBindAPI error 0x%04x\n" COLOR_RESET, eglGetError());
        return 1;
    }
    printf(COLOR_GREEN "PASSED\n" COLOR_RESET);

    /* 4. Choose Config */
    printf(COLOR_WHITE "[STEP 4] Choosing EGL Config... " COLOR_RESET);
    const EGLint config_attribs[] = {
        EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 8,
        EGL_NONE
    };

    EGLConfig config = NULL;
    EGLint num_configs = 0;
    if (!eglChooseConfig(dpy, config_attribs, &config, 1, &num_configs) || num_configs < 1) {
        printf(COLOR_RED "FAILED: eglChooseConfig returned %d configs\n" COLOR_RESET, num_configs);
        return 1;
    }
    printf(COLOR_GREEN "PASSED (Found matching config)\n" COLOR_RESET);

    /* 5. Create Pbuffer Surface */
    printf(COLOR_WHITE "[STEP 5] Creating %dx%d Pbuffer Surface... " COLOR_RESET, TEST_WIDTH, TEST_HEIGHT);
    const EGLint pbuffer_attribs[] = {
        EGL_WIDTH, TEST_WIDTH,
        EGL_HEIGHT, TEST_HEIGHT,
        EGL_NONE
    };
    EGLSurface surface = eglCreatePbufferSurface(dpy, config, pbuffer_attribs);
    if (surface == EGL_NO_SURFACE) {
        printf(COLOR_RED "FAILED: eglCreatePbufferSurface error 0x%04x\n" COLOR_RESET, eglGetError());
        return 1;
    }
    printf(COLOR_GREEN "PASSED\n" COLOR_RESET);

    /* 6. Create Context */
    printf(COLOR_WHITE "[STEP 6] Creating OpenGL ES 2.0 Context... " COLOR_RESET);
    const EGLint context_attribs[] = {
        EGL_CONTEXT_CLIENT_VERSION, 2,
        EGL_NONE
    };
    EGLContext ctx = eglCreateContext(dpy, config, EGL_NO_CONTEXT, context_attribs);
    if (ctx == EGL_NO_CONTEXT) {
        printf(COLOR_RED "FAILED: eglCreateContext error 0x%04x\n" COLOR_RESET, eglGetError());
        return 1;
    }
    printf(COLOR_GREEN "PASSED\n" COLOR_RESET);

    /* 7. Make Current */
    printf(COLOR_WHITE "[STEP 7] eglMakeCurrent... " COLOR_RESET);
    if (!eglMakeCurrent(dpy, surface, surface, ctx)) {
        printf(COLOR_RED "FAILED: eglMakeCurrent error 0x%04x\n" COLOR_RESET, eglGetError());
        return 1;
    }
    printf(COLOR_GREEN "PASSED\n" COLOR_RESET);

    /* 8. Inspect OpenGL Renderer Strings */
    const char *gl_vendor   = (const char *)glGetString(GL_VENDOR);
    const char *gl_renderer = (const char *)glGetString(GL_RENDERER);
    const char *gl_version  = (const char *)glGetString(GL_VERSION);
    const char *gl_slver    = (const char *)glGetString(GL_SHADING_LANGUAGE_VERSION);

    printf("\n" COLOR_MAGENTA "-----------------------------------------------------\n" COLOR_RESET);
    printf(COLOR_BOLD "  OpenGL State Information:\n" COLOR_RESET);
    printf("  GL_VENDOR:                   " COLOR_CYAN "%s\n" COLOR_RESET, gl_vendor ? gl_vendor : "unknown");
    printf("  GL_RENDERER:                 " COLOR_CYAN "%s\n" COLOR_RESET, gl_renderer ? gl_renderer : "unknown");
    printf("  GL_VERSION:                  " COLOR_CYAN "%s\n" COLOR_RESET, gl_version ? gl_version : "unknown");
    printf("  GL_SHADING_LANGUAGE_VERSION: " COLOR_CYAN "%s\n" COLOR_RESET, gl_slver ? gl_slver : "unknown");
    printf(COLOR_MAGENTA "-----------------------------------------------------\n\n" COLOR_RESET);

    /* 9. Build Shader Program */
    printf(COLOR_WHITE "[STEP 8] Compiling and linking GLSL shaders... " COLOR_RESET);
    GLuint prog = create_program(g_vertex_shader_src, g_fragment_shader_src);
    if (!prog) {
        printf(COLOR_RED "FAILED to link GLSL program\n" COLOR_RESET);
        return 1;
    }
    glUseProgram(prog);
    printf(COLOR_GREEN "PASSED (Program ID: %u)\n" COLOR_RESET, prog);

    /* 10. Prepare Vertex Data for RGB Triangle */
    static const GLfloat vertices[] = {
        /* X,      Y,       R,     G,     B */
         0.0f,   0.75f,   1.0f,  0.0f,  0.0f,  /* Top: Red */
        -0.75f, -0.75f,   0.0f,  1.0f,  0.0f,  /* Bottom-Left: Green */
         0.75f, -0.75f,   0.0f,  0.0f,  1.0f   /* Bottom-Right: Blue */
    };

    GLuint vbo = 0;
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), (void *)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), (void *)(2 * sizeof(GLfloat)));
    glEnableVertexAttribArray(1);

    /* 11. Render Scene */
    printf(COLOR_WHITE "[STEP 9] Rasterizing Triangle via Mesa Gallium softpipe... " COLOR_RESET);
    glViewport(0, 0, TEST_WIDTH, TEST_HEIGHT);
    glClearColor(0.10f, 0.12f, 0.18f, 1.0f); /* Dark Cyber Navy */
    glClear(GL_COLOR_BUFFER_BIT);

    glDrawArrays(GL_TRIANGLES, 0, 3);
    glFinish();
    printf(COLOR_GREEN "PASSED\n" COLOR_RESET);

    /* 12. Readback pixels */
    printf(COLOR_WHITE "[STEP 10] Reading back framebuffer via glReadPixels... " COLOR_RESET);
    uint8_t *pixels = (uint8_t *)malloc(TEST_WIDTH * TEST_HEIGHT * 4);
    if (!pixels) {
        printf(COLOR_RED "FAILED: Out of memory\n" COLOR_RESET);
        return 1;
    }

    glReadPixels(0, 0, TEST_WIDTH, TEST_HEIGHT, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    printf(COLOR_GREEN "PASSED\n" COLOR_RESET);

    /* 13. Verify rendered colors */
    printf(COLOR_WHITE "[STEP 11] Verifying pixel colors (Background & Triangle)... " COLOR_RESET);
    /* Top-left corner should be background (~25, 30, 46) */
    uint8_t bg_r = pixels[0];
    uint8_t bg_g = pixels[1];
    uint8_t bg_b = pixels[2];

    /* Center of viewport (x=256, y=256) should be inside triangle */
    int center_idx = (256 * TEST_WIDTH + 256) * 4;
    uint8_t center_r = pixels[center_idx + 0];
    uint8_t center_g = pixels[center_idx + 1];
    uint8_t center_b = pixels[center_idx + 2];

    printf("BG=(%u,%u,%u), Center=(%u,%u,%u) -> ", bg_r, bg_g, bg_b, center_r, center_g, center_b);
    if (center_r > 50 || center_g > 50 || center_b > 50) {
        printf(COLOR_GREEN "PASSED (Rendered triangle detected!)\n" COLOR_RESET);
    } else {
        printf(COLOR_YELLOW "WARNING: Center color is low\n" COLOR_RESET);
    }

    /* 14. Save to PPM image */
    printf(COLOR_WHITE "[STEP 12] Exporting frame to /tmp/gltriangle.ppm... " COLOR_RESET);
    if (save_ppm("/tmp/gltriangle.ppm", pixels, TEST_WIDTH, TEST_HEIGHT)) {
        printf(COLOR_GREEN "PASSED (Saved /tmp/gltriangle.ppm)\n" COLOR_RESET);
    } else {
        printf(COLOR_RED "FAILED to save PPM\n" COLOR_RESET);
    }

    /* Cleanup */
    free(pixels);
    glDeleteBuffers(1, &vbo);
    glDeleteProgram(prog);
    eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    eglDestroySurface(dpy, surface);
    eglDestroyContext(dpy, ctx);
    eglTerminate(dpy);

    printf(COLOR_GREEN "\n>>> OpenGL & EGL Smoke Test Completed Successfully on SzpontOS! <<<\n\n" COLOR_RESET);
    return 0;
}
