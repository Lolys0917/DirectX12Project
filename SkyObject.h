#pragma once

#include "Object.h"

namespace Engine
{
	class SkyObject : protected Object
	{
	public:
		void Init();
		void Update();
		void Draw();
		void Release();
	private:

	}
}