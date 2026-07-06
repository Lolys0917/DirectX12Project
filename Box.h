#pragma once
#include "Object.h"

namespace Engine
{
    class Box : public Object
    {
    public:
        Box();
        virtual ~Box();

        void SetSize(float width, float height, float depth);

    protected:
        virtual void BuildMesh() override;

    private:
        float m_Width;
        float m_Height;
        float m_Depth;
    };
}