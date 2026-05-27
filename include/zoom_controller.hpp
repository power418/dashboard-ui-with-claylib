#pragma once

#include <SDL2/SDL.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstddef>

namespace dashboard {

class ZoomController {
public:
    bool HandleEvent(const SDL_Event &event) {
        if (event.type == SDL_MOUSEWHEEL) {
            if (!AcceleratorDown(SDL_GetModState())) {
                return false;
            }
            int wheelY = event.wheel.y;
            if (event.wheel.direction == SDL_MOUSEWHEEL_FLIPPED) {
                wheelY = -wheelY;
            }
            if (wheelY > 0) {
                ZoomIn();
                return true;
            }
            if (wheelY < 0) {
                ZoomOut();
                return true;
            }
            return true;
        }

        if (event.type != SDL_KEYDOWN || !AcceleratorDown(static_cast<SDL_Keymod>(event.key.keysym.mod))) {
            return false;
        }

        switch (event.key.keysym.sym) {
            case SDLK_PLUS:
            case SDLK_EQUALS:
            case SDLK_KP_PLUS:
            case SDLK_KP_EQUALS:
                ZoomIn();
                return true;
            case SDLK_MINUS:
            case SDLK_UNDERSCORE:
            case SDLK_KP_MINUS:
                ZoomOut();
                return true;
            case SDLK_0:
            case SDLK_KP_0:
                Reset();
                return true;
            default:
                return false;
        }
    }

    void ZoomIn() {
        const auto next = std::find_if(kBrowserSteps.begin(), kBrowserSteps.end(), [this](float step) {
            return step > scale_ + kEpsilon;
        });
        if (next != kBrowserSteps.end()) {
            scale_ = *next;
        }
    }

    void ZoomOut() {
        for (auto it = kBrowserSteps.rbegin(); it != kBrowserSteps.rend(); ++it) {
            if (*it < scale_ - kEpsilon) {
                scale_ = *it;
                return;
            }
        }
    }

    void Reset() {
        scale_ = 1.0f;
    }

    float Scale() const {
        return scale_;
    }

    int Percent() const {
        return static_cast<int>(std::round(scale_ * 100.0f));
    }

    void FormatPercent(char *buffer, std::size_t bufferSize) const {
        if (!buffer || bufferSize == 0) {
            return;
        }
        std::snprintf(buffer, bufferSize, "%d%%", Percent());
    }

private:
    static bool AcceleratorDown(SDL_Keymod mod) {
        return (mod & (KMOD_CTRL | KMOD_GUI)) != 0;
    }

    static constexpr float kEpsilon = 0.001f;
    static constexpr std::array<float, 17> kBrowserSteps{
        0.25f,
        0.33f,
        0.50f,
        0.67f,
        0.75f,
        0.80f,
        0.90f,
        1.00f,
        1.10f,
        1.25f,
        1.50f,
        1.75f,
        2.00f,
        2.50f,
        3.00f,
        4.00f,
        5.00f,
    };

    float scale_ = 1.0f;
};

inline ZoomController &Zoom() {
    static ZoomController controller;
    return controller;
}

inline bool HandleZoomEvent(const SDL_Event &event) {
    return Zoom().HandleEvent(event);
}

inline float ZoomScale() {
    return Zoom().Scale();
}

inline int ZoomPercent() {
    return Zoom().Percent();
}

} // namespace dashboard
