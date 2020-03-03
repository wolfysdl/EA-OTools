#pragma once
#include "shaders.h"

class Target abstract {
public:
    virtual unsigned int GetMaxBoneWeightsPerVertex() = 0;
    virtual unsigned int GetMaxVertexWeightsPerMesh() = 0;
    virtual Shader *Shaders() = 0;
    virtual unsigned int NumShaders() = 0;
    virtual Shader *FindShader(std::string const &name);
};

class FifaTarget abstract : public Target {
    unsigned int GetMaxBoneWeightsPerVertex();
    unsigned int GetMaxVertexWeightsPerMesh();
};

class TargetFIFA07 : public FifaTarget {
    virtual Shader *Shaders();
    virtual unsigned int NumShaders();
};

class TargetFM13 : public FifaTarget {
    Shader *Shaders();
    unsigned int NumShaders();
};
