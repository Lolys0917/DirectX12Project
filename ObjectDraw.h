#pragma once

#include <vector>
#include <cmath>

//クラスを一次元として
//その中の固有名(インデックス)を二次元としてとらえる

enum class ObjectType
{
	None,
	Polygon,
};
struct PolygonState
{
	float x, y, z;
	float r, g, b, a;
};

class ObjectDraw
{
private:
	std::vector<const char*> vcName;
	std::vector<ObjectType> vOtType;
	std::vector<int> sides;
	std::vector<float> size;

	int NameToIndex(const char* name);
public:
	void AddObject(const char* name, ObjectType Ot);
	bool SetType(ObjectType Ot);
	void AllDraw();
	std::vector<PolygonState> CreatePolygon(const char* name);
};

#include <vector>
#include <cmath>

std::vector<PolygonState> CreatePolygon(int sides, float radius)
{
	std::vector<PolygonState> vertices;

	// 中心点
	vertices.push_back({ 0.0f, 0.0f, 0.0f, 1,1,1,1 });

	for (int i = 0; i <= sides; i++) // 最後は閉じるため+1
	{
		float angle = (float)i / sides * 2.0f * 3.141592f;

		float x = cosf(angle) * radius;
		float y = sinf(angle) * radius;

		vertices.push_back({ x, y, 0.0f, 0,1,0,1 });
	}

	return vertices;
}

