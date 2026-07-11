#pragma once
#include "Util.h"
#include <unordered_map>
#include <string>

namespace Engine
{
	inline int ComponentID;		//統括ID
	inline std::unordered_map<std::string, int> ComponentTagMap;	//タグ→int
	inline std::unordered_map<std::string, int> ComponentLayerMap;	//レイヤー→int

	class Component
	{
	protected:
		class Object* object = nullptr;
	public:
		Component() = delete;

		virtual ~Component();
		virtual void Init() {}
		virtual void Update() {}
		virtual void Draw() {}
		virtual void End() {}

		//Set関数.................................
		void SetPosition(Float3 pos)
		{
			Position = pos;
		}
		void SetSize(Float3 size)
		{
			Size = size;
		}
		void SetAngle(Float3 angle)
		{
			Angle = angle;
		}
		void SetID(int id)
		{
			ComponentID = id;
		}
		void SetActive(bool active)
		{
			Active = active;
		}
		void SetTagName(std::string tagName)
		{
			TagName = tagName;
		}
		void SetLayerName(std::string layerName)
		{
			LayerName = layerName;
		}
		//Get関数..................................
		Float3 GetPosition() { return Position; }
		Float3 GetSize() { return Size; }
		Float3 GetAngle() { return Angle; }
		int GetID() { return ComponentID; }
		int GetTag() { return ComponentTagMap[TagName]; }
		int GetLeyer() { return ComponentLayerMap[LayerName]; }
	private:
		bool Active;	//使用未使用設定
		std::string TagName;		//タグ判定
		std::string LayerName;		//レイヤー判定
		Float3 Position;		//座標　※１
		Float3 Size;			//大きさ　※１
		Float3 Angle;			//角度　※１
	};
}
//※１(2D時 x,y判定)