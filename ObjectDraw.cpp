#include "ObjectDraw.h"

#include <windows.h>
#include <wrl.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <cassert>

#include "d3dx12.h"

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")


int ObjectDraw::NameToIndex(const char* name)
{
	for (int i = 0; i < vcName.size(); i++)
	{
		if (name == vcName[i])
		{
			return i;
		}
	}
	MessageBoxA(NULL, "指定された名前が見つかりません", "Error", MB_OK);
	return -1;
}

void ObjectDraw::AddObject(const char* name, ObjectType Ot)
{
	vcName.push_back(name);
	vOtType.push_back(Ot);

	sides.push_back(3);
	size.push_back(1.0f);
}

std::vector<Vertex> ObjectDraw::CreatePolygon(const char* name)
{
	std::vector<Vertex> vertices;

	int Idx = NameToIndex(name);

	// 中心点
	vertices.push_back({ 0.0f, 0.0f, 0.0f, 1,1,1,1 });

	for (int i = 0; i <= sides[Idx]; i++) // 最後は閉じるため+1
	{
		float angle = (float)i / sides[Idx] * 2.0f * 3.141592f;

		float x = cosf(angle) * size[Idx];
		float y = sinf(angle) * size[Idx];

		vertices.push_back({ x, y, 0.0f, 0,1,0,1 });
	}

	return vertices;
}