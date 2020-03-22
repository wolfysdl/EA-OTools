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
    string filepath;
    struct Embedded {
        unsigned int width = 0;
        unsigned int height = 0;
        string format;
        void *data = nullptr;
    } embedded;

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

    Tex(string const &_name, Mode _u, Mode _v, Filter _filtering, MipMapMode _mipMapMode, float _mipMapLodBias = 0.0f, unsigned char _anisotropy = 1, unsigned int _offset = 0, Embedded const &_embedded = Embedded()) {
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
        return !IsFIFAAdboardsTexture();
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
    unsigned int wrapU = 1;
    unsigned int wrapV = 1;
    unsigned int wrapW = 1;
    unsigned int unknown6 = 0;
    unsigned int unknown7 = 0;
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

union VertexBoneInfo {
    float fValue;
    unsigned int uiValue;
    unsigned char ucValue;
};

struct VertexWeightInfo {
    VertexBoneInfo bones[3];
    unsigned int numBones;

    VertexWeightInfo() {
        Memory_Zero(*this);
    }
};

bool operator<(VertexBoneInfo const &a, VertexBoneInfo const &b) {
    return a.uiValue > b.uiValue;
}

bool operator>(VertexBoneInfo const &a, VertexBoneInfo const &b) {
    return a.uiValue < b.uiValue;
}

bool operator<(VertexWeightInfo const &a, VertexWeightInfo const &b) {
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
    map<VertexWeightInfo, vector<unsigned int>> weightsMap;
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
            auto options = Split(name.substr(be + 1, en - be - 1), ',', true, true);
            for (unsigned int i = 0; i < options.size(); i++) {
                string optionName, optionValue;
                auto dd = options[i].find(':');
                if (dd != string::npos) {
                    optionName = ToLower(options[i].substr(0, dd));
                    optionValue = options[i].substr(dd + 1);
                }
                else {
                    if (isMaterial && i == 0) {
                        optionName = "shader";
                        optionValue = options[i];
                    }
                    else
                        optionName = options[i];
                }
                if (!result.contains(optionName))
                    result[optionName] = optionValue;
            }
        }
    }
    return result;
}

bool ShouldIgnoreThisNode(aiNode *node) {
    auto options = GetNameOptions(node->mName.C_Str());
    return options.contains("ignore");
}

bool IsSkeletonNode(aiNode *node) {
    if (!ShouldIgnoreThisNode(node)) {
        string nodeName = node->mName.C_Str();
        if (nodeName.length() >= 8) {
            nodeName = ToLower(nodeName);
            if (nodeName.starts_with("skeleton")) {
                auto c = nodeName.c_str()[8];
                return c == '\0' || c == '.' || c == '_'; // 'Skeleton', 'Skeleton.001', 'Skeleton_001'
            }
        }
    }
    return false;
}

void NodeAddCallback(aiNode *node, vector<Node> &nodes) {
    if (!ShouldIgnoreThisNode(node)) {
        if (node->mNumMeshes)
            nodes.emplace_back(node);
    }
    for (unsigned int c = 0; c < node->mNumChildren; c++)
        NodeAddCallback(node->mChildren[c], nodes);
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

unsigned int SamplerIndex(unsigned int argType) {
    if (argType == Shader::Sampler1)
        return 1;
    else if (argType == Shader::Sampler2)
        return 2;
    else if (argType == Shader::Sampler3)
        return 3;
    return 0;
}

bool GetTexInfo(aiScene const *scene, aiMaterial const *mat, aiTextureType texType, aiTextureMapMode &mapMode, path &texFilePath, string &texFileName, string &texFileNameLowered, bool &isGlobal, Tex::Embedded &embedded) {
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
        if (!texFileName.empty()) {
            texFileNameLowered = ToLower(texFileName);
            if (texFileNameLowered.starts_with("global_")) {
                string globalTexName = texFileName.substr(7);
                if (globalTexName.empty())
                    return false;
                isGlobal = true;
                texFileName = globalTexName;
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
    Tex::Embedded embedded;
    bool present = !mat || GetTexInfo(scene, mat, texType, mapMode, texFilePath, texFileName, texFileNameLowered, isGlobal, embedded);
    if (present) {
        if (!mat) {
            if (customName.empty())
                return false;
            mapMode = aiTextureMapMode_Wrap;
            texFilePath = customName;
            texFileName = customName;
            texFileNameLowered = ToLower(customName);
            isGlobal = texFileNameLowered.starts_with("global_");
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
    importer.SetPropertyInteger(AI_CONFIG_PP_LBW_MAX_WEIGHTS, target->GetMaxBoneWeightsPerVertex());
    unsigned int sceneLoadingFlags = aiProcess_Triangulate | aiProcess_GenSmoothNormals | aiProcess_GenUVCoords | aiProcess_SplitLargeMeshes |
        aiProcess_SortByPType | aiProcess_PopulateArmatureData | aiProcess_LimitBoneWeights;
    if (options().scale > 0.0f && options().scale != 1.0f) {
        importer.SetPropertyFloat(AI_CONFIG_GLOBAL_SCALE_FACTOR_KEY, options().scale);
        sceneLoadingFlags |= aiProcess_GlobalScale;
    }
    if (!options().swapYZ)
        sceneLoadingFlags |= aiProcess_FlipUVs;
    if (options().preTransformVertices)
        sceneLoadingFlags |= aiProcess_PreTransformVertices;
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
    bool flipAxis = options().swapYZ;
    bool hasSkeleton = false;
    bool hasMorph = false;
    bool hasLights = scene->HasLights() || options().forceLighting;
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
    unsigned int uid = Hash(modelName);
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

    NodeAddCallback(scene->mRootNode, nodes);

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
                    Tex::Embedded embedded;
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
            auto optIt = matOptions.find("shader");
            if (optIt != matOptions.end()) {
                shader = target->FindShader((*optIt).second);
                if (shader)
                    usesCustomShaderName = true;
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
                shader = target->DecideShader(matProps);
            }

            if (!shader)
                shader = target->FindShader("Gouraud");

            if (!shader)
                throw runtime_error("Unable to decide shader");

            if (!isTextured[1]) {
                if (!hasReflectionTex && !hasSpecularTex &&
                    (
                    shader->nameLowered == "littextureirradenvmap" ||
                    shader->nameLowered == "irradlittextureenvmaptransparent2x" ||
                    shader->nameLowered == "littextureirradenvmap_skin"
                    )
                    )
                {
                    if (LoadTextureIntoTexSlot(scene, nullptr, aiTextureType_NONE, textures, &texAlreadyPresent[1], &tex[1], generatedTexNames, "spec")) {
                        hasSpecularTex = true;
                        isTextured[1] = true;
                    }
                } else if (shader->nameLowered == "littexture4head_skin") {
                    if (LoadTextureIntoTexSlot(scene, nullptr, aiTextureType_NONE, textures, &texAlreadyPresent[1], &tex[1], generatedTexNames, "tp02"))
                        isTextured[1] = true;
                }
                else if (shader->nameLowered == "littexture2irradskinsubsurfspec") {
                    if (LoadTextureIntoTexSlot(scene, nullptr, aiTextureType_NONE, textures, &texAlreadyPresent[1], &tex[1], generatedTexNames, "glos"))
                        isTextured[1] = true;
                }
            }

            isMeshSkinned = shader->HasAttribute(Shader::BlendWeight) && shader->HasAttribute(Shader::BlendIndices) && shader->HasAttribute(Shader::Color1);
            bool useSkinning = isMeshSkinned && meshHasBones;

            vector<VertexWeightInfo> allMeshesVertexWeights;

            if (useSkinning) {
                if (!hasSkeleton)
                    hasSkeleton = true;
                if (bones.empty()) {
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
                // Find weights for all vertices
                allMeshesVertexWeights.resize(mesh->mNumVertices);
                for (unsigned int b = 0; b < mesh->mNumBones; b++) {
                    aiBone *bone = mesh->mBones[b];
                    auto bit = bones.find(bone->mNode->mName.C_Str());
                    if (bit != bones.end()) {
                        for (unsigned int w = 0; w < bone->mNumWeights; w++) {
                            if (bone->mWeights[w].mWeight > 0) {
                                auto &vw = allMeshesVertexWeights[bone->mWeights[w].mVertexId];
                                if (vw.numBones == 3)
                                    throw runtime_error("More than 3 bone weights on vertex");
                                vw.bones[vw.numBones].fValue = bone->mWeights[w].mWeight;
                                vw.bones[vw.numBones].ucValue = (*bit).second.index;
                                vw.numBones++;
                            }
                        }
                    }
                    else
                        throw runtime_error("Unable to find bone in bones array");
                }
                // Sort weights in vertices
                for (unsigned int v = 0; v < mesh->mNumVertices; v++) {
                    if (allMeshesVertexWeights[v].numBones > 1)
                        sort(&allMeshesVertexWeights[v].bones[0], &allMeshesVertexWeights[v].bones[allMeshesVertexWeights[v].numBones]);
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
                            if (!meshes.back().weightsMap.contains(allMeshesVertexWeights[tri[ind]])) {
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
                    for (unsigned int ind = 0; ind < 3; ind++)
                        meshes.back().weightsMap[allMeshesVertexWeights[tri[ind]]].push_back(tri[ind]);
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
                    SetListsOnly(true);
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

                vector<VertexWeightInfo> skinVertexWeights;
                vector<unsigned int> skinVertexWeightsIndices;

                if (useSkinning && !m.weightsMap.empty()) {
                    skinVertexWeights.resize(m.weightsMap.size());
                    skinVertexWeightsIndices.resize(numVertices);
                    unsigned int weightInfoIndex = 0;
                    for (auto const &[w, vertIndices] : m.weightsMap) {
                        skinVertexWeights[weightInfoIndex] = w;
                        if (skinVertexWeights[weightInfoIndex].numBones == 3)
                            vertexWeightsNumBones3++;
                        else if (skinVertexWeights[weightInfoIndex].numBones == 2)
                            vertexWeightsNumBones2++;
                        else if (skinVertexWeights[weightInfoIndex].numBones == 1)
                            vertexWeightsNumBones1++;
                        skinVertexWeights[weightInfoIndex].numBones = 0;
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
                                    if (numColors > 0 && mesh->HasVertexColors(0) && mesh->mColors[0]) {
                                        vertexColor = mesh->mColors[0][v];
                                        swap(vertexColor.r, vertexColor.b);
                                        if (options().vColScale != 0.0f) {
                                            vertexColor.r *= options().vColScale;
                                            vertexColor.g *= options().vColScale;
                                            vertexColor.b *= options().vColScale;
                                        }
                                    }
                                    else {
                                        if (options().hasDefaultVCol)
                                            vertexColor = options().defaultVCol;
                                        else
                                            vertexColor = DEFAULT_COLOR;
                                    }
                                }
                                if (!options().ignoreMatColor) {
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
                                    rgba[clr] = unsigned char(vertexColor[clr] * 255);
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
                        bufData.Put(skinVertexWeights.data(), skinVertexWeights.size() * sizeof(VertexWeightInfo));
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
                    case Shader::UVOffset0:
                    {
                        Vector4D uvOffset0;
                        globalArgs.emplace_back(modifiables.GetArg("Coordinate4::" + n.name + "::UVOffset0", bufData, uvOffset0, false, false));
                    }
                    break;
                    case Shader::UVOffset1:
                    {
                        Vector4D uvOffset1;
                        globalArgs.emplace_back(modifiables.GetArg("Coordinate4::" + n.name + "::UVOffset1", bufData, uvOffset1, false, false));
                    }
                    break;
                    case Shader::XFade:
                    {
                        Vector4D xFade;
                        globalArgs.emplace_back(modifiables.GetArg("Coordinate4::" + n.name + "::XFade", bufData, xFade, false, false));
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
                    case Shader::Sampler0:
                    case Shader::Sampler1:
                    case Shader::Sampler2:
                    case Shader::Sampler3:
                    {
                        unsigned int s = SamplerIndex(arg.type);
                        if (tex[s].isGlobal)
                            globalArgs.emplace_back("__EAGL::TAR:::" + tex[s].name);
                        else {
                            if (texAlreadyPresent[s]) {
                                if (tex[s].IsRuntimeConstructed())
                                    globalArgs.emplace_back(tex[s].GetRuntimeConstructorLine(uid, numVariations));
                                else
                                    globalArgs.emplace_back(tex[s].offset);
                            }
                            else {
                                if (tex[s].IsRuntimeConstructed())
                                    globalArgs.emplace_back(tex[s].GetRuntimeConstructorLine(uid, numVariations));
                                else {
                                    bufData.Align(16);
                                    tex[s].offset = bufData.Position();
                                    globalArgs.emplace_back(bufData.Position(), 1);
                                    symbols.emplace_back("__EAGL::TAR:::" + tex[s].name + "_" + to_string(Hash(modelName + "_" + tex[s].name)), bufData.Position());
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
                    bufData.Put(shader->name);
                }
                // RenderMethod
                bufData.Align(16);
                unsigned int renderMethodOffset = bufData.Position();
                symbols.emplace_back("__RenderMethod:::__GPRenderMethod_" + modelName + ".tagged_" + to_string(meshCounter), bufData.Position());
                relocations[""].push_back(bufData.Position());
                bufData.Put(codeBlockOffset);
                bufData.Put(ZERO);
                relocations[shader->name + "__EAGLMicroCode"].push_back(bufData.Position());
                bufData.Put(ZERO);
                bufData.Put(ZERO);
                relocations["ParentRM_" + shader->name].push_back(bufData.Position());
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
        bufData.Align(16);
        vector<BoneInfo> vecBones;
        if (!bones.empty()) {
            vecBones.resize(bones.size());
            for (auto const &[name, info] : bones)
                vecBones[info.index] = info;
            // bones
            for (auto const &b : vecBones) {
                symbols.emplace_back("__Bone:::" + modelName + "." + b.name, bufData.Position());
                bufData.Put(unsigned int(b.index));
                bufData.Put(ZERO);
                bufData.Put(ZERO);
                bufData.Put(ZERO);
            }
        }
        // skeleton
        symbols.emplace_back("__Skeleton:::" + modelName, bufData.Position());
        bufData.Put(unsigned short(ANIM_VERSION));
        bufData.Put(unsigned short(510));
        bufData.Put(unsigned short(ANIM_VERSION));
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
    static unsigned char TTARSig[] = { 'T', 'T', 'A', 'R' };
    bufData.Put(TTARSig, std::size(TTARSig));
    // ModelLayers
    unsigned int modelLayersOffset = bufData.Position();
    if (target->Version() >= 3) {
        bufData.Put(ZERO);
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
        bufData.Put(ZERO);
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
    if (!options().noMetadata) {
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
    bufSymbols.Put(Elf32_Sym(bufSymbolNames.Position(), 0, 4, 0x21, 0, 1));
    bufSymbolNames.Put(string("__OTOOLS_VERSION:::OTOOLS_VERSION-") + OTOOLS_VERSION);
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
    unsigned int symbolIndex = 4 + symbols.size();
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

    if (!options().noMetadata) {
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

    string versionMessage = "This file was generated with otools version ";
    versionMessage += OTOOLS_VERSION;

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
    header.e_shoff = headerBlockSize + GetAligned(versionMessage.size(), 16) + bufData.Size() + bufSectionNames.Size() + bufSymbolNames.Size() 
        + bufSymbols.Size() + bufRelocations.Size() + (options().noMetadata ? 0 : bufMetadata.Size());
    header.e_flags = 0x20924000;
    header.e_ehsize = sizeof(Elf32_Ehdr);
    header.e_phentsize = 0;
    header.e_phnum = 0;
    header.e_shentsize = sizeof(Elf32_Shdr);
    header.e_shnum = options().noMetadata ? 6 : 7;
    header.e_shstrndx = 2;
    sectionOffsets.push_back(0);
    bufElf.Put(header);
    bufElf.Align(16);
    bufElf.Put(versionMessage);
    bufElf.Align(16);
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
    if (!options().noMetadata) {
        sectionOffsets.push_back(bufElf.Position());
        bufElf.Put(bufMetadata);
    }
    bufElf.Put(Elf32_Shdr(sectionNamesOffets[0], SHT_NULL, 0, 0, sectionOffsets[0], 0, 0, 0, 0, 0));
    bufElf.Put(Elf32_Shdr(sectionNamesOffets[1], SHT_PROGBITS, 0x3, 0, sectionOffsets[1], bufData.Size(), 0, 0, 16, 0));
    bufElf.Put(Elf32_Shdr(sectionNamesOffets[2], SHT_STRTAB, 0, 0, sectionOffsets[2], bufSectionNames.Size(), 0, 0, 1, 0));
    bufElf.Put(Elf32_Shdr(sectionNamesOffets[3], SHT_STRTAB, 0, 0, sectionOffsets[3], bufSymbolNames.Size(), 0, 0, 1, 0));
    bufElf.Put(Elf32_Shdr(sectionNamesOffets[4], SHT_SYMTAB, 0, 0, sectionOffsets[4], bufSymbols.Size(), 3, 2, 4, sizeof(Elf32_Sym)));
    bufElf.Put(Elf32_Shdr(sectionNamesOffets[5], SHT_REL, 0, 0, sectionOffsets[5], bufRelocations.Size(), 4, 1, 4, sizeof(Elf32_Rel)));
    if (!options().noMetadata)
        bufElf.Put(Elf32_Shdr(sectionNamesOffets[6], SHT_PROGBITS, 0, 0, sectionOffsets[6], bufMetadata.Size(), 0, 0, 1, 0));
    if (options().pad > 0 && bufElf.Size() < options().pad) {
        unsigned int numPaddingBytes = options().pad - bufElf.Size();
        for (unsigned int i = 0; i < numPaddingBytes; i++)
            bufElf.Put<unsigned char>(0);
    }
    if (!isOrd)
        bufElf.WriteToFile(out);
    else {
        bufElf.WriteToFile(out, 0, sectionOffsets[2]);
        auto orlPath = out;
        orlPath.replace_extension(outExt == ".ORD" ? ".ORL" : ".orl");
        bufElf.WriteToFile(orlPath, sectionOffsets[2], bufElf.Size() - sectionOffsets[2]);
    }

    struct TextureToAdd {
        string name;
        string filepath;
        Tex::Embedded embedded;

        TextureToAdd() {};

        TextureToAdd(string const &_name, string const &_filepath, Tex::Embedded const &_embedded = Tex::Embedded()) {
            name = _name; filepath = _filepath; embedded = _embedded;
        }
    };

    auto WriteFsh = [](path const &fshFilePath, path const &searchDir, map<string, TextureToAdd> const &texturesToAdd) {
        static vector<string> imgExt = { ".png", ".jpg", ".jpeg", ".bmp", ".dds", ".tga" };
        path fshDir = fshFilePath.parent_path();
        string fshFileName = fshFilePath.filename().string();
        ea::Fsh fsh;
        ea::Buffer metalBinData;
        metalBinData.Allocate(64);
        Memory_Zero(metalBinData.GetData(), metalBinData.GetSize());
        strcpy((char *)metalBinData.GetData(), "EAGL64 metal bin attachment for runtime texture management");
        if (!texturesToAdd.empty()) {
            for (auto const &[k, img] : texturesToAdd) {
                ea::FshImage::LoadingInfo loadingInfo;
                if (img.embedded.data && !options().ignoreEmbeddedTextures) {
                    if (img.embedded.height == 0) { // compressed data
                        auto fileFormat = ea::FshImage::DIB;
                        if (img.embedded.format == "png")
                            fileFormat = ea::FshImage::PNG;
                        else if (img.embedded.format == "jpg")
                            fileFormat = ea::FshImage::JPG;
                        else if (img.embedded.format == "bmp")
                            fileFormat = ea::FshImage::BMP;
                        else if (img.embedded.format == "tga")
                            fileFormat = ea::FshImage::TGA;
                        else if (img.embedded.format == "dds")
                            fileFormat = ea::FshImage::DDS;
                        if (fileFormat != ea::FshImage::DIB) {
                            loadingInfo.fileData = img.embedded.data;
                            loadingInfo.fileDataSize = img.embedded.width;
                            loadingInfo.fileFormat = fileFormat;
                        }
                    }
                    else { // assimp uncompressed data
                        D3DFORMAT dataFormat = D3DFMT_UNKNOWN;
                        if (img.embedded.format == "rgba8888")
                            dataFormat = D3DFMT_A8R8G8B8;
                        else if (img.embedded.format == "argb8888")
                            dataFormat = D3DFMT_A8B8G8R8;
                        else if (img.embedded.format == "rgba5650")
                            dataFormat = D3DFMT_R5G6B5;
                        else if (img.embedded.format == "rgba0010")
                            dataFormat = D3DFMT_L8;
                        if (dataFormat != D3DFMT_UNKNOWN) {
                            loadingInfo.data = img.embedded.data;
                            loadingInfo.dataWidth = img.embedded.width;
                            loadingInfo.dataHeight = img.embedded.height;
                            loadingInfo.dataFormat = unsigned int(dataFormat);
                        }
                    }
                }
                if (!loadingInfo.fileData && !loadingInfo.data) {
                    path imgPath = img.filepath;
                    loadingInfo.filepath = imgPath;
                    bool hasExtension = false;
                    if (imgPath.has_extension()) {
                        string ext = ToLower(imgPath.extension().string());
                        for (auto const &ie : imgExt) {
                            if (ext == ie) {
                                hasExtension = true;
                                break;
                            }
                        }
                    }
                    if (hasExtension) {
                        loadingInfo.fileExists = exists(imgPath);
                        if (!loadingInfo.fileExists) {
                            loadingInfo.filepath = searchDir / imgPath;
                            loadingInfo.fileExists = exists(loadingInfo.filepath);
                            if (!loadingInfo.fileExists) {
                                imgPath.replace_extension();
                                hasExtension = false;
                            }
                        }
                    }
                    if (!loadingInfo.fileExists && !hasExtension) {
                        for (auto const &ie : imgExt) {
                            string filePathWithExt = imgPath.string() + ie;
                            loadingInfo.filepath = filePathWithExt;
                            loadingInfo.fileExists = exists(loadingInfo.filepath);
                            if (loadingInfo.fileExists)
                                break;
                            loadingInfo.filepath = searchDir / filePathWithExt;
                            loadingInfo.fileExists = exists(loadingInfo.filepath);
                            if (loadingInfo.fileExists)
                                break;
                        }
                    }
                }
                if (loadingInfo.fileData || loadingInfo.data || loadingInfo.fileExists) {
                    auto &image = fsh.AddImage();
                    image.Load(loadingInfo, options().fshFormat, options().fshLevels, options().fshRescale);
                    ea::FshPixelData *pixelsData = image.FindFirstData(ea::FshData::PIXELDATA)->As<ea::FshPixelData>();
                    image.AddData(new ea::FshMetalBin(metalBinData, 0x10));
                    image.SetTag(img.name);
                    image.AddData(new ea::FshName(img.name));
                    char comment[256];
                    static char idStr[260];
                    unsigned int texNameHash = Hash(fshFilePath.stem().string() + "_" + img.name);
                    sprintf_s(idStr, "0x%.8x", texNameHash);
                    sprintf_s(comment, "TXLY,%s,1,%d,%d,%d,%s", image.GetTag().c_str(), pixelsData->GetNumMipLevels() > 0 ? 1 : 0,
                        pixelsData->GetWidth(), pixelsData->GetHeight(), idStr);
                    image.AddData(new ea::FshComment(comment));
                }
            }
            if (fsh.GetImagesCount() > 0) {
                fsh.ForAllImages([&](ea::FshImage &image) {
                    auto hotSpot = image.AddData(new ea::FshHotSpot())->As<ea::FshHotSpot>();
                    fsh.ForAllImages([&](ea::FshImage &image2) {
                        char fourcc[4] = { 0, 0, 0, 0 };
                        auto tag = image2.GetTag();
                        for (unsigned int i = 0; i < 4; i++) {
                            if (tag.size() > i)
                                fourcc[i] = tag[i];
                        }
                        std::swap(fourcc[0], fourcc[3]);
                        std::swap(fourcc[1], fourcc[2]);
                        hotSpot->Regions().push_back(ea::FshHotSpot::Region(*((unsigned int *)fourcc), 0, 0,
                            image2.FindFirstData(ea::FshData::PIXELDATA)->As<ea::FshPixelData>()->GetWidth(),
                            image2.FindFirstData(ea::FshData::PIXELDATA)->As<ea::FshPixelData>()->GetHeight()));
                        });
                });
                if (!fshDir.empty())
                    create_directories(fshDir);
                fsh.Write(fshFilePath);
                if (options().padFsh > 0) {
                    FILE *fshFile = _wfopen(fshFilePath.c_str(), L"a+");
                    if (fshFile) {
                        fseek(fshFile, 0, SEEK_END);
                        unsigned int fshSize = ftell(fshFile);
                        if (fshSize < options().padFsh) {
                            unsigned int numPaddingBytes = options().padFsh - fshSize;
                            static unsigned char zero = 0;
                            for (unsigned int i = 0; i < numPaddingBytes; i++)
                                fwrite(&zero, 1, 1, fshFile);
                        }
                        fclose(fshFile);
                    }
                }
            }
        }
    };

    if (options().writeFsh) {

        if (options().head && modelName.starts_with("m228__")) {
            unsigned int playerId = 0;
            if (sscanf(modelName.substr(6).c_str(), "%d", &playerId) == 1) {
                string playerIdStr = to_string(playerId);
                auto storedLevels = options().fshLevels;
                auto storedFormat = options().fshFormat;
                auto storedPad = options().padFsh;

                // writing t21__

                options().fshLevels = 99;
                options().fshFormat = D3DFMT_DXT1;
                if (targetName == "FIFA06" || targetName == "FIFA07" || targetName == "FIFA08" || targetName == "FIFA09")
                    options().fshLevels = 1;
                else if (targetName == "FIFA10")
                    options().fshLevels = 99;
                if (targetName == "FIFA06" || targetName == "FIFA07")
                    options().padFsh = 16'656;
                else if (targetName == "FIFA08" || targetName == "FIFA09")
                    options().padFsh = 65'808;
                else if (targetName == "FIFA10")
                    options().padFsh = 87'648;
                string t21filesuffix = "_0_0_0_0";
                if (targetName == "FIFA06" || targetName == "FIFA07" || targetName == "FIFA08")
                    t21filesuffix = "_0_0";
                map<string, TextureToAdd> fshTextures1;
                fshTextures1["tp01"] = { "tp01", "tp01@" + playerIdStr };
                for (auto const &a : options().fshAddTextures) {
                    path ap = a;
                    string afilename = ap.stem().string();
                    if (!afilename.empty()) {
                        if (afilename.length() > 4)
                            afilename = afilename.substr(0, 4);
                        string akey = ToLower(afilename);
                        if (!fshTextures1.contains(akey))
                            fshTextures1[akey] = { afilename, a };
                    }
                }
                WriteFsh(out.parent_path() / ("t21__" + playerIdStr + t21filesuffix + ".fsh"), in.parent_path(), fshTextures1);

                // writing t22__

                options().fshLevels = 99;
                options().fshFormat = D3DFMT_DXT5;
                options().padFsh = storedPad;
                //if (targetName == "FIFA06" || targetName == "FIFA07")
                //    options().fshFormat = D3DFMT_A4R4G4B4;
                //else if (targetName == "FIFA08" || targetName == "FIFA09" || targetName == "FIFA10")
                //    options().fshFormat = D3DFMT_DXT1;
                if (targetName == "FIFA06" || targetName == "FIFA07")
                    options().padFsh = 43'920;
                else if (targetName == "FIFA08")
                    options().padFsh = 11'168;
                else if (targetName == "FIFA09" || targetName == "FIFA10")
                    options().padFsh = 43'936;
                if (options().fshFormat != D3DFMT_UNKNOWN) {
                    map<string, TextureToAdd> fshTextures2;
                    fshTextures2["tp02"] = { "tp02", "tp02@" + playerIdStr };
                    WriteFsh(out.parent_path() / ("t22__" + playerIdStr + "_0.fsh"), in.parent_path(), fshTextures2);
                }

                options().fshLevels = storedLevels;
                options().fshFormat = storedFormat;
                options().padFsh = storedPad;
            }
        }
        else {
            path fshPath;
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
                                    fshTextures[imgLoweredName] = { img.name, img.filepath, img.embedded };
                                    texFound = true;
                                    break;
                                }
                            }
                            if (!texFound)
                                fshTextures[akey] = { afilename, a };
                        }
                    }
                }
            }
            else {
                static set<string> defaultTexturesToIgnore = { "eyeb", "rwa0", "rwa1", "rwa2", "rwa3", "rwh0", "rwh1", "rwh2", "rwh3", "rwn0", "rwn1", "rwn2", "rwn3", "abna", "abnb", "abnc", "afla", "aflb", "aflc", "hbna", "hbnb", "hbnc", "hfla", "hflb", "hflc", "adba", "adbb", "adbc", "chf0", "chf1", "chf2", "chf3","caf0", "caf1", "caf2", "caf3" };
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
                        fshTextures[imgLoweredName] = { img.name, img.filepath, img.embedded };
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
                        fshTextures[akey] = { afilename, a };
                }
            }
            WriteFsh(fshPath, in.parent_path(), fshTextures);
        }
    }
}
