// Core
#include <Imgui.h>
#include <ImGui/imgui_impl_win32.h>
#include <ImGui/imgui_internal.h>

// rePlayer
#include "GraphicsImGui.h"

// data
#include "GraphicsFont3x5.h"
#include "GraphicsFontLog.h"
#include "MediaIcons.h"

namespace rePlayer
{
    static int32_t s_widths[] = { 19, 19, 15, 14, 14, 25, 25, 25, 21, 25, 25, 15, 15, 15 };
    static int32_t s_rectIds[NumItemsOf(s_widths)];
    static ImFontLoader s_fontLoader;

    static bool s_fontSrcContainsGlyph(ImFontAtlas* atlas, ImFontConfig* src, ImWchar codepoint)
    {
        if (codepoint >= ImWchar(0xE000) && codepoint < ImWchar(0xE000 + NumItemsOf(s_widths)))
            return true;
        return ImFontAtlasGetFontLoaderForStbTruetype()->FontSrcContainsGlyph(atlas, src, codepoint);
    }

    static bool s_fontBakedLoadGlyph(ImFontAtlas* atlas, ImFontConfig* src, ImFontBaked* baked, void* loader_data_for_baked_src, ImWchar codepoint, ImFontGlyph* out_glyph)
    {
        if (codepoint >= ImWchar(0xE000) && codepoint < ImWchar(0xE000 + NumItemsOf(s_widths)))
        {
            const int w = s_widths[codepoint - 0xe000];
            const int h = 15;

            ImFontAtlasRectId pack_id = ImFontAtlasPackAddRect(atlas, w, h);
            if (pack_id == ImFontAtlasRectId_Invalid)
            {
                // Pathological out of memory case (TexMaxWidth/TexMaxHeight set too small?)
                IM_ASSERT(pack_id != ImFontAtlasRectId_Invalid && "Out of texture memory.");
                return false;
            }
            ImTextureRect* r = ImFontAtlasPackGetRect(atlas, pack_id);

            // Render
            ImFontAtlasBuilder* builder = atlas->Builder;
            builder->TempBuffer.resize(w * h);
            unsigned char* bitmap_pixels = builder->TempBuffer.Data;

            uint32_t mediaIconsOffset = 0;
            for (uint32_t i = 0; i < uint32_t(codepoint - 0xE000); i++)
                mediaIconsOffset += s_widths[i];
            for (int32_t y = 0; y < r->h; y++)
            {
                auto* dst = bitmap_pixels + w * y;
                auto* bmp = reinterpret_cast<const uint8_t*>(s_mediaIcons) + mediaIconsOffset + 272 * y;
                memcpy(dst, bmp, s_widths[codepoint - 0xE000]);
            }

            out_glyph->Codepoint = codepoint;
            out_glyph->AdvanceX = s_widths[codepoint - 0xE000] * ImGui::GetStyle().FontScaleMain;
            out_glyph->X0 = 0.0f;
            out_glyph->Y0 = -1.0f * ImGui::GetStyle().FontScaleMain;
            out_glyph->X1 = r->w * ImGui::GetStyle().FontScaleMain;
            out_glyph->Y1 = (r->h - 1.0f) * ImGui::GetStyle().FontScaleMain;
            out_glyph->Visible = true;
            out_glyph->Colored = false;
            out_glyph->PackId = pack_id;

            ImFontAtlasBakedSetFontGlyphBitmap(atlas, baked, src, out_glyph, r, bitmap_pixels, ImTextureFormat_Alpha8, w);

            return true;
        }
        return ImFontAtlasGetFontLoaderForStbTruetype()->FontBakedLoadGlyph(atlas, src, baked, loader_data_for_baked_src, codepoint, out_glyph);
    }

    GraphicsImGui::GraphicsImGui(void* hWnd)
        : m_hWnd(hWnd)
        , m_font3x5_data(reinterpret_cast<const uint8_t*>(s_font3x5_data))
    {}

    bool GraphicsImGui::Init(bool isTextureRGBA)
    {
        ImGui_ImplWin32_Init(m_hWnd);

        // Build texture atlas
        ImGuiIO& io = ImGui::GetIO();
        io.Fonts->TexDesiredFormat = isTextureRGBA ? ImTextureFormat_RGBA32 : ImTextureFormat_Alpha8;

        ImFontAtlasBuildSetupFontLoader(io.Fonts, ImFontAtlasGetFontLoaderForStbTruetype());

        ImFontConfig imFontConfig = {};
        imFontConfig.OversampleH = imFontConfig.OversampleV = 1;
        imFontConfig.PixelSnapH = true;

        s_fontLoader = *ImFontAtlasGetFontLoaderForStbTruetype();
        s_fontLoader.FontSrcContainsGlyph = s_fontSrcContainsGlyph;
        s_fontLoader.FontBakedLoadGlyph = s_fontBakedLoadGlyph;
        imFontConfig.FontLoader = &s_fontLoader;

        io.Fonts->AddFontDefault(&imFontConfig);

/*
        {
            ImFontConfig fontConfig;
            fontConfig.SizePixels = 12.0f;
            fontConfig.OversampleH = fontConfig.OversampleV = 1;
            fontConfig.PixelSnapH = true;
            fontConfig.GlyphOffset.y = 0.0f;
            strcpy_s(fontConfig.Name, "test");
            io.Fonts->AddFontFromMemoryCompressedBase85TTF(s_font_compressed_data_base85, 0, &fontConfig);
        }
*/
        {
            ImFontConfig fontConfig;
            fontConfig.SizePixels = 9.0f;
            fontConfig.OversampleH = fontConfig.OversampleV = 1;
            fontConfig.PixelSnapH = true;
            fontConfig.GlyphOffset.y = 0.0f;
#ifdef _DEBUG
            strcpy_s(fontConfig.Name, "Log");
#endif
            io.Fonts->AddFontFromMemoryCompressedBase85TTF(s_fontLog_compressed_data_base85, 0, &fontConfig);
        }
        {
            m_3x5BaseRect = io.Fonts->AddCustomRect(3, 5); // first character is '!' (we don't need the ' ')
            for (int32_t i = 2; i < 16 * 6 - 1; ++i)
            {
                auto b = io.Fonts->AddCustomRect(3, 5);
                assert(b == (m_3x5BaseRect + i - 1));
                (void)b;
            }
        }

        return OnInit();
    }

    void GraphicsImGui::OnDelete()
    {
        for (ImTextureData* tex : ImGui::GetPlatformIO().Textures)
            if (tex->RefCount == 1)
                DestroyTexture(*tex);

        ImGuiIO& io = ImGui::GetIO();
        io.Fonts->Clear();

        ImGui_ImplWin32_Shutdown();
    }
}
// namespace rePlayer