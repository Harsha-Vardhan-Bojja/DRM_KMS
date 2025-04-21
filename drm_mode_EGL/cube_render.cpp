#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "cube_render.h"
#include <ctime>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

using namespace glm;

static GLuint fbo, fbo_tex, depth_rb;
static GLuint vbo;
static GLuint tex;
static GLuint program;
static GLint mvp_loc;


// Shader sources
const char* vertex_shader_source = R"(
attribute vec3 position;
attribute vec2 texCoord;
uniform mat4 mvp;
varying vec2 v_texCoord;
void main() {
    gl_Position = mvp * vec4(position, 1.0);
    v_texCoord = texCoord;
}
)";

const char* fragment_shader_source = R"(
precision mediump float;
varying vec2 v_texCoord;
uniform sampler2D tex;
void main() {
    gl_FragColor = texture2D(tex, v_texCoord);
}
)";

// Cube vertex data
float cube_vertices[] = {
    // Positions          // Texture Coords
    -0.5f, -0.5f, -0.5f,  0.0f, 0.0f,
     0.5f, -0.5f, -0.5f,  1.0f, 0.0f,
     0.5f,  0.5f, -0.5f,  1.0f, 1.0f,
     0.5f,  0.5f, -0.5f,  1.0f, 1.0f,
    -0.5f,  0.5f, -0.5f,  0.0f, 1.0f,
    -0.5f, -0.5f, -0.5f,  0.0f, 0.0f,

    -0.5f, -0.5f,  0.5f,  0.0f, 0.0f,
     0.5f, -0.5f,  0.5f,  1.0f, 0.0f,
     0.5f,  0.5f,  0.5f,  1.0f, 1.0f,
     0.5f,  0.5f,  0.5f,  1.0f, 1.0f,
    -0.5f,  0.5f,  0.5f,  0.0f, 1.0f,
    -0.5f, -0.5f,  0.5f,  0.0f, 0.0f,

    -0.5f,  0.5f,  0.5f,  1.0f, 0.0f,
    -0.5f,  0.5f, -0.5f,  1.0f, 1.0f,
    -0.5f, -0.5f, -0.5f,  0.0f, 1.0f,
    -0.5f, -0.5f, -0.5f,  0.0f, 1.0f,
    -0.5f, -0.5f,  0.5f,  0.0f, 0.0f,
    -0.5f,  0.5f,  0.5f,  1.0f, 0.0f,

     0.5f,  0.5f,  0.5f,  1.0f, 0.0f,
     0.5f,  0.5f, -0.5f,  1.0f, 1.0f,
     0.5f, -0.5f, -0.5f,  0.0f, 1.0f,
     0.5f, -0.5f, -0.5f,  0.0f, 1.0f,
     0.5f, -0.5f,  0.5f,  0.0f, 0.0f,
     0.5f,  0.5f,  0.5f,  1.0f, 0.0f,

    -0.5f, -0.5f, -0.5f,  0.0f, 1.0f,
     0.5f, -0.5f, -0.5f,  1.0f, 1.0f,
     0.5f, -0.5f,  0.5f,  1.0f, 0.0f,
     0.5f, -0.5f,  0.5f,  1.0f, 0.0f,
    -0.5f, -0.5f,  0.5f,  0.0f, 0.0f,
    -0.5f, -0.5f, -0.5f,  0.0f, 1.0f,

    -0.5f,  0.5f, -0.5f,  0.0f, 1.0f,
     0.5f,  0.5f, -0.5f,  1.0f, 1.0f,
     0.5f,  0.5f,  0.5f,  1.0f, 0.0f,
     0.5f,  0.5f,  0.5f,  1.0f, 0.0f,
    -0.5f,  0.5f,  0.5f,  0.0f, 0.0f,
    -0.5f,  0.5f, -0.5f,  0.0f, 1.0f
};

GLuint compile_shader(GLenum type, const char* src) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, NULL);
    glCompileShader(shader);
    
    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(shader, 512, NULL, infoLog);
        printf("Shader compilation failed: %s\n", infoLog);
    }
    return shader;
}

void create_program() {
    program = glCreateProgram();
    GLuint vs = compile_shader(GL_VERTEX_SHADER, vertex_shader_source);
    GLuint fs = compile_shader(GL_FRAGMENT_SHADER, fragment_shader_source);
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);
    
    GLint success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(program, 512, NULL, infoLog);
        printf("Program linking failed: %s\n", infoLog);
        program = 0; // Mark as invalid
    }
}

void load_texture(const char* path) {
    int w, h, n;
    unsigned char* data = stbi_load(path, &w, &h, &n, 4);
    
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    
    if (data) {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
        stbi_image_free(data);
    } else {
        printf("Failed to load texture %s\n", path);
        unsigned char pixels[] = {255,0,255,255, 0,255,0,255, 0,0,255,255, 255,255,0,255};
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    }
    
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
}

int EGL_init(int width, int height) {
    // 1. Load the extension function
    PFNEGLGETPLATFORMDISPLAYEXTPROC getPlatformDisplayEXT = 
        (PFNEGLGETPLATFORMDISPLAYEXTPROC)eglGetProcAddress("eglGetPlatformDisplayEXT");
    
    // 2. Try surfaceless first if available
    if (getPlatformDisplayEXT) {
        egl.display = getPlatformDisplayEXT(EGL_PLATFORM_SURFACELESS_MESA, 
                                          EGL_DEFAULT_DISPLAY, 
                                          NULL);
        if (egl.display != EGL_NO_DISPLAY) {
            printf("Using surfaceless EGL\n");
        }
    }
    
    // 3. Fallback to default display
    if (egl.display == EGL_NO_DISPLAY) {
        egl.display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
        printf("Using default EGL display\n");
    }

    // Rest of initialization remains the same...
    EGLint major, minor;
    if (!eglInitialize(egl.display, &major, &minor)) {
        printf("EGL init failed: %#x\n", eglGetError());
        return -1;
    }
    const EGLint configAttribs[] = {
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_NONE
    };

    EGLint numConfigs;
    if (!eglChooseConfig(egl.display, configAttribs, &egl.config, 1, &numConfigs)) {
        printf("No EGL config found. Error: %#x\n", eglGetError());
        return -1;
    }

    const EGLint contextAttribs[] = {
        EGL_CONTEXT_CLIENT_VERSION, 2,
        EGL_NONE
    };
    egl.context = eglCreateContext(egl.display, egl.config, EGL_NO_CONTEXT, contextAttribs);
    if (egl.context == EGL_NO_CONTEXT) {
        printf("Context creation failed. Error: %#x\n", eglGetError());
        return -1;
    }

    if (!eglMakeCurrent(egl.display, EGL_NO_SURFACE, EGL_NO_SURFACE, egl.context)) {
        printf("MakeCurrent failed. Error: %#x\n", eglGetError());
        return -1;
    }

    printf("EGL initialized successfully\n");
    return 0;
}


int setup_textures_framebuffers(int width, int height) {
    // Create framebuffer with depth buffer  
    glGenTextures(1, &fbo_tex);
    glBindTexture(GL_TEXTURE_2D, fbo_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // Create depth renderbuffer
    glGenRenderbuffers(1, &depth_rb);
    glBindRenderbuffer(GL_RENDERBUFFER, depth_rb);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, width, height);

    // Create and configure FBO
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fbo_tex, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth_rb);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        printf("Framebuffer not complete (status: 0x%x)\n", glCheckFramebufferStatus(GL_FRAMEBUFFER));
        return -1;
    }

    // Set up shader program and get uniform locations
    create_program();
    load_texture("container.jpg");

    // Set up vertex buffers
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cube_vertices), cube_vertices, GL_STATIC_DRAW);

    glUseProgram(program);
    GLuint pos_loc = glGetAttribLocation(program, "position");
    GLuint tex_loc = glGetAttribLocation(program, "texCoord");
    mvp_loc = glGetUniformLocation(program, "mvp");

    glEnableVertexAttribArray(pos_loc);
    glVertexAttribPointer(pos_loc, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(tex_loc);
    glVertexAttribPointer(tex_loc, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));

    // Set viewport
    glViewport(0, 0, width, height);
    return 0;
}

int render_the_cube(int width, int height, uint8_t* dumb_buffer) {
    // Clear and enable depth test
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);

    // Calculate transformation matrices
    float time = (float)clock() / CLOCKS_PER_SEC * 4; //time * 4 bcz to move cube faster
    float angle = time * radians(75.0f);

    mat4 model = rotate(mat4(1.0f), angle, vec3(0.5f, 1.0f, 0.0f));
    mat4 view = lookAt(vec3(2.0f, 2.0f, 2.0f), vec3(0.0f, 0.0f, 0.0f), vec3(0.0f, 1.0f, 0.0f));
    mat4 proj = perspective(radians(45.0f), (float)width/height, 0.1f, 100.0f);
    mat4 mvp = proj * view * model;

    // Draw the cube
    glUniformMatrix4fv(mvp_loc, 1, GL_FALSE, &mvp[0][0]);
    glBindTexture(GL_TEXTURE_2D, tex);
    glDrawArrays(GL_TRIANGLES, 0, 36);

    // Read pixels to dumb buffer
    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, dumb_buffer);

    return 0;
}

int cleanup_gl_setup() {
    glDeleteBuffers(1, &vbo);
    glDeleteTextures(1, &tex);
    glDeleteProgram(program);
    glDeleteTextures(1, &fbo_tex);
    glDeleteRenderbuffers(1, &depth_rb);
    glDeleteFramebuffers(1, &fbo);
    

    return 0;
}