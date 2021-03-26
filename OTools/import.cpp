#include "d3d9.h"
#include "main.h"
#include <assimp\Importer.hpp>
#include <assimp\scene.h>
#include <assimp\postprocess.h>
#include <assimp\pbrmaterial.h>
#include <fstream>
#include <sstream>
#include <iostream>
#include <cassert>
#include <cstdlib>
#include "binbuf.h"
#include "shaders.h"
#include "NvTriStrip/NvTriStrip.h"
#include "Fsh\Fsh.h"
#include "FifamReadWrite.h"
#include "srgb/SrgbTransform.hpp"

struct Vector4D {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float w = 0.0f;
};

struct Vector4D_int {
    int x = 0;
    int y = 0;
    int z = 0;
    int w = 0;
};

struct Matrix4x4 {
    Vector4D a, b, c, d;
};

enum AlphaMode {
    ALPHA_NOT_SET,
    ALPHA_OPAQUE,
    ALPHA_MASK,
    ALPHA_BLEND
};

struct Symbol {
    string name;
    int offset;

    Symbol(string const &_name, int _offset = -1) {
        name = _name;
        offset = _offset;
    }
};

struct GlobalArg {
    unsigned int offset;
    unsigned int count;
    string name;

    GlobalArg(unsigned int _offset, unsigned int _count = 1) {
        offset = _offset;
        count = _count;
    }

    GlobalArg(string const &_name, unsigned int _count = 1) {
        name = _name;
        offset = 0;
        count = _count;
    }
};

struct CollTriangle {
    unsigned short normal;
    unsigned short vertPos[3];
};

struct CollLine {
    unsigned short vertPos[2];
};

void GetMatColorAndAlphaProperties(aiMaterial *mat, aiColor3D &matColor, float &alpha, AlphaMode &alphaMode, int &blendFunc) {
    matColor.r = 255;
    matColor.g = 255;
    matColor.b = 255;
    mat->Get(AI_MATKEY_COLOR_DIFFUSE, matColor);
    alpha = 1.0f;
    mat->Get(AI_MATKEY_OPACITY, alpha);
    blendFunc = 0;
    mat->Get(AI_MATKEY_BLEND_FUNC, blendFunc);
    aiString alphaModeAiStr;
    mat->Get(AI_MATKEY_GLTF_ALPHAMODE, alphaModeAiStr);
    string alphaModeStr = alphaModeAiStr.C_Str();
    alphaMode = ALPHA_NOT_SET;
    if (!alphaModeStr.empty()) {
        if (alphaModeStr == "OPAQUE")
            alphaMode = ALPHA_OPAQUE;
        else if (alphaModeStr == "MASK")
            alphaMode = ALPHA_MASK;
        else if (alphaModeStr == "BLEND")
            alphaMode = ALPHA_BLEND;
    }
}

struct Node {
    string name;
    aiNode *node = nullptr;
    vector<unsigned int> renderDescriptorsOffsets;
    aiVector3D boundMin = { 0.0f, 0.0f, 0.0f };
    aiVector3D boundMax = { 0.0f, 0.0f, 0.0f };
    bool anyVertexProcessed = false;
    static aiScene const *scene;

    Node(aiNode *_node) {
        node = _node;
        name = _node->mName.C_Str();
    }

    static bool SortByName(Node const &a, Node const &b) {
        return a.name <= b.name;
    }

    bool HasTransparency() const {
        bool result = false;
        for (unsigned int i = 0; i < node->mNumMeshes; i++) {
            aiMesh *mesh = scene->mMeshes[node->mMeshes[i]];
            aiMaterial *mat = scene->mMaterials[mesh->mMaterialIndex];
            aiColor3D matColor(255, 255, 255);
            float matAlpha = 1;
            AlphaMode alphaMode = ALPHA_NOT_SET;
            int blendFunc = 0;
            GetMatColorAndAlphaProperties(mat, matColor, matAlpha, alphaMode, blendFunc);
            if (matAlpha != 1 || alphaMode == ALPHA_BLEND || blendFunc == 1)
                return true;
        }
        return false;
    }

    static bool SortByAlpha(Node const &a, Node const &b) {
        return a.HasTransparency() <= b.HasTransparency();
    }

    static bool SortByNameAndAlpha(Node const &a, Node const &b) {
        bool atp = a.HasTransparency();
        bool btp = b.HasTransparency();
        if (atp == btp)
            return SortByName(a, b);
        return atp <= btp;
    }
};

aiScene const *Node::scene = nullptr;

struct Tex {
    enum Mode {
        Wrap = 1,
        Mirror = 2,
        Clamp = 3,
        Border = 4,
        ClampToEdge = 5
    };
    enum Filter {
        Point = 1,
        Bilinear = 2,
        Anisotropic = 3,
        GaussianCubic = 5
    };
    enum MipMapMode {
        Off = 0,
        Nearest = 1,
        Linear = 2
    };
    string name;
    Mode uAddressing = Wrap;
    Mode vAddressing = Mirror;
    unsigned char anisotropy = 1;
    Filter filtering = Bilinear;
    float mipMapLodBias = 0.0f;
    MipMapMode mipMapMode = Linear;
    unsigned int offset = 0;
    bool isGlobal = false;
    bool runtimeConstructed = true;
    string filepath;
    TexEmbedded embedded;

    Tex() {
        name = "----";
        uAddressing = Wrap;
        vAddressing = Mirror;
        filtering = Bilinear;
        mipMapMode = Linear;
        mipMapLodBias = 0.0f;
        anisotropy = 1;
        isGlobal = false;
        embedded.data = nullptr;
        embedded.width = 0;
        embedded.height = 0;
    }

    Tex(string const &_name, Mode _u, Mode _v, Filter _filtering, MipMapMode _mipMapMode, float _mipMapLodBias = 0.0f, unsigned char _anisotropy = 1, unsigned int _offset = 0, TexEmbedded const &_embedded = TexEmbedded()) {
        name = _name;
        uAddressing = _u;
        vAddressing = _v;
        filtering = _filtering;
        mipMapMode = _mipMapMode;
        mipMapLodBias = _mipMapLodBias;
        anisotropy = _anisotropy;
        offset = _offset;
        isGlobal = false;
        embedded = _embedded;
    }

    bool IsRuntimeConstructed() const {
        return runtimeConstructed;
    }

    bool IsFIFAAdboardsTexture() const {
        string ln = ToLower(name);
        return ln == "adba" || ln == "adbb" || ln == "adbc";
    }

    bool IsFIFACrowdHomeTexture() const {
        string ln = ToLower(name);
        return ln == "chf0" || ln == "chf1" || ln == "chf2" || ln == "chf3";
    }

    bool IsFIFACrowdAwayTexture() const {
        string ln = ToLower(name);
        return ln == "caf0" || ln == "caf1" || ln == "caf2" || ln == "caf3";
    }

    void SetUVAddressingMode(aiTextureMapMode _mode) {
        Mode m = Wrap;
        switch (_mode) {
        case aiTextureMapMode_Wrap:
            m = Wrap;
            break;
        case aiTextureMapMode_Clamp:
            m = Clamp;
            break;
        case aiTextureMapMode_Mirror:
            m = Mirror;
            break;
        case aiTextureMapMode_Decal:
            m = Border;
            break;
        }
        uAddressing = m;
        vAddressing = m;
    }

    static string GetModeName(Mode m) {
        switch (m) {
        case Wrap:
            return "EAGL::PCCM_WRAP";
        case Mirror:
            return "EAGL::PCCM_MIRROR";
        case Clamp:
            return "EAGL::PCCM_CLAMP";
        case Border:
            return "EAGL::PCCM_BORDER";
        case ClampToEdge:
            return "PCCM_CLAMPTOEDGE";
        }
        return "";
    }

    static string GetFilteringName(Filter f) {
        switch (f) {
        case Point:
            return "EAGL::FM_POINT";
        case Bilinear:
            return "EAGL::FM_BILINEAR";
        case Anisotropic:
            return "EAGL::FM_ANISOTROPIC";
        case GaussianCubic:
            return "EAGL::FM_GAUSSIANCUBIC";
        }
        return "";
    }

    static string GetMipMapModeName(MipMapMode m) {
        switch (m) {
        case Off:
            return "EAGL::MMM_OFF";
        case Nearest:
            return "EAGL::MMM_NEAREST";
        case Linear:
            return "EAGL::MMM_LINEAR";
        }
        return "";
    }

    string GetRuntimeConstructorLine(unsigned int uid, unsigned int numVariations = 1) const {
        string line = "__EAGL::TAR:::RUNTIME_ALLOC::";
        vector<string> components;
        components.push_back("UID=" + to_string(uid));
        components.push_back("SHAPENAME=" + name + "," + to_string(numVariations));
        components.push_back("PCEXTOBJ_SetClampU=" + GetModeName(uAddressing));
        components.push_back("PCEXTOBJ_SetClampV=" + GetModeName(vAddressing));
        if (filtering != Bilinear) {
            components.push_back("SetFilterMode=" + GetFilteringName(filtering));
            if (filtering == Anisotropic && anisotropy != 1)
                components.push_back("PCEXTOBJ_SetAnisotropy=" + to_string(anisotropy));
        }
        if (mipMapMode != Linear)
            components.push_back("SetMIPMAPMode=" + GetMipMapModeName(mipMapMode));
        if (mipMapLodBias != 0.0f)
            components.push_back("SetMIPMAPLODBias=" + Format("%g", mipMapLodBias));
        bool first = true;
        for (auto const &c : components) {
            if (first)
                first = false;
            else
                line += ";";
            line += c;
        }
        return line;
    }
};

struct ModData {
    string name;
    unsigned int offset = 0;
    vector<unsigned char> data;
    string runtimeConstructorLine;
    unsigned int runtimeSize = 0;

    ModData(string const &_name, unsigned int _offset, vector<unsigned char> const &_data) {
        name = _name;
        offset = _offset;
        data = _data;
    }

    ModData(string const &_name, string const &_runtimeConstructorLine, unsigned int _runtimeSize) {
        name = _name;
        offset = 0;
        runtimeConstructorLine = _runtimeConstructorLine;
        runtimeSize = _runtimeSize;
    }

    ModData() {
        offset = 0;
    }
};

struct Modifiables {
    vector<ModData> vec;

    GlobalArg GetArg(string const &name, string const &_runtimeConstructorLine, unsigned int _runtimeSize) {
        bool isPresent = false;
        for (auto const &m : vec) {
            if (m.name == name && m.runtimeConstructorLine == _runtimeConstructorLine && m.runtimeSize == _runtimeSize) {
                isPresent = true;
                break;
            }
        }
        if (!isPresent)
            vec.emplace_back(name, _runtimeConstructorLine, _runtimeSize);
        return GlobalArg(_runtimeConstructorLine);
    }

    template<typename T>
    GlobalArg GetArg(string const &name, BinaryBuffer &buf, T const &obj, bool putZero = false, bool align = false) {
        vector<unsigned char> newVec(sizeof(T));
        Memory_Copy(&newVec[0], &obj, sizeof(T));
        bool isPresent = false;
        unsigned int offset = 0;
        for (auto const &m : vec) {
            if (m.name == name && m.data == newVec) {
                isPresent = true;
                offset = m.offset;
                break;
            }
        }
        if (!isPresent) {
            if (align)
                buf.Align(16);
            offset = buf.Position();
            vec.emplace_back(name, buf.Position(), newVec);
            buf.Put(obj);
            if (putZero)
                buf.Put(int(0));
        }
        return GlobalArg(offset);
    }
};

struct Coordiante4 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float w = 0.0f;
};

struct ComputationIndex {
    unsigned short activeTechnique = 0;
    unsigned short unknown1 = 2;
};

struct TAR {
    unsigned int unknown1 = 0;
    char tag[4] = { 0, 0, 0, 0 };
    unsigned int unknown2 = 0;
    float unknown3 = 1.0f;
    unsigned int unknown4 = 0;
    float unknown5 = 1.0f;
    unsigned int unknown6 = 0;
    unsigned int wrapU = 1;
    unsigned int wrapV = 1;
    unsigned int wrapW = 1;
    unsigned int unknown7 = 0;
    unsigned int unknown8 = 0;
};

struct GeoPrimState {
    unsigned int nPrimitiveType = 5;
    unsigned int nShading = 1;
    unsigned int bCullEnable = 0;
    unsigned int nCullDirection = 1;
    unsigned int nDepthTestMetod = 4;
    unsigned int nAlphaBlendMode = 1;
    unsigned int bAlphaTestEnable = 1;
    unsigned int nAlphaCompareValue = 16;
    unsigned int nAlphaTestMethod = 5;
    unsigned int bTextureEnable = 1;
    unsigned int nTransparencyMethod = 1;
    unsigned int nFillMode = 3;
    unsigned int nBlendOperation = 1;
    unsigned int nSrcBlend = 5;
    unsigned int nDstBlend = 6;
    float fNumPatchSegments = 1.0f;
    int nZWritesEnable = -1;
};

struct Light {
    aiMatrix4x4 unknown1;
    Vector4D unknown2[3];

    Light() {
        unknown1 = { 0.7071f, 0, 0, 0,  0, 0.7071f, 0, 0,  0, 0, 0.7071f, 0,  0, 0, 0, 0 };
        unknown2[0] = { 0.0f, 0.0f, 0.0f, 0.0f };
        unknown2[1] = { 0.0f, 0.0f, 0.0f, 0.0f };
        unknown2[2] = { 0.5f, 0.5f, 0.5f, 1.0f };
    }
};

struct IrradLight {
    Vector4D unknown1[10];

    IrradLight() {
        for (auto &v : unknown1)
            v = { 0.5f, 0.5f, 0.5f, 0.0f };
    }
};

struct VertexBoneInfo {
    float weight;
    unsigned char boneIndex;
};

struct VertexWeightInfo {
    vector<VertexBoneInfo> bones;
};

union VertexBoneInfoLayout {
    float fValue;
    unsigned int uiValue;
    unsigned char ucValue;
};

struct VertexWeightInfoLayout {
    VertexBoneInfoLayout bones[3]; // TODO: rework this
    unsigned int numBones;

    VertexWeightInfoLayout() {
        Memory_Zero(*this);
    }

    VertexWeightInfoLayout(VertexWeightInfo const &w) {
        Memory_Zero(*this);
        if (w.bones.size() > 3)
            throw runtime_error("more than 3 bones in VertexWeightInfo (VertexWeightInfoLayout(VertexWeightInfo const &w))");
        numBones = w.bones.size();
        for (unsigned int wb = 0; wb < w.bones.size(); wb++) {
            bones[wb].fValue = w.bones[wb].weight;
            bones[wb].ucValue = w.bones[wb].boneIndex;
        }       
    }

    VertexWeightInfoLayout(VertexWeightInfoLayout const &w) {
        Memory_Copy(this, &w, sizeof(VertexWeightInfoLayout));
    }

    VertexWeightInfoLayout &operator=(VertexWeightInfoLayout const &w) {
        Memory_Copy(this, &w, sizeof(VertexWeightInfoLayout));
        return *this;
    }
};

bool operator<(VertexBoneInfo const &a, VertexBoneInfo const &b) {
    return a.weight > b.weight;
}

bool operator>(VertexBoneInfo const &a, VertexBoneInfo const &b) {
    return a.weight < b.weight;
}

bool operator<(VertexBoneInfoLayout const &a, VertexBoneInfoLayout const &b) {
    return a.uiValue > b.uiValue;
}

bool operator>(VertexBoneInfoLayout const &a, VertexBoneInfoLayout const &b) {
    return a.uiValue < b.uiValue;
}

bool operator<(VertexWeightInfoLayout const &a, VertexWeightInfoLayout const &b) {
    if (a.numBones > b.numBones)
        return true;
    if (b.numBones > a.numBones)
        return false;
    for (unsigned i = 0; i < 3; i++) {
        if (a.bones[i] > b.bones[i])
            return true;
        if (b.bones[i] > a.bones[i])
            return false;
    }
    return false;
}

struct BoneInfo {
    unsigned char index = 0;
    aiBone *bone = nullptr;
    string name;

    BoneInfo() {}

    BoneInfo(unsigned char _index, aiBone *_bone, string _name) {
        index = _index;
        bone = _bone;
        name = _name;
    }
};

struct MeshInfo {
    map<VertexWeightInfoLayout, vector<unsigned int>> weightsMap;
    map<unsigned int, unsigned int> verticesMap; // original vertex index > new vertex index
    unsigned int startFace = 0;
    unsigned int numFaces = 0;
};

unsigned int FshHash (string const &name) {
    unsigned int hash = 0;
    for (unsigned char c : name) {
        if (c < 'A' || c > 'Z')
            c = c - 97;
        else
            c = c - 65;
        hash = c + 32 * hash;
    }
    return hash;
};

map<string, string> GetNameOptions(string const &name, bool isMaterial = false) {
    map<string, string> result;
    auto be = name.find('[');
    if (be != string::npos) {
        auto en = name.find(']', be + 1);
        if (en != string::npos) {
            auto nameOptions = Split(name.substr(be + 1, en - be - 1), ',', true, true);
            for (unsigned int i = 0; i < nameOptions.size(); i++) {
                string optionName, optionValue;
                auto dd = nameOptions[i].find(':');
                if (dd != string::npos) {
                    optionName = ToLower(nameOptions[i].substr(0, dd));
                    optionValue = nameOptions[i].substr(dd + 1);
                }
                else {
                    if (isMaterial && i == 0) {
                        optionName = "shader";
                        optionValue = nameOptions[i];
                    }
                    else
                        optionName = nameOptions[i];
                }
                if (!result.contains(optionName))
                    result[optionName] = optionValue;
            }
        }
    }
    return result;
}

bool ShouldIgnoreThisNode(aiNode *node) {
    auto nameOptions = GetNameOptions(node->mName.C_Str());
    return nameOptions.contains("ignore");
}

bool IsSkeletonNode(aiNode *node) {
    if (!ShouldIgnoreThisNode(node)) {
        string nodeName = ToLower(node->mName.C_Str());
        if (nodeName.starts_with("skeleton")) {
            auto c = nodeName.c_str()[8];
            return c == '\0' || c == '.' || c == '_'; // 'Skeleton', 'Skeleton.001', 'Skeleton_001'
        }
    }
    return false;
}

bool IsFlagsNode(aiNode *node) {
    if (!ShouldIgnoreThisNode(node)) {
        string nodeName = ToLower(node->mName.C_Str());
        if (nodeName.starts_with("flags")) {
            auto c = nodeName.c_str()[5];
            return c == '\0' || c == '.' || c == '_'; // 'Flags', 'Flags.001', 'Flags_001'
        }
    }
    return false;
}

bool IsEffectsNode(aiNode *node) {
    if (!ShouldIgnoreThisNode(node)) {
        string nodeName = ToLower(node->mName.C_Str());
        if (nodeName.starts_with("effects")) {
            auto c = nodeName.c_str()[7];
            return c == '\0' || c == '.' || c == '_'; // 'Effects', 'Effects.001', 'Effects_001'
        }
    }
    return false;
}

bool IsCollisionNode(aiNode *node) {
    if (!ShouldIgnoreThisNode(node)) {
        string nodeName = ToLower(node->mName.C_Str());
        if (nodeName.starts_with("collision")) {
            auto c = nodeName.c_str()[9];
            return c == '\0' || c == '.' || c == '_'; // 'Collision', 'Collision.001', 'Collision_001'
        }
    }
    return false;
}

struct StadiumExtra {
    bool used = false;
    unsigned int stadiumId = 0;
    unsigned int lightingId = 0;
    aiNode *flags = nullptr;
    aiNode *effects = nullptr;
    aiNode *collision = nullptr;
    enum FIFA_STAD_TYPE { STAD_DEFAULT, STAD_CUSTOM } stadType = STAD_DEFAULT;
};

void NodeAddCallback(aiNode *node, vector<Node> &nodes, StadiumExtra &stadExtra) {
    if (!ShouldIgnoreThisNode(node)) {
        bool isExtra = false;
        if (stadExtra.used) {
            if (IsFlagsNode(node)) {
                if (!stadExtra.flags)
                    stadExtra.flags = node;
                isExtra = true;
            }
            else if (IsEffectsNode(node)) {
                if (!stadExtra.effects)
                    stadExtra.effects = node;
                isExtra = true;
            }
            else if (IsCollisionNode(node)) {
                if (!stadExtra.collision)
                    stadExtra.collision = node;
                isExtra = true;
            }
        }
        if (!isExtra) {
            if (node->mNumMeshes)
                nodes.emplace_back(node);
            for (unsigned int c = 0; c < node->mNumChildren; c++)
                NodeAddCallback(node->mChildren[c], nodes, stadExtra);
        }
    }
}

void ProcessBoundBox(aiVector3D &boundMin, aiVector3D &boundMax, bool &anyVertexProcessed, aiVector3D const &pos) {
    if (!anyVertexProcessed) {
        boundMin = pos;
        boundMax = pos;
        anyVertexProcessed = true;
    }
    else {
        if (pos.x < boundMin.x)
            boundMin.x = pos.x;
        if (pos.y < boundMin.y)
            boundMin.y = pos.y;
        if (pos.z < boundMin.z)
            boundMin.z = pos.z;
        if (pos.x > boundMax.x)
            boundMax.x = pos.x;
        if (pos.y > boundMax.y)
            boundMax.y = pos.y;
        if (pos.z > boundMax.z)
            boundMax.z = pos.z;
    }
}

void ScaleBoundBox(aiVector3D& boundMin, aiVector3D& boundMax) {
    aiVector3D boundCenter = boundMax - boundMin;
    boundCenter /= 2.0f;
    boundCenter += boundMin;
    aiVector3D boundUp = boundMax - boundCenter;
    aiVector3D boundLow = boundMin - boundCenter;
    boundUp *= options().bboxScale;
    boundLow *= options().bboxScale;
    boundMax = boundCenter + boundUp;
    boundMin = boundCenter + boundLow;
}

unsigned int SamplerIndex(unsigned int argType) {
    if (argType == Shader::Sampler1 || argType == Shader::Sampler1Local)
        return 1;
    else if (argType == Shader::Sampler2 || argType == Shader::Sampler2Local)
        return 2;
    else if (argType == Shader::Sampler3 || argType == Shader::Sampler3Local)
        return 3;
    return 0;
}

bool IsGlobalSampler(unsigned int argType) {
    return argType == Shader::Sampler0 || argType == Shader::Sampler1 || argType == Shader::Sampler2 || argType == Shader::Sampler3;
}

bool IsLocalSampler(unsigned int argType) {
    return argType == Shader::Sampler0Local || argType == Shader::Sampler1Local || argType == Shader::Sampler2Local || argType == Shader::Sampler3Local;
}

bool GetTexInfo(aiScene const *scene, aiMaterial const *mat, aiTextureType texType, aiTextureMapMode &mapMode, path &texFilePath, string &texFileName, string &texFileNameLowered, bool &isGlobal, TexEmbedded &embedded) {
    mapMode = aiTextureMapMode_Wrap;
    texFilePath.clear();
    texFileName.clear();
    isGlobal = false;
    embedded.data = nullptr;
    auto texCount = mat->GetTextureCount(texType);
    if (texCount > 0) {
        aiString texPath;
        mat->GetTexture(texType, 0, &texPath, nullptr, nullptr, nullptr, nullptr, &mapMode);
        if (auto texture = scene->GetEmbeddedTexture(texPath.C_Str())) {
            texFilePath = texture->mFilename.C_Str();
            embedded.data = texture->pcData;
            embedded.width = texture->mWidth;
            embedded.height = texture->mHeight;
            embedded.format = texture->achFormatHint;
        }
        else
            texFilePath = texPath.C_Str();
        texFileName = texFilePath.stem().string();
        texFileNameLowered.clear();
        if (!texFileName.empty()) {
            texFileNameLowered = ToLower(texFileName);
            if (texFileNameLowered.starts_with("g_")) {
                string globalTexName = texFileName.substr(2);
                if (globalTexName.empty())
                    return false;
                isGlobal = true;
                texFileName = globalTexName;
                texFileNameLowered = ToLower(texFileName);
            }
            return true;
        }
    }
    return false;
}

bool LoadTextureIntoTexSlot(aiScene const *scene, aiMaterial const *mat, aiTextureType texType, map<string, Tex> &texMap, bool *texPresentFlag, Tex *slot, map<string, string> &generatedTexNames, string const &customName) {
    aiTextureMapMode mapMode = aiTextureMapMode_Wrap;
    path texFilePath;
    string texFileName;
    string texFileNameLowered;
    bool isGlobal = false;
    TexEmbedded embedded;
    bool present = !mat || GetTexInfo(scene, mat, texType, mapMode, texFilePath, texFileName, texFileNameLowered, isGlobal, embedded);
    if (present) {
        if (!mat) {
            if (customName.empty())
                return false;
            mapMode = aiTextureMapMode_Wrap;
            texFilePath = customName;
            texFileName = customName;
            texFileNameLowered = ToLower(customName);
            isGlobal = texFileNameLowered.starts_with("g_");
            if (isGlobal) {
                string globalTexName = texFileName.substr(2);
                if (globalTexName.empty())
                    return false;
                texFileName = globalTexName;
                texFileNameLowered = ToLower(texFileName);
            }
            embedded.data = nullptr;
        }
        if (isGlobal) {
            *texPresentFlag = true;
            slot->isGlobal = true;
            slot->name = texFileName;
        }
        else {
            if (options().genTexNames) {
                auto tnit = generatedTexNames.find(texFileNameLowered);
                if (tnit == generatedTexNames.end())
                    throw runtime_error("Unable to find generated texture name");
                texFileName = (*tnit).second;
            }
            else if (texFileName.length() > 4)
                texFileName = texFileName.substr(0, 4);
            string texId = ToLower(texFileName);
            auto texit = texMap.find(texId);
            if (texit != texMap.end()) {
                *texPresentFlag = true;
                *slot = (*texit).second;
            }
            else {
                slot->name = texFileName;
                slot->filepath = texFilePath.string();
                slot->embedded = embedded;
                slot->SetUVAddressingMode(mapMode);
                texMap[texId] = *slot;
            }
        }
        return true;
    }
    return false;
}

void oimport(path const &out, path const &in) {
    Target *target = globalVars().target;
    if (!target)
        throw runtime_error("Unknown target");
    string targetName = target->Name();
    Assimp::Importer importer;
    importer.SetPropertyInteger(AI_CONFIG_PP_SBP_REMOVE, aiPrimitiveType_POINT | aiPrimitiveType_LINE);
    //importer.SetPropertyInteger(AI_CONFIG_IMPORT_REMOVE_EMPTY_BONES, 0);
    importer.SetPropertyInteger(AI_CONFIG_PP_SLM_VERTEX_LIMIT, 32'767);
    unsigned int sceneLoadingFlags = aiProcess_Triangulate | aiProcess_GenSmoothNormals | aiProcess_GenUVCoords | aiProcess_SplitLargeMeshes |
        aiProcess_SortByPType | aiProcess_PopulateArmatureData;
    bool doScale = options().scale.x != 1.0f || options().scale.y != 1.0f || options().scale.z != 1.0f;
    if (doScale && !options().scaleXYZ) {
        importer.SetPropertyFloat(AI_CONFIG_GLOBAL_SCALE_FACTOR_KEY, options().scale.x);
        sceneLoadingFlags |= aiProcess_GlobalScale;
    }
    if (!options().swapYZ)
        sceneLoadingFlags |= aiProcess_FlipUVs;
    if (options().preTransformVertices)
        sceneLoadingFlags |= aiProcess_PreTransformVertices;
    unsigned int maxBones = target->GetMaxBoneWeightsPerVertex();
    if (options().maxBonesPerVertex != 0) {
        if (options().maxBonesPerVertex > 3)
            maxBones = 3;
        else
            maxBones = options().maxBonesPerVertex;
    }
    if (options().boneRemap.empty()) {
        importer.SetPropertyInteger(AI_CONFIG_PP_LBW_MAX_WEIGHTS, maxBones);
        sceneLoadingFlags |= aiProcess_LimitBoneWeights;
    }
    const aiScene *scene = importer.ReadFile(in.string(), sceneLoadingFlags);
    if (!scene)
        throw runtime_error("Unable to load scene");
    if (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE)
        throw runtime_error("Unable to load a complete scene");
    if (!scene->mRootNode)
        throw runtime_error("Unable to find scene root node");
    Node::scene = scene;

    // TODO: axis detection

    static aiMatrix4x4 identityMatrix = { 1, 0, 0, 0,  0, 1, 0, 0,  0, 0, 1, 0,  0, 0, 0, 1 };
    Vector4D vecZeroOneTwoThree = { 0, 1, 2, 3 };
    Vector4D vec0000 = { 1, 1, 1, 1 };
    Vector4D vec1111 = { 1, 1, 1, 1 };
    Vector4D vecEnvMapConstants = { 1.0f, 0.25f, 0.5f, 0.75f };
    Vector4D_int vec0505051 = { 0x3EFEFF00, 0x3EFEFF00, 0x3EFEFF00, 0x3F800000 };
    Vector4D_int vec3E30B0B1 = { 0x3E30B0B1, 0x3E30B0B1, 0x3E30B0B1, 0x3E30B0B1 };
    Vector4D_int vec3DA0A0A1 = { 0x3DA0A0A1, 0x3DA0A0A1, 0x3DA0A0A1, 0x3DA0A0A1 };
    Vector4D_int vec40200000 = { 0x40200000, 0x40A00000, 0x3F000000, 0x3F800000 };

    bool flipAxis = options().swapYZ;
    bool doTranslate = options().translate.x != 0 || options().translate.y != 0 || options().translate.z != 0;
    bool hasSkeleton = false;
    bool hasMorph = false;
    bool hasLights = /*scene->HasLights() ||*/ options().forceLighting;
    const unsigned int ZERO = 0;
    const unsigned int ONE = 1;
    unsigned int ANIM_VERSION = target->AnimVersion();
    const int MINONE = -1;
    const float FONE = 1.0f;
    const float FZERO = 0.0f;
    BinaryBuffer bufData;
    string modelName = out.stem().string();
    auto outExt = out.extension().string();
    bool isOrd = ToLower(outExt) == ".ord";
    unsigned int uid = options().uid;
    if (uid == 0)
        uid = Hash(modelName);
    aiColor4D DEFAULT_COLOR = { 0.5f, 0.5f, 0.5f, 1.0f };

    vector<Symbol> symbols;
    map<string, vector<unsigned int>> relocations;
    Modifiables modifiables;
    unsigned int numVariations = 1;
    if (options().instances != 0)
        numVariations = options().instances;
    unsigned short computationIndex = 2;
    if (options().computationIndex != -1)
        numVariations = unsigned short(options().computationIndex);
    vector<Node> nodes;
    map<string, BoneInfo> bones; // name -> [index, aiBone]
    map<string, Tex> textures;

    StadiumExtra stadExtra;

    if (modelName.starts_with("m716__")) {
        if (sscanf(modelName.c_str(), "m716__%d_%d", &stadExtra.stadiumId, &stadExtra.lightingId) == 2) {
            stadExtra.used = true;
            stadExtra.stadType = StadiumExtra::STAD_DEFAULT;
        }
    }
    else if (modelName.starts_with("stadium_")) {
        if (sscanf(modelName.c_str(), "stadium_%d", &stadExtra.lightingId) == 1) {
            stadExtra.used = true;
            stadExtra.stadType = StadiumExtra::STAD_CUSTOM;
        }
    }

    NodeAddCallback(scene->mRootNode, nodes, stadExtra);

    unsigned int nodeCounter = 0;
    unsigned int meshCounter = 0;
    // generate node names for unnamed nodes
    unsigned int nextSortgroup = 0;
    for (auto &n : nodes) {
        if (n.name.empty()) {
            string newname = "sortgroup" + to_string(nextSortgroup++);
            while (nextSortgroup < 10'000) {
                bool exists = false;
                for (auto const &nn : nodes) {
                    if (ToLower(nn.name) == newname) {
                        exists = true;
                        break;
                    }
                }
                if (!exists)
                    break;
                newname = "sortgroup" + to_string(nextSortgroup++);
            }
            n.name = newname;
        }
    }
    if (options().sortByName) {
        if (options().sortByAlpha)
            sort(nodes.begin(), nodes.end(), Node::SortByNameAndAlpha);
        else
            sort(nodes.begin(), nodes.end(), Node::SortByName);
    }
    else if (options().sortByAlpha)
        sort(nodes.begin(), nodes.end(), Node::SortByAlpha);

    map<string, string> generatedTexNames; // key: lowered original filename, value - 4-byte name
    if (options().genTexNames) {
        map<string, pair<string, path>> usedTexNames; // key: lowered original filename, value - original filename and filepath
        for (auto &n : nodes) {
            for (unsigned int m = 0; m < n.node->mNumMeshes; m++) {
                aiMesh *mesh = scene->mMeshes[n.node->mMeshes[m]];
                aiMaterial *mat = scene->mMaterials[mesh->mMaterialIndex];
                aiTextureType texTypes[] = { aiTextureType_DIFFUSE, aiTextureType_REFLECTION, aiTextureType_SPECULAR };
                for (auto texType : texTypes) {
                    aiTextureMapMode mapMode = aiTextureMapMode_Wrap;
                    path texFilePath;
                    string texFileName;
                    string texFileNameLowered;
                    bool isGlobal = false;
                    TexEmbedded embedded;
                    // TODO [Improvement]: store information for next usage
                    if (GetTexInfo(scene, mat, texType, mapMode, texFilePath, texFileName, texFileNameLowered, isGlobal, embedded) && !isGlobal) {
                        if (!usedTexNames.contains(texFileNameLowered))
                            usedTexNames[texFileNameLowered] = { texFileName, texFilePath };
                    }
                }
            }
        }
        set<string> newTexNames; // 4-byte names
        unsigned int numTexturesForNameGen = 0;
        for (auto const &[k, t] : usedTexNames) {
            if (t.first.length() <= 4) {
                generatedTexNames[k] = t.first;
                newTexNames.insert(ToLower(t.first));
            }
            else
                numTexturesForNameGen++;
        }
        if (numTexturesForNameGen > 0) {
            unsigned int texIdLimit = 0;
            string texNameFormat;
            if (numTexturesForNameGen < 10) {
                texNameFormat = "tex%1d";
                texIdLimit = 10;
            }
            else if (numTexturesForNameGen < 100) {
                texNameFormat = "tx%02d";
                texIdLimit = 100;
            }
            else if (numTexturesForNameGen < 1'000) {
                texNameFormat = "t%03d";
                texIdLimit = 1'000;
            }
            else if (numTexturesForNameGen < 10'000) {
                texNameFormat = "%04d";
                texIdLimit = 10'000;
            }
            else
                throw runtime_error("Reached textures limit for texture names generator. Either disable getTexNames option or decrease the amount of textures in the file.");
            unsigned int nextIndexToUse = 0;
            for (auto const &[k, t] : usedTexNames) {
                if (t.first.length() > 4) {
                    while (true) {
                        if (nextIndexToUse >= texIdLimit)
                            throw runtime_error("Reached textures limit for texture names generator. Either disable getTexNames option or decrease the amount of textures in the file.");
                        string newTexName = Format(texNameFormat, nextIndexToUse++);
                        string newTexNameLowered = ToLower(newTexName);
                        if (!newTexNames.contains(newTexNameLowered)) {
                            generatedTexNames[k] = newTexName;
                            newTexNames.insert(newTexNameLowered);
                            break;
                        }
                    }
                }
            }
        }
    }

    for (auto &n : nodes) {
        for (unsigned int m = 0; m < n.node->mNumMeshes; m++) {
            aiMesh *mesh = scene->mMeshes[n.node->mMeshes[m]];
            aiMaterial *mat = scene->mMaterials[mesh->mMaterialIndex];
            string matName = mat->GetName().C_Str();
            auto matOptions = GetNameOptions(matName, true);

            if (matOptions.empty() && options().head && (targetName == "FIFA09" || targetName == "FIFA10" || targetName == "FM13")) {
                string matNameL = ToLower(matName);
                enum class HeadMatType { None, Head, Hair, Haircap, Eyes } headMatType = HeadMatType::None;
                if (matNameL == "head" || matNameL == "face" || matNameL == "skin" || matNameL == "mouth" || matNameL == "teeth" || matNameL == "oral")
                    headMatType = HeadMatType::Head;
                else if (matNameL == "hair")
                    headMatType = HeadMatType::Hair;
                else if (matNameL == "haircap")
                    headMatType = HeadMatType::Haircap;
                else if (matNameL == "eyes" || matNameL == "eye")
                    headMatType = HeadMatType::Eyes;
                if (headMatType == HeadMatType::None) {
                    string nodeNameL = ToLower(n.node->mName.C_Str());
                    if (nodeNameL == "head" || nodeNameL == "face" || nodeNameL == "skin" || nodeNameL == "mouth" || nodeNameL == "teeth" || nodeNameL == "oral")
                        headMatType = HeadMatType::Head;
                    else if (nodeNameL == "hair")
                        headMatType = HeadMatType::Hair;
                    else if (nodeNameL == "haircap")
                        headMatType = HeadMatType::Haircap;
                    else if (nodeNameL == "eyes" || nodeNameL == "eye")
                        headMatType = HeadMatType::Eyes;
                }
                if (headMatType != HeadMatType::None) {
                    if (headMatType == HeadMatType::Eyes) {
                        matOptions["shader"] = "LitTextureEye_Skin";
                        matOptions["tex0"] = "eyeb";
                    }
                    else if (headMatType == HeadMatType::Head) {
                        matOptions["shader"] = "LitTexture4Head_Skin";
                        matOptions["tex0"] = "tp01";
                        matOptions["tex1"] = "tp02";
                    }
                    else if (headMatType == HeadMatType::Haircap) {
                        matOptions["shader"] = "LitTexture2Haircap_Skin";
                        matOptions["tex0"] = "tp02";
                    }
                    else if (headMatType == HeadMatType::Hair) {
                        matOptions["shader"] = "LitTexture2Hair_Skin";
                        matOptions["tex0"] = "tp02";
                    }
                }
            }

            Shader *shader = nullptr;
            bool meshHasBones = mesh->HasBones();
            bool isMeshSkinned = false;

            aiColor3D matColor(255, 255, 255);
            float matAlpha = 1;
            AlphaMode alphaMode = ALPHA_NOT_SET;
            int blendFunc = 0;
            GetMatColorAndAlphaProperties(mat, matColor, matAlpha, alphaMode, blendFunc);
            aiColor3D matSpecColor(1, 1, 1);
            mat->Get(AI_MATKEY_COLOR_SPECULAR, matSpecColor);
            int isWireframe = 0;
            mat->Get(AI_MATKEY_ENABLE_WIREFRAME, isWireframe);
            isWireframe = isWireframe != 0;
            int isTwoSided = 0;
            mat->Get(AI_MATKEY_TWOSIDED, isTwoSided);
            isTwoSided = isTwoSided != 0;
            int shadingMode = aiShadingMode_Gouraud;
            mat->Get(AI_MATKEY_SHADING_MODEL, shadingMode);
            float shininess = 0.0f;
            mat->Get(AI_MATKEY_SHININESS, shininess);
            float shininessStrength = 0.0f;
            mat->Get(AI_MATKEY_SHININESS_STRENGTH, shininessStrength);
            bool isUnlit = false;
            mat->Get(AI_MATKEY_GLTF_UNLIT, isUnlit);
            float alphaCutoff = 0.0f;
            mat->Get(AI_MATKEY_GLTF_ALPHACUTOFF, alphaCutoff);
            float roughnessFactor = 1.0f;
            mat->Get(AI_MATKEY_GLTF_PBRMETALLICROUGHNESS_ROUGHNESS_FACTOR, roughnessFactor);
            float metallicFactor = 0.0f;
            mat->Get(AI_MATKEY_GLTF_PBRMETALLICROUGHNESS_METALLIC_FACTOR, metallicFactor);

            bool hasMatColor = matColor.r != 1 || matColor.g != 1 || matColor.b != 1;
            bool hasMatAlpha = matAlpha != 1;
            bool usesAlphaBlending = blendFunc != 0 || hasMatAlpha || alphaMode == ALPHA_BLEND;
            bool isMetallic = false; // roughnessFactor <= 0.5f && metallicFactor >= 0.5f;

            Tex tex[4];
            bool isTextured[4] = { false, false, false, false };
            bool texAlreadyPresent[4] = { false, false, false, false };

            for (size_t t = 0; t < std::size(isTextured); t++) {
                auto texIt = matOptions.find("tex" + to_string(t));
                if (texIt != matOptions.end()) {
                    isTextured[t] = true;
                    LoadTextureIntoTexSlot(scene, nullptr, aiTextureType_NONE, textures, &texAlreadyPresent[t], &tex[t], generatedTexNames, (*texIt).second);
                }
            }

            bool hasDiffuseTex = isTextured[0];
            if (!hasDiffuseTex) {
                if (LoadTextureIntoTexSlot(scene, mat, aiTextureType_DIFFUSE, textures, &texAlreadyPresent[0], &tex[0], generatedTexNames, string())) {
                    hasDiffuseTex = true;
                    isTextured[0] = true;
                }
            }
            bool hasReflectionTex = false;
            bool hasSpecularTex = false;
            if (!isTextured[1]) {
                hasReflectionTex = LoadTextureIntoTexSlot(scene, mat, aiTextureType_REFLECTION, textures, &texAlreadyPresent[1], &tex[1], generatedTexNames, string());
                if (!hasReflectionTex)
                    hasSpecularTex = LoadTextureIntoTexSlot(scene, mat, aiTextureType_SPECULAR, textures, &texAlreadyPresent[1], &tex[1], generatedTexNames, string());
                if (hasReflectionTex || hasSpecularTex)
                    isTextured[1] = true;
            }

            bool usesCustomShaderName = false;

            if (!options().forceShader.empty()) {
                shader = target->FindShader(options().forceShader);
            }

            if (!shader) {
                auto optIt = matOptions.find("shader");
                if (optIt != matOptions.end()) {
                    shader = target->FindShader((*optIt).second);
                    if (shader)
                        usesCustomShaderName = true;
                }
            }

            if (!shader) {
                MaterialProperties matProps;
                matProps.isAdboard = tex[0].IsFIFAAdboardsTexture();
                if (!matProps.isAdboard)
                    matProps.isHomeCrowd = tex[0].IsFIFACrowdHomeTexture();
                else
                    matProps.isHomeCrowd = false;
                if (!matProps.isAdboard && !matProps.isHomeCrowd)
                    matProps.isAwayCrowd = tex[0].IsFIFACrowdAwayTexture();
                else
                    matProps.isAwayCrowd = false;
                matProps.isAdditive = blendFunc == aiBlendMode::aiBlendMode_Additive;
                matProps.isLit = hasLights;
                matProps.isMetallic = isMetallic;
                matProps.isReflective = hasReflectionTex;
                matProps.isShiny = hasSpecularTex;
                matProps.isSkinned = meshHasBones;
                matProps.isTextured = hasDiffuseTex;
                matProps.isTransparent = usesAlphaBlending;
                matProps.isUnlit = isUnlit;
                matProps.numUVs = mesh->GetNumUVChannels();
                shader = target->DecideShader(matProps);
            }

            if (!shader)
                shader = target->FindShader("Gouraud");

            if (!shader)
                throw runtime_error("Unable to decide shader");

            auto shaderName = shader->name;
            auto shaderExt = shaderName.find('.');
            if (shaderExt != string::npos)
                shaderName = shaderName.substr(0, shaderExt);
            auto shaderNameLowered = ToLower(shaderName);

            if (!isTextured[1]) {
                if (!hasReflectionTex && !hasSpecularTex &&
                    (
                    shaderNameLowered == "littextureirradenvmap" ||
                    shaderNameLowered == "irradlittextureenvmaptransparent2x" ||
                    shaderNameLowered == "littextureirradenvmap_skin"
                    )
                    )
                {
                    if (LoadTextureIntoTexSlot(scene, nullptr, aiTextureType_NONE, textures, &texAlreadyPresent[1], &tex[1], generatedTexNames, "spec")) {
                        hasSpecularTex = true;
                        isTextured[1] = true;
                    }
                } else if (shaderNameLowered == "littexture4head_skin") {
                    if (LoadTextureIntoTexSlot(scene, nullptr, aiTextureType_NONE, textures, &texAlreadyPresent[1], &tex[1], generatedTexNames, "tp02"))
                        isTextured[1] = true;
                }
                else if (shaderNameLowered == "littexture2irradskinsubsurfspec") {
                    if (LoadTextureIntoTexSlot(scene, nullptr, aiTextureType_NONE, textures, &texAlreadyPresent[1], &tex[1], generatedTexNames, "glos"))
                        isTextured[1] = true;
                }
            }

            if (shaderNameLowered == "xfadescrolltexture") {
                if (!n.name.starts_with("sortgroup"))
                    n.name = "sortgroup_" + n.name;
            }

            isMeshSkinned = shader->HasAttribute(Shader::BlendWeight) && shader->HasAttribute(Shader::BlendIndices) && shader->HasAttribute(Shader::Color1);
            bool useSkinning = isMeshSkinned && meshHasBones;

            vector<VertexWeightInfo> allMeshesVertexWeights;

            if (useSkinning) {
                if (!hasSkeleton)
                    hasSkeleton = true;
                if (bones.empty()) {
                    if (options().boneRemap.empty()) {
                        if (mesh->mNumBones > 255)
                            throw runtime_error("Failed to load bones array: using more than 255 bones in skeleton is not allowed");
                        unsigned char maxBoneIndex = 0;
                        set<unsigned char> usedBoneIndices;
                        for (unsigned int b = 0; b < mesh->mNumBones; b++) {
                            unsigned int boneIndex = 0;
                            string boneName;
                            string bname = mesh->mBones[b]->mNode->mName.C_Str();
                            auto idbp = bname.find('[');
                            if (idbp != string::npos) {
                                auto idbe = bname.find(']', idbp + 1);
                                if (idbe != string::npos) {
                                    string idstr = bname.substr(idbp + 1, idbe - idbp - 1);
                                    if (!idstr.empty()) {
                                        try {
                                            boneIndex = stoi(idstr);
                                        }
                                        catch (...) {
                                            throw runtime_error("Failed to get bone index: index is incorrect");
                                        }
                                        if (boneIndex > 255)
                                            throw runtime_error("Failed to load bones array: bone with index greater than 255 is not allowed");
                                    }
                                    else
                                        throw runtime_error("Failed to get bone index: index is incorrect");
                                }
                                else
                                    throw runtime_error("Failed to get bone index: index format is incorrect");
                            }
                            else
                                throw runtime_error("Failed to get bone index: index is not present");
                            if (!usedBoneIndices.contains(boneIndex))
                                usedBoneIndices.insert(boneIndex);
                            else
                                throw runtime_error("Failed to load bones array: duplicated bone index in bones array");
                            boneName = bname.substr(0, idbp);
                            Trim(boneName);
                            bones[bname] = { unsigned char(boneIndex), mesh->mBones[b], boneName };
                            if (maxBoneIndex < boneIndex)
                                maxBoneIndex = boneIndex;
                        }
                        if (!bones.empty()) {
                            if (bones.size() != (maxBoneIndex + 1))
                                throw runtime_error(Format("Failed to load bones array: bones array size (%d) does not match the highest bone index (%d)", bones.size(), maxBoneIndex));
                        }
                    }
                    else {
                        for (unsigned int b = 0; b < mesh->mNumBones; b++) {
                            string bname = mesh->mBones[b]->mNode->mName.C_Str();
                            string boneName;
                            auto idbp = bname.find('[');
                            if (idbp != string::npos)
                                boneName = bname.substr(0, idbp);
                            else
                                boneName = bname;
                            Trim(boneName);
                            bones[bname] = { 0, mesh->mBones[b], boneName };
                        }
                    }
                }
                // Find weights for all vertices
                allMeshesVertexWeights.resize(mesh->mNumVertices);
                for (unsigned int b = 0; b < mesh->mNumBones; b++) {
                    aiBone *bone = mesh->mBones[b];
                    if (bone->mNumWeights > 1 || (bone->mNumWeights == 1 && bone->mWeights[0].mWeight > 0.0f)) {
                        if (bones.contains(bone->mNode->mName.C_Str())) {
                            auto const &boneInfo = bones[bone->mNode->mName.C_Str()];
                            BoneTargets *targets = nullptr;
                            bool use = true;
                            if (!options().boneRemap.empty()) {
                                if (globalVars().boneRemap.contains(boneInfo.name))
                                    targets = &globalVars().boneRemap[boneInfo.name];
                                else {
                                    use = false; // false
                                    //throw runtime_error(Format("No remap info for bone %s", bone->mNode->mName.C_Str()));
                                    //Error(Format("No remap info for bone %s (%d weights)", bone->mNode->mName.C_Str(), bone->mNumWeights));
                                }
                            }
                            if (use) {
                                for (unsigned int w = 0; w < bone->mNumWeights; w++) {
                                    if (bone->mWeights[w].mWeight > 0) {
                                        auto &vw = allMeshesVertexWeights[bone->mWeights[w].mVertexId];
                                        if (targets) {
                                            auto const &targetBones = targets->targetBones;
                                            for (auto const &tb : targetBones) {
                                                float weight = bone->mWeights[w].mWeight * tb.factor;
                                                bool found = false;
                                                for (auto &b : vw.bones) {
                                                    if (b.boneIndex == tb.boneIndex) {
                                                        b.weight += weight;
                                                        found = true;
                                                        break;
                                                    }
                                                }
                                                if (!found) {
                                                    VertexBoneInfo vwi;
                                                    vwi.weight = weight;
                                                    vwi.boneIndex = tb.boneIndex;
                                                    vw.bones.push_back(vwi);
                                                }
                                            }
                                        }
                                        else {
                                            float weight = bone->mWeights[w].mWeight;
                                            bool found = false;
                                            for (auto &b : vw.bones) {
                                                if (b.boneIndex == boneInfo.index) {
                                                    b.weight += weight;
                                                    found = true;
                                                    break;
                                                }
                                            }
                                            if (!found) {
                                                VertexBoneInfo vwi;
                                                vwi.weight = weight;
                                                vwi.boneIndex = boneInfo.index;
                                                vw.bones.push_back(vwi);
                                            }
                                        }
                                    }
                                }
                            }
                        }
                        else
                            throw runtime_error("Unable to find bone in bones array");
                    }
                }
                // Sort weights in vertices
                for (auto &vw : allMeshesVertexWeights) {
                    if (vw.bones.empty()) {
                        VertexBoneInfo bi;
                        bi.boneIndex = 0;
                        bi.weight = 1.0f;
                        vw.bones.push_back(bi);
                    }
                    if (vw.bones.size() > 1)
                        sort(vw.bones.begin(), vw.bones.end());
                    if (vw.bones.size() > maxBones)
                        vw.bones.resize(maxBones);
                    float totalBoneWeights = 0.0f;
                    for (auto &b : vw.bones)
                        totalBoneWeights += b.weight;
                    if (totalBoneWeights != 1.0f) {
                        for (auto &b : vw.bones)
                            b.weight /= totalBoneWeights;
                    }
                    if (options().vertexWeightPaletteSize > 0) {
                        if (options().vertexWeightPaletteSize == 1) {
                            for (auto &b : vw.bones)
                                b.weight = 1.0f;
                        }
                        else {
                            VertexWeightInfo newvw;
                            for (VertexBoneInfo b : vw.bones) {
                                b.weight = floor(b.weight * options().vertexWeightPaletteSize);
                                if (b.weight > 0.0f)
                                    newvw.bones.push_back(b);
                            }
                            if (newvw.bones.empty()) {
                                VertexBoneInfo bi;
                                bi.boneIndex = vw.bones[0].boneIndex;
                                bi.weight = 1.0f;
                                newvw.bones.push_back(bi);
                            }
                            vw = newvw;
                        }
                        totalBoneWeights = 0.0f;
                        for (auto &b : vw.bones)
                            totalBoneWeights += b.weight;
                        if (totalBoneWeights != 1.0f) {
                            for (auto &b : vw.bones)
                                b.weight /= totalBoneWeights;
                        }
                    }
                }
            }

            unsigned int vertexSize = shader->VertexSize();
            const unsigned int indexSize = 2;
            unsigned int numColors = mesh->GetNumColorChannels();
            unsigned int numTexCoords = mesh->GetNumUVChannels();

            unsigned int totalNumIndices = mesh->mNumFaces * 3;
            unsigned int totalNumFaces = mesh->mNumFaces;
            unsigned int totalIndexBufferSize = indexSize * totalNumIndices;
            vector<unsigned short> allMeshesIndexBuffer(totalNumIndices);
            Memory_Zero(allMeshesIndexBuffer.data(), totalIndexBufferSize);

            vector<MeshInfo> meshes;

            meshes.push_back(MeshInfo());
            meshes.back().startFace = 0;

            for (unsigned int f = 0; f < mesh->mNumFaces; f++) {
                unsigned int *tri = mesh->mFaces[f].mIndices;
                if (useSkinning) {
                    unsigned int numBoneWeights = meshes.back().weightsMap.size();
                    if ((numBoneWeights + 3) > target->GetMaxVertexWeightsPerMesh()) {
                        unsigned int maxNumBoneWeightsToAdd = target->GetMaxVertexWeightsPerMesh() - numBoneWeights;
                        unsigned int numWeightsToAdd = 0;
                        for (unsigned int ind = 0; ind < 3; ind++) {
                            VertexWeightInfoLayout vwl(allMeshesVertexWeights[tri[ind]]);
                            if (!meshes.back().weightsMap.contains(vwl)) {
                                numWeightsToAdd++;
                                if (numWeightsToAdd > maxNumBoneWeightsToAdd) {
                                    meshes.back().numFaces = f - meshes.back().startFace;
                                    meshes.push_back(MeshInfo());
                                    meshes.back().startFace = f;
                                    break;
                                }
                            }
                        }
                    }
                    for (unsigned int ind = 0; ind < 3; ind++) {
                        VertexWeightInfoLayout vwl(allMeshesVertexWeights[tri[ind]]);
                        meshes.back().weightsMap[vwl].push_back(tri[ind]);
                    }
                }
                for (unsigned int ind = 0; ind < 3; ind++)
                    meshes.back().verticesMap[tri[ind]] = 0;
                allMeshesIndexBuffer[f * 3 + 0] = tri[0];
                allMeshesIndexBuffer[f * 3 + 1] = tri[1];
                allMeshesIndexBuffer[f * 3 + 2] = tri[2];
            }
            meshes.back().numFaces = totalNumFaces - meshes.back().startFace;

            //Error("%d meshes");
            
            for (auto &m : meshes) {
                unsigned int numVertices = m.verticesMap.size();
                unsigned int vertIndex = 0;
                for (auto &e : m.verticesMap)
                    e.second = vertIndex++;
                unsigned int vertexBufferSize = vertexSize * numVertices;
                vector<unsigned char> vertexBuffer(vertexBufferSize);
                Memory_Zero(vertexBuffer.data(), vertexBufferSize);
                unsigned int vertexBufferOffset = 0;
                unsigned int indexBufferOffset = 0;
                unsigned int startIndex = m.startFace * 3;
                unsigned int numFaces = m.numFaces;
                unsigned int numIndices = numFaces * 3;
                unsigned int indexBufferSize = numIndices * indexSize;
                vector<unsigned short> indexBuffer(numIndices);
                for (unsigned int ind = 0; ind < numIndices; ind++)
                    indexBuffer[ind] = m.verticesMap[allMeshesIndexBuffer[startIndex + ind]];
                unsigned int vertexWeightsNumBones3 = 0;
                unsigned int vertexWeightsNumBones2 = 0;
                unsigned int vertexWeightsNumBones1 = 0;

                // generate tristrips
                if (options().tristrip) {
                    SetListsOnly(false);
                    SetCacheSize(CACHESIZE_GEFORCE3);
                    PrimitiveGroup *prims = nullptr;
                    unsigned short numprims = 0;
                    GenerateStrips(indexBuffer.data(), numIndices, &prims, &numprims);
                    numIndices = prims[0].numIndices;
                    numFaces = numIndices - 2;
                    indexBufferSize = indexSize * numIndices;
                    indexBuffer.resize(numIndices);
                    Memory_Copy(indexBuffer.data(), prims[0].indices, indexBufferSize);
                    delete[] prims;
                }

                vector<VertexWeightInfoLayout> skinVertexWeights;
                vector<unsigned int> skinVertexWeightsIndices;

                if (useSkinning && !m.weightsMap.empty()) {
                    skinVertexWeights.resize(m.weightsMap.size());
                    skinVertexWeightsIndices.resize(numVertices);
                    unsigned int weightInfoIndex = 0;
                    for (auto const &[w, vertIndices] : m.weightsMap) {
                        skinVertexWeights[weightInfoIndex] = w;
                        if (w.numBones == 3)
                            vertexWeightsNumBones3++;
                        else if (w.numBones == 2)
                            vertexWeightsNumBones2++;
                        else if (w.numBones == 1)
                            vertexWeightsNumBones1++;
                        skinVertexWeights[weightInfoIndex].numBones = 0;
                        //for (int b = 0; b < 3; b++) {
                        //    if (skinVertexWeights[weightInfoIndex].bones[b].ucValue > 51)
                        //        Error("Incorrect bone struct");
                        //}
                        for (auto const vertIndex : vertIndices)
                            skinVertexWeightsIndices[m.verticesMap[vertIndex]] = weightInfoIndex;
                        weightInfoIndex++;
                    }
                }
                unsigned int vertexDataOffset = 0;
                for (auto const &[v, vi] : m.verticesMap) {
                    unsigned int vertexOffset = vertexDataOffset;
                    for (auto const &d : shader->declaration) {
                        switch (d.usage) {
                        case Shader::Position:
                            if (d.type == Shader::Float3 && mesh->mVertices) {
                                aiVector3D vecPos = mesh->mVertices[v];
                                if (doScale && options().scaleXYZ) {
                                    vecPos.x *= options().scale.x;
                                    vecPos.y *= options().scale.y;
                                    vecPos.z *= options().scale.z;
                                }
                                if (doTranslate) {
                                    vecPos.x += options().translate.x;
                                    vecPos.y += options().translate.y;
                                    vecPos.z += options().translate.z;
                                }
                                if (flipAxis)
                                    swap(vecPos.y, vecPos.z);
                                Memory_Copy(&vertexBuffer.data()[vertexOffset], &vecPos, 12);
                                ProcessBoundBox(n.boundMin, n.boundMax, n.anyVertexProcessed, vecPos);
                            }
                            break;
                        case Shader::Normal:
                            if (d.type == Shader::Float3 && mesh->mNormals) {
                                aiVector3D vecNormal = mesh->mNormals[v];
                                if (flipAxis)
                                    swap(vecNormal.y, vecNormal.z);
                                Memory_Copy(&vertexBuffer.data()[vertexOffset], &vecNormal, 12);
                            }
                            break;
                        case Shader::Color0:
                            if (d.type == Shader::D3DColor || d.type == Shader::UByte4) {
                                aiColor4D vertexColor;
                                if (options().hasSetVCol)
                                    vertexColor = options().setVCol;
                                else {
                                    auto GetMeshVCol = [](aiMesh *colMesh, unsigned int index, unsigned int vertexId, bool swapRB, bool srgb) {
                                        aiColor4D out = colMesh->mColors[index][vertexId];
                                        if (swapRB)
                                            swap(out.r, out.b);
                                        if (srgb) {
                                            for (unsigned int ci = 0; ci < 3; ci++)
                                                out[ci] = SrgbTransform::linearToSrgb(out[ci]);
                                        }
                                        return out;
                                    };
                                    bool colorPostProcess = true;
                                    if (options().mergeVCols) {
                                        bool hasVColMergeConfig = !options().vColMergeConfig.empty();
                                        vertexColor = { 1.0f, 1.0f, 1.0f, 1.0f };
                                        unsigned int startColIndex = hasVColMergeConfig ? 0 : 1;
                                        unsigned int endColIndex = hasVColMergeConfig ? options().vColMergeConfig.size() : AI_MAX_NUMBER_OF_COLOR_SETS;
                                        for (unsigned int colIndex = 0; colIndex < AI_MAX_NUMBER_OF_COLOR_SETS; colIndex++) {
                                            if (numColors > colIndex &&mesh->HasVertexColors(colIndex) && mesh->mColors[colIndex]) {
                                                bool colIndexUsed = hasVColMergeConfig ? options().vColMergeConfig.contains(colIndex) : true;
                                                if (colIndexUsed) {
                                                    auto vColLayer = GetMeshVCol(mesh, colIndex, v, true, options().srgb);
                                                    if (hasVColMergeConfig) {
                                                        auto const &config = options().vColMergeConfig[colIndex];
                                                        vColLayer = config.bottomRange + vColLayer * (config.topRange - config.bottomRange);
                                                    }
                                                    for (unsigned int clrComp = 0; clrComp < 4; clrComp++)
                                                        vertexColor[clrComp] *= vColLayer[clrComp];
                                                }
                                            }
                                        }
                                    }
                                    else {
                                        if (numColors > 0 && mesh->HasVertexColors(0) && mesh->mColors[0])
                                            vertexColor = GetMeshVCol(mesh, 0, v, true, options().srgb);
                                        else {
                                            if (options().hasDefaultVCol)
                                                vertexColor = options().defaultVCol;
                                            else
                                                vertexColor = DEFAULT_COLOR;
                                            colorPostProcess = false;
                                        }
                                    }
                                    if (colorPostProcess) {                                        
                                        if (options().vColScale != 0.0f) {
                                            vertexColor.r *= options().vColScale;
                                            vertexColor.g *= options().vColScale;
                                            vertexColor.b *= options().vColScale;
                                        }
                                        if (options().hasMinVCol) {
                                            if (vertexColor.r < options().minVCol.r)
                                                vertexColor.r = options().minVCol.r;
                                            if (vertexColor.g < options().minVCol.g)
                                                vertexColor.g = options().minVCol.g;
                                            if (vertexColor.b < options().minVCol.b)
                                                vertexColor.b = options().minVCol.b;
                                        }
                                        if (options().hasMaxVCol) {
                                            if (vertexColor.r > options().maxVCol.r)
                                                vertexColor.r = options().maxVCol.r;
                                            if (vertexColor.g > options().maxVCol.g)
                                                vertexColor.g = options().maxVCol.g;
                                            if (vertexColor.b > options().maxVCol.b)
                                                vertexColor.b = options().maxVCol.b;
                                        }
                                    }
                                }
                                if (options().useMatColor) {
                                    if (hasMatColor) {
                                        vertexColor.r *= matColor.r;
                                        vertexColor.g *= matColor.g;
                                        vertexColor.b *= matColor.b;
                                    }
                                    if (hasMatAlpha)
                                        vertexColor.a *= matAlpha;
                                }
                                unsigned char rgba[4];
                                for (unsigned int clr = 0; clr < 4; clr++)
                                    rgba[clr] = unsigned char(clamp(vertexColor[clr], 0.0f, 1.0f) * 255);
                                Memory_Copy(&vertexBuffer.data()[vertexOffset], rgba, 4);
                            }
                            break;
                        case Shader::Color1:
                            if (useSkinning)
                                Memory_Copy(&vertexBuffer.data()[vertexOffset], &skinVertexWeightsIndices.data()[vi], 4);
                            break;
                        case Shader::Texcoord0:
                            if (d.type == Shader::Float2) {
                                unsigned int channel = 0;
                                if (numTexCoords > channel && mesh->mTextureCoords[channel])
                                    Memory_Copy(&vertexBuffer.data()[vertexOffset], &mesh->mTextureCoords[channel][v], 8);
                            }
                            break;
                        case Shader::Texcoord1:
                            if (d.type == Shader::Float2) {
                                unsigned int channel = 1;
                                if (numTexCoords <= channel || !mesh->mTextureCoords[channel])
                                    channel = 0;
                                if (numTexCoords > channel && mesh->mTextureCoords[channel])
                                    Memory_Copy(&vertexBuffer.data()[vertexOffset], &mesh->mTextureCoords[channel][v], 8);
                            }
                            break;
                        case Shader::Texcoord2:
                            if (d.type == Shader::Float2) {
                                unsigned int channel = 2;
                                if (numTexCoords <= channel || !mesh->mTextureCoords[channel])
                                    channel = 0;
                                if (numTexCoords > channel && mesh->mTextureCoords[channel])
                                    Memory_Copy(&vertexBuffer.data()[vertexOffset], &mesh->mTextureCoords[channel][v], 8);
                            }
                            break;
                        case Shader::BlendIndices:
                            break;
                        case Shader::BlendWeight:
                            break;
                        }
                        vertexOffset += d.Size();
                    }
                    vertexDataOffset += vertexSize;
                }
                vector<GlobalArg> globalArgs;
                for (auto const &arg : shader->globalArguments) {
                    switch (arg.type) {
                    case Shader::GeometryInfo:
                        globalArgs.emplace_back(bufData.Position());
                        bufData.Put(numIndices);
                        bufData.Put(numVertices);
                        bufData.Put(numFaces);
                        bufData.Put(ZERO);
                        bufData.Put(ZERO);
                        break;
                    case Shader::ComputationIndex:
                    {
                        ComputationIndex idx;
                        globalArgs.emplace_back(modifiables.GetArg("ComputationIndex::CompIdx", bufData, idx, true));
                    }
                    break;
                    case Shader::VertexData:
                        vertexBufferOffset = bufData.Position();
                        globalArgs.emplace_back(bufData.Position(), numVertices);
                        bufData.Put(vertexBuffer.data(), vertexBufferSize);
                        bufData.Put(ONE);
                        break;
                    case Shader::IndexData:
                        indexBufferOffset = bufData.Position();
                        globalArgs.emplace_back(bufData.Position(), numIndices);
                        bufData.Put(indexBuffer.data(), indexBufferSize);
                        bufData.Put(ONE);
                        break;
                    case Shader::VertexSkinData:
                        globalArgs.emplace_back(bufData.Position(), skinVertexWeights.size());
                        bufData.Put(skinVertexWeights.data(), skinVertexWeights.size() * sizeof(VertexWeightInfoLayout));
                        bufData.Put(ZERO);
                        break;
                    case Shader::ModelMatrix:
                        globalArgs.emplace_back("__const MATRIX4:::EAGL::ViewPort::gpModelMatrix");
                        break;
                    case Shader::ViewMatrix:
                        globalArgs.emplace_back("__const MATRIX4:::EAGL::ViewPort::gpViewMatrix");
                        break;
                    case Shader::ProjectionMatrix:
                        globalArgs.emplace_back("__const MATRIX4:::EAGL::ViewPort::gpProjectionMatrix");
                        break;
                    case Shader::ModelViewProjectionMatrix:
                        globalArgs.emplace_back("__const MATRIX4:::EAGL::ViewPort::gpModelViewProjectionMatrix");
                        break;
                    case Shader::ModelViewMatrix:
                        globalArgs.emplace_back("__const MATRIX4:::EAGL::ViewPort::gpModelViewMatrix");
                        break;
                    case Shader::ZeroOneTwoThree:
                        globalArgs.emplace_back("__COORD4:::&EAGL::RenderMethodConstants::gZeroOneTwoThree");
                        break;
                    case Shader::ZeroOneTwoThreeLocal:
                        globalArgs.emplace_back(bufData.Position());
                        bufData.Put(vecZeroOneTwoThree);
                        bufData.Put(ZERO);
                        break;
                    case Shader::EnvMapConstants:
                        globalArgs.emplace_back("__COORD4:::&EAGL::RenderMethodConstants::gEnvMapConstants");
                        break;
                    case Shader::EnvMapConstantsLocal:
                        globalArgs.emplace_back(bufData.Position());
                        bufData.Put(vecEnvMapConstants);
                        bufData.Put(ZERO);
                        break;
                    case Shader::VecZeroLocal:
                        globalArgs.emplace_back(bufData.Position());
                        bufData.Put(vec0000);
                        bufData.Put(ZERO);
                        break;
                    case Shader::VecOneLocal:
                        globalArgs.emplace_back(bufData.Position());
                        bufData.Put(vec1111);
                        bufData.Put(ZERO);
                        break;
                    case Shader::Vec0505051Local:
                        globalArgs.emplace_back(bufData.Position());
                        bufData.Put(vec0505051);
                        bufData.Put(ZERO);
                        break;
                    case Shader::Vec3E30B0B1Local:
                        globalArgs.emplace_back(bufData.Position());
                        bufData.Put(vec3E30B0B1);
                        bufData.Put(ZERO);
                        break;
                    case Shader::Vec3DA0A0A1Local:
                        globalArgs.emplace_back(bufData.Position());
                        bufData.Put(vec3DA0A0A1);
                        bufData.Put(ZERO);
                        break;
                    case Shader::Vec40200000Local:
                        globalArgs.emplace_back(bufData.Position());
                        bufData.Put(vec40200000);
                        bufData.Put(ZERO);
                        break;
                    case Shader::EnvmapColour:
                        globalArgs.emplace_back("__COORD4:::EnvmapColour");
                        break;
                    case Shader::FogParameters:
                        globalArgs.emplace_back("__COORD4:::&EAGL::RenderMethodConstants::gFogParameters");
                        break;
                    case Shader::FogParameters0:
                        globalArgs.emplace_back("__COORD4:::SGR::Fog::Parameters0");
                        break;
                    case Shader::FogParameters1:
                        globalArgs.emplace_back("__COORD4:::SGR::Fog::Parameters1");
                        break;
                    case Shader::FogParameters2:
                        globalArgs.emplace_back("__COORD4:::SGR::Fog::Parameters2");
                        break;
                    case Shader::FogParameters3:
                        globalArgs.emplace_back("__COORD4:::SGR::Fog::Parameters3");
                        break;
                    case Shader::BaseColour:
                        globalArgs.emplace_back(modifiables.GetArg("Coordinate4::BaseColour", bufData, vec1111, true));
                        break;
                    case Shader::LightMultipliers:
                        globalArgs.emplace_back(modifiables.GetArg("Coordinate4::LightMultipliers", bufData, vec1111, true));
                        break;
                    case Shader::CrowdTintH:
                        globalArgs.emplace_back(modifiables.GetArg("Coordinate4::GeomName::CrowdTintH", bufData, vec0000, true));
                        break;
                    case Shader::CrowdTintA:
                        globalArgs.emplace_back(modifiables.GetArg("Coordinate4::GeomName::CrowdTintA", bufData, vec0000, true));
                        break;
                    case Shader::ShadowColour:
                        globalArgs.emplace_back("__COORD4:::gShadowColour");
                        break;
                    case Shader::ShadowColour2:
                        globalArgs.emplace_back("__COORD4:::gShadowColour2");
                        break;
                    case Shader::RMGrass_PSConstants:
                        globalArgs.emplace_back("__COORD4:::RMGrass::PSConstants");
                        break;
                    case Shader::RMGrass_VSConstants:
                        globalArgs.emplace_back("__COORD4:::RMGrass::VSConstants");
                        break;
                    case Shader::RMGrass_CameraPosition:
                        globalArgs.emplace_back("__COORD4:::RMGrass::CameraPosition");
                        break;
                    case Shader::EAGLAnimationBuffer:
                        globalArgs.emplace_back("__const MATRIX4:::EAGLAnimationBuffer", target->GetMaxVertexWeightsPerMesh());
                        break;
                    case Shader::ViewVector:
                        globalArgs.emplace_back("__COORD4:::ViewVector");
                        break;
                    case Shader::RimLightCol:
                        globalArgs.emplace_back("__COORD4:::SGR::Rim::RimLightCol");
                        break;
                    case Shader::SubSurfFactor:
                        globalArgs.emplace_back("__COORD4:::SGR::SubSurf::SubSurfFactor");
                        break;
                    case Shader::SpecLightVec:
                        globalArgs.emplace_back("__COORD4:::SGR::Specular::SpecLightVec");
                        break;
                    case Shader::SpecLightCol:
                        globalArgs.emplace_back("__COORD4:::SGR::Specular::SpecLightCol");
                        break;
                    case Shader::HalfVector:
                        globalArgs.emplace_back("__COORD4:::SGR::Specular::HalfVector");
                        break;
                    case Shader::HighlightAlpha:
                        globalArgs.emplace_back("__COORD4:::SGR::GameFace::HighlightAlpha");
                        break;
                    case Shader::ColourModulator:
                        globalArgs.emplace_back(modifiables.GetArg("Coordinate4::ColourModulator", "__COORD4:::ColourModulator", 16));
                        break;
                    case Shader::ColourTranslate:
                        globalArgs.emplace_back(modifiables.GetArg("Coordinate4::ColourTranslate", "__COORD4:::ColourTranslate", 16));
                        break;
                    case Shader::Fresnel:
                        globalArgs.emplace_back("__COORD4:::Hbs::Render::Fresnel");
                        break;
                    case Shader::RMStadium_CameraPos:
                        globalArgs.emplace_back("__COORD4:::RMStadium::CameraPos");
                        break;
                    case Shader::SubSurfFactor2:
                        globalArgs.emplace_back("__COORD4:::SGR:SubSurf:SubSurfFactor");
                        break;
                    case Shader::SpecFactor:
                        globalArgs.emplace_back("__COORD4:::SGR:SubSurf:SpecFactor");
                        break;
                    case Shader::StarBall_MatrixMVP:
                        globalArgs.emplace_back("__const MATRIX4:::RM::StarBall::MatrixMVP");
                        break;
                    case Shader::StarBall_MatrixMV:
                        globalArgs.emplace_back("__const MATRIX4:::RM::StarBall::MatrixMV");
                        break;
                    case Shader::StarBall_MatrixMVR:
                        globalArgs.emplace_back("__const MATRIX4:::RM::StarBall::MatrixMVR");
                        break;
                    case Shader::StarBall_Params0:
                        globalArgs.emplace_back("__COORD4:::RM::StarBall::Params0");
                        break;
                    case Shader::StarBall_Params1:
                        globalArgs.emplace_back("__COORD4:::RM::StarBall::Params1");
                        break;
                    case Shader::PlaneEquation:
                        globalArgs.emplace_back("__COORD4:::RM::Globe::PlaneEquation");
                        break;
                    case Shader::FresnelColour:
                        globalArgs.emplace_back("__COORD4:::RM::Globe::FresnelColour");
                        break;
                    case Shader::Irradiance:
                        globalArgs.emplace_back("__COORD4:::gpIrradiance");
                        break;
                    case Shader::FaceIrradiance:
                        globalArgs.emplace_back("__COORD4:::gpFaceIrradiance");
                        break;
                    case Shader::ColourScaleFactor:
                        globalArgs.emplace_back("__COORD4:::ColourScaleFactor");
                        break;
                    case Shader::EyeVector:
                        globalArgs.emplace_back("__COORD4:::Hbs::Render::EyeVector");
                        break;
                    case Shader::Contrast:
                        globalArgs.emplace_back("__COORD4:::Hbs::Render::MowPattern::Contrast");
                        break;
                    case Shader::GlobalDiffuse:
                        globalArgs.emplace_back("__COORD4:::GlobalDiffuse");
                        break;
                    case Shader::TextureProjectionMatrix:
                        globalArgs.emplace_back("__const MATRIX4:::TextureProjectionMatrix");
                        break;
                    case Shader::LocalLightDirection:
                        globalArgs.emplace_back("__COORD3:::LocalLightDirection");
                        break;
                    case Shader::UVOffset0:
                    {
                        Vector4D uvOffset0;
                        globalArgs.emplace_back(modifiables.GetArg("Coordinate4::" + n.name + "::UVOffset0", bufData, uvOffset0, true, false));
                    }
                    break;
                    case Shader::UVOffset1:
                    {
                        Vector4D uvOffset1;
                        globalArgs.emplace_back(modifiables.GetArg("Coordinate4::" + n.name + "::UVOffset1", bufData, uvOffset1, true, false));
                    }
                    break;
                    case Shader::XFade:
                    {
                        Vector4D xFade;
                        globalArgs.emplace_back(modifiables.GetArg("Coordinate4::" + n.name + "::XFade", bufData, xFade, true, false));
                    }
                    break;
                    case Shader::Light:
                    {
                        Light lightBlock;
                        globalArgs.emplace_back(modifiables.GetArg("Light::LightBlock", bufData, lightBlock, true, true));
                    }
                    break;
                    case Shader::IrradLight:
                    {
                        IrradLight irradBlock;
                        globalArgs.emplace_back(modifiables.GetArg("IrradLight::IrradBlock", bufData, irradBlock, true, true));
                    }
                    break;
                    case Shader::RuntimeGeoPrimState:
                    case Shader::RuntimeGeoPrimState2:
                    {
                        string geoPrimStateFormat = "__EAGL::GeoPrimState:::RUNTIME_ALLOC::UID=" + to_string(uid) + ";" + arg.format + "SetPrimitiveType=EAGL::" + (options().tristrip ? "PT_TRIANGLESTRIP" : "PT_TRIANGLELIST");
                        globalArgs.emplace_back(modifiables.GetArg((arg.type == Shader::RuntimeGeoPrimState ? "GeoPrimState::State" : "State::GeoPrimState"), geoPrimStateFormat, sizeof(GeoPrimState)));
                    }
                    break;
                    case Shader::CrowdState:
                        globalArgs.emplace_back(modifiables.GetArg("State::State", "__EAGL::GeoPrimState:::crowd_state", sizeof(GeoPrimState)));
                        break;
                    case Shader::LineNoDepthWriteState:
                        globalArgs.emplace_back(modifiables.GetArg("GeoPrimState::State", "__EAGL::GeoPrimState:::line_no_depth_write_state", sizeof(GeoPrimState)));
                        break;
                    case Shader::GlowerState:
                        globalArgs.emplace_back(modifiables.GetArg("State::State", "__EAGL::GeoPrimState:::glower_state", sizeof(GeoPrimState)));
                        break;
                    case Shader::ProjectiveShadow2State:
                        globalArgs.emplace_back(modifiables.GetArg("State::State", "__EAGL::GeoPrimState:::projective_shadow2_state", sizeof(GeoPrimState)));
                        break;
                    case Shader::FlagLightBlock:
                        globalArgs.emplace_back(modifiables.GetArg("Light::LightBlock", "__EAGL::LightBlock:::FlagLightBlock", sizeof(Light)));
                        break;
                    case Shader::Sampler0:
                    case Shader::Sampler1:
                    case Shader::Sampler2:
                    case Shader::Sampler3:
                    case Shader::Sampler0Local:
                    case Shader::Sampler1Local:
                    case Shader::Sampler2Local:
                    case Shader::Sampler3Local:
                    {
                        unsigned int s = SamplerIndex(arg.type);
                        if (tex[s].isGlobal)
                            globalArgs.emplace_back("__EAGL::TAR:::" + tex[s].name);
                        else {
                            if (texAlreadyPresent[s]) {
                                if (IsGlobalSampler(arg.type))
                                    globalArgs.emplace_back(tex[s].GetRuntimeConstructorLine(uid, numVariations));
                                else
                                    globalArgs.emplace_back(tex[s].offset);
                            }
                            else {
                                if (IsGlobalSampler(arg.type)) {
                                    globalArgs.emplace_back(tex[s].GetRuntimeConstructorLine(uid, numVariations));
                                    tex[s].runtimeConstructed = true;
                                }
                                else {
                                    tex[s].runtimeConstructed = false;
                                    bufData.Align(16);
                                    tex[s].offset = bufData.Position();
                                    globalArgs.emplace_back(bufData.Position(), 1);
                                    symbols.emplace_back("__EAGL::TAR:::tar_" + tex[s].name + "_" + to_string(Hash(modelName + "_" + tex[s].name)), bufData.Position());
                                    TAR tar;
                                    strncpy(tar.tag, tex[s].name.c_str(), 4);
                                    bufData.Put(tar);
                                }
                                textures[ToLower(tex[s].name)] = tex[s];
                            }
                        }
                    }
                    break;
                    case Shader::GeoPrimState:
                    {
                        GeoPrimState state;
                        state.nPrimitiveType = options().tristrip ? 5 : 4;
                        globalArgs.emplace_back(modifiables.GetArg("State::State", bufData, state, true));
                    }
                    break;
                    case Shader::UVOffset:
                    {
                        Vector4D uvOffset;
                        globalArgs.emplace_back(modifiables.GetArg("Coordinate4::UVOffset", bufData, uvOffset, true, false));
                    }
                    break;
                    case Shader::UVMatrix:
                    {
                        Matrix4x4 uvMatrix;
                        //SetAt<unsigned int>(&uvMatrix, 0, 0x01000000);
                        globalArgs.emplace_back(modifiables.GetArg("Matrix::UVMatrix", bufData, uvMatrix, true, false));
                    }
                    break;
                    case Shader::ColourScale:
                    {
                        Vector4D_int colourScale = { -1, -1, -1, -1 };
                        globalArgs.emplace_back(modifiables.GetArg("Coordinate4::ColourScale", bufData, colourScale, true, false));
                    }
                    break;
                    case Shader::UVOffset_Layer:
                    {
                        Vector4D uvOffset;
                        globalArgs.emplace_back(modifiables.GetArg("Coordinate4::" + n.name + "::UVOffset", bufData, uvOffset, true, false));
                    }
                    break;
                    case Shader::UVMatrix_Layer:
                    {
                        Matrix4x4 uvMatrix;
                        globalArgs.emplace_back(modifiables.GetArg("Matrix::" + n.name + "::UVMatrix", bufData, uvMatrix, true, false));
                    }
                    break;
                    case Shader::InstanceColour:
                    {
                        Vector4D instanceColour;
                        globalArgs.emplace_back(modifiables.GetArg("Coordinate4::" + n.name + "::InstanceColour", bufData, instanceColour, true, false));
                    }
                    break;
                    }
                }
                unsigned int geoPrimDataBufferOffset = bufData.Position();
                symbols.emplace_back("__geoprimdatabuffer_" + to_string(meshCounter) + "_" + modelName + ".tagged", bufData.Position());
                bufData.Put(ZERO);
                unsigned int codeBlockOffset = bufData.Position();
                if (shader->commands.size() && shader->commands.size() > shader->numTechniques) {
                    unsigned int codeSize = shader->commands.size() / shader->numTechniques;
                    for (unsigned int c = 0; c < shader->commands.size(); c++) {
                        unsigned int numArgs = shader->commands[c].arguments.size();
                        unsigned int commandSize = numArgs + 1;
                        bufData.Put(unsigned short(commandSize));
                        bufData.Put(unsigned short(shader->commands[c].id));
                        for (unsigned int a = 0; a < numArgs; a++) {
                            int arg = shader->commands[c].arguments[a];
                            if (arg == Shader::VertexData) {
                                relocations[""].push_back(bufData.Position());
                                bufData.Put(vertexBufferOffset);
                            }
                            else if (arg == Shader::VertexCount)
                                bufData.Put(numVertices);
                            else if (arg == Shader::VertexBufferIndex)
                                bufData.Put(meshCounter * numVariations);
                            else if (arg == Shader::VariationsCount) {
                                if (hasMorph)
                                    bufData.Put(numVariations);
                                else
                                    bufData.Put(-int(numVariations));
                            }
                            else if (arg == Shader::IndexData) {
                                relocations[""].push_back(bufData.Position());
                                bufData.Put(indexBufferOffset);
                            }
                            else if (arg == Shader::IndexCount)
                                bufData.Put(numIndices);
                            else if (arg == Shader::Sampler0Size)
                                bufData.Put(unsigned int(tex[0].isGlobal ? 0 : 48));
                            else if (arg == Shader::Sampler1Size)
                                bufData.Put(unsigned int(tex[1].isGlobal ? 0 : 48));
                            else if (arg == Shader::Sampler2Size)
                                bufData.Put(unsigned int(tex[2].isGlobal ? 0 : 48));
                            else if (arg == Shader::Sampler3Size)
                                bufData.Put(unsigned int(tex[3].isGlobal ? 0 : 48));
                            else if (arg == Shader::VertexWeights3Bones)
                                bufData.Put(vertexWeightsNumBones3);
                            else if (arg == Shader::VertexWeights2Bones)
                                bufData.Put(vertexWeightsNumBones2);
                            else if (arg == Shader::VertexWeights1Bone)
                                bufData.Put(vertexWeightsNumBones1);
                            else
                                bufData.Put(arg);
                        }
                        if (((c + 1) % codeSize) == 0)
                            bufData.Put(ZERO);
                    }
                }
                else {
                    for (unsigned int t = 0; t < shader->numTechniques; t++)
                        bufData.Put(ZERO);
                }
                bufData.Put(ZERO);
                // ShaderName
                unsigned int shaderNameOffset = 0;
                if (target->Version() >= 3) {
                    shaderNameOffset = bufData.Position();
                    bufData.Put(shaderName);
                }
                // RenderMethod
                bufData.Align(16);
                unsigned int renderMethodOffset = bufData.Position();
                symbols.emplace_back("__RenderMethod:::__GPRenderMethod_" + modelName + ".tagged_" + to_string(meshCounter), bufData.Position());
                relocations[""].push_back(bufData.Position());
                bufData.Put(codeBlockOffset);
                bufData.Put(ZERO);
                relocations[shaderName + "__EAGLMicroCode"].push_back(bufData.Position());
                bufData.Put(ZERO);
                bufData.Put(ZERO);
                relocations["ParentRM_" + shaderName].push_back(bufData.Position());
                bufData.Put(ZERO);
                bufData.Put(ZERO);
                bufData.Put(ZERO);
                bufData.Put(MINONE);
                bufData.Put(ZERO);
                relocations[""].push_back(bufData.Position());
                bufData.Put(geoPrimDataBufferOffset);
                bufData.Put(shader->ComputationCommandIndex());
                relocations[""].push_back(bufData.Position());
                if (target->Version() >= 3) {
                    bufData.Put(shaderNameOffset);
                    bufData.Put(ZERO);
                }
                static unsigned char EASig[] = { 0xEA, 0xEF, 0xCD, 0xAB };
                bufData.Put(EASig, std::size(EASig));
                // RenderDescriptor
                n.renderDescriptorsOffsets.push_back(bufData.Position());
                relocations[""].push_back(bufData.Position());
                bufData.Put(renderMethodOffset);
                for (auto const &ga : globalArgs) {
                    bufData.Put(ga.count);
                    if (!ga.name.empty()) {
                        relocations[ga.name].push_back(bufData.Position());
                        bufData.Put(ZERO);
                    }
                    else {
                        relocations[""].push_back(bufData.Position());
                        bufData.Put(ga.offset);
                    }
                }
                meshCounter++;
            }
        }
        if (options().bboxScale != 0.0f && options().bboxScale != 1.0f)
            ScaleBoundBox(n.boundMin, n.boundMax);
        nodeCounter++;
    }
    // BBOX
    //Error("%d %d", bufData.Position(), bufData.Capacity());
    bufData.Align(16);
    symbols.emplace_back("__BBOX:::" + modelName + ".tagged", bufData.Position());
    aiVector3D boundMin = { 0.0f, 0.0f, 0.0f };
    aiVector3D boundMax = { 0.0f, 0.0f, 0.0f };
    bool anyNodeProcessed = false;
    for (auto const &n : nodes) {
        if (n.anyVertexProcessed) {
            ProcessBoundBox(boundMin, boundMax, anyNodeProcessed, n.boundMin);
            ProcessBoundBox(boundMin, boundMax, anyNodeProcessed, n.boundMax);
        }
    }
    bufData.Put(boundMin);
    bufData.Put(boundMax);
    // Skeleton
    if (hasSkeleton) {
        vector<BoneInfo> vecBones;
        if (globalVars().customBones.empty()) {
            if (!bones.empty()) {
                vecBones.resize(bones.size());
                for (auto const &[name, info] : bones)
                    vecBones[info.index] = info;
            }
        }
        else {
            vecBones.resize(globalVars().customBones.size());
            for (auto const &[name, index] : globalVars().customBones) {
                vecBones[index].name = name;
                vecBones[index].index = index;
                vecBones[index].bone = nullptr;
            }
        }
        if (ToLower(options().skeletonData.string()) != "none") {
            bufData.Align(16);
            // bones
            for (auto const &b : vecBones) {
                symbols.emplace_back("__Bone:::" + modelName + "." + b.name, bufData.Position());
                bufData.Put(unsigned int(b.index));
                bufData.Put(ZERO);
                bufData.Put(ZERO);
                bufData.Put(ZERO);
            }
            // skeleton
            symbols.emplace_back("__Skeleton:::" + modelName, bufData.Position());
            if (options().skeletonData.empty()) {
                bufData.Put(unsigned short(ANIM_VERSION));
                if (ANIM_VERSION == 0xDB15) {
                    bufData.Put(unsigned short(690));
                    bufData.Put(unsigned short(ZERO));
                }
                else if (ANIM_VERSION == 0x0504) {
                    bufData.Put(unsigned short(660));
                    bufData.Put(unsigned short(ZERO));
                }
                else {
                    bufData.Put(unsigned short(510));
                    bufData.Put(unsigned short(ANIM_VERSION));
                }
                bufData.Put(unsigned short(ZERO));
                bufData.Put(vecBones.size());
                bufData.Put(ZERO);
                for (auto const &b : vecBones) {
                    auto bnode = b.bone->mNode;
                    aiVector3D scaling, position;
                    aiQuaternion rotation;
                    bnode->mTransformation.Decompose(scaling, rotation, position);
                    bufData.Put(scaling);
                    int parentId = -1;
                    if (bnode->mParent) {
                        auto pit = bones.find(bnode->mParent->mName.C_Str());
                        if (pit != bones.end())
                            parentId = (*pit).second.index;
                    }
                    bufData.Put(parentId);
                    bufData.Put(rotation.x);
                    bufData.Put(rotation.y);
                    bufData.Put(rotation.z);
                    bufData.Put(rotation.w);
                    bufData.Put(position);
                    bufData.Put(ZERO);
                    auto mat = b.bone->mOffsetMatrix;
                    mat.Transpose();
                    bufData.Put(mat);
                }
            }
            else {
                FILE *skelFile = nullptr;
                _wfopen_s(&skelFile, options().skeletonData.c_str(), L"rb");
                if (skelFile) {
                    fseek(skelFile, 0, SEEK_END);
                    auto fileSize = ftell(skelFile);
                    fseek(skelFile, 0, SEEK_SET);
                    unsigned char *skelData = new unsigned char[fileSize];
                    fread(skelData, fileSize, 1, skelFile);
                    fclose(skelFile);
                    bufData.Put(skelData, fileSize);
                    delete[] skelData;
                }
                else
                    throw runtime_error("Unable to open skeleton file");
            }
        }
    }
    static unsigned char TTARSig[] = { 'T', 'T', 'A', 'R' };
    bufData.Put(TTARSig, std::size(TTARSig));
    // ModelLayers
    unsigned int modelLayersOffset = bufData.Position();
    if (target->Version() >= 3) {
        bufData.Put(0);
        for (auto const &n : nodes) {
            bufData.Put(n.renderDescriptorsOffsets.size());
            for (auto const &d : n.renderDescriptorsOffsets) {
                relocations[""].push_back(bufData.Position());
                bufData.Put(d);
            }
        }
    }
    else {
        bufData.Put(unsigned int(0xA0000000));
        bufData.Put(ZERO);
        unsigned int numModelLayersBlocks = nodes.size() * 2;
        for (auto const &n : nodes)
            numModelLayersBlocks += n.renderDescriptorsOffsets.size() * 2;
        bufData.Put(numModelLayersBlocks);
        for (auto const &n : nodes) {
            bufData.Put(unsigned int(0xA0000001));
            bufData.Put(ZERO);
            bufData.Put(n.renderDescriptorsOffsets.size() * 2);
            for (auto const &d : n.renderDescriptorsOffsets) {
                bufData.Put(unsigned int(0xA000FFFF));
                relocations[""].push_back(bufData.Position());
                bufData.Put(d);
            }
        }
        bufData.Put(ZERO);
        bufData.Align(16);
    }
    unsigned int modelNameOffset = bufData.Position();
    bufData.Put(modelName);
    // LayersNames
    vector<unsigned int> layersNamesOffsets;
    for (auto const &n : nodes) {
        layersNamesOffsets.push_back(bufData.Position());
        bufData.Put(n.name);
    }
    // layersNamesOffsets
    bufData.Align(4);
    unsigned int layersNamesOffsetsOffset = bufData.Position();
    for (auto const &ln : layersNamesOffsets) {
        relocations[""].push_back(bufData.Position());
        bufData.Put(ln);
    }
    vector<unsigned int> modifiablesNameOffsets;
    if (!modifiables.vec.empty()) {
        std::sort(modifiables.vec.begin(), modifiables.vec.end(), [](ModData const &a, ModData const &b) {
            if (a.name < b.name)
                return true;
            if (b.name < a.name)
                return false;
            return a.offset <= b.offset;
        });
        // Modifiable data names
        string prevName;
        unsigned int prevOffset = 0;
        bool first = true;
        for (auto const &m : modifiables.vec) {
            if (first || m.name != prevName) {
                bufData.Align(4);
                prevOffset = bufData.Position();
                modifiablesNameOffsets.push_back(bufData.Position());
                bufData.Put(m.name);
                first = false;
                prevName = m.name;
            }
            else
                modifiablesNameOffsets.push_back(prevOffset);
        }
    }

    bufData.Align(16);
    unsigned int modifiablesOffset = bufData.Position();
    if (!modifiables.vec.empty()) {
        // Modifiable datas
        unsigned int m = 0;
        for (auto const &mod : modifiables.vec) {
            relocations[""].push_back(bufData.Position());
            bufData.Put(modifiablesNameOffsets[m]);
            if (!mod.runtimeConstructorLine.empty())
                bufData.Put(unsigned short(mod.runtimeSize));
            else
                bufData.Put(unsigned short(mod.data.size()));
            bufData.Put(unsigned short(1));
            bufData.Put(unsigned int(1));
            if (!mod.runtimeConstructorLine.empty()) {
                relocations[mod.runtimeConstructorLine].push_back(bufData.Position());
                bufData.Put(ZERO);
            }
            else {
                relocations[""].push_back(bufData.Position());
                bufData.Put(mod.offset);
            }
            m++;
        }
    }
    // Texture names
    vector<unsigned int> textureNamesOffsets;
    for (auto const &[id, t] : textures) {
        bufData.Align(4);
        textureNamesOffsets.push_back(bufData.Position());
        bufData.Put(t.name);
    }
    // Morph vertices info
    bufData.Align(4);
    unsigned int modelMorphVerticesInfoOffset = bufData.Position();
    bufData.Align(16);
    // TODO: Morph data
    bufData.Put(ZERO);
    bufData.Put(ZERO);

    for (unsigned int v = 0; v < numVariations; v++) {
        // Model textures
        unsigned int texturesOffset = bufData.Position();
        unsigned int texIndex = 0;
        for (auto const &[id, t] : textures) {
            relocations[""].push_back(bufData.Position());
            bufData.Put(textureNamesOffsets[texIndex]);
            bufData.Put(ONE);
            if (t.IsRuntimeConstructed()) {
                relocations[t.GetRuntimeConstructorLine(uid, numVariations)].push_back(bufData.Position());
                bufData.Put(ZERO);
            }
            else {
                relocations[""].push_back(bufData.Position());
                bufData.Put(t.offset);
            }
            if (target->Version() >= 4) {
                bufData.Put(unsigned short(1));
                bufData.Put(unsigned short(1));
            }
            texIndex++;
        }
        bufData.Put(ZERO);
        // Model layers states
        unsigned int modelLayersStatesOffset = bufData.Position();
        bufData.Put(options().layerFlags);
        for (auto const &n : nodes) {
            bufData.Put<unsigned short>(ONE); // TODO
            bufData.Put<unsigned short>(ONE);
        }
        // Model layer boundings
        for (auto const &n : nodes) {
            bufData.Put(n.boundMin);
            bufData.Put(FONE);
            bufData.Put(n.boundMax);
            bufData.Put(FONE);
            aiVector3D nodeVecCenter = n.boundMax - n.boundMin;
            float nodeBoundRadius = nodeVecCenter.Length() / 2;
            nodeVecCenter /= 2;
            nodeVecCenter += n.boundMin;
            bufData.Put(nodeVecCenter);
            bufData.Put(nodeBoundRadius);
        }
        // Model
        auto modelSymbolName = "__Model:::" + modelName + ".tagged";
        if (v != 0)
            modelSymbolName += ".variation" + to_string(v);
        symbols.emplace_back(modelSymbolName, bufData.Position());
        relocations[""].push_back(bufData.Position());
        bufData.Put(modifiablesOffset);
        bufData.Put(modifiables.vec.size());
        bufData.Put(numVariations);
        bufData.Put(identityMatrix);
        aiVector3D modelVec2;
        modelVec2.x = max(fabs(boundMin.x), fabs(boundMax.x));
        modelVec2.y = max(fabs(boundMin.y), fabs(boundMax.y));
        modelVec2.z = max(fabs(boundMin.z), fabs(boundMax.z));
        modelVec2 *= 2;
        aiVector3D modelVec1 = modelVec2;
        modelVec1 *= 2;
        bufData.Put(modelVec1);
        bufData.Put(FONE);
        bufData.Put(modelVec2);
        bufData.Put(FZERO);
        bufData.Put(boundMin);
        bufData.Put(FONE);
        bufData.Put(boundMax);
        bufData.Put(FONE);
        aiVector3D modelVecCenter = boundMax - boundMin;
        float boundRadius = modelVecCenter.Length() / 2;
        modelVecCenter /= 2;
        modelVecCenter += boundMin;
        bufData.Put(modelVecCenter);
        bufData.Put(boundRadius);
        bufData.Put(nodes.size());
        relocations[""].push_back(bufData.Position());
        bufData.Put(layersNamesOffsetsOffset);
        bufData.Put(ZERO);
        bufData.Put(ZERO);
        bufData.Put(ZERO);
        bufData.Put(ZERO);
        relocations[""].push_back(bufData.Position());
        bufData.Put(modelNameOffset);
        relocations[""].push_back(bufData.Position());
        bufData.Put(modelMorphVerticesInfoOffset);
        relocations[""].push_back(bufData.Position());
        bufData.Put(texturesOffset);
        bufData.Put(v);
        relocations[""].push_back(bufData.Position());
        bufData.Put(modelLayersStatesOffset);
        bufData.Put(ONE);
        relocations[""].push_back(bufData.Position());
        bufData.Put(modelLayersOffset);
        bufData.Put(ZERO);
        bufData.Put(ZERO);
        bufData.Put(hasSkeleton ? ANIM_VERSION : ZERO);
        bufData.Put(ZERO);
    }
    bufData.Align(16);
    if (options().embeddedTextures) { // TODO
        for (auto const &[id, t] : textures) {
            // load texture from disk
            // convert to fsh shape
        //    symbols.emplace_back("__SHAPE::shape_" + t.name, bufData.Position());
            // bufData.Put(shapeData, shapeDataSize);
        //    bufData.Align(16);
        }
    }

    vector<unsigned int> sectionOffsets;
    vector<unsigned int> sectionNamesOffets;

    BinaryBuffer bufSectionNames(48);
    static char const *sectionNames[] = { "", ".data", ".shstrtab", ".strtab", ".symtab", ".rel.data" };
    for (auto const &sn : sectionNames) {
        sectionNamesOffets.push_back(bufSectionNames.Position());
        bufSectionNames.Put(sn);
    }
    bool noMetadata = options().noMetadata || options().conformant;
    if (!noMetadata) {
        sectionNamesOffets.push_back(bufSectionNames.Position());
        bufSectionNames.Put(".comment");
    }
    bufSectionNames.Align(16);

    // symbols and relocations
    BinaryBuffer bufSymbols;
    BinaryBuffer bufSymbolNames;
    BinaryBuffer bufRelocations;
    BinaryBuffer bufMetadata;

    bufSymbols.Put(Elf32_Sym(0, 0, 0, 0x00, 0, 0));
    bufSymbolNames.Put("");
    bufSymbols.Put(Elf32_Sym(0, 0, 0, 0x03, 0, 1));
    bufSymbolNames.Put("");
    
    if (!options().conformant) {
        bufSymbols.Put(Elf32_Sym(bufSymbolNames.Position(), 0, 4, 0x21, 0, 1));
        bufSymbolNames.Put(string("__OTOOLS_VERSION:::OTOOLS_VERSION-") + OTOOLS_VERSION);
    }
    bufSymbols.Put(Elf32_Sym(bufSymbolNames.Position(), 0, 4, 0x21, 0, 1));
    if (target->Version() >= 3)
        bufSymbolNames.Put("__EAGL_TOOLLIB_VERSION:::EAGL_TOOLLIB_VERSION-" + to_string(target->Version()));
    else
        bufSymbolNames.Put("__LASTCHANGELIST:::4.06.01 Tue_June_25th_2002");
    
    for (auto const &s : symbols) {
        bufSymbols.Put(Elf32_Sym(bufSymbolNames.Position(), s.offset, 4, 0x11, 0, 1));
        bufSymbolNames.Put(s.name);
    }

    vector<Elf32_Rel> elfRel;
    unsigned int symbolIndex = (options().conformant? 3 : 4) + symbols.size();
    for (auto const &[n, v] : relocations) {
        if (!n.empty()) {
            bufSymbols.Put(Elf32_Sym(bufSymbolNames.Position(), 0, 0, 0x10, 0, 0));
            bufSymbolNames.Put(n);
            for (auto const &r : v)
                elfRel.push_back(Elf32_Rel(r, 2, symbolIndex));
            symbolIndex++;
        }
        else {
            for (auto const &r : v)
                elfRel.push_back(Elf32_Rel(r, 2, 1));
        }
    }
    std::sort(elfRel.begin(), elfRel.end(), [](Elf32_Rel const &a, Elf32_Rel const &b) {
        return a.r_offset <= b.r_offset;
    });

    for (auto const &r : elfRel)
        bufRelocations.Put(r);

    bufSymbols.Align(16);
    bufSymbolNames.Align(16);
    bufRelocations.Align(16);

    if (!noMetadata) {
        string metadata;
        auto t = std::time(nullptr);
        auto tm = *std::localtime(&t);
        std::ostringstream timess;
        timess << std::put_time(&tm, "%d-%b-%Y %H:%M:%S");
        metadata += "<Metadata>";
        metadata += "<Tools>otools</Tools>";
        metadata += "<ToolsVersion>" + string(OTOOLS_VERSION) + "</ToolsVersion>";
        metadata += "<TimeStamp>" + timess.str() + "</TimeStamp>";
        //metadata += "<Author>" + "</Author>";
        metadata += "<OriginalFileName>" + out.filename().string() + "</OriginalFileName>";
        metadata += "<SourceFile>" + in.string() + "</SourceFile>";
        metadata += "</Metadata>";
        bufMetadata.Put(metadata);
        bufMetadata.Align(16);
    }

    string versionMessage;
    
    if (!options().conformant) {
        versionMessage = "This file was generated with otools version ";
        versionMessage += OTOOLS_VERSION;
    }

    BinaryBuffer bufElf;
    Elf32_Ehdr header;
    Memory_Zero(header);
    static unsigned char elfSig[] = { 0x7F, 0x45, 0x4C, 0x46, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
    unsigned int headerBlockSize = GetAligned(sizeof(Elf32_Ehdr), 16);
    Memory_Copy(&header.e_ident, elfSig, sizeof(elfSig));
    header.e_type = ET_REL;
    header.e_machine = EM_MIPS;
    header.e_version = EV_CURRENT;
    header.e_entry = 0;
    header.e_phoff = 0;
    header.e_shoff = headerBlockSize + (options().conformant? 0 : GetAligned(versionMessage.size(), 16))
        + bufData.Size() + bufSectionNames.Size() + bufSymbolNames.Size() 
        + bufSymbols.Size() + bufRelocations.Size() + (noMetadata ? 0 : bufMetadata.Size());
    header.e_flags = 0x20924000;
    header.e_ehsize = sizeof(Elf32_Ehdr);
    header.e_phentsize = 0;
    header.e_phnum = 0;
    header.e_shentsize = sizeof(Elf32_Shdr);
    header.e_shnum = noMetadata ? 6 : 7;
    header.e_shstrndx = 2;
    sectionOffsets.push_back(0);
    bufElf.Put(header);
    bufElf.Align(16);
    if (!options().conformant) {
        bufElf.Put(versionMessage);
        bufElf.Align(16);
    }
    sectionOffsets.push_back(bufElf.Position());
    bufElf.Put(bufData);
    sectionOffsets.push_back(bufElf.Position());
    bufElf.Put(bufSectionNames);
    sectionOffsets.push_back(bufElf.Position());
    bufElf.Put(bufSymbolNames);
    sectionOffsets.push_back(bufElf.Position());
    bufElf.Put(bufSymbols);
    sectionOffsets.push_back(bufElf.Position());
    bufElf.Put(bufRelocations);
    if (!noMetadata) {
        sectionOffsets.push_back(bufElf.Position());
        bufElf.Put(bufMetadata);
    }
    bufElf.Put(Elf32_Shdr(sectionNamesOffets[0], SHT_NULL, 0, 0, sectionOffsets[0], 0, 0, 0, 0, 0));
    bufElf.Put(Elf32_Shdr(sectionNamesOffets[1], SHT_PROGBITS, 0x3, 0, sectionOffsets[1], bufData.Size(), 0, 0, 16, 0));
    bufElf.Put(Elf32_Shdr(sectionNamesOffets[2], SHT_STRTAB, 0, 0, sectionOffsets[2], bufSectionNames.Size(), 0, 0, 1, 0));
    bufElf.Put(Elf32_Shdr(sectionNamesOffets[3], SHT_STRTAB, 0, 0, sectionOffsets[3], bufSymbolNames.Size(), 0, 0, 1, 0));
    bufElf.Put(Elf32_Shdr(sectionNamesOffets[4], SHT_SYMTAB, 0, 0, sectionOffsets[4], bufSymbols.Size(), 3, 2, 4, sizeof(Elf32_Sym)));
    bufElf.Put(Elf32_Shdr(sectionNamesOffets[5], SHT_REL, 0, 0, sectionOffsets[5], bufRelocations.Size(), 4, 1, 4, sizeof(Elf32_Rel)));
    if (!noMetadata)
        bufElf.Put(Elf32_Shdr(sectionNamesOffets[6], SHT_PROGBITS, 0, 0, sectionOffsets[6], bufMetadata.Size(), 0, 0, 1, 0));
    if (!isOrd) {
        unsigned int pad = 0;
        if (options().pad > 0)
            pad = options().pad;
        else if (options().hd)
            pad = 1'048'576;
        if (pad > 0 && bufElf.Size() < pad) {
            unsigned int numPaddingBytes = pad - bufElf.Size();
            for (unsigned int i = 0; i < numPaddingBytes; i++)
                bufElf.Put<unsigned char>(0);
        }
        bufElf.WriteToFile(out);
    }
    else {
        bufElf.WriteToFile(out, 0, sectionOffsets[2]);
        auto orlPath = out;
        orlPath.replace_extension(outExt == ".ORD" ? ".ORL" : ".orl");
        bufElf.WriteToFile(orlPath, sectionOffsets[2], bufElf.Size() - sectionOffsets[2]);
    }

    if (options().writeFsh) {
        auto modelNameLow = ToLower(modelName);
        if (options().head &&
            (
                modelNameLow.starts_with("m228__") ||
                modelNameLow.starts_with("player____model60__") ||
                modelNameLow.starts_with("player____m228__")
            )
            &&
            (
                targetName == "FIFA03" ||
                targetName == "FIFA04" ||
                targetName == "FIFA05" ||
                targetName == "FIFA06" ||
                targetName == "FIFA07" ||
                targetName == "FIFA08" ||
                targetName == "FIFA09" ||
                targetName == "FIFA10" ||
                targetName == "EURO04" ||
                targetName == "EURO08" ||
                targetName == "WC06" ||
                targetName == "CL0405" ||
                targetName == "CL0607" ||
                targetName == "FM13"
            )
            )
        {
            unsigned int playerId = 0;
            unsigned int numChars = 6;
            if (modelNameLow.starts_with("player____model60__"))
                numChars = 19;
            else if (modelNameLow.starts_with("player____m228__"))
                numChars = 16;
            if (sscanf(modelName.substr(numChars).c_str(), "%d", &playerId) == 1) {
                string playerIdStr = to_string(playerId);

                // writing head texture
                string headTexName;
                map<string, TextureToAdd> fshTextures1;

                unsigned int fshFormat32Bit = options().hasFshFormat ? options().fshFormat : D3DFMT_DXT1;
                unsigned int fshFormat32BitAlpha = options().hasFshFormat ? options().fshFormat : D3DFMT_DXT5;
                unsigned int fshFormat16Bit = options().hasFshFormat ? options().fshFormat : D3DFMT_R5G6B5;
                unsigned int fshFormat16BitAlpha = options().hasFshFormat ? options().fshFormat : D3DFMT_A4R4G4B4;
                
                if (targetName == "FIFA03")
                    headTexName = "PlayerTexObj.texobj11__texture12__" + playerIdStr + "_0_0.fsh";
                else if (targetName == "FIFA04" || targetName == "FIFA05" || targetName == "EURO04" || targetName == "CL0405")
                    headTexName = "playertexobj.texobj11__texture12__" + playerIdStr + "_0_0.fsh";
                else if (targetName == "FIFA06" || targetName == "FIFA07" || targetName == "FIFA08" || targetName == "WC06" || targetName == "CL0607" || targetName == "EURO08")
                    headTexName = "t21__" + playerIdStr + "_0_0.fsh";
                else
                    headTexName = "t21__" + playerIdStr + "_0_0_0_0.fsh";

                if (targetName == "CL0405") {
                    if (options().hd) {
                        fshTextures1["glos"] = { "glos", "glos@" + playerIdStr, fshFormat32Bit, 99 };
                        fshTextures1["face"] = { "face", "face@" + playerIdStr, fshFormat32Bit, 99 };
                    }
                    else {
                        fshTextures1["glos"] = { "glos", "glos@" + playerIdStr, fshFormat16Bit, 1 };
                        fshTextures1["face"] = { "face", "face@" + playerIdStr, fshFormat32Bit, 1 };
                    }
                }
                else {
                    if (options().hd)
                        fshTextures1["tp01"] = { "tp01", "tp01@" + playerIdStr, fshFormat32Bit, 99 };
                    else {
                        if (targetName == "FIFA03")
                            fshTextures1["tp01"] = { "tp01", "tp01@" + playerIdStr, fshFormat32Bit, 7 };
                        else if (targetName == "FIFA10")
                            fshTextures1["tp01"] = { "tp01", "tp01@" + playerIdStr, fshFormat32Bit, 8 };
                        else
                            fshTextures1["tp01"] = { "tp01", "tp01@" + playerIdStr, fshFormat32Bit, 1 };
                    }
                }
                WriteFsh(out.parent_path() / headTexName, in.parent_path(), fshTextures1);

                // writing hair texture
                if (targetName != "FIFA09" && targetName != "FIFA10" && targetName != "FM13") {
                    string hairTexName;
                    if (targetName == "FIFA03")
                        hairTexName = "PlayerTexObj.texobj11__texture14__0_" + playerIdStr + "_0.fsh";
                    else if (targetName == "FIFA04" || targetName == "FIFA05" || targetName == "EURO04" || targetName == "CL0405")
                        hairTexName = "playertexobj.texobj11__texture14__0_" + playerIdStr + "_0.fsh";
                    else if (targetName == "FIFA06" || targetName == "FIFA07" || targetName == "FIFA08" || targetName == "WC06" || targetName == "CL0607" || targetName == "EURO08")
                        hairTexName = "t22__" + playerIdStr + "_0.fsh";
                    
                    map<string, TextureToAdd> fshTextures2;
                    if (options().hd)
                        fshTextures2["tp02"] = { "tp02", "tp02@" + playerIdStr, fshFormat32BitAlpha, 99 };
                    else {
                        int hairTexLevels = 99;
                        unsigned int hairTexFormat = fshFormat16BitAlpha;
                        if (targetName == "FIFA08" || targetName == "EURO08")
                            hairTexFormat = fshFormat32Bit;
                        fshTextures2["tp02"] = { "tp02", "tp02@" + playerIdStr, hairTexFormat, 7 };
                    }
                    WriteFsh(out.parent_path() / hairTexName, in.parent_path(), fshTextures2);
                }
            }
        }
        else {
            if (!options().stadium || stadExtra.used) {
                path fshPath;
                bool hasFshName = false;
                if (stadExtra.used) {
                    if (stadExtra.stadType == StadiumExtra::STAD_CUSTOM) {
                        fshPath = out;
                        fshPath.replace_filename("texture_" + to_string(stadExtra.lightingId) + ".fsh");
                        hasFshName = true;
                    }
                    else if (stadExtra.stadType == StadiumExtra::STAD_DEFAULT) {
                        fshPath = out;
                        fshPath.replace_filename("t226__" + to_string(stadExtra.stadiumId) + "_" + to_string(stadExtra.lightingId) + ".fsh");
                        hasFshName = true;
                    }
                }
                if (!hasFshName) {
                    if (!options().fshOutput.empty()) {
                        if (options().processingFolders)
                            fshPath = path(options().fshOutput) / (out.stem().string() + ".fsh");
                        else
                            fshPath = options().fshOutput;
                    }
                    else {
                        fshPath = out;
                        fshPath.replace_extension(".fsh");
                    }
                }
                map<string, TextureToAdd> fshTextures;
                if (!options().fshTextures.empty()) {
                    for (auto const &a : options().fshTextures) {
                        path ap = a;
                        string texFilenameLowered = ToLower(ap.stem().string());
                        string afilename = ap.stem().string();
                        if (!afilename.empty()) {
                            if (afilename.length() > 4)
                                afilename = afilename.substr(0, 4);
                            string akey = ToLower(afilename);
                            if (!fshTextures.contains(akey)) {
                                bool texFound = false;
                                for (auto const &[k, img] : textures) {
                                    auto imgLoweredName = ToLower(img.name);
                                    auto imgLoweredFilename = ToLower(path(img.filepath).stem().string());
                                    if (imgLoweredFilename == texFilenameLowered) {
                                        fshTextures[imgLoweredName] = { img.name, img.filepath, options().fshFormat, options().fshLevels, img.embedded };
                                        texFound = true;
                                        break;
                                    }
                                }
                                if (!texFound)
                                    fshTextures[akey] = { afilename, a, options().fshFormat, options().fshLevels };
                            }
                        }
                    }
                }
                else {
                    static set<string> defaultTexturesToIgnore = { "eyeb", "rwa0", "rwa1", "rwa2", "rwa3", "rwh0", "rwh1", "rwh2", "rwh3", "rwn0", "rwn1", "rwn2", "rwn3", "abna", "abnb", "abnc", "afla", "aflb", "aflc", "hbna", "hbnb", "hbnc", "hfla", "hflb", "hflc", "adba", "adbb", "adbc", "chf0", "chf1", "chf2", "chf3","caf0", "caf1", "caf2", "caf3", "hcrs", "acrs", "hcla", "hclb", "acla", "aclb" };
                    for (auto const &[k, img] : textures) {
                        auto imgLoweredName = ToLower(img.name);
                        auto imgLoweredFilename = ToLower(path(img.filepath).stem().string());
                        bool ignoreThisTexture = false;
                        if (!options().fshDisableTextureIgnore) {
                            if (defaultTexturesToIgnore.contains(imgLoweredName) || options().fshIgnoreTextures.contains(imgLoweredName)
                                || defaultTexturesToIgnore.contains(imgLoweredFilename) || options().fshIgnoreTextures.contains(imgLoweredFilename))
                            {
                                ignoreThisTexture = true;
                            }
                        }
                        if (!ignoreThisTexture) {
                            fshTextures[imgLoweredName] = { img.name, img.filepath, options().fshFormat, options().fshLevels, img.embedded };
                        }
                    }
                }
                for (auto const &a : options().fshAddTextures) {
                    path ap = a;
                    string afilename = ap.stem().string();
                    if (!afilename.empty()) {
                        if (afilename.length() > 4)
                            afilename = afilename.substr(0, 4);
                        string akey = ToLower(afilename);
                        if (!fshTextures.contains(akey))
                            fshTextures[akey] = { afilename, a, options().fshFormat, options().fshLevels };
                    }
                }
                WriteFsh(fshPath, in.parent_path(), fshTextures);
            }
        }
    }

    if (stadExtra.used) {
        path targetFolder;
        if (out.is_absolute())
            targetFolder = out.parent_path();
        else
            targetFolder = absolute(out).parent_path();
        if (stadExtra.flags) {
            path stadFlagsPath = targetFolder /
                (stadExtra.stadType == StadiumExtra::STAD_DEFAULT ?
                    Format("sle-%d-%d.loc", stadExtra.stadiumId, stadExtra.lightingId) :
                    Format("flags_%d.loc", stadExtra.lightingId));
            FifamWriter writer(stadFlagsPath, 14, FifamVersion(), false);
            if (writer.Available()) {
                vector<aiNode *> flagNodes;
                for (unsigned int c = 0; c < stadExtra.flags->mNumChildren; c++) {
                    if (!ShouldIgnoreThisNode(stadExtra.flags->mChildren[c]))
                        flagNodes.push_back(stadExtra.flags->mChildren[c]);
                }
                sort(flagNodes.begin(), flagNodes.end(), [](aiNode *a, aiNode *b) {
                    string s1 = a->mName.C_Str();
                    string s2 = b->mName.C_Str();
                    return (s1.length() == s2.length()) ? (s1 < s2) : (s1.length() < s2.length());
                });
                for (auto const &flagNode : flagNodes) {
                    auto flagOptions = GetNameOptions(flagNode->mName.C_Str());
                    unsigned int flagType = 0;
                    if (flagOptions.contains("type"))
                        flagType = SafeConvertInt<unsigned int>(flagOptions["type"]);
                    aiVector3D scaling, position;
                    aiQuaternion rotation;
                    flagNode->mTransformation.Decompose(scaling, rotation, position);
                    if (doScale && options().scaleXYZ) {
                        position.x *= options().scale.x;
                        position.y *= options().scale.y;
                        position.z *= options().scale.z;
                    }
                    if (doTranslate) {
                        position.x += options().translate.x;
                        position.y += options().translate.y;
                        position.z += options().translate.z;
                    }
                    writer.WriteLine(Format(L"%f %f %f %d %f %f %f", position.x / 100.0f, position.y / 100.0f, position.z / 100.0f, flagType, scaling.x, scaling.y, scaling.z));
                }
            }
        }
        if (stadExtra.effects) {
            path stadEffectsPath = targetFolder / (stadExtra.stadType == StadiumExtra::STAD_DEFAULT ? 
                Format("tag-%d-%d.loc", stadExtra.stadiumId, stadExtra.lightingId) :
                Format("lights_%d.loc", stadExtra.lightingId));
            FifamWriter writer(stadEffectsPath, 14, FifamVersion(), false);
            if (writer.Available()) {
                vector<aiNode *> effNodes;
                for (unsigned int c = 0; c < stadExtra.effects->mNumChildren; c++) {
                    if (!ShouldIgnoreThisNode(stadExtra.effects->mChildren[c]))
                        effNodes.push_back(stadExtra.effects->mChildren[c]);
                }
                sort(effNodes.begin(), effNodes.end(), [](aiNode *a, aiNode *b) {
                    string s1 = a->mName.C_Str();
                    string s2 = b->mName.C_Str();
                    return s1 < s2;
                });
                struct Prop {
                    aiVector3D pos, dir;
                    unsigned int type = 0;
                };
                vector<pair<string, vector<Prop>>> effects;
                for (auto const &effNode : effNodes) {
                    auto effOptions = GetNameOptions(effNode->mName.C_Str());
                    string effType;
                    if (effOptions.contains("type"))
                        effType = effOptions["type"];
                    else {
                        string effNodeName = effNode->mName.C_Str();
                        auto brp = effNodeName.find_first_of(" _.[");
                        if (brp != string::npos)
                            effType = effNodeName.substr(0, brp);
                        else
                            effType = effNodeName;
                    }
                    Prop p;
                    auto const &m = effNode->mTransformation;
                    aiVector3D scaling, position;
                    aiQuaternion rotation;
                    effNode->mTransformation.Decompose(scaling, rotation, position);
                    if (doScale && options().scaleXYZ) {
                        position.x *= options().scale.x;
                        position.y *= options().scale.y;
                        position.z *= options().scale.z;
                    }
                    if (doTranslate) {
                        position.x += options().translate.x;
                        position.y += options().translate.y;
                        position.z += options().translate.z;
                    }
                    p.pos = position;
                    p.dir = aiVector3D(m.a2, m.b2, m.c2);
                    p.type = 1;
                    if (effOptions.contains("dir")) {
                        if (effOptions["dir"] == "null")
                            p.type = 3;
                        else
                            p.type = 2;
                    }
                    if (effects.empty() || effects.back().first != effType)
                        effects.emplace_back();
                    effects.back().first = effType;
                    effects.back().second.push_back(p);
                }
                writer.WriteLine(effects.size());
                for (auto const &[name, ev] : effects) {
                    bool hasDir = false;
                    for (auto const &e : ev) {
                        if (e.type == 2 || e.type == 3) {
                            hasDir = true;
                            break;
                        }
                    }
                    string effHeader = name + " " + to_string(ev.size()) + " Position";
                    if (hasDir)
                        effHeader += " Direction";
                    writer.WriteLine(effHeader);
                    for (auto const &e : ev) {
                        if (hasDir) {
                            if (e.type == 2)
                                writer.WriteLine(Format(L"%f %f %f %f %f %f", e.pos.x / 1.12f, e.pos.y / 1.12f, e.pos.z / 1.12f, e.dir.x, e.dir.y, e.dir.z));
                            else
                                writer.WriteLine(Format(L"%f %f %f 0 0 0", e.pos.x / 1.12f, e.pos.y / 1.12f, e.pos.z / 1.12f));
                        }
                        else
                            writer.WriteLine(Format(L"%f %f %f", e.pos.x / 1.12f, e.pos.y / 1.12f, e.pos.z / 1.12f));
                    }
                }
            }
        }
        if (stadExtra.collision) {
            path stadCollPath = targetFolder / (stadExtra.stadType == StadiumExtra::STAD_DEFAULT ? 
                Format("coll-%d-%d.bin", stadExtra.stadiumId, stadExtra.lightingId) :
                Format("collision_%d.bin", stadExtra.lightingId));
            vector<aiNode *> colNodes;
            if (stadExtra.collision->mNumMeshes > 0)
                colNodes.push_back(stadExtra.collision);
            for (unsigned int c = 0; c < stadExtra.collision->mNumChildren; c++) {
                if (stadExtra.collision->mChildren[c]->mNumMeshes > 0) {
                    if (!ShouldIgnoreThisNode(stadExtra.collision->mChildren[c]))
                        colNodes.push_back(stadExtra.collision->mChildren[c]);
                }
            }
            BinaryBuffer colFile;
            colFile.Put<unsigned int>(2);
            colFile.Put<unsigned int>(0);
            colFile.Put<unsigned int>(colNodes.size());
            for (unsigned int n = 0; n < colNodes.size(); n++) {
                auto const &node = colNodes[n];
                map<aiVector3D, unsigned int> uniquePos;
                map<aiVector3D, unsigned int> uniqueNormal;
                vector<CollTriangle> triangles;
                vector<CollLine> lines;
                auto AddCollVert = [](map<aiVector3D, unsigned int> &uniqueMap, aiVector3D const &collVec) {
                    auto it = uniqueMap.find(collVec);
                    if (it == uniqueMap.end()) {
                        unsigned int newId = uniqueMap.size();
                        uniqueMap[collVec] = newId;
                        return newId;
                    }
                    return (*it).second;
                };
                for (unsigned int m = 0; m < node->mNumMeshes; m++) {
                    auto const &mesh = scene->mMeshes[node->mMeshes[m]];
                    for (unsigned int f = 0; f < mesh->mNumFaces; f++) {
                        auto const &face = mesh->mFaces[f];
                        aiVector3D triPos[3];
                        for (unsigned int v = 0; v < 3; v++) {
                            triPos[v] = mesh->mVertices[face.mIndices[v]];
                            if (doScale && options().scaleXYZ) {
                                triPos[v].x *= options().scale.x;
                                triPos[v].y *= options().scale.y;
                                triPos[v].z *= options().scale.z;
                            }
                            if (doTranslate) {
                                triPos[v].x += options().translate.x;
                                triPos[v].y += options().translate.y;
                                triPos[v].z += options().translate.z;
                            }
                        }
                        if (triPos[0] == triPos[1] || triPos[1] == triPos[2]) {
                            if (triPos[0] != triPos[2]) {
                                CollLine line;
                                line.vertPos[0] = AddCollVert(uniquePos, triPos[0]);
                                line.vertPos[1] = AddCollVert(uniquePos, triPos[0] == triPos[1] ? triPos[2] : triPos[1]);
                                lines.push_back(line);
                            }
                        }
                        else {
                            CollTriangle triangle;
                            for (unsigned int v = 0; v < 3; v++)
                                triangle.vertPos[v] = AddCollVert(uniquePos, triPos[v]);
                            aiVector3D normal;
                            aiVector3D a = triPos[1] - triPos[0];
                            aiVector3D b = triPos[2] - triPos[0];
                            normal.x = a.y * b.z - a.z * b.y;
                            normal.y = a.z * b.x - a.x * b.z;
                            normal.z = a.x * b.y - a.y * b.x;
                            normal = normal.NormalizeSafe();
                            triangle.normal = AddCollVert(uniqueNormal, normal);
                            triangles.push_back(triangle);
                        }
                    }
                }
                vector<aiVector3D> positions(uniquePos.size());
                vector<aiVector3D> normals(uniqueNormal.size());
                aiVector3D min, max;
                bool isFirstVec = true;
                for (auto const &i : uniquePos) {
                    positions[i.second] = i.first;
                    if (isFirstVec) {
                        min = i.first;
                        max = i.first;
                        isFirstVec = false;
                    }
                    else {
                        if (i.first.x < min.x)
                            min.x = i.first.x;
                        if (i.first.y < min.y)
                            min.y = i.first.y;
                        if (i.first.z < min.z)
                            min.z = i.first.z;
                        if (i.first.x > max.x)
                            max.x = i.first.x;
                        if (i.first.y > max.y)
                            max.y = i.first.y;
                        if (i.first.z > max.z)
                            max.z = i.first.z;
                    }
                }
                for (auto const &i : uniqueNormal)
                    normals[i.second] = i.first;

                colFile.Put(min);
                colFile.Put(max);
                colFile.Put(unsigned short(positions.size()));
                colFile.Put(unsigned short(normals.size()));
                colFile.Put(unsigned short(triangles.size()));
                colFile.Put(unsigned short(lines.size()));
                for (auto const &i : positions)
                    colFile.Put(i);
                for (auto const &i : normals)
                    colFile.Put(i);
                for (auto const &i : triangles)
                    colFile.Put(i);
                for (auto const &i : lines)
                    colFile.Put(i);
            }
            colFile.WriteToFile(stadCollPath);
        }
    }
}
