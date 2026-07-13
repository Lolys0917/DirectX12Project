//#include "ObjectSkyBox.h"
//#include "Component.h"
//#include "Model.h"
//
//namespace Engine
//{
//	//ObjectSkyBox::ObjectSkyBox(SkyBoxType type, std::string texturePath)
//	//	: Object(ObjectType::SkyBox)
//	//{
//	//	TexturePath = std::move(texturePath);
//	//	Type = type;
//	//	switch (Type)
//	//	{
//	//	case Engine::SkyBoxType::Model:
//	//		break;
//	//	case Engine::SkyBoxType::Plane:
//	//		break;
//	//	case Engine::SkyBoxType::Cube:
//	//		break;
//	//	case Engine::SkyBoxType::Sphere:
//	//		break;
//	//	case Engine::SkyBoxType::Count:
//	//		break;
//	//	default:
//	//		break;
//	//	}
//	//}
//	ObjectSkyBox::~ObjectSkyBox() = default;
//	std::unique_ptr<Object> ObjectSkyBox::Clone() const
//	{
//		return std::make_unique<ObjectSkyBox>(*this);
//	}
//}