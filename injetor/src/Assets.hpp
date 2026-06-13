#pragma once

#include "../resource/FrameWork.hpp"
#include <../resource/incluides/ImGui/imgui.h>

#include "../resource/assets/FontInter.hpp"
#include "../resource/assets/FontAwesome.hpp"

#include <d3d11.h>
#include <d3dx11.h>

namespace FrameWork
{
	namespace Assets
	{
		inline ImFont* FontAwesomeRegular = nullptr;
		inline ImFont* FontAwesomeSolid = nullptr;
		inline ImFont* FontAwesomeSolid18 = nullptr;
		inline ImFont* FontAwesomeSolidBig = nullptr;

		inline ImFont* InterExtraLight = nullptr;
		inline ImFont* InterLight = nullptr;
		inline ImFont* InterThin = nullptr;
		inline ImFont* InterRegular = nullptr;
		inline ImFont* InterRegular14 = nullptr;
		inline ImFont* InterMedium = nullptr;
		inline ImFont* InterMediumBig = nullptr;
		inline ImFont* InterMediumSmall = nullptr;
		inline ImFont* InterSemiBold = nullptr;
		inline ImFont* InterBold = nullptr;
		inline ImFont* InterBoldBig = nullptr;
		inline ImFont* InterBold12 = nullptr;
		inline ImFont* InterExtraBold = nullptr;
		inline ImFont* InterBlack = nullptr;
		inline ImFont* InterBlackBig = nullptr;                          // Nova fonte InterBlack grande
		inline ImFont* Verdana = nullptr;
		inline ImFont* VerdanaBig = nullptr;
		inline ImFont* VerdanaSmall = nullptr;

		inline ImFont* lexendfont = nullptr;


		inline ID3D11ShaderResourceView* BannerFreeFire;
		inline ID3D11ShaderResourceView* LogoFreeFire;
		inline ID3D11ShaderResourceView* UserLogo;
		inline ID3D11ShaderResourceView* LogoZero;


		void Initialize(ID3D11Device* Device);
	}
}