#include "../src/Assets.hpp"

#include "../resource/Assets/ImagesBytes.hpp"
#include "../resource/Assets/nicolasnetoo.hpp"
#include "../resource/Assets/laxend.hpp"
#include "../resource/Assets/logosmall.hpp"
#include "Logo.hpp"

namespace FrameWork
{
	void Assets::Initialize(ID3D11Device* Device)
	{
		ImGuiIO& io = ImGui::GetIO();
		ImFontConfig cfg;


		lexendfont = io.Fonts->AddFontFromMemoryTTF(lexendfontona, sizeof(lexendfontona), 18.0f, NULL, io.Fonts->GetGlyphRangesCyrillic());
		InterRegular = io.Fonts->AddFontFromMemoryCompressedTTF(InterRegular_compressed_data, InterRegular_compressed_size, 16);
		Verdana = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\Verdana.ttf", 16, &cfg, io.Fonts->GetGlyphRangesCyrillic());
		VerdanaBig = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\Verdana.ttf", 20, &cfg, io.Fonts->GetGlyphRangesCyrillic());
		VerdanaSmall = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\Verdana.ttf", 14, &cfg, io.Fonts->GetGlyphRangesCyrillic());
		InterBlack = io.Fonts->AddFontFromMemoryCompressedTTF(InterBlack_compressed_data, InterBlack_compressed_size, 14);
		InterBlackBig = io.Fonts->AddFontFromMemoryCompressedTTF(InterBlack_compressed_data, InterBlack_compressed_size, 28);  // Nova fonte grande
		InterBold = io.Fonts->AddFontFromMemoryCompressedTTF(InterBold_compressed_data, InterBold_compressed_size, 16);
		InterBoldBig = io.Fonts->AddFontFromMemoryCompressedTTF(InterBold_compressed_data, InterBold_compressed_size, 30);
		InterBold12 = io.Fonts->AddFontFromMemoryCompressedTTF(InterBold_compressed_data, InterBold_compressed_size, 12);
		InterExtraBold = io.Fonts->AddFontFromMemoryCompressedTTF(InterExtraBold_compressed_data, InterExtraBold_compressed_size, 14);
		InterExtraLight = io.Fonts->AddFontFromMemoryCompressedTTF(InterExtraLight_compressed_data, InterExtraLight_compressed_size, 14);
		InterLight = io.Fonts->AddFontFromMemoryCompressedTTF(InterLight_compressed_data, InterLight_compressed_size, 12);
		InterMedium = io.Fonts->AddFontFromMemoryCompressedTTF(InterMedium_compressed_data, InterMedium_compressed_size, 16);
		InterMediumSmall = io.Fonts->AddFontFromMemoryCompressedTTF(InterMedium_compressed_data, InterMedium_compressed_size, 13);
		InterMediumBig = io.Fonts->AddFontFromMemoryCompressedTTF(InterMedium_compressed_data, InterMedium_compressed_size, 18);
		InterRegular14 = io.Fonts->AddFontFromMemoryCompressedTTF(InterRegular_compressed_data, InterRegular_compressed_size, 14);
		InterSemiBold = io.Fonts->AddFontFromMemoryCompressedTTF(InterSemiBold_compressed_data, InterSemiBold_compressed_size, 14);
		InterThin = io.Fonts->AddFontFromMemoryCompressedTTF(InterThin_compressed_data, InterThin_compressed_size, 14);

		static const ImWchar IconRanges[] =
		{
			ICON_MIN_FA, ICON_MAX_FA, 0
		};

		ImFontConfig FontAwesomeConfig;
		FontAwesomeConfig.GlyphMinAdvanceX = 25.f * (2.0f / 3.0f);

		FontAwesomeRegular = io.Fonts->AddFontFromMemoryCompressedTTF(FontAwesomeRegular_compressed_data, FontAwesomeRegular_compressed_size, 25.f * (2.0f / 3.0f), &FontAwesomeConfig, &IconRanges[0]);
		FontAwesomeSolid = ImGui::GetIO().Fonts->AddFontFromMemoryCompressedTTF(FontAwesomeSolid_compressed_data, FontAwesomeSolid_compressed_size, 25.f * (2.0f / 3.0f), &FontAwesomeConfig, &IconRanges[0]);
		FontAwesomeSolid18 = ImGui::GetIO().Fonts->AddFontFromMemoryCompressedTTF(FontAwesomeSolid_compressed_data, FontAwesomeSolid_compressed_size, 18.f * (2.0f / 3.0f), &FontAwesomeConfig, &IconRanges[0]);
		FontAwesomeSolidBig = io.Fonts->AddFontFromMemoryCompressedTTF(FontAwesomeSolid_compressed_data, FontAwesomeSolid_compressed_size, 30.f * (2.0f / 3.0f), &FontAwesomeConfig, &IconRanges[0]);
		
		D3DX11CreateShaderResourceViewFromMemory(Device, LogoMenuRawBytes, sizeof(LogoMenuRawBytes), NULL, NULL, &BannerFreeFire, NULL); 
		D3DX11CreateShaderResourceViewFromMemory(Device, nicolasnetoo, sizeof(nicolasnetoo), NULL, NULL, &LogoFreeFire, NULL); 
		D3DX11CreateShaderResourceViewFromMemory(Device, LogoBytes, sizeof(LogoBytes), NULL, NULL, &UserLogo, NULL); 
		D3DX11CreateShaderResourceViewFromMemory(Device, logosmall, sizeof(logosmall), NULL, NULL, &LogoZero, NULL); 


	}
}