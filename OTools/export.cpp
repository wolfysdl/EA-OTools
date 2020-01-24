#include "main.h"
#include <fstream>
#include "binbuf.h"

class exporter {
    string mResult;
    unsigned int mCurrentSpacing = 0;
    bool mJustOpened = true;
    bool mJustClosed = false;

    struct FileSymbol : public Elf32_Sym {
        unsigned int id = 0;
        string name;
    };

    string spacing() {
        if (mCurrentSpacing > 0)
            return string(mCurrentSpacing * 4, L' ');
        return string();
    };

    void startScope() {
        mResult.clear();
        mCurrentSpacing = 0;
        mJustOpened = true;
        mJustClosed = false;
        mResult += "{";
        mCurrentSpacing++;
    }

    void endScope() {
        mResult += "\n}\n";
    }

    void openScope(string const &title = string()) {
        if (!mJustOpened)
            mResult += ",";
        mResult += "\n" + spacing();
        if (!title.empty())
            mResult += "\"" + title + "\" : ";
        mResult += "{";
        mJustOpened = true;
        mCurrentSpacing++;
    };

    void openArray(string const &title = string()) {
        if (!mJustOpened)
            mResult += ",";
        mResult += "\n" + spacing();
        if (!title.empty())
            mResult += "\"" + title + "\" : ";
        mResult += "[";
        mJustOpened = true;
        mCurrentSpacing++;
    };

    void closeScope(bool isLast = false) {
        mJustOpened = false;
        mCurrentSpacing--;
        mResult += "\n" + spacing() + "}";
    };

    void closeArray(bool isLast = false) {
        mJustOpened = false;
        mCurrentSpacing--;
        mResult += "\n" + spacing() + "]";
    };

    void writeValueString(string const &value) {
        mResult += (mJustOpened ? "\n" : ",\n") + spacing() + "\"" + value + "\"";
        mJustOpened = false;
    };

    void writeValueInt(int value) {
        mResult += (mJustOpened ? "\n" : ",\n") + spacing() + to_string(value);
        mJustOpened = false;
    };

    void writeValueFloat(float value) {
        mResult += (mJustOpened ? "\n" : ",\n") + spacing() + Format("%.15g", value);
        mJustOpened = false;
    };

    void writeValueDouble(double value) {
        mResult += (mJustOpened ? "\n" : ",\n") + spacing() + to_string(value);
        mJustOpened = false;
    };

    void writeValuebool(bool value) {
        mResult += (mJustOpened ? "\n" : ",\n") + spacing() + (value ? "true" : "false");
        mJustOpened = false;
    };

    void writeFieldString(string const &name, string const &value) {
        mResult += (mJustOpened ? "\n" : ",\n") + spacing() + "\"" + name + "\" : \"" + value + "\"";
        mJustOpened = false;
    };

    void writeFieldInt(string const &name, int value) {
        mResult += (mJustOpened ? "\n" : ",\n") + spacing() + "\"" + name + "\" : " + to_string(value);
        mJustOpened = false;
    };

    void writeFieldFloat(string const &name, float value) {
        mResult += (mJustOpened ? "\n" : ",\n") + spacing() + "\"" + name + "\" : " + Format("%.15g", value);
        mJustOpened = false;
    };

    void writeFieldDouble(string const &name, double value) {
        mResult += (mJustOpened ? "\n" : ",\n") + spacing() + "\"" + name + "\" : " + to_string(value);
        mJustOpened = false;
    };

    void writeFieldBool(string const &name, bool value) {
        mResult += (mJustOpened ? "\n" : ",\n") + spacing() + "\"" + name + "\" : " + (value ? "true" : "false");
        mJustOpened = false;
    };

    string base64_encode(unsigned char const *bytes_to_encode, unsigned int in_len) {
        static const string base64_chars =
            "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
            "abcdefghijklmnopqrstuvwxyz"
            "0123456789+/";
        string ret;
        int i = 0;
        int j = 0;
        unsigned char char_array_3[3];
        unsigned char char_array_4[4];

        while (in_len--) {
            char_array_3[i++] = *(bytes_to_encode++);
            if (i == 3) {
                char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
                char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
                char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
                char_array_4[3] = char_array_3[2] & 0x3f;

                for (i = 0; (i < 4); i++)
                    ret += base64_chars[char_array_4[i]];
                i = 0;
            }
        }
        if (i)
        {
            for (j = i; j < 3; j++)
                char_array_3[j] = '\0';

            char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
            char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
            char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);

            for (j = 0; (j < i + 1); j++)
                ret += base64_chars[char_array_4[j]];

            while ((i++ < 3))
                ret += '=';

        }
        return ret;
    }

    bool compareCommands(unsigned int a, unsigned int b) {
        if ((a == 31 || a == 46) && (b == 31 || b == 46))
            return true;
        return a == b;
    }

    struct Vector3 { float x, y, z; };
    struct Vector4 { float x, y, z, w; };
    struct Matrix4x4 { Vector4 vec1, vec2, vec3, posn; };
    struct BBOX { Vector3 min, max; };

    struct Model {
        void *mModifiableData;
        unsigned int mNumModifiableDatas;
        unsigned int mNumVariations;
        Matrix4x4 mTransform;
        Vector4 field_4C;
        Vector4 field_5C;
        Vector4 mBoundMin;
        Vector4 mBoundMax;
        Vector4 mCenter;
        unsigned int mNumLayers;
        char const **mLayerNames;
        unsigned int field_A4;
        Model *field_A8;
        Model *field_AC;
        Model *mNext;
        char const *mName;
        void *mMorphVertexInfo;
        void *mTextures;
        unsigned int mVariationID;
        void *mLayersStates;
        unsigned int mIsRenderable;
        void *mLayers;
        unsigned int field_D0;
        unsigned int field_D4;
        unsigned int mSkeletonVersion;
        unsigned int mLastPresentCall;
    };

    struct RenderMethod {
        unsigned char *mCodeBlock;
        unsigned char **mCodeToUse;
        void *mMicroCode;
        void *mETTObject;
        RenderMethod *mNext;
        char const **mpParameterNames;
        unsigned int field_18;
        int field_1C;
        RenderMethod *mParent;
        void *mGeoPrimDataBuffer;
        int mCurrentTechnique;
        char const *mShaderName;
        void *mEAGLModel;
    };

    struct GeometryInfo {
        unsigned int mNumIndices;
        unsigned int mNumVertices;
        unsigned int mNumPrimitives;
        unsigned int unknown;
    };

    struct Bone {
        unsigned int mIndex;
        unsigned int unknown1;
        unsigned int unknown2;
        unsigned int unknown3;
    };

    struct BoneState {
        Vector3 unknown1;
        int mParentBoneId;
        Vector4 unknown2;
        float unknown3;
        float unknown4;
        float unknown5;
        float unknown6;
        Matrix4x4 mInvBaseTransform;
    };

    struct Skeleton {
        unsigned short mSignature1;
        unsigned short unknown1;
        unsigned int mSignature2;
        unsigned int mNumBones;
        void *unknown2;
    };

    struct Shader {
        enum DataType {
            NoType, Float2, Float3, Float4, D3DColor, UByte4
        };
        enum DataUsage {
            NoUsage, Position, Normal, Color0, Color1, Texcoord0, Texcoord1, Texcoord2, BlendIndices, BlendWeight
        };
        struct VertexDeclElement {
            DataType type = NoType;
            DataUsage usage = NoUsage;
        };
        
        unsigned int numTechniques;
        vector<VertexDeclElement> declaration;

        Shader() {}

        Shader(unsigned int _numTechniques, vector<VertexDeclElement> const &_declaration) {
            numTechniques = _numTechniques;
            declaration = _declaration;
        }

        Shader(unsigned int _numTechniques) {
            numTechniques = _numTechniques;
        }
    };

    struct Buffer {
        enum Type {
            NotSet, Vertex, Index, VertexSkin, Matrix
        };
        Type type = NotSet;
        void *data = nullptr;
        unsigned int length = 0;
        unsigned int stride = 0;
    };

    struct Accessor {
        unsigned int componentType = 0;
        unsigned int count = 0;
        string type;
        unsigned int buffer = 0;
        unsigned int length = 0;
        unsigned int offset = 0;
        unsigned int stride = 0;
        Buffer::Type bufferType;
        bool normalized = false;
        bool usesMinMax = false;
        Vector3 min;
        Vector3 max;
    };

    struct VertexSkinDataPacked {
        Vector3 packedData;
        unsigned int padding;
    };

    struct VertexSkinData {
        unsigned char indices[4];
        Vector4 weights;
    };

    struct Texture {
        unsigned int unknown1;
        char tag[4];
        unsigned int unknown2;
        float unknown3;
        float unknown4;
        unsigned int unknown5;
        unsigned int wrapU;
        unsigned int wrapV;
        unsigned int wrapW;
        unsigned int unknown6;
        unsigned int unknown7;
    };

    struct Material {
        unsigned int id = 0;
        string shader;
        int textures[3] = { -1, -1, -1 };
        bool doubleSided = false;
        string alphaMode;
        float alphaCutoff = 0.0f;
    };

    struct TexDesc {
        unsigned int symbolId;
        Texture *pTex;
        string name;
        string source;
        string mimeType;
        unsigned int wrapU = 33071;
        unsigned int wrapV = 33071;
        unsigned int minFilter = 9729;
        unsigned int magFilter = 9729;
    };

    struct GeoPrimState {
        unsigned int nPrimitiveType;
        unsigned int nShading;
        unsigned int bCullEnable;
        unsigned int nCullDirection;
        unsigned int nDepthTestMetod;
        unsigned int nAlphaBlendMode;
        unsigned int bAlphaTestEnable;
        unsigned int nAlphaCompareValue;
        unsigned int nAlphaTestMethod;
        unsigned int bTextureEnable;
        unsigned int nTransparencyMethod;
        unsigned int nFillMode;
        unsigned int nBlendOperation;
        unsigned int nSrcBlend;
        unsigned int nDstBlend;
        float fNumPatchSegments;
        unsigned int nZWritesEnable;
    };
public:
    void convert_o_to_gltf(unsigned char *fileData, unsigned int fileDataSize, path const &outPath) {

        unsigned char *data = nullptr;
        unsigned int dataSize = 0;
        Elf32_Sym *symbolsData = nullptr;
        unsigned int numSymbols = 0;
        Elf32_Rel *rel = nullptr;
        unsigned int numRelocations = 0;
        char *symbolNames = nullptr;
        unsigned int symbolNamesSize = 0;
        unsigned int dataIndex = 0;

        Elf32_Ehdr *h = (Elf32_Ehdr *)fileData;
        Elf32_Shdr *s = At<Elf32_Shdr>(h, h->e_shoff);
        for (unsigned int i = 0; i < h->e_shnum; i++) {
            if (s[i].sh_size > 0) {
                if (s[i].sh_type == 1 && !data) {
                    data = At<unsigned char>(h, s[i].sh_offset);
                    dataSize = s[i].sh_size;
                    dataIndex = i;
                }
                else if (s[i].sh_type == 2) {
                    symbolsData = At<Elf32_Sym>(h, s[i].sh_offset);
                    numSymbols = s[i].sh_size / 16;
                }
                else if (s[i].sh_type == 3) {
                    symbolNames = At<char>(h, s[i].sh_offset);
                    symbolNamesSize = s[i].sh_size;
                }
                else if (s[i].sh_type == 9) {
                    rel = At<Elf32_Rel>(h, s[i].sh_offset);
                    numRelocations = s[i].sh_size / 8;
                }
            }
        }

        static map<string, Shader> shaderInfo = {
            { "PlayerIDShader", { 9, { { Shader::Float3, Shader::Position }, { Shader::D3DColor, Shader::Color1 }, { Shader::Float3, Shader::Normal }, { Shader::D3DColor, Shader::Color0 }, { Shader::UByte4, Shader::BlendIndices }, { Shader::Float4, Shader::BlendWeight } } } },
            { "ClipTextureAlphablend", { 9, { { Shader::Float3, Shader::Position }, { Shader::D3DColor, Shader::Color0 }, { Shader::Float2, Shader::Texcoord0 } } } },
            { "PitchLitMow", { 9, { { Shader::Float3, Shader::Position }, { Shader::D3DColor, Shader::Color0 }, { Shader::Float2, Shader::Texcoord0 }, { Shader::Float2, Shader::Texcoord1 }, { Shader::Float2, Shader::Texcoord2 } } } },
            { "LitTextureIrradEnvmap_Skin", { 3, { { Shader::Float3, Shader::Position }, { Shader::D3DColor, Shader::Color1 }, { Shader::Float3, Shader::Normal }, { Shader::D3DColor, Shader::Color0 }, { Shader::Float2, Shader::Texcoord0 }, { Shader::UByte4, Shader::BlendIndices }, { Shader::Float4, Shader::BlendWeight } } } },
            { "PostFX", { 2, { { Shader::Float4, Shader::Position }, { Shader::Float3, Shader::Texcoord0 }, { Shader::D3DColor, Shader::Color0 } } } },
            { "Gouraud_Skin", { 9, { { Shader::Float3, Shader::Position }, { Shader::D3DColor, Shader::Color1 }, { Shader::Float3, Shader::Normal }, { Shader::D3DColor, Shader::Color0 }, { Shader::UByte4, Shader::BlendIndices }, { Shader::Float4, Shader::BlendWeight } } } },
            { "LitTextureIrradEnvmap", { 9, { { Shader::Float3, Shader::Position }, { Shader::Float3, Shader::Normal }, { Shader::D3DColor, Shader::Color0 }, { Shader::Float2, Shader::Texcoord0 } } } },
            { "LitTexture2IrradSkinSubSurfSpec", { 9, { { Shader::Float3, Shader::Position }, { Shader::D3DColor, Shader::Color1 }, { Shader::Float3, Shader::Normal }, { Shader::D3DColor, Shader::Color0 }, { Shader::Float2, Shader::Texcoord0 }, { Shader::UByte4, Shader::BlendIndices }, { Shader::Float4, Shader::BlendWeight } } } },
            { "LitTextureIrradSpecMap_Skin", { 9, { { Shader::Float3, Shader::Position }, { Shader::D3DColor, Shader::Color1 }, { Shader::Float3, Shader::Normal }, { Shader::D3DColor, Shader::Color0 }, { Shader::Float2, Shader::Texcoord0 }, { Shader::UByte4, Shader::BlendIndices }, { Shader::Float4, Shader::BlendWeight } } } },
            { "ClipTextureNodepthwriteWithAlpha", { 9, { { Shader::Float3, Shader::Position }, { Shader::D3DColor, Shader::Color0 }, { Shader::Float2, Shader::Texcoord0 } } } },
            { "GouraudApt", { 2, { { Shader::Float3, Shader::Position } } } },
            { "LitGouraud", { 2, { { Shader::Float3, Shader::Position }, { Shader::Float3, Shader::Normal }, { Shader::D3DColor, Shader::Color0 } } } },
            { "LitTexture2IrradSpecMap_Skin", { 17, { { Shader::Float3, Shader::Position }, { Shader::D3DColor, Shader::Color1 }, { Shader::Float3, Shader::Normal }, { Shader::D3DColor, Shader::Color0 }, { Shader::Float2, Shader::Texcoord0 }, { Shader::Float2, Shader::Texcoord1 }, { Shader::UByte4, Shader::BlendIndices }, { Shader::Float4, Shader::BlendWeight } } } },
            { "LitTexture4Head_Skin", { 3, { { Shader::Float3, Shader::Position }, { Shader::D3DColor, Shader::Color1 }, { Shader::Float3, Shader::Normal }, { Shader::D3DColor, Shader::Color0 }, { Shader::Float2, Shader::Texcoord0 }, { Shader::Float2, Shader::Texcoord1 }, { Shader::UByte4, Shader::BlendIndices }, { Shader::Float4, Shader::BlendWeight } } } },
            { "LitTexture2Hair_Skin", { 18, { { Shader::Float3, Shader::Position }, { Shader::D3DColor, Shader::Color1 }, { Shader::Float3, Shader::Normal }, { Shader::D3DColor, Shader::Color0 }, { Shader::Float2, Shader::Texcoord0 }, { Shader::Float2, Shader::Texcoord1 }, { Shader::UByte4, Shader::BlendIndices }, { Shader::Float4, Shader::BlendWeight } } } },
            { "NewMethod2", { 9, { { Shader::Float3, Shader::Position }, { Shader::D3DColor, Shader::Color0 }, { Shader::Float2, Shader::Texcoord0 } } } },
            { "PlanarShadow", { 12, { { Shader::Float3, Shader::Position }, { Shader::D3DColor, Shader::Color0 } } } },
            { "ClipTextureModulateNodepthwrite", { 9, { { Shader::Float3, Shader::Position }, { Shader::D3DColor, Shader::Color0 }, { Shader::Float2, Shader::Texcoord0 } } } },
            { "LitTexture2x_Skin", { 9, { { Shader::Float3, Shader::Position }, { Shader::D3DColor, Shader::Color1 }, { Shader::Float3, Shader::Normal }, { Shader::D3DColor, Shader::Color0 }, { Shader::Float2, Shader::Texcoord0 }, { Shader::UByte4, Shader::BlendIndices }, { Shader::Float4, Shader::BlendWeight } } } },
            { "ClipTextureNoalphablend", { 9, { { Shader::Float3, Shader::Position }, { Shader::D3DColor, Shader::Color0 }, { Shader::Float2, Shader::Texcoord0 } } } },
            { "LitTexture2x", { 9, { { Shader::Float3, Shader::Position }, { Shader::Float3, Shader::Normal }, { Shader::D3DColor, Shader::Color0 }, { Shader::Float2, Shader::Texcoord0 } } } },
            { "Texture2x_Skin", { 9, { { Shader::Float3, Shader::Position }, { Shader::UByte4, Shader::BlendIndices }, { Shader::D3DColor, Shader::Color0 }, { Shader::Float2, Shader::Texcoord0 } } } },
            { "BloomExtractHot", { 2, { { Shader::Float4, Shader::Position }, { Shader::D3DColor, Shader::Color0 }, { Shader::Float3, Shader::Texcoord0 } } } },
            { "LitTextureEye_Skin", { 9, { { Shader::Float3, Shader::Position }, { Shader::D3DColor, Shader::Color1 }, { Shader::Float3, Shader::Normal }, { Shader::D3DColor, Shader::Color0 }, { Shader::Float2, Shader::Texcoord0 }, { Shader::UByte4, Shader::BlendIndices }, { Shader::Float4, Shader::BlendWeight } } } },
            { "LitTexture2Alpha2x_Skin", { 18, { { Shader::Float3, Shader::Position }, { Shader::D3DColor, Shader::Color1 }, { Shader::Float3, Shader::Normal }, { Shader::D3DColor, Shader::Color0 }, { Shader::Float2, Shader::Texcoord0 }, { Shader::Float2, Shader::Texcoord1 }, { Shader::UByte4, Shader::BlendIndices }, { Shader::Float4, Shader::BlendWeight } } } },
            { "BloomTint", { 2, { { Shader::Float4, Shader::Position }, { Shader::D3DColor, Shader::Color0 }, { Shader::Float3, Shader::Texcoord0 } } } },
            { "PlanarShadow_Skin", { 15, { { Shader::Float3, Shader::Position }, { Shader::UByte4, Shader::BlendIndices }, { Shader::D3DColor, Shader::Color0 } } } },
            { "DATexture", { 2, { { Shader::Float4, Shader::Position }, { Shader::D3DColor, Shader::Color0 }, { Shader::Float4, Shader::Texcoord0 } } } },
            { "XFadeScrollTexture", { 9, { { Shader::Float3, Shader::Position }, { Shader::D3DColor, Shader::Color0 }, { Shader::Float2, Shader::Texcoord0 } } } },
            { "LitTexture2IrradSkinSubSurfSpecGameface", { 13, { { Shader::Float3, Shader::Position }, { Shader::D3DColor, Shader::Color1 }, { Shader::Float3, Shader::Normal }, { Shader::D3DColor, Shader::Color0 }, { Shader::Float2, Shader::Texcoord0 }, { Shader::UByte4, Shader::BlendIndices }, { Shader::Float4, Shader::BlendWeight } } } },
            { "NewMethod1", { 9, { { Shader::Float3, Shader::Position }, { Shader::D3DColor, Shader::Color0 }, { Shader::Float2, Shader::Texcoord0 } } } },
            { "ClipTextureNodepthwrite", { 9, { { Shader::Float3, Shader::Position }, { Shader::D3DColor, Shader::Color0 }, { Shader::Float2, Shader::Texcoord0 } } } },
            { "DAGouraud", { 2, { { Shader::Float4, Shader::Position }, { Shader::D3DColor, Shader::Color0 } } } },
            { "TextureApt", { 4, { { Shader::Float3, Shader::Position } } } },
            { "ClipTextureNodepthwriteFade", { 9, { { Shader::Float3, Shader::Position }, { Shader::D3DColor, Shader::Color0 }, { Shader::Float2, Shader::Texcoord0 } } } },
            { "Texture2x", { 9, { { Shader::Float3, Shader::Position }, { Shader::D3DColor, Shader::Color0 }, { Shader::Float2, Shader::Texcoord0 } } } },
            { "LitTexture2Haircap_Skin", { 18, { { Shader::Float3, Shader::Position }, { Shader::D3DColor, Shader::Color1 }, { Shader::Float3, Shader::Normal }, { Shader::D3DColor, Shader::Color0 }, { Shader::Float2, Shader::Texcoord0 }, { Shader::Float2, Shader::Texcoord1 }, { Shader::UByte4, Shader::BlendIndices }, { Shader::Float4, Shader::BlendWeight } } } },
            { "Gouraud", { 2, { { Shader::Float3, Shader::Position }, { Shader::D3DColor, Shader::Color0 } } } },
            { "IrradLitTextureTransparent2x", { 9, { { Shader::Float3, Shader::Position }, { Shader::Float3, Shader::Normal }, { Shader::D3DColor, Shader::Color0 }, { Shader::Float2, Shader::Texcoord0 } } } },
            { "IrradLitTextureColored2x", { 6, { { Shader::Float3, Shader::Position }, { Shader::Float3, Shader::Normal }, { Shader::D3DColor, Shader::Color0 }, { Shader::Float2, Shader::Texcoord0 } } } },
            { "IrradLitTextureEnvmapMasked2x", { 3, { { Shader::Float3, Shader::Position }, { Shader::Float3, Shader::Normal }, { Shader::D3DColor, Shader::Color0 }, { Shader::Float2, Shader::Texcoord0 } } } },
            { "XFadeFixTexture", { 9, { { Shader::Float3, Shader::Position }, { Shader::Float3, Shader::Normal }, { Shader::D3DColor, Shader::Color0 } } } },
            { "XFadeDigitalScrollTexture", { 3, { { Shader::Float3, Shader::Position }, { Shader::Float3, Shader::Normal }, { Shader::D3DColor, Shader::Color0 } } } },
            { "IrradLitTexture2x", { 9, { { Shader::Float3, Shader::Position }, { Shader::Float3, Shader::Normal }, { Shader::D3DColor, Shader::Color0 }, { Shader::Float2, Shader::Texcoord0 } } } },
            { "IrradLitTextureColoured2x", { 6, { { Shader::Float3, Shader::Position }, { Shader::Float3, Shader::Normal }, { Shader::D3DColor, Shader::Color0 } } } },
            { "ClipTextureAddNodepthwrite", { 9, { { Shader::Float3, Shader::Position }, { Shader::D3DColor, Shader::Color0 }, { Shader::Float2, Shader::Texcoord0 } } } },
            { "IrradLitGouraud2x", { 3, { { Shader::Float3, Shader::Position }, { Shader::Float3, Shader::Normal }, { Shader::D3DColor, Shader::Color0 } } } },
            { "IrradLitTextureColouredTransparent2x", { 6, { { Shader::Float3, Shader::Position }, { Shader::Float3, Shader::Normal }, { Shader::D3DColor, Shader::Color0 } } } },
            { "IrradLitTextureColoredTransparent2x", { 6, { { Shader::Float3, Shader::Position }, { Shader::Float3, Shader::Normal }, { Shader::D3DColor, Shader::Color0 }, { Shader::Float2, Shader::Texcoord0 } } } },
            { "IrradLitTextureEnvmapTransparent2x", { 3, { { Shader::Float3, Shader::Position }, { Shader::Float3, Shader::Normal }, { Shader::D3DColor, Shader::Color0 }, { Shader::Float2, Shader::Texcoord0 } } } },
            //{ "XFadeTexture", 4 }
            { "FIFACrowda", { 9, { { Shader::Float3, Shader::Position }, { Shader::D3DColor, Shader::Color0 }, { Shader::Float2, Shader::Texcoord0 } } } },
            { "FIFACrowdh", { 9, { { Shader::Float3, Shader::Position }, { Shader::D3DColor, Shader::Color0 }, { Shader::Float2, Shader::Texcoord0 } } } },
        };

        // get symbols

        vector<FileSymbol> symbols;

        symbols.resize(numSymbols);

        for (unsigned int i = 0; i < numSymbols; i++) {
            symbols[i].st_info = symbolsData[i].st_info;
            symbols[i].st_name = symbolsData[i].st_name;
            symbols[i].st_other = symbolsData[i].st_other;
            symbols[i].st_shndx = symbolsData[i].st_shndx;
            symbols[i].st_size = symbolsData[i].st_size;
            symbols[i].st_value = symbolsData[i].st_value;
            symbols[i].name = &symbolNames[symbolsData[i].st_name];
            symbols[i].id = i;
        }

        auto isSymbolDataPresent = [&](FileSymbol const &s) {
            return (s.st_info & 0xF) != 0 && s.st_shndx == dataIndex;
        };

        // get relocation symbols

        map<unsigned int, FileSymbol> symbolRelocations;

        for (unsigned int i = 0; i < numRelocations; i++) {
            if (rel[i].r_info_sym < symbols.size())
                symbolRelocations[rel[i].r_offset] = symbols[rel[i].r_info_sym];
        }

        // do relocations

        for (unsigned int i = 0; i < numRelocations; i++) {
            auto symbolId = rel[i].r_info_sym;
            if (symbolId < numSymbols && isSymbolDataPresent(symbols[symbolId]))
                SetAt(data, rel[i].r_offset, &data[GetAt<unsigned int>(data, rel[i].r_offset)]);
        }

        // find model

        vector<Model *> models;
        vector<RenderMethod *> renderMethods;
        vector<void *> geoPrimDataBuffers;
        Skeleton *skeleton = nullptr;
        vector<Bone *> bones;
        vector<string> boneNames;
        vector<VertexSkinData *> vertexSkinBuffers;
        Matrix4x4 *skinMatrices = nullptr;
        vector<Material> materials;
        vector<TexDesc> textures;

        for (auto const &s : symbols) {
            if (isSymbolDataPresent(s)) {
                if (s.name.starts_with("__Model:::"))
                    models.push_back(At<Model>(data, s.st_value));
                else if (s.name.starts_with("__RenderMethod:::"))
                    renderMethods.push_back(At<RenderMethod>(data, s.st_value));
                else if (s.name.starts_with("__geoprimdatabuffer"))
                    geoPrimDataBuffers.push_back(At<void>(data, s.st_value));
                else if (s.name.starts_with("__Bone:::")) {
                    bones.push_back(At<Bone>(data, s.st_value));
                    string boneName = s.name.substr(9);
                    auto dotPos = boneName.find_last_of('.');
                    if (dotPos != string::npos)
                        boneName = boneName.substr(dotPos + 1);
                    boneNames.push_back(boneName);
                }
                else if (s.name.starts_with("__Skeleton:::")) {
                    if (!skeleton)
                        skeleton = At<Skeleton>(data, s.st_value);
                }
            }
        }

        // TODO: validate skeleton

        if (skeleton && skeleton->mNumBones != bones.size())
            skeleton = nullptr;

        // TODO: validate RMs and GPBs

        Model *model = nullptr;

        if (!models.empty()) {
            if (models.size() != 1)
                sort(models.begin(), models.end(), [](Model *a, Model *b) { return a->mVariationID <= b->mVariationID; });
            model = models[0];
        }

        startScope();
        openScope("asset");
        writeFieldString("generator", "EA Sports .O to Khronos glTF - converter");
        writeFieldString("version", "2.0");
        closeScope();
        writeFieldInt("scene", 0);
        if ((model && model->mNumLayers && model->mLayers) || skeleton) {
            // scenes
            openArray("scenes");
            openScope();
            if (model && model->mName)
                writeFieldString("name", model->mName);
            openArray("nodes");
            unsigned int numNodes = (model ? model->mNumLayers : 0) + (skeleton ? 1 : 0);
            for (unsigned int i = 0; i < numNodes; i++)
                writeValueInt(i);
            closeArray();
            closeScope();
            closeArray();
            // nodes
            openArray("nodes");
            if (model) {
                for (unsigned int i = 0; i < model->mNumLayers; i++) {
                    openScope();
                    writeFieldInt("mesh", i);
                    if (skeleton)
                        writeFieldInt("skin", 0);
                    if (model->mLayerNames[i])
                        writeFieldString("name", model->mLayerNames[i]);
                    closeScope();
                }
            }
            if (skeleton) {
                struct SkeletonNode {
                    unsigned int index = 0;
                    SkeletonNode *parent = nullptr;
                    vector<SkeletonNode *> children;
                    string name;
                };
                vector<SkeletonNode> skelNodes;
                skelNodes.resize(bones.size());
                for (unsigned int b = 0; b < bones.size(); b++) {
                    auto const &bone = bones[b];
                    if (bone->mIndex < skelNodes.size()) {
                        skelNodes[bone->mIndex].index = bone->mIndex;
                        skelNodes[bone->mIndex].name = boneNames[b];
                    }
                }
                sort(skelNodes.begin(), skelNodes.end(), [](SkeletonNode const &a, SkeletonNode const &b) {
                    return a.index <= b.index;
                });
                BoneState *boneStates = At<BoneState>(skeleton, 0x10);
                for (unsigned int b = 0; b < skelNodes.size(); b++) {
                    int parentId = boneStates[b].mParentBoneId;
                    if (parentId >= 0 && parentId < int(skelNodes.size())) {
                        auto parent = &skelNodes[parentId];
                        skelNodes[b].parent = parent;
                        parent->children.push_back(&skelNodes[b]);
                    }
                }
                unsigned int nodeIndex = model ? model->mNumLayers : 0;
                openScope();
                writeFieldString("name", "Skeleton");
                vector<unsigned int> skelRootNodes;
                for (unsigned int i = 0; i < skelNodes.size(); i++) {
                    if (!skelNodes[i].parent)
                        skelRootNodes.push_back(skelNodes[i].index);
                }
                if (skelRootNodes.size() > 0) {
                    openArray("children");
                    for (unsigned int i = 0; i < skelRootNodes.size(); i++)
                        writeValueInt(nodeIndex + 1 + skelRootNodes[i]);
                    closeArray();
                }
                closeScope();
                for (unsigned int i = 0; i < skelNodes.size(); i++) {
                    openScope();
                    writeFieldString("name", skelNodes[i].name);
                    openArray("rotation");
                    writeValueFloat(boneStates[i].unknown2.x);
                    writeValueFloat(boneStates[i].unknown2.y);
                    writeValueFloat(boneStates[i].unknown2.z);
                    writeValueFloat(boneStates[i].unknown2.w);
                    closeArray();
                    openArray("translation");
                    writeValueFloat(boneStates[i].unknown3);
                    writeValueFloat(boneStates[i].unknown4);
                    writeValueFloat(boneStates[i].unknown5);
                    closeArray();
                    //openArray("matrix");
                    //D3DXMATRIX *unkMat = At<D3DXMATRIX>(skeleton, 0x10 + 0x30 + sizeof(BoneState) * i);
                    //D3DXMATRIX mat = *unkMat;
                    //D3DXMatrixInverse(&mat, NULL, unkMat);
                    //for (unsigned int m = 0; m < 16; m++) {
                    //    writeValueFloat(GetAt<float>(&mat, m * 4));
                    //}
                    //closeArray();
                    if (skelNodes[i].children.size() > 0) {
                        openArray("children");
                        for (unsigned int c = 0; c < skelNodes[i].children.size(); c++)
                            writeValueInt(nodeIndex + 1 + skelNodes[i].children[c]->index);
                        closeArray();
                    }
                    closeScope();
                }
            }
            closeArray();
            vector<Buffer> buffers;
            vector<Accessor> accessors;
            // skins
            if (skeleton) {
                openArray("skins");
                openScope();
                if (bones.size() > 0) {
                    writeFieldInt("inverseBindMatrices", accessors.size());
                    Accessor a;
                    skinMatrices = new Matrix4x4[bones.size()];
                    for (unsigned int m = 0; m < bones.size(); m++) {
                        Matrix4x4 *unkMat = At<Matrix4x4>(skeleton, 0x10 + 0x30 + sizeof(BoneState) * m);
                        //D3DXMatrixInverse(&skinMatrices[m], NULL, unkMat);
                        //D3DXMATRIX tmpMat;
                        //D3DXMatrixInverse(&tmpMat, NULL, &skinMatrices[m]);
                        CopyMemory(&skinMatrices[m], unkMat, 64);
                    }
                    a.buffer = buffers.size();
                    a.componentType = 5126;
                    a.count = bones.size();
                    a.length = bones.size() * 64;
                    a.offset = 0;
                    a.type = "MAT4";
                    a.stride = 64;
                    a.bufferType = Buffer::Matrix;
                    accessors.push_back(a);
                    Buffer b;
                    b.data = skinMatrices;
                    b.length = bones.size() * 64;
                    b.type = Buffer::Matrix;
                    b.stride = 64;
                    buffers.push_back(b);
                }
                unsigned int skelRootNodeIndex = model ? model->mNumLayers : 0;
                writeFieldInt("skeleton", skelRootNodeIndex);
                if (bones.size() > 0) {
                    openArray("joints");
                    for (unsigned int i = 0; i < bones.size(); i++)
                        writeValueInt(skelRootNodeIndex + 1 + i);
                    closeArray();
                }
                closeScope();
                closeArray();
            }
            // meshes
            if (model && model->mNumLayers) {
                openArray("meshes");
                unsigned int *modelLayers = (unsigned int *)(model->mLayers);
                modelLayers++;
                for (unsigned int i = 0; i < model->mNumLayers; i++) {
                    openScope();
                    if (model->mLayerNames[i])
                        writeFieldString("name", model->mLayerNames[i]);
                    openArray("primitives");
                    unsigned int numPrimitives = *modelLayers;
                    modelLayers++;
                    for (unsigned int p = 0; p < numPrimitives; p++) {
                        void *renderDescriptor = GetAt<void *>(modelLayers, p * 4);
                        void *renderMethod = GetAt<void *>(renderDescriptor, 0);
                        void *globalParameters = At<void *>(renderDescriptor, 4);
                        GeometryInfo *geometryInfo = GetAt<GeometryInfo *>(globalParameters, 4);
                        unsigned int rmCodeOffset = At<unsigned char>(renderMethod, 8) - data;
                        void *vertexBuffer = nullptr;
                        unsigned int vertexSize = 0;
                        unsigned int numVertices = 0;
                        void *indexBuffer = nullptr;
                        unsigned int indexSize = 0;
                        unsigned int numIndices = 0;
                        VertexSkinDataPacked *skinVertexDataBuffer = nullptr;
                        unsigned int numSkinVertexInfos = 0;
                        GeoPrimState *geoPrimState = nullptr;
                        unsigned int geoPrimMode = 5;
                        Shader shader;
                        string shaderName;
                        int color1Offset = -1;
                        Material mat;
                        mat.id = materials.size();
                        //Error("%X", rmCodeOffset);
                        auto it = symbolRelocations.find(rmCodeOffset);
                        if (it != symbolRelocations.end() && (*it).second.st_info == 0x10) {
                            string codeName = (*it).second.name;
                            if (codeName.ends_with("__EAGLMicroCode")) {
                                shaderName = codeName.substr(0, codeName.length() - 15);
                                mat.shader = shaderName;
                                shader = shaderInfo[shaderName];
                                if (shader.numTechniques) {
                                    void *renderCode = GetAt<void *>(renderMethod, 0);
                                    unsigned int numCommands = 0;
                                    unsigned int commandOffset = 0;
                                    unsigned short id = GetAt<unsigned short>(renderCode, commandOffset + 2);
                                    unsigned short size = GetAt<unsigned short>(renderCode, commandOffset);
                                    while (id != 0 && (!vertexBuffer || !indexBuffer)) {
                                        switch (id) {
                                        case 4:
                                        case 75: //TODO
                                            if (!vertexBuffer) {
                                                vertexBuffer = GetAt<void *>(globalParameters, 4);
                                                numVertices = GetAt<unsigned int>(globalParameters, 0);
                                                vertexSize = GetAt<unsigned int>(renderCode, commandOffset + 8);
                                            }
                                            break;
                                        case 7:
                                            if (!indexBuffer) {
                                                indexBuffer = GetAt<void *>(globalParameters, 4);
                                                numIndices = GetAt<unsigned int>(globalParameters, 0); // GetAt<unsigned int>(renderCode, commandOffset + 20) - 1;
                                                indexSize = GetAt<unsigned int>(renderCode, commandOffset + 4);
                                            }
                                            break;
                                        case 28:
                                            if (!skinVertexDataBuffer) {
                                                numSkinVertexInfos = GetAt<unsigned int>(globalParameters, 0);
                                                skinVertexDataBuffer = GetAt<VertexSkinDataPacked *>(globalParameters, 4);
                                            }
                                            break;
                                        case 33:
                                            geoPrimState = GetAt<GeoPrimState *>(globalParameters, 4);
                                            if (geoPrimState) {
                                                if (geoPrimState->nPrimitiveType == 1)
                                                    geoPrimMode = 0;
                                                else if (geoPrimState->nPrimitiveType == 2)
                                                    geoPrimMode = 1;
                                                else if (geoPrimState->nPrimitiveType == 3)
                                                    geoPrimMode = 3;
                                                else if (geoPrimState->nPrimitiveType == 4)
                                                    geoPrimMode = 4;
                                                else if (geoPrimState->nPrimitiveType == 5)
                                                    geoPrimMode = 5;
                                                else if (geoPrimState->nPrimitiveType == 6)
                                                    geoPrimMode = 6;
                                            }
                                            else {
                                                it = symbolRelocations.find(unsigned int(At<GeoPrimState *>(globalParameters, 4)) - unsigned int(data));
                                                if (it != symbolRelocations.end() && (*it).second.st_info == 0x10) {
                                                    string format = (*it).second.name;
                                                    auto primTypePos = format.rfind("SetPrimitiveType=");
                                                    if (primTypePos != string::npos) {
                                                        string primTypeStr = format.substr(primTypePos + 17);
                                                        if (primTypeStr.starts_with("EAGL::PT_POINTLIST"))
                                                            geoPrimMode = 0;
                                                        else if (primTypeStr.starts_with("EAGL::PT_LINELIST"))
                                                            geoPrimMode = 1;
                                                        else if (primTypeStr.starts_with("EAGL::PT_LINESTRIP"))
                                                            geoPrimMode = 3;
                                                        else if (primTypeStr.starts_with("EAGL::PT_TRIANGLELIST"))
                                                            geoPrimMode = 4;
                                                        else if (primTypeStr.starts_with("EAGL::PT_TRIANGLESTRIP"))
                                                            geoPrimMode = 5;
                                                        else if (primTypeStr.starts_with("EAGL::PT_TRIANGLEFAN"))
                                                            geoPrimMode = 6;
                                                    }
                                                }
                                            }
                                            break;
                                        case 9:
                                        case 32:
                                            if (GetAt<unsigned int>(renderCode, commandOffset + 4) < 3)
                                                mat.textures[GetAt<unsigned int>(renderCode, commandOffset + 4)] = (unsigned int)At<char *>(globalParameters, 4) - (unsigned int)data;
                                            break;
                                        }
                                        if (numCommands != 0)
                                            globalParameters = At<void *>(globalParameters, 8);
                                        numCommands++;
                                        commandOffset += size * 4;
                                        id = GetAt<unsigned short>(renderCode, commandOffset + 2);
                                        size = GetAt<unsigned short>(renderCode, commandOffset);
                                    }
                                }
                                //else {
                                //    Error("Shader can't be found (%s) in file %s", codeName.c_str(), outPath.filename().string().c_str());
                                //}
                            }
                        }
                        openScope();
                        if (vertexBuffer) {
                            openScope("attributes");
                            unsigned int attrOffset = 0;
                            bool hasBlendIndices = false;
                            bool hasBlendWeights = false;
                            for (auto const &d : shader.declaration) {
                                if (d.usage == Shader::BlendIndices)
                                    hasBlendIndices = true;
                                else if (d.usage == Shader::BlendWeight)
                                    hasBlendWeights = true;
                            }
                            bool uses2Streams = skinVertexDataBuffer && hasBlendIndices && hasBlendWeights;
                            unsigned int streamNumber = 0;
                            for (auto const &d : shader.declaration) {
                                switch (d.usage) {
                                case Shader::Position:
                                    writeFieldInt("POSITION", accessors.size());
                                    break;
                                case Shader::Normal:
                                    writeFieldInt("NORMAL", accessors.size());
                                    break;
                                case Shader::Texcoord0:
                                    writeFieldInt("TEXCOORD_0", accessors.size());
                                    break;
                                case Shader::Texcoord1:
                                    writeFieldInt("TEXCOORD_1", accessors.size());
                                    break;
                                case Shader::Texcoord2:
                                    writeFieldInt("TEXCOORD_2", accessors.size());
                                    break;
                                case Shader::Color0:
                                    writeFieldInt("COLOR_0", accessors.size());
                                    break;
                                case Shader::Color1:
                                    //writeFieldInt("COLOR_1", accessors.size());
                                    color1Offset = attrOffset;
                                    attrOffset += 4;
                                    continue;
                                case Shader::BlendIndices:
                                    writeFieldInt("JOINTS_0", accessors.size());
                                    if (uses2Streams) {
                                        attrOffset = 0;
                                        streamNumber = 1;
                                    }
                                    break;
                                case Shader::BlendWeight:
                                    writeFieldInt("WEIGHTS_0", accessors.size());
                                    break;
                                }
                                Accessor a;
                                a.offset = attrOffset;
                                switch (d.type) {
                                case Shader::Float4:
                                    a.type = "VEC4";
                                    a.componentType = 5126;
                                    attrOffset += 16;
                                    break;
                                case Shader::Float3:
                                    a.type = "VEC3";
                                    a.componentType = 5126;
                                    attrOffset += 12;
                                    break;
                                case Shader::Float2:
                                    a.type = "VEC2";
                                    a.componentType = 5126;
                                    attrOffset += 8;
                                    break;
                                case Shader::D3DColor:
                                case Shader::UByte4:
                                    a.type = "VEC4";
                                    a.componentType = 5121;
                                    attrOffset += 4;
                                    break;
                                }
                                a.buffer = buffers.size() + streamNumber;
                                a.count = numVertices;
                                a.length = numVertices * (streamNumber ? 20 : vertexSize);
                                a.stride = streamNumber ? 20 : vertexSize;
                                a.bufferType = streamNumber ? Buffer::VertexSkin : Buffer::Vertex;
                                a.normalized = d.usage == Shader::Color0;
                                if (d.usage == Shader::Position) {
                                    a.usesMinMax = true;
                                    Vector3 boundMin = { 0.0f, 0.0f, 0.0f };
                                    Vector3 boundMax = { 0.0f, 0.0f, 0.0f };
                                    bool anyVertexProcessed = false;
                                    Vector3 *posn = (Vector3 *)(unsigned int(vertexBuffer) + a.offset);
                                    for (unsigned int vert = 0; vert < numVertices; vert++) {
                                        if (!anyVertexProcessed) {
                                            boundMin = *posn;
                                            boundMax = *posn;
                                            anyVertexProcessed = true;
                                        }
                                        else {
                                            if (posn->x < boundMin.x)
                                                boundMin.x = posn->x;
                                            if (posn->y < boundMin.y)
                                                boundMin.y = posn->y;
                                            if (posn->z < boundMin.z)
                                                boundMin.z = posn->z;
                                            if (posn->x > boundMax.x)
                                                boundMax.x = posn->x;
                                            if (posn->y > boundMax.y)
                                                boundMax.y = posn->y;
                                            if (posn->z > boundMax.z)
                                                boundMax.z = posn->z;
                                        }
                                        posn = (Vector3 *)(unsigned int(posn) + a.stride);
                                    }
                                    a.min = boundMin;
                                    a.max = boundMax;
                                }
                                //else if (d.usage == Shader::Color0) {
                                //    unsigned char *clr = (unsigned char *)(unsigned int(vertexBuffer) + a.offset);
                                //    for (unsigned int vert = 0; vert < numVertices; vert++) {
                                //        clr[0] *= 2;
                                //        clr[1] *= 2;
                                //        clr[2] *= 2;
                                //        clr = (unsigned char *)(unsigned int(clr) + a.stride);
                                //    }
                                //}
                                accessors.push_back(a);
                            }
                            closeScope();
                            Buffer b;
                            b.data = vertexBuffer;
                            b.length = vertexSize * numVertices;
                            b.type = Buffer::Vertex;
                            b.stride = vertexSize;
                            buffers.push_back(b);
                            if (uses2Streams) {
                                Buffer b2;
                                VertexSkinData *vsb = new VertexSkinData[numVertices];
                                ZeroMemory(vsb, numVertices * sizeof(VertexSkinData));
                                b2.data = vsb;
                                if (color1Offset != -1) {
                                    for (unsigned int v = 0; v < numVertices; v++) {
                                        unsigned int boneIndex = GetAt<unsigned char>(vertexBuffer, vertexSize * v + color1Offset);
                                        vsb[v].indices[0] = GetAt<unsigned char>(&skinVertexDataBuffer[boneIndex].packedData.x, 0);
                                        vsb[v].indices[1] = GetAt<unsigned char>(&skinVertexDataBuffer[boneIndex].packedData.y, 0);
                                        vsb[v].indices[2] = GetAt<unsigned char>(&skinVertexDataBuffer[boneIndex].packedData.z, 0);
                                        vsb[v].indices[3] = 0;
                                        vsb[v].weights.x = skinVertexDataBuffer[boneIndex].packedData.x;
                                        vsb[v].weights.y = skinVertexDataBuffer[boneIndex].packedData.y;
                                        vsb[v].weights.z = skinVertexDataBuffer[boneIndex].packedData.z;
                                        vsb[v].weights.w = 0.0f;
                                        //Error(L"%d %d-%d-%d %.2f %.2f %.2f", boneIndex, vsb[v].indices[0], vsb[v].indices[1], vsb[v].indices[2],
                                        //    vsb[v].weights.x, vsb[v].weights.y, vsb[v].weights.z);
                                    }
                                }
                                vertexSkinBuffers.push_back(vsb);
                                b2.length = 20 * numVertices;
                                b2.type = Buffer::VertexSkin;
                                b2.stride = 20;
                                buffers.push_back(b2);
                            }
                        }
                        if (indexBuffer) {
                            Accessor a;
                            a.buffer = buffers.size();
                            a.componentType = 5123;
                            a.count = numIndices;
                            a.length = numIndices * 2;
                            a.offset = 0;
                            a.type = "SCALAR";
                            a.stride = 2;
                            a.bufferType = Buffer::Index;
                            writeFieldInt("indices", accessors.size());
                            accessors.push_back(a);
                            Buffer b;
                            b.data = indexBuffer;
                            b.length = 2 * numIndices;
                            b.type = Buffer::Index;
                            b.stride = 2;
                            buffers.push_back(b);
                        }
                        writeFieldInt("mode", geoPrimMode);
                        writeFieldInt("material", materials.size());
                        materials.push_back(mat);
                        closeScope();
                    }
                    modelLayers += numPrimitives;
                    closeArray();
                    closeScope();
                }
                closeArray();
            }
            
            if (model && model->mTextures) {
                void *texDesc = model->mTextures;
                char const *texName = GetAt<char const *>(texDesc, 0);
                while (texName) { // TODO: replace with IsValidOffset()
                    TexDesc t;
                    t.name = texName;
                    t.source = t.name + (options().jpegTextures? ".jpeg" : ".png");
                    t.mimeType = string("image/") + (options().jpegTextures ? "jpeg" : "png");
                    void **pTexTar = At<void *>(texDesc, 8);
                    unsigned int tarOffset = unsigned int(pTexTar) - unsigned int(data);
                    auto it = symbolRelocations.find(tarOffset);
                    if (it != symbolRelocations.end()) {
                        auto const &s = (*it).second;
                        t.symbolId = s.id;
                    }
                    Texture *tar = (Texture *)(*pTexTar);
                    t.pTex = tar;
                    if (tar) {
                        //Error(Utils::Format(L"%d %d %d", tar->wrapU, tar->wrapV, tar->wrapW));
                    }
                    else {
                        unsigned int tarOffset = unsigned int(pTexTar) - unsigned int(data);
                        auto it = symbolRelocations.find(tarOffset);
                        if (it != symbolRelocations.end()) {
                            auto const &s = (*it).second;
                            //Error(s.name);
                        }
                    }
                    textures.push_back(t);
                    unsigned int texCount = GetAt<unsigned int>(texDesc, 4);
                    unsigned int texSize = texCount * 4;
                    unsigned short info = GetAt<unsigned int>(texDesc, 8 + texSize);
                    if (info == 1)
                        texDesc = At<void>(texDesc, 12 + texSize);
                    else
                        texDesc = At<void>(texDesc, 8 + texSize);
                    texName = GetAt<char const *>(texDesc, 0);
                }
                if (textures.size() > 0) {
                    // samplers
                    openArray("samplers");
                    for (auto const &t : textures) {
                        openScope();
                        writeFieldString("name", t.name);
                        writeFieldInt("magFilter", t.magFilter);
                        writeFieldInt("minFilter", t.minFilter);
                        writeFieldInt("wrapS", t.wrapU);
                        writeFieldInt("wrapT", t.wrapV);
                        closeScope();
                    }
                    closeArray();
                    // textures
                    openArray("textures");
                    unsigned int i = 0;
                    for (auto const &t : textures) {
                        openScope();
                        writeFieldString("name", t.name);
                        writeFieldInt("sampler", i);
                        writeFieldInt("source", i);
                        closeScope();
                        i++;
                    }
                    closeArray();
                    // images
                    openArray("images");
                    for (auto const &t : textures) {
                        openScope();
                        writeFieldString("name", t.name);
                        writeFieldString("mimeType", t.mimeType);
                        writeFieldString("uri", t.source);
                        closeScope();

                        if (options().dummyTextures) {
                            if (!options().jpegTextures) {
                                static unsigned char pngData[] = {
                                    0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A, 0x00, 0x00, 0x00, 0x0D, 0x49, 0x48, 0x44, 0x52,
                                    0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x04, 0x08, 0x06, 0x00, 0x00, 0x00, 0xA9, 0xF1, 0x9E,
                                    0x7E, 0x00, 0x00, 0x00, 0x13, 0x49, 0x44, 0x41, 0x54, 0x18, 0x57, 0x63, 0xFC, 0xFF, 0xFF, 0xFF,
                                    0x7F, 0x06, 0x24, 0xC0, 0x48, 0xBA, 0x00, 0x00, 0x7E, 0x8C, 0x0F, 0xF5, 0xE8, 0x50, 0x94, 0x80,
                                    0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4E, 0x44, 0xAE, 0x42, 0x60, 0x82
                                };
                                static BinaryBuffer pngBuf(std::size(pngData));
                                static bool pngBufInitialized = false;
                                if (!pngBufInitialized) {
                                    pngBuf.Put(pngData, std::size(pngData));
                                    pngBufInitialized = true;
                                }
                                pngBuf.WriteToFile(outPath.parent_path() / t.source);
                            }
                            else {
                                unsigned char jpegData[] = {
                                    0xFF, 0xD8, 0xFF, 0xE0, 0x00, 0x10, 0x4A, 0x46, 0x49, 0x46, 0x00, 0x01, 0x01, 0x00, 0x00, 0x01,
                                    0x00, 0x01, 0x00, 0x00, 0xFF, 0xDB, 0x00, 0x43, 0x00, 0x03, 0x02, 0x02, 0x02, 0x02, 0x02, 0x03,
                                    0x02, 0x02, 0x02, 0x03, 0x03, 0x03, 0x03, 0x04, 0x06, 0x04, 0x04, 0x04, 0x04, 0x04, 0x08, 0x06,
                                    0x06, 0x05, 0x06, 0x09, 0x08, 0x0A, 0x0A, 0x09, 0x08, 0x09, 0x09, 0x0A, 0x0C, 0x0F, 0x0C, 0x0A,
                                    0x0B, 0x0E, 0x0B, 0x09, 0x09, 0x0D, 0x11, 0x0D, 0x0E, 0x0F, 0x10, 0x10, 0x11, 0x10, 0x0A, 0x0C,
                                    0x12, 0x13, 0x12, 0x10, 0x13, 0x0F, 0x10, 0x10, 0x10, 0xFF, 0xC0, 0x00, 0x0B, 0x08, 0x00, 0x04,
                                    0x00, 0x04, 0x01, 0x01, 0x11, 0x00, 0xFF, 0xC4, 0x00, 0x14, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00,
                                    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x09, 0xFF, 0xC4, 0x00, 0x14,
                                    0x10, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                    0x00, 0x00, 0xFF, 0xDA, 0x00, 0x08, 0x01, 0x01, 0x00, 0x00, 0x3F, 0x00, 0x54, 0xDF, 0xFF, 0xD9
                                };
                                static BinaryBuffer jpegBuf(std::size(jpegData));
                                static bool jpegBufInitialized = false;
                                if (!jpegBufInitialized) {
                                    jpegBuf.Put(jpegData, std::size(jpegData));
                                    jpegBufInitialized = true;
                                }
                                jpegBuf.WriteToFile(outPath.parent_path() / t.source);
                            }
                        }
                    }
                    closeArray();
                }
            }
            // materials
            if (!materials.empty()) {
                openArray("materials");
                for (unsigned int i = 0; i < materials.size(); i++) {
                    openScope();
                    writeFieldString("name", string("material") + Format("%02d", materials[i].id) + "[" + materials[i].shader + "]");
                    writeFieldBool("doubleSided", true);
                    openScope("pbrMetallicRoughness");
                    if (materials[i].textures[0] != -1) {
                        int symbolId = -1;
                        auto it = symbolRelocations.find(materials[i].textures[0]);
                        if (it != symbolRelocations.end())
                            symbolId = (*it).second.id;
                        Texture *pTex = GetAt<Texture *>(data, materials[i].textures[0]);
                        int texId = -1;
                        for (unsigned int t = 0; t < textures.size(); t++) {
                            if (textures[t].symbolId == symbolId && textures[t].pTex == pTex) {
                                texId = t;
                                break;
                            }
                        }
                        if (texId != -1 && !options().noTextures) {
                            openScope("baseColorTexture");
                            writeFieldInt("index", texId);
                            //writeFieldInt("texCoord", 0);
                            closeScope();
                        }
                    }
                    writeFieldFloat("metallicFactor", 0.0f);
                    writeFieldFloat("roughnessFactor", 1.0f);
                    closeScope();
                    closeScope();
                }
                closeArray();
            }
            // accessors
            if (accessors.size() > 0) {
                openArray("accessors");
                
                for (auto const &a : accessors) {
                    openScope();
                    writeFieldInt("bufferView", a.buffer);
                    writeFieldInt("componentType", a.componentType);
                    writeFieldInt("count", a.count);
                    writeFieldString("type", a.type);
                    writeFieldInt("byteOffset", a.offset);
                    if (a.normalized)
                        writeFieldBool("normalized", true);
                    if (a.usesMinMax) {
                        openArray("min");
                        writeValueFloat(a.min.x);
                        writeValueFloat(a.min.y);
                        writeValueFloat(a.min.z);
                        closeArray();
                        openArray("max");
                        writeValueFloat(a.max.x);
                        writeValueFloat(a.max.y);
                        writeValueFloat(a.max.z);
                        closeArray();
                    }
                    closeScope();
                }
                closeArray();
            }
            // bufferViews
            if (buffers.size() > 0) {
                unsigned int i = 0;
                openArray("bufferViews");
                for (auto const &b : buffers) {
                    openScope();
                    writeFieldInt("buffer", i++);
                    writeFieldInt("byteLength", b.length);
                    if (b.type == Buffer::Vertex || b.type == Buffer::VertexSkin)
                        writeFieldInt("byteStride", b.stride);
                    else if (b.type == Buffer::Index)
                        writeFieldInt("target", 34963);
                    closeScope();
                }
                closeArray();
                // buffers
                openArray("buffers");
                for (auto const &b : buffers) {
                    openScope();
                    writeFieldInt("byteLength", b.length);
                    writeFieldString("uri", "data:application/octet-stream;base64," + base64_encode((unsigned char *)b.data, b.length));
                    closeScope();
                }
                closeArray();
            }
        }
        endScope();

        for (auto const &v : vertexSkinBuffers)
            delete[] v;
        delete[] skinMatrices;

        ofstream w(outPath, ios::out);
        if (w.is_open())
            w << mResult;
        //else
        //    Error(L"failed to open %s", outPath.c_str());
    }

    void convert_o_to_gltf(path const &inPath, path const &outPath) {
        FILE *f = _wfopen(inPath.c_str(), L"rb");
        if (f) {
            fseek(f, 0, SEEK_END);
            unsigned int fileSize = ftell(f);
            fseek(f, 0, SEEK_SET);
            unsigned char *fileData = new unsigned char[fileSize];
            if (fread(fileData, 1, fileSize, f) == fileSize)
                convert_o_to_gltf(fileData, fileSize, outPath);
            //else
            //    Error(L"failed to read %s", inPath.c_str());
            delete[] fileData;
            fclose(f);
        }
        //else
        //    Error(L"failed to open %s", inPath.c_str());
    }
};

void oexport(path const &out, path const &in) {
    exporter e;
    e.convert_o_to_gltf(in, out);
}
