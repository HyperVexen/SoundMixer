#include "UI.h"
#include "MicCaptureSource.h"
#include "ProcessCaptureSource.h"
#include "ConfigManager.h"
#include "../third_party/imgui/imgui.h"
#include <windows.h>
#include <commdlg.h>
#include <algorithm>
#include <cmath>

SoundMixerUI::SoundMixerUI(AudioEngine& engine)
    : m_engine(engine), m_bridge(engine) {
    applyDarkTheme();
}

SoundMixerUI::~SoundMixerUI() = default;

void SoundMixerUI::applyDarkTheme() {
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;

    style.WindowRounding = 8.0f;
    style.ChildRounding = 6.0f;
    style.FrameRounding = 5.0f;
    style.PopupRounding = 6.0f;
    style.ScrollbarRounding = 6.0f;
    style.GrabRounding = 4.0f;
    style.TabRounding = 5.0f;

    style.WindowPadding = ImVec2(14.0f, 14.0f);
    style.FramePadding = ImVec2(8.0f, 5.0f);
    style.ItemSpacing = ImVec2(10.0f, 8.0f);
    style.ItemInnerSpacing = ImVec2(6.0f, 4.0f);
    style.ScrollbarSize = 12.0f;
    style.GrabMinSize = 10.0f;

    // Dark sleek cyberpunk/gaming palette
    colors[ImGuiCol_Text]                  = ImVec4(0.95f, 0.96f, 0.98f, 1.00f);
    colors[ImGuiCol_TextDisabled]          = ImVec4(0.50f, 0.53f, 0.58f, 1.00f);
    colors[ImGuiCol_WindowBg]              = ImVec4(0.09f, 0.10f, 0.12f, 1.00f);
    colors[ImGuiCol_ChildBg]               = ImVec4(0.12f, 0.13f, 0.16f, 0.80f);
    colors[ImGuiCol_PopupBg]               = ImVec4(0.12f, 0.13f, 0.16f, 0.98f);
    colors[ImGuiCol_Border]                = ImVec4(0.22f, 0.24f, 0.28f, 0.70f);
    colors[ImGuiCol_BorderShadow]          = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_FrameBg]               = ImVec4(0.16f, 0.18f, 0.22f, 1.00f);
    colors[ImGuiCol_FrameBgHovered]        = ImVec4(0.22f, 0.25f, 0.30f, 1.00f);
    colors[ImGuiCol_FrameBgActive]         = ImVec4(0.26f, 0.30f, 0.36f, 1.00f);
    colors[ImGuiCol_TitleBg]               = ImVec4(0.08f, 0.09f, 0.11f, 1.00f);
    colors[ImGuiCol_TitleBgActive]         = ImVec4(0.11f, 0.13f, 0.16f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed]      = ImVec4(0.08f, 0.09f, 0.11f, 1.00f);
    colors[ImGuiCol_MenuBarBg]             = ImVec4(0.11f, 0.12f, 0.14f, 1.00f);
    colors[ImGuiCol_ScrollbarBg]           = ImVec4(0.09f, 0.10f, 0.12f, 0.60f);
    colors[ImGuiCol_ScrollbarGrab]         = ImVec4(0.24f, 0.27f, 0.33f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabHovered]  = ImVec4(0.32f, 0.36f, 0.44f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabActive]   = ImVec4(0.40f, 0.45f, 0.54f, 1.00f);
    colors[ImGuiCol_CheckMark]             = ImVec4(0.26f, 0.72f, 1.00f, 1.00f);
    colors[ImGuiCol_SliderGrab]            = ImVec4(0.26f, 0.72f, 1.00f, 1.00f);
    colors[ImGuiCol_SliderGrabActive]      = ImVec4(0.42f, 0.80f, 1.00f, 1.00f);
    colors[ImGuiCol_Button]                = ImVec4(0.20f, 0.23f, 0.28f, 1.00f);
    colors[ImGuiCol_ButtonHovered]         = ImVec4(0.28f, 0.32f, 0.40f, 1.00f);
    colors[ImGuiCol_ButtonActive]          = ImVec4(0.35f, 0.40f, 0.50f, 1.00f);
    colors[ImGuiCol_Header]                = ImVec4(0.20f, 0.24f, 0.30f, 1.00f);
    colors[ImGuiCol_HeaderHovered]         = ImVec4(0.26f, 0.32f, 0.40f, 1.00f);
    colors[ImGuiCol_HeaderActive]          = ImVec4(0.32f, 0.38f, 0.48f, 1.00f);
    colors[ImGuiCol_Separator]             = ImVec4(0.22f, 0.24f, 0.28f, 0.80f);
    colors[ImGuiCol_SeparatorHovered]      = ImVec4(0.30f, 0.34f, 0.40f, 1.00f);
    colors[ImGuiCol_SeparatorActive]       = ImVec4(0.40f, 0.46f, 0.56f, 1.00f);
    colors[ImGuiCol_ResizeGrip]            = ImVec4(0.24f, 0.27f, 0.33f, 0.50f);
    colors[ImGuiCol_ResizeGripHovered]     = ImVec4(0.36f, 0.41f, 0.50f, 0.80f);
    colors[ImGuiCol_ResizeGripActive]      = ImVec4(0.46f, 0.52f, 0.62f, 1.00f);
    colors[ImGuiCol_Tab]                   = ImVec4(0.14f, 0.16f, 0.20f, 1.00f);
    colors[ImGuiCol_TabHovered]            = ImVec4(0.26f, 0.30f, 0.38f, 1.00f);
    colors[ImGuiCol_TabActive]             = ImVec4(0.22f, 0.26f, 0.34f, 1.00f);
    colors[ImGuiCol_TabUnfocused]          = ImVec4(0.12f, 0.14f, 0.17f, 1.00f);
    colors[ImGuiCol_TabUnfocusedActive]    = ImVec4(0.16f, 0.19f, 0.24f, 1.00f);
}

void SoundMixerUI::renderVUMeter(const char* label, float peakL, float peakR, float rmsL, float rmsR, float width, float height) {
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImVec2 end = ImVec2(pos.x + width, pos.y + height);

    // Background track
    drawList->AddRectFilled(pos, end, IM_COL32(20, 22, 26, 255), 3.0f);
    drawList->AddRect(pos, end, IM_COL32(45, 48, 56, 255), 3.0f);

    float chHeight = (height - 3.0f) * 0.5f;

    auto drawChannel = [&](float yOffset, float val, float peakVal) {
        float norm = (std::min)(1.0f, (std::max)(0.0f, val));
        float barW = (width - 4.0f) * norm;

        ImVec2 chP0 = ImVec2(pos.x + 2.0f, pos.y + 1.0f + yOffset);
        ImVec2 chP1 = ImVec2(chP0.x + barW, chP0.y + chHeight);

        if (barW > 0.5f) {
            ImU32 color;
            if (norm < 0.70f) {
                color = IM_COL32(34, 197, 94, 230); // Green (-inf to -12 dB)
            } else if (norm < 0.90f) {
                color = IM_COL32(234, 179, 8, 230); // Yellow (-12 to -3 dB)
            } else {
                color = IM_COL32(239, 68, 68, 240); // Red (> -3 dB)
            }
            drawList->AddRectFilled(chP0, chP1, color, 1.5f);
        }

        // Peak mark tick
        float peakNorm = (std::min)(1.0f, (std::max)(0.0f, peakVal));
        if (peakNorm > 0.01f) {
            float peakX = pos.x + 2.0f + (width - 4.0f) * peakNorm;
            drawList->AddLine(
                ImVec2(peakX, chP0.y),
                ImVec2(peakX, chP0.y + chHeight),
                IM_COL32(255, 255, 255, 200),
                1.5f
            );
        }
    };

    drawChannel(0.0f, rmsL * 1.4f, peakL);
    drawChannel(chHeight + 1.0f, rmsR * 1.4f, peakR);

    ImGui::Dummy(ImVec2(width, height));
}

bool SoundMixerUI::renderVolumeFader(const char* str_id, float* pVolume, float width, float height) {
    if (!pVolume) return false;

    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    // Map volume [0.0, 1.5] to normalized slider position t [0.0, 1.0]
    // 0.0 to 1.0 (unity 0 dB) maps to 0.0 to 0.70 with cubic perceptual curve
    // 1.0 to 1.5 (boost +3.5 dB) maps to 0.70 to 1.00 linearly
    auto volToT = [](float v) -> float {
        if (v <= 0.0001f) return 0.0f;
        if (v <= 1.0f) {
            float u = std::cbrt((std::max)(0.0f, v));
            return u * 0.70f;
        }
        float w = (v - 1.0f) / 0.50f;
        w = (std::min)(1.0f, (std::max)(0.0f, w));
        return 0.70f + w * 0.30f;
    };

    auto tToVol = [](float t) -> float {
        if (t <= 0.005f) return 0.0f;
        if (t <= 0.70f) {
            float u = t / 0.70f;
            return u * u * u;
        }
        float w = (t - 0.70f) / 0.30f;
        return 1.0f + w * 0.50f;
    };

    float currentT = volToT(*pVolume);
    bool changed = false;

    // Use InvisibleButton to handle all mouse interactions
    ImGui::InvisibleButton(str_id, ImVec2(width, height));
    bool isHovered = ImGui::IsItemHovered();
    bool isActive = ImGui::IsItemActive();

    // Dragging
    if (isActive) {
        ImVec2 mousePos = ImGui::GetIO().MousePos;
        float pad = 7.0f;
        float t = (mousePos.x - (pos.x + pad)) / (width - pad * 2.0f);
        t = (std::max)(0.0f, (std::min)(1.0f, t));
        float newVol = tToVol(t);
        // Magnetic snap to exact 1.0 (0.0 dB / 100%) when close
        if (std::abs(newVol - 1.0f) < 0.025f) {
            newVol = 1.0f;
        }
        if (newVol != *pVolume) {
            *pVolume = newVol;
            changed = true;
            currentT = volToT(newVol);
        }
    }

    // Double-click resets to 0.0 dB (100% unity gain)
    if (isHovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
        *pVolume = 1.0f;
        changed = true;
        currentT = volToT(1.0f);
    }

    // Mouse wheel for fine 0.5 dB steps
    if (isHovered && ImGui::GetIO().MouseWheel != 0.0f) {
        float step = ImGui::GetIO().MouseWheel * 0.015f;
        float t = currentT + step;
        t = (std::max)(0.0f, (std::min)(1.0f, t));
        *pVolume = tToVol(t);
        changed = true;
        currentT = t;
    }

    // Right-click context menu for audio presets
    std::string popupId = std::string(str_id) + "_presets";
    if (isHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
        ImGui::OpenPopup(popupId.c_str());
    }

    if (ImGui::BeginPopup(popupId.c_str())) {
        ImGui::TextColored(ImVec4(0.26f, 0.72f, 1.00f, 1.00f), "Volume Presets");
        ImGui::Separator();
        if (ImGui::MenuItem("  0.0 dB (100% - Unity)")) { *pVolume = 1.0000f; changed = true; }
        if (ImGui::MenuItem(" +3.5 dB (150% - Max)"))   { *pVolume = 1.5000f; changed = true; }
        if (ImGui::MenuItem(" +2.0 dB (126%)"))         { *pVolume = 1.2589f; changed = true; }
        if (ImGui::MenuItem(" -3.0 dB (71%)"))          { *pVolume = 0.7079f; changed = true; }
        if (ImGui::MenuItem(" -6.0 dB (50%)"))          { *pVolume = 0.5012f; changed = true; }
        if (ImGui::MenuItem("-12.0 dB (25%)"))          { *pVolume = 0.2512f; changed = true; }
        if (ImGui::MenuItem("-18.0 dB (12.5%)"))        { *pVolume = 0.1259f; changed = true; }
        if (ImGui::MenuItem("  Mute (-inf dB)"))        { *pVolume = 0.0000f; changed = true; }
        ImGui::EndPopup();
    }

    // --- DRAWING ---
    ImVec2 end = ImVec2(pos.x + width, pos.y + height);
    float pad = 7.0f;
    float trackW = width - pad * 2.0f;

    // 1. Dark channel track
    drawList->AddRectFilled(pos, end, IM_COL32(18, 20, 24, 255), 4.0f);
    drawList->AddRect(pos, end, isHovered ? IM_COL32(60, 68, 80, 255) : IM_COL32(38, 42, 50, 255), 4.0f);

    // 2. 0 dB Unity Notch Mark (at 70% width)
    float notchX = pos.x + pad + trackW * 0.70f;
    drawList->AddLine(
        ImVec2(notchX, pos.y + 2.0f),
        ImVec2(notchX, end.y - 2.0f),
        IM_COL32(85, 95, 115, 220),
        1.5f
    );

    // 3. Active Fill Bar
    float handleX = pos.x + pad + trackW * currentT;
    if (currentT > 0.005f) {
        if (currentT <= 0.70f) {
            drawList->AddRectFilled(
                ImVec2(pos.x + 2.0f, pos.y + 2.0f),
                ImVec2(handleX, end.y - 2.0f),
                IM_COL32(38, 140, 245, 140),
                2.5f
            );
        } else {
            drawList->AddRectFilled(
                ImVec2(pos.x + 2.0f, pos.y + 2.0f),
                ImVec2(notchX, end.y - 2.0f),
                IM_COL32(38, 140, 245, 140),
                2.5f
            );
            drawList->AddRectFilled(
                ImVec2(notchX, pos.y + 2.0f),
                ImVec2(handleX, end.y - 2.0f),
                IM_COL32(245, 158, 11, 180),
                2.5f
            );
        }
    }

    // 4. Fader Cap / Handle
    float capW = 12.0f;
    float capH = height + 2.0f;
    ImVec2 capP0(handleX - capW * 0.5f, pos.y - 1.0f);
    ImVec2 capP1(handleX + capW * 0.5f, pos.y - 1.0f + capH);

    ImU32 capBg = (isActive || isHovered) ? IM_COL32(245, 248, 255, 255) : IM_COL32(215, 222, 235, 255);
    drawList->AddRectFilled(capP0, capP1, capBg, 3.0f);
    drawList->AddRect(capP0, capP1, (isActive ? IM_COL32(56, 189, 248, 255) : IM_COL32(30, 34, 42, 255)), 3.0f, 0, (isActive ? 1.5f : 1.0f));

    // Cap center tactile groove
    drawList->AddLine(
        ImVec2(handleX, capP0.y + 3.0f),
        ImVec2(handleX, capP1.y - 3.0f),
        IM_COL32(60, 68, 82, 255),
        1.5f
    );

    // 5. Text Readout (dB and %)
    char labelBuf[48];
    float vol = *pVolume;
    if (vol <= 0.0001f) {
        snprintf(labelBuf, sizeof(labelBuf), "-inf dB (0%%)");
    } else if (std::abs(vol - 1.0f) < 0.005f) {
        snprintf(labelBuf, sizeof(labelBuf), "0.0 dB (100%%)");
    } else {
        float db = 20.0f * std::log10(vol);
        snprintf(labelBuf, sizeof(labelBuf), "%+.1f dB (%.0f%%)", db, vol * 100.0f);
    }

    ImVec2 textSize = ImGui::CalcTextSize(labelBuf);
    ImVec2 textPos = ImVec2(pos.x + (width - textSize.x) * 0.5f, pos.y + (height - textSize.y) * 0.5f);

    // Text drop shadow for legibility over active fill bar
    drawList->AddText(ImVec2(textPos.x + 1.0f, textPos.y + 1.0f), IM_COL32(0, 0, 0, 230), labelBuf);

    ImU32 textColor = (std::abs(vol - 1.0f) < 0.005f)
        ? IM_COL32(66, 215, 255, 255) // Bright cyan at 0 dB unity
        : (vol > 1.0f ? IM_COL32(255, 215, 120, 255) : IM_COL32(235, 240, 248, 240));

    drawList->AddText(textPos, textColor, labelBuf);

    // Tooltip with interaction instructions
    if (isHovered && !isActive) {
        ImGui::SetTooltip("Volume: %s\n* Drag to adjust\n* Scroll wheel to fine-tune (0.5 dB)\n* Double-click to reset (0 dB)\n* Right-click for presets", labelBuf);
    }

    return changed;
}

void SoundMixerUI::renderHeader() {
    ImGui::BeginGroup();
    ImGui::TextColored(ImVec4(0.26f, 0.72f, 1.00f, 1.00f), "SOUNDMIXER");
    ImGui::SameLine();
    ImGui::TextDisabled("v2.1");
    ImGui::EndGroup();

    // Tab navigation buttons
    ImGui::SameLine(135.0f);
    if (m_activeTab == 0) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.45f, 0.72f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
    } else {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.13f, 0.15f, 0.20f, 0.8f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.65f, 0.75f, 1.0f));
    }
    if (ImGui::Button("Mixer", ImVec2(75, 24))) {
        m_activeTab = 0;
    }
    ImGui::PopStyleColor(2);

    ImGui::SameLine();
    if (m_activeTab == 1) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.45f, 0.72f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
    } else {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.13f, 0.15f, 0.20f, 0.8f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.65f, 0.75f, 1.0f));
    }
    if (ImGui::Button("Voice FX", ImVec2(90, 24))) {
        m_activeTab = 1;
    }
    ImGui::PopStyleColor(2);

    ImGui::SameLine();
    if (m_activeTab == 2) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.45f, 0.72f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
    } else {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.13f, 0.15f, 0.20f, 0.8f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.65f, 0.75f, 1.0f));
    }
    auto sbSounds = m_engine.getSoundboard().getSounds();
    char tabLabel[64];
    snprintf(tabLabel, sizeof(tabLabel), "Soundboard (%zu)", sbSounds.size());
    if (ImGui::Button(tabLabel, ImVec2(130, 24))) {
        m_activeTab = 2;
    }
    ImGui::PopStyleColor(2);

    ImGui::SameLine(ImGui::GetWindowWidth() - 330.0f);

    bool sidetoneOn = m_engine.getMixer().isSidetoneEnabled();
    if (sidetoneOn) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.12f, 0.50f, 0.40f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.16f, 0.65f, 0.52f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
    } else {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.14f, 0.16f, 0.20f, 0.7f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.20f, 0.24f, 0.30f, 0.9f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.65f, 0.75f, 1.0f));
    }
    if (ImGui::Button(sidetoneOn ? "[ Hear Voice: ON ]" : "[ Hear Voice: OFF ]", ImVec2(150, 24))) {
        m_engine.getMixer().setSidetoneEnabled(!sidetoneOn);
    }
    ImGui::PopStyleColor(3);
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Instant Zero-Latency Sidetone (Ctrl+Shift+H)\nWhen ON, your modulated voice is monitored in your headphones.\nDiscord / Games always hear your voice regardless of this toggle.");
    }

    ImGui::SameLine();
    if (ImGui::Button("Save Config", ImVec2(90, 24))) {
        auto cfg = ConfigManager::extractFromEngine(m_engine);
        cfg.customPresets = m_customPresets;
        if (ConfigManager::saveConfig(ConfigManager::getDefaultConfigPath(), cfg)) {
            m_statusMessage = "Configuration saved successfully!";
            m_statusTimer = 3.0f;
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Tray", ImVec2(50, 24))) {
        m_minimizeToTrayRequested = true;
    }

    if (!m_statusMessage.empty() && m_statusTimer > 0.0f) {
        ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.4f, 1.0f), "%s", m_statusMessage.c_str());
        m_statusTimer -= ImGui::GetIO().DeltaTime;
    } else {
        ImGui::Separator();
    }
}

void SoundMixerUI::renderSourceList() {
    ImGui::BeginChild("SourcesPanel", ImVec2(0, 240), true);

    ImGui::TextColored(ImVec4(0.9f, 0.9f, 0.9f, 1.0f), "AUDIO INPUTS");
    ImGui::SameLine(ImGui::GetWindowWidth() - 150.0f);
    if (ImGui::Button("+ Add Source", ImVec2(130, 24))) {
        m_showAddSourceModal = true;
        m_appsNeedRefresh = true;
    }

    ImGui::Spacing();

    auto sources = m_engine.getMixer().getSources();
    if (sources.empty()) {
        ImGui::Spacing();
        ImGui::TextDisabled("No active sources. Click '+ Add Source' to capture your Microphone, Spotify, or Chrome!");
    } else {
        for (size_t i = 0; i < sources.size(); ++i) {
            auto& src = sources[i];
            ImGui::PushID(static_cast<int>(i));

            ImGui::BeginChild(src->getId().c_str(), ImVec2(0, 64), true, ImGuiWindowFlags_NoScrollbar);

            // Left badge: [APP] in cyan or [MIC] in purple
            if (src->getType() == SourceType::Application) {
                ImGui::TextColored(ImVec4(0.2f, 0.8f, 1.0f, 1.0f), "[APP]");
            } else {
                ImGui::TextColored(ImVec4(0.8f, 0.5f, 1.0f, 1.0f), "[MIC]");
            }

            ImGui::SameLine();
            ImGui::TextUnformatted(src->getName().c_str());

            // VU meter
            ImGui::SameLine(290.0f);
            float pL, pR, rL, rR;
            src->getLevels(pL, pR, rL, rR);
            renderVUMeter("##meter", pL, pR, rL, rR, 110.0f, 20.0f);

            // Volume fader
            ImGui::SameLine(410.0f);
            float vol = src->getVolume();
            if (renderVolumeFader("##vol", &vol, 145.0f, 22.0f)) {
                src->setVolume(vol);
            }

            // Quick 0 dB snap button
            ImGui::SameLine(562.0f);
            bool atUnity = std::abs(vol - 1.0f) < 0.005f;
            if (!atUnity) {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.35f, 0.52f, 0.9f));
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.45f, 0.88f, 1.0f, 1.0f));
            } else {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.14f, 0.16f, 0.20f, 0.5f));
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.55f, 0.65f, 0.8f));
            }
            if (ImGui::Button("0dB", ImVec2(34, 22))) {
                src->setVolume(1.0f);
            }
            ImGui::PopStyleColor(2);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Snap volume to 0.0 dB (100%%)");

            // Mute toggle
            ImGui::SameLine(604.0f);
            bool muted = src->isMuted();
            if (muted) {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
            }
            if (ImGui::Button(muted ? "MUTED" : "LIVE", ImVec2(54, 22))) {
                src->setMuted(!muted);
            }
            if (muted) {
                ImGui::PopStyleColor();
            }

            // Remove button
            ImGui::SameLine(666.0f);
            if (ImGui::Button("X", ImVec2(26, 22))) {
                m_engine.getMixer().removeSource(src->getId());
            }

            // Sub-info: glitch count & status
            ImGui::TextDisabled("Status: %s | Glitches: %u", src->getStatusText().c_str(), src->getGlitchCount());

            ImGui::EndChild();
            ImGui::PopID();
            ImGui::Spacing();
        }
    }

    ImGui::EndChild();
}

void SoundMixerUI::renderOutputsList() {
    ImGui::BeginChild("OutputsPanel", ImVec2(0, -90), true);

    ImGui::TextColored(ImVec4(0.26f, 0.72f, 1.00f, 1.00f), "AUDIO OUTPUTS (TARGET DEVICES)");
    ImGui::SameLine(ImGui::GetWindowWidth() - 150.0f);
    if (ImGui::Button("+ Add Output", ImVec2(130, 24))) {
        m_showAddOutputModal = true;
    }

    ImGui::Spacing();

    // Virtual Audio Bridge Banner - only shown if driver is missing and needs to be installed
    auto bridgeInfo = m_bridge.refreshStatus();
    if (bridgeInfo.status == VirtualBridgeStatus::NotInstalled) {
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.14f, 0.10f, 0.10f, 0.90f));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 4.0f);
        ImGui::BeginChild("VirtualBridgeBanner", ImVec2(0, 36), true, ImGuiWindowFlags_NoScrollbar);

        ImGui::AlignTextToFramePadding();
        ImGui::TextColored(ImVec4(0.95f, 0.35f, 0.35f, 1.0f), "[ ! ] NO VIRTUAL MIC DETECTED");
        ImGui::SameLine();
        ImGui::TextDisabled("| Virtual driver needed to route audio to Discord/games");

        ImGui::SameLine(ImGui::GetWindowWidth() - 235.0f);
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.70f, 0.25f, 0.15f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.85f, 0.32f, 0.20f, 1.0f));
        if (ImGui::Button("[ Install Virtual Cable Driver ]", ImVec2(220, 22))) {
            std::string errMsg;
            if (m_bridge.installDriver(errMsg)) {
                m_statusMessage = "Virtual Audio Bridge installed successfully!";
                m_statusTimer = 4.0f;
            } else {
                m_statusMessage = "Install: " + errMsg;
                m_statusTimer = 5.0f;
            }
        }
        ImGui::PopStyleColor(2);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Silently downloads and installs the WHQL-certified Windows Virtual Audio Cable driver.\nZero external websites needed!");
        }

        ImGui::EndChild();
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();
        ImGui::Spacing();
    }

    auto outputs = m_engine.getOutputs();
    if (outputs.empty()) {
        ImGui::Spacing();
        ImGui::TextDisabled("No active outputs. Click '+ Add Output' to select a playback device!");
    } else {
        for (size_t i = 0; i < outputs.size(); ++i) {
            auto& out = outputs[i];
            ImGui::PushID(static_cast<int>(1000 + i));

            ImGui::BeginChild(out->getId().c_str(), ImVec2(0, 68), true, ImGuiWindowFlags_NoScrollbar);

            // Badge: [FX MIC], [PRIMARY PC], or [OUTPUT PC]
            if (out->getRouteMode() == OutputRouteMode::VirtualMicStream) {
                ImGui::TextColored(ImVec4(0.2f, 0.95f, 0.55f, 1.00f), "[FX MIC]");
            } else if (out->isPrimary()) {
                ImGui::TextColored(ImVec4(0.26f, 0.72f, 1.00f, 1.00f), "[PRIMARY PC]");
            } else {
                ImGui::TextColored(ImVec4(0.6f, 0.7f, 0.8f, 1.00f), "[OUTPUT PC]");
            }

            ImGui::SameLine();
            ImGui::TextUnformatted(out->getName().c_str());

            // VU meter for this output
            ImGui::SameLine(290.0f);
            float pL, pR, rL, rR;
            out->getLevels(pL, pR, rL, rR);
            renderVUMeter("##outmeter", pL, pR, rL, rR, 110.0f, 20.0f);

            // Volume fader for this output
            ImGui::SameLine(410.0f);
            float vol = out->getVolume();
            if (renderVolumeFader("##outvol", &vol, 145.0f, 22.0f)) {
                out->setVolume(vol);
            }

            // Quick 0 dB snap button
            ImGui::SameLine(562.0f);
            bool atUnity = std::abs(vol - 1.0f) < 0.005f;
            if (!atUnity) {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.35f, 0.52f, 0.9f));
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.45f, 0.88f, 1.0f, 1.0f));
            } else {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.14f, 0.16f, 0.20f, 0.5f));
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.55f, 0.65f, 0.8f));
            }
            if (ImGui::Button("0dB", ImVec2(34, 22))) {
                out->setVolume(1.0f);
            }
            ImGui::PopStyleColor(2);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Snap volume to 0.0 dB (100%%)");

            // Mute toggle for this output
            ImGui::SameLine(604.0f);
            bool muted = out->isMuted();
            if (muted) {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
            }
            if (ImGui::Button(muted ? "MUTED" : "LIVE", ImVec2(54, 22))) {
                out->setMuted(!muted);
            }
            if (muted) {
                ImGui::PopStyleColor();
            }

            // Remove button (allowed if > 1 output)
            ImGui::SameLine(666.0f);
            if (outputs.size() > 1) {
                if (ImGui::Button("X", ImVec2(26, 22))) {
                    m_engine.removeOutput(out->getId());
                }
            } else {
                ImGui::TextDisabled("Lock");
            }

            // Sub-info: latency, period, and primary button
            ImGui::TextDisabled("Latency: %.2f ms (%u frames) | Glitches: %u",
                out->getLatencyMs(), out->getBufferPeriodFrames(), out->getGlitchCount());

            bool isVirtualMode = (out->getRouteMode() == OutputRouteMode::VirtualMicStream);
            bool isFullMixMode = (out->getRouteMode() == OutputRouteMode::FullMix);

            ImGui::SameLine(295.0f);

            // BUTTON 1: [ FX ] - Mic with FX (Voice + SFX for Discord/Games)
            if (isVirtualMode) {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.12f, 0.48f, 0.30f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.16f, 0.58f, 0.36f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.09f, 0.38f, 0.24f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.35f, 1.0f, 0.65f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.25f, 0.90f, 0.55f, 0.9f));
            } else {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.13f, 0.15f, 0.19f, 0.6f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.18f, 0.22f, 0.28f, 0.8f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.10f, 0.12f, 0.16f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.50f, 0.55f, 0.65f, 0.7f));
                ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.20f, 0.24f, 0.30f, 0.4f));
            }
            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, isVirtualMode ? 1.5f : 1.0f);
            if (ImGui::Button("[ FX ]##routeMic", ImVec2(68, 22))) {
                out->setRouteMode(OutputRouteMode::VirtualMicStream);
            }
            ImGui::PopStyleVar();
            ImGui::PopStyleColor(5);
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Voice + SFX Stream (Discord / Games / Voicemod)\n"
                                  "Routes ONLY your Mic (with Voice FX) + Soundboard to this device.\n"
                                  "Desktop PC & game audio is blocked to eliminate echo back into Discord/chat.");
            }

            ImGui::SameLine();

            // BUTTON 2: [ PC ] - PC Desktop Mix (Full mix for Headphones/Speakers)
            if (isFullMixMode) {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.42f, 0.68f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.24f, 0.52f, 0.82f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.14f, 0.34f, 0.56f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.40f, 0.90f, 1.0f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.35f, 0.80f, 1.0f, 0.9f));
            } else {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.13f, 0.15f, 0.19f, 0.6f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.18f, 0.22f, 0.28f, 0.8f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.10f, 0.12f, 0.16f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.50f, 0.55f, 0.65f, 0.7f));
                ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.20f, 0.24f, 0.30f, 0.4f));
            }
            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, isFullMixMode ? 1.5f : 1.0f);
            if (ImGui::Button("[ PC ]##routePc", ImVec2(68, 22))) {
                out->setRouteMode(OutputRouteMode::FullMix);
            }
            ImGui::PopStyleVar();
            ImGui::PopStyleColor(5);
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Full PC Desktop Mix (Headphones / Speakers)\n"
                                  "Routes all PC desktop apps, music, soundboard, and sidetone mic to this device.");
            }

            if (!out->isPrimary()) {
                ImGui::SameLine(465.0f);
                if (ImGui::SmallButton("Set as Primary Clock")) {
                    m_engine.setPrimaryOutput(out->getId());
                }
            }

            ImGui::EndChild();
            ImGui::PopID();
            ImGui::Spacing();
        }
    }

    ImGui::EndChild();
}

void SoundMixerUI::renderMasterSection() {
    ImGui::BeginChild("MasterBottomPanel", ImVec2(0, 75), true);

    ImGui::Columns(2, "MasterBottomCols", false);
    ImGui::SetColumnWidth(0, 340.0f);

    ImGui::TextColored(ImVec4(0.9f, 0.9f, 0.9f, 1.0f), "MASTER BUS");
    ImGui::TextDisabled("Global mix bus before output faders | Soft Limiter: %s",
        m_engine.getMixer().isLimiterActive() ? "ENGAGED" : "Ready");

    ImGui::NextColumn();

    ImGui::Text("Master Bus Level");
    float masterVol = m_engine.getMixer().getMasterVolume();
    if (renderVolumeFader("##mvol", &masterVol, 150.0f, 24.0f)) {
        m_engine.getMixer().setMasterVolume(masterVol);
    }

    ImGui::SameLine();
    bool atUnity = std::abs(masterVol - 1.0f) < 0.005f;
    if (!atUnity) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.35f, 0.52f, 0.9f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.45f, 0.88f, 1.0f, 1.0f));
    } else {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.14f, 0.16f, 0.20f, 0.5f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.55f, 0.65f, 0.8f));
    }
    if (ImGui::Button("0dB##master", ImVec2(34, 24))) {
        m_engine.getMixer().setMasterVolume(1.0f);
    }
    ImGui::PopStyleColor(2);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Snap master volume to 0.0 dB (100%%)");

    ImGui::SameLine();
    bool mMuted = m_engine.getMixer().isMasterMuted();
    if (mMuted) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
    if (ImGui::Button(mMuted ? "MUTE" : "ON", ImVec2(48, 24))) {
        m_engine.getMixer().setMasterMuted(!mMuted);
    }
    if (mMuted) ImGui::PopStyleColor();

    ImGui::SameLine();
    float mPL, mPR, mRL, mRR;
    m_engine.getMixer().getMasterLevels(mPL, mPR, mRL, mRR);
    renderVUMeter("##mmeter", mPL, mPR, mRL, mRR, 100.0f, 24.0f);

    if (m_engine.getMixer().isLimiterActive()) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.0f, 1.0f), "[LIMIT]");
    }

    ImGui::Columns(1);
    ImGui::EndChild();
}

void SoundMixerUI::renderAddOutputModal() {
    if (!m_showAddOutputModal) return;

    ImGui::OpenPopup("Add Output Device");
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(600, 390));

    if (ImGui::BeginPopupModal("Add Output Device", &m_showAddOutputModal, ImGuiWindowFlags_NoResize)) {
        auto renderDevs = m_engine.getDeviceManager().getRenderDevices();
        auto currentOutputs = m_engine.getOutputs();

        if (ImGui::BeginTabBar("AddOutputTabBar")) {
            if (ImGui::BeginTabItem("Playback Devices (Speakers / Headphones)")) {
                ImGui::Spacing();
                ImGui::BeginChild("OutputDeviceListChild", ImVec2(0, 240), true);

                for (size_t i = 0; i < renderDevs.size(); ++i) {
                    const auto& dev = renderDevs[i];
                    ImGui::PushID(static_cast<int>(i));

                    bool alreadyAdded = false;
                    for (const auto& out : currentOutputs) {
                        if (out->getId() == dev.idUtf8) {
                            alreadyAdded = true;
                            break;
                        }
                    }

                    if (dev.isVBCable) {
                        ImGui::TextColored(ImVec4(0.26f, 0.72f, 1.00f, 1.00f), "[VB-CABLE]");
                        ImGui::SameLine();
                    } else if (dev.isDefault) {
                        ImGui::TextColored(ImVec4(0.4f, 0.8f, 0.4f, 1.00f), "[DEFAULT]");
                        ImGui::SameLine();
                    }

                    ImGui::Text("%s", dev.nameUtf8.c_str());
                    ImGui::SameLine(ImGui::GetWindowWidth() - 90.0f);

                    if (alreadyAdded) {
                        ImGui::TextDisabled("Added");
                    } else {
                        if (ImGui::Button("+ Add", ImVec2(75, 22))) {
                            m_engine.addOutput(dev.idUtf8, dev.nameUtf8);
                            m_showAddOutputModal = false;
                        }
                    }

                    ImGui::Separator();
                    ImGui::PopID();
                }

                ImGui::EndChild();
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Virtual Mics & Inputs (Discord / Games)")) {
                ImGui::Spacing();
                ImGui::TextDisabled("Select an input / recording device to stream your Voice FX and Soundboard to:");
                ImGui::Spacing();

                auto captureDevs = m_engine.getDeviceManager().getCaptureDevices();
                ImGui::BeginChild("VirtualInputDeviceListChild", ImVec2(0, 220), true);

                for (size_t i = 0; i < captureDevs.size(); ++i) {
                    const auto& dev = captureDevs[i];
                    ImGui::PushID(static_cast<int>(2000 + i));

                    bool isVirtual = dev.isVBCable || (dev.nameUtf8.find("CABLE") != std::string::npos) ||
                                     (dev.nameUtf8.find("VB-Audio") != std::string::npos) ||
                                     (dev.nameUtf8.find("Voicemod") != std::string::npos) ||
                                     (dev.nameUtf8.find("Virtual") != std::string::npos);

                    // Find corresponding render device for virtual cables
                    std::string targetRenderId;
                    std::string targetRenderName;
                    for (const auto& rDev : renderDevs) {
                        if (rDev.isVBCable || rDev.nameUtf8.find("VB-Audio") != std::string::npos ||
                            rDev.nameUtf8.find("CABLE") != std::string::npos ||
                            rDev.nameUtf8.find("Voicemod") != std::string::npos) {
                            targetRenderId = rDev.idUtf8;
                            targetRenderName = rDev.nameUtf8;
                            break;
                        }
                    }

                    bool alreadyAdded = false;
                    for (const auto& out : currentOutputs) {
                        if (!targetRenderId.empty() && out->getId() == targetRenderId) {
                            alreadyAdded = true;
                            break;
                        }
                        if (out->getId() == dev.idUtf8 || out->getName() == dev.nameUtf8) {
                            alreadyAdded = true;
                            break;
                        }
                    }

                    if (isVirtual) {
                        ImGui::TextColored(ImVec4(0.2f, 0.95f, 0.55f, 1.0f), "[VIRTUAL MIC]");
                        ImGui::SameLine();
                        ImGui::Text("%s", dev.nameUtf8.c_str());
                        ImGui::SameLine(ImGui::GetWindowWidth() - 90.0f);

                        if (alreadyAdded) {
                            ImGui::TextDisabled("Added");
                        } else {
                            if (ImGui::Button("+ Add", ImVec2(75, 22))) {
                                std::string renderIdToAdd = targetRenderId.empty() ? dev.idUtf8 : targetRenderId;
                                std::string displayName = dev.nameUtf8;
                                m_engine.addOutput(renderIdToAdd, displayName);
                                auto outputs = m_engine.getOutputs();
                                for (auto& out : outputs) {
                                    if (out->getId() == renderIdToAdd) {
                                        out->setRouteMode(OutputRouteMode::VirtualMicStream);
                                    }
                                }
                                m_statusMessage = "Added " + displayName + " as Voice FX output!";
                                m_statusTimer = 3.5f;
                                m_showAddOutputModal = false;
                            }
                        }
                    } else {
                        ImGui::TextColored(ImVec4(0.55f, 0.6f, 0.7f, 1.0f), "[HARDWARE MIC]");
                        ImGui::SameLine();
                        ImGui::Text("%s", dev.nameUtf8.c_str());
                        ImGui::SameLine(ImGui::GetWindowWidth() - 110.0f);
                        ImGui::TextDisabled("Input Only");
                        if (ImGui::IsItemHovered()) {
                            ImGui::SetTooltip("Physical microphones are input devices.\nTo capture your voice, add this under Audio Sources above.");
                        }
                    }

                    ImGui::Separator();
                    ImGui::PopID();
                }

                ImGui::EndChild();
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }

        ImGui::Spacing();
        if (ImGui::Button("Close", ImVec2(80, 26))) {
            m_showAddOutputModal = false;
        }

        ImGui::EndPopup();
    }
}

void SoundMixerUI::renderAddSourceModal() {
    if (!m_showAddSourceModal) return;

    ImGui::OpenPopup("Add Audio Source");
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(650, 480));

    if (ImGui::BeginPopupModal("Add Audio Source", &m_showAddSourceModal, ImGuiWindowFlags_NoResize)) {
        if (ImGui::BeginTabBar("AddSourceTabs")) {
            // Tab 1: Applications (Process Loopback)
            if (ImGui::BeginTabItem("Applications (Spotify, Chrome, etc.)")) {
                if (m_appsNeedRefresh) {
                    m_cachedApps = ProcessEnumerator::getRunningApplications();
                    m_appsNeedRefresh = false;
                }

                ImGui::Spacing();
                ImGui::Text("Filter:");
                ImGui::SameLine();
                char filterBuf[128] = {0};
                std::strncpy(filterBuf, m_appSearchFilter.c_str(), sizeof(filterBuf) - 1);
                ImGui::SetNextItemWidth(350.0f);
                if (ImGui::InputText("##search", filterBuf, sizeof(filterBuf))) {
                    m_appSearchFilter = filterBuf;
                }
                ImGui::SameLine();
                if (ImGui::Button("Refresh")) {
                    m_appsNeedRefresh = true;
                }

                ImGui::Spacing();
                ImGui::BeginChild("AppListChild", ImVec2(0, 310), true);

                std::string filterLower = m_appSearchFilter;
                std::transform(filterLower.begin(), filterLower.end(), filterLower.begin(), [](unsigned char c) {
                    return static_cast<char>(std::tolower(c));
                });

                for (const auto& app : m_cachedApps) {
                    std::string combined = app.exeName + " " + app.windowTitle;
                    std::string combinedLower = combined;
                    std::transform(combinedLower.begin(), combinedLower.end(), combinedLower.begin(), [](unsigned char c) {
                        return static_cast<char>(std::tolower(c));
                    });

                    if (!filterLower.empty() && combinedLower.find(filterLower) == std::string::npos) {
                        continue;
                    }

                    ImGui::PushID(static_cast<int>(app.pid));

                    if (app.hasAudioSession) {
                        ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.4f, 1.0f), "[AUDIO]");
                        ImGui::SameLine();
                    }

                    std::string display = app.exeName;
                    if (!app.windowTitle.empty()) {
                        display += " - " + app.windowTitle;
                    }
                    if (display.length() > 60) {
                        display = display.substr(0, 57) + "...";
                    }

                    ImGui::Text("%s", display.c_str());
                    ImGui::SameLine(ImGui::GetWindowWidth() - 90.0f);

                    if (ImGui::Button("+ Add", ImVec2(75, 22))) {
                        DWORD rootPid = ProcessEnumerator::findRootProcessId(app.pid);
                        auto processSource = std::make_shared<ProcessCaptureSource>(rootPid, app.exeName);
                        if (processSource->start()) {
                            m_engine.getMixer().addSource(processSource);
                            m_showAddSourceModal = false;
                        }
                    }

                    ImGui::Separator();
                    ImGui::PopID();
                }

                ImGui::EndChild();
                ImGui::EndTabItem();
            }

            // Tab 2: Microphones / Input devices
            if (ImGui::BeginTabItem("Microphones / Inputs")) {
                ImGui::Spacing();
                auto captureDevs = m_engine.getDeviceManager().getCaptureDevices();

                ImGui::BeginChild("MicListChild", ImVec2(0, 340), true);
                for (size_t i = 0; i < captureDevs.size(); ++i) {
                    const auto& dev = captureDevs[i];
                    ImGui::PushID(static_cast<int>(i));

                    if (dev.isDefault) {
                        ImGui::TextColored(ImVec4(0.26f, 0.72f, 1.00f, 1.00f), "[DEFAULT]");
                        ImGui::SameLine();
                    }

                    ImGui::Text("%s", dev.nameUtf8.c_str());
                    ImGui::SameLine(ImGui::GetWindowWidth() - 90.0f);

                    if (ImGui::Button("+ Add", ImVec2(75, 22))) {
                        auto micSource = std::make_shared<MicCaptureSource>(dev.idUtf8, dev.nameUtf8);
                        if (micSource->start()) {
                            m_engine.getMixer().addSource(micSource);
                            m_showAddSourceModal = false;
                        }
                    }

                    ImGui::Separator();
                    ImGui::PopID();
                }
                ImGui::EndChild();

                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }

        ImGui::Spacing();
        if (ImGui::Button("Close", ImVec2(80, 26))) {
            m_showAddSourceModal = false;
        }

        ImGui::EndPopup();
    }
}

void SoundMixerUI::openFileAndAddSound() {
    OPENFILENAMEW ofn;
    ZeroMemory(&ofn, sizeof(ofn));
    wchar_t szFile[MAX_PATH] = { 0 };

    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = GetActiveWindow();
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile) / sizeof(wchar_t);
    ofn.lpstrFilter = L"Audio Files (*.wav;*.mp3;*.m4a;*.flac;*.aac;*.wma)\0*.wav;*.mp3;*.m4a;*.flac;*.aac;*.wma\0All Files (*.*)\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

    if (GetOpenFileNameW(&ofn)) {
        int utf8Size = WideCharToMultiByte(CP_UTF8, 0, szFile, -1, nullptr, 0, nullptr, nullptr);
        if (utf8Size > 0) {
            std::string utf8Path(utf8Size - 1, 0);
            WideCharToMultiByte(CP_UTF8, 0, szFile, -1, &utf8Path[0], utf8Size, nullptr, nullptr);
            auto sfx = m_engine.getSoundboard().addSound(utf8Path);
            if (sfx) {
                m_statusMessage = "Loaded: " + sfx->getName();
                m_statusTimer = 3.0f;
            } else {
                m_statusMessage = "Error loading audio file: " + utf8Path;
                m_statusTimer = 4.0f;
            }
        }
    }
}

void SoundMixerUI::renderSoundboardTab() {
    auto& sb = m_engine.getSoundboard();

    // Top control banner
    ImGui::BeginChild("SoundboardHeaderChild", ImVec2(0, 52), true);

    auto sounds = sb.getSounds();
    ImGui::AlignTextToFramePadding();
    ImGui::TextColored(ImVec4(0.26f, 0.72f, 1.00f, 1.0f), "SOUNDBOARD");
    ImGui::SameLine();
    ImGui::TextDisabled("(%zu Sounds)", sounds.size());

    // SFX Master Volume
    ImGui::SameLine(180.0f);
    ImGui::AlignTextToFramePadding();
    ImGui::Text("Master FX:");
    ImGui::SameLine();
    float sfxVol = sb.getMasterVolume();
    if (renderVolumeFader("##sbMasterVol", &sfxVol, 110.0f, 22.0f)) {
        sb.setMasterVolume(sfxVol);
    }
    ImGui::SameLine();
    if (ImGui::Button("0dB##sbMaster", ImVec2(32, 22))) {
        sb.setMasterVolume(1.0f);
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Snap SFX master to 0.0 dB");

    // Right action buttons: Panic button, Add File, Presets
    ImGui::SameLine(ImGui::GetWindowWidth() - 360.0f);
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.75f, 0.15f, 0.15f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.90f, 0.22f, 0.22f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.60f, 0.10f, 0.10f, 1.0f));
    std::string stopAllLabel = "Stop All (" + sb.getStopAllText() + ")";
    if (ImGui::Button(stopAllLabel.c_str(), ImVec2(150, 24))) {
        sb.stopAllSounds();
    }
    ImGui::PopStyleColor(3);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Instant Panic Silencer (Global Hotkey: %s)", sb.getStopAllText().c_str());

    ImGui::SameLine();
    if (ImGui::Button("Key##panicGear", ImVec2(32, 24))) {
        m_showPanicHotkeyModal = true;
        m_recordedModifiers = sb.getStopAllModifiers();
        m_recordedVk = sb.getStopAllVk();
        m_recordedText = sb.getStopAllText();
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Configure Global Panic Hotkey");

    ImGui::SameLine();
    if (ImGui::Button("+ File", ImVec2(70, 24))) {
        openFileAndAddSound();
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Browse and add WAV, MP3, M4A, FLAC, or AAC file");

    ImGui::SameLine();
    if (ImGui::Button("+ Preset", ImVec2(72, 24))) {
        ImGui::OpenPopup("AddPresetSfxPopup");
    }

    if (ImGui::BeginPopup("AddPresetSfxPopup")) {
        ImGui::TextDisabled("Built-in Instant Sound Effects");
        ImGui::Separator();
        if (ImGui::MenuItem("Airhorn Blast (Stadium 3-burst)")) {
            auto sfx = sb.addBuiltIn(BuiltInSoundType::Airhorn);
            if (sfx) {
                m_statusMessage = "Added Airhorn sound effect!";
                m_statusTimer = 3.0f;
            }
        }
        if (ImGui::MenuItem("Rimshot (Ba-Dum-Tss)")) {
            auto sfx = sb.addBuiltIn(BuiltInSoundType::Rimshot);
            if (sfx) {
                m_statusMessage = "Added Rimshot sound effect!";
                m_statusTimer = 3.0f;
            }
        }
        if (ImGui::MenuItem("Victory Fanfare (Level Up)")) {
            auto sfx = sb.addBuiltIn(BuiltInSoundType::Fanfare);
            if (sfx) {
                m_statusMessage = "Added Victory Fanfare!";
                m_statusTimer = 3.0f;
            }
        }
        if (ImGui::MenuItem("Coin Chime (Retro Arcade)")) {
            auto sfx = sb.addBuiltIn(BuiltInSoundType::CoinChime);
            if (sfx) {
                m_statusMessage = "Added Coin Chime!";
                m_statusTimer = 3.0f;
            }
        }
        if (ImGui::MenuItem("Wrong Buzzer (Game Show)")) {
            auto sfx = sb.addBuiltIn(BuiltInSoundType::Buzzer);
            if (sfx) {
                m_statusMessage = "Added Wrong Buzzer!";
                m_statusTimer = 3.0f;
            }
        }
        ImGui::EndPopup();
    }

    ImGui::EndChild();

    // Main sound effects list
    ImGui::BeginChild("SoundboardItemsList", ImVec2(0, 0), true);

    if (sounds.empty()) {
        ImGui::Spacing();
        ImGui::Spacing();
        ImGui::TextDisabled("No sound effects loaded yet.");
        ImGui::TextDisabled("Click '+ File' to load your audio files, or '+ Preset' for built-in sound effects!");
        ImGui::Spacing();
        if (ImGui::Button("Load Sample Sound Effects Bundle", ImVec2(270, 32))) {
            auto s1 = sb.addBuiltIn(BuiltInSoundType::Airhorn);
            if (s1) s1->setHotkey(MOD_CONTROL | MOD_ALT, VK_F8, "Ctrl+Alt+F8");
            auto s2 = sb.addBuiltIn(BuiltInSoundType::Rimshot);
            if (s2) s2->setHotkey(MOD_CONTROL | MOD_ALT, VK_F9, "Ctrl+Alt+F9");
            auto s3 = sb.addBuiltIn(BuiltInSoundType::Fanfare);
            if (s3) s3->setHotkey(MOD_CONTROL | MOD_ALT, VK_F10, "Ctrl+Alt+F10");
            auto s4 = sb.addBuiltIn(BuiltInSoundType::CoinChime);
            if (s4) s4->setHotkey(MOD_CONTROL | MOD_ALT, VK_F11, "Ctrl+Alt+F11");
            sb.reregisterHotkeys();
        }
    } else {
        for (size_t i = 0; i < sounds.size(); ++i) {
            auto& sfx = sounds[i];
            ImGui::PushID(static_cast<int>(i));

            bool isPlaying = sfx->isPlaying();

            if (isPlaying) {
                ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.12f, 0.22f, 0.26f, 0.9f));
                ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.15f, 0.85f, 0.85f, 0.9f));
            } else {
                ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.11f, 0.12f, 0.15f, 0.6f));
                ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.20f, 0.22f, 0.28f, 0.5f));
            }

            ImGui::BeginChild(sfx->getId().c_str(), ImVec2(0, 68), true, ImGuiWindowFlags_NoScrollbar);

            // 1. Play / Stop Button
            if (isPlaying) {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.85f, 0.22f, 0.22f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.95f, 0.32f, 0.32f, 1.0f));
                if (ImGui::Button("STOP", ImVec2(62, 52))) {
                    sfx->stop();
                }
                ImGui::PopStyleColor(2);
            } else {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.55f, 0.45f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.20f, 0.70f, 0.58f, 1.0f));
                if (ImGui::Button("PLAY", ImVec2(62, 52))) {
                    sfx->play();
                }
                ImGui::PopStyleColor(2);
            }

            // 2. Title & Duration
            ImGui::SameLine(74.0f);
            ImGui::BeginGroup();
            ImGui::TextColored(isPlaying ? ImVec4(0.2f, 1.0f, 0.9f, 1.0f) : ImVec4(0.92f, 0.92f, 0.92f, 1.0f),
                               "%s", sfx->getName().c_str());

            char timeBuf[64];
            if (isPlaying) {
                snprintf(timeBuf, sizeof(timeBuf), "Playing: %.1fs / %.1fs", sfx->getCurrentTimeSeconds(), sfx->getDurationSeconds());
                ImGui::TextColored(ImVec4(0.3f, 0.9f, 0.6f, 1.0f), "%s", timeBuf);
            } else {
                snprintf(timeBuf, sizeof(timeBuf), "Duration: %.1fs", sfx->getDurationSeconds());
                ImGui::TextDisabled("%s", timeBuf);
            }

            // Mini progress bar
            float prog = sfx->getPlaybackProgress();
            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, isPlaying ? ImVec4(0.10f, 0.85f, 0.85f, 1.0f) : ImVec4(0.25f, 0.28f, 0.35f, 0.5f));
            ImGui::ProgressBar(prog, ImVec2(165.0f, 3.0f), "");
            ImGui::PopStyleColor();
            ImGui::EndGroup();

            // 3. Macro Hotkey Trigger Button
            ImGui::SameLine(255.0f);
            std::string hotkeyStr = sfx->hasHotkey() ? ("[KEY] " + sfx->getHotkeyText()) : "[+] Bind Key...";
            if (sfx->hasHotkey()) {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.24f, 0.20f, 0.38f, 0.9f));
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.95f, 0.75f, 0.25f, 1.0f));
            } else {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.16f, 0.18f, 0.22f, 0.8f));
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.65f, 0.75f, 1.0f));
            }
            if (ImGui::Button(hotkeyStr.c_str(), ImVec2(135, 24))) {
                m_showHotkeyModal = true;
                m_hotkeyTargetSfxId = sfx->getId();
                m_hotkeyTargetName = sfx->getName();
                m_recordedModifiers = sfx->getHotkeyModifiers();
                m_recordedVk = sfx->getHotkeyVk();
                m_recordedText = sfx->getHotkeyText();
            }
            ImGui::PopStyleColor(2);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Click to set global macro hotkey");

            // 4. Volume Fader
            ImGui::SameLine(402.0f);
            float vol = sfx->getVolume();
            if (renderVolumeFader("##sfxVol", &vol, 125.0f, 22.0f)) {
                sfx->setVolume(vol);
            }

            // 0dB snap
            ImGui::SameLine(534.0f);
            if (ImGui::Button("0dB", ImVec2(32, 22))) {
                sfx->setVolume(1.0f);
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Snap to 0.0 dB (100%%)");

            // 5. Loop Toggle
            ImGui::SameLine(572.0f);
            bool isLoop = sfx->isLooping();
            if (isLoop) {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.52f, 0.72f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
            } else {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.14f, 0.16f, 0.20f, 0.6f));
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.55f, 0.65f, 0.8f));
            }
            if (ImGui::Button("Loop", ImVec2(44, 22))) {
                sfx->setLooping(!isLoop);
            }
            ImGui::PopStyleColor(2);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Toggle continuous loop");

            // 6. Delete Button
            ImGui::SameLine(622.0f);
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.14f, 0.14f, 0.6f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.80f, 0.20f, 0.20f, 0.9f));
            if (ImGui::Button("X##delSfx", ImVec2(24, 22))) {
                sb.removeSound(sfx->getId());
            }
            ImGui::PopStyleColor(2);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Remove this sound effect");

            ImGui::EndChild();
            ImGui::PopStyleColor(2);
            ImGui::PopID();
            ImGui::Spacing();
        }
    }

    ImGui::EndChild();
}

void SoundMixerUI::renderHotkeyModal() {
    if (!m_showHotkeyModal) return;

    ImGui::OpenPopup("Bind Macro Hotkey");
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(480, 320));

    if (ImGui::BeginPopupModal("Bind Macro Hotkey", &m_showHotkeyModal, ImGuiWindowFlags_NoResize)) {
        ImGui::Text("Assign Macro Hotkey for: \"%s\"", m_hotkeyTargetName.c_str());
        ImGui::Spacing();
        ImGui::TextDisabled("Press any key or key combination on your keyboard (e.g. F8, Ctrl+Alt+1, Num 5)...");
        ImGui::Spacing();

        // Keyboard detection using GetAsyncKeyState
        for (int vk = 8; vk <= 255; ++vk) {
            if (vk == VK_SHIFT || vk == VK_CONTROL || vk == VK_MENU || vk == VK_LWIN || vk == VK_RWIN) {
                continue;
            }
            if ((GetAsyncKeyState(vk) & 0x8000) != 0) {
                uint32_t mods = 0;
                if ((GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0) mods |= MOD_CONTROL;
                if ((GetAsyncKeyState(VK_MENU) & 0x8000) != 0)    mods |= MOD_ALT;
                if ((GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0)   mods |= MOD_SHIFT;
                if (((GetAsyncKeyState(VK_LWIN) | GetAsyncKeyState(VK_RWIN)) & 0x8000) != 0) mods |= MOD_WIN;

                m_recordedModifiers = mods;
                m_recordedVk = vk;
                m_recordedText = SoundboardManager::formatHotkeyText(mods, vk);
                break;
            }
        }

        // Display current detected key
        ImGui::BeginChild("DetectedKeyBox", ImVec2(0, 54), true);
        ImGui::AlignTextToFramePadding();
        ImGui::TextDisabled("Assigned Key:");
        ImGui::SameLine();
        if (m_recordedVk != 0) {
            ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.25f, 1.0f), "[ %s ]", m_recordedText.c_str());
        } else {
            ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "[ No Key Assigned ]");
        }
        ImGui::EndChild();

        ImGui::Spacing();
        ImGui::TextDisabled("Quick Select Function Keys:");
        for (int f = 1; f <= 12; ++f) {
            char fBuf[8];
            snprintf(fBuf, sizeof(fBuf), "F%d", f);
            if (f > 1 && f != 7) ImGui::SameLine();
            if (ImGui::SmallButton(fBuf)) {
                m_recordedModifiers = 0;
                m_recordedVk = VK_F1 + (f - 1);
                m_recordedText = fBuf;
            }
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if (ImGui::Button("Save Hotkey", ImVec2(120, 28))) {
            auto sfx = m_engine.getSoundboard().getSound(m_hotkeyTargetSfxId);
            if (sfx) {
                sfx->setHotkey(m_recordedModifiers, m_recordedVk, m_recordedText);
                m_engine.getSoundboard().reregisterHotkeys();
                m_statusMessage = "Bound macro: " + m_recordedText;
                m_statusTimer = 3.0f;
            }
            m_showHotkeyModal = false;
        }

        ImGui::SameLine();
        if (ImGui::Button("Clear Key", ImVec2(100, 28))) {
            auto sfx = m_engine.getSoundboard().getSound(m_hotkeyTargetSfxId);
            if (sfx) {
                sfx->clearHotkey();
                m_engine.getSoundboard().reregisterHotkeys();
                m_statusMessage = "Cleared macro hotkey";
                m_statusTimer = 3.0f;
            }
            m_showHotkeyModal = false;
        }

        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(80, 28))) {
            m_showHotkeyModal = false;
        }

        ImGui::EndPopup();
    }
}

void SoundMixerUI::renderPanicHotkeyModal() {
    if (!m_showPanicHotkeyModal) return;

    ImGui::OpenPopup("Configure Panic Hotkey");
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(420, 240));

    if (ImGui::BeginPopupModal("Configure Panic Hotkey", &m_showPanicHotkeyModal, ImGuiWindowFlags_NoResize)) {
        ImGui::Text("Configure Global 'Stop All Sounds' Panic Hotkey");
        ImGui::Spacing();
        ImGui::TextDisabled("Press any key combination to silence all sounds instantly...");
        ImGui::Spacing();

        for (int vk = 8; vk <= 255; ++vk) {
            if (vk == VK_SHIFT || vk == VK_CONTROL || vk == VK_MENU || vk == VK_LWIN || vk == VK_RWIN) {
                continue;
            }
            if ((GetAsyncKeyState(vk) & 0x8000) != 0) {
                uint32_t mods = 0;
                if ((GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0) mods |= MOD_CONTROL;
                if ((GetAsyncKeyState(VK_MENU) & 0x8000) != 0)    mods |= MOD_ALT;
                if ((GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0)   mods |= MOD_SHIFT;

                m_recordedModifiers = mods;
                m_recordedVk = vk;
                m_recordedText = SoundboardManager::formatHotkeyText(mods, vk);
                break;
            }
        }

        ImGui::BeginChild("DetectedPanicBox", ImVec2(0, 48), true);
        ImGui::AlignTextToFramePadding();
        ImGui::TextDisabled("Panic Key:");
        ImGui::SameLine();
        if (m_recordedVk != 0) {
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "[ %s ]", m_recordedText.c_str());
        } else {
            ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "[ None ]");
        }
        ImGui::EndChild();

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if (ImGui::Button("Save Panic Key", ImVec2(130, 28))) {
            m_engine.getSoundboard().setStopAllHotkey(m_recordedModifiers, m_recordedVk, m_recordedText);
            m_statusMessage = "Updated panic hotkey: " + m_recordedText;
            m_statusTimer = 3.0f;
            m_showPanicHotkeyModal = false;
        }

        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(80, 28))) {
            m_showPanicHotkeyModal = false;
        }

        ImGui::EndPopup();
    }
}

void SoundMixerUI::renderVoiceFxTab() {
    auto sources = m_engine.getMixer().getSources();
    std::shared_ptr<MicCaptureSource> activeMic = nullptr;
    for (auto& s : sources) {
        if (s->getType() == SourceType::Microphone) {
            activeMic = std::dynamic_pointer_cast<MicCaptureSource>(s);
            if (activeMic) break;
        }
    }

    if (!activeMic) {
        ImGui::BeginChild("VoiceFxNoMicChild", ImVec2(0, 0), true);
        ImGui::Spacing();
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.2f, 1.0f), "No Active Microphone Detected");
        ImGui::TextDisabled("Voice FX modulation processes your live microphone input.");
        ImGui::Spacing();
        if (ImGui::Button("+ Add Microphone", ImVec2(160, 32))) {
            m_showAddSourceModal = true;
            m_addSourceTab = 1; // Mic tab
        }
        ImGui::EndChild();
        return;
    }

    auto& vp = activeMic->getVoiceProcessor();

    // Top control banner
    ImGui::BeginChild("VoiceFxHeaderChild", ImVec2(0, 50), true);
    ImGui::AlignTextToFramePadding();
    ImGui::TextColored(ImVec4(0.26f, 0.72f, 1.00f, 1.0f), "VOICE MODULATOR");
    ImGui::SameLine();
    ImGui::TextDisabled("(Target: %s)", activeMic->getName().c_str());

    // Sidetone and FX enable buttons
    ImGui::SameLine(ImGui::GetWindowWidth() - 390.0f);
    bool sidetoneOn = m_engine.getMixer().isSidetoneEnabled();
    if (sidetoneOn) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.12f, 0.50f, 0.40f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.16f, 0.65f, 0.52f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
    } else {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.14f, 0.16f, 0.20f, 0.7f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.20f, 0.24f, 0.30f, 0.9f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.65f, 0.75f, 1.0f));
    }
    if (ImGui::Button(sidetoneOn ? "[ Sidetone: ON ]" : "[ Sidetone: OFF ]", ImVec2(130, 24))) {
        m_engine.getMixer().setSidetoneEnabled(!sidetoneOn);
    }
    ImGui::PopStyleColor(3);
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Instant Zero-Latency Sidetone (Ctrl+Shift+H)\nHear your modulated voice in your headphones.\nDoes not affect what Discord/Games hear.");
    }

    // Master enable toggle
    ImGui::SameLine();
    bool fxEnabled = vp.isEnabled();
    if (fxEnabled) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.55f, 0.45f, 1.0f));
    } else {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.22f, 0.26f, 0.8f));
    }
    if (ImGui::Button(fxEnabled ? "FX: ACTIVE" : "FX: BYPASS", ImVec2(100, 24))) {
        vp.setEnabled(!fxEnabled);
    }
    ImGui::PopStyleColor();

    ImGui::SameLine();
    float pL, pR, rL, rR;
    activeMic->getLevels(pL, pR, rL, rR);
    renderVUMeter("##micLevels", pL, pR, rL, rR, 100.0f, 22.0f);

    ImGui::EndChild();

    // Preset selection cards
    ImGui::BeginChild("VoiceFxPresetsChild", ImVec2(0, 105), true, ImGuiWindowFlags_HorizontalScrollbar);
    ImGui::TextColored(ImVec4(0.9f, 0.9f, 0.9f, 1.0f), "VOICE PRESETS & PROFILES");
    ImGui::SameLine(ImGui::GetWindowWidth() - 170.0f);
    if (ImGui::Button("+ New Custom Preset", ImVec2(150, 22))) {
        m_showNewPresetModal = true;
    }
    ImGui::Spacing();

    struct PresetInfo {
        VoicePreset preset;
        const char* label;
        const char* desc;
        const char* hotkey;
    };
    PresetInfo presets[] = {
        { VoicePreset::Clean,           "Clean",         "Natural",       "Ctrl+1" },
        { VoicePreset::DeepDemon,       "Deep Demon",    "-7 st Deep",    "Ctrl+2" },
        { VoicePreset::Chipmunk,        "Chipmunk",      "+8.5 st High",  "Ctrl+3" },
        { VoicePreset::Robot,           "Robot",         "110Hz Ring",    "Ctrl+4" },
        { VoicePreset::Radio,           "Walkie-Talkie", "Bandpass",      "Ctrl+5" },
        { VoicePreset::CaveReverb,      "Cathedral",     "Echo Space",    "Ctrl+6" },
        { VoicePreset::Alien,           "Alien",         "Modulated",     "Ctrl+7" },
        { VoicePreset::StudioBroadcast, "Studio Mic",    "Comp + EQ",     "Broadcast" },
        { VoicePreset::FemaleVoice,     "Female Voice",  "+4st Pitch/Fmt","Natural" },
        { VoicePreset::DeepRadioHost,   "Deep Radio",    "Baritone Host", "Rich Bass" }
    };

    VoicePreset curPreset = vp.getPreset();
    for (size_t i = 0; i < sizeof(presets) / sizeof(presets[0]); ++i) {
        if (i > 0) ImGui::SameLine();
        bool isSelected = (curPreset == presets[i].preset && vp.isEnabled() && m_selectedCustomPresetIdx == -1);
        if (presets[i].preset == VoicePreset::Clean && !vp.isEnabled() && m_selectedCustomPresetIdx == -1) {
            isSelected = true;
        }

        if (isSelected) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.45f, 0.65f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.24f, 0.55f, 0.78f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.3f, 0.85f, 1.0f, 1.0f));
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.12f, 0.14f, 0.18f, 0.8f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.18f, 0.22f, 0.28f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.22f, 0.25f, 0.32f, 0.5f));
        }

        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, isSelected ? 1.5f : 1.0f);
        char btnId[96];
        snprintf(btnId, sizeof(btnId), "%s\n%s\n[%s]##preset_%zu", presets[i].label, presets[i].desc, presets[i].hotkey, i);
        if (ImGui::Button(btnId, ImVec2(98, 56))) {
            m_selectedCustomPresetIdx = -1;
            vp.setPreset(presets[i].preset);
        }
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(3);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Switch to %s voice preset (Global Hotkey: %s)", presets[i].label, presets[i].hotkey);
        }
    }

    // Render User Custom Presets
    for (size_t c = 0; c < m_customPresets.size(); ++c) {
        ImGui::SameLine();
        auto& cp = m_customPresets[c];
        bool isSelected = (curPreset == VoicePreset::Custom && m_selectedCustomPresetIdx == static_cast<int>(c));

        if (isSelected) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.38f, 0.20f, 0.58f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.48f, 0.26f, 0.72f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.80f, 0.45f, 1.0f, 1.0f));
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.16f, 0.13f, 0.22f, 0.8f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.24f, 0.18f, 0.32f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.35f, 0.25f, 0.45f, 0.5f));
        }

        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, isSelected ? 1.5f : 1.0f);
        char cpBtnId[128];
        snprintf(cpBtnId, sizeof(cpBtnId), "%s\n%s\n[Custom]##cp_%zu", cp.name.c_str(), cp.description.c_str(), c);
        if (ImGui::Button(cpBtnId, ImVec2(104, 56))) {
            m_selectedCustomPresetIdx = static_cast<int>(c);
            vp.applyCustomPreset(cp);
        }
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(3);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Custom Profile: %s\n%s", cp.name.c_str(), cp.description.c_str());
        }
    }

    ImGui::EndChild();

    // Fine-tuning parameters panel
    ImGui::BeginChild("VoiceFxParamsChild", ImVec2(0, 0), true);
    ImGui::Columns(2, "VoiceFxCols", false);
    ImGui::SetColumnWidth(0, ImGui::GetWindowWidth() * 0.53f);

    // Left Column: Pitch, Formants & Modulation
    ImGui::TextColored(ImVec4(0.26f, 0.72f, 1.00f, 1.0f), "DSP MODULATION & PITCH");
    ImGui::Spacing();

    // Pitch Slider
    float pitch = vp.getPitchSemitones();
    char pitchBuf[64];
    snprintf(pitchBuf, sizeof(pitchBuf), "%.1f st (%+.0f%%)", pitch, (std::pow(2.0f, pitch / 12.0f) - 1.0f) * 100.0f);
    ImGui::Text("Pitch Transposition: %s", pitchBuf);
    if (ImGui::SliderFloat("##pitchSlider", &pitch, -12.0f, 12.0f, "%.1f st")) {
        vp.setPitchSemitones(pitch);
        if (!vp.isEnabled()) vp.setEnabled(true);
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("Reset##pitch")) {
        vp.setPitchSemitones(0.0f);
    }

    // Formant Shifter (Vocal Tract Resonance)
    float formant = vp.getFormantSemitones();
    char fmtBuf[64];
    snprintf(fmtBuf, sizeof(fmtBuf), "%.1f st", formant);
    ImGui::Text("Formant / Vocal Tract Resonance: %s", fmtBuf);
    if (ImGui::SliderFloat("##formantSlider", &formant, -8.0f, 8.0f, "%.1f st")) {
        vp.setFormantSemitones(formant);
        if (!vp.isEnabled()) vp.setEnabled(true);
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("Reset##fmt")) {
        vp.setFormantSemitones(0.0f);
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Vocal Tract Shifter:\nChanges perceived throat size, age, and gender without changing pitch!\nNo chipmunk artifacts.");
    }

    // Quick Formant Presets
    ImGui::TextDisabled("Quick Formant:");
    ImGui::SameLine();
    if (ImGui::SmallButton("Deep Male (-2.5)")) { vp.setFormantSemitones(-2.5f); if (!vp.isEnabled()) vp.setEnabled(true); }
    ImGui::SameLine();
    if (ImGui::SmallButton("Natural (0)")) { vp.setFormantSemitones(0.0f); }
    ImGui::SameLine();
    if (ImGui::SmallButton("Female (+3.0)")) { vp.setFormantSemitones(3.0f); if (!vp.isEnabled()) vp.setEnabled(true); }
    ImGui::SameLine();
    if (ImGui::SmallButton("Child (+4.5)")) { vp.setFormantSemitones(4.5f); if (!vp.isEnabled()) vp.setEnabled(true); }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Robotizer
    bool robotOn = vp.isRobotEnabled();
    if (ImGui::Checkbox("Robotizer / Carrier Ring Modulator", &robotOn)) {
        vp.setRobotEnabled(robotOn);
        if (robotOn && !vp.isEnabled()) vp.setEnabled(true);
    }
    if (robotOn) {
        float rFreq = vp.getRobotFreq();
        if (ImGui::SliderFloat("Carrier Freq##robotFreq", &rFreq, 60.0f, 300.0f, "%.0f Hz")) {
            vp.setRobotFreq(rFreq);
        }
    }

    ImGui::Spacing();

    // Megaphone / Walkie-Talkie
    bool radioOn = vp.isRadioEnabled();
    if (ImGui::Checkbox("Radio / Walkie-Talkie Bandpass Saturation", &radioOn)) {
        vp.setRadioEnabled(radioOn);
        if (radioOn && !vp.isEnabled()) vp.setEnabled(true);
    }

    ImGui::Spacing();

    // Reverb
    float reverb = vp.getReverbWet();
    ImGui::Text("Space / Cavern Reverb Mix: %.0f%%", reverb * 100.0f);
    float revVal = reverb;
    if (ImGui::SliderFloat("##reverbSlider", &revVal, 0.0f, 0.85f, "%.2f")) {
        vp.setReverbWet(revVal);
        if (revVal > 0.0f && !vp.isEnabled()) vp.setEnabled(true);
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Preset Action Buttons
    ImGui::TextColored(ImVec4(0.9f, 0.85f, 0.3f, 1.0f), "CUSTOM PRESET CONTROLS");
    if (ImGui::Button("[+] Save as New Preset", ImVec2(165, 26))) {
        m_showNewPresetModal = true;
    }
    if (m_selectedCustomPresetIdx >= 0 && m_selectedCustomPresetIdx < static_cast<int>(m_customPresets.size())) {
        ImGui::SameLine();
        auto& curCp = m_customPresets[m_selectedCustomPresetIdx];
        if (ImGui::Button("Update##curPreset", ImVec2(75, 26))) {
            curCp = vp.exportCurrentPreset(curCp.id, curCp.name, curCp.description);
            m_statusMessage = "Updated preset: " + curCp.name;
            m_statusTimer = 3.0f;
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Overwrite '%s' with current slider settings", curCp.name.c_str());

        ImGui::SameLine();
        if (ImGui::Button("Delete##curPreset", ImVec2(75, 26))) {
            m_customPresets.erase(m_customPresets.begin() + m_selectedCustomPresetIdx);
            m_selectedCustomPresetIdx = -1;
            vp.setPreset(VoicePreset::Clean);
            m_statusMessage = "Deleted custom preset";
            m_statusTimer = 3.0f;
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Delete '%s'", curCp.name.c_str());
    }

    ImGui::NextColumn();

    // Right Column: Noise Gate, Broadcast Compressor & Studio EQ
    ImGui::TextColored(ImVec4(0.26f, 0.72f, 1.00f, 1.0f), "STUDIO VOCAL DYNAMICS & EQUALIZER");
    ImGui::Spacing();

    // Smart Noise Gate
    bool gateOn = vp.isNoiseGateEnabled();
    if (ImGui::Checkbox("Low-Latency Smart Noise Gate", &gateOn)) {
        vp.setNoiseGateEnabled(gateOn);
    }

    if (gateOn) {
        float thresh = vp.getNoiseGateThresholdDb();
        ImGui::Text("Gate Threshold: %.0f dB", thresh);
        if (ImGui::SliderFloat("##gateThresh", &thresh, -70.0f, -20.0f, "%.0f dB")) {
            vp.setNoiseGateThresholdDb(thresh);
        }

        bool isOpen = vp.isGateOpen();
        if (isOpen) {
            ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.4f, 1.0f), "[OPEN] Voice Passing");
        } else {
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "[MUTED] Background Noise Blocked");
        }
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Broadcast Vocal Compressor
    bool compOn = vp.isCompressorEnabled();
    if (ImGui::Checkbox("Broadcast Vocal Compressor (Studio Punch)", &compOn)) {
        vp.setCompressorEnabled(compOn);
        if (compOn && !vp.isEnabled()) vp.setEnabled(true);
    }

    if (compOn) {
        float compThresh = vp.getCompressorThresholdDb();
        ImGui::Text("Threshold: %.0f dB", compThresh);
        if (ImGui::SliderFloat("##compThresh", &compThresh, -40.0f, 0.0f, "%.0f dB")) {
            vp.setCompressorThresholdDb(compThresh);
        }

        float compRatio = vp.getCompressorRatio();
        ImGui::Text("Ratio: %.1f:1", compRatio);
        if (ImGui::SliderFloat("##compRatio", &compRatio, 1.0f, 8.0f, "%.1f:1")) {
            vp.setCompressorRatio(compRatio);
        }

        float compMakeup = vp.getCompressorMakeupGainDb();
        ImGui::Text("Makeup Gain: %+.1f dB", compMakeup);
        if (ImGui::SliderFloat("##compMakeup", &compMakeup, 0.0f, 12.0f, "%+.1f dB")) {
            vp.setCompressorMakeupGainDb(compMakeup);
        }

        // Live Gain Reduction readout & meter
        float grDb = vp.getGainReductionDb();
        ImGui::AlignTextToFramePadding();
        ImGui::TextDisabled("Gain Reduction: %4.1f dB", grDb);
        ImGui::SameLine();
        float normGr = (std::min)(1.0f, (std::max)(0.0f, grDb / 18.0f));
        ImVec2 grPos = ImGui::GetCursorScreenPos();
        ImDrawList* dl = ImGui::GetWindowDrawList();
        float barW = 120.0f;
        float barH = 16.0f;
        dl->AddRectFilled(grPos, ImVec2(grPos.x + barW, grPos.y + barH), IM_COL32(20, 22, 28, 255), 2.0f);
        if (normGr > 0.01f) {
            dl->AddRectFilled(ImVec2(grPos.x + barW - barW * normGr, grPos.y + 1.0f),
                              ImVec2(grPos.x + barW, grPos.y + barH - 1.0f),
                              IM_COL32(245, 158, 11, 230), 2.0f);
        }
        ImGui::Dummy(ImVec2(barW, barH));
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // 3-Band Parametric Studio Equalizer (EQ)
    bool eqOn = vp.isEqEnabled();
    if (ImGui::Checkbox("3-Band Studio Parametric Equalizer", &eqOn)) {
        vp.setEqEnabled(eqOn);
        if (eqOn && !vp.isEnabled()) vp.setEnabled(true);
    }

    if (eqOn) {
        float lowGain = vp.getEqLowGainDb();
        ImGui::Text("Low Shelf / Warmth (120 Hz): %+.1f dB", lowGain);
        if (ImGui::SliderFloat("##eqLow", &lowGain, -12.0f, 12.0f, "%+.1f dB")) {
            vp.setEqLowGainDb(lowGain);
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("0dB##eqLow")) vp.setEqLowGainDb(0.0f);

        float midGain = vp.getEqMidGainDb();
        ImGui::Text("Mid Peak / Presence (2.2 kHz): %+.1f dB", midGain);
        if (ImGui::SliderFloat("##eqMid", &midGain, -12.0f, 12.0f, "%+.1f dB")) {
            vp.setEqMidGainDb(midGain);
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("0dB##eqMid")) vp.setEqMidGainDb(0.0f);

        float highGain = vp.getEqHighGainDb();
        ImGui::Text("High Shelf / Air (7.5 kHz): %+.1f dB", highGain);
        if (ImGui::SliderFloat("##eqHigh", &highGain, -12.0f, 12.0f, "%+.1f dB")) {
            vp.setEqHighGainDb(highGain);
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("0dB##eqHigh")) vp.setEqHighGainDb(0.0f);
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::TextColored(ImVec4(0.9f, 0.85f, 0.3f, 1.0f), "GLOBAL MACROS");
    ImGui::TextDisabled("- Ctrl+1 .. Ctrl+7: Voice Presets");
    ImGui::TextDisabled("- Ctrl+Shift+M: Mute/Unmute Mic");
    ImGui::TextDisabled("- Ctrl+Shift+H: Sidetone Monitor");

    ImGui::Columns(1);
    ImGui::EndChild();
}

void SoundMixerUI::renderNewPresetModal(const std::shared_ptr<MicCaptureSource>& activeMic) {
    if (!m_showNewPresetModal) return;

    ImGui::OpenPopup("Create Custom Voice Preset");
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(440, 240));

    if (ImGui::BeginPopupModal("Create Custom Voice Preset", &m_showNewPresetModal, ImGuiWindowFlags_NoResize)) {
        ImGui::TextColored(ImVec4(0.26f, 0.72f, 1.00f, 1.0f), "SAVE CURRENT SETTINGS AS NEW PRESET");
        ImGui::Spacing();

        ImGui::Text("Preset Name:");
        ImGui::SetNextItemWidth(-1);
        ImGui::InputText("##newPresetName", m_newPresetName, sizeof(m_newPresetName));

        ImGui::Spacing();
        ImGui::Text("Description:");
        ImGui::SetNextItemWidth(-1);
        ImGui::InputText("##newPresetDesc", m_newPresetDesc, sizeof(m_newPresetDesc));

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if (ImGui::Button("Save Preset", ImVec2(120, 28))) {
            if (activeMic) {
                auto& vp = activeMic->getVoiceProcessor();
                std::string id = "preset_" + std::to_string(time(nullptr));
                auto preset = vp.exportCurrentPreset(id, m_newPresetName, m_newPresetDesc);
                m_customPresets.push_back(preset);
                m_selectedCustomPresetIdx = static_cast<int>(m_customPresets.size()) - 1;
                vp.setPreset(VoicePreset::Custom);

                m_statusMessage = "Created voice preset: " + std::string(m_newPresetName);
                m_statusTimer = 3.5f;
            }
            m_showNewPresetModal = false;
        }

        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(80, 28))) {
            m_showNewPresetModal = false;
        }

        ImGui::EndPopup();
    }
}

void SoundMixerUI::render() {
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->Pos);
    ImGui::SetNextWindowSize(viewport->Size);

    ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoTitleBar |
                                   ImGuiWindowFlags_NoResize |
                                   ImGuiWindowFlags_NoMove |
                                   ImGuiWindowFlags_NoCollapse |
                                   ImGuiWindowFlags_NoBringToFrontOnFocus;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::Begin("SoundMixerMain", nullptr, windowFlags);
    ImGui::PopStyleVar();

    renderHeader();

    if (m_activeTab == 0) {
        renderSourceList();
        renderOutputsList();
        renderMasterSection();
    } else if (m_activeTab == 1) {
        renderVoiceFxTab();
    } else {
        renderSoundboardTab();
    }

    auto sources = m_engine.getMixer().getSources();
    std::shared_ptr<MicCaptureSource> activeMic = nullptr;
    for (auto& s : sources) {
        if (s->getType() == SourceType::Microphone) {
            activeMic = std::dynamic_pointer_cast<MicCaptureSource>(s);
            if (activeMic) break;
        }
    }

    renderAddSourceModal();
    renderAddOutputModal();
    renderHotkeyModal();
    renderPanicHotkeyModal();
    renderNewPresetModal(activeMic);

    ImGui::End();
}
