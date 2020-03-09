#pragma once
#include "shaders.h"

struct MaterialProperties {
    bool isTextured = false;
    bool isAdboard = false;
    bool isHomeCrowd = false;
    bool isAwayCrowd = false;
    bool isSkinned = false;
    bool isTransparent = false;
    bool isLit = false;
    bool isUnlit = false;
    bool isReflective = false;
    bool isShiny = false;
    bool isMetallic = false;
    bool isAdditive = false;
};

class Target abstract {
public:
    virtual unsigned int GetMaxBoneWeightsPerVertex();
    virtual unsigned int GetMaxVertexWeightsPerMesh();
    virtual char const *Name() = 0;
    virtual int Version() = 0;
    virtual Shader *Shaders() = 0;
    virtual unsigned int NumShaders() = 0;
    virtual Shader *DecideShader(MaterialProperties const &properties) = 0;
    virtual Shader *FindShader(std::string const &name);
};

class TargetFIFA03 : public Target {
    char const *Name();
    int Version();
    Shader *Shaders();
    unsigned int NumShaders();
    Shader *DecideShader(MaterialProperties const &properties);
};

Shader *DecideShader_FIFA_04_05(Target *target, MaterialProperties const &properties);

class TargetFIFA04 : public Target {
    char const *Name();
    int Version();
    Shader *Shaders();
    unsigned int NumShaders();
    Shader *DecideShader(MaterialProperties const &properties);
};

class TargetFIFA05 : public Target {
    char const *Name();
    int Version();
    Shader *Shaders();
    unsigned int NumShaders();
    Shader *DecideShader(MaterialProperties const &properties);
};

Shader *DecideShader_FIFA_06_09(Target *target, MaterialProperties const &properties);

class TargetFIFA06 : public Target {
    char const *Name();
    int Version();
    Shader *Shaders();
    unsigned int NumShaders();
    Shader *DecideShader(MaterialProperties const &properties);
};

class TargetFIFA07 : public Target {
    char const *Name();
    int Version();
    Shader *Shaders();
    unsigned int NumShaders();
    Shader *DecideShader(MaterialProperties const &properties);
};

class TargetFIFA08 : public Target {
    char const *Name();
    int Version();
    Shader *Shaders();
    unsigned int NumShaders();
    Shader *DecideShader(MaterialProperties const &properties);
};

class TargetFIFA09 : public Target {
    char const *Name();
    int Version();
    Shader *Shaders();
    unsigned int NumShaders();
    Shader *DecideShader(MaterialProperties const &properties);
};

class TargetFIFA10 : public Target {
    char const *Name();
    int Version();
    Shader *Shaders();
    unsigned int NumShaders();
    Shader *DecideShader(MaterialProperties const &properties);
};

class TargetEURO04 : public Target {
    char const *Name();
    int Version();
    Shader *Shaders();
    unsigned int NumShaders();
    Shader *DecideShader(MaterialProperties const &properties);
};

class TargetEURO08 : public Target {
    char const *Name();
    int Version();
    Shader *Shaders();
    unsigned int NumShaders();
    Shader *DecideShader(MaterialProperties const &properties);
};

class TargetWC06 : public Target {
    char const *Name();
    int Version();
    Shader *Shaders();
    unsigned int NumShaders();
    Shader *DecideShader(MaterialProperties const &properties);
};

class TargetCL0405 : public Target {
    char const *Name();
    int Version();
    Shader *Shaders();
    unsigned int NumShaders();
    Shader *DecideShader(MaterialProperties const &properties);
};

class TargetCL0607 : public Target {
    char const *Name();
    int Version();
    Shader *Shaders();
    unsigned int NumShaders();
    Shader *DecideShader(MaterialProperties const &properties);
};

class TargetFM13 : public Target {
    char const *Name();
    int Version();
    Shader *Shaders();
    unsigned int NumShaders();
    Shader *DecideShader(MaterialProperties const &properties);
};

class TargetCRICKET07 : public Target {
    char const *Name();
    int Version();
    Shader *Shaders();
    unsigned int NumShaders();
    Shader *DecideShader(MaterialProperties const &properties);
};
