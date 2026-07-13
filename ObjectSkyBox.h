#pragma once
#include "Object.h"

namespace Engine
{
	enum class SkyBoxType
	{
		Model, //3D Model
		Plane, //•½–ÊTexture
		Cube, //—§•û‘ÌTexture
		Sphere, //‹…–ÊTexture
		Count //SkyBoxType‚Ì”
	};

	class ObjectSkyBox final : public Object
	{
	public:
		//–¢“o˜^ó‘Ô‚ÌSkyBox Object‚ğì¬‚·‚é
		//ObjectSkyBox(SkyBoxType type, std::string texturePath);
		//SkyBox Object‚ğ”jŠü‚·‚é
		~ObjectSkyBox() override;
		//–¢“o˜^ó‘Ô‚ÌSkyBox’è‹`‚ğ•¡»‚·‚é
		//–ß‚è’l: GPU Resource‚ğ‚½‚È‚¢•¡»Object
		std::unique_ptr<Object> Clone() const override;

	private:
		std::string TexturePath; //SkyBox‚Ég—p‚·‚éTexture‚ÌƒpƒX
		SkyBoxType Type; //SkyBox‚Ìí—Ş
	};
}