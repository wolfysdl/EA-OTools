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

const char *IMPORTER_VERSION = "0.100";

struct Vector4D {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float w = 0.0f;
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

struct Node {
    string name;
    aiNode *node = nullptr;
    vector<unsigned int> renderDescriptorsOffsets;
    aiVector3D boundMin = { 0.0f, 0.0f, 0.0f };
    aiVector3D boundMax = { 0.0f, 0.0f, 0.0f };
    bool anyVertexProcessed = false;

    Node(aiNode *_node) {
        node = _node;
        name = _node->mName.C_Str();
    }
};

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

    Tex() {
        name = "----";
        uAddressing = Wrap;
        vAddressing = Mirror;
        filtering = Bilinear;
        mipMapMode = Linear;
        mipMapLodBias = 0.0f;
        anisotropy = 1;
        isGlobal = false;
    }

    Tex(string const &_name, Mode _u, Mode _v, Filter _filtering, MipMapMode _mipMapMode, float _mipMapLodBias = 0.0f, unsigned char _anisotropy = 1, unsigned int _offset = 0) {
        name = _name;
        uAddressing = _u;
        vAddressing = _v;
        filtering = _filtering;
        mipMapMode = _mipMapMode;
        mipMapLodBias = _mipMapLodBias;
        anisotropy = _anisotropy;
        offset = _offset;
        isGlobal = false;
    }

    bool IsRuntimeConstructed() const {
        return !IsAdboardsTexture();
    }

    bool IsAdboardsTexture() const {
        string ln = ToLower(name);
        return ln == "adba" || ln == "adbb" || ln == "adbc";
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
    string layerName;

    ModData(string const &_name, unsigned int _offset, vector<unsigned char> const &_data, string const &_layerName) {
        name = _name;
        offset = _offset;
        data = _data;
        layerName = _layerName;
    }

    ModData(string const &_name, string const &_runtimeConstructorLine, unsigned int _runtimeSize, string const &_layerName) {
        name = _name;
        offset = 0;
        runtimeConstructorLine = _runtimeConstructorLine;
        runtimeSize = _runtimeSize;
        layerName = _layerName;
    }

    ModData() {
        offset = 0;
    }
};

struct Modifiables {
    vector<ModData> vec;

    GlobalArg GetArg(string const &name, string const &_runtimeConstructorLine, unsigned int _runtimeSize, string const &_layerName = string()) {
        bool isPresent = false;
        for (auto const &m : vec) {
            if (m.name == name && m.runtimeConstructorLine == _runtimeConstructorLine && m.runtimeSize == _runtimeSize) {
                isPresent = true;
                break;
            }
        }
        if (!isPresent)
            vec.emplace_back(name, _runtimeConstructorLine, _runtimeSize, _layerName);
        return GlobalArg(_runtimeConstructorLine);
    }

    template<typename T>
    GlobalArg GetArg(string const &name, BinaryBuffer &buf, T const &obj, bool putZero = false, bool align = false, string const &_layerName = string()) {
        vector<unsigned char> newVec(sizeof(T));
        CopyMemory(&newVec[0], &obj, sizeof(T));
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
            vec.emplace_back(name, buf.Position(), newVec, _layerName);
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

bool IsRootNode(aiNode *node) {
    string nodeName = node->mName.C_Str();
    return nodeName == "<3DSRoot>" ||
        nodeName == "RootNode" ||
        nodeName == "ROOT" ||
        nodeName == "Scene" ||
        nodeName == "ID2";
}

bool ShouldIgnoreThisNode(aiNode *node) {
    if (!node->mNumMeshes)
        return true;
    auto l = ToLower(node->mName.C_Str());
    return l.find("[ignore]") != string::npos;
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
    return 0;
}

bool LoadTextureIntoTexSlot(aiScene const *scene, aiMaterial const *mat, aiTextureType texType, map<string, Tex> &texMap, bool *texPresentFlag, Tex *slot) {
    auto texCount = mat->GetTextureCount(texType);
    if (texCount > 0) {
        aiString texPath;
        aiTextureMapMode mapMode = aiTextureMapMode_Wrap;
        mat->GetTexture(texType, 0, &texPath, nullptr, nullptr, nullptr, nullptr, &mapMode);
        string texFileName;
        if (auto texture = scene->GetEmbeddedTexture(texPath.C_Str()))
            texFileName = path(texture->mFilename.C_Str()).stem().string();
        else
            texFileName = path(texPath.C_Str()).stem().string();
        if (!texFileName.empty()) {
            string texFileNameLowered = ToLower(texFileName);
            if (texFileNameLowered.starts_with("global_")) {
                string globalTexName = texFileName.substr(7);
                if (globalTexName.empty())
                    return false;
                *texPresentFlag = true;
                slot->isGlobal = true;
                slot->name = globalTexName;
            }
            else {
                if (texFileName.length() > 4)
                    texFileName = texFileName.substr(0, 4);
                string texId = ToLower(texFileName);
                auto texit = texMap.find(texId);
                if (texit != texMap.end()) {
                    *texPresentFlag = true;
                    *slot = (*texit).second;
                }
                else {
                    slot->name = texFileName;
                    slot->SetUVAddressingMode(mapMode);
                    texMap[texId] = *slot;
                }
            }
            return true;
        }
    }
    return false;
}

void oimport(path const &out, path const &in) {
    Assimp::Importer importer;
    importer.SetPropertyInteger(AI_CONFIG_PP_SBP_REMOVE, aiPrimitiveType_POINT | aiPrimitiveType_LINE);
    unsigned int sceneLoadingFlags = aiProcess_Triangulate | aiProcess_GenSmoothNormals | aiProcess_SplitLargeMeshes;
    if (!options().swapYZ)
        sceneLoadingFlags |= aiProcess_FlipUVs;
    const aiScene *scene = importer.ReadFile(in.string(), sceneLoadingFlags);
    if (!scene)
        throw runtime_error("Unable to load scene");
    if (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE)
        throw runtime_error("Unable to load a complete scene");
    if (!scene->mRootNode)
        throw runtime_error("Unable to find scene root node");

    // TODO: axis detection
    // TODO: node (vertices) pre-transform

    static aiMatrix4x4 identityMatrix = { 1, 0, 0, 0,  0, 1, 0, 0,  0, 0, 1, 0,  0, 0, 0, 1 };
    Vector4D vecZeroOneTwoThree = { 0, 1, 2, 3 };
    Vector4D vec1111 = { 1, 1, 1, 1 };
    Vector4D vecEnvMapConstants = { 1.0f, 0.25f, 0.5f, 0.75f };
    bool flipAxis = options().swapYZ;
    bool isSkinned = false;
    bool hasLights = scene->HasLights() || options().forceLighting;
    const unsigned int ZERO = 0;
    const unsigned int ONE = 1;
    const unsigned int ANIM_VERSION = 0xC0DA;
    const int MINONE = -1;
    const float FONE = 1.0f;
    const float FZERO = 0.0f;
    BinaryBuffer bufData;
    string modelName = out.stem().string();
    unsigned int uid = Hash(modelName);
    aiColor4D DEFAULT_COLOR = { 0.5f, 0.5f, 0.5f, 1.0f };

    vector<Symbol> symbols;
    map<string, vector<unsigned int>> relocations;
    Modifiables modifiables;
    unsigned int numVariations = 1;
    unsigned short computationIndex = 2;
    vector<Node> nodes;
    map<string, Tex> textures;

    if (IsRootNode(scene->mRootNode)) {
        for (unsigned int c = 0; c < scene->mRootNode->mNumChildren; c++) {
            if (!ShouldIgnoreThisNode(scene->mRootNode->mChildren[c]))
                nodes.emplace_back(scene->mRootNode->mChildren[c]);
        }
    }
    else {
        if (!ShouldIgnoreThisNode(scene->mRootNode))
            nodes.emplace_back(scene->mRootNode);
    }
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
    for (auto &n : nodes) {
        for (unsigned int m = 0; m < n.node->mNumMeshes; m++) {
            aiMesh *mesh = scene->mMeshes[n.node->mMeshes[m]];
            aiMaterial *mat = scene->mMaterials[mesh->mMaterialIndex];
            string matName = mat->GetName().C_Str();
            Shader *shader = nullptr;

            aiColor3D matColor(1, 1, 1);
            mat->Get(AI_MATKEY_COLOR_DIFFUSE, matColor);
            float matAlpha = 1;
            mat->Get(AI_MATKEY_OPACITY, matAlpha);
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
            int blendFunc = 0;
            mat->Get(AI_MATKEY_BLEND_FUNC, blendFunc);
            float shininess = 0.0f;
            mat->Get(AI_MATKEY_SHININESS, shininess);
            float shininessStrength = 0.0f;
            mat->Get(AI_MATKEY_SHININESS_STRENGTH, shininessStrength);
            bool isUnlit = false;
            mat->Get(AI_MATKEY_GLTF_UNLIT, isUnlit);
            aiString alphaModeAiStr;
            mat->Get(AI_MATKEY_GLTF_ALPHAMODE, alphaModeAiStr);
            string alphaModeStr = alphaModeAiStr.C_Str();
            AlphaMode alphaMode = ALPHA_NOT_SET;
            if (!alphaModeStr.empty()) {
                if (alphaModeStr == "OPAQUE")
                    alphaMode = ALPHA_OPAQUE;
                else if (alphaModeStr == "MASK")
                    alphaMode = ALPHA_MASK;
                else if (alphaModeStr == "BLEND")
                    alphaMode = ALPHA_BLEND;
            }
            float alphaCutoff = 0.0f;
            mat->Get(AI_MATKEY_GLTF_ALPHACUTOFF, alphaCutoff);

            bool hasMatColor = matColor.r != 1 || matColor.g != 1 || matColor.b != 1;
            bool hasMatAlpha = matAlpha != 1;
            bool usesAlphaBlending = blendFunc != 0 || hasMatAlpha || alphaMode == ALPHA_BLEND;

            Tex tex[3];
            bool texUsed[3] = { false, false, false };
            bool texAlreadyPresent[3] = { false, false, false };
            bool hasDiffuseTex = LoadTextureIntoTexSlot(scene, mat, aiTextureType_DIFFUSE, textures, &texAlreadyPresent[0], &tex[0]);
            bool hasReflectionTex = LoadTextureIntoTexSlot(scene, mat, aiTextureType_REFLECTION, textures, &texAlreadyPresent[1], &tex[1]);
            bool hasSpecularTex = false;
            if (!hasReflectionTex)
                hasSpecularTex = LoadTextureIntoTexSlot(scene, mat, aiTextureType_SPECULAR, textures, &texAlreadyPresent[1], &tex[1]);

            bool usesCustomShaderName = false;
            if (!matName.empty()) {
                auto b_start = matName.find('[');
                if (b_start != string::npos && matName.length() > b_start) {
                    auto b_end = matName.find(']', b_start + 1);
                    if (b_end != string::npos) {
                        shader = FindShader(matName.substr(b_start + 1, b_end - b_start - 1));
                        if (shader)
                            usesCustomShaderName = true;
                    }
                }
            }

            if (!shader) {
                // TODO: handle skin shaders
                if (hasDiffuseTex) {
                    if (tex[0].IsAdboardsTexture())
                        shader = FindShader("XFadeScrollTexture");
                    else {
                        if (usesAlphaBlending) {
                            if (!isUnlit && (hasReflectionTex || hasSpecularTex))
                                shader = FindShader("IrradLitTextureEnvmapTransparent2x");
                            else {
                                if (blendFunc == aiBlendMode::aiBlendMode_Additive)
                                    shader = FindShader("ClipTextureAddNodepthwrite");
                                else
                                    shader = FindShader("ClipTextureAlphablend");
                            }
                        }
                        else { // no alpha blending
                            if (isUnlit)
                                shader = FindShader("Texture2x");
                            else {
                                if (hasReflectionTex || hasSpecularTex)
                                    shader = FindShader("LitTextureIrradEnvmap");
                                else {
                                    if (hasLights)
                                        shader = FindShader("LitTexture2x");
                                    else
                                        shader = FindShader("Texture2x");
                                }
                            }
                        }
                    }
                }
                else {
                    if (hasLights && !isUnlit)
                        shader = FindShader("IrradLitGouraud2x");
                    else
                        shader = FindShader("Gouraud");
                }
            }

            if (!shader)
                shader = &Shaders[0];

            unsigned int numVertices = mesh->mNumVertices;
            unsigned int numIndices = mesh->mNumFaces * 3;
            unsigned int numPrimitives = mesh->mNumFaces;
            unsigned int vertexSize = shader->VertexSize();
            const unsigned int indexSize = 2;
            unsigned int vertexBufferSize = vertexSize * numVertices;
            unsigned int indexBufferSize = indexSize * numIndices;
            unsigned int vertexBufferOffset = 0;
            unsigned int indexBufferOffset = 0;
            unsigned char *vertexBuffer = new unsigned char[vertexBufferSize];
            ZeroMemory(vertexBuffer, vertexBufferSize);
            unsigned short *indexBuffer = new unsigned short[numIndices];
            ZeroMemory(indexBuffer, indexBufferSize);
            unsigned int numColors = mesh->GetNumColorChannels();
            unsigned int numTexCoords = mesh->GetNumUVChannels();

            for (unsigned int f = 0; f < numPrimitives; f++) {
                indexBuffer[f * 3 + 0] = mesh->mFaces[f].mIndices[0];
                indexBuffer[f * 3 + 1] = mesh->mFaces[f].mIndices[1];
                indexBuffer[f * 3 + 2] = mesh->mFaces[f].mIndices[2];
            }
            // generate tristrips
            if (options().tristrip) {
                SetListsOnly(true);
                SetCacheSize(CACHESIZE_GEFORCE1_2);
                PrimitiveGroup *prims = nullptr;
                unsigned short numprims = 0;
                GenerateStrips(indexBuffer, numIndices, &prims, &numprims);
                delete[] indexBuffer;
                indexBuffer = prims[0].indices;
                numIndices = prims[0].numIndices;
                numPrimitives = numIndices - 2;
                indexBufferSize = indexSize * numIndices;
            }
            unsigned int vertexDataOffset = 0;
            for (unsigned int v = 0; v < numVertices; v++) {
                unsigned int vertexOffset = vertexDataOffset;
                for (auto const &d : shader->declaration) {
                    switch (d.usage) {
                    case Shader::Position:
                        if (d.type == Shader::Float3 && mesh->mVertices) {
                            aiVector3D vecPos = mesh->mVertices[v];
                            if (flipAxis)
                                swap(vecPos.y, vecPos.z);
                            CopyMemory(&vertexBuffer[vertexOffset], &vecPos, 12);
                            ProcessBoundBox(n.boundMin, n.boundMax, n.anyVertexProcessed, vecPos);
                        }
                        break;
                    case Shader::Normal:
                        if (d.type == Shader::Float3 && mesh->mNormals) {
                            aiVector3D vecNormal = mesh->mNormals[v];
                            if (flipAxis)
                                swap(vecNormal.y, vecNormal.z);
                            CopyMemory(&vertexBuffer[vertexOffset], &vecNormal, 12);
                        }
                        break;
                    case Shader::Color0:
                        {
                            aiColor4D vertexColor;
                            if ((d.type == Shader::D3DColor || d.type == Shader::UByte4) && numColors > 0 && mesh->mColors[0]) {
                                aiColor4D vertexColor = mesh->mColors[0][v];
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
                            if (hasMatColor) {
                                vertexColor.r *= matColor.r;
                                vertexColor.g *= matColor.g;
                                vertexColor.b *= matColor.b;
                            }
                            if (hasMatAlpha)
                                vertexColor.a *= matAlpha;
                            unsigned char rgba[4];
                            for (unsigned int clr = 0; clr < 4; clr++)
                                rgba[clr] = unsigned char(mesh->mColors[0][v][clr] * 255);
                            CopyMemory(&vertexBuffer[vertexOffset], rgba, 4);
                        }
                        break;
                    case Shader::Color1:
                        // skinning info - TODO
                        break;
                    case Shader::Texcoord0:
                        if (d.type == Shader::Float2 && numTexCoords > 0 && mesh->mTextureCoords[0])
                            CopyMemory(&vertexBuffer[vertexOffset], &mesh->mTextureCoords[0][v], 8);
                        break;
                    case Shader::Texcoord1:
                        if (d.type == Shader::Float2 && numTexCoords > 1 && mesh->mTextureCoords[1])
                            CopyMemory(&vertexBuffer[vertexOffset], &mesh->mTextureCoords[1][v], 8);
                        break;
                    case Shader::Texcoord2:
                        if (d.type == Shader::Float2 && numTexCoords > 2 && mesh->mTextureCoords[2])
                            CopyMemory(&vertexBuffer[vertexOffset], &mesh->mTextureCoords[2][v], 8);
                        break;
                    case Shader::BlendIndices:
                        // TODO
                        break;
                    case Shader::BlendWeight:
                        // TODO
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
                    bufData.Put(numPrimitives);
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
                    bufData.Put(vertexBuffer, vertexBufferSize);
                    bufData.Put(ONE);
                    break;
                case Shader::IndexData:
                    indexBufferOffset = bufData.Position();
                    globalArgs.emplace_back(bufData.Position(), numIndices);
                    bufData.Put(indexBuffer, indexBufferSize);
                    bufData.Put(ONE);
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
                    {
                        globalArgs.emplace_back(modifiables.GetArg("Coordinate4::BaseColour", bufData, vec1111, true));
                    }
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
                case Shader::UVOffset0:
                    {
                        Vector4D uvOffset0;
                        globalArgs.emplace_back(modifiables.GetArg("Coordinate4::$LAYERNAME$::UVOffset0", bufData, uvOffset0, false, false, n.name));
                    }
                    break;
                case Shader::UVOffset1:
                    {
                        Vector4D uvOffset1;
                        globalArgs.emplace_back(modifiables.GetArg("Coordinate4::$LAYERNAME$::UVOffset1", bufData, uvOffset1, false, false, n.name));
                    }
                    break;
                case Shader::XFade:
                    {
                        Vector4D xFade;
                        globalArgs.emplace_back(modifiables.GetArg("Coordinate4::$LAYERNAME$::XFade", bufData, xFade, false, false, n.name));
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
                        else if (arg == Shader::IndexData) {
                            relocations[""].push_back(bufData.Position());
                            bufData.Put(indexBufferOffset);
                        }
                        else if (arg == Shader::IndexCount)
                            bufData.Put(numIndices);
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
            unsigned int shaderNameOffset = bufData.Position();
            bufData.Put(shader->name);
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
            bufData.Put(shaderNameOffset);
            bufData.Put(ZERO);
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

            delete[] vertexBuffer;
            delete[] indexBuffer;

            meshCounter++;
        }
        nodeCounter++;
    }
    // BBOX
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
    if (false) {
        bufData.Align(16);
        // bones
        // TODO
        // skeleton
        // TODO
    }
    static unsigned char TTARSig[] = { 'T', 'T', 'A', 'R' };
    bufData.Put(TTARSig, std::size(TTARSig));
    // ModelLayers
    unsigned int modelLayersOffset = bufData.Position();
    bufData.Put(ZERO);
    for (auto const &n : nodes) {
        bufData.Put(n.renderDescriptorsOffsets.size());
        for (auto const &d : n.renderDescriptorsOffsets) {
            relocations[""].push_back(bufData.Position());
            bufData.Put(d);
        }
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
        for (auto &m : modifiables.vec)
            Replace(m.name, "$LAYERNAME$", m.layerName);
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
    // TODO
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
            bufData.Put(unsigned short(1));
            bufData.Put(unsigned short(1));
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
        bufData.Put(isSkinned ? ANIM_VERSION : ZERO);
        bufData.Put(ZERO);
    }
    bufData.Align(16);
    if (options().embeddedTextures) { // TODO
        for (auto const &[id, t] : textures) {
            // load texture from disk
            // convert to fsh shape
            symbols.emplace_back("__SHAPE::shape_" + t.name, bufData.Position());
            // bufData.Put(shapeData, shapeDataSize);
            bufData.Align(16);
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
    bufSymbolNames.Put(string("__OTOOLS_VERSION:::OTOOLS_VERSION-") + IMPORTER_VERSION);
    bufSymbols.Put(Elf32_Sym(bufSymbolNames.Position(), 0, 4, 0x21, 0, 1));
    bufSymbolNames.Put("__EAGL_TOOLLIB_VERSION:::EAGL_TOOLLIB_VERSION-4");
    
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
        metadata += "<ToolsVersion>" + string(IMPORTER_VERSION) + "</ToolsVersion>";
        metadata += "<TimeStamp>" + timess.str() + "</TimeStamp>";
        //metadata += "<Author>" + "</Author>";
        metadata += "<OriginalFileName>" + out.filename().string() + "</OriginalFileName>";
        metadata += "<SourceFile>" + in.string() + "</SourceFile>";
        metadata += "</Metadata>";
        bufMetadata.Put(metadata);
        bufMetadata.Align(16);
    }

    string versionMessage = "This file was generated with otools version ";
    versionMessage += IMPORTER_VERSION;

    BinaryBuffer bufElf;
    Elf32_Ehdr header;
    ZeroMemory(header);
    static unsigned char elfSig[] = { 0x7F, 0x45, 0x4C, 0x46, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
    unsigned int headerBlockSize = GetAligned(sizeof(Elf32_Ehdr), 16);
    CopyMemory(&header.e_ident, elfSig, sizeof(elfSig));
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
    bufElf.WriteToFile(out);
}
