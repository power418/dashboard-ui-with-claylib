#pragma once

#include "render_buffer.hpp"

#include <array>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

namespace dashboard {

enum class AntiAliasMode {
    Off,
    FXAA,
    SMAA,
};

class ShaderProgram {
public:
    ShaderProgram() = default;
    ShaderProgram(const ShaderProgram &) = delete;
    ShaderProgram &operator=(const ShaderProgram &) = delete;

    ~ShaderProgram() {
        Destroy();
    }

    void Destroy() {
        if (program_) {
            glDeleteProgram(program_);
            program_ = 0;
        }
    }

    bool Load(const std::string &vertexPath, const std::string &fragmentPath) {
        Destroy();
        const std::string vertexSource = ReadFile(vertexPath);
        const std::string fragmentSource = ReadFile(fragmentPath);
        if (vertexSource.empty() || fragmentSource.empty()) {
            return false;
        }

        const GLuint vertexShader = Compile(GL_VERTEX_SHADER, vertexSource, vertexPath);
        const GLuint fragmentShader = Compile(GL_FRAGMENT_SHADER, fragmentSource, fragmentPath);
        if (!vertexShader || !fragmentShader) {
            if (vertexShader) {
                glDeleteShader(vertexShader);
            }
            if (fragmentShader) {
                glDeleteShader(fragmentShader);
            }
            return false;
        }

        program_ = glCreateProgram();
        glAttachShader(program_, vertexShader);
        glAttachShader(program_, fragmentShader);
        glLinkProgram(program_);
        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);

        GLint linked = GL_FALSE;
        glGetProgramiv(program_, GL_LINK_STATUS, &linked);
        if (!linked) {
            char log[2048]{};
            glGetProgramInfoLog(program_, sizeof(log), nullptr, log);
            std::fprintf(stderr, "OpenGL shader link failed for %s: %s\n", fragmentPath.c_str(), log);
            Destroy();
            return false;
        }
        return true;
    }

    void Use() const {
        glUseProgram(program_);
    }

    GLint Uniform(const char *name) const {
        return glGetUniformLocation(program_, name);
    }

    void SetInt(const char *name, int value) const {
        glUniform1i(Uniform(name), value);
    }

    void SetFloat(const char *name, float value) const {
        glUniform1f(Uniform(name), value);
    }

    void SetVec2(const char *name, float x, float y) const {
        glUniform2f(Uniform(name), x, y);
    }

private:
    static std::string ReadFile(const std::string &path) {
        std::ifstream file(path, std::ios::binary);
        if (!file) {
            std::fprintf(stderr, "Could not open shader: %s\n", path.c_str());
            return {};
        }
        std::ostringstream stream;
        stream << file.rdbuf();
        return stream.str();
    }

    static GLuint Compile(GLenum type, const std::string &source, const std::string &path) {
        const GLuint shader = glCreateShader(type);
        const char *sourceChars = source.c_str();
        glShaderSource(shader, 1, &sourceChars, nullptr);
        glCompileShader(shader);

        GLint compiled = GL_FALSE;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
        if (!compiled) {
            char log[2048]{};
            glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
            std::fprintf(stderr, "OpenGL shader compile failed for %s: %s\n", path.c_str(), log);
            glDeleteShader(shader);
            return 0;
        }
        return shader;
    }

    GLuint program_ = 0;
};

class PostProcessPipeline {
public:
    PostProcessPipeline() = default;
    PostProcessPipeline(const PostProcessPipeline &) = delete;
    PostProcessPipeline &operator=(const PostProcessPipeline &) = delete;

    ~PostProcessPipeline() {
        Destroy();
    }

    void Destroy() {
        passthrough_.Destroy();
        fxaa_.Destroy();
        smaaEdges_.Destroy();
        smaaBlend_.Destroy();
        smaaNeighborhood_.Destroy();
        edgeTarget_.Destroy();
        blendTarget_.Destroy();
        if (quadVbo_) {
            glDeleteBuffers(1, &quadVbo_);
            quadVbo_ = 0;
        }
        if (quadVao_) {
            glDeleteVertexArrays(1, &quadVao_);
            quadVao_ = 0;
        }
    }

    bool Load(const std::string &shaderDirectory) {
        const std::string prefix = shaderDirectory + "/";
        const std::string vertex = prefix + "fullscreen.vert";
        if (!passthrough_.Load(vertex, prefix + "passthrough.frag")) {
            return false;
        }
        if (!fxaa_.Load(vertex, prefix + "fxaa.frag")) {
            return false;
        }
        if (!smaaEdges_.Load(vertex, prefix + "smaa_edges.frag")) {
            return false;
        }
        if (!smaaBlend_.Load(vertex, prefix + "smaa_blend.frag")) {
            return false;
        }
        if (!smaaNeighborhood_.Load(vertex, prefix + "smaa_neighborhood.frag")) {
            return false;
        }
        CreateFullscreenQuad();
        return true;
    }

    bool Render(GLuint sceneTexture, int inputWidth, int inputHeight, int outputWidth, int outputHeight, AntiAliasMode mode) {
        if (!quadVao_) {
            CreateFullscreenQuad();
        }
        inputWidth = inputWidth < 1 ? 1 : inputWidth;
        inputHeight = inputHeight < 1 ? 1 : inputHeight;
        outputWidth = outputWidth < 1 ? 1 : outputWidth;
        outputHeight = outputHeight < 1 ? 1 : outputHeight;

        glDisable(GL_DEPTH_TEST);
        glDisable(GL_BLEND);
        glActiveTexture(GL_TEXTURE0);

        switch (mode) {
            case AntiAliasMode::Off:
                DrawToBackbuffer(passthrough_, sceneTexture, inputWidth, inputHeight, outputWidth, outputHeight);
                break;
            case AntiAliasMode::FXAA:
                DrawToBackbuffer(fxaa_, sceneTexture, inputWidth, inputHeight, outputWidth, outputHeight);
                break;
            case AntiAliasMode::SMAA:
                if (!edgeTarget_.Resize(inputWidth, inputHeight) || !blendTarget_.Resize(inputWidth, inputHeight)) {
                    return false;
                }
                edgeTarget_.Bind();
                glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
                glClear(GL_COLOR_BUFFER_BIT);
                DrawScenePass(smaaEdges_, sceneTexture, inputWidth, inputHeight);

                blendTarget_.Bind();
                glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
                glClear(GL_COLOR_BUFFER_BIT);
                smaaBlend_.Use();
                BindTexture(0, edgeTarget_.Texture());
                smaaBlend_.SetInt("uEdges", 0);
                smaaBlend_.SetVec2("uResolution", static_cast<float>(inputWidth), static_cast<float>(inputHeight));
                smaaBlend_.SetVec2("uInvResolution", 1.0f / static_cast<float>(inputWidth), 1.0f / static_cast<float>(inputHeight));
                DrawQuad();

                glBindFramebuffer(GL_FRAMEBUFFER, 0);
                glViewport(0, 0, outputWidth, outputHeight);
                smaaNeighborhood_.Use();
                BindTexture(0, sceneTexture);
                BindTexture(1, blendTarget_.Texture());
                smaaNeighborhood_.SetInt("uTexture", 0);
                smaaNeighborhood_.SetInt("uBlend", 1);
                smaaNeighborhood_.SetVec2("uResolution", static_cast<float>(inputWidth), static_cast<float>(inputHeight));
                smaaNeighborhood_.SetVec2("uInvResolution", 1.0f / static_cast<float>(inputWidth), 1.0f / static_cast<float>(inputHeight));
                DrawQuad();
                break;
        }
        glBindTexture(GL_TEXTURE_2D, 0);
        glUseProgram(0);
        return true;
    }

    void CompositeOverlay(GLuint overlayTexture, int width, int height) {
        width = width < 1 ? 1 : width;
        height = height < 1 ? 1 : height;
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, width, height);
        glDisable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        passthrough_.Use();
        BindTexture(0, overlayTexture);
        passthrough_.SetInt("uTexture", 0);
        passthrough_.SetVec2("uResolution", static_cast<float>(width), static_cast<float>(height));
        passthrough_.SetVec2("uInvResolution", 1.0f / static_cast<float>(width), 1.0f / static_cast<float>(height));
        DrawQuad();
        glDisable(GL_BLEND);
        glBindTexture(GL_TEXTURE_2D, 0);
        glUseProgram(0);
    }

private:
    static void BindTexture(int unit, GLuint texture) {
        glActiveTexture(static_cast<GLenum>(GL_TEXTURE0 + unit));
        glBindTexture(GL_TEXTURE_2D, texture);
    }

    void DrawScenePass(const ShaderProgram &program, GLuint texture, int width, int height) {
        program.Use();
        BindTexture(0, texture);
        program.SetInt("uTexture", 0);
        program.SetVec2("uResolution", static_cast<float>(width), static_cast<float>(height));
        program.SetVec2("uInvResolution", 1.0f / static_cast<float>(width), 1.0f / static_cast<float>(height));
        DrawQuad();
    }

    void DrawToBackbuffer(const ShaderProgram &program, GLuint texture, int inputWidth, int inputHeight, int outputWidth, int outputHeight) {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, outputWidth, outputHeight);
        DrawScenePass(program, texture, inputWidth, inputHeight);
    }

    void CreateFullscreenQuad() {
        if (quadVao_) {
            return;
        }
        constexpr std::array<float, 24> vertices{
            -1.0f, -1.0f, 0.0f, 1.0f,
             1.0f, -1.0f, 1.0f, 1.0f,
             1.0f,  1.0f, 1.0f, 0.0f,
            -1.0f, -1.0f, 0.0f, 1.0f,
             1.0f,  1.0f, 1.0f, 0.0f,
            -1.0f,  1.0f, 0.0f, 0.0f,
        };

        glGenVertexArrays(1, &quadVao_);
        glGenBuffers(1, &quadVbo_);
        glBindVertexArray(quadVao_);
        glBindBuffer(GL_ARRAY_BUFFER, quadVbo_);
        glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vertices.size() * sizeof(float)), vertices.data(), GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), reinterpret_cast<void *>(0));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), reinterpret_cast<void *>(2 * sizeof(float)));
        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    void DrawQuad() {
        glBindVertexArray(quadVao_);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glBindVertexArray(0);
    }

    ShaderProgram passthrough_;
    ShaderProgram fxaa_;
    ShaderProgram smaaEdges_;
    ShaderProgram smaaBlend_;
    ShaderProgram smaaNeighborhood_;
    GlRenderTarget edgeTarget_;
    GlRenderTarget blendTarget_;
    GLuint quadVao_ = 0;
    GLuint quadVbo_ = 0;
};

inline const char *AntiAliasModeName(AntiAliasMode mode) {
    switch (mode) {
        case AntiAliasMode::Off:
            return "AA Off";
        case AntiAliasMode::FXAA:
            return "FXAA";
        case AntiAliasMode::SMAA:
            return "SMAA";
    }
    return "AA";
}

} // namespace dashboard
