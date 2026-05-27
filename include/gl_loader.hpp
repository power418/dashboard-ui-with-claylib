#pragma once

#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>
#include <SDL3/SDL_opengl_glext.h>

#include <cstdio>

namespace dashboard {

inline PFNGLACTIVETEXTUREPROC glActiveTexture_ = nullptr;
inline PFNGLATTACHSHADERPROC glAttachShader_ = nullptr;
inline PFNGLBINDBUFFERPROC glBindBuffer_ = nullptr;
inline PFNGLBINDFRAMEBUFFERPROC glBindFramebuffer_ = nullptr;
inline PFNGLBINDVERTEXARRAYPROC glBindVertexArray_ = nullptr;
inline PFNGLBUFFERDATAPROC glBufferData_ = nullptr;
inline PFNGLCHECKFRAMEBUFFERSTATUSPROC glCheckFramebufferStatus_ = nullptr;
inline PFNGLCOMPILESHADERPROC glCompileShader_ = nullptr;
inline PFNGLCREATEPROGRAMPROC glCreateProgram_ = nullptr;
inline PFNGLCREATESHADERPROC glCreateShader_ = nullptr;
inline PFNGLDELETEBUFFERSPROC glDeleteBuffers_ = nullptr;
inline PFNGLDELETEFRAMEBUFFERSPROC glDeleteFramebuffers_ = nullptr;
inline PFNGLDELETEPROGRAMPROC glDeleteProgram_ = nullptr;
inline PFNGLDELETESHADERPROC glDeleteShader_ = nullptr;
inline PFNGLDELETEVERTEXARRAYSPROC glDeleteVertexArrays_ = nullptr;
inline PFNGLENABLEVERTEXATTRIBARRAYPROC glEnableVertexAttribArray_ = nullptr;
inline PFNGLFRAMEBUFFERTEXTURE2DPROC glFramebufferTexture2D_ = nullptr;
inline PFNGLGENBUFFERSPROC glGenBuffers_ = nullptr;
inline PFNGLGENFRAMEBUFFERSPROC glGenFramebuffers_ = nullptr;
inline PFNGLGENVERTEXARRAYSPROC glGenVertexArrays_ = nullptr;
inline PFNGLGETPROGRAMINFOLOGPROC glGetProgramInfoLog_ = nullptr;
inline PFNGLGETPROGRAMIVPROC glGetProgramiv_ = nullptr;
inline PFNGLGETSHADERINFOLOGPROC glGetShaderInfoLog_ = nullptr;
inline PFNGLGETSHADERIVPROC glGetShaderiv_ = nullptr;
inline PFNGLGETUNIFORMLOCATIONPROC glGetUniformLocation_ = nullptr;
inline PFNGLLINKPROGRAMPROC glLinkProgram_ = nullptr;
inline PFNGLSHADERSOURCEPROC glShaderSource_ = nullptr;
inline PFNGLUNIFORM1FPROC glUniform1f_ = nullptr;
inline PFNGLUNIFORM1IPROC glUniform1i_ = nullptr;
inline PFNGLUNIFORM2FPROC glUniform2f_ = nullptr;
inline PFNGLUSEPROGRAMPROC glUseProgram_ = nullptr;
inline PFNGLVERTEXATTRIBPOINTERPROC glVertexAttribPointer_ = nullptr;

template <typename T>
bool LoadGlFunction(T &target, const char *name) {
    target = reinterpret_cast<T>(SDL_GL_GetProcAddress(name));
    if (!target) {
        std::fprintf(stderr, "Failed to load OpenGL function: %s\n", name);
        return false;
    }
    return true;
}

inline bool LoadOpenGLFunctions() {
    bool ok = true;
    ok &= LoadGlFunction(glActiveTexture_, "glActiveTexture");
    ok &= LoadGlFunction(glAttachShader_, "glAttachShader");
    ok &= LoadGlFunction(glBindBuffer_, "glBindBuffer");
    ok &= LoadGlFunction(glBindFramebuffer_, "glBindFramebuffer");
    ok &= LoadGlFunction(glBindVertexArray_, "glBindVertexArray");
    ok &= LoadGlFunction(glBufferData_, "glBufferData");
    ok &= LoadGlFunction(glCheckFramebufferStatus_, "glCheckFramebufferStatus");
    ok &= LoadGlFunction(glCompileShader_, "glCompileShader");
    ok &= LoadGlFunction(glCreateProgram_, "glCreateProgram");
    ok &= LoadGlFunction(glCreateShader_, "glCreateShader");
    ok &= LoadGlFunction(glDeleteBuffers_, "glDeleteBuffers");
    ok &= LoadGlFunction(glDeleteFramebuffers_, "glDeleteFramebuffers");
    ok &= LoadGlFunction(glDeleteProgram_, "glDeleteProgram");
    ok &= LoadGlFunction(glDeleteShader_, "glDeleteShader");
    ok &= LoadGlFunction(glDeleteVertexArrays_, "glDeleteVertexArrays");
    ok &= LoadGlFunction(glEnableVertexAttribArray_, "glEnableVertexAttribArray");
    ok &= LoadGlFunction(glFramebufferTexture2D_, "glFramebufferTexture2D");
    ok &= LoadGlFunction(glGenBuffers_, "glGenBuffers");
    ok &= LoadGlFunction(glGenFramebuffers_, "glGenFramebuffers");
    ok &= LoadGlFunction(glGenVertexArrays_, "glGenVertexArrays");
    ok &= LoadGlFunction(glGetProgramInfoLog_, "glGetProgramInfoLog");
    ok &= LoadGlFunction(glGetProgramiv_, "glGetProgramiv");
    ok &= LoadGlFunction(glGetShaderInfoLog_, "glGetShaderInfoLog");
    ok &= LoadGlFunction(glGetShaderiv_, "glGetShaderiv");
    ok &= LoadGlFunction(glGetUniformLocation_, "glGetUniformLocation");
    ok &= LoadGlFunction(glLinkProgram_, "glLinkProgram");
    ok &= LoadGlFunction(glShaderSource_, "glShaderSource");
    ok &= LoadGlFunction(glUniform1f_, "glUniform1f");
    ok &= LoadGlFunction(glUniform1i_, "glUniform1i");
    ok &= LoadGlFunction(glUniform2f_, "glUniform2f");
    ok &= LoadGlFunction(glUseProgram_, "glUseProgram");
    ok &= LoadGlFunction(glVertexAttribPointer_, "glVertexAttribPointer");
    return ok;
}

} // namespace dashboard

#define glActiveTexture dashboard::glActiveTexture_
#define glAttachShader dashboard::glAttachShader_
#define glBindBuffer dashboard::glBindBuffer_
#define glBindFramebuffer dashboard::glBindFramebuffer_
#define glBindVertexArray dashboard::glBindVertexArray_
#define glBufferData dashboard::glBufferData_
#define glCheckFramebufferStatus dashboard::glCheckFramebufferStatus_
#define glCompileShader dashboard::glCompileShader_
#define glCreateProgram dashboard::glCreateProgram_
#define glCreateShader dashboard::glCreateShader_
#define glDeleteBuffers dashboard::glDeleteBuffers_
#define glDeleteFramebuffers dashboard::glDeleteFramebuffers_
#define glDeleteProgram dashboard::glDeleteProgram_
#define glDeleteShader dashboard::glDeleteShader_
#define glDeleteVertexArrays dashboard::glDeleteVertexArrays_
#define glEnableVertexAttribArray dashboard::glEnableVertexAttribArray_
#define glFramebufferTexture2D dashboard::glFramebufferTexture2D_
#define glGenBuffers dashboard::glGenBuffers_
#define glGenFramebuffers dashboard::glGenFramebuffers_
#define glGenVertexArrays dashboard::glGenVertexArrays_
#define glGetProgramInfoLog dashboard::glGetProgramInfoLog_
#define glGetProgramiv dashboard::glGetProgramiv_
#define glGetShaderInfoLog dashboard::glGetShaderInfoLog_
#define glGetShaderiv dashboard::glGetShaderiv_
#define glGetUniformLocation dashboard::glGetUniformLocation_
#define glLinkProgram dashboard::glLinkProgram_
#define glShaderSource dashboard::glShaderSource_
#define glUniform1f dashboard::glUniform1f_
#define glUniform1i dashboard::glUniform1i_
#define glUniform2f dashboard::glUniform2f_
#define glUseProgram dashboard::glUseProgram_
#define glVertexAttribPointer dashboard::glVertexAttribPointer_
