#include "overlay.hpp"
#include "other/configs/globals.h"
#include "libraries/opus/include/opus.h"
#include <Windows.h>
#include <iostream>
#include <algorithm> // For std::min, std::max
#include <cmath>     // For sinf, log10f, powf
#include "skCrypt.hpp"
#include <fstream>
#include <WinUser.h>
#include <TlHelp32.h>
#include <psapi.h>   // For GetModuleInformation
#include <chrono>
#include <dwmapi.h>
#include <map> // Added for std::map

// Function declarations
std::string GetProcessName();

// Define PI constant since IM_PI is undefined
#define MY_PI 3.14159265358979323846f

// Define our own min/max functions to avoid namespace issues
template <typename T>
T Min(T a, T b) { return (a < b) ? a : b; }

template <typename T>
T Max(T a, T b) { return (a > b) ? a : b; }

static float time_since_start = 0.0f;
ImVec4 clear_color = ImVec4(0.0f, 0.0f, 0.0f, 1.00f); // Black background color
ImVec4 main_color = ImVec4(1.0f, 0.0f, 0.0f, 1.00f); // Red main color
int TOGGLE_HOTKEY = VK_F5; // Default hotkey is F5, but can be changed

// Global window handles
HWND hwnd = nullptr;
bool show_imgui_window = true;

// Audio processing variables - moved to global scope for access across functions
float bassEQ = 0.0f;
float midEQ = 0.0f;
float highEQ = 0.0f;
float panningValue = 0.0f;
bool inHeadLeft = false;
bool inHeadRight = false;
bool energyEnabled = true;
float energyValue = 510000.0f;
float bitrateValue = 510000.0f; // Bitrate slider value with minimum as default
int audioChannelMode = 1; // Default to stereo (index 1 in the combo)
const char* channelNames[] = { "Mono", "Stereo" }; // Options for channel selector
float reverbMix = 0.5f;        // Dry/wet mix (0.0 to 1.0)
float reverbSize = 0.7f;       // Room size (0.0 to 1.0)
float reverbDamping = 0.5f;    // High frequency damping (0.0 to 1.0)
float reverbWidth = 1.0f;      // Stereo width (0.0 to 1.0)
bool reverbEnabled = false;    // Toggle for reverb effect
bool rgbModeEnabled = false;   // Toggle for RGB color picker mode
float rgbCycleSpeed = 0.5f;    // Speed of RGB color cycling
bool bassBoostEnabled = false; // Toggle for extra bass boost effect

// Forward declarations
void StyleTabBar();
void DrawNestedFrame(const char* title, bool rgbMode);
void EndNestedFrame(bool rgbMode);
void DrawAlignedSeparator(const char* label, bool rgbMode, ImVec4 customColor = ImVec4(0, 0, 0, 0));

// Forward declarations for animation functions
float AnimateSin(float speed, float min, float max, float phase);
float AnimateExpo(float speed, float min, float max);
float AnimateElastic(float speed, float min, float max);
void AnimateBorderCorners(const ImVec2& topLeft, const ImVec2& bottomRight, const ImVec4& color, float thickness);
bool AnimatedButton(const char* label, const ImVec2& size);
void AnimatedSlider(const char* label, float* value, float min, float max, const char* tooltip);
bool AnimatedToggleButton(const char* label, bool* value);
bool AnimatedPanningSlider(const char* label, float* value, float min, float max);
void DrawAdvancedSeparator(bool rgbMode, float widthFactor, float yFactor);
void DrawEnhancedPanel(const char* label, float alpha);
void EndEnhancedPanel(bool rgbMode);
bool AnimatedProfessionalSlider(const char* label, float* value, float min, float max, const char* format = "%.2f", const char* tooltip = nullptr);
bool PremiumButton(const char* label, const ImVec2& size);
void DrawInnerContentBorder();

// Implementations of missing functions
void EndNestedFrame(bool rgbMode) {
    // Add some padding at the bottom of the frame
    ImGui::Spacing();

    // Draw bottom separator line
    ImVec4 separatorColor;
    if (rgbMode) {
        // Use RGB cycling colors for separator in RGB mode
        float timeOffset = time_since_start * rgbCycleSpeed;
        float r = 0.5f + 0.5f * sinf(timeOffset);
        float g = 0.5f + 0.5f * sinf(timeOffset + 2.0f * MY_PI / 3.0f);
        float b = 0.5f + 0.5f * sinf(timeOffset + 4.0f * MY_PI / 3.0f);
        separatorColor = ImVec4(r, g, b, 0.7f);
    }
    else {
        // Use main accent color for separator
        float pulse = AnimateSin(1.5f, 0.7f, 1.0f, 0.0f);
        separatorColor = ImVec4(main_color.x * pulse, main_color.y * pulse, main_color.z * pulse, 0.7f);
    }

    // Get current position to draw the bottom line
    ImVec2 p = ImGui::GetCursorScreenPos();
    float width = ImGui::GetContentRegionAvail().x;
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    // Draw a subtle gradient line
    float lineHeight = 2.0f;
    drawList->AddRectFilledMultiColor(
        ImVec2(p.x - 10.0f, p.y),
        ImVec2(p.x + width + 10.0f, p.y + lineHeight),
        ImGui::ColorConvertFloat4ToU32(ImVec4(separatorColor.x, separatorColor.y, separatorColor.z, 0.0f)),
        ImGui::ColorConvertFloat4ToU32(separatorColor),
        ImGui::ColorConvertFloat4ToU32(separatorColor),
        ImGui::ColorConvertFloat4ToU32(ImVec4(separatorColor.x, separatorColor.y, separatorColor.z, 0.0f))
    );

    // Add some spacing after the line
    ImGui::Dummy(ImVec2(0, lineHeight + 5.0f));

    // Pop the ID pushed in DrawNestedFrame
    ImGui::PopID();

    // Remove the indentation
    ImGui::Unindent(10.0f);

    // Add some spacing after the frame
    ImGui::Spacing();
    ImGui::Spacing();
}
void DrawAlignedSeparator(const char* label, bool rgbMode, ImVec4 customColor) {
    // Get colors
    ImVec4 sepColor;
    if (customColor.x != 0 || customColor.y != 0 || customColor.z != 0 || customColor.w != 0) {
        // Use the custom color if provided
        sepColor = customColor;
    }
    else {
        // Use the default color logic
        sepColor = rgbMode ?
            ImVec4(
                0.5f + 0.5f * sinf(time_since_start * rgbCycleSpeed),
                0.5f + 0.5f * sinf(time_since_start * rgbCycleSpeed + 2.0f * MY_PI / 3.0f),
                0.5f + 0.5f * sinf(time_since_start * rgbCycleSpeed + 4.0f * MY_PI / 3.0f),
                0.8f
            ) : ImVec4(main_color.x, main_color.y, main_color.z, 0.8f);
    }

    ImVec4 textColor = ImVec4(1.0f, 1.0f, 1.0f, 1.0f); // White text for better visibility

    // Get essential dimensions - we need the TOTAL window width for true centering
    float windowWidth = ImGui::GetWindowWidth();
    float contentWidth = ImGui::GetContentRegionAvail().x;
    // Adjust line width to prevent going through the right border
    float lineWidth = contentWidth * 0.75f; // Use shorter width for the separator
    // Right border offset adjustment to match the left side
    float rightBorderMargin = 16.0f;

    // Hard-coded correction offset to adjust for any UI layout oddities
    float centeringOffset = 12.0f;  // Adjusted to better center the separators

    // Add minimal spacing before separator for a cleaner look
    ImGui::Dummy(ImVec2(0, 3.0f));

    ImDrawList* drawList = ImGui::GetWindowDrawList();

    if (label && label[0] != '\0') {
        // Calculate text dimensions
        float textWidth = ImGui::CalcTextSize(label).x;
        float textHeight = ImGui::GetTextLineHeight();
        float textPadding = 10.0f; // Padding on each side of text

        // Get screen positions
        ImVec2 windowPos = ImGui::GetWindowPos();
        float startX = windowPos.x + (windowWidth - lineWidth) / 2.0f - centeringOffset;
        float startY = ImGui::GetCursorScreenPos().y + textHeight / 2.0f;

        // Calculate exact center of window (not just content region)
        float windowCenterX = windowPos.x + windowWidth / 2.0f - centeringOffset;
        float textStartX = windowCenterX - textWidth / 2.0f;

        // Create slightly thinner, more polished line thickness
        float lineThickness = 0.8f;

        // Draw first line (left side) with rounded end caps
        // First line - left side with rounded cap at start
        float lineLeftEnd = textStartX - textPadding;

        // Ensure the left line doesn't start too far from the left border
        float leftBorderMargin = 16.0f;
        startX = Max(startX, windowPos.x + leftBorderMargin);

        // Draw the line with rounded caps by adding a small circle at each end
        // Left side rounded cap
        drawList->AddCircleFilled(
            ImVec2(startX, startY),
            lineThickness / 2.0f,
            ImGui::ColorConvertFloat4ToU32(sepColor),
            12
        );

        // Left line segment
        drawList->AddLine(
            ImVec2(startX, startY),
            ImVec2(lineLeftEnd, startY),
            ImGui::ColorConvertFloat4ToU32(sepColor),
            lineThickness
        );

        // Draw the centered text exactly in the middle of the window with a subtle shadow for better readability
        // Text shadow
        drawList->AddText(
            ImVec2(textStartX + 1.0f, startY - textHeight / 2.0f + 1.0f),
            ImGui::ColorConvertFloat4ToU32(ImVec4(0.0f, 0.0f, 0.0f, 0.4f)),
            label
        );

        // Main text
        drawList->AddText(
            ImVec2(textStartX, startY - textHeight / 2.0f),
            ImGui::ColorConvertFloat4ToU32(textColor),
            label
        );

        // Second line (right side) with rounded cap at end
        float lineRightStart = textStartX + textWidth + textPadding;
        // Ensure the right end point doesn't go through the border
        float lineRightEnd = Min(startX + lineWidth, windowPos.x + windowWidth - rightBorderMargin);

        // Right line segment with proper end position
        drawList->AddLine(
            ImVec2(lineRightStart, startY),
            ImVec2(lineRightEnd, startY),
            ImGui::ColorConvertFloat4ToU32(sepColor),
            lineThickness
        );

        // Right side rounded cap
        drawList->AddCircleFilled(
            ImVec2(lineRightEnd, startY),
            lineThickness / 2.0f,
            ImGui::ColorConvertFloat4ToU32(sepColor),
            12
        );

        // Move cursor past the separator
        ImGui::Dummy(ImVec2(0, textHeight));
    }
    else {
        // Just draw a thinner full line with rounded caps
        ImVec2 windowPos = ImGui::GetWindowPos();
        float startX = windowPos.x + (windowWidth - lineWidth) / 2.0f - centeringOffset;
        float startY = ImGui::GetCursorScreenPos().y;

        // Ensure the left end point doesn't start too close to the left border
        float leftBorderMargin = 16.0f;
        startX = Max(startX, windowPos.x + leftBorderMargin);

        // Ensure the right end point stays within the border
        float endX = Min(startX + lineWidth, windowPos.x + windowWidth - rightBorderMargin);
        float lineThickness = 0.8f;

        // Left side rounded cap
        drawList->AddCircleFilled(
            ImVec2(startX, startY),
            lineThickness / 2.0f,
            ImGui::ColorConvertFloat4ToU32(sepColor),
            12
        );

        // Line segment
        drawList->AddLine(
            ImVec2(startX, startY),
            ImVec2(endX, startY),
            ImGui::ColorConvertFloat4ToU32(sepColor),
            lineThickness
        );

        // Right side rounded cap
        drawList->AddCircleFilled(
            ImVec2(endX, startY),
            lineThickness / 2.0f,
            ImGui::ColorConvertFloat4ToU32(sepColor),
            12
        );

        ImGui::Dummy(ImVec2(contentWidth, 1.0f));
    }

    // Add minimal spacing after separator
    ImGui::Dummy(ImVec2(0, 3.0f));
}

// Creates a console window for debug outputs
void CreateConsole() {
    AllocConsole();
    FILE* f;
    freopen_s(&f, "CONOUT$", "w", stdout);
}

// Update the time since the start of the application
void UpdateTime() {
    static auto start_time = std::chrono::high_resolution_clock::now();
    auto current_time = std::chrono::high_resolution_clock::now();
    time_since_start = std::chrono::duration<float>(current_time - start_time).count();

    // Update RGB colors for UI elements when RGB mode is enabled
    if (rgbModeEnabled) {
        // Calculate RGB colors based on time with the configured cycle speed
        float timeOffset = time_since_start * rgbCycleSpeed;
        float r = 0.5f + 0.5f * sinf(timeOffset);
        float g = 0.5f + 0.5f * sinf(timeOffset + 2.0f * MY_PI / 3.0f);
        float b = 0.5f + 0.5f * sinf(timeOffset + 4.0f * MY_PI / 3.0f);

        // Store the current RGB color for use throughout the UI
        ImVec4 rgbColor = ImVec4(r, g, b, 1.0f);

        ImGuiStyle& style = ImGui::GetStyle();

        // Ensure tab bar separators are visible with RGB effect
        style.TabBarBorderSize = 1.0f;
        style.TabRounding = 4.0f;

        // Set RGB colors for all UI elements with consistent intensity levels
        // Main interactive elements
        style.Colors[ImGuiCol_Button] = ImVec4(r * 0.6f, g * 0.6f, b * 0.6f, 0.8f);
        style.Colors[ImGuiCol_ButtonHovered] = ImVec4(r * 0.8f, g * 0.8f, b * 0.8f, 0.9f);
        style.Colors[ImGuiCol_ButtonActive] = ImVec4(r, g, b, 1.0f);

        // Sliders and checkboxes
        style.Colors[ImGuiCol_SliderGrab] = rgbColor;
        style.Colors[ImGuiCol_SliderGrabActive] = ImVec4(r * 1.2f, g * 1.2f, b * 1.2f, 1.0f);
        style.Colors[ImGuiCol_CheckMark] = rgbColor;

        // Explicitly set all separator types to ensure they're colored
        style.Colors[ImGuiCol_Separator] = rgbColor;
        style.Colors[ImGuiCol_SeparatorHovered] = ImVec4(r * 1.1f, g * 1.1f, b * 1.1f, 1.0f);
        style.Colors[ImGuiCol_SeparatorActive] = ImVec4(r * 1.2f, g * 1.2f, b * 1.2f, 1.0f);

        // Tab related colors
        style.Colors[ImGuiCol_Tab] = ImVec4(r * 0.5f, g * 0.5f, b * 0.5f, 0.8f);
        style.Colors[ImGuiCol_TabHovered] = ImVec4(r * 0.8f, g * 0.8f, b * 0.8f, 0.9f);
        style.Colors[ImGuiCol_TabActive] = rgbColor;
        style.Colors[ImGuiCol_TabUnfocused] = ImVec4(r * 0.3f, g * 0.3f, b * 0.3f, 0.8f);
        style.Colors[ImGuiCol_TabUnfocusedActive] = ImVec4(r * 0.6f, g * 0.6f, b * 0.6f, 0.9f);

        // Border color (affects all borders including tab bar separator)
        style.Colors[ImGuiCol_Border] = rgbColor;

        // Header colors for collapsing headers
        style.Colors[ImGuiCol_Header] = ImVec4(r * 0.4f, g * 0.4f, b * 0.4f, 0.8f);
        style.Colors[ImGuiCol_HeaderHovered] = ImVec4(r * 0.6f, g * 0.6f, b * 0.6f, 0.9f);
        style.Colors[ImGuiCol_HeaderActive] = ImVec4(r * 0.8f, g * 0.8f, b * 0.8f, 1.0f);

        // Frame backgrounds
        style.Colors[ImGuiCol_FrameBg] = ImVec4(r * 0.2f, g * 0.2f, b * 0.2f, 0.8f);
        style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(r * 0.3f, g * 0.3f, b * 0.3f, 0.9f);
        style.Colors[ImGuiCol_FrameBgActive] = ImVec4(r * 0.4f, g * 0.4f, b * 0.4f, 1.0f);

        // Scrollbars
        style.Colors[ImGuiCol_ScrollbarGrab] = ImVec4(r * 0.8f, g * 0.8f, b * 0.8f, 1.0f);
        style.Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(r, g, b, 1.0f);
        style.Colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(r * 1.2f, g * 1.2f, b * 1.2f, 1.0f);

        // Other UI elements
        style.Colors[ImGuiCol_ResizeGrip] = ImVec4(r * 0.4f, g * 0.4f, b * 0.4f, 0.8f);
        style.Colors[ImGuiCol_ResizeGripHovered] = ImVec4(r * 0.6f, g * 0.6f, b * 0.6f, 1.0f);
        style.Colors[ImGuiCol_ResizeGripActive] = ImVec4(r * 0.8f, g * 0.8f, b * 0.8f, 1.0f);
        style.Colors[ImGuiCol_PlotLines] = rgbColor;
        style.Colors[ImGuiCol_PlotLinesHovered] = ImVec4(r * 1.2f, g * 1.2f, b * 1.2f, 1.0f);
        style.Colors[ImGuiCol_PlotHistogram] = rgbColor;
        style.Colors[ImGuiCol_PlotHistogramHovered] = ImVec4(r * 1.2f, g * 1.2f, b * 1.2f, 1.0f);
        style.Colors[ImGuiCol_TextSelectedBg] = ImVec4(r * 0.5f, g * 0.5f, b * 0.5f, 1.0f);
    }
    else {
        // Get ImGui style
        ImGuiStyle& style = ImGui::GetStyle();

        // Ensure tab bar separators are visible with solid color
        style.TabBarBorderSize = 1.0f;
        style.TabRounding = 4.0f;

        // Use static accent color
        style.Colors[ImGuiCol_Button] = ImVec4(main_color.x * 0.6f, main_color.y * 0.6f, main_color.z * 0.6f, 1.0f);
        style.Colors[ImGuiCol_ButtonHovered] = ImVec4(main_color.x * 0.7f, main_color.y * 0.7f, main_color.z * 0.7f, 1.0f);
        style.Colors[ImGuiCol_ButtonActive] = ImVec4(main_color.x * 0.8f, main_color.y * 0.8f, main_color.z * 0.8f, 1.0f);
        style.Colors[ImGuiCol_CheckMark] = main_color;
        style.Colors[ImGuiCol_SliderGrab] = main_color;
        style.Colors[ImGuiCol_SliderGrabActive] = ImVec4(main_color.x * 1.2f, main_color.y * 1.2f, main_color.z * 1.2f, 1.0f);

        // Make separators more vibrant with full color
        style.Colors[ImGuiCol_Separator] = main_color;
        style.Colors[ImGuiCol_SeparatorHovered] = ImVec4(main_color.x * 1.2f, main_color.y * 1.2f, main_color.z * 1.2f, 1.0f);
        style.Colors[ImGuiCol_SeparatorActive] = ImVec4(main_color.x * 1.4f, main_color.y * 1.4f, main_color.z * 1.4f, 1.0f);

        // Tab bar and tab related colors
        style.Colors[ImGuiCol_Tab] = ImVec4(main_color.x * 0.5f, main_color.y * 0.5f, main_color.z * 0.5f, 1.0f);
        style.Colors[ImGuiCol_TabHovered] = main_color;
        style.Colors[ImGuiCol_TabActive] = main_color;
        style.Colors[ImGuiCol_TabUnfocused] = ImVec4(main_color.x * 0.3f, main_color.y * 0.3f, main_color.z * 0.3f, 1.0f);
        style.Colors[ImGuiCol_TabUnfocusedActive] = ImVec4(main_color.x * 0.5f, main_color.y * 0.5f, main_color.z * 0.5f, 1.0f);

        // Tab bar separator color
        style.Colors[ImGuiCol_Border] = main_color;

        // Other UI elements
        style.Colors[ImGuiCol_FrameBg] = ImVec4(main_color.x * 0.2f, main_color.y * 0.2f, main_color.z * 0.2f, 1.0f);
        style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(main_color.x * 0.3f, main_color.y * 0.3f, main_color.z * 0.3f, 1.0f);
        style.Colors[ImGuiCol_FrameBgActive] = ImVec4(main_color.x * 0.4f, main_color.y * 0.4f, main_color.z * 0.4f, 1.0f);
        style.Colors[ImGuiCol_Header] = ImVec4(main_color.x * 0.4f, main_color.y * 0.4f, main_color.z * 0.4f, 1.0f);
        style.Colors[ImGuiCol_HeaderHovered] = ImVec4(main_color.x * 0.5f, main_color.y * 0.5f, main_color.z * 0.5f, 1.0f);
        style.Colors[ImGuiCol_HeaderActive] = ImVec4(main_color.x * 0.6f, main_color.y * 0.6f, main_color.z * 0.6f, 1.0f);
        style.Colors[ImGuiCol_ResizeGrip] = ImVec4(main_color.x * 0.4f, main_color.y * 0.4f, main_color.z * 0.4f, 1.0f);
        style.Colors[ImGuiCol_ResizeGripHovered] = ImVec4(main_color.x * 0.6f, main_color.y * 0.6f, main_color.z * 0.6f, 1.0f);
        style.Colors[ImGuiCol_ResizeGripActive] = ImVec4(main_color.x * 0.8f, main_color.y * 0.8f, main_color.z * 0.8f, 1.0f);
        style.Colors[ImGuiCol_ScrollbarGrab] = main_color;
        style.Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(main_color.x * 1.1f, main_color.y * 1.1f, main_color.z * 1.1f, 1.0f);
        style.Colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(main_color.x * 1.2f, main_color.y * 1.2f, main_color.z * 1.2f, 1.0f);
    }
}

// Converts a UTF-8 string to a wide string
std::wstring ConvertToWide(const char* str) {
    int len = MultiByteToWideChar(CP_UTF8, 0, str, -1, nullptr, 0);
    std::wstring wstr(len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, str, -1, &wstr[0], len);
    return wstr;
}

namespace Spoofing {
    bool ProcessIsolation = false;
    bool Hider = true;
}

// Creates a new configuration file with default settings
void CreateConfiguration(const char* file_path) {
    std::ofstream ofs(file_path, std::ios::binary);
    if (ofs) {
        // Default gain settings
        float default_gain = 1.0f;
        float default_exp_gain = 1.0f;
        float default_vunits_gain = 1.0f;

        // Default system settings
        bool default_isolation = true;
        bool default_hider = false;
        DWORD default_hotkey = VK_INSERT;
        float default_bitrate = 64000.0f;

        // Default color settings
        ImVec4 default_main_color = ImVec4(0.5f, 0.8f, 0.5f, 1.0f);
        ImVec4 default_clear_color = ImVec4(0.05f, 0.05f, 0.05f, 0.94f);

        // Default reverb settings
        bool default_reverb = false;
        float default_reverb_mix = 0.2f;
        float default_reverb_size = 0.5f;
        float default_reverb_damping = 0.5f;
        float default_reverb_width = 1.0f;

        // Default RGB mode settings
        bool default_rgb_mode = false;
        float default_rgb_speed = 0.6f;

        // Default channel setting
        int default_channel_mode = 1; // Stereo

        // Default EQ settings
        float default_bass_eq = 0.0f;
        float default_mid_eq = 0.0f;
        float default_high_eq = 0.0f;
        bool default_bass_boost = false;

        // Default energy effect settings
        bool default_energy_enabled = false;
        float default_energy_value = 510000.0f;

        // Default stereo effect settings
        float default_panning = 0.0f;
        bool default_in_head_left = false;
        bool default_in_head_right = false;

        // Write default values to file
        ofs.write(reinterpret_cast<const char*>(&default_gain), sizeof(default_gain));
        ofs.write(reinterpret_cast<const char*>(&default_exp_gain), sizeof(default_exp_gain));
        ofs.write(reinterpret_cast<const char*>(&default_vunits_gain), sizeof(default_vunits_gain));
        ofs.write(reinterpret_cast<const char*>(&default_isolation), sizeof(default_isolation));
        ofs.write(reinterpret_cast<const char*>(&default_hider), sizeof(default_hider));
        ofs.write(reinterpret_cast<const char*>(&default_hotkey), sizeof(default_hotkey));
        ofs.write(reinterpret_cast<const char*>(&default_bitrate), sizeof(default_bitrate));
        ofs.write(reinterpret_cast<const char*>(&default_main_color), sizeof(default_main_color));
        ofs.write(reinterpret_cast<const char*>(&default_clear_color), sizeof(default_clear_color));
        ofs.write(reinterpret_cast<const char*>(&default_reverb), sizeof(default_reverb));
        ofs.write(reinterpret_cast<const char*>(&default_reverb_mix), sizeof(default_reverb_mix));
        ofs.write(reinterpret_cast<const char*>(&default_reverb_size), sizeof(default_reverb_size));
        ofs.write(reinterpret_cast<const char*>(&default_reverb_damping), sizeof(default_reverb_damping));
        ofs.write(reinterpret_cast<const char*>(&default_reverb_width), sizeof(default_reverb_width));
        ofs.write(reinterpret_cast<const char*>(&default_rgb_mode), sizeof(default_rgb_mode));
        ofs.write(reinterpret_cast<const char*>(&default_rgb_speed), sizeof(default_rgb_speed));
        ofs.write(reinterpret_cast<const char*>(&default_channel_mode), sizeof(default_channel_mode));
        ofs.write(reinterpret_cast<const char*>(&default_bass_eq), sizeof(default_bass_eq));
        ofs.write(reinterpret_cast<const char*>(&default_mid_eq), sizeof(default_mid_eq));
        ofs.write(reinterpret_cast<const char*>(&default_high_eq), sizeof(default_high_eq));
        ofs.write(reinterpret_cast<const char*>(&default_bass_boost), sizeof(default_bass_boost));
        ofs.write(reinterpret_cast<const char*>(&default_energy_enabled), sizeof(default_energy_enabled));
        ofs.write(reinterpret_cast<const char*>(&default_energy_value), sizeof(default_energy_value));
        ofs.write(reinterpret_cast<const char*>(&default_panning), sizeof(default_panning));
        ofs.write(reinterpret_cast<const char*>(&default_in_head_left), sizeof(default_in_head_left));
        ofs.write(reinterpret_cast<const char*>(&default_in_head_right), sizeof(default_in_head_right));
        ofs.close();
    }
}

// Save configuration to a file
void SaveConfiguration(const char* file_path) {
    std::ofstream ofs(file_path, std::ios::binary);
    if (ofs) {
        // Save gain settings
        ofs.write(reinterpret_cast<const char*>(&Gain), sizeof(Gain));
        ofs.write(reinterpret_cast<const char*>(&ExpGain), sizeof(ExpGain));
        ofs.write(reinterpret_cast<const char*>(&VunitsGain), sizeof(VunitsGain));

        // Save system settings
        ofs.write(reinterpret_cast<const char*>(&Spoofing::ProcessIsolation), sizeof(Spoofing::ProcessIsolation));
        ofs.write(reinterpret_cast<const char*>(&Spoofing::Hider), sizeof(Spoofing::Hider));
        ofs.write(reinterpret_cast<const char*>(&TOGGLE_HOTKEY), sizeof(TOGGLE_HOTKEY));
        ofs.write(reinterpret_cast<const char*>(&bitrateValue), sizeof(bitrateValue));

        // Save color settings
        ofs.write(reinterpret_cast<const char*>(&main_color), sizeof(main_color));
        ofs.write(reinterpret_cast<const char*>(&clear_color), sizeof(clear_color));

        // Save reverb settings
        ofs.write(reinterpret_cast<const char*>(&reverbEnabled), sizeof(reverbEnabled));
        ofs.write(reinterpret_cast<const char*>(&reverbMix), sizeof(reverbMix));
        ofs.write(reinterpret_cast<const char*>(&reverbSize), sizeof(reverbSize));
        ofs.write(reinterpret_cast<const char*>(&reverbDamping), sizeof(reverbDamping));
        ofs.write(reinterpret_cast<const char*>(&reverbWidth), sizeof(reverbWidth));

        // Save RGB mode setting
        ofs.write(reinterpret_cast<const char*>(&rgbModeEnabled), sizeof(rgbModeEnabled));
        ofs.write(reinterpret_cast<const char*>(&rgbCycleSpeed), sizeof(rgbCycleSpeed));

        // Save audio channel mode setting
        ofs.write(reinterpret_cast<const char*>(&audioChannelMode), sizeof(audioChannelMode));

        // Save EQ settings
        ofs.write(reinterpret_cast<const char*>(&bassEQ), sizeof(bassEQ));
        ofs.write(reinterpret_cast<const char*>(&midEQ), sizeof(midEQ));
        ofs.write(reinterpret_cast<const char*>(&highEQ), sizeof(highEQ));
        ofs.write(reinterpret_cast<const char*>(&bassBoostEnabled), sizeof(bassBoostEnabled));

        // Save energy effect settings
        ofs.write(reinterpret_cast<const char*>(&energyEnabled), sizeof(energyEnabled));
        ofs.write(reinterpret_cast<const char*>(&energyValue), sizeof(energyValue));

        // Save stereo effects
        ofs.write(reinterpret_cast<const char*>(&panningValue), sizeof(panningValue));
        ofs.write(reinterpret_cast<const char*>(&inHeadLeft), sizeof(inHeadLeft));
        ofs.write(reinterpret_cast<const char*>(&inHeadRight), sizeof(inHeadRight));

        ofs.close();
    }
}

// Load configuration from a file
void LoadConfiguration(const char* file_path) {
    std::ifstream ifs(file_path, std::ios::binary);
    if (ifs) {
        // Load gain settings
        ifs.read(reinterpret_cast<char*>(&Gain), sizeof(Gain));
        ifs.read(reinterpret_cast<char*>(&ExpGain), sizeof(ExpGain));

        // Try to read VunitsGain if it exists
        if (ifs.peek() != EOF) {
            ifs.read(reinterpret_cast<char*>(&VunitsGain), sizeof(VunitsGain));
        }

        // Load system settings
        ifs.read(reinterpret_cast<char*>(&Spoofing::ProcessIsolation), sizeof(Spoofing::ProcessIsolation));
        ifs.read(reinterpret_cast<char*>(&Spoofing::Hider), sizeof(Spoofing::Hider));

        // Read hotkey if it exists in the file (for backward compatibility)
        if (ifs.peek() != EOF) {
            ifs.read(reinterpret_cast<char*>(&TOGGLE_HOTKEY), sizeof(TOGGLE_HOTKEY));
        }

        // Try to read bitrate value if it exists
        if (ifs.peek() != EOF) {
            ifs.read(reinterpret_cast<char*>(&bitrateValue), sizeof(bitrateValue));
            // Ensure bitrate is within valid range
            bitrateValue = Max(16000.0f, Min(bitrateValue, 510000.0f));
        }

        // Try to read color settings if they exist
        if (ifs.peek() != EOF) {
            ifs.read(reinterpret_cast<char*>(&main_color), sizeof(main_color));
            if (ifs.peek() != EOF) {
                ifs.read(reinterpret_cast<char*>(&clear_color), sizeof(clear_color));
            }
        }

        // Try to read reverb settings
        if (ifs.peek() != EOF) {
            ifs.read(reinterpret_cast<char*>(&reverbEnabled), sizeof(reverbEnabled));

            // Read new reverb parameters if they exist
            if (ifs.peek() != EOF) {
                ifs.read(reinterpret_cast<char*>(&reverbMix), sizeof(reverbMix));
                ifs.read(reinterpret_cast<char*>(&reverbSize), sizeof(reverbSize));
                ifs.read(reinterpret_cast<char*>(&reverbDamping), sizeof(reverbDamping));
                ifs.read(reinterpret_cast<char*>(&reverbWidth), sizeof(reverbWidth));
            }
        }

        // Try to read RGB mode setting if it exists
        if (ifs.peek() != EOF) {
            ifs.read(reinterpret_cast<char*>(&rgbModeEnabled), sizeof(rgbModeEnabled));

            // Try to read RGB cycle speed if it exists
            if (ifs.peek() != EOF) {
                ifs.read(reinterpret_cast<char*>(&rgbCycleSpeed), sizeof(rgbCycleSpeed));
                // Ensure speed is within valid range
                rgbCycleSpeed = Max(0.1f, Min(rgbCycleSpeed, 2.0f));
            }
        }

        // Try to read audio channel mode if it exists
        if (ifs.peek() != EOF) {
            ifs.read(reinterpret_cast<char*>(&audioChannelMode), sizeof(audioChannelMode));
            // Ensure value is within valid range (0=Mono, 1=Stereo)
            audioChannelMode = Max(0, Min(audioChannelMode, 1));
        }

        // Try to read EQ settings if they exist
        if (ifs.peek() != EOF) {
            ifs.read(reinterpret_cast<char*>(&bassEQ), sizeof(bassEQ));
            if (ifs.peek() != EOF) {
                ifs.read(reinterpret_cast<char*>(&midEQ), sizeof(midEQ));
                if (ifs.peek() != EOF) {
                    ifs.read(reinterpret_cast<char*>(&highEQ), sizeof(highEQ));
                    if (ifs.peek() != EOF) {
                        ifs.read(reinterpret_cast<char*>(&bassBoostEnabled), sizeof(bassBoostEnabled));
                    }
                }
            }
        }

        // Try to read energy effect settings if they exist
        if (ifs.peek() != EOF) {
            ifs.read(reinterpret_cast<char*>(&energyEnabled), sizeof(energyEnabled));
            if (ifs.peek() != EOF) {
                ifs.read(reinterpret_cast<char*>(&energyValue), sizeof(energyValue));
                // Ensure value is within valid range
                energyValue = Max(100000.0f, Min(energyValue, 1000000.0f));
            }
        }

        // Try to read stereo effect settings if they exist
        if (ifs.peek() != EOF) {
            ifs.read(reinterpret_cast<char*>(&panningValue), sizeof(panningValue));
            if (ifs.peek() != EOF) {
                ifs.read(reinterpret_cast<char*>(&inHeadLeft), sizeof(inHeadLeft));
                if (ifs.peek() != EOF) {
                    ifs.read(reinterpret_cast<char*>(&inHeadRight), sizeof(inHeadRight));
                }
            }
        }

        ifs.close();

        // If we have a window, update the hotkey registration
        if (hwnd) {
            UnregisterHotKey(hwnd, 1);
            RegisterHotKey(hwnd, 1, MOD_NOREPEAT, TOGGLE_HOTKEY);
        }
    }
    else {
        // If file doesn't exist, create it with default settings
        CreateConfiguration(file_path);
    }
}

// Function to reset configuration to default values
void ResetConfiguration() {
    // Reset gain settings
    Gain = 1.0f;
    ExpGain = 1.0f;
    VunitsGain = 1.0f;

    // Reset system settings
    Spoofing::ProcessIsolation = true;
    Spoofing::Hider = false;
    TOGGLE_HOTKEY = VK_INSERT;
    bitrateValue = 64000.0f;

    // Reset color settings
    main_color = ImVec4(0.5f, 0.8f, 0.5f, 1.0f); // Default green color
    clear_color = ImVec4(0.05f, 0.05f, 0.05f, 0.94f);

    // Reset reverb settings
    reverbEnabled = false;
    reverbMix = 0.2f;
    reverbSize = 0.5f;
    reverbDamping = 0.5f;
    reverbWidth = 1.0f;

    // Reset RGB mode settings
    rgbModeEnabled = false;
    rgbCycleSpeed = 0.6f;

    // Reset audio channel mode setting
    audioChannelMode = 1; // Default to stereo

    // Reset EQ settings
    bassEQ = 0.0f;
    midEQ = 0.0f;
    highEQ = 0.0f;
    bassBoostEnabled = false;

    // Reset energy effect settings
    energyEnabled = false;
    energyValue = 510000.0f;

    // Reset stereo effect settings
    panningValue = 0.0f;
    inHeadLeft = false;
    inHeadRight = false;

    // If we have a window, update the hotkey registration
    if (hwnd) {
        UnregisterHotKey(hwnd, 1);
        RegisterHotKey(hwnd, 1, MOD_NOREPEAT, TOGGLE_HOTKEY);
    }
}

// Toggle the visibility of the window
void ToggleWindowVisibility() {
    show_imgui_window ? ::ShowWindow(hwnd, SW_HIDE) : ::ShowWindow(hwnd, SW_SHOW);
    show_imgui_window = !show_imgui_window;
}

// Helper function to draw pulsating effect for buttons and sliders
float GetPulsatingValue(float speed = 1.0f, float min = 0.7f, float max = 1.0f) {
    return min + (max - min) * 0.5f * (1.0f + sinf(time_since_start * speed));
}

// Helper to draw a nicer separator with gradient
void DrawSeparator(ImVec4 color = ImVec4(0.7f, 0.0f, 0.0f, 1.0f), float thickness = 1.0f) {
    // Use RGB color if RGB mode is enabled
    if (rgbModeEnabled) {
        // Calculate RGB colors based on time
        float timeOffset = time_since_start * rgbCycleSpeed;
        float r = 0.5f + 0.5f * sinf(timeOffset);
        float g = 0.5f + 0.5f * sinf(timeOffset + 2.0f * MY_PI / 3.0f);
        float b = 0.5f + 0.5f * sinf(timeOffset + 4.0f * MY_PI / 3.0f);

        // Override the input color with RGB color
        color = ImVec4(r, g, b, 1.0f);
    }

    ImGui::PushStyleColor(ImGuiCol_Separator, color);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 10));
    ImGui::Separator();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
    ImGui::Spacing();
}

// Helper to draw decorative corners around sections
void DrawCornerDecorations(const ImVec2& topLeft, const ImVec2& bottomRight, const ImVec4& color, float thickness = 1.0f, float cornerLength = 10.0f) {
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    // Draw top-left corner
    drawList->AddLine(
        ImVec2(topLeft.x, topLeft.y),
        ImVec2(topLeft.x + cornerLength, topLeft.y),
        ImGui::ColorConvertFloat4ToU32(color),
        thickness
    );
    drawList->AddLine(
        ImVec2(topLeft.x, topLeft.y),
        ImVec2(topLeft.x, topLeft.y + cornerLength),
        ImGui::ColorConvertFloat4ToU32(color),
        thickness
    );

    // Draw top-right corner
    drawList->AddLine(
        ImVec2(bottomRight.x, topLeft.y),
        ImVec2(bottomRight.x - cornerLength, topLeft.y),
        ImGui::ColorConvertFloat4ToU32(color),
        thickness
    );
    drawList->AddLine(
        ImVec2(bottomRight.x, topLeft.y),
        ImVec2(bottomRight.x, topLeft.y + cornerLength),
        ImGui::ColorConvertFloat4ToU32(color),
        thickness
    );

    // Draw bottom-left corner
    drawList->AddLine(
        ImVec2(topLeft.x, bottomRight.y),
        ImVec2(topLeft.x + cornerLength, bottomRight.y),
        ImGui::ColorConvertFloat4ToU32(color),
        thickness
    );
    drawList->AddLine(
        ImVec2(topLeft.x, bottomRight.y),
        ImVec2(topLeft.x, bottomRight.y - cornerLength),
        ImGui::ColorConvertFloat4ToU32(color),
        thickness
    );

    // Draw bottom-right corner
    drawList->AddLine(
        ImVec2(bottomRight.x, bottomRight.y),
        ImVec2(bottomRight.x - cornerLength, bottomRight.y),
        ImGui::ColorConvertFloat4ToU32(color),
        thickness
    );
    drawList->AddLine(
        ImVec2(bottomRight.x, bottomRight.y),
        ImVec2(bottomRight.x, bottomRight.y - cornerLength),
        ImGui::ColorConvertFloat4ToU32(color),
        thickness
    );
}

// Helper to draw a section that looks like a nested window
void DrawSectionBorder(const ImVec2& min, const ImVec2& max, const ImVec4& color, float thickness = 1.0f, float rounding = 4.0f) {
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    // Draw the main border with rounded corners to give it a window-like appearance
    drawList->AddRect(
        min,
        max,
        ImGui::ColorConvertFloat4ToU32(color),
        rounding,
        0, // No specific corner flags, draw all corners
        thickness
    );

    // Optional: Add a subtle background to enhance the window appearance
    ImVec4 bgColor = ImVec4(0.05f, 0.05f, 0.05f, 0.3f); // Subtle dark background
    drawList->AddRectFilled(
        ImVec2(min.x + 1, min.y + 1),
        ImVec2(max.x - 1, max.y - 1),
        ImGui::ColorConvertFloat4ToU32(bgColor),
        rounding - 1.0f
    );
}

// Function to draw a customized border for the main window with RGB effects
void DrawMainWindowBorder()
{
    ImVec2 windowPos = ImGui::GetWindowPos();
    ImVec2 windowSize = ImGui::GetWindowSize();
    ImVec2 windowEnd = ImVec2(windowPos.x + windowSize.x, windowPos.y + windowSize.y);

    // Draw the main border outline
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    // Get the current accent color based on mode
    ImVec4 borderColor;
    if (rgbModeEnabled) {
        float timeOffset = time_since_start * rgbCycleSpeed;
        float r = 0.5f + 0.5f * sinf(timeOffset);
        float g = 0.5f + 0.5f * sinf(timeOffset + 2.0f * MY_PI / 3.0f);
        float b = 0.5f + 0.5f * sinf(timeOffset + 4.0f * MY_PI / 3.0f);
        borderColor = ImVec4(r, g, b, 1.0f);
    }
    else {
        // Use main color with subtle pulsating effect
        float pulse = AnimateSin(1.5f, 0.9f, 1.0f, 0.0f);
        borderColor = ImVec4(main_color.x * pulse, main_color.y * pulse, main_color.z * pulse, 1.0f);
    }

    // Use absolutely no corner radius to ensure clean edges
    float thickness = 1.0f;

    // Draw four separate lines instead of a rectangle to avoid corner issues
    // Top line
    drawList->AddLine(
        ImVec2(windowPos.x, windowPos.y),
        ImVec2(windowEnd.x - 1, windowPos.y),
        ImGui::ColorConvertFloat4ToU32(borderColor),
        thickness
    );

    // Right line
    drawList->AddLine(
        ImVec2(windowEnd.x - 1, windowPos.y),
        ImVec2(windowEnd.x - 1, windowEnd.y - 1),
        ImGui::ColorConvertFloat4ToU32(borderColor),
        thickness
    );

    // Bottom line
    drawList->AddLine(
        ImVec2(windowEnd.x - 1, windowEnd.y - 1),
        ImVec2(windowPos.x, windowEnd.y - 1),
        ImGui::ColorConvertFloat4ToU32(borderColor),
        thickness
    );

    // Left line
    drawList->AddLine(
        ImVec2(windowPos.x, windowEnd.y - 1),
        ImVec2(windowPos.x, windowPos.y),
        ImGui::ColorConvertFloat4ToU32(borderColor),
        thickness
    );
}

// Helper to draw a section header with clean styling
void SectionHeader(const char* label) {
    ImGui::Spacing();
    ImGui::Spacing();

    // Get color to use
    ImVec4 headerColor;
    if (rgbModeEnabled) {
        // Calculate RGB colors based on time
        float timeOffset = time_since_start * rgbCycleSpeed;
        float r = 0.5f + 0.5f * sinf(timeOffset);
        float g = 0.5f + 0.5f * sinf(timeOffset + 2.0f * MY_PI / 3.0f);
        float b = 0.5f + 0.5f * sinf(timeOffset + 4.0f * MY_PI / 3.0f);

        headerColor = ImVec4(r, g, b, 1.0f);
    }
    else {
        float pulse = AnimateSin(1.5f, 0.7f, 1.0f, 0.0f);
        headerColor = ImVec4(main_color.x * pulse, main_color.y * pulse, main_color.z * pulse, 1.0f);
    }

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    float width = ImGui::GetWindowWidth() - 30.0f; // Padding on both sides

    // Calculate text dimensions and positioning
    ImVec2 textSize = ImGui::CalcTextSize(label);
    float textX = (width - textSize.x) / 2.0f + 15.0f; // Center the text with the padding

    // Starting position for drawing
    ImVec2 startPos = ImGui::GetCursorScreenPos();
    float separatorY = startPos.y + textSize.y / 2.0f;

    // Draw gradient background for header
    float headerHeight = textSize.y + 8.0f;
    ImVec4 bgColor = ImVec4(0.12f, 0.12f, 0.12f, 0.8f);
    ImVec4 bgColorDarker = ImVec4(0.08f, 0.08f, 0.08f, 0.9f);

    // Gradient background with rounded corners
    drawList->AddRectFilledMultiColor(
        ImVec2(startPos.x + 8.0f, startPos.y - 2.0f),
        ImVec2(startPos.x + width - 8.0f, startPos.y + headerHeight),
        ImGui::ColorConvertFloat4ToU32(bgColorDarker),
        ImGui::ColorConvertFloat4ToU32(bgColorDarker),
        ImGui::ColorConvertFloat4ToU32(bgColor),
        ImGui::ColorConvertFloat4ToU32(bgColor)
    );

    // Add subtle border around the header
    ImVec4 borderColor = headerColor;
    borderColor.w = 0.7f;
    drawList->AddRect(
        ImVec2(startPos.x + 8.0f, startPos.y - 2.0f),
        ImVec2(startPos.x + width - 8.0f, startPos.y + headerHeight),
        ImGui::ColorConvertFloat4ToU32(borderColor),
        4.0f, ImDrawFlags_RoundCornersAll, 1.0f
    );

    // Draw left separator line
    float lineWidth = (textX - 20.0f) * 0.8f;
    if (lineWidth > 0) {
        drawList->AddRectFilledMultiColor(
            ImVec2(startPos.x + 15.0f, separatorY),
            ImVec2(startPos.x + 15.0f + lineWidth, separatorY + 1.0f),
            ImGui::ColorConvertFloat4ToU32(ImVec4(headerColor.x, headerColor.y, headerColor.z, 0.0f)),
            ImGui::ColorConvertFloat4ToU32(headerColor),
            ImGui::ColorConvertFloat4ToU32(headerColor),
            ImGui::ColorConvertFloat4ToU32(ImVec4(headerColor.x, headerColor.y, headerColor.z, 0.0f))
        );
    }

    // Draw right separator line
    float rightLineStart = textX + textSize.x + 5.0f;
    float rightLineWidth = (width - (rightLineStart - startPos.x)) * 0.8f;
    if (rightLineWidth > 0) {
        drawList->AddRectFilledMultiColor(
            ImVec2(rightLineStart, separatorY),
            ImVec2(rightLineStart + rightLineWidth, separatorY + 1.0f),
            ImGui::ColorConvertFloat4ToU32(headerColor),
            ImGui::ColorConvertFloat4ToU32(ImVec4(headerColor.x, headerColor.y, headerColor.z, 0.0f)),
            ImGui::ColorConvertFloat4ToU32(ImVec4(headerColor.x, headerColor.y, headerColor.z, 0.0f)),
            ImGui::ColorConvertFloat4ToU32(headerColor)
        );
    }

    // Ensure proper spacing for the background
    ImGui::Dummy(ImVec2(1.0f, 2.0f));

    // Draw centered header text with subtle shadow for depth
    ImVec4 shadowColor = ImVec4(0.0f, 0.0f, 0.0f, 0.5f);
    drawList->AddText(
        ImVec2(startPos.x + textX + 1.0f, startPos.y + 4.0f + 1.0f),
        ImGui::ColorConvertFloat4ToU32(shadowColor),
        label
    );

    drawList->AddText(
        ImVec2(startPos.x + textX, startPos.y + 4.0f),
        ImGui::ColorConvertFloat4ToU32(headerColor),
        label
    );

    // Add padding after the header
    ImGui::Dummy(ImVec2(1.0f, headerHeight + 2.0f));
    ImGui::Spacing();
}

// Close the section with a bottom border
void DrawSectionEnd() {
    // Empty function to avoid compilation errors - will be removed
    ImGui::Spacing();
}

// Helper to draw sliders with consistent format
void DrawSlider(const char* label, float* value, float min, float max, const char* tooltip = nullptr) {
    // Use our professional animated slider instead of the basic implementation
    AnimatedProfessionalSlider(label, value, min, max, "%.1f", tooltip);
}

// Simple band-pass filter coefficients for EQ
struct BandPassFilter {
    float a0 = 0.0f, a1 = 0.0f, a2 = 0.0f, b1 = 0.0f, b2 = 0.0f;
    float x1, x2, y1, y2;
    float gain = 1.0f; // Added gain control

    BandPassFilter() : x1(0), x2(0), y1(0), y2(0) {}

    void reset() {
        x1 = x2 = y1 = y2 = 0;
    }

    float process(float sample) {
        // Direct form II biquad filter
        float result = a0 * sample + a1 * x1 + a2 * x2 - b1 * y1 - b2 * y2;
        x2 = x1;
        x1 = sample;
        y2 = y1;
        y1 = result;
        return result * gain; // Apply gain factor
    }
};

// Define EQ filters
BandPassFilter bassFilter, midFilter, highFilter;
BandPassFilter deesingFilter; // Add de-essing filter to reduce harsh S sounds

// Replace MultiTapEcho with FreeverbReverb - based on Freeverb algorithm
class FreeverbReverb {
private:
    // Constants for the Freeverb algorithm
    static constexpr int NUM_COMBS = 8;
    static constexpr int NUM_ALLPASSES = 4;
    static constexpr float FIXED_GAIN = 0.015f;
    static constexpr float SCALE_WET = 3.0f;
    static constexpr float SCALE_DRY = 2.0f;
    static constexpr float SCALE_DAMP = 0.4f;
    static constexpr float SCALE_ROOM = 0.28f;
    static constexpr float OFFSET_ROOM = 0.7f;
    static constexpr float INITIAL_ROOM = 0.5f;
    static constexpr float INITIAL_DAMP = 0.5f;
    static constexpr float INITIAL_WET = 1.0f / SCALE_WET;
    static constexpr float INITIAL_DRY = 0.0f;
    static constexpr float INITIAL_WIDTH = 1.0f;
    static constexpr float INITIAL_MODE = 0.0f;
    static constexpr float FREEZE_MODE = 0.5f;

    // Internal buffer sizes (adjust these if needed for performance)
    static constexpr int STEREO_SPREAD = 23;

    // Comb filter tunings for 44.1kHz (will be adjusted for actual sample rate)
    static constexpr int COMB_TUNING_L[NUM_COMBS] = { 1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617 };
    static constexpr int ALLPASS_TUNING_L[NUM_ALLPASSES] = { 556, 441, 341, 225 };

    // Comb filter implementation
    struct Comb {
        float* buffer = nullptr;
        int bufsize = 0;
        int bufidx = 0;
        float feedback = 0.0f;
        float filterstore = 0.0f;
        float damp1 = 0.0f;
        float damp2 = 0.0f;

        ~Comb() {
            if (buffer) delete[] buffer;
            buffer = nullptr;
        }

        void init(int size) {
            bufsize = size;
            bufidx = 0;
            filterstore = 0.0f;

            if (buffer) delete[] buffer;
            buffer = new float[size]();  // Zero-initialize
        }

        inline float process(float input) {
            float output = buffer[bufidx];
            filterstore = (output * damp2) + (filterstore * damp1);

            buffer[bufidx] = input + (filterstore * feedback);
            if (++bufidx >= bufsize) bufidx = 0;

            return output;
        }

        void mute() {
            filterstore = 0.0f;
            if (buffer) {
                memset(buffer, 0, bufsize * sizeof(float));
            }
        }

        void setdamp(float val) {
            damp1 = val;
            damp2 = 1.0f - val;
        }

        void setfeedback(float val) {
            feedback = val;
        }
    };

    // Allpass filter implementation
    struct Allpass {
        float* buffer = nullptr;
        int bufsize = 0;
        int bufidx = 0;
        float feedback = 0.5f;

        ~Allpass() {
            if (buffer) delete[] buffer;
            buffer = nullptr;
        }

        void init(int size) {
            bufsize = size;
            bufidx = 0;

            if (buffer) delete[] buffer;
            buffer = new float[size]();  // Zero-initialize
        }

        inline float process(float input) {
            float output = buffer[bufidx];
            buffer[bufidx] = input + (output * feedback);
            if (++bufidx >= bufsize) bufidx = 0;

            return output - input;
        }

        void mute() {
            if (buffer) {
                memset(buffer, 0, bufsize * sizeof(float));
            }
        }

        void setfeedback(float val) {
            feedback = val;
        }
    };

    // Filter arrays
    Comb combL[NUM_COMBS];
    Comb combR[NUM_COMBS];
    Allpass allpassL[NUM_ALLPASSES];
    Allpass allpassR[NUM_ALLPASSES];

    // Control parameters
    float gain = FIXED_GAIN;
    float roomsize = INITIAL_ROOM * SCALE_ROOM + OFFSET_ROOM;
    float damp = INITIAL_DAMP * SCALE_DAMP;
    float wet = INITIAL_WET * SCALE_WET;
    float wet1 = INITIAL_WIDTH / 2.0f * wet;
    float wet2 = (1.0f - INITIAL_WIDTH) / 2.0f * wet;
    float dry = INITIAL_DRY * SCALE_DRY;
    float width = INITIAL_WIDTH;
    float mode = INITIAL_MODE;

    // Sample rate and other state
    int sampleRate = 48000;
    int channels = 2;
    bool initialized = false;
    float sampleRateRatio = 1.0f;  // Used to adjust buffer sizes for different sample rates

public:
    FreeverbReverb() = default;

    ~FreeverbReverb() {
        // Cleanup handled by destructors of Comb and Allpass
    }

    void init(int rate, int numChannels) {
        // Set state
        sampleRate = rate;
        channels = numChannels;
        initialized = false;

        // Calculate sample rate ratio compared to 44.1kHz
        sampleRateRatio = (float)sampleRate / 44100.0f;

        try {
            // Initialize comb filters with adjusted sizes for sample rate
            for (int i = 0; i < NUM_COMBS; i++) {
                int adjustedSize = (int)(COMB_TUNING_L[i] * sampleRateRatio);
                if (adjustedSize < 10) adjustedSize = 10;  // Safety

                combL[i].init(adjustedSize);
                combR[i].init(adjustedSize + STEREO_SPREAD);
            }

            // Initialize allpass filters with adjusted sizes for sample rate
            for (int i = 0; i < NUM_ALLPASSES; i++) {
                int adjustedSize = (int)(ALLPASS_TUNING_L[i] * sampleRateRatio);
                if (adjustedSize < 10) adjustedSize = 10;  // Safety

                allpassL[i].init(adjustedSize);
                allpassR[i].init(adjustedSize + STEREO_SPREAD);

                allpassL[i].setfeedback(0.5f);
                allpassR[i].setfeedback(0.5f);
            }

            // Set default parameters
            updateParams(0.8f, 0.2f, 1.0f, 0.5f);
            mute();

            initialized = true;
        }
        catch (...) {
            initialized = false;
        }
    }

    // Update all reverb parameters at once
    void updateParams(float size, float dampening, float reverbWidth, float mix) {
        if (!initialized) return;

        setRoomSize(size);
        setDamp(dampening);
        setWidth(reverbWidth);
        setWet(mix);
        setDry(1.0f - mix);
    }

    // Process a block of audio
    void process(float* inBuffer, int numSamples) {
        if (!initialized || !inBuffer || numSamples <= 0) return;

        // If mono
        if (channels == 1) {
            for (int i = 0; i < numSamples; i++) {
                float input = inBuffer[i] * gain;
                float outL = 0.0f;
                float outR = 0.0f;

                // Process comb filters in parallel
                for (int j = 0; j < NUM_COMBS; j++) {
                    outL += combL[j].process(input);
                    outR += combR[j].process(input);
                }

                // Process allpass filters in series
                for (int j = 0; j < NUM_ALLPASSES; j++) {
                    outL = allpassL[j].process(outL);
                    outR = allpassR[j].process(outR);
                }

                // Calculate stereo output
                inBuffer[i] = outL * wet1 + outR * wet2 + inBuffer[i] * dry;
            }
        }
        // If stereo
        else if (channels >= 2) {
            for (int i = 0; i < numSamples; i++) {
                float inputL = inBuffer[i * 2] * gain;
                float inputR = inBuffer[i * 2 + 1] * gain;
                float outL = 0.0f;
                float outR = 0.0f;

                // Process comb filters in parallel
                for (int j = 0; j < NUM_COMBS; j++) {
                    outL += combL[j].process(inputL);
                    outR += combR[j].process(inputR);
                }

                // Process allpass filters in series
                for (int j = 0; j < NUM_ALLPASSES; j++) {
                    outL = allpassL[j].process(outL);
                    outR = allpassR[j].process(outR);
                }

                // Calculate stereo output with cross-feed
                float outL2 = outL * wet1 + outR * wet2;
                float outR2 = outR * wet1 + outL * wet2;

                inBuffer[i * 2] = outL2 + inputL * dry;
                inBuffer[i * 2 + 1] = outR2 + inputR * dry;
            }
        }
    }

    // Set room size (affects feedback of comb filters)
    void setRoomSize(float value) {
        if (!initialized) return;

        roomsize = value * SCALE_ROOM + OFFSET_ROOM;

        for (int i = 0; i < NUM_COMBS; i++) {
            combL[i].setfeedback(roomsize);
            combR[i].setfeedback(roomsize);
        }
    }

    // Set damping factor
    void setDamp(float value) {
        if (!initialized) return;

        damp = value * SCALE_DAMP;

        for (int i = 0; i < NUM_COMBS; i++) {
            combL[i].setdamp(damp);
            combR[i].setdamp(damp);
        }
    }

    // Set wet level (reverb amount)
    void setWet(float value) {
        if (!initialized) return;

        wet = value * SCALE_WET;
        updateWetValues();
    }

    // Set dry level (original signal)
    void setDry(float value) {
        if (!initialized) return;

        dry = value * SCALE_DRY;
    }

    // Set stereo width
    void setWidth(float value) {
        if (!initialized) return;

        width = value;
        updateWetValues();
    }

    // Set freezing mode (infinite sustain)
    void setFreeze(bool freezeMode) {
        if (!initialized) return;

        if (freezeMode) {
            roomsize = 1.0f;
            damp = 0.0f;

            for (int i = 0; i < NUM_COMBS; i++) {
                combL[i].setfeedback(1.0f);
                combR[i].setfeedback(1.0f);
                combL[i].setdamp(0.0f);
                combR[i].setdamp(0.0f);
            }
        }
        else {
            // Restore previous values
            for (int i = 0; i < NUM_COMBS; i++) {
                combL[i].setfeedback(roomsize);
                combR[i].setfeedback(roomsize);
                combL[i].setdamp(damp);
                combR[i].setdamp(damp);
            }
        }
    }

    // Mute/reset all internal buffers
    void mute() {
        if (!initialized) return;

        for (int i = 0; i < NUM_COMBS; i++) {
            combL[i].mute();
            combR[i].mute();
        }

        for (int i = 0; i < NUM_ALLPASSES; i++) {
            allpassL[i].mute();
            allpassR[i].mute();
        }
    }

private:
    // Update wet1 and wet2 values based on width
    void updateWetValues() {
        wet1 = wet * (width / 2.0f + 0.5f);
        wet2 = wet * ((1.0f - width) / 2.0f);
    }
};

// Global reverb processor
FreeverbReverb reverbProcessor;

// Initialize EQ filters with appropriate coefficients
void InitEQFilters() {
    // Bass filter (lowpass, cutoff ~200Hz at 48kHz sample rate) - EXTREMELY POWERFUL
    bassFilter.a0 = 0.025f;  // Increased for stronger bass response
    bassFilter.a1 = 0.050f;  // Increased for stronger bass response
    bassFilter.a2 = 0.025f;  // Increased for stronger bass response
    bassFilter.b1 = -1.70f;  // Adjusted for more resonance
    bassFilter.b2 = 0.80f;   // Increased for stronger bass effect
    bassFilter.gain = 2.5f;  // Dramatically increased from 1.2f for massive bass impact

    // Mid filter (bandpass, center ~1kHz at 48kHz sample rate) - EXTREMELY POWERFUL
    // Wider Q and stronger gain for much more dramatic mid range boost
    midFilter.a0 = 0.15f;    // Increased from 0.10f for stronger mid response
    midFilter.a1 = 0.0f;
    midFilter.a2 = -0.15f;   // Increased for stronger mid response
    midFilter.b1 = -1.80f;   // Adjusted for more presence
    midFilter.b2 = 0.85f;    // Increased for more resonance
    midFilter.gain = 2.2f;   // Dramatically increased from 1.0f for massive mid range impact

    // High filter (highpass, extremely aggressive slope, cutoff ~4.5kHz) - EXTREMELY POWERFUL
    // Ultra aggressive slope for dramatically pronounced high frequency enhancement
    highFilter.a0 = 0.50f;   // Increased from 0.45f for stronger high frequency presence 
    highFilter.a1 = -0.87f;  // Adjusted from -0.90f for more upper harmonic content
    highFilter.a2 = 0.50f;   // Increased from 0.45f for stronger high frequency presence
    highFilter.b1 = -0.87f;  // Adjusted from -0.90f for more upper harmonic content
    highFilter.b2 = 0.45f;   // Decreased from 0.50f for less filtering of high frequencies
    highFilter.gain = 2.6f;  // Increased from 2.4f for stronger high frequency presence

    // De-essing filter (notch around 6-8kHz where sibilance occurs)
    // More aggressive de-essing filter parameters for stronger S reduction
    deesingFilter.a0 = 0.87f;  // Reduced from 0.94f to make less aggressive on non-S sounds
    deesingFilter.a1 = -1.65f; // Adjusted from -1.82f for narrower notch to prevent muffling
    deesingFilter.a2 = 0.87f;  // Reduced from 0.94f to make less aggressive on non-S sounds
    deesingFilter.b1 = -1.65f; // Adjusted from -1.82f for narrower notch to prevent muffling
    deesingFilter.b2 = 0.85f;  // Increased from 0.78f for less aggressive filtering
    deesingFilter.gain = 0.75f; // Increased from 0.60f to preserve high frequencies

    // Initialize reverb processor with default settings
    reverbProcessor.init(48000, 2); // 48kHz stereo
}

// Format panning value to string safely to prevent crashes
const char* FormatPanningText(float value) {
    static char buffer[32];

    // Make the display cleaner and more intuitive
    if (value < -0.05f) {
        // Left panning (negative values)
        // Convert to percentage (0-100%) for display
        float percentage = -value * 10.0f;
        percentage = Min(percentage, 100.0f); // Cap at 100%
        snprintf(buffer, sizeof(buffer), "L %.0f%%", percentage);
    }
    else if (value > 0.05f) {
        // Right panning (positive values)
        // Convert to percentage (0-100%) for display
        float percentage = value * 10.0f;
        percentage = Min(percentage, 100.0f); // Cap at 100%
        snprintf(buffer, sizeof(buffer), "R %.0f%%", percentage);
    }
    else {
        // Center (near zero)
        snprintf(buffer, sizeof(buffer), "CENTER");
    }

    return buffer;
}

// Safe audio processing that handles stereo properly
void ApplyAudioEffects(float* audioBuffer, int bufferSize, int channels) {
    // Input validation to prevent crashes
    if (!audioBuffer || bufferSize <= 0 || channels <= 0) {
        return;
    }

    // Static variables for smoothing parameter changes
    static float prevBassEQ = 0.0f;
    static float prevMidEQ = 0.0f;
    static float prevHighEQ = 0.0f;
    static float prevGain = 1.0f;
    static float prevExpGain = 1.0f;
    static float prevVunitsGain = 1.0f;
    static bool prevBassBoostEnabled = false;

    // Smoothing factor - higher values = faster transitions
    const float smoothingFactor = 0.2f;

    // Smoothly interpolate parameters to prevent audio artifacts
    float smoothBassEQ = prevBassEQ + smoothingFactor * (bassEQ - prevBassEQ);
    float smoothMidEQ = prevMidEQ + smoothingFactor * (midEQ - prevMidEQ);
    float smoothHighEQ = prevHighEQ + smoothingFactor * (highEQ - prevHighEQ);
    float smoothGain = prevGain + smoothingFactor * (Gain - prevGain);
    float smoothExpGain = prevExpGain + smoothingFactor * (ExpGain - prevExpGain);
    float smoothVunitsGain = prevVunitsGain + smoothingFactor * (VunitsGain - prevVunitsGain);

    // Store for next buffer
    prevBassEQ = smoothBassEQ;
    prevMidEQ = smoothMidEQ;
    prevHighEQ = smoothHighEQ;
    prevGain = smoothGain;
    prevExpGain = smoothExpGain;
    prevVunitsGain = smoothVunitsGain;

    // Reset filters for this buffer
    bassFilter.reset();
    midFilter.reset();
    highFilter.reset();
    deesingFilter.reset();

    // Create temporary buffer for processing
    float* processedBuffer = nullptr;
    try {
        processedBuffer = new float[bufferSize * channels];
        if (!processedBuffer) {
            return; // Memory allocation failed
        }

        // First copy the original buffer
        memcpy(processedBuffer, audioBuffer, bufferSize * channels * sizeof(float));

        // Pre-processing safety check: scan for extreme values and scale down if needed
        float maxAmplitude = 0.0f;
        for (int i = 0; i < bufferSize * channels; i++) {
            float absValue = fabsf(processedBuffer[i]);
            if (absValue > maxAmplitude) {
                maxAmplitude = absValue;
            }
        }

        // If input is extremely loud, apply pre-attenuation to avoid overflow
        float safetyScale = 1.0f;
        if (maxAmplitude > 0.9f) {
            safetyScale = 0.9f / maxAmplitude;
            for (int i = 0; i < bufferSize * channels; i++) {
                processedBuffer[i] *= safetyScale;
            }
        }

        // Apply EQ separately to maintain independent control
        for (int i = 0; i < bufferSize; i++) {
            // Calculate per-sample smoothing for effect transitions
            float interpolationFactor = static_cast<float>(i) / bufferSize;
            float bassBoostTransition = 0.0f;

            // If bass boost state changed, gradually apply it across the buffer
            if (bassBoostEnabled != prevBassBoostEnabled) {
                bassBoostTransition = bassBoostEnabled ? interpolationFactor : (1.0f - interpolationFactor);
            }
            else {
                bassBoostTransition = bassBoostEnabled ? 1.0f : 0.0f;
            }

            for (int ch = 0; ch < channels; ch++) {
                int idx = i * channels + ch;

                // Make a copy of the original sample
                float original = processedBuffer[idx];

                // Scale down bass EQ effect based on the input level to prevent overloading
                // Higher input levels get less aggressive bass to prevent clipping
                float dynamicBassScale = 1.0f;
                float absInput = fabsf(original);
                if (absInput > 0.3f) {
                    // Progressive scaling - reduce bass effect as input level increases
                    dynamicBassScale = 1.0f - ((absInput - 0.3f) / 0.7f) * 0.7f;
                    dynamicBassScale = Max(0.3f, dynamicBassScale); // Ensure at least 30% of effect remains
                }

                // Apply each EQ band and scale by the slider value - ULTRA POWERFUL scaling
                // Add safety limiter for bass processing but allow more extreme values
                float bassInputSafe = Max(-0.97f, Min(0.97f, original)); // Prevent extremely extreme inputs
                // More aggressive scaling for the dramatically increased max value (70 instead of 30)
                // Use custom curve to make the effect more dramatic at higher values
                float bassEQScaled = (smoothBassEQ / 25.0f) * (1.0f + (smoothBassEQ / 70.0f));
                float bassOut = bassFilter.process(bassInputSafe) * bassEQScaled * dynamicBassScale;

                // Apply bass boost if enabled (ULTRA POWERFUL bass effect separate from EQ)
                if (bassBoostEnabled) {
                    // Apply a much more aggressive bass boost with smooth transition
                    float deepBassInput = Max(-0.95f, Min(0.95f, original)); // Moderate input limiting
                    // Double-process for extreme resonance and apply stronger gain
                    float extremeBass = bassFilter.process(bassFilter.process(deepBassInput)) * 1.5f; // Increased from 1.0f

                    // Less attenuation for more consistent rumble
                    float boostAttenuationFactor = 1.0f - Min(0.8f, absInput * 0.6f);
                    extremeBass *= boostAttenuationFactor;

                    // Apply tanh limiting with higher ceiling to allow more extreme values
                    float safeBassBoost = tanh(extremeBass * 0.5f) * 0.5f; // Increased from 0.3f

                    // Apply the transition smoothly to avoid clicks/pops
                    bassOut += (extremeBass + safeBassBoost) * bassBoostTransition * dynamicBassScale * 1.5f; // Extra 1.5x boost
                }

                // Hard limit bass to prevent catastrophic overflow but allow more extreme values
                bassOut = Max(-2.0f, Min(2.0f, bassOut)); // Increased from -1.5/1.5 to -2.0/2.0

                // Apply mid frequencies with more aggressive scaling for the higher max value
                float midInputSafe = Max(-0.97f, Min(0.97f, original)); // Slightly less limiting for more punch
                // Custom mid curve for more dramatic effect at higher values
                float midEQScaled = (smoothMidEQ / 25.0f) * (1.0f + (smoothMidEQ / 80.0f));
                float midOut = midFilter.process(midInputSafe) * midEQScaled;

                // Hard limit mid to prevent overflow but allow more extreme values
                midOut = Max(-1.8f, Min(1.8f, midOut)); // Increased from -1.0/1.0 to -1.8/1.8

                // Apply de-essing before high frequencies to reduce sibilance
                float highInputSafe = Max(-0.97f, Min(0.97f, original)); // Slightly less limiting
                float highPassed = highFilter.process(highInputSafe);

                // Enhanced de-essing with dynamic response
                float deEssed = deesingFilter.process(highPassed); // De-ess the high frequencies

                // Apply a dramatically more powerful high frequency processing
                // Use an EXTREMELY pronounced non-linear scaling for massive effect at higher EQ values
                // Custom curve for extreme brightness without harshness
                float highEQScaled = (smoothHighEQ / 25.0f) * (1.0f + (smoothHighEQ / 60.0f));
                float highOut = deEssed * highEQScaled; // Much more aggressive scaling for higher max value

                // Apply intelligent limiter for high frequencies to prevent harshness but allow sparkle
                if (highOut > 1.2f) {
                    // More gradual soft-knee limiting to maintain some brightness while preventing harshness
                    highOut = 1.2f + (highOut - 1.2f) * 0.2f; // Less aggressive limiting for highs
                }
                else if (highOut < -1.2f) {
                    // More gradual soft-knee limiting to maintain some brightness while preventing harshness
                    highOut = -1.2f + (highOut + 1.2f) * 0.2f; // Less aggressive limiting for highs
                }

                // Apply a more aggressive mixing approach for maximum impact while still preserving some clarity
                float eq_mix = 0.85f; // 85% processed, 15% original signal for more power while maintaining clarity

                // Combine with safety against extreme values but allow more intensity
                float combined = original * (1.0f - eq_mix) + (original + bassOut + midOut + highOut) * eq_mix;

                // Hard limit the combined EQ output to prevent clipping but allow more extreme values
                combined = Max(-2.5f, Min(2.5f, combined)); // Increased from -2.0/2.0 to -2.5/2.5

                // More sophisticated limiting with tanh for smoother ceiling when EQ is at extreme values
                // Use a multi-stage approach to maintain more dynamics
                if (fabsf(combined) > 1.5f) {
                    // Very extreme values get more aggressive limiting
                    combined = 1.5f * tanh(combined / 1.5f);
                }
                else if (fabsf(combined) > 1.0f) {
                    // Moderate-high values get gentler limiting
                    float limitFactor = 0.2f + 0.8f * ((1.5f - fabsf(combined)) / 0.5f);
                    combined = combined * limitFactor + (combined > 0 ? 1.0f : -1.0f) * (1.0f - limitFactor);
                }

                processedBuffer[idx] = combined;
            }
        }

        // Update bass boost state for next buffer
        prevBassBoostEnabled = bassBoostEnabled;

        // Apply reverb (if enabled)
        if (reverbEnabled && reverbMix > 0.0f) {
            // Update reverb parameters (only when processing audio to avoid clicks)
            reverbProcessor.updateParams(reverbSize, reverbDamping, reverbWidth, reverbMix);

            // Process the audio through the reverb
            reverbProcessor.process(processedBuffer, bufferSize);
        }

        // We'll apply panning and in-head effects after gain processing

        // Apply energy effect with safety limiter
        if (energyEnabled) {
            float energyMod = sinf(time_since_start * 8.0f) * 0.3f + 0.8f;
            // Capped energy factor to prevent extreme values
            float energyFactor = 1.0f + Min(3.0f, (energyValue / 750000.0f)) * energyMod;

            for (int i = 0; i < bufferSize * channels; i++) {
                processedBuffer[i] *= energyFactor;

                // Safety limiter for energy effect
                if (processedBuffer[i] > 1.5f) {
                    processedBuffer[i] = 1.5f + (processedBuffer[i] - 1.5f) * 0.1f;
                }
                else if (processedBuffer[i] < -1.5f) {
                    processedBuffer[i] = -1.5f + (processedBuffer[i] + 1.5f) * 0.1f;
                }
            }
        }

        // Apply gain with per-sample smoothing to prevent clicks/pops
        float totalGain = smoothGain * smoothExpGain;

        // Apply vUnits as a decibel power gain
        float vUnitsDecibels = 20.0f * log10f(Max(0.0001f, smoothVunitsGain)); // Convert to dB, avoid log of 0
        float vUnitsMultiplier = powf(10.0f, vUnitsDecibels / 20.0f); // Convert dB back to gain multiplier

        // Enhanced power calculation for extreme boost with upper cap for safety
        vUnitsMultiplier = Min(vUnitsMultiplier, 50000.0f); // Reduced from 100000.0f for less distortion

        // Special handling for combined VunitsGain and ExpGain (rage gain)
        // This specifically addresses the issue when both gains are combined
        if (smoothVunitsGain > 10.0f && smoothExpGain > 5.0f) {
            // Apply a progressive reduction factor based on how high both gains are
            float combinedReductionFactor = 1.0f - Min(0.5f, (smoothVunitsGain * smoothExpGain) / 2000.0f);
            vUnitsMultiplier *= combinedReductionFactor;

            // Apply additional safety cap specific to combined gain scenario
            vUnitsMultiplier = Min(vUnitsMultiplier, 30000.0f);
        }

        if (smoothVunitsGain > 1000.0f) {
            // Apply extra boost for very high values with exponential scaling and safety cap
            float extraBoost = Min(5.0f, powf((smoothVunitsGain - 1000.0f) / 6000.0f, 1.5f) * 1.5f + 1.0f);
            // Modified from 10.0f, 4000.0f, 2.0f, 2.0f to provide smoother curve with less distortion
            vUnitsMultiplier = Min(vUnitsMultiplier * extraBoost, 50000.0f); // Reduced from 100000.0f
        }

        // Safety cap on total gain to prevent crashes
        totalGain = Min(totalGain * vUnitsMultiplier, 250000.0f); // Reduced from 500000.0f for less distortion

        // Calculate strongest limiting for extreme gain values
        float limiterThreshold, limiterRatio, finalSafetyScale;
        if (totalGain > 10000.0f) {
            limiterThreshold = 0.25f; // Reduced from 0.4f to prevent distortion at high gain
            limiterRatio = 0.005f;    // Reduced from 0.01f for smoother limiting
            finalSafetyScale = 0.4f;  // Reduced from 0.5f for extra headroom
        }
        else if (totalGain > 1000.0f) {
            limiterThreshold = 0.35f; // Reduced from 0.6f
            limiterRatio = 0.015f;    // Reduced from 0.03f
            finalSafetyScale = 0.55f; // Reduced from 0.7f
        }
        else if (totalGain > 100.0f) {
            limiterThreshold = 0.45f; // Reduced from 0.7f
            limiterRatio = 0.03f;     // Reduced from 0.05f
            finalSafetyScale = 0.7f;  // Reduced from 0.8f
        }
        else if (totalGain > 50.0f) {
            limiterThreshold = 0.6f;  // Reduced from 0.8f
            limiterRatio = 0.05f;     // Reduced from 0.1f
            finalSafetyScale = 0.8f;  // Reduced from 0.9f
        }
        else {
            limiterThreshold = 0.8f;  // Reduced from 0.9f
            limiterRatio = 0.1f;      // Reduced from 0.15f
            finalSafetyScale = 1.0f;
        }

        for (int i = 0; i < bufferSize; i++) {
            // Create smoother gain transition throughout the buffer
            // This helps especially when gain is first applied
            float gainInterpolationFactor = static_cast<float>(i) / bufferSize;
            float frameGain = totalGain; // Default full gain

            // If total gain is above threshold, apply it gradually across the buffer
            // This prevents the "pixel" sound when high gain is first applied
            if (totalGain > 20.0f) {
                // Apply gain smoothing at beginning of buffer, more natural ramp-up
                float smoothStartRatio = 0.1f + 0.9f * gainInterpolationFactor;
                frameGain *= smoothStartRatio;
            }

            for (int ch = 0; ch < channels; ch++) {
                int idx = i * channels + ch;
                // Apply a progressive soft-knee limiter BEFORE applying extreme gain
                // This prevents harsh clipping and distortion
                float sample = processedBuffer[idx];

                // Apply a sequence of progressively stricter limiters for proper gain control
                // First gentle compression to tame initial peaks
                if (sample > 0.4f) { // Reduced from 0.5f for more aggressive limiting
                    float excess = sample - 0.4f;
                    sample = 0.4f + excess * 0.7f; // Reduced from 0.8f for stronger compression
                }
                else if (sample < -0.4f) { // Reduced from -0.5f
                    float excess = -0.4f - sample;
                    sample = -0.4f - excess * 0.7f; // Reduced from 0.8f
                }

                // Apply a second compression stage for stronger limiting
                if (sample > 0.7f) { // Reduced from 0.8f
                    float excess = sample - 0.7f;
                    sample = 0.7f + excess * 0.4f; // Reduced from 0.5f for stronger compression
                }
                else if (sample < -0.7f) { // Reduced from -0.8f
                    float excess = -0.7f - sample;
                    sample = -0.7f - excess * 0.4f; // Reduced from 0.5f
                }

                // Additional third-stage limiting for extreme high gain scenarios
                if (totalGain > 200.0f) {
                    if (sample > 0.85f) {
                        float excess = sample - 0.85f;
                        sample = 0.85f + excess * 0.2f; // Very aggressive limiting for highest peaks
                    }
                    else if (sample < -0.85f) {
                        float excess = -0.85f - sample;
                        sample = -0.85f - excess * 0.2f;
                    }
                }

                // For high gain values, apply additional de-essing before the gain
                // This specifically targets the sibilance (S sounds) that causes distortion at high gain
                if (totalGain > 60.0f) {
                    // Calculate how much extra de-essing to apply based on gain
                    float deEssFactor = Min(1.0f, (totalGain - 60.0f) / 60.0f); // 0 to 1 based on gain from 60 to 120

                    // Apply dynamic notch filtering to reduce high frequency content (mainly affects S sounds)
                    float highFreq = sample * 0.8f; // High pass approximation
                    float notchEffect = highFreq * deEssFactor * 0.6f;

                    // Reduce the sample amplitude in a frequency-dependent way
                    sample = sample - notchEffect;

                    // Add special handling when both VunitsGain and ExpGain are active together
                    // This addresses the specific distortion case mentioned by the user
                    if (smoothVunitsGain > 10.0f && smoothExpGain > 5.0f) {
                        // Calculate the combined gain factor to determine how aggressive to be
                        float combinedGainFactor = Min(1.0f, (smoothVunitsGain * smoothExpGain) / 500.0f);

                        // Create a multi-band approach specifically targeting "s" sounds (5-9kHz range)
                        // Use a simple but effective approach - detect rapid transients typical of sibilance
                        float currentAbsSample = fabsf(sample);
                        static float prevAbsSample = 0.0f;
                        static float prevDelta = 0.0f;

                        // Calculate rate of change - rapid positive change is characteristic of "s" sounds
                        float delta = currentAbsSample - prevAbsSample;
                        bool isTransient = (delta > 0.02f && delta > prevDelta * 1.2f);

                        // If we detect a pattern that looks like an "s" sound and we're using high combined gain, apply more reduction
                        if (isTransient) {
                            // Apply additional targeted reduction on potential "s" sounds
                            float sReduction = combinedGainFactor * 0.4f * currentAbsSample;
                            if (sample > 0) {
                                sample -= sReduction;
                            }
                            else {
                                sample += sReduction;
                            }

                            // Apply a gentle brick-wall limit specifically for these transients
                            float transientLimit = 0.85f - (0.2f * combinedGainFactor);
                            if (sample > transientLimit) {
                                sample = transientLimit + (sample - transientLimit) * 0.2f;
                            }
                            else if (sample < -transientLimit) {
                                sample = -transientLimit + (sample + transientLimit) * 0.2f;
                            }
                        }

                        // Store values for next sample
                        prevAbsSample = currentAbsSample;
                        prevDelta = delta;
                    }
                }

                // Apply gain after limiting
                sample *= frameGain;

                // Apply additional safety scaling for extreme gain values
                sample *= finalSafetyScale;

                // Special clarity enhancement for rage gain (when ExpGain is high)
                if (smoothExpGain > 5.0f) {
                    // Add specific S sound handling for rage gain
                    // More aggressive when higher rage gain is used
                    float rageGainFactor = Min(1.0f, smoothExpGain / 100.0f);

                    // Detect S sound characteristics - sharp transients with high frequency content
                    // Use simple but effective envelope detection
                    static float sEnvelope = 0.0f;
                    float currentAbs = fabsf(sample);
                    float attackTime = 0.0008f; // Faster attack to catch only true S transients
                    float releaseTime = 0.05f;  // Faster release to avoid affecting adjacent sounds

                    // Simple envelope follower specifically tuned for S sounds
                    if (currentAbs > sEnvelope) {
                        sEnvelope = sEnvelope + attackTime * (currentAbs - sEnvelope);
                    }
                    else {
                        sEnvelope = sEnvelope + releaseTime * (currentAbs - sEnvelope);
                    }

                    // Detect potential S sound by looking for rapid rise in envelope
                    // Make more selective to avoid affecting non-S sounds
                    static float prevSEnvelope = 0.0f;
                    float sRise = sEnvelope - prevSEnvelope;
                    bool isPotentialS = (sRise > 0.05f) && (sEnvelope > 0.3f); // More selective threshold

                    // Apply specialized S sound reduction when rage gain is active
                    if (isPotentialS) {
                        // Calculate reduction amount based on rage gain level
                        // Less aggressive to prevent muffling
                        float sReduction = 0.2f * rageGainFactor * sEnvelope;

                        // Apply reduction with proper sign handling
                        if (sample > 0) {
                            sample -= sReduction;
                        }
                        else {
                            sample += sReduction;
                        }

                        // Apply gentler soft clipping focused on S frequencies
                        float softLimit = 0.85f - (0.2f * rageGainFactor); // Less aggressive limiting
                        if (sample > softLimit) {
                            sample = softLimit + (sample - softLimit) * 0.25f; // More gentle curve
                        }
                        else if (sample < -softLimit) {
                            sample = -softLimit + (sample + softLimit) * 0.25f; // More gentle curve
                        }
                    }

                    // Store envelope for next sample
                    prevSEnvelope = sEnvelope;

                    // Calculate clarity factor - increased with higher rage gain
                    float clarityFactor = Min(0.5f, (smoothExpGain - 5.0f) / 120.0f); // Increased from 0.3f for more clarity

                    // Presence boost - enhance mid frequencies for better speech intelligibility
                    // Boost upper mids more to prevent muffled sound
                    float presenceBoost = midFilter.process(sample) * 0.35f * clarityFactor; // Increased from 0.2f

                    // Apply subtle harmonic enhancement for clarity
                    // Add more harmonic content to compensate for de-essing
                    float harmonic = tanh(sample * 0.6f) * 0.25f * clarityFactor; // Increased from 0.4f and 0.15f

                    // Mix in the clarity enhancements - higher mix for better clarity
                    sample = sample * (1.0f - clarityFactor * 1.2f) + (sample + presenceBoost + harmonic) * clarityFactor * 1.2f;

                    // Dynamic range adjustment to preserve transients
                    float attackSharpness = Min(1.0f, fabs(sample * 2.5f)); // Reduced from 3.0f
                    sample *= (0.9f + attackSharpness * 0.1f); // Changed from 0.85f and 0.15f
                }

                // Apply an ultra-aggressive limiter after gain to prevent distortion while preserving clarity
                if (sample > limiterThreshold) {
                    float excess = sample - limiterThreshold;
                    // Hard limiting with extremely low ratio for extreme gain values
                    sample = limiterThreshold + excess * limiterRatio;
                }
                else if (sample < -limiterThreshold) {
                    float excess = -limiterThreshold - sample;
                    // Hard limiting with extremely low ratio for extreme gain values
                    sample = -limiterThreshold - excess * limiterRatio;
                }

                // Special case for rage gain to handle S sound distortion
                if (smoothExpGain > 20.0f) {
                    // Apply extra limiting specifically for high frequencies (S sounds)
                    // using a multi-band approach that focuses on sibilant range
                    float sibilantThreshold = limiterThreshold - (0.15f * Min(1.0f, (smoothExpGain - 20.0f) / 100.0f));

                    // Process through de-essing filter to detect S energy
                    float sBandEnergy = deesingFilter.process(sample * 0.4f);
                    float absEnergy = fabsf(sBandEnergy);

                    // If significant energy in S band, apply targeted limiting
                    if (absEnergy > 0.15f) {
                        float sLimitFactor = Min(0.8f, (absEnergy - 0.15f) * 4.0f);
                        float reductionAmount = sLimitFactor * (fabsf(sample) - sibilantThreshold) * 0.7f;

                        // Only apply if we're above the special threshold
                        if (reductionAmount > 0) {
                            if (sample > 0) {
                                sample -= reductionAmount;
                            }
                            else {
                                sample += reductionAmount;
                            }

                            // Add a tiny bit of high frequency energy back to prevent muffling
                            float compensationAmount = reductionAmount * 0.15f;
                            sample += (sample > 0) ? compensationAmount : -compensationAmount;
                        }
                    }
                }

                // Final safety clamp with soft tanh limiting for smoother ceiling
                // Make it more aggressive to prevent any possible distortion
                if (totalGain > 50.0f) {
                    // Apply a gentler tanh-based soft clipper for very high gain values
                    sample = 0.95f * tanh(sample * 0.9f); // Added to create softer limiting
                }
                else if (fabs(sample) > 0.95f) {
                    // Otherwise just apply normal soft clipping for samples near max
                    sample = 0.95f * (sample > 0 ? 1.0f : -1.0f) +
                        0.05f * sample; // Soft clip with linear component for more natural sound
                }

                // Absolute safety limit to prevent any crashes
                sample = sample > 10.0f ? 10.0f : (sample < -10.0f ? -10.0f : sample);

                // Store the processed sample with gain applied
                processedBuffer[idx] = sample;
            }
        }

        // Apply panning (for stereo only) AFTER gain processing for greater effect
        if (channels == 2 && audioChannelMode == 1) { // Only apply panning in stereo mode
            // Enhanced panning with stronger effect at high gain
            float panBoostFactor = 1.0f;
            if (totalGain > 50.0f) {
                // Progressively increase panning effect with higher gain
                panBoostFactor = 1.0f + Min(0.5f, (totalGain - 50.0f) / 500.0f);
            }

            // Use a more accurate panning law for better spatial positioning
            // Convert from -10.0 to 10.0 range to -1.0 to 1.0 range
            float normalizedPanning = panningValue / 10.0f;

            // Constant power panning law (square root) for more accurate stereo imaging
            float panAngle = (normalizedPanning + 1.0f) * (MY_PI / 4.0f); // Map -1..1 to 0..PI/2
            float leftGain = cosf(panAngle) * (1.0f + (panBoostFactor - 1.0f) * 0.5f);
            float rightGain = sinf(panAngle) * (1.0f + (panBoostFactor - 1.0f) * 0.5f);

            // Apply slight frequency-dependent adjustments for more natural panning
            // Calculate micro-delay for enhanced spatial cues (subtle HRTF simulation)
            float microDelay = fabsf(normalizedPanning) * 0.2f; // 0-0.2ms max

            // Previous sample buffer for inter-channel delay simulation
            static float prevLeftSample = 0.0f;
            static float prevRightSample = 0.0f;

            for (int i = 0; i < bufferSize; i++) {
                // Safe array access 
                int leftIdx = i * 2;
                int rightIdx = i * 2 + 1;

                if (leftIdx < bufferSize * channels && rightIdx < bufferSize * channels) {
                    float leftSample = processedBuffer[leftIdx];
                    float rightSample = processedBuffer[rightIdx];

                    // Apply constant power panning
                    float leftPanned = leftSample * leftGain;
                    float rightPanned = rightSample * rightGain;

                    // Add slight cross-feed for more natural stereo image
                    float crossfeedAmount = 0.1f * (1.0f - fabsf(normalizedPanning));
                    leftPanned += rightSample * crossfeedAmount;
                    rightPanned += leftSample * crossfeedAmount;

                    // Apply subtle inter-channel delay for enhanced spatial positioning
                    if (normalizedPanning < 0) { // Panning left
                        // Delay right channel slightly
                        rightPanned = rightPanned * (1.0f - microDelay) + prevRightSample * microDelay;
                    }
                    else if (normalizedPanning > 0) { // Panning right
                        // Delay left channel slightly
                        leftPanned = leftPanned * (1.0f - microDelay) + prevLeftSample * microDelay;
                    }

                    // Store current samples for next iteration's delay
                    prevLeftSample = leftSample;
                    prevRightSample = rightSample;

                    // Apply the more accurate panning
                    processedBuffer[leftIdx] = leftPanned;
                    processedBuffer[rightIdx] = rightPanned;
                }
            }
        }

        // Apply In-Head effects (for stereo only) AFTER gain processing for greater effect
        if (channels == 2 && audioChannelMode == 1) { // Only apply in-head effects in stereo mode
            // Enhanced in-head effect with stronger effect at high gain
            float inHeadBoostFactor = 1.5f;
            float inHeadReductionFactor = 0.6f;

            if (totalGain > 50.0f) {
                // Progressively increase in-head effect with higher gain
                inHeadBoostFactor = 1.5f + Min(0.5f, (totalGain - 50.0f) / 500.0f);
                inHeadReductionFactor = 0.6f - Min(0.2f, (totalGain - 50.0f) / 1000.0f);
            }

            // Create more accurate in-head spatial modeling
            // Use previous sample memory for phase manipulation
            static float leftDelayBuffer[8] = { 0 };
            static float rightDelayBuffer[8] = { 0 };
            const int delayBufferSize = 8;
            static int delayBufferIndex = 0;

            for (int i = 0; i < bufferSize; i++) {
                int leftIdx = i * 2;
                int rightIdx = i * 2 + 1;

                if (leftIdx < bufferSize * channels && rightIdx < bufferSize * channels) {
                    // Get current samples
                    float leftSample = processedBuffer[leftIdx];
                    float rightSample = processedBuffer[rightIdx];

                    // Store samples in delay buffer
                    leftDelayBuffer[delayBufferIndex] = leftSample;
                    rightDelayBuffer[delayBufferIndex] = rightSample;

                    // Calculate next buffer index (circular buffer)
                    int nextBufferIndex = (delayBufferIndex + 1) % delayBufferSize;

                    // In-Head Left: More accurate localization effect
                    if (inHeadLeft) {
                        // Apply primary boost with natural harmonics
                        float leftEarEffect = leftSample * inHeadBoostFactor;

                        // Add subtle bone conduction simulation (mid-range emphasis)
                        float boneConduction = (leftSample + rightSample) * 0.15f;
                        leftEarEffect += boneConduction;

                        // Apply frequency-dependent attenuation to the opposite ear (more accurate HRTF)
                        float rightEarEffect = rightSample * inHeadReductionFactor;

                        // Add slight delayed crossfeed for more natural spatial positioning
                        float delayedLeft = leftDelayBuffer[(delayBufferIndex + delayBufferSize - 3) % delayBufferSize];
                        rightEarEffect += delayedLeft * 0.05f;

                        // Apply proximity effect (bass boost on primary side)
                        // This simulates close-mic effect that happens in real earphones
                        float bassBoost = leftDelayBuffer[(delayBufferIndex + delayBufferSize - 2) % delayBufferSize] * 0.2f;
                        leftEarEffect += bassBoost;

                        // Set the processed samples
                        processedBuffer[leftIdx] = leftEarEffect;
                        processedBuffer[rightIdx] = rightEarEffect;
                    }

                    // In-Head Right: More accurate localization effect
                    else if (inHeadRight) {
                        // Apply primary boost with natural harmonics
                        float rightEarEffect = rightSample * inHeadBoostFactor;

                        // Add subtle bone conduction simulation (mid-range emphasis)
                        float boneConduction = (leftSample + rightSample) * 0.15f;
                        rightEarEffect += boneConduction;

                        // Apply frequency-dependent attenuation to the opposite ear (more accurate HRTF)
                        float leftEarEffect = leftSample * inHeadReductionFactor;

                        // Add slight delayed crossfeed for more natural spatial positioning
                        float delayedRight = rightDelayBuffer[(delayBufferIndex + delayBufferSize - 3) % delayBufferSize];
                        leftEarEffect += delayedRight * 0.05f;

                        // Apply proximity effect (bass boost on primary side)
                        // This simulates close-mic effect that happens in real earphones
                        float bassBoost = rightDelayBuffer[(delayBufferIndex + delayBufferSize - 2) % delayBufferSize] * 0.2f;
                        rightEarEffect += bassBoost;

                        // Set the processed samples
                        processedBuffer[leftIdx] = leftEarEffect;
                        processedBuffer[rightIdx] = rightEarEffect;
                    }

                    // Update delay buffer index
                    delayBufferIndex = nextBufferIndex;
                }
            }
        }

        // Copy back to original buffer
        memcpy(audioBuffer, processedBuffer, bufferSize * channels * sizeof(float));
    }
    catch (...) {
        // Handle any exceptions that might occur during processing
    }

    // Clean up
    if (processedBuffer) {
        delete[] processedBuffer;
    }
}

// Hook function for audio callbacks - this is what would be connected to the voice processing
extern "C" opus_int32 custom_opus_encode(OpusEncoder* st, const opus_int16* pcm, int frame_size,
    unsigned char* data, opus_int32 max_data_bytes) {
    // Input validation
    if (!st || !pcm || !data || frame_size <= 0 || max_data_bytes <= 0) {
        // If inputs are invalid, fall back to original opus_encode
        return opus_encode(st, pcm, frame_size, data, max_data_bytes);
    }

    // Get number of channels from the encoder - use constants directly instead of the missing macro
    int channels = 2; // Default to stereo

    // Using OPUS_GET_CHANNELS_REQUEST (1029) directly since the macro might not be available
    opus_int32 channels_i32 = 0;
    if (opus_encoder_ctl(st, 1029, &channels_i32) == OPUS_OK) {
        channels = (int)channels_i32;
    }

    try {
        // Set the bitrate using OPUS_SET_BITRATE_REQUEST (4002)
        // Make sure bitrateValue is within valid range
        int bitrate = static_cast<int>(bitrateValue);
        bitrate = Max(16000, Min(bitrate, 510000)); // Ensure value is within valid range
        opus_encoder_ctl(st, 4002, bitrate); // Set bitrate using Opus control interface

        // Set the channel mode (mono/stereo) using OPUS_SET_FORCE_CHANNELS (4022)
        // Convert from UI index (0 or 1) to actual channel count (1 or 2)
        int actualChannels = audioChannelMode + 1;
        opus_encoder_ctl(st, 4022, actualChannels);

        // Check if audio is silent or near-silent
        int total_samples = frame_size * channels;

        // Use RMS (Root Mean Square) to better detect silence
        double rms = 0.0;
        for (int i = 0; i < total_samples; i++) {
            rms += static_cast<double>(pcm[i]) * pcm[i];
        }
        rms = sqrt(rms / total_samples);

        // Silence detection with a somewhat higher threshold to catch very quiet audio too
        // This handles both explicit muting and natural silences
        bool is_silent = (rms < 30.0);

        // Store previous frame's silence state to detect transitions
        static bool was_silent_prev_frame = false;
        // Store our generated noise pattern to ensure smooth transitions
        static opus_int16* prev_noise_buffer = nullptr;
        static int prev_noise_size = 0;

        // If we're transitioning from sound to silence, we need to be extra careful
        bool is_transition = (is_silent != was_silent_prev_frame);

        // Handle all cases of silence - both explicit muting and natural pauses
        if (is_silent) {
            // Allocate or reuse noise buffer
            if (!prev_noise_buffer || prev_noise_size != total_samples) {
                // Clean up old buffer if it exists but size doesn't match
                if (prev_noise_buffer) {
                    delete[] prev_noise_buffer;
                }

                // Create new buffer
                prev_noise_buffer = new opus_int16[total_samples];
                prev_noise_size = total_samples;

                // Initialize with fresh noise pattern
                srand(static_cast<unsigned int>(time(nullptr)));
                for (int i = 0; i < total_samples; i++) {
                    // Extremely low amplitude noise (15-25 range) - just enough to keep the encoder happy
                    prev_noise_buffer[i] = 15 + (rand() % 10);
                    if (rand() % 2) {
                        prev_noise_buffer[i] = -prev_noise_buffer[i];
                    }
                }
            }

            // Create a copy for this frame (so we can modify it safely)
            opus_int16* noise_pcm = new opus_int16[total_samples];
            if (!noise_pcm) {
                return opus_encode(st, pcm, frame_size, data, max_data_bytes);
            }

            // If we're transitioning from sound to silence, blend the real signal with our noise
            if (is_transition && was_silent_prev_frame == false) {
                // Crossfade smoothly
                for (int i = 0; i < total_samples; i++) {
                    // Gradually fade from real signal to noise over the frame
                    float mix_ratio = static_cast<float>(i) / total_samples;
                    noise_pcm[i] = static_cast<opus_int16>(
                        pcm[i] * (1.0f - mix_ratio) + prev_noise_buffer[i] * mix_ratio
                        );
                }
            }
            else {
                // Just use our existing noise pattern with slight variations
                for (int i = 0; i < total_samples; i++) {
                    // Add tiny random variations to prevent encoder from getting "stuck"
                    int variation = (rand() % 5) - 2; // -2 to +2 variation
                    noise_pcm[i] = prev_noise_buffer[i] + variation;
                }
            }

            // Try encoding with the noise pattern
            opus_int32 result = opus_encode(st, noise_pcm, frame_size, data, max_data_bytes);

            // Save the noise buffer for next time if needed
            memcpy(prev_noise_buffer, noise_pcm, total_samples * sizeof(opus_int16));
            delete[] noise_pcm;

            // If primary approach fails, try fallback strategies
            if (result < 0) {
                // Strategy 2: Try with DTX enabled
                opus_encoder_ctl(st, 4016, 1); // OPUS_SET_DTX(1)
                result = opus_encode(st, pcm, frame_size, data, max_data_bytes);
                opus_encoder_ctl(st, 4016, 0); // OPUS_SET_DTX(0)

                if (result < 0) {
                    // Strategy 3: Try with constant DC values
                    opus_int16* dc_pcm = new opus_int16[total_samples];
                    if (!dc_pcm) {
                        return opus_encode(st, pcm, frame_size, data, max_data_bytes);
                    }

                    for (int i = 0; i < total_samples; i++) {
                        dc_pcm[i] = 64; // Very small constant value
                    }

                    result = opus_encode(st, dc_pcm, frame_size, data, max_data_bytes);
                    delete[] dc_pcm;
                }
            }

            // Update silence tracking
            was_silent_prev_frame = true;
            return result;
        }
        else {
            // Normal audio processing

            // If transitioning from silence to sound, do a gentle fade-in
            if (is_transition && was_silent_prev_frame && prev_noise_buffer) {
                // Create a temp buffer for the crossfade
                opus_int16* transition_pcm = new opus_int16[total_samples];
                if (transition_pcm) {
                    // Crossfade from noise to real audio
                    for (int i = 0; i < total_samples; i++) {
                        float mix_ratio = static_cast<float>(i) / total_samples;
                        transition_pcm[i] = static_cast<opus_int16>(
                            prev_noise_buffer[i] * (1.0f - mix_ratio) + pcm[i] * mix_ratio
                            );
                    }

                    // Process this crossfaded audio instead of the original
                    float* audioBuffer = new float[frame_size * channels];
                    if (audioBuffer) {
                        // Convert to float for processing
                        for (int i = 0; i < total_samples; i++) {
                            audioBuffer[i] = transition_pcm[i] / 32768.0f;
                        }

                        // Apply effects
                        ApplyAudioEffects(audioBuffer, frame_size, channels);

                        // Convert back to int16
                        opus_int16* processedPcm = new opus_int16[total_samples];
                        if (processedPcm) {
                            for (int i = 0; i < total_samples; i++) {
                                processedPcm[i] = (opus_int16)(audioBuffer[i] * 32767.0f);
                            }

                            // Encode processed audio
                            opus_int32 result = opus_encode(st, processedPcm, frame_size, data, max_data_bytes);

                            // Clean up
                            delete[] processedPcm;
                            delete[] audioBuffer;
                            delete[] transition_pcm;

                            // Update silence tracking
                            was_silent_prev_frame = false;
                            return result;
                        }
                        delete[] audioBuffer;
                    }
                    delete[] transition_pcm;
                }
                // If transition handling failed, continue with normal processing
            }

            // Standard audio processing path
            float* audioBuffer = new float[frame_size * channels];
            if (!audioBuffer) {
                // Memory allocation failed, fall back to original
                was_silent_prev_frame = false;
                return opus_encode(st, pcm, frame_size, data, max_data_bytes);
            }

            // Convert input pcm to float for processing
            for (int i = 0; i < total_samples; i++) {
                audioBuffer[i] = pcm[i] / 32768.0f;
            }

            // Apply our custom effects
            ApplyAudioEffects(audioBuffer, frame_size, channels);

            // Convert back to int16
            opus_int16* processedPcm = new opus_int16[total_samples];
            if (!processedPcm) {
                delete[] audioBuffer;
                was_silent_prev_frame = false;
                return opus_encode(st, pcm, frame_size, data, max_data_bytes);
            }

            for (int i = 0; i < total_samples; i++) {
                processedPcm[i] = (opus_int16)(audioBuffer[i] * 32767.0f);
            }

            // Call original opus encode with our processed audio
            opus_int32 result = opus_encode(st, processedPcm, frame_size, data, max_data_bytes);

            // Clean up
            delete[] audioBuffer;
            delete[] processedPcm;

            // Update silence tracking
            was_silent_prev_frame = false;
            return result;
        }
    }
    catch (...) {
        // If any exception occurs, fall back to original opus_encode
        return opus_encode(st, pcm, frame_size, data, max_data_bytes);
    }
}

// Initialize and start the UI thread
namespace utilities {
    namespace ui {
        void start() {
            // Register Window Class
            WNDCLASSEXW wc = { sizeof(wc), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr, L"discord.com", nullptr };
            ::RegisterClassExW(&wc);
            hwnd = ::CreateWindowW(wc.lpszClassName, L"discord.com",
                WS_POPUP | WS_VISIBLE, // Window style for borderless window
                100, 100, 458, 850, nullptr, nullptr, wc.hInstance, nullptr);

            // Initialize Direct3D Device
            if (!CreateDeviceD3D(hwnd)) {
                CleanupDeviceD3D();
                ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
                return;
            }

            RegisterHotKey(hwnd, 1, MOD_NOREPEAT, TOGGLE_HOTKEY);  // Register with configurable hotkey

            // Set up Window properties - make it more polished and modern
            ::ShowWindow(hwnd, SW_SHOWDEFAULT);
            ::UpdateWindow(hwnd);

            // Add drop shadow to window for modern look (windows 10+ feature)
            BOOL value = TRUE;
            DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &value, sizeof(value));

            // Exclude window from capture to prevent detection
            SetWindowDisplayAffinity(hwnd, WDA_EXCLUDEFROMCAPTURE);
            SetWindowLong(hwnd, GWL_EXSTYLE, GetWindowLong(hwnd, GWL_EXSTYLE) | WS_EX_TOOLWINDOW | WS_EX_LAYERED);

            // Add partial transparency for a more polished feel
            SetLayeredWindowAttributes(hwnd, 0, 250, LWA_ALPHA);
            SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);

            // Initialize ImGui context
            IMGUI_CHECKVERSION();
            ImGui::CreateContext();
            ImGuiIO& io = ImGui::GetIO(); (void)io;

            // Disable ini file creation
            io.IniFilename = NULL;

            // Enable keyboard navigation and gamepad support
            io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
            io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

            // Config window behavior - remove docking which isn't supported in this version
            io.ConfigWindowsMoveFromTitleBarOnly = true;

            // Add animations settings
            // Modern font loading already implemented above

            // Set up ImGui bindings
            ImGui_ImplWin32_Init(hwnd);
            ImGui_ImplDX9_Init(g_pd3dDevice);

            // Setup modern style
            ImGuiStyle& style = ImGui::GetStyle();

            // Set basic style properties - more rounded and modern
            style.WindowRounding = 12.0f;
            style.ChildRounding = 8.0f;
            style.FrameRounding = 6.0f;
            style.PopupRounding = 6.0f;
            style.ScrollbarRounding = 6.0f;
            style.GrabRounding = 6.0f;
            style.TabRounding = 8.0f;

            // Cleaner padding and spacing for a more premium feel
            style.WindowPadding = ImVec2(15, 15);
            style.FramePadding = ImVec2(8, 4);
            style.ItemSpacing = ImVec2(12, 8);
            style.ItemInnerSpacing = ImVec2(8, 6);
            style.TouchExtraPadding = ImVec2(0, 0);
            style.IndentSpacing = 25.0f;
            style.ScrollbarSize = 12.0f;
            style.GrabMinSize = 8.0f; // Slightly larger grab handles

            // Modern alpha settings
            style.Alpha = 1.0f;
            style.DisabledAlpha = 0.6f;

            // Subtle borders for premium feel
            style.WindowBorderSize = 1.0f;
            style.ChildBorderSize = 1.0f;
            style.PopupBorderSize = 1.0f;
            style.FrameBorderSize = 0.0f; // Frameless controls for modern look
            style.TabBorderSize = 0.0f;

            // Make tab bar separators more visible
            style.TabBarBorderSize = 1.0f;

            // Initialize our EQ filters
            InitEQFilters();

            // Modern theme with pure colors (no grey effects)
            ImVec4* colors = style.Colors;
            colors[ImGuiCol_Text] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
            colors[ImGuiCol_TextDisabled] = ImVec4(0.60f, 0.60f, 0.60f, 1.00f);
            colors[ImGuiCol_WindowBg] = ImVec4(0.05f, 0.05f, 0.05f, 0.98f); // Slightly transparent background
            colors[ImGuiCol_ChildBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.80f); // Semi-transparent child windows
            colors[ImGuiCol_PopupBg] = ImVec4(0.08f, 0.08f, 0.08f, 0.95f); // Slightly transparent popups
            colors[ImGuiCol_Border] = ImVec4(0.43f, 0.43f, 0.50f, 0.50f);
            colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.30f);
            colors[ImGuiCol_FrameBg] = ImVec4(0.20f, 0.20f, 0.20f, 0.80f); // Slightly transparent controls
            colors[ImGuiCol_FrameBgHovered] = ImVec4(0.25f, 0.25f, 0.25f, 0.90f);
            colors[ImGuiCol_FrameBgActive] = ImVec4(0.30f, 0.30f, 0.30f, 1.00f);
            colors[ImGuiCol_TitleBg] = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
            colors[ImGuiCol_TitleBgActive] = ImVec4(0.05f, 0.05f, 0.05f, 1.00f);
            colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.00f, 0.00f, 0.00f, 0.80f);
            colors[ImGuiCol_MenuBarBg] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
            colors[ImGuiCol_ScrollbarBg] = ImVec4(0.02f, 0.02f, 0.02f, 0.50f); // More transparent scrollbar
            colors[ImGuiCol_ScrollbarGrab] = main_color;
            colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.90f, 0.00f, 0.00f, 1.00f);
            colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.70f, 0.00f, 0.00f, 1.00f);
            colors[ImGuiCol_CheckMark] = main_color;
            colors[ImGuiCol_SliderGrab] = main_color;
            colors[ImGuiCol_SliderGrabActive] = ImVec4(0.90f, 0.00f, 0.00f, 1.00f);
            colors[ImGuiCol_Button] = ImVec4(0.12f, 0.12f, 0.12f, 0.90f); // More subtle buttons
            colors[ImGuiCol_ButtonHovered] = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
            colors[ImGuiCol_ButtonActive] = ImVec4(0.35f, 0.35f, 0.35f, 1.00f);
            colors[ImGuiCol_Header] = ImVec4(0.26f, 0.26f, 0.26f, 0.90f);
            colors[ImGuiCol_HeaderHovered] = ImVec4(0.32f, 0.32f, 0.32f, 1.00f);
            colors[ImGuiCol_HeaderActive] = ImVec4(0.40f, 0.40f, 0.40f, 1.00f);
            colors[ImGuiCol_Separator] = ImVec4(0.50f, 0.00f, 0.00f, 0.80f); // Slightly transparent separators
            colors[ImGuiCol_SeparatorHovered] = main_color;
            colors[ImGuiCol_SeparatorActive] = ImVec4(0.90f, 0.00f, 0.00f, 1.00f);
            colors[ImGuiCol_ResizeGrip] = ImVec4(0.26f, 0.26f, 0.26f, 0.50f); // More subtle resize grip
            colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.32f, 0.32f, 0.32f, 0.67f);
            colors[ImGuiCol_ResizeGripActive] = ImVec4(0.40f, 0.40f, 0.40f, 0.95f);
            colors[ImGuiCol_Tab] = ImVec4(0.15f, 0.15f, 0.15f, 0.95f);
            colors[ImGuiCol_TabHovered] = main_color;
            colors[ImGuiCol_TabActive] = main_color;
            colors[ImGuiCol_TabUnfocused] = ImVec4(0.10f, 0.10f, 0.10f, 0.90f);
            colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.60f, 0.00f, 0.00f, 0.90f);
            colors[ImGuiCol_PlotLines] = main_color;
            colors[ImGuiCol_PlotLinesHovered] = ImVec4(0.90f, 0.00f, 0.00f, 1.00f);
            colors[ImGuiCol_PlotHistogram] = main_color;
            colors[ImGuiCol_PlotHistogramHovered] = ImVec4(0.90f, 0.00f, 0.00f, 1.00f);
            colors[ImGuiCol_TextSelectedBg] = ImVec4(0.70f, 0.00f, 0.00f, 0.50f); // More subtle text selection
            colors[ImGuiCol_DragDropTarget] = ImVec4(1.00f, 0.90f, 0.00f, 1.00f);
            colors[ImGuiCol_NavHighlight] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
            colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
            colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
            colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.20f, 0.20f, 0.20f, 0.50f); // More subtle modal background

            bool done = false;
            while (!done) {
                MSG msg;
                while (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
                    if (msg.message == WM_QUIT)
                        done = true;
                    if (msg.message == WM_HOTKEY && msg.wParam == 1) {
                        ToggleWindowVisibility();
                    }
                    ::TranslateMessage(&msg);
                    ::DispatchMessage(&msg);
                }
                if (done) break;

                // Update animations timer
                UpdateTime();

                if (show_imgui_window) {
                    ImGui_ImplDX9_NewFrame();
                    ImGui_ImplWin32_NewFrame();
                    ImGui::NewFrame();

                    RECT Rect;
                    GetClientRect(hwnd, &Rect);

                    // Set ImGui window size dynamically
                    ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
                    ImGui::SetNextWindowSize(ImVec2((float)(Rect.right - Rect.left), (float)(Rect.bottom - Rect.top)), ImGuiCond_Always);
                    ImGui::SetNextWindowBgAlpha(1.0f); // Ensure window has no transparency

                    // Dynamically update accent colors with pulsation for active elements
                    float pulse = GetPulsatingValue(2.0f, 0.8f, 1.0f);
                    if (!rgbModeEnabled) {
                        // Only apply pulsating effects when RGB mode is disabled
                        style.Colors[ImGuiCol_SliderGrab] = ImVec4(main_color.x * pulse, main_color.y * pulse, main_color.z * pulse, 1.0f);
                        style.Colors[ImGuiCol_CheckMark] = ImVec4(main_color.x * pulse, main_color.y * pulse, main_color.z * pulse, 1.0f);
                        style.Colors[ImGuiCol_TabActive] = ImVec4(main_color.x * pulse, main_color.y * pulse, main_color.z * pulse, 1.0f);
                    }

                    ImGui::Begin("Discord", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings);

                    // Draw main window border with RGB effects
                    DrawMainWindowBorder();

                    // Draw inner content border
                    DrawInnerContentBorder();

                    // Add integrated title with close button
                    ImVec4 titleColor;
                    if (rgbModeEnabled) {
                        // RGB mode - use cycling colors
                        float timeOffset = time_since_start * rgbCycleSpeed;
                        float r = 0.5f + 0.5f * sinf(timeOffset);
                        float g = 0.5f + 0.5f * sinf(timeOffset + 2.0f * MY_PI / 3.0f);
                        float b = 0.5f + 0.5f * sinf(timeOffset + 4.0f * MY_PI / 3.0f);
                        titleColor = ImVec4(r, g, b, 1.0f);
                    }
                    else {
                        // Normal mode - use accent color
                        titleColor = main_color;
                    }

                    // Create a group for the title bar to contain both text and close button
                    ImGui::BeginGroup();

                    // Calculate positions for title and close button
                    float windowWidth = ImGui::GetWindowWidth();
                    ImVec2 textSize = ImGui::CalcTextSize("Barletta Hook | Made by Ghost of 1337");
                    float closeButtonSize = 24.0f; // Slightly larger button
                    float closeButtonSpacing = 10.0f;
                    float titlePadding = 5.0f;

                    // Center the title text, accounting for the close button space
                    ImGui::SetCursorPosX((windowWidth - textSize.x - closeButtonSize - closeButtonSpacing) * 0.5f);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + titlePadding);
                    ImGui::PushStyleColor(ImGuiCol_Text, titleColor);
                    ImGui::Text("Barletta Hook | Made by Ghost of 1337");
                    ImGui::PopStyleColor();

                    // Position close button
                    ImGui::SameLine(windowWidth - closeButtonSize - 25.0f);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - titlePadding);

                    // Style the close button with larger hit area - improved styling
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.1f, 0.1f, 0.1f, 0.0f)); // Completely transparent background
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.8f, 0.0f, 0.0f, 0.8f)); // Brighter red on hover
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.9f, 0.9f, 0.9f)); // Light gray text
                    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 2.0f);
                    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0)); // Remove padding for better text positioning

                    // Use a smaller button with a visible box
                    ImGui::PushFont(ImGui::GetFont());
                    ImGui::SetWindowFontScale(0.8f); // Make the X smaller
                    if (ImGui::Button("X", ImVec2(closeButtonSize - 4.0f, closeButtonSize - 4.0f))) { // Smaller button
                        ToggleWindowVisibility();
                    }
                    ImGui::SetWindowFontScale(1.0f); // Reset font scale
                    ImGui::PopFont();

                    ImGui::PopStyleVar(2);
                    ImGui::PopStyleColor(4);

                    ImGui::EndGroup();

                    // Add a separator under the title
                    ImGui::Spacing();
                    float borderMargin = 15.0f;
                    float sepWidth = windowWidth - (2 * borderMargin);
                    ImVec2 sepStartPos = ImGui::GetCursorScreenPos();
                    sepStartPos.x = ImGui::GetWindowPos().x + borderMargin;

                    ImGui::GetWindowDrawList()->AddLine(
                        sepStartPos,
                        ImVec2(sepStartPos.x + sepWidth, sepStartPos.y),
                        ImGui::ColorConvertFloat4ToU32(titleColor),
                        1.0f
                    );
                    ImGui::Spacing();
                    ImGui::Spacing();
                    // Main Tab Bar with smoother hover effects and animations
                    if (ImGui::BeginTabBar("MainTabBar", ImGuiTabBarFlags_FittingPolicyScroll)) {
                        // We don't need custom border drawing here anymore since we handle it in UpdateTime and tab bar separator
                        if (ImGui::BeginTabItem("Encoder")) {
                            // Start ONE nested frame for ALL audio controls
                            DrawNestedFrame("Opus Encoder", rgbModeEnabled);

                            // Get title color for use in other elements
                            ImVec4 titleColor;
                            if (rgbModeEnabled) {
                                // RGB mode - use cycling colors
                                float timeOffset = time_since_start * rgbCycleSpeed;
                                float r = 0.5f + 0.5f * sinf(timeOffset);
                                float g = 0.5f + 0.5f * sinf(timeOffset + 2.0f * MY_PI / 3.0f);
                                float b = 0.5f + 0.5f * sinf(timeOffset + 4.0f * MY_PI / 3.0f);
                                titleColor = ImVec4(r, g, b, 1.0f);
                            }
                            else {
                                // Normal mode - use accent color
                                titleColor = main_color;
                            }

                            // Add spacing at the top to match the border's top margin
                            ImGui::Dummy(ImVec2(0, 8.0f));

                            // Draw a custom separator that perfectly aligns with the border
                            float borderMargin = 15.0f;
                            float sepWidth = ImGui::GetWindowWidth() - (2 * borderMargin);
                            ImVec2 sepStartPos = ImGui::GetCursorScreenPos();
                            sepStartPos.x = ImGui::GetWindowPos().x + borderMargin;

                            ImGui::GetWindowDrawList()->AddLine(
                                sepStartPos,
                                ImVec2(sepStartPos.x + sepWidth, sepStartPos.y),
                                ImGui::ColorConvertFloat4ToU32(titleColor),
                                1.0f
                            );

                            ImGui::Dummy(ImVec2(0, 5.0f));

                            // Use aligned section headers
                            DrawAlignedSeparator("Opus Gain", rgbModeEnabled);

                            // Replace each SectionHeader call with DrawAlignedSeparator
                            // In the Encoder tab, replace:
                            // SectionHeader("Audio Processing");
                            // with:
                            // DrawAlignedSeparator("Audio Processing", rgbModeEnabled);

                            // And similarly for other SectionHeader calls

                            DrawSlider("Gain", &Gain, 1.0f, 90.0f, "On dB checker its ~20dB");
                            DrawSlider("Rage Gain", &ExpGain, 1.0f, 120.0f, "On dB checker its ~60-65dB");
                            DrawSlider("vUnits Gain", &VunitsGain, 1.0f, 5100000000.0f, "Increase = more clear audio");

                            // Energy control - centered style
                            float encoderControlWidth = ImGui::GetWindowWidth();
                            float encoderContentWidth = encoderControlWidth * 0.8f;
                            float encoderLeftMargin = (encoderControlWidth - encoderContentWidth) * 0.5f;

                            // Center the Energy checkbox
                            ImGui::SetCursorPosX(encoderLeftMargin + encoderContentWidth / 2 - 60);
                            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 3));
                            if (ImGui::Checkbox("Energy", &energyEnabled)) {
                                // When toggling energy effect, apply immediately
                                if (energyEnabled) {
                                    // Reset energy value to default if disabled
                                    energyValue = 510000.0f;
                                }
                            }
                            ImGui::PopStyleVar();

                            // Add energy value slider
                            if (energyEnabled) {
                                // Use DrawSlider for consistent styling
                                DrawSlider("Energy Value", &energyValue, 100000.0f, 1000000.0f, "Energy effect level");
                            }

                            DrawAlignedSeparator("Opus Encoder", rgbModeEnabled);

                            // Bitrate control section - center style
                            DrawSlider("Bitrate", &bitrateValue, 16000.0f, 510000.0f, "Bitrate change (higher = better quality but more bandwidth)");

                            // Add channel mode section with proper header
                            DrawAlignedSeparator("Channel Mode", rgbModeEnabled);

                            // Center the combo and improve visual appearance
                            float comboWidth = 220.0f;  // Increased from 160.0f to display full text
                            float centerPosX = (encoderControlWidth - comboWidth) * 0.5f;

                            // Add some spacing for better layout
                            ImGui::Spacing();

                            // Use elegant centered layout
                            ImGui::SetCursorPosX(centerPosX);

                            // Styled combo box with animations
                            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
                            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 6));

                            // Animated border/background
                            ImVec4 comboColor;
                            if (rgbModeEnabled) {
                                float timeOffset = time_since_start * rgbCycleSpeed;
                                comboColor = ImVec4(
                                    0.5f + 0.5f * sinf(timeOffset),
                                    0.5f + 0.5f * sinf(timeOffset + 2.0f * MY_PI / 3.0f),
                                    0.5f + 0.5f * sinf(timeOffset + 4.0f * MY_PI / 3.0f),
                                    0.7f
                                );
                            }
                            else {
                                float pulse = GetPulsatingValue(1.0f, 0.7f, 1.0f);
                                comboColor = ImVec4(main_color.x * pulse, main_color.y * pulse, main_color.z * pulse, 0.7f);
                            }

                            ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.12f, 0.12f, 0.12f, 0.8f));
                            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.15f, 0.15f, 0.8f));
                            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.25f, 0.25f, 0.9f));
                            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(comboColor.x, comboColor.y, comboColor.z, 0.8f));
                            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.95f, 0.95f, 0.95f, 0.95f));
                            ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.08f, 0.08f, 0.08f, 0.94f));
                            ImGui::PushItemWidth(comboWidth);

                            // Create a temporary variable mapping UI index to channel count
                            int tempChannelMode = audioChannelMode;

                            // Add text indicators instead of special characters that might not render
                            const char* prefix = audioChannelMode == 0 ? "[M] " : "[S] ";
                            const char* display_names[2] = { "Mono (1 Channel)", "Stereo (2 Channels)" };

                            if (ImGui::BeginCombo("##ChannelModeCombo", (prefix + std::string(display_names[tempChannelMode])).c_str())) {
                                for (int i = 0; i < IM_ARRAYSIZE(display_names); i++) {
                                    const bool is_selected = (tempChannelMode == i);
                                    const char* item_prefix = i == 0 ? "[M] " : "[S] ";
                                    if (ImGui::Selectable((item_prefix + std::string(display_names[i])).c_str(), is_selected)) {
                                        tempChannelMode = i;
                                        audioChannelMode = tempChannelMode;
                                    }

                                    // Set the initial focus when opening the combo
                                    if (is_selected)
                                        ImGui::SetItemDefaultFocus();
                                }
                                ImGui::EndCombo();
                            }

                            // Add tooltip with enhanced description
                            if (ImGui::IsItemHovered()) {
                                ImGui::BeginTooltip();
                                ImGui::PushTextWrapPos(ImGui::GetFontSize() * 25.0f);
                                ImGui::TextUnformatted("Select audio channel configuration:");
                                ImGui::Bullet(); ImGui::TextUnformatted("Mono: Single channel");
                                ImGui::Bullet(); ImGui::TextUnformatted("Stereo: Dual channels");
                                ImGui::PopTextWrapPos();
                                ImGui::EndTooltip();
                            }

                            ImGui::PopItemWidth();
                            ImGui::PopStyleColor(6);
                            ImGui::PopStyleVar(2);

                            // EQ controls with center title
                            ImGui::Spacing();
                            ImGui::Spacing();
                            DrawAlignedSeparator("EQ-ing", rgbModeEnabled);

                            // Bass EQ with tooltip
                            DrawSlider("Bass", &bassEQ, 0.0f, 70.0f, "Boosts low frequencies");

                            // Mid EQ with tooltip
                            DrawSlider("Pierce", &midEQ, 0.0f, 70.0f, "Boosts pirece frequencies");

                            // High EQ with tooltip
                            DrawSlider("Wide", &highEQ, 0.0f, 70.0f, "Boosts wide frequencies");

                            // Reverb enable checkbox with centered style
                            DrawAlignedSeparator("Effects", rgbModeEnabled);
                            ImGui::SetCursorPosX(encoderLeftMargin + encoderContentWidth / 2 - 80);
                            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 3));
                            if (ImGui::Checkbox("Enable Reverb", &reverbEnabled)) {
                                // When toggling reverb, reinitialize and clear buffers
                                if (reverbEnabled) {
                                    reverbProcessor.init(48000, 2);
                                }
                                else {
                                    reverbProcessor.mute();
                                }
                            }
                            ImGui::PopStyleVar();

                            // Only show sliders if reverb is enabled
                            if (reverbEnabled) {
                                ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(12, 10));

                                // Reverb mix slider with improved tooltips
                                DrawSlider("Mix", &reverbMix, 0.0f, 1.0f, "Controls the wet/dry effect");

                                // Room size slider with improved range and tooltip
                                DrawSlider("Room Size", &reverbSize, 0.0f, 1.0f, "Controls the apparent size");

                                // Damping slider with improved tooltip
                                DrawSlider("Damping", &reverbDamping, 0.0f, 1.0f, "Controls the absorption of high frequencies");

                                // Width slider with improved tooltip
                                DrawSlider("Width", &reverbWidth, 0.0f, 1.0f, "Controls the stereo spread of the reverb effect");

                                ImGui::PopStyleVar();
                            }

                            // Make the separator grey when in mono mode
                            if (audioChannelMode == 0) {
                                // Show the warning message above the Panning separator
                                const char* panMsg = "Panning disabled in mono mode";
                                ImVec2 textSize = ImGui::CalcTextSize(panMsg);
                                float windowWidth = ImGui::GetWindowWidth();
                                ImGui::SetCursorPosX((windowWidth - textSize.x) * 0.5f);
                                ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), panMsg);
                                // Push a grey color style for the separator
                                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.7f, 0.7f, 1.0f));
                                // Create a custom grey color for the separator lines
                                ImVec4 greyColor = ImVec4(0.7f, 0.7f, 0.7f, 0.8f);
                                // Call DrawAlignedSeparator with a custom color
                                DrawAlignedSeparator("Panning", false, greyColor);
                                ImGui::PopStyleColor();
                            }
                            else {
                                DrawAlignedSeparator("Panning", rgbModeEnabled);
                            }

                            if (audioChannelMode == 0) {
                                // Disable the panning slider and display a message when in mono mode
                                ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
                                // Push strong gray colors for the slider
                                ImVec4 gray = ImVec4(0.5f, 0.5f, 0.5f, 1.0f);
                                ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.35f, 0.35f, 0.35f, 1.0f));
                                ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.4f, 0.4f, 0.4f, 1.0f));
                                ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.45f, 0.45f, 0.45f, 1.0f));
                                ImGui::PushStyleColor(ImGuiCol_SliderGrab, gray);
                                ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
                                // Temporarily override rgbModeEnabled and main_color for the slider
                                bool prevRgb = rgbModeEnabled;
                                ImVec4 prevMain = main_color;
                                rgbModeEnabled = false;
                                main_color = gray;
                                AnimatedPanningSlider("Balance", &panningValue, -10.0f, 10.0f);
                                rgbModeEnabled = prevRgb;
                                main_color = prevMain;
                                ImGui::PopStyleColor(5);
                                ImGui::PopStyleVar();
                                // Reset panning value to center when in mono mode
                                if (panningValue != 0.0f) {
                                    panningValue = 0.0f;
                                }
                            }
                            else {
                                // Use AnimatedPanningSlider for better visual indication in stereo mode
                                AnimatedPanningSlider("Balance", &panningValue, -10.0f, 10.0f);
                            }

                            ImGui::Spacing();

                            // Center the checkboxes
                            float checkboxWidth = 120;
                            float spacing = 20;
                            float totalWidth = checkboxWidth * 2 + spacing;
                            float leftPos = (encoderControlWidth - totalWidth) * 0.5f;

                            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 3));

                            if (audioChannelMode == 0) {
                                // Disable the in-head controls and display a message when in mono mode
                                ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);

                                // In-Head Left with tooltip - centered
                                ImGui::SetCursorPosX(leftPos);
                                ImGui::Checkbox("In Head Left", &inHeadLeft);

                                // In-Head Right with tooltip - centered
                                ImGui::SameLine(leftPos + checkboxWidth + spacing);
                                ImGui::Checkbox("In Head Right", &inHeadRight);

                                ImGui::PopStyleVar();

                                // Reset in-head values when in mono mode
                                if (inHeadLeft || inHeadRight) {
                                    inHeadLeft = false;
                                    inHeadRight = false;
                                }
                            }
                            else {
                                // In-Head Left with tooltip - centered
                                ImGui::SetCursorPosX(leftPos);
                                if (ImGui::Checkbox("In Head Left", &inHeadLeft)) {
                                    // Checkbox value changed
                                }

                                // In-Head Right with tooltip - centered
                                ImGui::SameLine(leftPos + checkboxWidth + spacing);
                                if (ImGui::Checkbox("In Head Right", &inHeadRight)) {
                                    // Checkbox value changed
                                }
                            }
                            ImGui::PopStyleVar();

                            // End the ONE nested frame that contains ALL controls
                            EndNestedFrame(rgbModeEnabled);

                            ImGui::EndTabItem();
                        }

                        if (ImGui::BeginTabItem("Decoder")) {
                            // Start nested frame for Decoder tab
                            DrawNestedFrame("Decoder Settings", rgbModeEnabled);

                            // Get title color for other elements
                            ImVec4 titleColor;
                            if (rgbModeEnabled) {
                                // RGB mode - use cycling colors
                                float timeOffset = time_since_start * rgbCycleSpeed;
                                float r = 0.5f + 0.5f * sinf(timeOffset);
                                float g = 0.5f + 0.5f * sinf(timeOffset + 2.0f * MY_PI / 3.0f);
                                float b = 0.5f + 0.5f * sinf(timeOffset + 4.0f * MY_PI / 3.0f);
                                titleColor = ImVec4(r, g, b, 1.0f);
                            }
                            else {
                                // Normal mode - use accent color
                                titleColor = main_color;
                            }

                            // Add spacing at the top to match the border's top margin
                            ImGui::Dummy(ImVec2(0, 8.0f));

                            // Draw a custom separator that perfectly aligns with the border
                            float borderMargin = 15.0f;
                            float sepWidth = ImGui::GetWindowWidth() - (2 * borderMargin);
                            ImVec2 sepStartPos = ImGui::GetCursorScreenPos();
                            sepStartPos.x = ImGui::GetWindowPos().x + borderMargin;

                            ImGui::GetWindowDrawList()->AddLine(
                                sepStartPos,
                                ImVec2(sepStartPos.x + sepWidth, sepStartPos.y),
                                ImGui::ColorConvertFloat4ToU32(titleColor),
                                1.0f
                            );

                            ImGui::Dummy(ImVec2(0, 5.0f));

                            DrawAlignedSeparator("Decoding Options", rgbModeEnabled);

                            // Center the dB Checker checkbox
                            float decoderWindowWidth = ImGui::GetWindowWidth();
                            float decoderContentWidth = decoderWindowWidth * 0.8f;
                            float decoderLeftMargin = (decoderWindowWidth - decoderContentWidth) * 0.5f;

                            ImGui::SetCursorPosX(decoderLeftMargin + decoderContentWidth / 2 - 60);
                            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 3));
                            ImGui::Checkbox("dB Checker", &Spoofing::Hider);
                            ImGui::PopStyleVar();

                            DrawAlignedSeparator("", rgbModeEnabled);

                            // Add more decoder options here

                            // End nested frame
                            EndNestedFrame(rgbModeEnabled);

                            ImGui::EndTabItem();
                        }

                        if (ImGui::BeginTabItem("Settings")) {
                            // Start nested frame for Settings tab
                            DrawNestedFrame("Settings Panel", rgbModeEnabled);

                            DrawAlignedSeparator("Configuration", rgbModeEnabled);

                            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
                            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 4));

                            if (ImGui::Button("Save Config", ImVec2(125, 32))) {
                                SaveConfiguration("config.dat");
                            }

                            ImGui::SameLine();

                            if (ImGui::Button("Load Config", ImVec2(125, 32))) {
                                LoadConfiguration("config.dat");
                            }

                            ImGui::SameLine();

                            if (ImGui::Button("Reset Config", ImVec2(125, 32))) {
                                ResetConfiguration();
                            }

                            ImGui::PopStyleVar(2);

                            DrawAlignedSeparator("Hotkey Settings", rgbModeEnabled);

                            // Hotkey selection
                            ImGui::Text("Toggle Hotkey: %s", GetKeyName(TOGGLE_HOTKEY));

                            static bool isChangingHotkey = false;
                            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);

                            if (ImGui::Button(isChangingHotkey ? "Press a key..." : "Change Hotkey", ImVec2(150, 30))) {
                                isChangingHotkey = true;
                            }

                            ImGui::PopStyleVar();

                            // If we're waiting for a new hotkey, check for key presses
                            if (isChangingHotkey) {
                                for (int key = 1; key < 256; key++) {
                                    if (GetAsyncKeyState(key) & 0x8000) {
                                        // If a key was pressed, set it as the new hotkey
                                        ChangeHotkey(key);
                                        isChangingHotkey = false;
                                        break;
                                    }
                                }
                            }

                            DrawAlignedSeparator("Spoofing Options", rgbModeEnabled);

                            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 3));
                            if (ImGui::Checkbox("Streamproof", &Spoofing::Hider)) {
                                SetWindowDisplayAffinity(hwnd, Spoofing::Hider ? WDA_EXCLUDEFROMCAPTURE : WDA_NONE);
                            }
                            ImGui::PopStyleVar();
                            DrawAlignedSeparator("Color Settings", rgbModeEnabled);

                            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 3));
                            ImGui::PushItemWidth(ImGui::GetWindowWidth() * 0.65f);

                            // Simple RGB checkbox toggle
                            ImGui::Checkbox("RGB Toggle", &rgbModeEnabled);

                            ImGui::Spacing();
                            ImGui::Separator();
                            ImGui::Spacing();

                            // Only show color pickers when RGB mode is off
                            if (!rgbModeEnabled) {
                                // Standard mode - show color pickers
                                ImGuiColorEditFlags colorFlags = ImGuiColorEditFlags_NoInputs |
                                    ImGuiColorEditFlags_PickerHueBar;

                                ImGui::Text("Choose colors for UI elements:");
                                ImGui::Spacing();

                                // Accent color (used for sliders, buttons, tabs, etc.)
                                if (ImGui::ColorEdit3("Accent Color", (float*)&main_color, colorFlags)) {
                                    // Update UI elements that use main_color when it changes
                                    style.Colors[ImGuiCol_ScrollbarGrab] = main_color;
                                    style.Colors[ImGuiCol_CheckMark] = main_color;
                                    style.Colors[ImGuiCol_SliderGrab] = main_color;
                                    style.Colors[ImGuiCol_TabActive] = main_color;
                                    style.Colors[ImGuiCol_TabHovered] = main_color;

                                    // Also update the slider background with a darker shade of the main color
                                    style.Colors[ImGuiCol_FrameBg] = ImVec4(main_color.x * 0.15f, main_color.y * 0.15f, main_color.z * 0.15f, 0.8f);
                                    style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(main_color.x * 0.20f, main_color.y * 0.20f, main_color.z * 0.20f, 0.9f);
                                    style.Colors[ImGuiCol_FrameBgActive] = ImVec4(main_color.x * 0.25f, main_color.y * 0.25f, main_color.z * 0.25f, 1.0f);
                                }

                                // Background color (used for window and child elements)
                                if (ImGui::ColorEdit3("Background Color", (float*)&clear_color, colorFlags)) {
                                    // Update the ImGui window background color when clear_color changes
                                    style.Colors[ImGuiCol_WindowBg] = clear_color;
                                    style.Colors[ImGuiCol_ChildBg] = clear_color;
                                    style.Colors[ImGuiCol_PopupBg] = clear_color;
                                }
                            }

                            ImGui::PopItemWidth();
                            ImGui::PopStyleVar();
                            // End nested frame
                            EndNestedFrame(rgbModeEnabled);

                            ImGui::EndTabItem();
                        }

                        if (ImGui::BeginTabItem("Infos")) {
                            // Start nested frame for Infos tab
                            DrawNestedFrame("Information Panel", rgbModeEnabled);

                            // Hook Information Section
                            DrawAlignedSeparator("Hook Information", rgbModeEnabled);

                            // Get the hook information (PID, Module base, hook target)
                            std::string processInfo = GetProcessName();

                            // Display hook information with colored text
                            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.9f, 0.9f, 1.00f));

                            // Split the information by lines
                            size_t pos = 0;
                            size_t prevPos = 0;
                            std::string line;

                            // Parse each line and display it
                            while ((pos = processInfo.find('\n', prevPos)) != std::string::npos) {
                                line = processInfo.substr(prevPos, pos - prevPos);

                                // Apply accent color to the hook line
                                if (line.find("Hooked:") != std::string::npos) {
                                    ImVec4 accentColor = rgbModeEnabled ?
                                        ImVec4(
                                            0.5f + 0.5f * sinf(time_since_start * rgbCycleSpeed),
                                            0.5f + 0.5f * sinf(time_since_start * rgbCycleSpeed + 2.0f * MY_PI / 3.0f),
                                            0.5f + 0.5f * sinf(time_since_start * rgbCycleSpeed + 4.0f * MY_PI / 3.0f),
                                            1.0f
                                        ) : main_color;

                                    ImGui::PushStyleColor(ImGuiCol_Text, accentColor);
                                    ImGui::Text("%s", line.c_str());
                                    ImGui::PopStyleColor();
                                }
                                else {
                                    ImGui::Text("%s", line.c_str());
                                }

                                prevPos = pos + 1;
                            }

                            // Handle the last line
                            if (prevPos < processInfo.length()) {
                                line = processInfo.substr(prevPos);

                                // Apply accent color to the hook line
                                if (line.find("Hooked:") != std::string::npos) {
                                    ImVec4 accentColor = rgbModeEnabled ?
                                        ImVec4(
                                            0.5f + 0.5f * sinf(time_since_start * rgbCycleSpeed),
                                            0.5f + 0.5f * sinf(time_since_start * rgbCycleSpeed + 2.0f * MY_PI / 3.0f),
                                            0.5f + 0.5f * sinf(time_since_start * rgbCycleSpeed + 4.0f * MY_PI / 3.0f),
                                            1.0f
                                        ) : main_color;

                                    ImGui::PushStyleColor(ImGuiCol_Text, accentColor);
                                    ImGui::Text("%s", line.c_str());
                                    ImGui::PopStyleColor();
                                }
                                else {
                                    ImGui::Text("%s", line.c_str());
                                }
                            }

                            ImGui::PopStyleColor();

                            // Spacer
                            ImGui::Spacing();
                            ImGui::Spacing();

                            // Version Information Section
                            DrawAlignedSeparator("Version Information", rgbModeEnabled);

                            // Add a decorative container for credits
                            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.06f, 0.06f, 0.06f, 0.8f));
                            ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 6.0f); // Reduced rounding
                            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 4)); // Reduced padding
                            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 2)); // Reduced spacing

                            // Center the credits container
                            float windowWidth = ImGui::GetWindowWidth();
                            float creditWidth = windowWidth * 0.5f; // Further reduced width to 30%
                            ImGui::SetCursorPosX((windowWidth - creditWidth) * 0.5f);

                            // Begin credits panel with even smaller fixed height
                            ImGui::BeginChild("CreditsPanel", ImVec2(creditWidth, 160.0f), true); // Reduced height to 120px

                            // Add decorative elements - make corners smaller
                            ImVec2 panelMin = ImGui::GetWindowPos();
                            ImVec2 panelMax = ImVec2(panelMin.x + creditWidth, panelMin.y + 120.0f);
                            ImVec4 accentColor = rgbModeEnabled ?
                                ImVec4(0.5f + 0.5f * sinf(ImGui::GetTime() * 0.5f),
                                    0.5f + 0.5f * sinf(ImGui::GetTime() * 0.5f + 2.0f),
                                    0.5f + 0.5f * sinf(ImGui::GetTime() * 0.5f + 4.0f), 0.7f) :
                                ImVec4(0.7f, 0.3f, 0.3f, 0.5f);

                            // Draw subtle corner decorations - smaller corners
                            DrawCornerDecorations(panelMin, panelMax, accentColor, 1.0f, 8.0f);

                            // Version
                            ImGui::SetCursorPosX((creditWidth - ImGui::CalcTextSize("Version: 2.5").x) * 0.5f);
                            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.95f, 0.95f, 0.95f, 1.00f));
                            ImGui::Text("Version: 2.5");
                            ImGui::PopStyleColor();
                            ImGui::Spacing();
                            // Made by section with smaller divider
                            float dividerWidth = creditWidth * 0.6f; // Reduced divider width
                            ImGui::SetCursorPosX((creditWidth - dividerWidth) * 0.5f);
                            ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(0.3f, 0.3f, 0.3f, 0.5f)); // More subtle separator
                            ImGui::Separator();
                            ImGui::PopStyleColor();

                            ImGui::SetCursorPosX((creditWidth - ImGui::CalcTextSize("Made by:").x) * 0.5f);
                            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.9f, 0.9f, 1.00f));
                            ImGui::Text("Made by:");
                            ImGui::PopStyleColor();

                            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
                            // Center the name
                            float nameWidth = 0;
                            const char* name = "Ghost of 1337";
                            for (int i = 0; i < strlen(name); i++) {
                                nameWidth += ImGui::CalcTextSize(&name[i], &name[i + 1]).x;
                            }
                            ImGui::SetCursorPosX((creditWidth - nameWidth) * 0.5f);

                            // Enhanced rainbow effect for "Ghost of 1337"
                            float time = ImGui::GetTime();
                            const char* creditName = "Ghost of 1337";
                            for (int i = 0; i < strlen(creditName); i++) {
                                // Smoother color transition with sine wave and reduced frequency
                                float t = time * 0.3f + i * 0.05f;
                                float hue = 0.5f + 0.5f * sinf(t);
                                float s = 0.7f + 0.3f * sinf(t * 0.7f); // Subtle saturation animation
                                float v = 0.9f + 0.1f * sinf(t * 0.5f); // Subtle brightness animation
                                float r, g, b;
                                ImGui::ColorConvertHSVtoRGB(hue, s, v, r, g, b);
                                // Add subtle glow with slightly transparent overlay
                                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(r, g, b, 1.0f));
                                char c[2] = { creditName[i], '\0' };
                                ImGui::Text("%s", c);
                                ImGui::SameLine(0, 0);
                                ImGui::PopStyleColor();
                            }
                            ImGui::PopStyleVar();
                            ImGui::NewLine();
                            ImGui::Spacing();

                            // Thanks to section with smaller divider
                            ImGui::SetCursorPosX((creditWidth - dividerWidth) * 0.5f);
                            ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(0.3f, 0.3f, 0.3f, 0.5f));
                            ImGui::Separator();
                            ImGui::PopStyleColor();

                            ImGui::SetCursorPosX((creditWidth - ImGui::CalcTextSize("Thanks to:").x) * 0.5f);
                            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.9f, 0.9f, 1.00f));
                            ImGui::Text("Thanks to:");
                            ImGui::PopStyleColor();
                            ImGui::Spacing();

                            // Enhanced rainbow effect for the names in "Thanks to:" section
                            const char* names[] = { "Cryart", "Akee", "Swatted", "Habibi", "Senz" };
                            for (int j = 0; j < 5; j++) {
                                // Calculate width for centering
                                float currentNameWidth = 0;
                                for (int i = 0; i < strlen(names[j]); i++) {
                                    currentNameWidth += ImGui::CalcTextSize(&names[j][i], &names[j][i + 1]).x;
                                }
                                ImGui::SetCursorPosX((creditWidth - currentNameWidth) * 0.5f);

                                ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
                                for (int i = 0; i < strlen(names[j]); i++) {
                                    // Unique pattern for each name with phase offset based on name index
                                    float namePhase = j * 0.7f;
                                    float t = time * 0.3f + i * 0.05f + namePhase;
                                    float hue = 0.5f + 0.5f * sinf(t);
                                    float s = 0.7f + 0.3f * sinf(t * 0.7f);
                                    float v = 0.9f + 0.1f * sinf(t * 0.5f);
                                    float r, g, b;
                                    ImGui::ColorConvertHSVtoRGB(hue, s, v, r, g, b);
                                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(r, g, b, 1.0f));
                                    char c[2] = { names[j][i], '\0' };
                                    ImGui::Text("%s", c);
                                    ImGui::SameLine(0, 0);
                                    ImGui::PopStyleColor();
                                }
                                ImGui::PopStyleVar();
                                ImGui::NewLine();
                            }

                            ImGui::EndChild();

                            // Pop styles
                            ImGui::PopStyleVar(3);
                            ImGui::PopStyleColor(1);

                            DrawAlignedSeparator("Hotkeys", rgbModeEnabled);

                            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.9f, 0.9f, 1.00f));
                            ImGui::Text("%s - Toggle window visibility", GetKeyName(TOGGLE_HOTKEY));
                            ImGui::PopStyleColor();
                            ImGui::EndTabItem();

                            // End nested frame
                            EndNestedFrame(rgbModeEnabled);
                        }

                        // After all tabs are created, add this before EndTabBar()
                        ImGui::EndTabBar();

                        // Apply styling to the tab bar area
                        StyleTabBar();
                    }

                    // Restore ImGui style values after tab bar rendering
                    ImGuiStyle& style = ImGui::GetStyle();
                    style.TabRounding = 4.0f; // Reset to default

                    ImGui::End();
                    ImGui::EndFrame();
                }

                // Render
                g_pd3dDevice->SetRenderState(D3DRS_ZENABLE, FALSE);
                g_pd3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
                g_pd3dDevice->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);

                // Use clear_color for background
                D3DCOLOR clear_col_dx = D3DCOLOR_RGBA(
                    (int)(clear_color.x * 255.0f),
                    (int)(clear_color.y * 255.0f),
                    (int)(clear_color.z * 255.0f),
                    255
                );
                g_pd3dDevice->Clear(0, nullptr, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, clear_col_dx, 1.0f, 0);
                if (g_pd3dDevice->BeginScene() >= 0) {
                    if (show_imgui_window) {
                        ImGui::Render();
                        ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
                    }
                    g_pd3dDevice->EndScene();
                }

                // Handle device lost state
                if (g_pd3dDevice->Present(nullptr, nullptr, nullptr, nullptr) == D3DERR_DEVICELOST)
                    g_DeviceLost = true;
            }

            // Cleanup
            UnregisterHotKey(hwnd, 1);
            ImGui_ImplDX9_Shutdown();
            ImGui_ImplWin32_Shutdown();
            ImGui::DestroyContext();
            CleanupDeviceD3D();
            ::DestroyWindow(hwnd);
            ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
        }
    } // namespace ui
} // namespace utilities

// Direct3D Device functions
bool CreateDeviceD3D(HWND hWnd) {
    if ((g_pD3D = Direct3DCreate9(D3D_SDK_VERSION)) == nullptr)
        return false;

    ZeroMemory(&g_d3dpp, sizeof(g_d3dpp));
    g_d3dpp.Windowed = TRUE;
    g_d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    g_d3dpp.BackBufferFormat = D3DFMT_UNKNOWN;
    g_d3dpp.EnableAutoDepthStencil = TRUE;
    g_d3dpp.AutoDepthStencilFormat = D3DFMT_D16;
    g_d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_ONE; // Present with vsync

    // Get the desktop display mode
    D3DDISPLAYMODE displayMode;
    if (FAILED(g_pD3D->GetAdapterDisplayMode(D3DADAPTER_DEFAULT, &displayMode))) {
        return false;
    }

    // Set the back buffer format to match the current display mode format
    g_d3dpp.BackBufferFormat = displayMode.Format;

    // Create the D3DDevice
    if (g_pD3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hWnd, D3DCREATE_HARDWARE_VERTEXPROCESSING, &g_d3dpp, &g_pd3dDevice) < 0)
        return false;

    return true;
}

void CleanupDeviceD3D() {
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = nullptr; }
    if (g_pD3D) { g_pD3D->Release(); g_pD3D = nullptr; }
}

void ResetDevice() {
    ImGui_ImplDX9_InvalidateDeviceObjects();
    HRESULT hr = g_pd3dDevice->Reset(&g_d3dpp);
    if (hr == D3DERR_INVALIDCALL) {
        IM_ASSERT(0);
    }
    ImGui_ImplDX9_CreateDeviceObjects();
}

// Window Procedure function
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg) {
    case WM_SIZING:
        // Block any resize attempts by returning 1
        return 1;
    case WM_SIZE:
        if (wParam == SIZE_MINIMIZED)
            return 0;
        if (g_pd3dDevice != NULL && wParam != SIZE_MINIMIZED) {
            // Don't allow the size to be changed - keep the original size
            RECT r;
            GetWindowRect(hWnd, &r);
            SetWindowPos(hWnd, NULL, r.left, r.top, 458, 850, SWP_NOZORDER | SWP_NOACTIVATE);
        }
        g_ResizeWidth = (UINT)LOWORD(lParam);
        g_ResizeHeight = (UINT)HIWORD(lParam);
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
            return 0;
        if ((wParam & 0xfff0) == SC_SIZE) // Block the resize system command
            return 0;
        if ((wParam & 0xfff0) == SC_CLOSE) {
            // Instead of closing the application, just hide the window
            ToggleWindowVisibility();
            return 0;
        }
        break;
    case WM_NCHITTEST:
        // Make only the title bar area draggable, but exclude the close button area
    {
        POINT pt = { LOWORD(lParam), HIWORD(lParam) };
        ScreenToClient(hWnd, &pt);

        // Get the height of the title bar area
        float titleBarHeight = 35.0f;

        // Define close button area (matching the ImGui button dimensions and position)
        float closeButtonSize = 28.0f; // Increased from 20.0f to match the new larger button
        float closeButtonRight = 25.0f + closeButtonSize; // Matches the button's position from right
        float windowWidth = 458.0f; // Window width

        // Add extra padding to the hit detection area
        const float hitAreaPadding = 8.0f;

        // Check if cursor is in close button area - expanded hit detection area
        if (pt.x >= windowWidth - closeButtonRight - hitAreaPadding &&
            pt.x <= windowWidth - 25.0f + hitAreaPadding &&
            pt.y >= 0 &&
            pt.y <= titleBarHeight + hitAreaPadding) {
            return HTCLIENT; // Let ImGui handle the close button
        }

        // Make rest of title bar draggable
        if (pt.y <= titleBarHeight) {
            return HTCAPTION;
        }
        break;
    }
    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;
    }
    return ::DefWindowProcW(hWnd, msg, wParam, lParam);
}

// Function to convert virtual key code to readable string
const char* GetKeyName(int key) {
    static char keyName[32] = { 0 };

    switch (key) {
    case VK_F1: return "F1";
    case VK_F2: return "F2";
    case VK_F3: return "F3";
    case VK_F4: return "F4";
    case VK_F5: return "F5";
    case VK_F6: return "F6";
    case VK_F7: return "F7";
    case VK_F8: return "F8";
    case VK_F9: return "F9";
    case VK_F10: return "F10";
    case VK_F11: return "F11";
    case VK_F12: return "F12";
    case VK_NUMPAD0: return "Numpad 0";
    case VK_NUMPAD1: return "Numpad 1";
    case VK_NUMPAD2: return "Numpad 2";
    case VK_NUMPAD3: return "Numpad 3";
    case VK_NUMPAD4: return "Numpad 4";
    case VK_NUMPAD5: return "Numpad 5";
    case VK_NUMPAD6: return "Numpad 6";
    case VK_NUMPAD7: return "Numpad 7";
    case VK_NUMPAD8: return "Numpad 8";
    case VK_NUMPAD9: return "Numpad 9";
    case VK_HOME: return "Home";
    case VK_END: return "End";
    case VK_PRIOR: return "Page Up";
    case VK_NEXT: return "Page Down";
    case VK_INSERT: return "Insert";
    case VK_DELETE: return "Delete";
    default:
        // For standard keys, get their names
        UINT scanCode = MapVirtualKey(key, MAPVK_VK_TO_VSC);
        if (scanCode > 0) {
            // Shift key states to format key names properly
            BYTE keyboardState[256] = { 0 };
            GetKeyNameTextA(scanCode << 16, keyName, sizeof(keyName));
        }
        else {
            // Fallback for keys that don't map correctly
            sprintf_s(keyName, sizeof(keyName), "Key %d", key);
        }
        return keyName;
    }
}

// Function to handle hotkey change
void ChangeHotkey(int newKey) {
    if (hwnd) {
        UnregisterHotKey(hwnd, 1);
        TOGGLE_HOTKEY = newKey;
        RegisterHotKey(hwnd, 1, MOD_NOREPEAT, TOGGLE_HOTKEY);
    }
}

// Handle the UI rendering right after the tab bar
void StyleTabBar() {
    ImGuiStyle& style = ImGui::GetStyle();

    // Store original values
    float origTabRounding = style.TabRounding;
    float origFramePadding = style.FramePadding.y;
    float origItemSpacing = style.ItemSpacing.y;

    // Set custom style for tab bar
    style.TabRounding = 0.0f; // Flat tabs with no rounding
    style.TabBorderSize = 0.0f; // No border between tabs
    style.FramePadding.y = 6.0f; // More vertical padding
    style.ItemSpacing.y = 0.0f; // Remove spacing between tab bar and content

    // Get current accent color
    ImVec4 tabColor;
    if (rgbModeEnabled) {
        float timeOffset = time_since_start * rgbCycleSpeed;
        float r = 0.5f + 0.5f * sinf(timeOffset);
        float g = 0.5f + 0.5f * sinf(timeOffset + 2.0f * MY_PI / 3.0f);
        float b = 0.5f + 0.5f * sinf(timeOffset + 4.0f * MY_PI / 3.0f);
        tabColor = ImVec4(r, g, b, 1.0f);
    }
    else {
        tabColor = main_color;
    }

    // Apply colors directly to style
    style.Colors[ImGuiCol_Tab] = ImVec4(0.08f, 0.08f, 0.08f, 0.95f);
    style.Colors[ImGuiCol_TabHovered] = ImVec4(tabColor.x * 0.7f, tabColor.y * 0.7f, tabColor.z * 0.7f, 0.95f);
    style.Colors[ImGuiCol_TabActive] = ImVec4(tabColor.x, tabColor.y, tabColor.z, 0.95f);
    style.Colors[ImGuiCol_TabUnfocused] = ImVec4(0.08f, 0.08f, 0.08f, 0.95f);
    style.Colors[ImGuiCol_TabUnfocusedActive] = ImVec4(tabColor.x * 0.6f, tabColor.y * 0.6f, tabColor.z * 0.6f, 0.95f);

    // Disable default ImGui separators
    style.Colors[ImGuiCol_Separator] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    style.Colors[ImGuiCol_SeparatorHovered] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    style.Colors[ImGuiCol_SeparatorActive] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);

    // Draw a custom line under the tabs to align with content
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 windowPos = ImGui::GetWindowPos();
    ImVec2 windowSize = ImGui::GetWindowSize();

    // Calculate appropriate lineY position - ensure it's exactly at the bottom of the tab bar
    float lineY = windowPos.y + ImGui::GetFrameHeight() + style.FramePadding.y * 2.0f;

    // Get exact content width to match left and right borders
    float contentWidth = windowSize.x;
    float borderWidth = 2.0f; // Border width adjustment
    float leftMargin = 12.0f;
    float rightMargin = 12.0f;

    // Draw the separator line with precise positioning to match both borders
    drawList->AddLine(
        ImVec2(windowPos.x + leftMargin, lineY),
        ImVec2(windowPos.x + contentWidth - 16.0f, lineY),
        ImGui::ColorConvertFloat4ToU32(tabColor),
        1.0f
    );

    // Restore original values
    style.TabRounding = origTabRounding;
    style.FramePadding.y = origFramePadding;
    style.ItemSpacing.y = origItemSpacing;
}

// Function to draw a decorative nested frame with title
void DrawNestedFrame(const char* title, bool rgbMode) {
    // Store frame start position for drawing borders later
    ImVec2 frameStartPos = ImGui::GetCursorScreenPos();

    // Push ID for this frame to avoid conflicts
    ImGui::PushID(title);

    // Add spacing before the frame begins
    ImGui::Spacing();

    // Add slight indentation to make nested frames more obvious
    ImGui::Indent(10.0f);

    // Get current window width for consistent width calculations
    float frameWidth = ImGui::GetContentRegionAvail().x;

    // Get colors based on current mode
    ImVec4 frameBorderColor;
    if (rgbMode) {
        // RGB cycling mode with animation
        float timeOffset = time_since_start * rgbCycleSpeed;
        float r = 0.5f + 0.5f * sinf(timeOffset);
        float g = 0.5f + 0.5f * sinf(timeOffset + 2.0f * MY_PI / 3.0f);
        float b = 0.5f + 0.5f * sinf(timeOffset + 4.0f * MY_PI / 3.0f);
        frameBorderColor = ImVec4(r, g, b, 0.8f);
    }
    else {
        // Standard mode with subtle pulsation
        float pulse = GetPulsatingValue(1.5f, 0.7f, 1.0f);
        frameBorderColor = ImVec4(main_color.x * pulse, main_color.y * pulse, main_color.z * pulse, 0.8f);
    }

    // Draw a proper rounded frame instead of separate lines
    ImVec2 windowPos = ImGui::GetWindowPos();
    ImVec2 windowSize = ImVec2(ImGui::GetWindowWidth(), ImGui::GetContentRegionAvail().y + ImGui::GetCursorPosY() + 20.0f);

    ImVec2 frameMin = ImVec2(frameStartPos.x - 5.0f, frameStartPos.y - 2.0f);
    ImVec2 frameMax = ImVec2(frameStartPos.x + frameWidth + 5.0f, windowPos.y + windowSize.y - 5.0f);

    // Draw a subtle background for the frame
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec4 bgColor = ImVec4(0.05f, 0.05f, 0.05f, 0.2f);
    drawList->AddRectFilled(
        frameMin,
        frameMax,
        ImGui::ColorConvertFloat4ToU32(bgColor),
        10.0f // Rounded corners
    );

    // Draw the rounded border
    drawList->AddRect(
        frameMin,
        frameMax,
        ImGui::ColorConvertFloat4ToU32(frameBorderColor),
        10.0f, // More rounded corners for a cleaner look
        ImDrawFlags_RoundCornersAll,
        1.0f // Thinner line
    );
}

// Animation utilities - new functions
float AnimateSin(float speed, float min, float max, float phase = 0.0f) {
    float timeOffset = time_since_start * speed;
    return min + (max - min) * (0.5f + 0.5f * sinf(timeOffset + phase));
}

float AnimateExpo(float speed, float min, float max) {
    float t = fmodf(time_since_start * speed, 1.0f);
    float value = t < 0.5f ? 8 * t * t * t * t : 1 - powf(-2 * t + 2, 4) / 2;
    return min + (max - min) * value;
}

float AnimateElastic(float speed, float min, float max) {
    float t = fmodf(time_since_start * speed, 1.0f);
    const float c4 = (2.0f * MY_PI) / 3.0f;
    float factor;
    if (t == 0.0f) {
        factor = 0.0f;
    }
    else if (t == 1.0f) {
        factor = 1.0f;
    }
    else {
        factor = powf(2.0f, -10.0f * t) * sinf((t * 10.0f - 0.75f) * c4) + 1.0f;
    }
    return min + (max - min) * factor;
}

// Animation effect for border corners
void AnimateBorderCorners(const ImVec2& topLeft, const ImVec2& bottomRight, const ImVec4& color, float thickness = 2.0f) {
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    // Use a fixed corner length that matches the border radius
    float cornerLength = 12.0f;

    // Add small offset to account for the border's corner radius (8.0f from DrawMainWindowBorder)
    float cornerOffset = 4.0f;  // Offset to account for rounded corners

    // Calculate colors with pulsating alpha for a glowing effect
    ImVec4 pulsingColor = color;
    pulsingColor.w = GetPulsatingValue(2.0f, 0.5f, 1.0f);
    ImU32 cornerColor = ImGui::ColorConvertFloat4ToU32(pulsingColor);

    // Top-left corner with adjusted positions
    drawList->AddLine(
        ImVec2(topLeft.x + cornerOffset, topLeft.y),
        ImVec2(topLeft.x + cornerLength, topLeft.y),
        cornerColor, thickness);
    drawList->AddLine(
        ImVec2(topLeft.x, topLeft.y + cornerOffset),
        ImVec2(topLeft.x, topLeft.y + cornerLength),
        cornerColor, thickness);

    // Top-right corner with adjusted positions
    drawList->AddLine(
        ImVec2(bottomRight.x - cornerOffset, topLeft.y),
        ImVec2(bottomRight.x - cornerLength, topLeft.y),
        cornerColor, thickness);
    drawList->AddLine(
        ImVec2(bottomRight.x, topLeft.y + cornerOffset),
        ImVec2(bottomRight.x, topLeft.y + cornerLength),
        cornerColor, thickness);

    // Bottom-left corner with adjusted positions
    drawList->AddLine(
        ImVec2(topLeft.x + cornerOffset, bottomRight.y),
        ImVec2(topLeft.x + cornerLength, bottomRight.y),
        cornerColor, thickness);
    drawList->AddLine(
        ImVec2(topLeft.x, bottomRight.y - cornerOffset),
        ImVec2(topLeft.x, bottomRight.y - cornerLength),
        cornerColor, thickness);

    // Bottom-right corner with adjusted positions
    drawList->AddLine(
        ImVec2(bottomRight.x - cornerOffset, bottomRight.y),
        ImVec2(bottomRight.x - cornerLength, bottomRight.y),
        cornerColor, thickness);
    drawList->AddLine(
        ImVec2(bottomRight.x, bottomRight.y - cornerOffset),
        ImVec2(bottomRight.x, bottomRight.y - cornerLength),
        cornerColor, thickness);
}

// Draw animated button with hover effects
bool AnimatedButton(const char* label, const ImVec2& size) {
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4 origBtnColor = style.Colors[ImGuiCol_Button];
    ImVec4 origBtnHoverColor = style.Colors[ImGuiCol_ButtonHovered];
    ImVec4 origBtnActiveColor = style.Colors[ImGuiCol_ButtonActive];

    ImVec4 btnColor, btnHoverColor, btnActiveColor;

    if (rgbModeEnabled) {
        // RGB mode uses animated colors
        float timeOffsetLocal = time_since_start * rgbCycleSpeed;
        float r = 0.5f + 0.5f * sinf(timeOffsetLocal);
        float g = 0.5f + 0.5f * sinf(timeOffsetLocal + 2.0f * MY_PI / 3.0f);
        float b = 0.5f + 0.5f * sinf(timeOffsetLocal + 4.0f * MY_PI / 3.0f);

        btnColor = ImVec4(r * 0.5f, g * 0.5f, b * 0.5f, 0.9f);
        btnHoverColor = ImVec4(r * 0.8f, g * 0.8f, b * 0.8f, 1.0f);
        btnActiveColor = ImVec4(r, g, b, 1.0f);
    }
    else {
        // Standard mode with subtle pulsating effect
        float pulse = GetPulsatingValue(2.0f, 0.8f, 1.0f);
        btnColor = ImVec4(main_color.x * 0.5f, main_color.y * 0.5f, main_color.z * 0.5f, 0.9f);
        btnHoverColor = ImVec4(main_color.x * 0.7f * pulse, main_color.y * 0.7f * pulse, main_color.z * 0.7f * pulse, 1.0f);
        btnActiveColor = ImVec4(main_color.x * pulse, main_color.y * pulse, main_color.z * pulse, 1.0f);
    }

    // Apply the custom colors
    style.Colors[ImGuiCol_Button] = btnColor;
    style.Colors[ImGuiCol_ButtonHovered] = btnHoverColor;
    style.Colors[ImGuiCol_ButtonActive] = btnActiveColor;

    // Draw the button (use only two parameters)
    bool clicked = ImGui::Button(label, size);

    // Restore original colors
    style.Colors[ImGuiCol_Button] = origBtnColor;
    style.Colors[ImGuiCol_ButtonHovered] = origBtnHoverColor;
    style.Colors[ImGuiCol_ButtonActive] = origBtnActiveColor;

    return clicked;
}


void AnimatedSlider(const char* label, float* value, float min, float max, const char* tooltip) {
    ImGui::PushID(label);
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImGuiStyle& style = ImGui::GetStyle();
    float width = ImGui::GetContentRegionAvail().x;

    ImVec4 origFrameBg = style.Colors[ImGuiCol_FrameBg];
    ImVec4 origFrameBgHovered = style.Colors[ImGuiCol_FrameBgHovered];
    ImVec4 origFrameBgActive = style.Colors[ImGuiCol_FrameBgActive];
    ImVec4 origSliderGrab = style.Colors[ImGuiCol_SliderGrab];
    ImVec4 origSliderGrabActive = style.Colors[ImGuiCol_SliderGrabActive];
    float origGrabSize = style.GrabMinSize;
    float origRounding = style.FrameRounding;
    float origFramePaddingY = style.FramePadding.y;

    ImVec4 sliderGrabColor;
    ImVec4 sliderGrabActiveColor;
    ImVec4 frameBgColor;
    ImVec4 valueTextColor = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);

    if (rgbModeEnabled) {
        float timeOffset = time_since_start * rgbCycleSpeed;
        float r = 0.5f + 0.5f * sinf(timeOffset);
        float g = 0.5f + 0.5f * sinf(timeOffset + 2.0f * MY_PI / 3.0f);
        float b = 0.5f + 0.5f * sinf(timeOffset + 4.0f * MY_PI / 3.0f);

        sliderGrabColor = ImVec4(r, g, b, 0.9f);
        sliderGrabActiveColor = ImVec4(r * 1.2f, g * 1.2f, b * 1.2f, 1.0f);
        frameBgColor = ImVec4(r * 0.15f, g * 0.15f, b * 0.15f, 0.8f);
    }
    else {
        sliderGrabColor = main_color;
        sliderGrabColor.w = 0.9f;
        sliderGrabActiveColor = ImVec4(main_color.x * 1.2f, main_color.y * 1.2f, main_color.z * 1.2f, 1.0f);
        frameBgColor = ImVec4(main_color.x * 0.15f, main_color.y * 0.15f, main_color.z * 0.15f, 0.8f);
    }

    style.FrameRounding = 2.0f;
    style.Colors[ImGuiCol_FrameBg] = frameBgColor;
    style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(frameBgColor.x * 1.3f, frameBgColor.y * 1.3f, frameBgColor.z * 1.3f, frameBgColor.w);
    style.Colors[ImGuiCol_FrameBgActive] = ImVec4(frameBgColor.x * 1.5f, frameBgColor.y * 1.5f, frameBgColor.z * 1.5f, frameBgColor.w);
    style.Colors[ImGuiCol_SliderGrab] = sliderGrabColor;
    style.Colors[ImGuiCol_SliderGrabActive] = sliderGrabActiveColor;
    style.GrabMinSize = 8.0f;

    // Make slider thinner
    style.FramePadding.y = 2.0f;

    // Use 90% of the available width and center it
    float sliderWidth = width * 0.90f;
    float leftPadding = (width - sliderWidth) / 2.0f;

    // Format the value in a nice way
    char valueText[32];
    sprintf_s(valueText, "%.2f", *value);

    // Calculate text dimensions for positioning
    ImVec2 labelSize = ImGui::CalcTextSize(label);
    ImVec2 valueSize = ImGui::CalcTextSize(valueText);

    // Draw label perfectly centered
    ImGui::SetCursorPosX(leftPadding + (sliderWidth - labelSize.x) / 2.0f);
    ImGui::Text("%s", label);

    // Position the slider
    ImGui::SetCursorPosX(leftPadding);
    ImGui::PushItemWidth(sliderWidth);

    // Draw slider without displaying the value in the default way
    bool valueChanged = ImGui::SliderFloat("##slider", value, min, max, "", ImGuiSliderFlags_NoInput);

    // Get slider position for visual effects
    ImVec2 sliderMin = ImGui::GetItemRectMin();
    ImVec2 sliderMax = ImGui::GetItemRectMax();
    float sliderHeight = sliderMax.y - sliderMin.y;

    // Draw the background of the slider with a border
    drawList->AddRectFilled(
        sliderMin,
        sliderMax,
        ImGui::ColorConvertFloat4ToU32(frameBgColor),
        style.FrameRounding
    );

    // Determine border color
    ImVec4 borderColor;
    if (rgbModeEnabled) {
        float timeOffset = time_since_start * rgbCycleSpeed;
        float r = 0.5f + 0.5f * sinf(timeOffset);
        float g = 0.5f + 0.5f * sinf(timeOffset + 2.0f * MY_PI / 3.0f);
        float b = 0.5f + 0.5f * sinf(timeOffset + 4.0f * MY_PI / 3.0f);
        borderColor = ImVec4(r * 0.6f, g * 0.6f, b * 0.6f, 0.8f);
    }
    else {
        borderColor = ImVec4(main_color.x * 0.6f, main_color.y * 0.6f, main_color.z * 0.6f, 0.8f);
    }

    // Draw a thin border around the slider
    drawList->AddRect(
        sliderMin,
        sliderMax,
        ImGui::ColorConvertFloat4ToU32(borderColor),
        style.FrameRounding,
        0,
        0.5f // Thinner border
    );

    // Calculate the text position to center it in the slider
    ImVec2 textPos = ImVec2(
        sliderMin.x + (sliderMax.x - sliderMin.x - valueSize.x) / 2.0f,
        sliderMin.y + (sliderMax.y - sliderMin.y - valueSize.y) / 2.0f
    );

    // Draw the value text centered in the slider with a subtle shadow effect
    drawList->AddText(
        ImVec2(textPos.x + 1, textPos.y + 1),
        ImGui::ColorConvertFloat4ToU32(ImVec4(0.0f, 0.0f, 0.0f, 0.3f)),
        valueText
    );
    drawList->AddText(
        textPos,
        ImGui::ColorConvertFloat4ToU32(valueTextColor),
        valueText
    );

    // Add tooltip for hover
    if ((valueChanged || ImGui::IsItemHovered()) && tooltip) {
        ImGui::BeginTooltip();
        ImGui::Text("%s: %s", label, tooltip);
        ImGui::EndTooltip();
    }

    // Restore original style
    style.Colors[ImGuiCol_FrameBg] = origFrameBg;
    style.Colors[ImGuiCol_FrameBgHovered] = origFrameBgHovered;
    style.Colors[ImGuiCol_FrameBgActive] = origFrameBgActive;
    style.Colors[ImGuiCol_SliderGrab] = origSliderGrab;
    style.Colors[ImGuiCol_SliderGrabActive] = origSliderGrabActive;
    style.GrabMinSize = origGrabSize;
    style.FrameRounding = origRounding;
    style.FramePadding.y = origFramePaddingY;

    ImGui::PopItemWidth();
    ImGui::PopID();
}

// Animated toggle button with smooth transitions
bool AnimatedToggleButton(const char* label, bool* value) {
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    float height = ImGui::GetFrameHeight();
    float width = height * 1.8f;
    float radius = height * 0.5f;

    ImGui::InvisibleButton(label, ImVec2(width, height));
    bool changed = false;
    if (ImGui::IsItemClicked()) {
        *value = !*value;
        changed = true;
    }

    // Instead of using map for animations, use a simpler approach
    // Calculate animation based on current value (0.0 or 1.0)
    float t = *value ? 1.0f : 0.0f;

    // Colors
    ImU32 bgColor, knobColor;
    if (rgbModeEnabled) {
        // RGB mode
        float timeOffsetLocal = time_since_start * rgbCycleSpeed;
        float r = 0.5f + 0.5f * sinf(timeOffsetLocal);
        float g = 0.5f + 0.5f * sinf(timeOffsetLocal + 2.0f * MY_PI / 3.0f);
        float b = 0.5f + 0.5f * sinf(timeOffsetLocal + 4.0f * MY_PI / 3.0f);

        ImVec4 offColor = ImVec4(0.2f, 0.2f, 0.2f, 1.0f);
        ImVec4 onColor = ImVec4(r, g, b, 1.0f);
        ImVec4 blendedColor = ImVec4(
            offColor.x + (onColor.x - offColor.x) * t,
            offColor.y + (onColor.y - offColor.y) * t,
            offColor.z + (onColor.z - offColor.z) * t,
            1.0f
        );

        bgColor = ImGui::ColorConvertFloat4ToU32(blendedColor);
        knobColor = ImGui::ColorConvertFloat4ToU32(ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
    }
    else {
        // Standard mode with animation
        ImVec4 offColor = ImVec4(0.2f, 0.2f, 0.2f, 1.0f);
        ImVec4 onColor = main_color;
        ImVec4 blendedColor = ImVec4(
            offColor.x + (onColor.x - offColor.x) * t,
            offColor.y + (onColor.y - offColor.y) * t,
            offColor.z + (onColor.z - offColor.z) * t,
            1.0f
        );

        // Add pulsating effect when on
        if (*value) {
            float pulse = GetPulsatingValue(3.0f, 0.9f, 1.1f);
            blendedColor.x *= pulse;
            blendedColor.y *= pulse;
            blendedColor.z *= pulse;
        }

        bgColor = ImGui::ColorConvertFloat4ToU32(blendedColor);
        knobColor = ImGui::ColorConvertFloat4ToU32(ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
    }

    // Draw background track with rounded corners
    drawList->AddRectFilled(p, ImVec2(p.x + width, p.y + height), bgColor, height * 0.5f);

    // Add subtle border
    drawList->AddRect(p, ImVec2(p.x + width, p.y + height), ImGui::ColorConvertFloat4ToU32(ImVec4(0.6f, 0.1f, 0.1f, 1.0f)), height * 0.5f, 0, 1.0f);

    // Calculate the circle position with animation
    float posX = p.x + radius + t * (width - radius * 2.0f);

    // Draw a subtle shadow for the knob
    drawList->AddCircleFilled(ImVec2(posX + 1, p.y + radius + 1), radius * 0.8f, ImGui::ColorConvertFloat4ToU32(ImVec4(0.0f, 0.0f, 0.0f, 0.3f)));

    // Draw the knob (circle)
    drawList->AddCircleFilled(ImVec2(posX, p.y + radius), radius * 0.8f, knobColor);

    // Add a subtle highlight to the knob
    if (*value) {
        drawList->AddCircle(ImVec2(posX, p.y + radius), radius * 0.6f, ImGui::ColorConvertFloat4ToU32(ImVec4(1.0f, 1.0f, 1.0f, 0.5f)), 0, 1.5f);
    }

    // Add an indicator in the center of the knob
    if (*value) {
        drawList->AddCircleFilled(ImVec2(posX, p.y + radius), radius * 0.3f, ImGui::ColorConvertFloat4ToU32(ImVec4(0.8f, 0.0f, 0.0f, 1.0f)));
    }

    return changed;
}

// Advanced separator with glow effect and customizable style
void DrawAdvancedSeparator(bool rgbMode, float widthFactor, float yFactor) {
    // Get colors from our global values
    ImVec4 sepColor = rgbMode ?
        ImVec4(
            0.5f + 0.5f * sinf(time_since_start * rgbCycleSpeed),
            0.5f + 0.5f * sinf(time_since_start * rgbCycleSpeed + 2.0f * MY_PI / 3.0f),
            0.5f + 0.5f * sinf(time_since_start * rgbCycleSpeed + 4.0f * MY_PI / 3.0f),
            0.8f
        ) : ImVec4(main_color.x, main_color.y, main_color.z, 0.8f);

    // Get window dimensions
    float windowWidth = ImGui::GetWindowWidth();
    float contentWidth = ImGui::GetContentRegionAvail().x;

    // Calculate the separator width based on the content width and the provided factor
    float lineWidth = contentWidth * widthFactor;

    // Centering offset (can be adjusted as needed)
    float centeringOffset = 20.0f;

    // Add spacing before separator
    ImGui::Dummy(ImVec2(0, 2.0f * yFactor));

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 windowPos = ImGui::GetWindowPos();

    // Start position - centered horizontally
    float startX = windowPos.x + (windowWidth - lineWidth) / 2.0f - centeringOffset;
    float startY = ImGui::GetCursorScreenPos().y;

    // Calculate proper end position with margin to avoid going through borders
    // Right border margin to match the left side and avoid drawing through borders
    float rightBorderMargin = 16.0f;
    float endX = Min(startX + lineWidth, windowPos.x + windowWidth - rightBorderMargin);

    // Slightly thinner, more polished line thickness
    float lineThickness = 0.8f * yFactor;

    // Left side rounded cap
    drawList->AddCircleFilled(
        ImVec2(startX, startY),
        lineThickness / 2.0f,
        ImGui::ColorConvertFloat4ToU32(sepColor),
        12
    );

    // Line segment
    drawList->AddLine(
        ImVec2(startX, startY),
        ImVec2(endX, startY),
        ImGui::ColorConvertFloat4ToU32(sepColor),
        lineThickness
    );

    // Right side rounded cap
    drawList->AddCircleFilled(
        ImVec2(endX, startY),
        lineThickness / 2.0f,
        ImGui::ColorConvertFloat4ToU32(sepColor),
        12
    );

    // Add dummy element to advance cursor
    ImGui::Dummy(ImVec2(0, 2.0f * yFactor));
}

// Draw a panel with visual enhancements (for grouping controls)
void DrawEnhancedPanel(const char* label, float alpha = 0.1f) {
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImVec2 contentSize = ImGui::GetContentRegionAvail();

    // Get the content height based on label
    ImGui::BeginGroup();
    ImGui::TextUnformatted(label);
    ImGui::Spacing();

    // Start indentation for content
    ImGui::Indent(10.0f);

    // Save position after the label
    ImVec2 contentStart = ImGui::GetCursorScreenPos();

    // Reserve space for background (will be drawn in EndEnhancedPanel)
    ImGui::PushID(label);
}

// End the enhanced panel and draw its background
void EndEnhancedPanel(bool rgbMode) {
    // End the indentation
    ImGui::Unindent(10.0f);

    // End the group
    ImGui::EndGroup();

    // Now draw the background and border
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 min = ImGui::GetItemRectMin();
    ImVec2 max = ImGui::GetItemRectMax();

    // Calculate panel dimensions
    float width = max.x - min.x;
    float height = max.y - min.y;

    // Get color based on mode
    ImVec4 borderColor;
    if (rgbMode) {
        float timeOffset = time_since_start * rgbCycleSpeed;
        float r = 0.5f + 0.5f * sinf(timeOffset);
        float g = 0.5f + 0.5f * sinf(timeOffset + 2.0f * MY_PI / 3.0f);
        float b = 0.5f + 0.5f * sinf(timeOffset + 4.0f * MY_PI / 3.0f);
        borderColor = ImVec4(r, g, b, 0.7f);
    }
    else {
        float pulse = AnimateSin(1.5f, 0.7f, 1.0f, 0.0f);
        borderColor = ImVec4(main_color.x * pulse, main_color.y * pulse, main_color.z * pulse, 0.7f);
    }

    // Background with higher transparency
    ImVec4 bgColor = ImVec4(0.08f, 0.08f, 0.08f, 0.3f);

    // Draw a clean rounded background
    drawList->AddRectFilled(
        min,
        max,
        ImGui::ColorConvertFloat4ToU32(bgColor),
        8.0f // More rounded corners for cleaner look
    );

    // Draw a smooth border with higher rounding
    drawList->AddRect(
        min,
        max,
        ImGui::ColorConvertFloat4ToU32(borderColor),
        8.0f, // Increased from 4.0f for more rounded corners
        ImDrawFlags_RoundCornersAll,
        1.0f // Thin border for clean look
    );

    // Pop the ID pushed in DrawEnhancedPanel
    ImGui::PopID();

    // Add some spacing after the panel
    ImGui::Spacing();
    ImGui::Spacing();
}

// Enhanced professional slider with visual indicators
bool AnimatedProfessionalSlider(const char* label, float* value, float min, float max, const char* format, const char* tooltip) {
    ImGui::PushID(label);
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    // Store original style settings
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4 origFrameBg = style.Colors[ImGuiCol_FrameBg];
    ImVec4 origFrameBgHovered = style.Colors[ImGuiCol_FrameBgHovered];
    ImVec4 origFrameBgActive = style.Colors[ImGuiCol_FrameBgActive];
    ImVec4 origSliderGrab = style.Colors[ImGuiCol_SliderGrab];
    ImVec4 origSliderGrabActive = style.Colors[ImGuiCol_SliderGrabActive];
    float origRounding = style.FrameRounding;
    float origGrabRounding = style.GrabRounding;
    float origFramePaddingY = style.FramePadding.y;

    // Set up the color scheme based on RGB mode
    ImVec4 sliderColor, sliderActiveColor, bgColor, textColor, borderColor;

    if (rgbModeEnabled) {
        // RGB mode colors
        float timeOffset = time_since_start * rgbCycleSpeed;
        float r = 0.5f + 0.5f * sinf(timeOffset);
        float g = 0.5f + 0.5f * sinf(timeOffset + 2.0f * MY_PI / 3.0f);
        float b = 0.5f + 0.5f * sinf(timeOffset + 4.0f * MY_PI / 3.0f);

        sliderColor = ImVec4(r, g, b, 0.9f);
        sliderActiveColor = ImVec4(r * 1.2f, g * 1.2f, b * 1.2f, 1.0f);
        bgColor = ImVec4(r * 0.15f, g * 0.15f, b * 0.15f, 0.8f);
        borderColor = ImVec4(r * 0.6f, g * 0.6f, b * 0.6f, 0.8f);
        textColor = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
    }
    else {
        // Regular theme based on main_color
        sliderColor = main_color;
        sliderColor.w = 0.9f;
        sliderActiveColor = ImVec4(main_color.x * 1.2f, main_color.y * 1.2f, main_color.z * 1.2f, 1.0f);
        bgColor = ImVec4(main_color.x * 0.15f, main_color.y * 0.15f, main_color.z * 0.15f, 0.8f);
        textColor = ImVec4(1.0f, 1.0f, 1.0f, 1.0f); // White text for better visibility
        borderColor = ImVec4(main_color.x * 0.6f, main_color.y * 0.6f, main_color.z * 0.6f, 0.8f);
    }

    // Make slider rounded for a clean look
    style.FrameRounding = 2.0f;
    style.GrabRounding = 2.0f;

    // Make slider thinner
    style.FramePadding.y = 1.5f; // Even thinner padding (was 2.0f)

    // Customize colors for this slider
    style.Colors[ImGuiCol_FrameBg] = bgColor;
    style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(bgColor.x * 1.2f, bgColor.y * 1.2f, bgColor.z * 1.2f, bgColor.w);
    style.Colors[ImGuiCol_FrameBgActive] = ImVec4(bgColor.x * 1.5f, bgColor.y * 1.5f, bgColor.z * 1.5f, bgColor.w);
    style.Colors[ImGuiCol_SliderGrab] = sliderColor;
    style.Colors[ImGuiCol_SliderGrabActive] = sliderActiveColor;

    // Make sliders with smaller grab
    float originalGrabSize = style.GrabMinSize;
    style.GrabMinSize = 6.0f; // Smaller grab size (was 8.0f)

    // Calculate current positions for layout
    float totalWidth = ImGui::GetContentRegionAvail().x;
    float sliderWidth = totalWidth * 0.85f; // Slightly narrower width (was 0.90f)
    float leftPadding = (totalWidth - sliderWidth) / 2.0f;

    // Format the value text
    char labelText[64];
    sprintf_s(labelText, "%s", label);

    char valueText[32];
    sprintf_s(valueText, format, *value);

    // Calculate text dimensions
    ImVec2 labelSize = ImGui::CalcTextSize(labelText);
    ImVec2 valueSize = ImGui::CalcTextSize(valueText);

    // Use slightly smaller font for label too
    float labelFontScale = ImGui::GetFont()->Scale;
    ImGui::GetFont()->Scale *= 0.92f; // Slightly smaller font

    // Recalculate label size with smaller font
    labelSize = ImGui::CalcTextSize(labelText);

    // Draw the label perfectly centered
    ImGui::SetCursorPosX(leftPadding + (sliderWidth - labelSize.x) / 2.0f);
    ImGui::Text("%s", labelText);

    // Less spacing before slider
    ImGui::Dummy(ImVec2(0, 1.0f)); // Was ImGui::Spacing()

    // Restore font size
    ImGui::GetFont()->Scale = labelFontScale;

    // Position the slider
    ImGui::SetCursorPosX(leftPadding);
    ImGui::PushItemWidth(sliderWidth);

    // Draw slider without displaying the default value
    bool changed = ImGui::SliderFloat("##slider", value, min, max, "", ImGuiSliderFlags_NoInput);

    // Get positions for custom drawing
    ImVec2 sliderMin = ImGui::GetItemRectMin();
    ImVec2 sliderMax = ImGui::GetItemRectMax();
    float sliderHeight = sliderMax.y - sliderMin.y;

    // Draw the slider background with border
    drawList->AddRectFilled(
        sliderMin,
        sliderMax,
        ImGui::ColorConvertFloat4ToU32(bgColor),
        style.FrameRounding
    );

    // Add a subtle border
    drawList->AddRect(
        sliderMin,
        sliderMax,
        ImGui::ColorConvertFloat4ToU32(borderColor),
        style.FrameRounding,
        0,
        0.3f // Even thinner border (was 0.5f)
    );

    // Calculate the text position to center it in the slider
    ImVec2 textPos = ImVec2(
        sliderMin.x + (sliderMax.x - sliderMin.x - valueSize.x) / 2.0f,
        sliderMin.y + (sliderMax.y - sliderMin.y - valueSize.y) / 2.0f
    );

    // Use a slightly smaller font size for the value text
    float origFontScale = ImGui::GetFont()->Scale;
    ImGui::GetFont()->Scale *= 0.9f; // 10% smaller font

    // Recalculate text size with smaller font
    ImVec2 newValueSize = ImGui::CalcTextSize(valueText);
    textPos = ImVec2(
        sliderMin.x + (sliderMax.x - sliderMin.x - newValueSize.x) / 2.0f,
        sliderMin.y + (sliderMax.y - sliderMin.y - newValueSize.y) / 2.0f
    );

    // Draw the value text with subtle shadow
    drawList->AddText(
        ImGui::GetFont(),
        ImGui::GetFont()->FontSize,
        ImVec2(textPos.x + 1, textPos.y + 1),
        ImGui::ColorConvertFloat4ToU32(ImVec4(0.0f, 0.0f, 0.0f, 0.2f)), // More subtle shadow (was 0.3f)
        valueText
    );

    drawList->AddText(
        ImGui::GetFont(),
        ImGui::GetFont()->FontSize,
        textPos,
        ImGui::ColorConvertFloat4ToU32(textColor),
        valueText
    );

    // Restore original font size
    ImGui::GetFont()->Scale = origFontScale;

    // Add tooltip if provided
    if (tooltip && ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::Text("%s: %s", label, tooltip);
        ImGui::EndTooltip();
    }

    // Restore original style
    style.Colors[ImGuiCol_FrameBg] = origFrameBg;
    style.Colors[ImGuiCol_FrameBgHovered] = origFrameBgHovered;
    style.Colors[ImGuiCol_FrameBgActive] = origFrameBgActive;
    style.Colors[ImGuiCol_SliderGrab] = origSliderGrab;
    style.Colors[ImGuiCol_SliderGrabActive] = origSliderGrabActive;
    style.GrabMinSize = originalGrabSize;
    style.FrameRounding = origRounding;
    style.GrabRounding = origGrabRounding;
    style.FramePadding.y = origFramePaddingY;

    ImGui::PopItemWidth();
    ImGui::PopID();
    return changed;
}

// Premium button with advanced effects
bool PremiumButton(const char* label, const ImVec2& size) {
    // If size is empty (0,0), use defaults
    ImVec2 buttonSize = size;

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImGui::PushID(label);

    // Store original style
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4 origButtonCol = style.Colors[ImGuiCol_Button];
    ImVec4 origButtonHoveredCol = style.Colors[ImGuiCol_ButtonHovered];
    ImVec4 origButtonActiveCol = style.Colors[ImGuiCol_ButtonActive];

    // Get colors based on current theme
    ImVec4 buttonColor, hoverColor, activeColor;

    if (rgbModeEnabled) {
        float timeOffset = time_since_start * rgbCycleSpeed;
        float r = 0.5f + 0.5f * sinf(timeOffset);
        float g = 0.5f + 0.5f * sinf(timeOffset + 2.0f * MY_PI / 3.0f);
        float b = 0.5f + 0.5f * sinf(timeOffset + 4.0f * MY_PI / 3.0f);

        buttonColor = ImVec4(r * 0.3f, g * 0.3f, b * 0.3f, 1.0f);
        hoverColor = ImVec4(r * 0.5f, g * 0.5f, b * 0.5f, 1.0f);
        activeColor = ImVec4(r * 0.7f, g * 0.7f, b * 0.7f, 1.0f);
    }
    else {
        float pulse = AnimateSin(2.0f, 0.7f, 1.0f, 0.0f);
        buttonColor = ImVec4(main_color.x * 0.3f, main_color.y * 0.3f, main_color.z * 0.3f, 1.0f);
        hoverColor = ImVec4(main_color.x * 0.5f * pulse, main_color.y * 0.5f * pulse, main_color.z * 0.5f * pulse, 1.0f);
        activeColor = ImVec4(main_color.x * 0.7f, main_color.y * 0.7f, main_color.z * 0.7f, 1.0f);
    }

    // Apply theme colors
    style.Colors[ImGuiCol_Button] = buttonColor;
    style.Colors[ImGuiCol_ButtonHovered] = hoverColor;
    style.Colors[ImGuiCol_ButtonActive] = activeColor;

    // Calculate ideal button size
    if (buttonSize.x <= 0) buttonSize.x = ImGui::GetContentRegionAvail().x;
    if (buttonSize.y <= 0) buttonSize.y = ImGui::GetFrameHeight() * 1.5f;

    // Get start position
    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImVec2 textSize = ImGui::CalcTextSize(label);
    ImVec2 textPos = ImVec2(
        pos.x + (buttonSize.x - textSize.x) * 0.5f,
        pos.y + (buttonSize.y - textSize.y) * 0.5f
    );

    // Draw button
    bool clicked = ImGui::Button("##premium_button", buttonSize);

    // Get button state for visual effects
    bool hovered = ImGui::IsItemHovered();
    bool active = ImGui::IsItemActive();

    // Draw button background ourselves to add enhanced effects
    ImVec2 min = ImGui::GetItemRectMin();
    ImVec2 max = ImGui::GetItemRectMax();

    // Draw animated border accent
    if (hovered || active) {
        float borderTime = time_since_start * 3.0f;
        float borderThickness = active ? 2.0f : 1.5f;
        ImVec4 borderColor = active ? activeColor : hoverColor;
        borderColor.w = 1.0f;

        // Draw animated corners
        float cornerSize = 10.0f;
        float animOffset = fmodf(borderTime, 8.0f) - 4.0f; // -4 to 4 range
        float animPos = fabsf(animOffset) * 0.25f; // 0 to 1 range

        // Adjust corner size based on animation
        float animatedCornerSize = cornerSize * (1.0f + animPos * 0.5f);

        // Top-left corner
        drawList->AddLine(
            ImVec2(min.x, min.y + animatedCornerSize),
            ImVec2(min.x, min.y),
            ImGui::ColorConvertFloat4ToU32(borderColor),
            borderThickness
        );
        drawList->AddLine(
            ImVec2(min.x, min.y),
            ImVec2(min.x + animatedCornerSize, min.y),
            ImGui::ColorConvertFloat4ToU32(borderColor),
            borderThickness
        );

        // Top-right corner
        drawList->AddLine(
            ImVec2(max.x - animatedCornerSize, min.y),
            ImVec2(max.x, min.y),
            ImGui::ColorConvertFloat4ToU32(borderColor),
            borderThickness
        );
        drawList->AddLine(
            ImVec2(max.x, min.y),
            ImVec2(max.x, min.y + animatedCornerSize),
            ImGui::ColorConvertFloat4ToU32(borderColor),
            borderThickness
        );

        // Bottom-left corner
        drawList->AddLine(
            ImVec2(min.x, max.y - animatedCornerSize),
            ImVec2(min.x, max.y),
            ImGui::ColorConvertFloat4ToU32(borderColor),
            borderThickness
        );
        drawList->AddLine(
            ImVec2(min.x, max.y),
            ImVec2(min.x + animatedCornerSize, max.y),
            ImGui::ColorConvertFloat4ToU32(borderColor),
            borderThickness
        );

        // Bottom-right corner
        drawList->AddLine(
            ImVec2(max.x - animatedCornerSize, max.y),
            ImVec2(max.x, max.y),
            ImGui::ColorConvertFloat4ToU32(borderColor),
            borderThickness
        );
        drawList->AddLine(
            ImVec2(max.x, max.y),
            ImVec2(max.x, max.y - animatedCornerSize),
            ImGui::ColorConvertFloat4ToU32(borderColor),
            borderThickness
        );
    }

    // Draw text with optional shadow for depth
    if (active) {
        // Apply a subtle text shadow for pressed state
        ImVec4 shadowColor = ImVec4(0.0f, 0.0f, 0.0f, 0.7f);
        drawList->AddText(
            ImVec2(textPos.x + 1, textPos.y + 1),
            ImGui::ColorConvertFloat4ToU32(shadowColor),
            label
        );
    }

    // Draw the actual text
    ImVec4 textColor = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
    drawList->AddText(textPos, ImGui::ColorConvertFloat4ToU32(textColor), label);

    // Restore original style
    style.Colors[ImGuiCol_Button] = origButtonCol;
    style.Colors[ImGuiCol_ButtonHovered] = origButtonHoveredCol;
    style.Colors[ImGuiCol_ButtonActive] = origButtonActiveCol;

    ImGui::PopID();
    return clicked;
}

// Function to draw an inner content border with similar style to main border but smaller
void DrawInnerContentBorder()
{
    ImVec2 windowPos = ImGui::GetWindowPos();
    ImVec2 windowSize = ImGui::GetWindowSize();

    // Define inner border dimensions with proper margin
    float marginX = 12.0f;
    float marginY = 60.0f; // Larger top margin to account for the header
    float marginBottom = 12.0f;

    // Add padding to ensure the border doesn't interfere with separators
    float separatorPadding = 2.0f;

    ImVec2 innerStart = ImVec2(windowPos.x + marginX + separatorPadding, windowPos.y + marginY);
    ImVec2 innerEnd = ImVec2(windowPos.x + windowSize.x - marginX - separatorPadding, windowPos.y + windowSize.y - marginBottom);

    ImDrawList* drawList = ImGui::GetWindowDrawList();

    // Get border color
    ImVec4 borderColor;
    if (rgbModeEnabled) {
        float timeOffset = time_since_start * rgbCycleSpeed;
        float r = 0.5f + 0.5f * sinf(timeOffset);
        float g = 0.5f + 0.5f * sinf(timeOffset + 2.0f * MY_PI / 3.0f);
        float b = 0.5f + 0.5f * sinf(timeOffset + 4.0f * MY_PI / 3.0f);
        borderColor = ImVec4(r, g, b, 0.7f);
    }
    else {
        // Use main color with subtle pulsating effect
        float pulse = AnimateSin(1.5f, 0.9f, 1.0f, 0.0f);
        borderColor = ImVec4(main_color.x * pulse, main_color.y * pulse, main_color.z * pulse, 0.7f);
    }

    // Use a slightly thinner line for better visual appearance
    float thickness = 0.8f;

    // Draw four separate lines to avoid corner issues
    // Top line
    drawList->AddLine(
        ImVec2(innerStart.x, innerStart.y),
        ImVec2(innerEnd.x, innerStart.y),
        ImGui::ColorConvertFloat4ToU32(borderColor),
        thickness
    );

    // Right line
    drawList->AddLine(
        ImVec2(innerEnd.x, innerStart.y),
        ImVec2(innerEnd.x, innerEnd.y),
        ImGui::ColorConvertFloat4ToU32(borderColor),
        thickness
    );

    // Bottom line
    drawList->AddLine(
        ImVec2(innerEnd.x, innerEnd.y),
        ImVec2(innerStart.x, innerEnd.y),
        ImGui::ColorConvertFloat4ToU32(borderColor),
        thickness
    );

    // Left line
    drawList->AddLine(
        ImVec2(innerStart.x, innerEnd.y),
        ImVec2(innerStart.x, innerStart.y),
        ImGui::ColorConvertFloat4ToU32(borderColor),
        thickness
    );
}

// Special slider for audio panning with clean minimal design
bool AnimatedPanningSlider(const char* label, float* value, float min, float max) {
    ImGui::PushID(label);
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImGuiStyle& style = ImGui::GetStyle();
    float width = ImGui::GetContentRegionAvail().x;

    // Store original style
    ImVec4 origFrameBg = style.Colors[ImGuiCol_FrameBg];
    ImVec4 origFrameBgHovered = style.Colors[ImGuiCol_FrameBgHovered];
    ImVec4 origFrameBgActive = style.Colors[ImGuiCol_FrameBgActive];
    ImVec4 origSliderGrab = style.Colors[ImGuiCol_SliderGrab];
    ImVec4 origSliderGrabActive = style.Colors[ImGuiCol_SliderGrabActive];
    float origGrabSize = style.GrabMinSize;
    float origRounding = style.FrameRounding;
    float origFramePaddingY = style.FramePadding.y;

    // Set colors and style for clean minimal design
    ImVec4 sliderGrabColor;
    ImVec4 sliderGrabActiveColor;
    ImVec4 frameBgColor;
    ImVec4 valueTextColor = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
    ImVec4 leftColor;
    ImVec4 rightColor;
    ImVec4 centerColor;
    ImVec4 borderColor;

    // Apply RGB mode if enabled
    if (rgbModeEnabled) {
        // RGB mode colors - match the standard pattern used throughout the codebase
        float timeOffset = time_since_start * rgbCycleSpeed;
        float r = 0.5f + 0.5f * sinf(timeOffset);
        float g = 0.5f + 0.5f * sinf(timeOffset + 2.0f * MY_PI / 3.0f);
        float b = 0.5f + 0.5f * sinf(timeOffset + 4.0f * MY_PI / 3.0f);

        // Apply RGB coloring to various elements
        sliderGrabColor = ImVec4(r, g, b, 0.9f);
        sliderGrabActiveColor = ImVec4(r * 1.2f, g * 1.2f, b * 1.2f, 1.0f);

        // Left and right indicators with appropriate opacity
        leftColor = ImVec4(r, g, b, *value < 0 ? 0.9f : 0.4f);
        rightColor = ImVec4(r, g, b, *value > 0 ? 0.9f : 0.4f);

        // Center and border with RGB colors
        centerColor = ImVec4(r * 0.8f, g * 0.8f, b * 0.8f, 0.7f);
        borderColor = ImVec4(r * 0.6f, g * 0.6f, b * 0.6f, 0.8f);

        // Frame background with darkened RGB
        frameBgColor = ImVec4(r * 0.15f, g * 0.15f, b * 0.15f, 0.8f);
    }
    else {
        // Regular theme based on main_color
        sliderGrabColor = main_color;
        sliderGrabColor.w = 0.9f;

        sliderGrabActiveColor = ImVec4(main_color.x * 1.2f, main_color.y * 1.2f, main_color.z * 1.2f, 1.0f);

        leftColor = ImVec4(main_color.x, main_color.y, main_color.z, *value < 0 ? 0.9f : 0.4f);
        rightColor = ImVec4(main_color.x, main_color.y, main_color.z, *value > 0 ? 0.9f : 0.4f);

        centerColor = ImVec4(main_color.x * 0.8f, main_color.y * 0.8f, main_color.z * 0.8f, 0.7f);
        borderColor = ImVec4(main_color.x * 0.6f, main_color.y * 0.6f, main_color.z * 0.6f, 0.8f);

        frameBgColor = ImVec4(main_color.x * 0.15f, main_color.y * 0.15f, main_color.z * 0.15f, 0.8f);
    }

    // Apply clean style
    style.FrameRounding = 2.0f;
    style.Colors[ImGuiCol_FrameBg] = frameBgColor;
    style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(frameBgColor.x * 1.3f, frameBgColor.y * 1.3f, frameBgColor.z * 1.3f, frameBgColor.w);
    style.Colors[ImGuiCol_FrameBgActive] = ImVec4(frameBgColor.x * 1.5f, frameBgColor.y * 1.5f, frameBgColor.z * 1.5f, frameBgColor.w);
    style.Colors[ImGuiCol_SliderGrab] = sliderGrabColor;
    style.Colors[ImGuiCol_SliderGrabActive] = sliderGrabActiveColor;

    // Make slider thinner
    style.GrabMinSize = 8.0f;
    style.FramePadding.y = 2.0f;

    // Use 90% of the width, matching other sliders
    float sliderWidth = width * 0.90f;
    float leftPadding = (width - sliderWidth) / 2.0f;

    // Format display text
    const char* valueText = FormatPanningText(*value);

    // Draw label centered
    ImVec2 labelSize = ImGui::CalcTextSize(label);
    ImGui::SetCursorPosX(leftPadding + (sliderWidth - labelSize.x) / 2.0f);
    ImGui::Text("%s", label);

    // Draw the slider
    ImGui::SetCursorPosX(leftPadding);
    ImGui::PushItemWidth(sliderWidth);
    bool valueChanged = ImGui::SliderFloat("##panning", value, min, max, "", ImGuiSliderFlags_NoInput);

    // Get slider position for custom drawing
    ImVec2 sliderMin = ImGui::GetItemRectMin();
    ImVec2 sliderMax = ImGui::GetItemRectMax();
    float sliderHeight = sliderMax.y - sliderMin.y;

    // Draw background with thin border
    drawList->AddRectFilled(sliderMin, sliderMax, ImGui::ColorConvertFloat4ToU32(frameBgColor), style.FrameRounding);
    drawList->AddRect(sliderMin, sliderMax, ImGui::ColorConvertFloat4ToU32(borderColor), style.FrameRounding, 0, 0.5f);

    // Calculate the center position
    float centerX = sliderMin.x + (sliderMax.x - sliderMin.x) * 0.5f;

    // Draw center line
    drawList->AddLine(
        ImVec2(centerX, sliderMin.y + 2),
        ImVec2(centerX, sliderMax.y - 2),
        ImGui::ColorConvertFloat4ToU32(centerColor),
        0.5f // Thinner line
    );

    // Show "L" and "R" indicators subtly at the edges
    float textY = sliderMin.y + (sliderHeight - ImGui::GetFontSize()) / 2.0f;

    // Left indicator
    drawList->AddText(
        ImVec2(sliderMin.x + 4, textY),
        ImGui::ColorConvertFloat4ToU32(leftColor),
        "L"
    );

    // Right indicator
    drawList->AddText(
        ImVec2(sliderMax.x - 12, textY),
        ImGui::ColorConvertFloat4ToU32(rightColor),
        "R"
    );

    // Calculate the text position to center it in the slider
    ImVec2 valueSize = ImGui::CalcTextSize(valueText);
    ImVec2 textPos = ImVec2(
        sliderMin.x + (sliderMax.x - sliderMin.x - valueSize.x) / 2.0f,
        sliderMin.y + (sliderMax.y - sliderMin.y - valueSize.y) / 2.0f
    );

    // Draw value text with subtle shadow
    drawList->AddText(
        ImVec2(textPos.x + 1, textPos.y + 1),
        ImGui::ColorConvertFloat4ToU32(ImVec4(0.0f, 0.0f, 0.0f, 0.3f)),
        valueText
    );
    drawList->AddText(
        textPos,
        ImGui::ColorConvertFloat4ToU32(valueTextColor),
        valueText
    );

    // Restore original style
    style.Colors[ImGuiCol_FrameBg] = origFrameBg;
    style.Colors[ImGuiCol_FrameBgHovered] = origFrameBgHovered;
    style.Colors[ImGuiCol_FrameBgActive] = origFrameBgActive;
    style.Colors[ImGuiCol_SliderGrab] = origSliderGrab;
    style.Colors[ImGuiCol_SliderGrabActive] = origSliderGrabActive;
    style.GrabMinSize = origGrabSize;
    style.FrameRounding = origRounding;
    style.FramePadding.y = origFramePaddingY;

    ImGui::PopItemWidth();
    ImGui::PopID();
    return valueChanged;
}

// Helper function to get current process name
std::string GetProcessName() {
    char buffer[MAX_PATH] = "Unknown";
    GetModuleFileNameA(NULL, buffer, MAX_PATH);
    std::string path(buffer);
    size_t pos = path.find_last_of("\\/");
    std::string processName = (pos != std::string::npos) ? path.substr(pos + 1) : path;

    // Get PID of current process
    DWORD pid = GetCurrentProcessId();

    // Get process handle
    HANDLE hProcess = GetCurrentProcess();

    // Get the injection address - this is our own module
    HMODULE hModule = NULL;
    DWORD flags = GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT;
    GetModuleHandleExA(flags, (LPCSTR)&GetProcessName, &hModule);

    // Get the module size and base address info
    MODULEINFO moduleInfo;
    if (GetModuleInformation(hProcess, hModule, &moduleInfo, sizeof(moduleInfo))) {
        // Get the target module for hooking (discord_voice)
        HMODULE hDiscordVoice = GetModuleHandleA("discord_voice.node");
        void* hookTargetAddr = nullptr;

        if (hDiscordVoice) {
            // Note: 0x863E90 is the offset used in the hooking function
            hookTargetAddr = (char*)hDiscordVoice + 0x863E90;
        }

        char processLine[64] = { 0 };
        char baseLine[64] = { 0 };
        char hookLine[64] = { 0 };

        sprintf_s(processLine, "Process: %s | PID: %lu", processName.c_str(), pid);
        sprintf_s(baseLine, "Base: 0x%p | Size: %uKB", moduleInfo.lpBaseOfDll, moduleInfo.SizeOfImage / 1024);
        sprintf_s(hookLine, "Hooked: 0x%p [opus_encode]", hookTargetAddr);

        return std::string(processLine) + "\n" + baseLine + "\n" + hookLine;
    }

    // Fallback if GetModuleInformation fails
    char processLine[64] = { 0 };
    char moduleLine[64] = { 0 };

    sprintf_s(processLine, "Process: %s | PID: %lu", processName.c_str(), pid);
    sprintf_s(moduleLine, "Module: 0x%p", (void*)hModule);

    return std::string(processLine) + "\n" + moduleLine;
}
