#pragma once
#include "material.h"

class CloudMaterial : public Material
{
public:
    CloudMaterial();
    ~CloudMaterial();

public:
    // 这里可以添加更多控制参数
    float mDensityThreshold{ 0.4f };
    float mAbsorption{ 0.5f };
};