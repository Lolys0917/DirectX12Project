#pragma once
#include "Object.h"

namespace Engine
{
    class Sphere : public Object
    {
    public:
        Sphere();
        virtual ~Sphere();

        void SetRadius(float radius);
        void SetDivision(uint32_t slice, uint32_t stack);

    protected:
        virtual void BuildMesh() override;

    private:
        float m_Radius;

        uint32_t m_Slice;
        uint32_t m_Stack;
    };
}