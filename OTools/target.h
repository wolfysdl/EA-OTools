#pragma once
#include "shaders.h"

class Target {
public:
    virtual unsigned int GetMaxBoneWeightsPerVertex();
    virtual unsigned int GetMaxVertexWeightsPerMesh();
};

class DefaultTarget : public Target {

};

class TargetFIFA09 : public Target {

};
