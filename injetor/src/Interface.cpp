#include "Interface.hpp"

#include "Injection.h"
#include <thread> 

using namespace FrameWork::Assets;

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace FrameWork
{
    void Interface::UpdateStyle()
    {
        ImGuiStyle* style = &ImGui::GetStyle();
        style->WindowRounding = 12.0f;
        style->WindowBorderSize = 1.5f;
        style->WindowPadding = ImVec2(0, 0);
        style->ScrollbarSize = 3;
        style->FrameRounding = 8.0f;
        style->ScrollbarRounding = 12.0f;
        style->GrabRounding = 8.0f;
        style->ChildRounding = 10.0f;

        // Cores PEDRIN XITS - Tema Laranja/Vermelho
        style->Colors[ImGuiCol_WindowBg]             = ImColor(80, 20, 5, 255);        // Fundo laranja avermelhado fraco
        style->Colors[ImGuiCol_Border]               = ImColor(255, 50, 0, 255);      // Bordas laranja brilhante
        style->Colors[ImGuiCol_ChildBg]              = ImColor(0, 0, 0, 0);

        style->Colors[ImGuiCol_Separator]            = ImColor(0, 0, 0, 0);
        style->Colors[ImGuiCol_SeparatorActive]      = ImColor(0, 0, 0, 0);
        style->Colors[ImGuiCol_SeparatorHovered]     = ImColor(0, 0, 0, 0);

        style->Colors[ImGuiCol_ResizeGrip]           = ImColor(0, 0, 0, 0);
        style->Colors[ImGuiCol_ResizeGripActive]     = ImColor(0, 0, 0, 0);
        style->Colors[ImGuiCol_ResizeGripHovered]    = ImColor(0, 0, 0, 0);

        style->Colors[ImGuiCol_PopupBg]              = ImColor(80, 20, 5, 220);       // Fundo laranja avermelhado fraco
        style->Colors[ImGuiCol_ScrollbarBg]          = ImColor(0, 0, 0, 0);
        style->Colors[ImGuiCol_ScrollbarGrab]        = ImColor(255, 50, 0, 255);      // Laranja principal
        style->Colors[ImGuiCol_ScrollbarGrabActive]  = ImColor(255, 80, 0, 255);      // Laranja hover
        style->Colors[ImGuiCol_ScrollbarGrabHovered] = ImColor(255, 180, 0, 255);     // Laranja claro

        style->Colors[ImGuiCol_Text]                 = ImColor(255, 255, 255, 255);   // Branco - texto principal
        style->Colors[ImGuiCol_TextDisabled]         = ImColor(200, 200, 200, 255);   // Cinza claro - texto secundário
        style->Colors[ImGuiCol_TextSelectedBg]       = ImColor(255, 150, 0, 200);     // Laranja médio - elementos interativos

        style->Colors[ImGuiCol_TitleBg]              = ImColor(80, 20, 5, 242);       // Fundo laranja avermelhado fraco - header
        style->Colors[ImGuiCol_TitleBgActive]        = ImColor(80, 20, 5, 242);       // Fundo laranja avermelhado fraco - header
        style->Colors[ImGuiCol_TitleBgCollapsed]     = ImColor(80, 20, 5, 242);       // Fundo laranja avermelhado fraco - header

        style->Colors[ImGuiCol_Button]               = ImVec4(0.24f, 0.04f, 0.0f, 1.0f);  // Fundo escuro laranja
        style->Colors[ImGuiCol_ButtonHovered]        = ImVec4(0.31f, 0.06f, 0.0f, 1.0f);  // Fundo escuro hover
        style->Colors[ImGuiCol_ButtonActive]         = ImVec4(0.20f, 0.02f, 0.0f, 1.0f);  // Fundo escuro ativo

        style->Colors[ImGuiCol_CheckMark]            = ImColor(255, 180, 0, 255);     // Laranja claro - linhas decorativas
        style->Colors[ImGuiCol_FrameBg]              = ImColor(255, 50, 0, 255);      // Laranja principal - bordas e linhas
        style->Colors[ImGuiCol_FrameBgHovered]       = ImColor(255, 80, 0, 255);      // Laranja hover - efeitos de hover
        style->Colors[ImGuiCol_FrameBgActive]        = ImColor(200, 80, 0, 120);      // Laranja escuro - separadores
    }

    // Variáveis globais
    int tablogin = 0;
    int active_tab = 0;
    float tab_alpha = 0.f;
    static float load_anim_time = 0.0f;

    bool closeButtonModerno(const char* str_id, const ImVec2& pos, ImVec2 size)
    {
        ImGuiContext& g = *GImGui;
        ImGuiWindow* window = g.CurrentWindow;
        ImGuiIO& io = ImGui::GetIO();

        const ImRect bb(pos, pos + size);
        ImRect bb_interact = bb;
        bool is_clipped = !ImGui::ItemAdd(bb_interact, ImGui::GetID(str_id));

        bool hovered, held;
        bool pressed = ImGui::ButtonBehavior(bb_interact, ImGui::GetID(str_id), &hovered, &held);

        if (is_clipped)
            return pressed;

        ImGuiID storage_id = ImGui::GetID(str_id);
        ImGuiStorage* storage = &g.CurrentWindow->StateStorage;

        float* hover_anim = storage->GetFloatRef(storage_id, 0.0f);
        float* click_anim = storage->GetFloatRef(storage_id + 1, 0.0f);

        float hover_speed = 15.0f * io.DeltaTime;
        float click_speed = 20.0f * io.DeltaTime;

        *hover_anim = ImLerp(*hover_anim, hovered ? 1.0f : 0.0f, hover_speed);
        *click_anim = ImLerp(*click_anim, held ? 1.0f : 0.0f, click_speed);

        ImVec2 center = bb.GetCenter();
        float scale = 1.0f + (*hover_anim * 0.08f) - (*click_anim * 0.04f);
        ImVec2 scaled_size = ImVec2(size.x * scale, size.y * scale);
        ImVec2 scaled_offset = ImVec2((scaled_size.x - size.x) * 0.5f, (scaled_size.y - size.y) * 0.5f);

        ImRect scaled_bb = ImRect(bb.Min - scaled_offset, bb.Max + scaled_offset);

        ImU32 bg_color = IM_COL32(80, 20, 5, 220);                              // Fundo laranja avermelhado fraco
        ImU32 border_color = IM_COL32(255, 50, 0, (int)(150 * *hover_anim));    // Laranja principal

        float corner_radius = 6.0f;
        window->DrawList->AddRectFilled(scaled_bb.Min, scaled_bb.Max, bg_color, corner_radius);

        if (*hover_anim > 0.01f) {
            window->DrawList->AddRect(scaled_bb.Min, scaled_bb.Max, border_color, corner_radius, 0, 1.5f);
        }

        if (*hover_anim > 0.01f) {
            ImU32 glow_color = IM_COL32(255, 50, 0, (int)(30 * *hover_anim));   // Laranja principal
            for (int i = 1; i <= 2; i++) {
                float glow_expand = i * 2.0f * *hover_anim;
                ImRect glow_bb = ImRect(scaled_bb.Min - ImVec2(glow_expand, glow_expand),
                    scaled_bb.Max + ImVec2(glow_expand, glow_expand));
                window->DrawList->AddRect(glow_bb.Min, glow_bb.Max, glow_color, corner_radius + glow_expand, 0, 1.0f);
            }
        }

        float cross_size = (g.FontSize * 0.28f) * scale;
        ImU32 cross_color = *hover_anim > 0.5f ? IM_COL32(255, 50, 0, 255) : IM_COL32(200, 200, 200, 255);
        float line_thickness = 1.5f + (*hover_anim * 0.5f);

        window->DrawList->AddLine(center + ImVec2(-cross_size, -cross_size),
            center + ImVec2(cross_size, cross_size), cross_color, line_thickness);
        window->DrawList->AddLine(center + ImVec2(cross_size, -cross_size),
            center + ImVec2(-cross_size, cross_size), cross_color, line_thickness);

        return pressed;
    }

    bool CustomButton(const char* label, ImVec2 size, bool primary = true)
    {
        ImGuiContext& g = *GImGui;
        ImGuiWindow* window = g.CurrentWindow;
        ImGuiIO& io = ImGui::GetIO();

        ImVec2 pos = ImGui::GetCursorScreenPos();
        const ImRect bb(pos, pos + size);

        ImGui::ItemSize(bb);
        if (!ImGui::ItemAdd(bb, ImGui::GetID(label)))
            return false;

        bool hovered, held;
        bool pressed = ImGui::ButtonBehavior(bb, ImGui::GetID(label), &hovered, &held);

        ImGuiID storage_id = ImGui::GetID(label);
        ImGuiStorage* storage = &window->StateStorage;
        float* hover_anim = storage->GetFloatRef(storage_id, 0.0f);

        *hover_anim = ImLerp(*hover_anim, hovered ? 1.0f : 0.0f, 15.0f * io.DeltaTime);

        ImU32 bg_color, text_color;
        if (primary) {
            // Botão laranja principal
            bg_color = held ? IM_COL32(200, 80, 0, 120) :                       // Laranja escuro - separadores
                       ImGui::ColorConvertFloat4ToU32(ImVec4(
                           ImLerp(255/255.0f, 255/255.0f, *hover_anim),         // Laranja principal -> Laranja hover
                           ImLerp(50/255.0f,  80/255.0f,  *hover_anim),
                           ImLerp(0/255.0f,   0/255.0f,   *hover_anim),
                           1.0f
                       ));
            text_color = IM_COL32(255, 255, 255, 255);                          // Branco - texto principal
        } else {
            bg_color = held ? IM_COL32(80, 15, 0, 180) :                        // Fundo escuro hover
                       ImGui::ColorConvertFloat4ToU32(ImVec4(
                           ImLerp(60/255.0f, 80/255.0f, *hover_anim),           // Fundo escuro ativo -> Fundo escuro hover
                           ImLerp(10/255.0f, 15/255.0f, *hover_anim),
                           ImLerp(0/255.0f,  0/255.0f,  *hover_anim),
                           220/255.0f
                       ));
            text_color = IM_COL32(255, 50, 0, 255);                             // Laranja principal
        }

        float corner_radius = 8.0f;
        window->DrawList->AddRectFilled(bb.Min, bb.Max, bg_color, corner_radius);

        if (primary && *hover_anim > 0.01f) {
            ImU32 border_color = IM_COL32(255, 180, 0, (int)(100 * *hover_anim));  // Laranja claro - linhas decorativas
            window->DrawList->AddRect(bb.Min, bb.Max, border_color, corner_radius, 0, 1.5f);
        }

        ImVec2 text_size = ImGui::CalcTextSize(label);
        ImVec2 text_pos = ImVec2(
            bb.Min.x + (size.x - text_size.x) * 0.5f,
            bb.Min.y + (size.y - text_size.y) * 0.5f
        );
        window->DrawList->AddText(text_pos, text_color, label);

        return pressed;
    }

    void Interface::RenderGui()
    {
        static float alpha = 0.0f;
        float deltaTime = ImGui::GetIO().DeltaTime;
        alpha = ImClamp(alpha + deltaTime * 2.0f, 0.0f, 1.0f);

        if (CurrentTab == 0)
        {
            ImGui::SetNextWindowPos(ImVec2(0, 0));
            ImGui::SetNextWindowSize(ImVec2(565, 385));

            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 12.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
            ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));

            ImGui::Begin("L", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

            ImDrawList* bgDrawList = ImGui::GetWindowDrawList();
            ImVec2 bgPos = ImGui::GetWindowPos();
            ImVec2 bgSize = ImGui::GetWindowSize();

            bgDrawList->AddRectFilled(
                bgPos,
                ImVec2(bgPos.x + bgSize.x, bgPos.y + bgSize.y),
                IM_COL32(80, 20, 5, 255),                                        // Fundo laranja avermelhado fraco
                12.0f,
                ImDrawFlags_RoundCornersAll
            );

            ImGui::PopStyleColor();
            ImGui::PopStyleVar(2);

            tab_alpha = ImClamp(tab_alpha + (5.f * deltaTime * (tablogin == active_tab ? 1.f : -1.f)), 0.f, 1.f);

            if (tab_alpha <= 0.01f)
                active_tab = tablogin;

            ImGuiStyle& style = ImGui::GetStyle();
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, tab_alpha * style.Alpha);

            if (tablogin == 0)
            {
                tablogin = 2;
            }
            else if (tablogin == 1)
            {
                ImDrawList* DrawList = ImGui::GetWindowDrawList();
                ImVec2 Pos = ImGui::GetWindowPos();
                ImVec2 Size = ImGui::GetWindowSize();

                ImVec2 headerStart = Pos;
                ImVec2 headerEnd = ImVec2(Pos.x + Size.x, Pos.y + 110);             // Reduzido de 130 para 110 para logo menor

                DrawList->AddRectFilled(
                    headerStart,
                    headerEnd,
                    IM_COL32(80, 20, 5, 242),                                    // Fundo laranja avermelhado fraco - header
                    12.0f,
                    ImDrawFlags_RoundCornersTop
                );

                float logoHeight = 100.0f;                                           // Reduzido de 60 para 40 - logo menor
                float logoWidth = 100.0f;                                            // Reduzido de 60 para 40 - logo menor
                float logoX = Pos.x + 25.0f;                                        // Ajustado para centralizar melhor
                float logoY = Pos.y + 10.0f;                                        // Mantido na mesma posição vertical

                DrawList->AddImage(
                    Assets::UserLogo,
                    ImVec2(logoX, logoY),
                    ImVec2(logoX + logoWidth, logoY + logoHeight),
                    ImVec2(0, 0),
                    ImVec2(1, 1)
                );

                if (closeButtonModerno("Close", ImVec2(Pos.x + 525, Pos.y + 20), ImVec2(28, 28))) {
                    exit(1);
                }

                // Título PEDRIN XITS no header - centralizado entre logo e botão X com fonte grossa e grande
                ImGui::PushFont(Assets::InterBlackBig);
                ImVec2 titleSize = ImGui::CalcTextSize("PEDRIN XITS");
                float titleX = Pos.x + (Size.x - titleSize.x) * 0.5f; // Centralizado
                DrawList->AddText(
                    Assets::InterBlackBig, 28.0f,                           // Fonte InterBlack tamanho 28
                    ImVec2(titleX, Pos.y + 40),                             // Ajustado para o meio do header menor
                    IM_COL32(255, 50, 0, 255),
                    "PEDRIN XITS"
                );
                ImGui::PopFont();

                ImVec2 cardPos = ImVec2(Pos.x + 40, Pos.y + 120);                   // Ajustado para ficar abaixo do header menor
                ImVec2 cardSize = ImVec2(485, 220);                             // Aumentado altura para compensar header menor
                ImVec2 cardEnd = cardPos + cardSize;

                DrawList->AddRectFilled(cardPos, cardEnd, IM_COL32(80, 20, 5, 220), 10.0f);     // Fundo laranja avermelhado fraco
                // Borda removida conforme solicitado

                // Painel de informações na posição original
                // Centralizar elementos no painel
                float windowCenterX = Pos.x + Size.x * 0.5f;
                float windowCenterY = Pos.y + Size.y * 0.5f;
                
                // Painel de informações centralizado
                ImVec2 infoPanelSize = ImVec2(445, 80);
                ImVec2 infoPanelPos = ImVec2(windowCenterX - infoPanelSize.x * 0.5f, windowCenterY - 60);
                ImVec2 infoPanelEnd = infoPanelPos + infoPanelSize;

                DrawList->AddRectFilled(infoPanelPos, infoPanelEnd, IM_COL32(80, 15, 0, 180), 8.0f);  // Fundo escuro hover
                DrawList->AddRect(infoPanelPos, infoPanelEnd, IM_COL32(255, 50, 0, 255), 8.0f, 0, 1.0f);  // Laranja principal - bordas e linhas

                // Ícone de status laranja
                ImVec2 statusCircleCenter = ImVec2(infoPanelPos.x + 30, infoPanelPos.y + 40);
                DrawList->AddCircleFilled(statusCircleCenter, 6.0f, IM_COL32(255, 50, 0, 255));           // Laranja principal
                DrawList->AddCircle(statusCircleCenter, 8.0f, IM_COL32(255, 180, 0, 200), 0, 1.5f);      // Laranja claro - linhas decorativas

                DrawList->AddText(
                    Assets::InterSemiBold, 15.0f,
                    ImVec2(infoPanelPos.x + 50, infoPanelPos.y + 20),
                    IM_COL32(255, 255, 255, 255),                                // Branco - texto principal
                    "Pronto Para Abrir O Painel"
                );

                DrawList->AddText(
                    Assets::InterSemiBold, 12.0f,
                    ImVec2(infoPanelPos.x + 50, infoPanelPos.y + 42),
                    IM_COL32(220, 220, 220, 255),                                // Branco suave - labels
                    "Sistema: Online | Versao: 2.0"
                );

                // Botão centralizado abaixo do painel de informações
                ImVec2 buttonSize = ImVec2(445, 45);
                ImVec2 buttonPos = ImVec2(windowCenterX - buttonSize.x * 0.5f, infoPanelEnd.y + 20);
                ImGui::SetCursorPos(ImVec2(buttonPos.x - Pos.x, buttonPos.y - Pos.y));
                if (CustomButton("Abrir Painel", buttonSize, true))
                {
                    tablogin = 3;
                    load_anim_time = 0.0f;
                }

                // Borda principal desenhada por último (por cima de tudo)
                DrawList->AddRect(
                    Pos,
                    ImVec2(Pos.x + Size.x, Pos.y + Size.y),
                    IM_COL32(255, 50, 0, 255),                                   // Laranja principal - bordas e linhas
                    12.0f,
                    ImDrawFlags_RoundCornersAll,
                    3.0f
                );
            }
            else if (tablogin == 3)
            {
                const float duration_load_anim = 1.5f;

                load_anim_time += ImGui::GetIO().DeltaTime;

                float progress = ImClamp(load_anim_time / duration_load_anim, 0.0f, 1.0f);
                float alpha = 1.0f;
                float exit_progress = 0.0f;
                float exit_start_time = duration_load_anim - 0.3f;

                if (load_anim_time > exit_start_time)
                {
                    exit_progress = (load_anim_time - exit_start_time) / (duration_load_anim - exit_start_time);
                    alpha = 1.0f - exit_progress;
                }

                if (load_anim_time >= duration_load_anim && alpha <= 0.0f)
                {
                    InjectDLLFromWeb();
                    tablogin = 1;
                    load_anim_time = 0.0f;
                }

                ImGui::PushStyleVar(ImGuiStyleVar_Alpha, alpha);
                ImDrawList* drawList = ImGui::GetWindowDrawList();
                ImVec2 pos = ImGui::GetWindowPos();
                ImVec2 size = ImGui::GetWindowSize();

                float entry_time = 0.3f;
                float entry_progress = ImClamp(load_anim_time / entry_time, 0.0f, 1.0f);
                float eased_entry_progress = 1.0f - powf(1.0f - entry_progress, 2.0f);

                float t = ImGui::GetTime() * 6.0f;
                float spacing = 35.0f;
                float baseSize = 8.0f;
                float centerX = pos.x + size.x * 0.5f;
                float centerY = pos.y + size.y * 0.5f;

                // Círculos laranjas animados
                for (int i = 0; i < 3; ++i) {
                    float offsetX = (i - 1) * spacing * eased_entry_progress;
                    float bounce = sinf(t + i * 0.8f) * 10.0f;
                    float scale = 1.0f + 0.3f * sinf(t + i * 0.8f);
                    float alphaMult = 0.6f + 0.4f * sinf(t + i * 0.8f);
                    float exit_offset_y = exit_progress * (size.y * 0.5f);
                    float exit_scale_mult = 1.0f - exit_progress;

                    ImVec2 center = ImVec2(centerX + offsetX, centerY + bounce + exit_offset_y);
                    float radius = baseSize * scale * exit_scale_mult;

                    int a = static_cast<int>(alpha * alphaMult * 255);
                    drawList->AddCircleFilled(center, radius, IM_COL32(255, 50, 0, a));        // Laranja principal
                    drawList->AddCircle(center, radius + 3.0f, IM_COL32(255, 180, 0, a / 3), 0, 2.0f);  // Laranja claro
                }

                const char* loadingText = "Abrindo Painel...";
                ImVec2 textSize = ImGui::CalcTextSize(loadingText);
                ImVec2 textPos = ImVec2(centerX - textSize.x * 0.5f, centerY + 50);

                drawList->AddText(
                    Assets::InterSemiBold, 14.0f,
                    textPos,
                    IM_COL32(255, 50, 0, (int)(alpha * 255)),                    // Laranja principal
                    loadingText
                );

                ImGui::PopStyleVar();
            }
            else if (tablogin == 2)
            {
                const float duration = 1.5f;

                static float anim_time = 0.0f;
                anim_time += ImGui::GetIO().DeltaTime;

                float progress = ImClamp(anim_time / duration, 0.0f, 1.0f);
                float alpha = 1.0f;
                float exit_progress = 0.0f;
                float exit_start_time = duration - 0.3f;

                if (anim_time > exit_start_time)
                {
                    exit_progress = (anim_time - exit_start_time) / (duration - exit_start_time);
                    alpha = 1.0f - exit_progress;
                }

                if (anim_time >= duration && alpha <= 0.0f)
                {
                    tablogin = 1;
                    anim_time = 0.0f;
                }

                ImGui::PushStyleVar(ImGuiStyleVar_Alpha, alpha);
                ImDrawList* drawList = ImGui::GetWindowDrawList();
                ImVec2 pos = ImGui::GetWindowPos();
                ImVec2 size = ImGui::GetWindowSize();

                float entry_time = 0.3f;
                float entry_progress = ImClamp(anim_time / entry_time, 0.0f, 1.0f);
                float eased_entry_progress = 1.0f - powf(1.0f - entry_progress, 2.0f);

                float t = ImGui::GetTime() * 6.0f;
                float spacing = 35.0f;
                float baseSize = 8.0f;
                float centerX = pos.x + size.x * 0.5f;
                float centerY = pos.y + size.y * 0.5f;

                // Círculos laranjas animados
                for (int i = 0; i < 3; ++i) {
                    float offsetX = (i - 1) * spacing * eased_entry_progress;
                    float bounce = sinf(t + i * 0.8f) * 10.0f;
                    float scale = 1.0f + 0.3f * sinf(t + i * 0.8f);
                    float alphaMult = 0.6f + 0.4f * sinf(t + i * 0.8f);
                    float exit_offset_y = exit_progress * (size.y * 0.5f);
                    float exit_scale_mult = 1.0f - exit_progress;

                    ImVec2 center = ImVec2(centerX + offsetX, centerY + bounce + exit_offset_y);
                    float radius = baseSize * scale * exit_scale_mult;

                    int a = static_cast<int>(alpha * alphaMult * 255);
                    drawList->AddCircleFilled(center, radius, IM_COL32(255, 50, 0, a));        // Laranja principal
                    drawList->AddCircle(center, radius + 3.0f, IM_COL32(255, 180, 0, a / 3), 0, 2.0f);  // Laranja claro
                }

                ImGui::PopStyleVar();
            }

            ImGui::PopStyleVar();
            Overlay.Mouse_Move();
            ImGui::End();
        }
    }
}
