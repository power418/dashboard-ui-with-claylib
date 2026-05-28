#pragma once

#include <SDL3/SDL.h>

#include "gl_loader.hpp"

#include <algorithm>
#include <cstdio>

namespace dashboard {

class SoftwareRenderBuffer {
public:
    SoftwareRenderBuffer() = default;
    SoftwareRenderBuffer(const SoftwareRenderBuffer &) = delete;
    SoftwareRenderBuffer &operator=(const SoftwareRenderBuffer &) = delete;

    ~SoftwareRenderBuffer() {
        Destroy();
    }

    void Destroy() {
        DestroySurfaceOnly();
        if (texture_) {
            glDeleteTextures(1, &texture_);
            texture_ = 0;
        }
        width_ = 0;
        height_ = 0;
    }

    bool Resize(int requestedWidth, int requestedHeight, bool linearFiltering = true) {
        requestedWidth = std::max(1, requestedWidth);
        requestedHeight = std::max(1, requestedHeight);
        if (requestedWidth == width_ && requestedHeight == height_ && linearFiltering == linearFiltering_ && renderer_ && texture_) {
            return true;
        }

        DestroySurfaceOnly();
        width_ = requestedWidth;
        height_ = requestedHeight;
        linearFiltering_ = linearFiltering;

        surface_ = SDL_CreateSurface(width_, height_, SDL_PIXELFORMAT_RGBA32);
        if (!surface_) {
            std::fprintf(stderr, "SDL_CreateSurface failed: %s\n", SDL_GetError());
            return false;
        }

        renderer_ = SDL_CreateSoftwareRenderer(surface_);
        if (!renderer_) {
            std::fprintf(stderr, "SDL_CreateSoftwareRenderer failed: %s\n", SDL_GetError());
            return false;
        }
        SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);

        if (!texture_) {
            glGenTextures(1, &texture_);
        }
        glBindTexture(GL_TEXTURE_2D, texture_);
        const GLint filter = linearFiltering_ ? GL_LINEAR : GL_NEAREST;
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width_, height_, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glBindTexture(GL_TEXTURE_2D, 0);
        return true;
    }

    void Clear(Uint8 r, Uint8 g, Uint8 b, Uint8 a) const {
        if (!renderer_) {
            return;
        }
        SDL_BlendMode previousBlendMode = SDL_BLENDMODE_BLEND;
        SDL_GetRenderDrawBlendMode(renderer_, &previousBlendMode);
        SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_NONE);
        SDL_SetRenderDrawColor(renderer_, r, g, b, a);
        SDL_RenderClear(renderer_);
        SDL_SetRenderDrawBlendMode(renderer_, previousBlendMode);
    }

    void Upload() const {
        if (!surface_ || !texture_) {
            return;
        }
        glBindTexture(GL_TEXTURE_2D, texture_);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, surface_->pitch / 4);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width_, height_, GL_RGBA, GL_UNSIGNED_BYTE, surface_->pixels);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    SDL_Renderer *Renderer() const {
        return renderer_;
    }

    GLuint Texture() const {
        return texture_;
    }

    int Width() const {
        return width_;
    }

    int Height() const {
        return height_;
    }

private:
    void DestroySurfaceOnly() {
        if (renderer_) {
            SDL_DestroyRenderer(renderer_);
            renderer_ = nullptr;
        }
        if (surface_) {
            SDL_FreeSurface(surface_);
            surface_ = nullptr;
        }
    }

    int width_ = 0;
    int height_ = 0;
    bool linearFiltering_ = true;
    SDL_Surface *surface_ = nullptr;
    SDL_Renderer *renderer_ = nullptr;
    GLuint texture_ = 0;
};

class GlRenderTarget {
public:
    GlRenderTarget() = default;
    GlRenderTarget(const GlRenderTarget &) = delete;
    GlRenderTarget &operator=(const GlRenderTarget &) = delete;

    ~GlRenderTarget() {
        Destroy();
    }

    void Destroy() {
        if (texture_) {
            glDeleteTextures(1, &texture_);
            texture_ = 0;
        }
        if (framebuffer_) {
            glDeleteFramebuffers(1, &framebuffer_);
            framebuffer_ = 0;
        }
        width_ = 0;
        height_ = 0;
    }

    bool Resize(int requestedWidth, int requestedHeight) {
        requestedWidth = std::max(1, requestedWidth);
        requestedHeight = std::max(1, requestedHeight);
        if (requestedWidth == width_ && requestedHeight == height_ && framebuffer_ && texture_) {
            return true;
        }

        width_ = requestedWidth;
        height_ = requestedHeight;
        if (!framebuffer_) {
            glGenFramebuffers(1, &framebuffer_);
        }
        if (!texture_) {
            glGenTextures(1, &texture_);
        }

        glBindTexture(GL_TEXTURE_2D, texture_);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width_, height_, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture_, 0);
        const GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glBindTexture(GL_TEXTURE_2D, 0);

        if (status != GL_FRAMEBUFFER_COMPLETE) {
            std::fprintf(stderr, "OpenGL framebuffer incomplete: 0x%x\n", status);
            return false;
        }
        return true;
    }

    void Bind() const {
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_);
        glViewport(0, 0, width_, height_);
    }

    GLuint Texture() const {
        return texture_;
    }

private:
    int width_ = 0;
    int height_ = 0;
    GLuint framebuffer_ = 0;
    GLuint texture_ = 0;
};

} // namespace dashboard
