#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

#define CLAY_IMPLEMENTATION
#include "../lib/clay.h"

#include "../include/shader_pipeline.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace {

constexpr int kInitialWidth = 1180;
constexpr int kInitialHeight = 760;

constexpr Clay_Color Rgba(float r, float g, float b, float a = 255.0f) {
    return Clay_Color{r, g, b, a};
}

namespace Pal {
constexpr Clay_Color Page = Rgba(10, 11, 12);
constexpr Clay_Color Shell = Rgba(10, 11, 12);
constexpr Clay_Color ShellEdge = Rgba(43, 45, 52);
constexpr Clay_Color Sidebar = Rgba(11, 13, 15);
constexpr Clay_Color Card = Rgba(17, 18, 20);
constexpr Clay_Color Card2 = Rgba(21, 22, 25);
constexpr Clay_Color Card3 = Rgba(28, 30, 36);
constexpr Clay_Color Stroke = Rgba(35, 37, 43);
constexpr Clay_Color StrokeSoft = Rgba(28, 30, 36);
constexpr Clay_Color Text = Rgba(238, 241, 245);
constexpr Clay_Color TextDim = Rgba(151, 156, 165);
constexpr Clay_Color TextFaint = Rgba(94, 99, 110);
constexpr Clay_Color Green = Rgba(45, 206, 136);
constexpr Clay_Color GreenDim = Rgba(35, 126, 89);
constexpr Clay_Color Red = Rgba(255, 84, 109);
constexpr Clay_Color RedDim = Rgba(136, 42, 57);
constexpr Clay_Color Yellow = Rgba(243, 193, 73);
constexpr Clay_Color YellowDim = Rgba(136, 100, 35);
constexpr Clay_Color Blue = Rgba(77, 166, 255);
constexpr Clay_Color Purple = Rgba(186, 94, 255);
constexpr Clay_Color Cyan = Rgba(70, 213, 225);
constexpr Clay_Color Transparent = Rgba(0, 0, 0, 0);
} // namespace Pal

bool gHandCursorRequested = false;
dashboard::AntiAliasMode gAntiAliasMode = dashboard::AntiAliasMode::SMAA;
float gUiScale = 1.0f;
int gRenderScale = 1;
int gRequestedRenderScale = 2;
char gAntiAliasBadge[32] = "SMAA 1x";

float Scale(float value) {
    return value * gUiScale;
}

uint16_t ScaleU16(uint16_t value) {
    return static_cast<uint16_t>(std::max(1.0f, std::round(static_cast<float>(value) * gUiScale)));
}

uint16_t ScaleFont(uint16_t value) {
    return static_cast<uint16_t>(std::max(1.0f, std::round(static_cast<float>(value) * gUiScale)));
}

int ScalePixels(int value) {
    return std::max(1, static_cast<int>(std::round(static_cast<float>(value) * gUiScale)));
}

Clay_String Str(const char *value) {
    return Clay_String{false, static_cast<int32_t>(std::strlen(value)), value};
}

Clay_SizingAxis Fixed(float value) {
    Clay_SizingAxis axis{};
    axis.type = CLAY__SIZING_TYPE_FIXED;
    axis.size.minMax.min = Scale(value);
    axis.size.minMax.max = Scale(value);
    return axis;
}

Clay_SizingAxis Grow(float min = 0.0f) {
    Clay_SizingAxis axis{};
    axis.type = CLAY__SIZING_TYPE_GROW;
    axis.size.minMax.min = Scale(min);
    axis.size.minMax.max = 0.0f;
    return axis;
}

Clay_SizingAxis Fit(float min = 0.0f) {
    Clay_SizingAxis axis{};
    axis.type = CLAY__SIZING_TYPE_FIT;
    axis.size.minMax.min = Scale(min);
    axis.size.minMax.max = 0.0f;
    return axis;
}

Clay_SizingAxis Percent(float value) {
    Clay_SizingAxis axis{};
    axis.type = CLAY__SIZING_TYPE_PERCENT;
    axis.size.percent = value;
    return axis;
}

Clay_Padding Pad(uint16_t value) {
    const uint16_t scaled = ScaleU16(value);
    return Clay_Padding{scaled, scaled, scaled, scaled};
}

Clay_Padding PadXY(uint16_t x, uint16_t y) {
    return Clay_Padding{ScaleU16(x), ScaleU16(x), ScaleU16(y), ScaleU16(y)};
}

Clay_Padding PadLTRB(uint16_t left, uint16_t right, uint16_t top, uint16_t bottom) {
    return Clay_Padding{ScaleU16(left), ScaleU16(right), ScaleU16(top), ScaleU16(bottom)};
}

Clay_CornerRadius Radius(float value) {
    const float scaled = Scale(value);
    return Clay_CornerRadius{scaled, scaled, scaled, scaled};
}

Clay_BorderWidth BorderAll(uint16_t width) {
    const uint16_t scaled = ScaleU16(width);
    return Clay_BorderWidth{scaled, scaled, scaled, scaled, scaled};
}

Clay_ChildAlignment Align(Clay_LayoutAlignmentX x, Clay_LayoutAlignmentY y) {
    return Clay_ChildAlignment{x, y};
}

Clay_LayoutConfig Layout(
    Clay_SizingAxis width,
    Clay_SizingAxis height,
    Clay_LayoutDirection direction = CLAY_LEFT_TO_RIGHT,
    Clay_Padding padding = Clay_Padding{},
    uint16_t gap = 0,
    Clay_ChildAlignment align = Clay_ChildAlignment{CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_TOP}) {
    Clay_LayoutConfig config{};
    config.sizing = Clay_Sizing{width, height};
    config.padding = padding;
    config.childGap = gap == 0 ? 0 : ScaleU16(gap);
    config.childAlignment = align;
    config.layoutDirection = direction;
    return config;
}

void Text(const char *value, Clay_Color color, uint16_t size, uint16_t fontId = 0, uint16_t lineHeight = 0) {
    Clay_TextElementConfig config{};
    config.textColor = color;
    config.fontId = fontId;
    config.fontSize = ScaleFont(size);
    config.lineHeight = lineHeight == 0 ? ScaleFont(static_cast<uint16_t>(size + 4)) : ScaleFont(lineHeight);
    config.wrapMode = CLAY_TEXT_WRAP_NONE;
    CLAY_TEXT(Str(value), config);
}

bool PointerHeld() {
    const Clay_PointerData pointer = Clay_GetPointerState();
    return pointer.state == CLAY_POINTER_DATA_PRESSED || pointer.state == CLAY_POINTER_DATA_PRESSED_THIS_FRAME;
}

Clay_Color HoverColor(Clay_Color base, Clay_Color hover, bool handCursor = false) {
    const bool hovered = Clay_Hovered();
    if (hovered && handCursor) {
        gHandCursorRequested = true;
    }
    return hovered ? hover : base;
}

Clay_Color PressableColor(Clay_Color base, Clay_Color hover, Clay_Color pressed) {
    const bool hovered = Clay_Hovered();
    if (hovered) {
        gHandCursorRequested = true;
    }
    return hovered && PointerHeld() ? pressed : (hovered ? hover : base);
}

void Dot(Clay_Color color, float size = 5.0f) {
    CLAY_AUTO_ID({
        .layout = Layout(Fixed(size), Fixed(size)),
        .backgroundColor = color,
        .cornerRadius = Radius(size * 0.5f),
    }) {}
}

void Spacer() {
    CLAY_AUTO_ID({.layout = Layout(Grow(), Grow())}) {}
}

enum class CustomKind : uint8_t {
    Icon,
    Sparkline,
    MainChart,
    Avatar,
    MenuDots
};

struct CustomSpec {
    CustomKind kind;
    int variant;
    Clay_Color color;
    Clay_Color altColor;
};

const CustomSpec kIconSearch{CustomKind::Icon, 0, Pal::TextFaint, Pal::Transparent};
const CustomSpec kIconPlus{CustomKind::Icon, 1, Rgba(238, 255, 245), Pal::Transparent};
const CustomSpec kIconOverviewActive{CustomKind::Icon, 2, Pal::Text, Pal::Transparent};
const CustomSpec kIconOverview{CustomKind::Icon, 2, Pal::TextDim, Pal::Transparent};
const CustomSpec kIconAgents{CustomKind::Icon, 3, Pal::TextDim, Pal::Transparent};
const CustomSpec kIconRuns{CustomKind::Icon, 4, Pal::TextDim, Pal::Transparent};
const CustomSpec kIconTraces{CustomKind::Icon, 5, Pal::TextDim, Pal::Transparent};
const CustomSpec kIconEvals{CustomKind::Icon, 6, Pal::TextDim, Pal::Transparent};
const CustomSpec kIconCosts{CustomKind::Icon, 7, Pal::TextDim, Pal::Transparent};
const CustomSpec kIconLogs{CustomKind::Icon, 8, Pal::TextDim, Pal::Transparent};
const CustomSpec kIconAlerts{CustomKind::Icon, 9, Pal::TextDim, Pal::Transparent};
const CustomSpec kIconSettings{CustomKind::Icon, 10, Pal::TextDim, Pal::Transparent};
const CustomSpec kIconFilter{CustomKind::Icon, 11, Pal::TextDim, Pal::Transparent};
const CustomSpec kSparkGreen{CustomKind::Sparkline, 0, Pal::Green, Rgba(45, 206, 136, 34)};
const CustomSpec kSparkRed{CustomKind::Sparkline, 1, Pal::Red, Rgba(255, 84, 109, 36)};
const CustomSpec kMainChart{CustomKind::MainChart, 0, Pal::Green, Pal::Red};
const CustomSpec kAvatar{CustomKind::Avatar, 0, Pal::Cyan, Pal::Purple};
const CustomSpec kDots{CustomKind::MenuDots, 0, Pal::TextFaint, Pal::Transparent};

void CustomBox(const CustomSpec &spec, Clay_SizingAxis width, Clay_SizingAxis height, Clay_Color background = Pal::Transparent, float radius = 0.0f) {
    CLAY_AUTO_ID({
        .layout = Layout(width, height),
        .backgroundColor = background,
        .cornerRadius = Radius(radius),
        .custom = {.customData = const_cast<CustomSpec *>(&spec)},
    }) {}
}

void Icon(const CustomSpec &spec, float size = 14.0f) {
    CustomBox(spec, Fixed(size), Fixed(size));
}

void Pill(const char *label, Clay_Color background, Clay_Color color, float width = 45.0f) {
    CLAY_AUTO_ID({
        .layout = Layout(Fixed(width), Fixed(18), CLAY_LEFT_TO_RIGHT, PadXY(7, 0), 0, Align(CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER)),
        .backgroundColor = background,
        .cornerRadius = Radius(9),
    }) {
        Text(label, color, 8, 1, 10);
    }
}

void NavSection(const char *label) {
    CLAY_AUTO_ID({
        .layout = Layout(Grow(), Fixed(22), CLAY_LEFT_TO_RIGHT, PadLTRB(8, 8, 8, 0), 0, Align(CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_BOTTOM)),
    }) {
        Text(label, Pal::TextFaint, 9, 0, 11);
    }
}

void NavItem(const CustomSpec &icon, const char *label, const char *rightText, bool active = false, bool alert = false) {
    CLAY_AUTO_ID({
        .layout = Layout(Grow(), Fixed(24), CLAY_LEFT_TO_RIGHT, PadXY(8, 0), 8, Align(CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER)),
        .backgroundColor = PressableColor(active ? Pal::Card3 : Pal::Transparent, active ? Rgba(38, 41, 49) : Rgba(24, 26, 31), Rgba(18, 20, 25)),
        .cornerRadius = Radius(8),
    }) {
        const bool hovered = Clay_Hovered();
        Icon(active ? kIconOverviewActive : icon, 12);
        Text(label, active || hovered ? Pal::Text : Pal::TextDim, 9, active ? 1 : 0, 11);
        Spacer();
        if (alert) {
            CLAY_AUTO_ID({
                .layout = Layout(Fixed(14), Fixed(16), CLAY_LEFT_TO_RIGHT, {}, 0, Align(CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER)),
                .backgroundColor = Rgba(61, 17, 28),
                .cornerRadius = Radius(4),
            }) {
                Text(rightText, Pal::Red, 8, 1, 10);
            }
        } else if (rightText && rightText[0] != '\0') {
            Text(rightText, Pal::TextFaint, 8, 0, 10);
        }
    }
}

struct Metric {
    const char *title;
    const char *value;
    const char *suffix;
    const char *delta;
    bool positive;
    const CustomSpec *spark;
};

const std::array<Metric, 3> kMetrics{{
    {"Runs", "12,438", "", "+ 8.4% vs yesterday", true, &kSparkGreen},
    {"Success Rate", "98.7", "%", "- 0.4 pts vs 7d avg", false, &kSparkRed},
    {"Tokens Burned", "8.4", "M", "+ 12.1% / $142 / hr", true, &kSparkGreen},
}};

void MetricCard(const Metric &metric) {
    CLAY_AUTO_ID({
        .layout = Layout(Grow(), Fixed(86), CLAY_TOP_TO_BOTTOM, Pad(12), 6),
        .backgroundColor = HoverColor(Pal::Card, Rgba(22, 24, 28)),
        .cornerRadius = Radius(8),
        .border = {.color = HoverColor(Pal::StrokeSoft, Rgba(50, 55, 65)), .width = BorderAll(1)},
    }) {
        Text(metric.title, Pal::TextDim, 9, 0, 11);
        CLAY_AUTO_ID({
            .layout = Layout(Grow(), Fixed(28), CLAY_LEFT_TO_RIGHT, {}, 4, Align(CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_BOTTOM)),
        }) {
            Text(metric.value, Pal::Text, 22, 1, 26);
            if (metric.suffix[0] != '\0') {
                Text(metric.suffix, Pal::Text, 13, 1, 17);
            }
            Spacer();
            CustomBox(*metric.spark, Fixed(72), Fixed(30));
        }
        Text(metric.delta, metric.positive ? Pal::Green : Pal::Red, 9, 0, 11);
    }
}

void SearchBox() {
    CLAY_AUTO_ID({
        .layout = Layout(Fixed(246), Fixed(27), CLAY_LEFT_TO_RIGHT, PadLTRB(10, 12, 0, 0), 6, Align(CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER)),
        .backgroundColor = HoverColor(Rgba(28, 29, 35), Rgba(34, 36, 43), true),
        .cornerRadius = Radius(14),
        .border = {.color = HoverColor(Rgba(40, 42, 50), Rgba(65, 69, 82)), .width = BorderAll(1)},
    }) {
        Icon(kIconSearch, 13);
        Text("Search runs, agents, traces...", Pal::TextFaint, 9, 0, 11);
    }
}

void Header() {
    CLAY_AUTO_ID({
        .layout = Layout(Grow(), Fixed(69), CLAY_TOP_TO_BOTTOM, {}, 8),
    }) {
        CLAY_AUTO_ID({
            .layout = Layout(Grow(), Fixed(33), CLAY_LEFT_TO_RIGHT, {}, 12, Align(CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER)),
        }) {
            CLAY_AUTO_ID({
                .layout = Layout(Fixed(180), Grow(), CLAY_LEFT_TO_RIGHT, {}, 0, Align(CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER)),
            }) {
                Text("Overview", Pal::Text, 13, 1, 15);
            }
            Spacer();
            SearchBox();
            Spacer();
            CLAY_AUTO_ID({
                .layout = Layout(Fixed(64), Fixed(22), CLAY_LEFT_TO_RIGHT, {}, 0, Align(CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER)),
                .backgroundColor = HoverColor(Rgba(30, 32, 38), Rgba(38, 42, 50), true),
                .cornerRadius = Radius(11),
                .border = {.color = HoverColor(Rgba(45, 48, 58), Rgba(70, 78, 92)), .width = BorderAll(1)},
            }) {
                Text(gAntiAliasBadge, Pal::TextDim, 8, 1, 10);
            }
            CLAY_AUTO_ID({
                .layout = Layout(Fixed(84), Fixed(27), CLAY_LEFT_TO_RIGHT, PadLTRB(10, 10, 0, 0), 6, Align(CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER)),
                .backgroundColor = PressableColor(Rgba(34, 166, 112), Rgba(45, 190, 131), Rgba(26, 134, 90)),
                .cornerRadius = Radius(14),
            }) {
                Icon(kIconPlus, 10);
                Text("New Agent", Rgba(245, 255, 250), 9, 1, 11);
            }
        }
        CLAY_AUTO_ID({
            .layout = Layout(Grow(), Fixed(22), CLAY_LEFT_TO_RIGHT, {}, 0, Align(CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER)),
        }) {
            Text("Live monitoring across 24 agents", Pal::TextFaint, 9, 0, 11);
            Spacer();
            CLAY_AUTO_ID({
                .layout = Layout(Fixed(46), Fixed(18), CLAY_LEFT_TO_RIGHT, {}, 0, Align(CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER)),
                .backgroundColor = PressableColor(Pal::Transparent, Rgba(48, 19, 28), Rgba(36, 13, 20)),
                .cornerRadius = Radius(9),
            }) {
                Text("Filter", Pal::Red, 8, 1, 10);
            }
        }
    }
}

void ChartLegend(const char *label, const char *value, Clay_Color color) {
    CLAY_AUTO_ID({
        .layout = Layout(Fit(), Fixed(14), CLAY_LEFT_TO_RIGHT, {}, 4, Align(CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER)),
    }) {
        Dot(color, 7);
        Text(label, Pal::TextFaint, 8, 0, 10);
        Text(value, Pal::Text, 8, 0, 10);
    }
}

void MainChartCard() {
    CLAY_AUTO_ID({
        .layout = Layout(Grow(), Fixed(241), CLAY_TOP_TO_BOTTOM, PadLTRB(12, 12, 10, 10), 6),
        .backgroundColor = HoverColor(Pal::Card, Rgba(21, 23, 27)),
        .cornerRadius = Radius(8),
        .border = {.color = HoverColor(Pal::StrokeSoft, Rgba(48, 52, 62)), .width = BorderAll(1)},
    }) {
        CLAY_AUTO_ID({
            .layout = Layout(Grow(), Fixed(21), CLAY_LEFT_TO_RIGHT, {}, 10, Align(CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER)),
        }) {
            Text("Agent Runs - 30 min buckets", Pal::TextDim, 9, 0, 11);
            Spacer();
            ChartLegend("success", "12,272", Pal::Green);
            ChartLegend("failed", "128", Pal::Yellow);
            ChartLegend("timeout", "38", Pal::Red);
        }
        CustomBox(kMainChart, Grow(), Grow());
    }
}

void ProgressBar(float percent, Clay_Color color) {
    CLAY_AUTO_ID({
        .layout = Layout(Grow(), Fixed(3)),
        .backgroundColor = Rgba(33, 35, 40),
        .cornerRadius = Radius(2),
    }) {
        CLAY_AUTO_ID({
            .layout = Layout(Percent(percent), Grow()),
            .backgroundColor = color,
            .cornerRadius = Radius(2),
        }) {}
    }
}

void ToolRow(const char *name, const char *value, float percent, const char *note, Clay_Color noteColor = Pal::TextFaint) {
    CLAY_AUTO_ID({
        .layout = Layout(Grow(), Fixed(18), CLAY_TOP_TO_BOTTOM, {}, 3),
        .backgroundColor = HoverColor(Pal::Transparent, Rgba(27, 29, 35), true),
        .cornerRadius = Radius(4),
    }) {
        CLAY_AUTO_ID({
            .layout = Layout(Grow(), Fixed(9), CLAY_LEFT_TO_RIGHT, {}, 0, Align(CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER)),
        }) {
            Text(name, Pal::TextDim, 8, 0, 10);
            Spacer();
            Text(value, Pal::TextFaint, 8, 0, 10);
        }
        CLAY_AUTO_ID({
            .layout = Layout(Grow(), Fixed(5), CLAY_LEFT_TO_RIGHT, {}, 6, Align(CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER)),
        }) {
            ProgressBar(percent, Pal::Green);
            if (note && note[0] != '\0') {
                CLAY_AUTO_ID({.layout = Layout(Fixed(32), Fixed(10), CLAY_LEFT_TO_RIGHT, {}, 0, Align(CLAY_ALIGN_X_RIGHT, CLAY_ALIGN_Y_CENTER))}) {
                    Text(note, noteColor, 8, 0, 10);
                }
            }
        }
    }
}

void TopToolsCard() {
    CLAY_AUTO_ID({
        .layout = Layout(Grow(), Grow(), CLAY_TOP_TO_BOTTOM, Pad(10), 5),
        .backgroundColor = HoverColor(Pal::Card, Rgba(21, 23, 27)),
        .cornerRadius = Radius(8),
        .border = {.color = HoverColor(Pal::StrokeSoft, Rgba(48, 52, 62)), .width = BorderAll(1)},
    }) {
        CLAY_AUTO_ID({
            .layout = Layout(Grow(), Fixed(14), CLAY_LEFT_TO_RIGHT, {}, 0, Align(CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER)),
        }) {
            Text("Top Tools", Pal::Text, 9, 1, 11);
            Spacer();
            Icon(kDots, 10);
        }
        ToolRow("web_search", "24,820", 0.94f, "");
        ToolRow("r95 420ms", "0.4% err", 0.02f, "");
        ToolRow("fetch_url", "18,402", 0.82f, "");
        ToolRow("r95 680ms", "1.1% err", 0.18f, "1.1%", Pal::Yellow);
        ToolRow("db_query", "12,180", 0.64f, "");
        ToolRow("r95 1.4s", "0.0% err", 0.01f, "");
    }
}

struct ModelRow {
    const char *name;
    const char *tokens;
    const char *share;
    Clay_Color color;
    float percent;
};

const std::array<ModelRow, 6> kModelRows{{
    {"claude-sonnet-3.5", "4.4M", "52.4%", Pal::Blue, 0.524f},
    {"gpt-4o", "2.2M", "26.7%", Pal::Green, 0.267f},
    {"claude-haiku-3.5", "966K", "11.5%", Pal::Yellow, 0.115f},
    {"gemini-2.5-pro", "554K", "6.6%", Pal::Red, 0.066f},
    {"o1-preview", "235K", "2.8%", Pal::Cyan, 0.028f},
    {"claude-opus-4.7", "31K", "0.3%", Pal::Purple, 0.003f},
}};

void ModelSplitCard() {
    CLAY_AUTO_ID({
        .layout = Layout(Grow(), Grow(), CLAY_TOP_TO_BOTTOM, Pad(10), 5),
        .backgroundColor = HoverColor(Pal::Card, Rgba(21, 23, 27)),
        .cornerRadius = Radius(8),
        .border = {.color = HoverColor(Pal::StrokeSoft, Rgba(48, 52, 62)), .width = BorderAll(1)},
    }) {
        CLAY_AUTO_ID({
            .layout = Layout(Grow(), Fixed(13), CLAY_LEFT_TO_RIGHT, {}, 0, Align(CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER)),
        }) {
            Text("Model Split", Pal::Text, 9, 1, 11);
            Spacer();
            Text("8.4M Tok", Pal::TextFaint, 8, 0, 10);
        }
        CLAY_AUTO_ID({
            .layout = Layout(Grow(), Fixed(4), CLAY_LEFT_TO_RIGHT, {}, 2),
            .backgroundColor = Rgba(33, 35, 40),
            .cornerRadius = Radius(3),
        }) {
            for (const auto &row : kModelRows) {
                CLAY_AUTO_ID({
                    .layout = Layout(Percent(std::max(0.02f, row.percent)), Grow()),
                    .backgroundColor = row.color,
                    .cornerRadius = Radius(3),
                }) {}
            }
        }
        for (const auto &row : kModelRows) {
            CLAY_AUTO_ID({
                .layout = Layout(Grow(), Fixed(14), CLAY_LEFT_TO_RIGHT, {}, 6, Align(CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER)),
                .backgroundColor = HoverColor(Pal::Transparent, Rgba(27, 29, 35), true),
                .cornerRadius = Radius(4),
            }) {
                Dot(row.color, 5);
                Text(row.name, Pal::TextDim, 8, 0, 10);
                Spacer();
                Text(row.tokens, Pal::TextFaint, 8, 0, 10);
                CLAY_AUTO_ID({.layout = Layout(Fixed(31), Fixed(10), CLAY_LEFT_TO_RIGHT, {}, 0, Align(CLAY_ALIGN_X_RIGHT, CLAY_ALIGN_Y_CENTER))}) {
                    Text(row.share, Pal::Text, 8, 0, 10);
                }
            }
        }
    }
}

void ChangeRow(const char *agent, const char *change, const char *owner, const char *time, Clay_Color badge) {
    CLAY_AUTO_ID({
        .layout = Layout(Grow(), Fixed(30), CLAY_LEFT_TO_RIGHT, {}, 8, Align(CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER)),
        .backgroundColor = HoverColor(Pal::Transparent, Rgba(27, 29, 35), true),
        .cornerRadius = Radius(4),
    }) {
        CLAY_AUTO_ID({
            .layout = Layout(Grow(), Grow(), CLAY_TOP_TO_BOTTOM, {}, 1, Align(CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER)),
        }) {
            Text(agent, Pal::Text, 8, 1, 10);
            Text(change, Pal::TextDim, 8, 0, 10);
        }
        CLAY_AUTO_ID({
            .layout = Layout(Fixed(36), Grow(), CLAY_TOP_TO_BOTTOM, {}, 1, Align(CLAY_ALIGN_X_RIGHT, CLAY_ALIGN_Y_CENTER)),
        }) {
            Text(owner, Pal::Text, 8, 0, 10);
            Text(time, Pal::TextFaint, 8, 0, 10);
        }
        CLAY_AUTO_ID({
            .layout = Layout(Fixed(12), Fixed(18)),
            .backgroundColor = badge,
            .cornerRadius = Radius(4),
        }) {}
    }
}

void RecentChangesCard() {
    CLAY_AUTO_ID({
        .layout = Layout(Grow(), Grow(), CLAY_TOP_TO_BOTTOM, Pad(10), 5),
        .backgroundColor = HoverColor(Pal::Card, Rgba(21, 23, 27)),
        .cornerRadius = Radius(8),
        .border = {.color = HoverColor(Pal::StrokeSoft, Rgba(48, 52, 62)), .width = BorderAll(1)},
    }) {
        CLAY_AUTO_ID({
            .layout = Layout(Grow(), Fixed(13), CLAY_LEFT_TO_RIGHT, {}, 0, Align(CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER)),
        }) {
            Text("Recent Changes", Pal::Text, 9, 1, 11);
            Spacer();
            Text("Deploys & Config", Pal::TextFaint, 8, 0, 10);
        }
        ChangeRow("research-agent-v2", "v2.71 -> v2.72", "maya", "14:02", Pal::Cyan);
        ChangeRow("sales-assistant", "temp 0.4 -> 0.2", "liam", "11:30", Pal::Yellow);
        ChangeRow("data-pipeline", "auto-paused", "on-call", "09:15", Pal::Green);
        ChangeRow("code-reviewer", "baseline 0.87 -> 0.89", "maya", "07:48", Pal::Cyan);
    }
}

void BottomCards() {
    CLAY_AUTO_ID({
        .layout = Layout(Grow(), Grow(), CLAY_LEFT_TO_RIGHT, {}, 8),
    }) {
        TopToolsCard();
        ModelSplitCard();
        RecentChangesCard();
    }
}

struct ActivityItem {
    const char *name;
    const char *detail;
    const char *latency;
    const char *tokens;
    const char *pill;
    Clay_Color dot;
};

const std::array<ActivityItem, 13> kActivities{{
    {"research-agent-v2", "r_8a3f12 - 14:32:18", "1.4s", "12.4k tok", "", Pal::Green},
    {"support-prod", "r_8a3f1e - 14:32:15", "340ms", "2.1k tok", "", Pal::Green},
    {"data-pipeline", "r_8a3e12 - 14:32:11", "", "", "Retry 1/3", Pal::Yellow},
    {"code-reviewer", "r_8a3cd1 - 14:32:08", "892ms", "5.7k tok", "", Pal::Green},
    {"sales-assistant", "r_8a3f12 - 14:32:18", "1.1s", "8.0k tok", "", Pal::Green},
    {"qa-eval-bot", "r_8a3f12 - 14:32:18", "", "", "Tool Error", Pal::Red},
    {"research-agent-v2", "r_8a3f12 - 14:32:18", "1.4s", "18.9k tok", "", Pal::Green},
    {"support-prod", "r_8a3f12 - 14:32:18", "12.3s", "12.4k tok", "", Pal::Green},
    {"data-pipeline", "r_8a3f12 - 14:32:18", "410ms", "2.0k tok", "", Pal::Green},
    {"sales-assistant", "r_8a3f12 - 14:32:18", "720ms", "7.2k tok", "", Pal::Green},
    {"code-reviewer", "r_8a3f12 - 14:32:18", "1.4s", "12.4k tok", "", Pal::Green},
    {"research-agent-v2", "r_8a3f12 - 14:32:18", "6.1s", "42.8k tok", "", Pal::Green},
    {"support-prod", "r_8a3f12 - 14:32:18", "720ms", "2.0k tok", "", Pal::Green},
}};

void ActivityRow(const ActivityItem &item) {
    CLAY_AUTO_ID({
        .layout = Layout(Grow(), Fixed(31), CLAY_LEFT_TO_RIGHT, {}, 6, Align(CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER)),
        .backgroundColor = HoverColor(Pal::Transparent, Rgba(27, 29, 35), true),
        .cornerRadius = Radius(5),
    }) {
        CLAY_AUTO_ID({
            .layout = Layout(Fixed(6), Grow(), CLAY_LEFT_TO_RIGHT, {}, 0, Align(CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER)),
        }) {
            Dot(item.dot, 4);
        }
        CLAY_AUTO_ID({
            .layout = Layout(Grow(), Grow(), CLAY_TOP_TO_BOTTOM, {}, 0, Align(CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER)),
        }) {
            Text(item.name, Pal::Text, 8, 1, 10);
            Text(item.detail, Pal::TextFaint, 8, 0, 10);
        }
        CLAY_AUTO_ID({
            .layout = Layout(Fixed(52), Grow(), CLAY_TOP_TO_BOTTOM, {}, 0, Align(CLAY_ALIGN_X_RIGHT, CLAY_ALIGN_Y_CENTER)),
        }) {
            if (item.pill && item.pill[0] != '\0') {
                if (std::strcmp(item.pill, "Retry 1/3") == 0) {
                    Pill(item.pill, Rgba(78, 55, 17), Pal::Yellow, 50);
                } else {
                    Text(item.pill, Pal::Red, 8, 1, 10);
                }
            } else {
                Text(item.latency, Pal::Text, 8, 1, 10);
                Text(item.tokens, Pal::TextFaint, 8, 0, 10);
            }
        }
    }
}

void ActivityPanel() {
    CLAY_AUTO_ID({
        .layout = Layout(Fixed(187), Grow(), CLAY_TOP_TO_BOTTOM, Pad(10), 7),
        .backgroundColor = HoverColor(Pal::Card, Rgba(21, 23, 27)),
        .cornerRadius = Radius(8),
        .border = {.color = HoverColor(Pal::StrokeSoft, Rgba(48, 52, 62)), .width = BorderAll(1)},
    }) {
        CLAY_AUTO_ID({
            .layout = Layout(Grow(), Fixed(23), CLAY_LEFT_TO_RIGHT, {}, 8, Align(CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER)),
        }) {
            Dot(Pal::Green, 5);
            Text("Live Activity", Pal::Text, 9, 1, 11);
            Spacer();
            CLAY_AUTO_ID({
                .layout = Layout(Fixed(18), Fixed(18), CLAY_LEFT_TO_RIGHT, {}, 0, Align(CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER)),
                .backgroundColor = PressableColor(Rgba(30, 32, 37), Rgba(41, 44, 53), Rgba(24, 26, 32)),
                .cornerRadius = Radius(9),
            }) {
                Text("II", Pal::TextDim, 7, 1, 9);
            }
            CLAY_AUTO_ID({
                .layout = Layout(Fixed(45), Fixed(20), CLAY_LEFT_TO_RIGHT, PadLTRB(6, 6, 0, 0), 4, Align(CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER)),
                .backgroundColor = PressableColor(Rgba(30, 32, 37), Rgba(41, 44, 53), Rgba(24, 26, 32)),
                .cornerRadius = Radius(8),
                .border = {.color = HoverColor(Rgba(45, 47, 54), Rgba(67, 72, 84)), .width = BorderAll(1)},
            }) {
                Icon(kIconFilter, 10);
                Text("Filter", Pal::TextDim, 8, 0, 10);
            }
        }
        for (const auto &item : kActivities) {
            ActivityRow(item);
        }
    }
}

void Sidebar() {
    CLAY_AUTO_ID({
        .layout = Layout(Fixed(164), Grow(), CLAY_TOP_TO_BOTTOM, PadLTRB(0, 0, 2, 0), 5),
        .backgroundColor = Pal::Sidebar,
    }) {
        CLAY_AUTO_ID({
            .layout = Layout(Grow(), Fixed(25), CLAY_LEFT_TO_RIGHT, {}, 0, Align(CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER)),
        }) {
            Text("STEALTH", Pal::Text, 8, 2, 10);
            Spacer();
            Text("v2.7", Pal::TextFaint, 8, 0, 10);
        }
        CLAY_AUTO_ID({
            .layout = Layout(Grow(), Fixed(28), CLAY_LEFT_TO_RIGHT, PadXY(8, 0), 8, Align(CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER)),
            .backgroundColor = PressableColor(Pal::Card3, Rgba(38, 41, 49), Rgba(22, 24, 30)),
            .cornerRadius = Radius(6),
            .border = {.color = HoverColor(Rgba(47, 50, 59), Rgba(70, 74, 86)), .width = BorderAll(1)},
        }) {
            CLAY_AUTO_ID({
                .layout = Layout(Fixed(16), Fixed(16), CLAY_LEFT_TO_RIGHT, {}, 0, Align(CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER)),
                .backgroundColor = Rgba(31, 33, 39),
                .cornerRadius = Radius(5),
                .border = {.color = Rgba(65, 68, 78), .width = BorderAll(1)},
            }) {
                Text("A", Pal::Text, 9, 1, 11);
            }
            Text("Anthropic / prod", Pal::Text, 9, 0, 11);
            Spacer();
            Text("v", Pal::TextDim, 8, 1, 10);
        }
        NavSection("Monitor");
        NavItem(kIconOverview, "Overview", "", true);
        NavItem(kIconAgents, "Agents", "24");
        NavItem(kIconRuns, "Runs", "12.4k");
        NavItem(kIconTraces, "Traces", "");
        NavSection("Analyze");
        NavItem(kIconEvals, "Evals", "");
        NavItem(kIconCosts, "Costs", "");
        NavItem(kIconLogs, "Logs", "");
        NavSection("Operate");
        NavItem(kIconAlerts, "Alerts", "3", false, true);
        NavItem(kIconSettings, "Settings", "");
        Spacer();
        CLAY_AUTO_ID({
            .layout = Layout(Grow(), Fixed(38), CLAY_LEFT_TO_RIGHT, PadXY(8, 0), 8, Align(CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER)),
            .backgroundColor = PressableColor(Rgba(25, 27, 31), Rgba(34, 37, 44), Rgba(20, 22, 27)),
            .cornerRadius = Radius(7),
            .border = {.color = HoverColor(Rgba(34, 36, 42), Rgba(58, 62, 74)), .width = BorderAll(1)},
        }) {
            CustomBox(kAvatar, Fixed(16), Fixed(16), Pal::Transparent, 4);
            CLAY_AUTO_ID({
                .layout = Layout(Grow(), Grow(), CLAY_TOP_TO_BOTTOM, {}, 0, Align(CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER)),
            }) {
                Text("Maya Chen", Pal::Text, 8, 1, 10);
                Text("maya@anthr.com", Pal::TextFaint, 8, 0, 10);
            }
            Text("^", Pal::TextDim, 8, 1, 10);
        }
    }
}

Clay_RenderCommandArray BuildDashboard(float deltaTime) {
    gHandCursorRequested = false;
    Clay_BeginLayout();
    CLAY(CLAY_ID("Root"), {
        .layout = Layout(Grow(), Grow()),
        .backgroundColor = Pal::Page,
    }) {
        CLAY(CLAY_ID("DashboardShell"), {
            .layout = Layout(Grow(), Grow(), CLAY_LEFT_TO_RIGHT, Pad(14), 12),
            .backgroundColor = Pal::Shell,
            .cornerRadius = Radius(0),
            .clip = {.horizontal = true, .vertical = true},
        }) {
            Sidebar();
            CLAY_AUTO_ID({
                .layout = Layout(Grow(), Grow(), CLAY_TOP_TO_BOTTOM, {}, 0),
                .backgroundColor = Pal::Shell,
            }) {
                Header();
                CLAY_AUTO_ID({
                    .layout = Layout(Grow(), Grow(), CLAY_LEFT_TO_RIGHT, {}, 8),
                }) {
                    CLAY_AUTO_ID({
                        .layout = Layout(Grow(), Grow(), CLAY_TOP_TO_BOTTOM, {}, 8),
                    }) {
                        CLAY_AUTO_ID({
                            .layout = Layout(Grow(), Fixed(86), CLAY_LEFT_TO_RIGHT, {}, 8),
                        }) {
                            for (const auto &metric : kMetrics) {
                                MetricCard(metric);
                            }
                        }
                        MainChartCard();
                        BottomCards();
                    }
                    ActivityPanel();
                }
            }
        }
    }
    return Clay_EndLayout(deltaTime);
}

std::string ExistingPath(std::initializer_list<const char *> paths) {
    for (const char *path : paths) {
        if (FILE *file = std::fopen(path, "rb")) {
            std::fclose(file);
            return path;
        }
    }
    return {};
}

class FontBook {
public:
    FontBook() {
        regularPath_ = ExistingPath({
            "/usr/share/fonts/TTF/OpenSans-Regular.ttf",
            "/usr/share/fonts/Adwaita/AdwaitaSans-Regular.ttf",
            "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        });
        boldPath_ = ExistingPath({
            "/usr/share/fonts/TTF/OpenSans-Bold.ttf",
            "/usr/share/fonts/Adwaita/AdwaitaSans-Bold.ttf",
            "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
        });
        monoPath_ = ExistingPath({
            "/usr/share/fonts/Adwaita/AdwaitaMono-Regular.ttf",
            "/usr/share/fonts/TTF/DejaVuSansMono.ttf",
            "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf",
        });
        if (boldPath_.empty()) {
            boldPath_ = regularPath_;
        }
        if (monoPath_.empty()) {
            monoPath_ = regularPath_;
        }
    }

    ~FontBook() {
        Clear();
    }

    void Clear() {
        for (auto &[_, font] : fonts_) {
            if (font) {
                TTF_CloseFont(font);
            }
        }
        fonts_.clear();
    }

    TTF_Font *Get(uint16_t fontId, uint16_t size) {
        if (size == 0) {
            size = 12;
        }
        const uint32_t key = (static_cast<uint32_t>(fontId) << 16U) | size;
        auto found = fonts_.find(key);
        if (found != fonts_.end()) {
            return found->second;
        }

        const std::string &path = fontId == 1 ? boldPath_ : (fontId == 2 ? monoPath_ : regularPath_);
        if (path.empty()) {
            return nullptr;
        }
        TTF_Font *font = TTF_OpenFont(path.c_str(), size);
        if (!font && fontId != 0 && !regularPath_.empty()) {
            font = TTF_OpenFont(regularPath_.c_str(), size);
        }
        if (font) {
            TTF_SetFontHinting(font, TTF_HINTING_LIGHT_SUBPIXEL);
            TTF_SetFontKerning(font, 1);
        }
        fonts_[key] = font;
        return font;
    }

private:
    std::map<uint32_t, TTF_Font *> fonts_;
    std::string regularPath_;
    std::string boldPath_;
    std::string monoPath_;
};

Clay_Dimensions MeasureText(Clay_StringSlice text, Clay_TextElementConfig *config, void *userData) {
    auto *fonts = static_cast<FontBook *>(userData);
    const uint16_t fontId = config ? config->fontId : 0;
    const uint16_t fontSize = config ? config->fontSize : 12;
    TTF_Font *font = fonts->Get(fontId, fontSize);
    if (!font || !text.chars || text.length <= 0) {
        return Clay_Dimensions{0.0f, static_cast<float>(fontSize + 4)};
    }

    std::string value(text.chars, static_cast<size_t>(text.length));
    int width = 0;
    int height = 0;
    if (TTF_SizeUTF8(font, value.c_str(), &width, &height) != 0) {
        return Clay_Dimensions{0.0f, static_cast<float>(fontSize + 4)};
    }
    const float letterSpacing = config ? static_cast<float>(config->letterSpacing) * std::max(0, text.length - 1) : 0.0f;
    const float lineHeight = config && config->lineHeight > 0 ? static_cast<float>(config->lineHeight) : static_cast<float>(height);
    return Clay_Dimensions{static_cast<float>(width) + letterSpacing, lineHeight};
}

void ClayError(Clay_ErrorData error) {
    std::string message(error.errorText.chars, static_cast<size_t>(error.errorText.length));
    std::fprintf(stderr, "Clay error: %s\n", message.c_str());
}

SDL_Color ToSdl(Clay_Color color) {
    auto clamp = [](float value) -> Uint8 {
        return static_cast<Uint8>(std::clamp(value, 0.0f, 255.0f));
    };
    return SDL_Color{clamp(color.r), clamp(color.g), clamp(color.b), clamp(color.a)};
}

struct TextTextureKey {
    SDL_Renderer *renderer;
    std::string value;
    uint16_t fontId;
    uint16_t size;
    uint32_t color;

    bool operator<(const TextTextureKey &other) const {
        if (renderer != other.renderer) {
            return reinterpret_cast<uintptr_t>(renderer) < reinterpret_cast<uintptr_t>(other.renderer);
        }
        if (fontId != other.fontId) {
            return fontId < other.fontId;
        }
        if (size != other.size) {
            return size < other.size;
        }
        if (color != other.color) {
            return color < other.color;
        }
        return value < other.value;
    }
};

struct TextTexture {
    SDL_Texture *texture = nullptr;
    int width = 0;
    int height = 0;
};

std::map<TextTextureKey, TextTexture> gTextTextureCache;

uint32_t PackedColor(SDL_Color color) {
    return (static_cast<uint32_t>(color.r) << 24U)
        | (static_cast<uint32_t>(color.g) << 16U)
        | (static_cast<uint32_t>(color.b) << 8U)
        | static_cast<uint32_t>(color.a);
}

void ClearTextTextureCacheForRenderer(SDL_Renderer *renderer) {
    for (auto it = gTextTextureCache.begin(); it != gTextTextureCache.end();) {
        if (it->first.renderer == renderer) {
            if (it->second.texture) {
                SDL_DestroyTexture(it->second.texture);
            }
            it = gTextTextureCache.erase(it);
        } else {
            ++it;
        }
    }
}

void ClearTextTextureCache() {
    for (auto &[_, cached] : gTextTextureCache) {
        if (cached.texture) {
            SDL_DestroyTexture(cached.texture);
        }
    }
    gTextTextureCache.clear();
}

SDL_Rect Rect(Clay_BoundingBox box) {
    return SDL_Rect{
        static_cast<int>(std::round(box.x)),
        static_cast<int>(std::round(box.y)),
        static_cast<int>(std::round(box.width)),
        static_cast<int>(std::round(box.height)),
    };
}

void SetColor(SDL_Renderer *renderer, Clay_Color color) {
    SDL_Color c = ToSdl(color);
    SDL_SetRenderDrawColor(renderer, c.r, c.g, c.b, c.a);
}

Clay_Color WithAlpha(Clay_Color color, float alphaMultiplier) {
    color.a = std::clamp(color.a * alphaMultiplier, 0.0f, 255.0f);
    return color;
}

void FillCircle(SDL_Renderer *renderer, int centerX, int centerY, int radius, Clay_Color color) {
    if (radius <= 0 || color.a <= 0.0f) {
        return;
    }
    SetColor(renderer, color);
    for (int y = -radius; y <= radius; ++y) {
        const int span = static_cast<int>(std::sqrt(static_cast<float>(radius * radius - y * y)));
        SDL_RenderDrawLine(renderer, centerX - span, centerY + y, centerX + span, centerY + y);
    }
}

void DrawCircleOutline(SDL_Renderer *renderer, int centerX, int centerY, int radius, Clay_Color color) {
    SetColor(renderer, color);
    for (int i = 0; i < 360; i += 5) {
        const float a = static_cast<float>(i) * 3.14159265f / 180.0f;
        const int x = centerX + static_cast<int>(std::cos(a) * radius);
        const int y = centerY + static_cast<int>(std::sin(a) * radius);
        SDL_RenderDrawPoint(renderer, x, y);
    }
}

void FillRoundedRect(SDL_Renderer *renderer, Clay_BoundingBox box, Clay_Color color, float radius) {
    if (box.width <= 0.0f || box.height <= 0.0f || color.a <= 0.0f) {
        return;
    }
    const int x = static_cast<int>(std::round(box.x));
    const int y = static_cast<int>(std::round(box.y));
    const int w = static_cast<int>(std::round(box.width));
    const int h = static_cast<int>(std::round(box.height));
    const int r = std::max(0, std::min(static_cast<int>(std::round(radius)), std::min(w, h) / 2));

    SetColor(renderer, color);
    if (r == 0) {
        SDL_Rect rect{x, y, w, h};
        SDL_RenderFillRect(renderer, &rect);
        return;
    }

    SDL_Rect center{x + r, y, w - (2 * r), h};
    SDL_Rect left{x, y + r, r, h - (2 * r)};
    SDL_Rect right{x + w - r, y + r, r, h - (2 * r)};
    SDL_RenderFillRect(renderer, &center);
    SDL_RenderFillRect(renderer, &left);
    SDL_RenderFillRect(renderer, &right);

    for (int dy = 0; dy < r; ++dy) {
        const float cornerY = static_cast<float>(r - dy) - 0.5f;
        const float span = std::sqrt(std::max(0.0f, static_cast<float>(r * r) - cornerY * cornerY));
        const int fullSpan = static_cast<int>(std::floor(span));
        const float edgeAlpha = std::clamp(span - static_cast<float>(fullSpan), 0.0f, 1.0f);
        const int topY = y + dy;
        const int bottomY = y + h - dy - 1;
        const int leftStart = x + r - fullSpan;
        const int rightEnd = x + w - r + fullSpan - 1;

        SetColor(renderer, color);
        SDL_RenderDrawLine(renderer, leftStart, topY, rightEnd, topY);
        SDL_RenderDrawLine(renderer, leftStart, bottomY, rightEnd, bottomY);

        if (edgeAlpha > 0.01f) {
            SetColor(renderer, WithAlpha(color, edgeAlpha));
            SDL_RenderDrawPoint(renderer, leftStart - 1, topY);
            SDL_RenderDrawPoint(renderer, rightEnd + 1, topY);
            SDL_RenderDrawPoint(renderer, leftStart - 1, bottomY);
            SDL_RenderDrawPoint(renderer, rightEnd + 1, bottomY);
        }
    }
}

void DrawBorder(SDL_Renderer *renderer, Clay_BoundingBox box, Clay_BorderRenderData border) {
    if (border.color.a <= 0.0f) {
        return;
    }
    const int maxWidth = std::max({border.width.left, border.width.right, border.width.top, border.width.bottom});
    if (maxWidth <= 0) {
        return;
    }
    const float radius = std::max(0.0f, border.cornerRadius.topLeft);
    SetColor(renderer, border.color);
    if (radius > 0.0f) {
        const int x = static_cast<int>(std::round(box.x));
        const int y = static_cast<int>(std::round(box.y));
        const int w = static_cast<int>(std::round(box.width));
        const int h = static_cast<int>(std::round(box.height));
        const int r = std::min(static_cast<int>(std::round(radius)), std::min(w, h) / 2);
        for (int inset = 0; inset < maxWidth; ++inset) {
            const int rr = std::max(0, r - inset);
            const int left = x + inset;
            const int right = x + w - inset - 1;
            const int top = y + inset;
            const int bottom = y + h - inset - 1;
            SDL_RenderDrawLine(renderer, left + rr, top, right - rr, top);
            SDL_RenderDrawLine(renderer, left + rr, bottom, right - rr, bottom);
            SDL_RenderDrawLine(renderer, left, top + rr, left, bottom - rr);
            SDL_RenderDrawLine(renderer, right, top + rr, right, bottom - rr);
            for (int deg = 0; deg <= 90; deg += 6) {
                const float a = static_cast<float>(deg) * 3.14159265f / 180.0f;
                const int px = static_cast<int>(std::round(std::cos(a) * rr));
                const int py = static_cast<int>(std::round(std::sin(a) * rr));
                if (rr > 0) {
                    SDL_RenderDrawPoint(renderer, left + rr - px, top + rr - py);
                    SDL_RenderDrawPoint(renderer, right - rr + px, top + rr - py);
                    SDL_RenderDrawPoint(renderer, left + rr - px, bottom - rr + py);
                    SDL_RenderDrawPoint(renderer, right - rr + px, bottom - rr + py);
                }
            }
        }
        return;
    }
    const SDL_Rect rect = Rect(box);
    for (int i = 0; i < maxWidth; ++i) {
        SDL_Rect inset{rect.x + i, rect.y + i, std::max(0, rect.w - 2 * i), std::max(0, rect.h - 2 * i)};
        SDL_RenderDrawRect(renderer, &inset);
    }
}

float FractionalPart(float value) {
    return value - std::floor(value);
}

float ReverseFractionalPart(float value) {
    return 1.0f - FractionalPart(value);
}

void PlotAALinePoint(SDL_Renderer *renderer, bool steep, int x, int y, Clay_Color color, float coverage) {
    coverage = std::clamp(coverage, 0.0f, 1.0f);
    if (coverage <= 0.01f || color.a <= 0.0f) {
        return;
    }
    SetColor(renderer, WithAlpha(color, coverage));
    if (steep) {
        SDL_RenderDrawPoint(renderer, y, x);
    } else {
        SDL_RenderDrawPoint(renderer, x, y);
    }
}

void DrawLineAA(SDL_Renderer *renderer, float x0, float y0, float x1, float y1, Clay_Color color) {
    bool steep = std::fabs(y1 - y0) > std::fabs(x1 - x0);
    if (steep) {
        std::swap(x0, y0);
        std::swap(x1, y1);
    }
    if (x0 > x1) {
        std::swap(x0, x1);
        std::swap(y0, y1);
    }

    const float dx = x1 - x0;
    const float dy = y1 - y0;
    if (std::fabs(dx) < 0.001f) {
        PlotAALinePoint(renderer, steep, static_cast<int>(std::round(x0)), static_cast<int>(std::round(y0)), color, 1.0f);
        return;
    }
    const float gradient = dy / dx;

    float xEnd = std::round(x0);
    float yEnd = y0 + gradient * (xEnd - x0);
    float xGap = ReverseFractionalPart(x0 + 0.5f);
    int xPixel1 = static_cast<int>(xEnd);
    int yPixel1 = static_cast<int>(std::floor(yEnd));
    PlotAALinePoint(renderer, steep, xPixel1, yPixel1, color, ReverseFractionalPart(yEnd) * xGap);
    PlotAALinePoint(renderer, steep, xPixel1, yPixel1 + 1, color, FractionalPart(yEnd) * xGap);
    float intery = yEnd + gradient;

    xEnd = std::round(x1);
    yEnd = y1 + gradient * (xEnd - x1);
    xGap = FractionalPart(x1 + 0.5f);
    int xPixel2 = static_cast<int>(xEnd);
    int yPixel2 = static_cast<int>(std::floor(yEnd));
    PlotAALinePoint(renderer, steep, xPixel2, yPixel2, color, ReverseFractionalPart(yEnd) * xGap);
    PlotAALinePoint(renderer, steep, xPixel2, yPixel2 + 1, color, FractionalPart(yEnd) * xGap);

    for (int x = xPixel1 + 1; x <= xPixel2 - 1; ++x) {
        const int y = static_cast<int>(std::floor(intery));
        PlotAALinePoint(renderer, steep, x, y, color, ReverseFractionalPart(intery));
        PlotAALinePoint(renderer, steep, x, y + 1, color, FractionalPart(intery));
        intery += gradient;
    }
}

void DrawLineThick(SDL_Renderer *renderer, float x1, float y1, float x2, float y2, Clay_Color color, int thickness = 1) {
    thickness = ScalePixels(thickness);
    if (thickness <= 1) {
        DrawLineAA(renderer, x1, y1, x2, y2, color);
        return;
    }
    const float dx = x2 - x1;
    const float dy = y2 - y1;
    const float length = std::sqrt(dx * dx + dy * dy);
    if (length <= 0.001f) {
        FillCircle(renderer, static_cast<int>(std::round(x1)), static_cast<int>(std::round(y1)), std::max(1, thickness / 2), color);
        return;
    }
    const float nx = -dy / length;
    const float ny = dx / length;
    const float center = (static_cast<float>(thickness) - 1.0f) * 0.5f;
    for (int i = 0; i < thickness; ++i) {
        const float offset = static_cast<float>(i) - center;
        DrawLineAA(renderer, x1 + nx * offset, y1 + ny * offset, x2 + nx * offset, y2 + ny * offset, color);
    }
}

void DrawTextRaw(SDL_Renderer *renderer, FontBook &fonts, const std::string &value, float x, float y, Clay_Color color, uint16_t size, uint16_t fontId = 0, bool copyAlpha = false) {
    if (value.empty() || color.a <= 0.0f) {
        return;
    }
    TTF_Font *font = fonts.Get(fontId, size);
    if (!font) {
        return;
    }
    SDL_Color sdlColor = ToSdl(color);
    TextTextureKey key{renderer, value, fontId, size, PackedColor(sdlColor)};
    auto found = gTextTextureCache.find(key);
    if (found == gTextTextureCache.end()) {
        SDL_Surface *surface = TTF_RenderUTF8_Blended(font, value.c_str(), sdlColor);
        if (!surface) {
            return;
        }
        SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
        if (!texture) {
            SDL_FreeSurface(surface);
            return;
        }
        TextTexture cached{texture, surface->w, surface->h};
        SDL_FreeSurface(surface);
        found = gTextTextureCache.emplace(std::move(key), cached).first;
    }
    SDL_SetTextureBlendMode(found->second.texture, copyAlpha ? SDL_BLENDMODE_NONE : SDL_BLENDMODE_BLEND);
    SDL_Rect dst{static_cast<int>(std::round(x)), static_cast<int>(std::round(y)), found->second.width, found->second.height};
    SDL_RenderCopy(renderer, found->second.texture, nullptr, &dst);
}

void DrawTextCommand(SDL_Renderer *renderer, FontBook &fonts, Clay_RenderCommand *command) {
    const Clay_TextRenderData &data = command->renderData.text;
    if (!data.stringContents.chars || data.stringContents.length <= 0) {
        return;
    }
    std::string value(data.stringContents.chars, static_cast<size_t>(data.stringContents.length));
    TTF_Font *font = fonts.Get(data.fontId, data.fontSize);
    if (!font) {
        return;
    }
    int textWidth = 0;
    int textHeight = 0;
    TTF_SizeUTF8(font, value.c_str(), &textWidth, &textHeight);
    const float y = command->boundingBox.y + std::max(0.0f, (command->boundingBox.height - static_cast<float>(textHeight)) * 0.5f);
    DrawTextRaw(renderer, fonts, value, command->boundingBox.x, y, data.textColor, data.fontSize, data.fontId);
}

void DrawTextCommandScaled(SDL_Renderer *renderer, FontBook &fonts, Clay_RenderCommand *command, float inverseScale) {
    const Clay_TextRenderData &data = command->renderData.text;
    if (!data.stringContents.chars || data.stringContents.length <= 0) {
        return;
    }
    std::string value(data.stringContents.chars, static_cast<size_t>(data.stringContents.length));
    const uint16_t fontSize = static_cast<uint16_t>(std::max(1.0f, std::round(static_cast<float>(data.fontSize) * inverseScale)));
    TTF_Font *font = fonts.Get(data.fontId, fontSize);
    if (!font) {
        return;
    }

    int textWidth = 0;
    int textHeight = 0;
    TTF_SizeUTF8(font, value.c_str(), &textWidth, &textHeight);
    const float x = command->boundingBox.x * inverseScale;
    const float boxY = command->boundingBox.y * inverseScale;
    const float boxHeight = command->boundingBox.height * inverseScale;
    const float y = boxY + std::max(0.0f, (boxHeight - static_cast<float>(textHeight)) * 0.5f);
    DrawTextRaw(renderer, fonts, value, x, y, data.textColor, fontSize, data.fontId, true);
}

float Fract(float value) {
    return value - std::floor(value);
}

float Noise(int index, int salt) {
    return Fract(std::sin(static_cast<float>(index * 12 + salt * 47)) * 43758.5453f);
}

void DrawSparkline(SDL_Renderer *renderer, Clay_BoundingBox box, const CustomSpec &spec, float seconds) {
    const int count = 28;
    std::array<float, count> values{};
    for (int i = 0; i < count; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(count - 1);
        float value = 0.45f + 0.18f * std::sin(t * 8.0f + seconds * 0.6f) + 0.10f * std::sin(t * 23.0f + static_cast<float>(spec.variant));
        if (spec.variant == 0) {
            value += 0.32f * std::exp(-std::pow((t - 0.72f) * 4.2f, 2.0f));
        } else {
            value += 0.20f * std::exp(-std::pow((t - 0.28f) * 5.0f, 2.0f)) - 0.24f * t;
        }
        values[i] = std::clamp(value, 0.08f, 0.94f);
    }

    const float left = box.x + Scale(2.0f);
    const float top = box.y + Scale(3.0f);
    const float width = std::max(1.0f, box.width - Scale(4.0f));
    const float height = std::max(1.0f, box.height - Scale(6.0f));
    for (int i = 0; i < count - 1; ++i) {
        const float x1 = left + (static_cast<float>(i) / static_cast<float>(count - 1)) * width;
        const float x2 = left + (static_cast<float>(i + 1) / static_cast<float>(count - 1)) * width;
        const float y1 = top + (1.0f - values[i]) * height;
        const float y2 = top + (1.0f - values[i + 1]) * height;
        DrawLineThick(renderer, x1, y1, x2, y2, spec.color, 1);
        DrawLineThick(renderer, x1, y1 + 1.0f, x1, top + height, spec.altColor, 1);
    }
}

void DrawDashedH(SDL_Renderer *renderer, float x1, float x2, float y, Clay_Color color) {
    SetColor(renderer, color);
    for (float x = x1; x < x2; x += Scale(8.0f)) {
        SDL_RenderDrawLine(renderer, static_cast<int>(x), static_cast<int>(y), static_cast<int>(std::min(x + Scale(4.0f), x2)), static_cast<int>(y));
    }
}

void DrawMainChart(SDL_Renderer *renderer, FontBook &fonts, Clay_BoundingBox box) {
    const float left = box.x + Scale(29.0f);
    const float right = box.x + box.width - Scale(8.0f);
    const float top = box.y + Scale(8.0f);
    const float bottom = box.y + box.height - Scale(24.0f);
    const float width = right - left;
    const float height = bottom - top;

    const Clay_Color grid = Rgba(45, 47, 55, 112);
    for (int yv = 0; yv <= 80; yv += 20) {
        const float y = bottom - (static_cast<float>(yv) / 80.0f) * height;
        DrawDashedH(renderer, left, right, y, grid);
        DrawTextRaw(renderer, fonts, std::to_string(yv), box.x + Scale(8.0f), y - Scale(7.0f), Rgba(83, 87, 97), ScaleFont(8), 0);
    }
    for (int i = 0; i <= 6; ++i) {
        const float x = left + (static_cast<float>(i) / 6.0f) * width;
        SetColor(renderer, Rgba(36, 38, 44, 86));
        SDL_RenderDrawLine(renderer, static_cast<int>(x), static_cast<int>(top), static_cast<int>(x), static_cast<int>(bottom));
    }

    const std::array<const char *, 7> labels{{"00:00", "04:00", "08:00", "12:00", "16:00", "20:00", "24:00"}};
    for (int i = 0; i < static_cast<int>(labels.size()); ++i) {
        const float x = left + (static_cast<float>(i) / static_cast<float>(labels.size() - 1)) * width - Scale(10.0f);
        DrawTextRaw(renderer, fonts, labels[static_cast<size_t>(i)], x, bottom + Scale(6.0f), Rgba(72, 76, 86), ScaleFont(8), 0);
    }

    auto chartY = [&](float value) {
        return bottom - (std::clamp(value, 0.0f, 80.0f) / 80.0f) * height;
    };
    auto drawSeries = [&](Clay_Color color, int salt, float base, float amp, float drift, int thickness) {
        float prevX = left;
        float prevY = chartY(base);
        for (int i = 1; i < 128; ++i) {
            const float t = static_cast<float>(i) / 127.0f;
            float value = base
                + amp * std::sin(t * 4.2f + static_cast<float>(salt))
                + (amp * 0.45f) * std::sin(t * 17.0f + static_cast<float>(salt) * 0.7f)
                + (Noise(i, salt) - 0.5f) * 7.0f
                + drift * t;
            if (salt == 1) {
                value += 14.0f * std::exp(-std::pow((t - 0.55f) * 5.4f, 2.0f));
                value -= 20.0f * std::exp(-std::pow((t - 0.88f) * 10.0f, 2.0f));
            }
            if (salt == 2) {
                value += 8.0f * std::exp(-std::pow((t - 0.85f) * 6.0f, 2.0f));
            }
            const float x = left + t * width;
            const float y = chartY(value);
            DrawLineThick(renderer, prevX, prevY, x, y, color, thickness);
            prevX = x;
            prevY = y;
        }
    };

    drawSeries(Pal::Green, 1, 37.0f, 9.0f, 23.0f, 2);
    drawSeries(Pal::Yellow, 2, 28.0f, 4.5f, 2.0f, 2);
    drawSeries(Pal::Red, 3, 12.0f, 3.8f, -1.0f, 2);

    const float crossX = left + width * 0.225f;
    SetColor(renderer, Rgba(70, 73, 82, 140));
    for (float y = top; y < bottom; y += Scale(6.0f)) {
        SDL_RenderDrawLine(renderer, static_cast<int>(crossX), static_cast<int>(y), static_cast<int>(crossX), static_cast<int>(y + Scale(3.0f)));
    }
    const float crossY = chartY(50.0f);
    DrawDashedH(renderer, left, right, crossY, Rgba(70, 73, 82, 112));
    FillCircle(renderer, static_cast<int>(crossX), static_cast<int>(crossY), ScalePixels(4), Rgba(21, 22, 25));
    FillCircle(renderer, static_cast<int>(crossX), static_cast<int>(crossY), ScalePixels(2), Pal::Green);
    FillCircle(renderer, static_cast<int>(crossX), static_cast<int>(chartY(27.0f)), ScalePixels(4), Rgba(21, 22, 25));
    FillCircle(renderer, static_cast<int>(crossX), static_cast<int>(chartY(27.0f)), ScalePixels(2), Pal::Yellow);
    FillCircle(renderer, static_cast<int>(crossX), static_cast<int>(chartY(8.0f)), ScalePixels(4), Rgba(21, 22, 25));
    FillCircle(renderer, static_cast<int>(crossX), static_cast<int>(chartY(8.0f)), ScalePixels(2), Pal::Red);
}

void DrawIcon(SDL_Renderer *renderer, FontBook &fonts, Clay_BoundingBox box, const CustomSpec &spec) {
    const float x = box.x;
    const float y = box.y;
    const float w = box.width;
    const float h = box.height;
    const float cx = x + w * 0.5f;
    const float cy = y + h * 0.5f;
    const float unit = std::max(1.0f, std::min(w, h) / 12.0f);
    auto U = [unit](float value) { return value * unit; };
    const Clay_Color color = spec.color;
    SetColor(renderer, color);
    switch (spec.variant) {
        case 0: {
            DrawCircleOutline(renderer, static_cast<int>(cx - U(1.0f)), static_cast<int>(cy - U(1.0f)), static_cast<int>(U(4.0f)), color);
            DrawLineThick(renderer, cx + U(2.0f), cy + U(2.0f), cx + U(5.0f), cy + U(5.0f), color, 1);
            break;
        }
        case 1: {
            DrawLineThick(renderer, cx - U(3.0f), cy, cx + U(3.0f), cy, color, 2);
            DrawLineThick(renderer, cx, cy - U(3.0f), cx, cy + U(3.0f), color, 2);
            break;
        }
        case 2: {
            DrawCircleOutline(renderer, static_cast<int>(cx), static_cast<int>(cy), static_cast<int>(U(4.0f)), color);
            FillCircle(renderer, static_cast<int>(cx), static_cast<int>(cy), static_cast<int>(U(2.0f)), color);
            break;
        }
        case 3: {
            SDL_Rect a{static_cast<int>(x + U(2)), static_cast<int>(y + U(3)), static_cast<int>(U(4)), static_cast<int>(U(4))};
            SDL_Rect b{static_cast<int>(x + U(7)), static_cast<int>(y + U(3)), static_cast<int>(U(4)), static_cast<int>(U(4))};
            SDL_Rect c{static_cast<int>(x + U(2)), static_cast<int>(y + U(8)), static_cast<int>(U(9)), static_cast<int>(U(3))};
            SDL_RenderDrawRect(renderer, &a);
            SDL_RenderDrawRect(renderer, &b);
            SDL_RenderDrawRect(renderer, &c);
            break;
        }
        case 4: {
            DrawLineThick(renderer, x + U(3), y + U(3), x + U(9), cy, color, 1);
            DrawLineThick(renderer, x + U(9), cy, x + U(3), y + h - U(3), color, 1);
            break;
        }
        case 5: {
            DrawCircleOutline(renderer, static_cast<int>(cx), static_cast<int>(cy), static_cast<int>(U(5)), color);
            DrawLineThick(renderer, cx, cy, cx, cy - U(3), color, 1);
            DrawLineThick(renderer, cx, cy, cx + U(3), cy + U(2), color, 1);
            break;
        }
        case 6: {
            for (int i = 0; i < 3; ++i) {
                const float yy = y + U(3.0f + static_cast<float>(i) * 4.0f);
                DrawLineThick(renderer, x + U(4), yy, x + U(11), yy, color, 1);
                FillCircle(renderer, static_cast<int>(x + U(2)), static_cast<int>(yy), static_cast<int>(U(1)), color);
            }
            break;
        }
        case 7: {
            DrawTextRaw(renderer, fonts, "$", x + Scale(3.0f), y - Scale(1.0f), color, ScaleFont(11), 1);
            break;
        }
        case 8: {
            for (int i = 0; i < 3; ++i) {
                const float yy = y + U(3.0f + static_cast<float>(i) * 4.0f);
                DrawLineThick(renderer, x + U(3), yy, x + U(11), yy, color, 1);
            }
            break;
        }
        case 9: {
            DrawLineThick(renderer, cx, y + U(2), x + U(10), y + U(10), color, 1);
            DrawLineThick(renderer, x + U(10), y + U(10), x + U(3), y + U(10), color, 1);
            DrawLineThick(renderer, x + U(3), y + U(10), cx, y + U(2), color, 1);
            break;
        }
        case 10: {
            DrawCircleOutline(renderer, static_cast<int>(cx), static_cast<int>(cy), static_cast<int>(U(4)), color);
            FillCircle(renderer, static_cast<int>(cx), static_cast<int>(cy), static_cast<int>(U(1)), color);
            DrawLineThick(renderer, cx, y + U(1), cx, y + U(3), color, 1);
            DrawLineThick(renderer, cx, y + h - U(3), cx, y + h - U(1), color, 1);
            DrawLineThick(renderer, x + U(1), cy, x + U(3), cy, color, 1);
            DrawLineThick(renderer, x + w - U(3), cy, x + w - U(1), cy, color, 1);
            break;
        }
        case 11: {
            DrawLineThick(renderer, x + U(3), y + U(3), x + w - U(3), y + U(3), color, 1);
            DrawLineThick(renderer, x + U(5), y + U(5), cx, y + U(8), color, 1);
            DrawLineThick(renderer, x + w - U(5), y + U(5), cx, y + U(8), color, 1);
            DrawLineThick(renderer, cx, y + U(8), cx, y + h - U(3), color, 1);
            break;
        }
        default:
            break;
    }
}

void DrawCustom(SDL_Renderer *renderer, FontBook &fonts, Clay_RenderCommand *command, float seconds) {
    auto *spec = static_cast<CustomSpec *>(command->renderData.custom.customData);
    if (!spec) {
        return;
    }
    const Clay_BoundingBox box = command->boundingBox;
    if (command->renderData.custom.backgroundColor.a > 0.0f) {
        FillRoundedRect(renderer, box, command->renderData.custom.backgroundColor, command->renderData.custom.cornerRadius.topLeft);
    }
    switch (spec->kind) {
        case CustomKind::Icon:
            DrawIcon(renderer, fonts, box, *spec);
            break;
        case CustomKind::Sparkline:
            DrawSparkline(renderer, box, *spec, seconds);
            break;
        case CustomKind::MainChart:
            DrawMainChart(renderer, fonts, box);
            break;
        case CustomKind::Avatar:
            FillRoundedRect(renderer, box, Rgba(35, 187, 199), 4);
            FillCircle(renderer, static_cast<int>(box.x + box.width - 4), static_cast<int>(box.y + 5), 5, Rgba(100, 84, 255, 180));
            DrawTextRaw(renderer, fonts, "M", box.x + Scale(4.0f), box.y + Scale(1.0f), Rgba(240, 255, 255), ScaleFont(9), 1);
            break;
        case CustomKind::MenuDots:
            for (int i = 0; i < 3; ++i) {
                FillCircle(renderer, static_cast<int>(box.x + 2 + i * 4), static_cast<int>(box.y + box.height * 0.5f), 1, spec->color);
            }
            break;
    }
}

Clay_BoundingBox ScaledBox(Clay_BoundingBox box, float inverseScale) {
    return Clay_BoundingBox{
        box.x * inverseScale,
        box.y * inverseScale,
        box.width * inverseScale,
        box.height * inverseScale,
    };
}

void RenderClay(SDL_Renderer *renderer, FontBook &fonts, Clay_RenderCommandArray commands, float seconds, bool drawText) {
    std::vector<SDL_Rect> clipStack;
    for (int32_t i = 0; i < commands.length; ++i) {
        Clay_RenderCommand *command = Clay_RenderCommandArray_Get(&commands, i);
        if (!command) {
            continue;
        }
        switch (command->commandType) {
            case CLAY_RENDER_COMMAND_TYPE_RECTANGLE:
                FillRoundedRect(renderer, command->boundingBox, command->renderData.rectangle.backgroundColor, command->renderData.rectangle.cornerRadius.topLeft);
                break;
            case CLAY_RENDER_COMMAND_TYPE_BORDER:
                DrawBorder(renderer, command->boundingBox, command->renderData.border);
                break;
            case CLAY_RENDER_COMMAND_TYPE_TEXT:
                if (drawText) {
                    DrawTextCommand(renderer, fonts, command);
                }
                break;
            case CLAY_RENDER_COMMAND_TYPE_CUSTOM:
                DrawCustom(renderer, fonts, command, seconds);
                break;
            case CLAY_RENDER_COMMAND_TYPE_SCISSOR_START: {
                SDL_Rect current{};
                const SDL_bool clipEnabled = SDL_RenderIsClipEnabled(renderer);
                SDL_RenderGetClipRect(renderer, &current);
                clipStack.push_back(clipEnabled ? current : SDL_Rect{0, 0, 0, 0});
                SDL_Rect clip = Rect(command->boundingBox);
                SDL_RenderSetClipRect(renderer, &clip);
                break;
            }
            case CLAY_RENDER_COMMAND_TYPE_SCISSOR_END: {
                if (!clipStack.empty()) {
                    SDL_Rect previous = clipStack.back();
                    clipStack.pop_back();
                    if (previous.w == 0 && previous.h == 0) {
                        SDL_RenderSetClipRect(renderer, nullptr);
                    } else {
                        SDL_RenderSetClipRect(renderer, &previous);
                    }
                } else {
                    SDL_RenderSetClipRect(renderer, nullptr);
                }
                break;
            }
            case CLAY_RENDER_COMMAND_TYPE_IMAGE:
            case CLAY_RENDER_COMMAND_TYPE_OVERLAY_COLOR_START:
            case CLAY_RENDER_COMMAND_TYPE_OVERLAY_COLOR_END:
            case CLAY_RENDER_COMMAND_TYPE_NONE:
                break;
        }
    }
}

void RenderClayTextOverlay(SDL_Renderer *renderer, FontBook &fonts, Clay_RenderCommandArray commands, float inverseScale) {
    std::vector<SDL_Rect> clipStack;
    for (int32_t i = 0; i < commands.length; ++i) {
        Clay_RenderCommand *command = Clay_RenderCommandArray_Get(&commands, i);
        if (!command) {
            continue;
        }
        switch (command->commandType) {
            case CLAY_RENDER_COMMAND_TYPE_TEXT:
                DrawTextCommandScaled(renderer, fonts, command, inverseScale);
                break;
            case CLAY_RENDER_COMMAND_TYPE_SCISSOR_START: {
                SDL_Rect current{};
                const SDL_bool clipEnabled = SDL_RenderIsClipEnabled(renderer);
                SDL_RenderGetClipRect(renderer, &current);
                clipStack.push_back(clipEnabled ? current : SDL_Rect{0, 0, 0, 0});
                SDL_Rect clip = Rect(ScaledBox(command->boundingBox, inverseScale));
                SDL_RenderSetClipRect(renderer, &clip);
                break;
            }
            case CLAY_RENDER_COMMAND_TYPE_SCISSOR_END:
                if (!clipStack.empty()) {
                    SDL_Rect previous = clipStack.back();
                    clipStack.pop_back();
                    if (previous.w == 0 && previous.h == 0) {
                        SDL_RenderSetClipRect(renderer, nullptr);
                    } else {
                        SDL_RenderSetClipRect(renderer, &previous);
                    }
                } else {
                    SDL_RenderSetClipRect(renderer, nullptr);
                }
                break;
            default:
                break;
        }
    }
}

} // namespace

int main() {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
        std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }
    if (TTF_Init() != 0) {
        std::fprintf(stderr, "TTF_Init failed: %s\n", TTF_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_ACCELERATED_VISUAL, 1);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 4);

    SDL_Window *window = SDL_CreateWindow(
        "Clay Dashboard",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        kInitialWidth,
        kInitialHeight,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_OPENGL | SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_MAXIMIZED);
    if (!window) {
        std::fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        TTF_Quit();
        SDL_Quit();
        return 1;
    }
    SDL_SetWindowMinimumSize(window, 980, 620);

    SDL_GLContext glContext = SDL_GL_CreateContext(window);
    if (!glContext) {
        SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 0);
        SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 0);
        glContext = SDL_GL_CreateContext(window);
    }
    if (!glContext) {
        std::fprintf(stderr, "SDL_GL_CreateContext failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        TTF_Quit();
        SDL_Quit();
        return 1;
    }
    SDL_GL_MakeCurrent(window, glContext);
    SDL_GL_SetSwapInterval(1);

    dashboard::SoftwareRenderBuffer renderBuffer;
    dashboard::SoftwareRenderBuffer textBuffer;
    dashboard::PostProcessPipeline postProcess;
    if (!postProcess.Load(DASHBOARD_SHADER_DIR)) {
        SDL_GL_DeleteContext(glContext);
        SDL_DestroyWindow(window);
        TTF_Quit();
        SDL_Quit();
        return 1;
    }

    SDL_Cursor *arrowCursor = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_ARROW);
    SDL_Cursor *handCursor = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_HAND);
    if (arrowCursor) {
        SDL_SetCursor(arrowCursor);
    }

    FontBook fonts;
    const uint32_t clayMemorySize = Clay_MinMemorySize();
    void *clayMemory = std::malloc(clayMemorySize);
    Clay_Arena arena = Clay_CreateArenaWithCapacityAndMemory(clayMemorySize, clayMemory);
    Clay_ErrorHandler errorHandler{ClayError, nullptr};
    Clay_Initialize(arena, Clay_Dimensions{static_cast<float>(kInitialWidth), static_cast<float>(kInitialHeight)}, errorHandler);
    Clay_SetMeasureTextFunction(MeasureText, &fonts);
    Clay_SetCullingEnabled(false);

    bool running = true;
    bool fullscreen = false;
    uint64_t lastCounter = SDL_GetPerformanceCounter();
    float totalSeconds = 0.0f;
    while (running) {
        const uint64_t counter = SDL_GetPerformanceCounter();
        const float deltaTime = static_cast<float>(static_cast<double>(counter - lastCounter) / static_cast<double>(SDL_GetPerformanceFrequency()));
        lastCounter = counter;
        totalSeconds += deltaTime;

        float scrollY = 0.0f;
        SDL_Event event{};
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
            } else if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) {
                running = false;
            } else if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_F11) {
                fullscreen = !fullscreen;
                SDL_SetWindowFullscreen(window, fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
            } else if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_1) {
                gAntiAliasMode = dashboard::AntiAliasMode::Off;
            } else if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_2) {
                gAntiAliasMode = dashboard::AntiAliasMode::FXAA;
            } else if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_3) {
                gAntiAliasMode = dashboard::AntiAliasMode::SMAA;
            } else if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_4) {
                gRequestedRenderScale = gRequestedRenderScale == 1 ? 2 : 1;
            } else if (event.type == SDL_MOUSEWHEEL) {
                scrollY = static_cast<float>(event.wheel.y) * 20.0f;
            }
        }

        int drawableWidth = 0;
        int drawableHeight = 0;
        SDL_GL_GetDrawableSize(window, &drawableWidth, &drawableHeight);
        int windowWidth = 0;
        int windowHeight = 0;
        SDL_GetWindowSize(window, &windowWidth, &windowHeight);
        const float dpiScaleX = windowWidth > 0 ? static_cast<float>(drawableWidth) / static_cast<float>(windowWidth) : 1.0f;
        const float dpiScaleY = windowHeight > 0 ? static_cast<float>(drawableHeight) / static_cast<float>(windowHeight) : 1.0f;
        const float dpiScale = std::max(dpiScaleX, dpiScaleY);
        int renderScale = gRequestedRenderScale;
        const long long scaledPixels = static_cast<long long>(drawableWidth) * static_cast<long long>(drawableHeight) * renderScale * renderScale;
        if (scaledPixels > 9000000LL) {
            renderScale = 1;
        }
        const int renderWidth = std::max(1, drawableWidth * renderScale);
        const int renderHeight = std::max(1, drawableHeight * renderScale);
        gRenderScale = renderScale;
        gUiScale = static_cast<float>(renderScale) * dpiScale;
        std::snprintf(gAntiAliasBadge, sizeof(gAntiAliasBadge), "%s %dx", dashboard::AntiAliasModeName(gAntiAliasMode), gRenderScale);
        if (renderBuffer.Renderer() && (renderBuffer.Width() != renderWidth || renderBuffer.Height() != renderHeight)) {
            ClearTextTextureCacheForRenderer(renderBuffer.Renderer());
        }
        if (textBuffer.Renderer() && (textBuffer.Width() != drawableWidth || textBuffer.Height() != drawableHeight)) {
            ClearTextTextureCacheForRenderer(textBuffer.Renderer());
        }
        if (!renderBuffer.Resize(renderWidth, renderHeight, true) || !textBuffer.Resize(drawableWidth, drawableHeight, false)) {
            running = false;
            continue;
        }
        Clay_SetLayoutDimensions(Clay_Dimensions{static_cast<float>(renderWidth), static_cast<float>(renderHeight)});

        int mouseX = 0;
        int mouseY = 0;
        const Uint32 mouseButtons = SDL_GetMouseState(&mouseX, &mouseY);
        const float mouseScaleX = windowWidth > 0 ? static_cast<float>(renderWidth) / static_cast<float>(windowWidth) : 1.0f;
        const float mouseScaleY = windowHeight > 0 ? static_cast<float>(renderHeight) / static_cast<float>(windowHeight) : 1.0f;
        Clay_SetPointerState(Clay_Vector2{static_cast<float>(mouseX) * mouseScaleX, static_cast<float>(mouseY) * mouseScaleY}, (mouseButtons & SDL_BUTTON(SDL_BUTTON_LEFT)) != 0);
        Clay_UpdateScrollContainers(true, Clay_Vector2{0.0f, scrollY}, deltaTime);

        SDL_Renderer *renderer = renderBuffer.Renderer();
        renderBuffer.Clear(
            static_cast<Uint8>(std::clamp(Pal::Page.r, 0.0f, 255.0f)),
            static_cast<Uint8>(std::clamp(Pal::Page.g, 0.0f, 255.0f)),
            static_cast<Uint8>(std::clamp(Pal::Page.b, 0.0f, 255.0f)),
            static_cast<Uint8>(std::clamp(Pal::Page.a, 0.0f, 255.0f)));
        Clay_RenderCommandArray commands = BuildDashboard(deltaTime);
        RenderClay(renderer, fonts, commands, totalSeconds, false);
        SDL_RenderPresent(renderer);
        renderBuffer.Upload();
        if (!postProcess.Render(renderBuffer.Texture(), renderWidth, renderHeight, drawableWidth, drawableHeight, gAntiAliasMode)) {
            gAntiAliasMode = dashboard::AntiAliasMode::Off;
            postProcess.Render(renderBuffer.Texture(), renderWidth, renderHeight, drawableWidth, drawableHeight, gAntiAliasMode);
        }

        textBuffer.Clear(0, 0, 0, 0);
        SDL_Renderer *textRenderer = textBuffer.Renderer();
        RenderClayTextOverlay(textRenderer, fonts, commands, 1.0f / static_cast<float>(renderScale));
        SDL_RenderPresent(textRenderer);
        textBuffer.Upload();
        postProcess.CompositeOverlay(textBuffer.Texture(), drawableWidth, drawableHeight);

        if (gHandCursorRequested && handCursor) {
            SDL_SetCursor(handCursor);
        } else if (arrowCursor) {
            SDL_SetCursor(arrowCursor);
        }

        SDL_GL_SwapWindow(window);
    }

    std::free(clayMemory);
    fonts.Clear();
    ClearTextTextureCache();
    if (handCursor) {
        SDL_FreeCursor(handCursor);
    }
    if (arrowCursor) {
        SDL_FreeCursor(arrowCursor);
    }
    postProcess.Destroy();
    textBuffer.Destroy();
    renderBuffer.Destroy();
    SDL_GL_DeleteContext(glContext);
    SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();
    return 0;
}
