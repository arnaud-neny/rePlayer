#include "Patterns.h"

#include <Core/Window.inl.h>
#include <ImGui.h>

#include <Deck/Player.h>
#include <Graphics/Graphics.h>

namespace rePlayer
{
    Patterns::Patterns()
        : Window("Patterns", ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)
    {}

    Patterns::~Patterns()
    {}

    void Patterns::Update(Player* player)
    {
        if (player && IsEnabled())
            m_patterns = player->GetPatterns(m_numLines, kCharWidth, 2, Replay::Patterns::Flags(m_flags | Replay::Patterns::kEnablePatterns));
        else
            m_patterns = {};
    }

    std::string Patterns::OnGetWindowTitle()
    {
        ImGui::SetNextWindowPos(ImGui::GetMousePos(), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(320.0f, 256.0f), ImGuiCond_FirstUseEver);

        return "Patterns";
    }

    void Patterns::OnDisplay()
    {
        auto region = ImGui::GetContentRegionAvail();
        auto xMin = ImGui::GetCursorScreenPos().x;
        auto yMin = ImGui::GetCursorScreenPos().y;
        auto xMax = xMin + region.x;
        auto yMax = yMin + region.y;

        auto height = int32_t(region.y);
        auto numHalfLines = (m_numLines - 1) / 2;

        auto& patterns = m_patterns;
        if (patterns.lines.IsNotEmpty())
        {
            float scale = m_scale == Scale::kFit ? (xMax - xMin) / patterns.width : (m_scale.As<float>() + 1.0f);

            ImDrawList* drawList = ImGui::GetWindowDrawList();
            drawList->PushClipRect({ xMin, yMin }, { xMax, yMax });
            ImGuiIO& io = ImGui::GetIO();
            float y = yMin + (height / 2) - (kCharHeight * scale / 2 - 1) - numHalfLines * kCharHeight * scale;
            uint32_t colors[] = { 0xffFFffFF, 0xffa0a0a0 };
            if (patterns.currentLine & 1)
                std::swap(colors[0], colors[1]);
            auto baseRect = Graphics::Get3x5BaseRect();
            auto* c = patterns.lines.Items();

            for (auto& size : patterns.sizes)
            {
                if (size > 0)
                {
                    float x = xMin + (int32_t(xMax) - int32_t(xMin) - patterns.width * scale) / 2;

                    auto color = colors[0];
                    if (m_flags & Replay::Patterns::kEnableHighlight && (&size - patterns.sizes) == numHalfLines)
                        drawList->AddRect({ x - 1.0f, y - 1.0f }, { x + patterns.width * scale, y + kCharHeight * scale }, 0x8fFFffFF);
                    drawList->PrimReserve(int32_t(6 * size), int32_t(4 * size));
                    auto idxBase = drawList->_VtxCurrentIdx;
                    for (; *c; ++c)
                    {
                        auto cc = *c;
                        if (cc < 0)
                        {
                            color = Replay::Patterns::colors[cc & 0x7f];
                            continue;
                        }
                        if (cc >= 33 && cc <= 126)
                        {
                            ImFontAtlasRect rect;
                            io.Fonts->GetCustomRect(baseRect + cc - 33, &rect);
                            ImVec2 uv0 = rect.uv0, uv1 = rect.uv1;

                            drawList->PrimWriteVtx({ x, y }, uv0, color);
                            drawList->PrimWriteVtx({ x, y + 5.0f * scale }, { uv0.x, uv1.y }, color);
                            drawList->PrimWriteVtx({ x + 3.0f * scale, y }, { uv1.x, uv0.y }, color);
                            drawList->PrimWriteVtx({ x + 3.0f * scale, y + 5.0f * scale }, uv1, color);

                            drawList->PrimWriteIdx(ImDrawIdx(idxBase + 0));
                            drawList->PrimWriteIdx(ImDrawIdx(idxBase + 1));
                            drawList->PrimWriteIdx(ImDrawIdx(idxBase + 2));

                            drawList->PrimWriteIdx(ImDrawIdx(idxBase + 2));
                            drawList->PrimWriteIdx(ImDrawIdx(idxBase + 1));
                            drawList->PrimWriteIdx(ImDrawIdx(idxBase + 3));
                        }
                        else
                        {
                            drawList->PrimWriteVtx({ 0.0f, 0.0f }, { 0.0f, 0.0f }, 0);
                            drawList->PrimWriteVtx({ 0.0f, 0.0f }, { 0.0f, 0.0f }, 0);
                            drawList->PrimWriteVtx({ 0.0f, 0.0f }, { 0.0f, 0.0f }, 0);
                            drawList->PrimWriteVtx({ 0.0f, 0.0f }, { 0.0f, 0.0f }, 0);

                            drawList->PrimWriteIdx(ImDrawIdx(idxBase));
                            drawList->PrimWriteIdx(ImDrawIdx(idxBase));
                            drawList->PrimWriteIdx(ImDrawIdx(idxBase));

                            drawList->PrimWriteIdx(ImDrawIdx(idxBase));
                            drawList->PrimWriteIdx(ImDrawIdx(idxBase));
                            drawList->PrimWriteIdx(ImDrawIdx(idxBase));
                        }

                        idxBase += 4;
                        x += scale * (cc == ' ' ? 2 : kCharWidth);
                    }
                }
                std::swap(colors[0], colors[1]);
                y += kCharHeight * scale;
                c++;
            }
            drawList->PopClipRect();
        }

        numHalfLines = (((height - kCharHeight) / 2) + kCharHeight - 1) / kCharHeight;
        m_numLines = 1 + 2 * numHalfLines;

        if (ImGui::BeginPopupContextWindow("Flags"))
        {
            bool isSelected = m_flags & Replay::Patterns::kEnableRowNumbers;
            if (ImGui::Checkbox("Display Row Numbers", &isSelected))
                m_flags = Replay::Patterns::Flags(isSelected ? m_flags | Replay::Patterns::kEnableRowNumbers : (m_flags & ~Replay::Patterns::kEnableRowNumbers));
            isSelected = m_flags & Replay::Patterns::kEnableInstruments;
            if (ImGui::Checkbox("Display Instruments", &isSelected))
                m_flags = Replay::Patterns::Flags(isSelected ? m_flags | Replay::Patterns::kEnableInstruments : (m_flags & ~Replay::Patterns::kEnableInstruments));
            isSelected = m_flags & Replay::Patterns::kEnableVolume;
            if (ImGui::Checkbox("Display Volume", &isSelected))
                m_flags = Replay::Patterns::Flags(isSelected ? m_flags | Replay::Patterns::kEnableVolume : (m_flags & ~Replay::Patterns::kEnableVolume));
            isSelected = m_flags & Replay::Patterns::kEnableEffects;
            if (ImGui::Checkbox("Display Effects", &isSelected))
                m_flags = Replay::Patterns::Flags(isSelected ? m_flags | Replay::Patterns::kEnableEffects : (m_flags & ~Replay::Patterns::kEnableEffects));
            isSelected = m_flags & Replay::Patterns::kEnableColors;
            if (ImGui::Checkbox("Display Colors", &isSelected))
                m_flags = Replay::Patterns::Flags(isSelected ? m_flags | Replay::Patterns::kEnableColors : (m_flags & ~Replay::Patterns::kEnableColors));
            isSelected = m_flags & Replay::Patterns::kEnableHighlight;
            if (ImGui::Checkbox("Display Highlight", &isSelected))
                m_flags = Replay::Patterns::Flags(isSelected ? m_flags | Replay::Patterns::kEnableHighlight : (m_flags & ~Replay::Patterns::kEnableHighlight));
            ImGui::Separator();
            if (ImGui::BeginMenu("Scale"))
            {
                isSelected = m_scale == Scale::k1;
                if (ImGui::MenuItem("x1", nullptr, &isSelected))
                    m_scale = Scale::k1;
                isSelected = m_scale == Scale::k2;
                if (ImGui::MenuItem("x2", nullptr, &isSelected))
                    m_scale = Scale::k2;
                isSelected = m_scale == Scale::k3;
                if (ImGui::MenuItem("x3", nullptr, &isSelected))
                    m_scale = Scale::k3;
                isSelected = m_scale == Scale::k4;
                if (ImGui::MenuItem("x4", nullptr, &isSelected))
                    m_scale = Scale::k4;
                isSelected = m_scale == Scale::kFit;
                if (ImGui::MenuItem("Fit to window", nullptr, &isSelected))
                    m_scale = Scale::kFit;

                ImGui::EndMenu();
            }
            ImGui::EndPopup();
        }
    }
}
// namespace rePlayer