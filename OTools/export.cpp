#include "main.h"
#include <fstream>
#include "binbuf.h"
#include "jsonwriter.h"
#include "FifamReadWrite.h"
#include <assimp\scene.h>

class exporter {
    struct FileSymbol : public Elf32_Sym {
        unsigned int id = 0;
        string name;
    };

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

    struct TAR {
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

    struct Texture {
        string name;
        string source;
        string mimeType;
        unsigned int wrapU = 10497;
        unsigned int wrapV = 10497;
        unsigned int minFilter = 9729;
        unsigned int magFilter = 9729;
    };

    struct Material {
        string shader;
        Texture *textures[4] = { nullptr, nullptr, nullptr, nullptr };
        bool doubleSided = true;
        string alphaMode;
        float metallicFactor = 0.0f;
        float roughnessFactor = 1.0f;

        bool Compare(Material &m) const {
            return shader == m.shader &&
                textures[0] == m.textures[0] &&
                textures[1] == m.textures[1] &&
                textures[2] == m.textures[2] &&
                textures[3] == m.textures[3] &&
                doubleSided == m.doubleSided &&
                alphaMode == m.alphaMode &&
                metallicFactor == m.metallicFactor &&
                roughnessFactor == m.roughnessFactor;
        }
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

    struct Prop {
        Vector3 pos, dir;
        unsigned int type = 0;
        string name;
    };

    struct CollisionGeometry {
        struct Triangle {
            unsigned short normal;
            unsigned short verts[3];
        };
        
        struct Edge {
            unsigned short verts[2];
        };

        struct TriVertex {
            Vector3 pos;
            Vector3 normal;
        };

        struct EdgeVertex {
            Vector3 pos;
        };

        vector<Vector3> positions;
        vector<Vector3> normals;
        vector<Triangle> triangles;
        vector<Edge> edges;
        string name;

        vector<TriVertex> triVertBuffer;
        vector<EdgeVertex> edgeVertBuffer;
        vector<unsigned short> triIndexBuffer;
        vector<unsigned short> edgeIndexBuffer;
    };
public:
    void convert_o_to_gltf(unsigned char *fileData, unsigned int fileDataSize, path const &outPath, path const &inPath) {

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
        if (h->e_ident[0] != 0x7F || h->e_ident[1] != 'E' || h->e_ident[2] != 'L' || h->e_ident[3] != 'F')
            throw runtime_error("Not an ELF file");
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

        unsigned char *skeletonFileData = nullptr;
        vector<FileSymbol> skelSymbols;
        unsigned char *skel_data = nullptr;

        if (!options().skeleton.empty() && exists(options().skeleton)) {
            auto skelFile = readofile(options().skeleton);
            if (skelFile.first) {

                skeletonFileData = skelFile.first;
                unsigned int skel_dataSize = 0;
                Elf32_Sym *skel_symbolsData = nullptr;
                unsigned int skel_numSymbols = 0;
                Elf32_Rel *skel_rel = nullptr;
                unsigned int skel_numRelocations = 0;
                char *skel_symbolNames = nullptr;
                unsigned int skel_symbolNamesSize = 0;
                unsigned int skel_dataIndex = 0;

                Elf32_Ehdr *skel_h = (Elf32_Ehdr *)skeletonFileData;
                Elf32_Shdr *skel_s = At<Elf32_Shdr>(skel_h, skel_h->e_shoff);
                for (unsigned int i = 0; i < skel_h->e_shnum; i++) {
                    if (skel_s[i].sh_size > 0) {
                        if (skel_s[i].sh_type == 1 && !skel_data) {
                            skel_data = At<unsigned char>(skel_h, skel_s[i].sh_offset);
                            skel_dataSize = skel_s[i].sh_size;
                            skel_dataIndex = i;
                        }
                        else if (skel_s[i].sh_type == 2) {
                            skel_symbolsData = At<Elf32_Sym>(skel_h, skel_s[i].sh_offset);
                            skel_numSymbols = skel_s[i].sh_size / 16;
                        }
                        else if (skel_s[i].sh_type == 3) {
                            skel_symbolNames = At<char>(skel_h, skel_s[i].sh_offset);
                            skel_symbolNamesSize = skel_s[i].sh_size;
                        }
                        else if (skel_s[i].sh_type == 9) {
                            skel_rel = At<Elf32_Rel>(skel_h, skel_s[i].sh_offset);
                            skel_numRelocations = skel_s[i].sh_size / 8;
                        }
                    }
                }

                // get symbols

                skelSymbols.resize(skel_numSymbols);

                for (unsigned int i = 0; i < skel_numSymbols; i++) {
                    skelSymbols[i].st_info = skel_symbolsData[i].st_info;
                    skelSymbols[i].st_name = skel_symbolsData[i].st_name;
                    skelSymbols[i].st_other = skel_symbolsData[i].st_other;
                    skelSymbols[i].st_shndx = skel_symbolsData[i].st_shndx;
                    skelSymbols[i].st_size = skel_symbolsData[i].st_size;
                    skelSymbols[i].st_value = skel_symbolsData[i].st_value;
                    skelSymbols[i].name = &skel_symbolNames[skel_symbolsData[i].st_name];
                    skelSymbols[i].id = i;
                }

                auto skel_isSymbolDataPresent = [&](FileSymbol const &s) {
                    return (s.st_info & 0xF) != 0 && s.st_shndx == skel_dataIndex;
                };

                // get relocation symbols

                map<unsigned int, FileSymbol> skel_symbolRelocations;

                for (unsigned int i = 0; i < skel_numRelocations; i++) {
                    if (skel_rel[i].r_info_sym < skelSymbols.size())
                        skel_symbolRelocations[skel_rel[i].r_offset] = skelSymbols[skel_rel[i].r_info_sym];
                }

                // do relocations

                for (unsigned int i = 0; i < skel_numRelocations; i++) {
                    auto skel_symbolId = skel_rel[i].r_info_sym;
                    if (skel_symbolId < skel_numSymbols && skel_isSymbolDataPresent(skelSymbols[skel_symbolId]))
                        SetAt(skel_data, skel_rel[i].r_offset, &skel_data[GetAt<unsigned int>(skel_data, skel_rel[i].r_offset)]);
                }               
            }
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
        map<string, Texture *> textures;
        set<string> globalTextures;
        vector<Prop> flags;
        vector<Prop> effects;
        vector<Buffer> colBuffers;
        vector<CollisionGeometry> colGeometries;

        for (auto const &s : symbols) {
            if (isSymbolDataPresent(s)) {
                if (s.name.starts_with("__Model:::"))
                    models.push_back(At<Model>(data, s.st_value));
                else if (s.name.starts_with("__RenderMethod:::"))
                    renderMethods.push_back(At<RenderMethod>(data, s.st_value));
                else if (s.name.starts_with("__geoprimdatabuffer"))
                    geoPrimDataBuffers.push_back(At<void>(data, s.st_value));
                else if (s.name.starts_with("__Bone:::")) {
                    if (!skeletonFileData) {
                        bones.push_back(At<Bone>(data, s.st_value));
                        string boneName = s.name.substr(9);
                        auto dotPos = boneName.find_last_of('.');
                        if (dotPos != string::npos)
                            boneName = boneName.substr(dotPos + 1);
                        boneNames.push_back(boneName);
                    }
                }
                else if (s.name.starts_with("__Skeleton:::")) {
                    if (!skeletonFileData) {
                        if (!skeleton)
                            skeleton = At<Skeleton>(data, s.st_value);
                    }
                }
            }
        }

        if (skeletonFileData) {
            for (auto const &s : skelSymbols) {
                if (isSymbolDataPresent(s)) {
                    if (s.name.starts_with("__Bone:::")) {
                        bones.push_back(At<Bone>(skel_data, s.st_value));
                        string boneName = s.name.substr(9);
                        auto dotPos = boneName.find_last_of('.');
                        if (dotPos != string::npos)
                            boneName = boneName.substr(dotPos + 1);
                        boneNames.push_back(boneName);
                    }
                    else if (s.name.starts_with("__Skeleton:::")) {
                        if (!skeleton)
                            skeleton = At<Skeleton>(skel_data, s.st_value);
                    }
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

        auto originalFilePath = inPath;
        error_code ec;
        if (!originalFilePath.has_parent_path())
            originalFilePath = absolute(inPath, ec);
        auto originalFileName = originalFilePath.stem().string();

        bool hasFlags = false;
        bool hasEffects = false;
        bool hasCollision = false;

        if (true && originalFileName.starts_with("m716__")) {
            unsigned int stadiumId = 0;
            unsigned int lightingId = 0;
            if (sscanf(originalFileName.c_str(), "m716__%d_%d", &stadiumId, &lightingId) == 2) {
                auto originalFolder = originalFilePath.parent_path();
                // flags
                path stadFlagsPath = originalFolder / Format("sle-%d-%d.loc", stadiumId, lightingId);
                if (exists(stadFlagsPath)) {
                    FifamReader reader(stadFlagsPath, 14, false, false);
                    if (reader.Available()) {
                        hasFlags = true;
                        while (!reader.IsEof()) {
                            if (!reader.EmptyLine()) {
                                auto line = reader.ReadFullLine();
                                float a = 0.0f; float b = 0.0f; float c = 0.0f; int d = 0; float e = 0.0f; float f = 0.0f; float g = 0.0f;
                                auto numParams = swscanf(line.c_str(), L"%f %f %f %d %f %f %f", &a, &b, &c, &d, &e, &f, &g);
                                bool isLineValid = false;
                                if (numParams == 7)
                                    isLineValid = true;
                                else if (numParams == 3) {
                                    d = 0;
                                    e = 1.0f;
                                    f = 1.0f;
                                    g = 1.0f;
                                    isLineValid = true;
                                }
                                if (isLineValid) {
                                    Prop p;
                                    p.pos.x = a * 100.0f; p.pos.y = b * 100.0f; p.pos.z = c * 100.0f; p.type = d; p.dir.x = e; p.dir.y = f; p.dir.z = g;
                                    p.name = "Flag_" + to_string(flags.size() + 1);
                                    if (d != 0)
                                        p.name += " [type:" + to_string(d) + "]";
                                    flags.push_back(p);
                                }
                            }
                            else
                                reader.SkipLine();
                        }
                    }
                }

                // effects
                path stadLightsPath = originalFolder / Format("tag-%d-%d.loc", stadiumId, lightingId);
                if (exists(stadLightsPath)) {
                    FifamReader reader(stadLightsPath, 14, false, false);
                    if (reader.Available()) {
                        hasEffects = true;
                        if (!reader.IsEof()) {
                            auto l = reader.ReadFullLine();
                            unsigned int numEffectTypes;
                            if (swscanf(l.c_str(), L"%d", &numEffectTypes) == 1 && numEffectTypes > 0) {
                                map<wstring, size_t> effectTypes;
                                for (unsigned int i = 0; i < numEffectTypes; i++) {
                                    if (reader.IsEof())
                                        break;
                                    l = reader.ReadFullLine();
                                    wchar_t effectType[32];
                                    unsigned int numEffects = 0;
                                    if (swscanf(l.c_str(), L"%s %d", effectType, &numEffects) == 2 && effectType[0] && numEffects > 0) {
                                        bool hasPosition = wcsstr(l.c_str(), L"Position");
                                        bool hasDirection = wcsstr(l.c_str(), L"Direction");
                                        bool eof = false;
                                        unsigned int effectId = 1;
                                        for (unsigned int e = 0; e < numEffects; e++) {
                                            if (reader.IsEof())
                                                break;
                                            l = reader.ReadFullLine();
                                            Prop p;
                                            if (hasPosition) {
                                                if (hasDirection) {
                                                    if (swscanf(l.c_str(), L"%f %f %f %f %f %f", &p.pos.x, &p.pos.y, &p.pos.z, &p.dir.x, &p.dir.y, &p.dir.z) != 6)
                                                        break;
                                                    p.type = 2;
                                                    p.name = WtoA(effectType) + "_" + to_string(++effectTypes[effectType]) + " [dir]";
                                                    p.pos.x *= 1.12f;
                                                    p.pos.y *= 1.12f;
                                                    p.pos.z *= 1.12f;
                                                    //p.dir.x *= 1.12f;
                                                    //p.dir.y *= 1.12f;
                                                    //p.dir.z *= 1.12f;
                                                    effects.push_back(p);
                                                }
                                                else {
                                                    if (swscanf(l.c_str(), L"%f %f %f", &p.pos.x, &p.pos.y, &p.pos.z) != 3)
                                                        break;
                                                    p.type = 1;
                                                    p.name = WtoA(effectType) + "_" + to_string(++effectTypes[effectType]);
                                                    p.pos.x *= 1.12f;
                                                    p.pos.y *= 1.12f;
                                                    p.pos.z *= 1.12f;
                                                    effects.push_back(p);
                                                }
                                            }
                                        }
                                        if (eof)
                                            break;
                                    }
                                }
                            }
                        }
                    }
                }

                // collision
                path stadCollPath = originalFolder / Format("coll-%d-%d.bin", stadiumId, lightingId);
                if (exists(stadCollPath)) {
                    FILE *collFile = nullptr;
                    _wfopen_s(&collFile, stadCollPath.c_str(), L"rb");
                    if (collFile) {
                        bool isFileValid = false;
                        unsigned int collHeader[3] = { 0, 0, 0 };
                        if (fread(collHeader, 4, 3, collFile) == 3 && collHeader[0] == 2) {
                            isFileValid = true;
                            colGeometries.resize(collHeader[2]);
                            for (unsigned int collg = 0; collg < collHeader[2]; collg++) {
                                if (fseek(collFile, 24, SEEK_CUR) != 0) {
                                    isFileValid = false;
                                    break;
                                }
                                unsigned short desc[4] = { 0, 0, 0, 0 };
                                if (fread(desc, 2, 4, collFile) != 4) {
                                    isFileValid = false;
                                    break;
                                }
                                if (desc[0] > 0) {
                                    colGeometries[collg].positions.resize(desc[0]);
                                    if (fread(colGeometries[collg].positions.data(), 12, desc[0], collFile) != desc[0]) {
                                        isFileValid = false;
                                        break;
                                    }
                                }
                                if (desc[1] > 0) {
                                    colGeometries[collg].normals.resize(desc[1]);
                                    if (fread(colGeometries[collg].normals.data(), 12, desc[1], collFile) != desc[1]) {
                                        isFileValid = false;
                                        break;
                                    }
                                }
                                if (desc[2] > 0) {
                                    colGeometries[collg].triangles.resize(desc[2]);
                                    if (fread(colGeometries[collg].triangles.data(), 8, desc[2], collFile) != desc[2]) {
                                        isFileValid = false;
                                        break;
                                    }
                                }
                                if (desc[3] > 0) {
                                    colGeometries[collg].edges.resize(desc[3]);
                                    if (fread(colGeometries[collg].edges.data(), 4, desc[3], collFile) != desc[3]) {
                                        isFileValid = false;
                                        break;
                                    }
                                }
                                colGeometries[collg].name = "CollisionGeometry_" + to_string(collg + 1);
                            }
                        }
                        if (isFileValid)
                            hasCollision = true;
                        else
                            colGeometries.clear();
                        fclose(collFile);
                    }
                }
            }
        }

        JsonWriter j(outPath);
        j.startScope();
        j.openScope("asset");
        j.writeFieldString("generator", string("otools version ") + OTOOLS_VERSION);
        j.writeFieldString("version", "2.0");
        j.closeScope();
        j.writeFieldInt("scene", 0);
        if ((model && model->mNumLayers && model->mLayers) || skeleton) {

            // scenes
            j.openArray("scenes");
            j.openScope();
            if (model && model->mName)
                j.writeFieldString("name", model->mName);
            j.openArray("nodes");
            unsigned int numNodes = (model ? model->mNumLayers : 0) + (skeleton ? 1 : 0);
            unsigned int numWrittenNodes = 0;
            for (unsigned int i = 0; i < numNodes; i++)
                j.writeValueInt(i);
            if (hasCollision)
                j.writeValueInt(numNodes + bones.size());
            if (hasFlags)
                j.writeValueInt(numNodes + bones.size() + (hasCollision ? 1 : 0));
            if (hasEffects)
                j.writeValueInt(numNodes + bones.size() + (hasCollision ? 1 : 0) + (hasFlags ? 1 : 0));
            j.closeArray();
            j.closeScope();
            j.closeArray();
            // nodes
            j.openArray("nodes");
            if (model) {
                for (unsigned int i = 0; i < model->mNumLayers; i++) {
                    j.openScope();
                    j.writeFieldInt("mesh", i);
                    if (skeleton)
                        j.writeFieldInt("skin", 0);
                    if (model->mLayerNames[i])
                        j.writeFieldString("name", model->mLayerNames[i]);
                    j.closeScope();
                    numWrittenNodes++;
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
                        skelNodes[bone->mIndex].name = boneNames[b] + " [" + to_string(bone->mIndex) + "]";
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
                j.openScope();
                j.writeFieldString("name", "Skeleton");
                vector<unsigned int> skelRootNodes;
                for (unsigned int i = 0; i < skelNodes.size(); i++) {
                    if (!skelNodes[i].parent)
                        skelRootNodes.push_back(skelNodes[i].index);
                }
                if (skelRootNodes.size() > 0) {
                    j.openArray("children");
                    for (unsigned int i = 0; i < skelRootNodes.size(); i++)
                        j.writeValueInt(nodeIndex + 1 + skelRootNodes[i]);
                    j.closeArray();
                }
                numWrittenNodes++;
                j.closeScope();
                for (unsigned int i = 0; i < skelNodes.size(); i++) {
                    j.openScope();
                    j.writeFieldString("name", skelNodes[i].name);
                    j.openArray("rotation");
                    j.writeValueFloat(boneStates[i].unknown2.x);
                    j.writeValueFloat(boneStates[i].unknown2.y);
                    j.writeValueFloat(boneStates[i].unknown2.z);
                    j.writeValueFloat(boneStates[i].unknown2.w);
                    j.closeArray();
                    j.openArray("translation");
                    j.writeValueFloat(boneStates[i].unknown3);
                    j.writeValueFloat(boneStates[i].unknown4);
                    j.writeValueFloat(boneStates[i].unknown5);
                    j.closeArray();
                    //j.openArray("matrix");
                    //D3DXMATRIX *unkMat = At<D3DXMATRIX>(skeleton, 0x10 + 0x30 + sizeof(BoneState) * i);
                    //D3DXMATRIX mat = *unkMat;
                    //D3DXMatrixInverse(&mat, NULL, unkMat);
                    //for (unsigned int m = 0; m < 16; m++) {
                    //    j.writeValueFloat(GetAt<float>(&mat, m * 4));
                    //}
                    //j.closeArray();
                    if (skelNodes[i].children.size() > 0) {
                        j.openArray("children");
                        for (unsigned int c = 0; c < skelNodes[i].children.size(); c++)
                            j.writeValueInt(nodeIndex + 1 + skelNodes[i].children[c]->index);
                        j.closeArray();
                    }
                    j.closeScope();
                    numWrittenNodes++;
                }
            }
            if (hasCollision) {
                j.openScope();
                j.writeFieldString("name", "Collision");
                if (!colGeometries.empty()) {
                    j.openArray("children");
                    for (unsigned int i = 0; i < colGeometries.size(); i++)
                        j.writeValueInt(numWrittenNodes + 1 + i);
                    j.closeArray();
                }
                j.closeScope();
                numWrittenNodes++;
                for (unsigned int i = 0; i < colGeometries.size(); i++) {
                    j.openScope();
                    j.writeFieldString("name", colGeometries[i].name);
                    j.writeFieldInt("mesh", i + (model? model->mNumLayers : 0));
                    j.closeScope();
                    numWrittenNodes++;
                }
            }
            if (hasFlags) {
                j.openScope();
                j.writeFieldString("name", "Flags");
                if (!flags.empty()) {
                    j.openArray("children");
                    for (unsigned int i = 0; i < flags.size(); i++)
                        j.writeValueInt(numWrittenNodes + 1 + i);
                    j.closeArray();
                }
                j.closeScope();
                numWrittenNodes++;
                for (unsigned int i = 0; i < flags.size(); i++) {
                    j.openScope();
                    j.writeFieldString("name", flags[i].name);
                    j.openArray("translation");
                    j.writeValueFloat(flags[i].pos.x);
                    j.writeValueFloat(flags[i].pos.y);
                    j.writeValueFloat(flags[i].pos.z);
                    j.closeArray();
                    j.openArray("scale");
                    j.writeValueFloat(flags[i].dir.x);
                    j.writeValueFloat(flags[i].dir.y);
                    j.writeValueFloat(flags[i].dir.z);
                    j.closeArray();
                    j.closeScope();
                    numWrittenNodes++;
                }
            }
            if (hasEffects) {
                j.openScope();
                j.writeFieldString("name", "Effects");
                if (!effects.empty()) {
                    j.openArray("children");
                    for (unsigned int i = 0; i < effects.size(); i++)
                        j.writeValueInt(numWrittenNodes + 1 + i);
                    j.closeArray();
                }
                j.closeScope();
                numWrittenNodes++;
                for (unsigned int i = 0; i < effects.size(); i++) {
                    j.openScope();
                    j.writeFieldString("name", effects[i].name);
                    if (effects[i].type == 2) {
                        j.openArray("matrix");
                        aiMatrix4x4 m;
                        auto Cross = [](aiVector3D &a, aiVector3D &b) {
                            float ni = a.y * b.z - a.z * b.y;
                            float nj = a.z * b.x - a.x * b.z;
                            float nk = a.x * b.y - a.y * b.x;
                            return aiVector3D(ni, nj, nk);
                        };
                        aiVector3D up(0, 1, 0);
                        aiVector3D direction(effects[i].dir.x, effects[i].dir.y, effects[i].dir.z);
                        aiVector3D xaxis = Cross(up, direction);
                        xaxis.Normalize();
                        aiVector3D yaxis = Cross(direction, xaxis);
                        yaxis.Normalize();
                        m.a1 = xaxis.x;
                        m.a2 = xaxis.y;
                        m.a3 = xaxis.z;
                        m.b1 = direction.x;
                        m.b2 = direction.y;
                        m.b3 = direction.z;
                        m.c1 = yaxis.x;
                        m.c2 = yaxis.y;
                        m.c3 = yaxis.z;
                        //aiMatrix4x4::FromToMatrix(aiVector3D(0, 1, 0), aiVector3D(effects[i].dir.x, effects[i].dir.y, effects[i].dir.z), m);
                        m.d1 = effects[i].pos.x;
                        m.d2 = effects[i].pos.y;
                        m.d3 = effects[i].pos.z;
                        m.d4 = 1;

                        //m.c1 = effects[i].dir.x;
                        //m.c2 = effects[i].dir.y;
                        //m.c3 = effects[i].dir.z;

                        for (unsigned int i = 0; i < 4; i++) {
                            for (unsigned int k = 0; k < 4; k++)
                                j.writeValueFloat(m[i][k]);
                        }
                        j.closeArray();
                    }
                    else {
                        j.openArray("translation");
                        j.writeValueFloat(effects[i].pos.x);
                        j.writeValueFloat(effects[i].pos.y);
                        j.writeValueFloat(effects[i].pos.z);
                        j.closeArray();
                    }
                    j.closeScope();
                    numWrittenNodes++;
                }
            }
            j.closeArray();
            vector<Buffer> buffers;
            vector<Accessor> accessors;
            // skins
            if (skeleton) {
                j.openArray("skins");
                j.openScope();
                if (bones.size() > 0) {
                    j.writeFieldInt("inverseBindMatrices", accessors.size());
                    Accessor a;
                    skinMatrices = new Matrix4x4[bones.size()];
                    for (unsigned int m = 0; m < bones.size(); m++) {
                        Matrix4x4 *unkMat = At<Matrix4x4>(skeleton, 0x10 + 0x30 + sizeof(BoneState) * m);
                        //D3DXMatrixInverse(&skinMatrices[m], NULL, unkMat);
                        //D3DXMATRIX tmpMat;
                        //D3DXMatrixInverse(&tmpMat, NULL, &skinMatrices[m]);
                        Memory_Copy(&skinMatrices[m], unkMat, 64);
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
                j.writeFieldInt("skeleton", skelRootNodeIndex);
                if (bones.size() > 0) {
                    j.openArray("joints");
                    for (unsigned int i = 0; i < bones.size(); i++)
                        j.writeValueInt(skelRootNodeIndex + 1 + i);
                    j.closeArray();
                }
                j.closeScope();
                j.closeArray();
            }
            // meshes

            // TODO: embedded textures export (check for embedded textures symbols)

            if ((model && model->mNumLayers) || !colGeometries.empty()) {
                j.openArray("meshes");
                if ((model && model->mNumLayers)) {
                    unsigned int *modelLayers = (unsigned int *)(model->mLayers);
                    unsigned int modelLayersHeader = *modelLayers;
                    bool isOldFormat = modelLayersHeader == 0xA0000000;
                    modelLayers++;
                    if (isOldFormat)
                        modelLayers += 2;
                    for (unsigned int i = 0; i < model->mNumLayers; i++) {
                        j.openScope();
                        if (model->mLayerNames[i])
                            j.writeFieldString("name", model->mLayerNames[i]);
                        j.openArray("primitives");
                        if (isOldFormat)
                            modelLayers += 2;
                        unsigned int numPrimitives = *modelLayers;
                        if (isOldFormat)
                            numPrimitives /= 2;
                        modelLayers++;
                        for (unsigned int p = 0; p < numPrimitives; p++) {
                            void *renderDescriptor = GetAt<void *>(modelLayers, isOldFormat ? (p * 8 + 4) : (p * 4));
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
                            Shader *shader = nullptr;
                            string shaderName;
                            int color1Offset = -1;
                            Material mat;
                            //Error("%X", rmCodeOffset);
                            auto it = symbolRelocations.find(rmCodeOffset);
                            if (it != symbolRelocations.end() && (*it).second.st_info == 0x10) {
                                string codeName = (*it).second.name;
                                if (codeName.ends_with("__EAGLMicroCode")) {
                                    shaderName = codeName.substr(0, codeName.length() - 15);
                                    mat.shader = shaderName;
                                    string shaderLowered = ToLower(mat.shader);
                                    if (shaderLowered == "cliptextureaddnodepthwrite" || shaderLowered == "cliptexturealphablend" || shaderLowered.find("transparent") != string::npos)
                                        mat.alphaMode = "BLEND";
                                    shader = globalVars().target->FindShader(shaderName);
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
                                                if (geoPrimState->bCullEnable == 1)
                                                    mat.doubleSided = false;
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

                                                    auto cullEnablePos = format.rfind("SetCullEnable=");
                                                    if (cullEnablePos != string::npos) {
                                                        string cullEnableValue = format.substr(cullEnablePos + 14);
                                                        if (cullEnableValue == "true")
                                                            mat.doubleSided = false;
                                                    }
                                                }
                                            }
                                            break;
                                        case 9:
                                        case 32:
                                        {
                                            unsigned int samplerIndex = GetAt<unsigned int>(renderCode, commandOffset + 4);
                                            if (samplerIndex < 4) {
                                                Texture tex;
                                                TAR const *tar = GetAt<TAR *>(globalParameters, 4);
                                                FileSymbol const *texSymbol = nullptr;
                                                string tarAttributes;
                                                bool isGlobal = false;
                                                if (tar) { // local texture
                                                    char texTag[5];
                                                    Memory_Copy(texTag, tar->tag, 4);
                                                    texTag[4] = '\0';
                                                    tex.name = texTag;
                                                }
                                                else { // global or runtime-constructed texture
                                                    unsigned int tarOffset = unsigned int(At<TAR *>(globalParameters, 4)) - unsigned int(data);
                                                    auto it = symbolRelocations.find(tarOffset);
                                                    if (it != symbolRelocations.end()) {
                                                        auto const &s = (*it).second;
                                                        texSymbol = &s;
                                                        if (s.name.length() > 14 && s.name.starts_with("__EAGL::TAR:::")) {
                                                            tarAttributes = s.name.substr(14);
                                                            if (!tarAttributes.starts_with("RUNTIME_ALLOC")) {
                                                                tex.name = "global_" + tarAttributes;
                                                                isGlobal = true;
                                                            }
                                                            else {
                                                                auto shapenamePos = tarAttributes.find("SHAPENAME=");
                                                                if (shapenamePos != string::npos) {
                                                                    auto semiColonPos = tarAttributes.find_first_of(",;", shapenamePos + 10);
                                                                    if (semiColonPos != string::npos)
                                                                        tex.name = tarAttributes.substr(shapenamePos + 10, semiColonPos - shapenamePos - 10);
                                                                    else
                                                                        tex.name = tarAttributes.substr(shapenamePos + 10);
                                                                }
                                                            }
                                                        }
                                                    }
                                                }
                                                Texture *pTex = nullptr;
                                                if (!tex.name.empty()) {
                                                    auto texKey = ToLower(tex.name);
                                                    if (samplerIndex == 1 && (texKey == "spec")) {
                                                        mat.roughnessFactor = 0.3f;
                                                        mat.metallicFactor = 0.5f;
                                                    }
                                                    if (samplerIndex < 4) {
                                                        auto txit = textures.find(texKey);
                                                        if (txit != textures.end())
                                                            pTex = (*txit).second;
                                                        else {
                                                            if (tar) {
                                                                if (tar->wrapU == 3 || tar->wrapU == 5)
                                                                    tex.wrapU = 33071; // CLAMP_TO_EDGE
                                                                else if (tar->wrapU == 2)
                                                                    tex.wrapU = 33648; // MIRRORED_REPEAT
                                                                if (tar->wrapV == 3 || tar->wrapV == 5)
                                                                    tex.wrapV = 33071; // CLAMP_TO_EDGE
                                                                else if (tar->wrapV == 2)
                                                                    tex.wrapV = 33648; // MIRRORED_REPEAT
                                                            }
                                                            else if (texSymbol) {
                                                                if (!isGlobal && !tarAttributes.empty()) {
                                                                    bool foundU = false, foundV = false;
                                                                    {
                                                                        auto attrPos = tarAttributes.rfind("PCEXTOBJ_SetClampU=");
                                                                        if (attrPos != string::npos) {
                                                                            string attr;
                                                                            auto semiColonPos = tarAttributes.find_first_of(";", attrPos + 19);
                                                                            if (semiColonPos != string::npos)
                                                                                attr = tarAttributes.substr(attrPos + 19, semiColonPos - attrPos - 19);
                                                                            else
                                                                                attr = tarAttributes.substr(attrPos + 19);
                                                                            if (attr == "EAGL::PCCM_CLAMP" || attr == "EAGL::PCCM_CLAMPTOEDGE")
                                                                                tex.wrapU = 33071; // CLAMP_TO_EDGE
                                                                            else if (attr == "EAGL::PCCM_MIRROR")
                                                                                tex.wrapU = 33648; // MIRRORED_REPEAT
                                                                            foundU = true;
                                                                        }
                                                                    }
                                                                    {
                                                                        auto attrPos = tarAttributes.rfind("PCEXTOBJ_SetClampV=");
                                                                        if (attrPos != string::npos) {
                                                                            string attr;
                                                                            auto semiColonPos = tarAttributes.find_first_of(";", attrPos + 19);
                                                                            if (semiColonPos != string::npos)
                                                                                attr = tarAttributes.substr(attrPos + 19, semiColonPos - attrPos - 19);
                                                                            else
                                                                                attr = tarAttributes.substr(attrPos + 19);
                                                                            if (attr == "EAGL::PCCM_CLAMP" || attr == "EAGL::PCCM_CLAMPTOEDGE")
                                                                                tex.wrapV = 33071; // CLAMP_TO_EDGE
                                                                            else if (attr == "EAGL::PCCM_MIRROR")
                                                                                tex.wrapV = 33648; // MIRRORED_REPEAT
                                                                            foundV = true;
                                                                        }
                                                                    }
                                                                    if (!foundU && !foundV) {
                                                                        auto attrPos = tarAttributes.rfind("SetClampMode=");
                                                                        if (attrPos != string::npos) {
                                                                            string attr;
                                                                            auto semiColonPos = tarAttributes.find_first_of(";", attrPos + 13);
                                                                            if (semiColonPos != string::npos)
                                                                                attr = tarAttributes.substr(attrPos + 13, semiColonPos - attrPos - 13);
                                                                            else
                                                                                attr = tarAttributes.substr(attrPos + 13);
                                                                            if (attr == "EAGL::CM_CLAMP")
                                                                                tex.wrapU = tex.wrapV = 33071; // CLAMP_TO_EDGE
                                                                            else if (attr == "EAGL::CM_MIRROR")
                                                                                tex.wrapU = tex.wrapV = 33648; // MIRRORED_REPEAT
                                                                        }
                                                                    }
                                                                    {
                                                                        auto attrPos = tarAttributes.rfind("SetFilterMode=");
                                                                        if (attrPos != string::npos) {
                                                                            string attr;
                                                                            auto semiColonPos = tarAttributes.find_first_of(";", attrPos + 14);
                                                                            if (semiColonPos != string::npos)
                                                                                attr = tarAttributes.substr(attrPos + 14, semiColonPos - attrPos - 14);
                                                                            else
                                                                                attr = tarAttributes.substr(attrPos + 14);
                                                                            if (attr == "EAGL::FM_POINT")
                                                                                tex.magFilter = 9728; // NEAREST
                                                                        }
                                                                    }
                                                                    {
                                                                        auto attrPos = tarAttributes.rfind("SetMIPMAPMode=");
                                                                        if (attrPos != string::npos) {
                                                                            string attr;
                                                                            auto semiColonPos = tarAttributes.find_first_of(";", attrPos + 14);
                                                                            if (semiColonPos != string::npos)
                                                                                attr = tarAttributes.substr(attrPos + 14, semiColonPos - attrPos - 14);
                                                                            else
                                                                                attr = tarAttributes.substr(attrPos + 14);
                                                                            if (attr == "EAGL::MMM_NEAREST")
                                                                                tex.minFilter = 9985; // LINEAR_MIPMAP_NEAREST
                                                                            else if (attr == "EAGL::MMM_LINEAR")
                                                                                tex.minFilter = 9987; // LINEAR_MIPMAP_LINEAR
                                                                        }
                                                                    }
                                                                }
                                                            }
                                                            tex.source = tex.name + (options().jpegTextures ? ".jpeg" : ".png");
                                                            tex.mimeType = string("image/") + (options().jpegTextures ? "jpeg" : "png");
                                                            pTex = new Texture(tex);
                                                            textures[texKey] = pTex;
                                                        }
                                                    }
                                                }
                                                mat.textures[samplerIndex] = pTex;
                                            }
                                        }
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
                            }
                            j.openScope();
                            if (vertexBuffer) {
                                if (!shader) {
                                    if (skinVertexDataBuffer)
                                        shader = &DummyShader_Skin;
                                    else
                                        shader = &DummyShader;
                                }
                                j.openScope("attributes");
                                unsigned int attrOffset = 0;
                                bool hasBlendIndices = false;
                                bool hasBlendWeights = false;
                                for (auto const &d : shader->declaration) {
                                    if (d.usage == Shader::BlendIndices)
                                        hasBlendIndices = true;
                                    else if (d.usage == Shader::BlendWeight)
                                        hasBlendWeights = true;
                                }
                                bool uses2Streams = skinVertexDataBuffer && hasBlendIndices && hasBlendWeights;
                                unsigned int streamNumber = 0;
                                for (auto const &d : shader->declaration) {
                                    switch (d.usage) {
                                    case Shader::Position:
                                        j.writeFieldInt("POSITION", accessors.size());
                                        break;
                                    case Shader::Normal:
                                        j.writeFieldInt("NORMAL", accessors.size());
                                        break;
                                    case Shader::Texcoord0:
                                        j.writeFieldInt("TEXCOORD_0", accessors.size());
                                        break;
                                    case Shader::Texcoord1:
                                        j.writeFieldInt("TEXCOORD_1", accessors.size());
                                        break;
                                    case Shader::Texcoord2:
                                        j.writeFieldInt("TEXCOORD_2", accessors.size());
                                        break;
                                    case Shader::Color0:
                                        j.writeFieldInt("COLOR_0", accessors.size());
                                        break;
                                    case Shader::Color1:
                                        //j.writeFieldInt("COLOR_1", accessors.size());
                                        color1Offset = attrOffset;
                                        attrOffset += 4;
                                        continue;
                                    case Shader::BlendIndices:
                                        j.writeFieldInt("JOINTS_0", accessors.size());
                                        if (uses2Streams) {
                                            attrOffset = 0;
                                            streamNumber = 1;
                                        }
                                        break;
                                    case Shader::BlendWeight:
                                        j.writeFieldInt("WEIGHTS_0", accessors.size());
                                        break;
                                    }
                                    Accessor a;
                                    a.offset = attrOffset;
                                    auto declType = d.type;
                                    if (d.usage == Shader::BlendWeight && declType == Shader::Float3)
                                        declType = Shader::Float4;
                                    switch (declType) {
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
                                    else if (d.usage == Shader::Color0) {
                                        unsigned char *clr = (unsigned char *)(unsigned int(vertexBuffer) + a.offset);
                                        for (unsigned int vert = 0; vert < numVertices; vert++) {
                                            swap(clr[0], clr[2]);
                                            clr = (unsigned char *)(unsigned int(clr) + a.stride);
                                        }
                                    }
                                    accessors.push_back(a);
                                }
                                j.closeScope();
                                Buffer b;
                                b.data = vertexBuffer;
                                b.length = vertexSize * numVertices;
                                b.type = Buffer::Vertex;
                                b.stride = vertexSize;
                                buffers.push_back(b);
                                if (uses2Streams) {
                                    Buffer b2;
                                    VertexSkinData *vsb = new VertexSkinData[numVertices];
                                    Memory_Zero(vsb, numVertices * sizeof(VertexSkinData));
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
                                            *(unsigned char *)(&vsb[v].weights.x) = 0;
                                            *(unsigned char *)(&vsb[v].weights.y) = 0;
                                            *(unsigned char *)(&vsb[v].weights.z) = 0;
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
                                j.writeFieldInt("indices", accessors.size());
                                accessors.push_back(a);
                                Buffer b;
                                b.data = indexBuffer;
                                b.length = 2 * numIndices;
                                b.type = Buffer::Index;
                                b.stride = 2;
                                buffers.push_back(b);
                            }
                            j.writeFieldInt("mode", geoPrimMode);
                            unsigned int materialId = 0;
                            bool materialFound = false;
                            if (!options().noMeshJoin) {
                                for (unsigned int mid = 0; mid < materials.size(); mid++) {
                                    if (materials[mid].Compare(mat)) {
                                        materialId = mid;
                                        materialFound = true;
                                        break;
                                    }
                                }
                            }
                            if (!materialFound) {
                                materialId = materials.size();
                                materials.push_back(mat);
                            }
                            j.writeFieldInt("material", materialId);
                            j.closeScope();

                            // shapekeys

                        }
                        modelLayers += numPrimitives * (isOldFormat ? 2 : 1);
                        j.closeArray();
                        j.closeScope();
                    }
                }
                if (!colGeometries.empty()) {
                    for (unsigned int i = 0; i < colGeometries.size(); i++) {
                        // for each coll mesh:
                        //// trilist primitive:
                        ////// index buffer
                        ////// vertex buffer
                        //// linelist primitive:
                        ////// index buffer
                        ////// vertex buffer
                        auto &g = colGeometries[i];
                        j.openScope();
                        j.writeFieldString("name", g.name);
                        j.openArray("primitives");
                        if (!g.triangles.empty()) {
                            j.openScope();
                            j.openScope("attributes");
                            // create vertex buffer
                            map<pair<unsigned short, unsigned short>, unsigned int> usedVerts;
                            Vector3 vertMin, vertMax;
                            bool anyVertexProcessed = false;
                            for (auto const &t : g.triangles) {
                                for (unsigned int vi = 0; vi < 3; vi++) {
                                    auto key = make_pair(t.verts[vi], t.normal);
                                    if (!usedVerts.contains(key)) {
                                        auto newIndex = usedVerts.size();
                                        usedVerts[key] = newIndex;
                                        auto const &posn = g.positions[t.verts[vi]];
                                        if (!anyVertexProcessed) {
                                            vertMin = posn;
                                            vertMax = posn;
                                            anyVertexProcessed = true;
                                        }
                                        else {
                                            if (posn.x < vertMin.x)
                                                vertMin.x = posn.x;
                                            if (posn.y < vertMin.y)
                                                vertMin.y = posn.y;
                                            if (posn.z < vertMin.z)
                                                vertMin.z = posn.z;
                                            if (posn.x > vertMax.x)
                                                vertMax.x = posn.x;
                                            if (posn.y > vertMax.y)
                                                vertMax.y = posn.y;
                                            if (posn.z > vertMax.z)
                                                vertMax.z = posn.z;
                                        }
                                    }
                                }
                            }
                            g.triIndexBuffer.resize(g.triangles.size() * 3);
                            unsigned int ibcounter = 0;
                            for (auto const &t : g.triangles) {
                                for (unsigned int vi = 0; vi < 3; vi++)
                                    g.triIndexBuffer[ibcounter++] = usedVerts[make_pair(t.verts[vi], t.normal)];
                            }
                            g.triVertBuffer.resize(usedVerts.size());
                            for (auto const &[k, v] : usedVerts) {
                                g.triVertBuffer[v].pos = g.positions[k.first];
                                g.triVertBuffer[v].normal = g.normals[k.second];
                            }
                            j.writeFieldInt("POSITION", accessors.size());
                            Accessor colGeoAccessor;
                            colGeoAccessor.buffer = buffers.size();
                            colGeoAccessor.bufferType = exporter::Buffer::Type::Vertex;
                            colGeoAccessor.componentType = 5126;
                            colGeoAccessor.count = usedVerts.size();
                            colGeoAccessor.length = usedVerts.size() * 24;
                            colGeoAccessor.max = vertMax;
                            colGeoAccessor.min = vertMin;
                            colGeoAccessor.normalized = false;
                            colGeoAccessor.offset = 0;
                            colGeoAccessor.stride = 24;
                            colGeoAccessor.type = "VEC3";
                            colGeoAccessor.usesMinMax = true;
                            accessors.push_back(colGeoAccessor);
                            j.writeFieldInt("NORMAL", accessors.size());
                            colGeoAccessor.offset = 12;
                            colGeoAccessor.usesMinMax = false;
                            accessors.push_back(colGeoAccessor);
                            Buffer vb;
                            vb.data = g.triVertBuffer.data();
                            vb.length = g.triVertBuffer.size() * 24;
                            vb.type = Buffer::Vertex;
                            vb.stride = 24;
                            buffers.push_back(vb);
                            j.closeScope();
                            Accessor colIndicesAccessor;
                            colIndicesAccessor.buffer = buffers.size();
                            colIndicesAccessor.bufferType = Buffer::Index;
                            colIndicesAccessor.componentType = 5123;
                            colIndicesAccessor.count = g.triIndexBuffer.size();
                            colIndicesAccessor.length = g.triIndexBuffer.size() * 2;
                            colIndicesAccessor.normalized = false;
                            colIndicesAccessor.offset = 0;
                            colIndicesAccessor.stride = 2;
                            colIndicesAccessor.type = "SCALAR";
                            colIndicesAccessor.usesMinMax = false;
                            j.writeFieldInt("indices", accessors.size());
                            accessors.push_back(colIndicesAccessor);
                            Buffer ib;
                            ib.data = g.triIndexBuffer.data();
                            ib.length = g.triIndexBuffer.size() * 2;
                            ib.type = Buffer::Index;
                            ib.stride = 2;
                            buffers.push_back(ib);
                            j.writeFieldInt("mode", 4);
                            j.writeFieldInt("material", 0);
                            j.closeScope();
                        }
                        if (!g.edges.empty()) {
                            j.openScope();
                            j.openScope("attributes");
                            // create vertex buffer
                            map<unsigned short, unsigned int> usedVerts;
                            Vector3 vertMin, vertMax;
                            bool anyVertexProcessed = false;
                            for (auto const &e : g.edges) {
                                for (unsigned int vi = 0; vi < 2; vi++) {
                                    auto key = e.verts[vi];
                                    if (!usedVerts.contains(key)) {
                                        auto newIndex = usedVerts.size();
                                        usedVerts[key] = newIndex;
                                        auto const &posn = g.positions[e.verts[vi]];
                                        if (!anyVertexProcessed) {
                                            vertMin = posn;
                                            vertMax = posn;
                                            anyVertexProcessed = true;
                                        }
                                        else {
                                            if (posn.x < vertMin.x)
                                                vertMin.x = posn.x;
                                            if (posn.y < vertMin.y)
                                                vertMin.y = posn.y;
                                            if (posn.z < vertMin.z)
                                                vertMin.z = posn.z;
                                            if (posn.x > vertMax.x)
                                                vertMax.x = posn.x;
                                            if (posn.y > vertMax.y)
                                                vertMax.y = posn.y;
                                            if (posn.z > vertMax.z)
                                                vertMax.z = posn.z;
                                        }
                                    }
                                }
                            }
                            g.edgeIndexBuffer.resize(g.edges.size() * 2);
                            unsigned int ibcounter = 0;
                            for (auto const &e : g.edges) {
                                for (unsigned int vi = 0; vi < 2; vi++)
                                    g.edgeIndexBuffer[ibcounter++] = usedVerts[e.verts[vi]];
                            }
                            g.edgeVertBuffer.resize(usedVerts.size());
                            for (auto const &[k, v] : usedVerts)
                                g.edgeVertBuffer[v].pos = g.positions[k];
                            j.writeFieldInt("POSITION", accessors.size());
                            Accessor colGeoAccessor;
                            colGeoAccessor.buffer = buffers.size();
                            colGeoAccessor.bufferType = exporter::Buffer::Type::Vertex;
                            colGeoAccessor.componentType = 5126;
                            colGeoAccessor.count = usedVerts.size();
                            colGeoAccessor.length = usedVerts.size() * 12;
                            colGeoAccessor.max = vertMax;
                            colGeoAccessor.min = vertMin;
                            colGeoAccessor.normalized = false;
                            colGeoAccessor.offset = 0;
                            colGeoAccessor.stride = 12;
                            colGeoAccessor.type = "VEC3";
                            colGeoAccessor.usesMinMax = true;
                            accessors.push_back(colGeoAccessor);
                            Buffer vb;
                            vb.data = g.edgeVertBuffer.data();
                            vb.length = g.edgeVertBuffer.size() * 12;
                            vb.type = Buffer::Vertex;
                            vb.stride = 12;
                            buffers.push_back(vb);
                            j.closeScope();
                            Accessor colIndicesAccessor;
                            colIndicesAccessor.buffer = buffers.size();
                            colIndicesAccessor.bufferType = Buffer::Index;
                            colIndicesAccessor.componentType = 5123;
                            colIndicesAccessor.count = g.edgeIndexBuffer.size();
                            colIndicesAccessor.length = g.edgeIndexBuffer.size() * 2;
                            colIndicesAccessor.normalized = false;
                            colIndicesAccessor.offset = 0;
                            colIndicesAccessor.stride = 2;
                            colIndicesAccessor.type = "SCALAR";
                            colIndicesAccessor.usesMinMax = false;
                            j.writeFieldInt("indices", accessors.size());
                            accessors.push_back(colIndicesAccessor);
                            Buffer ib;
                            ib.data = g.edgeIndexBuffer.data();
                            ib.length = g.edgeIndexBuffer.size() * 2;
                            ib.type = Buffer::Index;
                            ib.stride = 2;
                            buffers.push_back(ib);
                            j.writeFieldInt("mode", 1);
                            j.writeFieldInt("material", 1);
                            j.closeScope();
                        }
                        j.closeArray();
                        j.closeScope();
                    }
                }
                j.closeArray();
            }
            
            vector<Texture *> vecTextures;
            if (!textures.empty()) {
                // samplers
                j.openArray("samplers");
                for (auto const &[k, t] : textures) {
                    j.openScope();
                    j.writeFieldString("name", t->name);
                    j.writeFieldInt("magFilter", t->magFilter);
                    j.writeFieldInt("minFilter", t->minFilter);
                    //if (t->wrapU != 10497)
                        j.writeFieldInt("wrapS", t->wrapU);
                    //if (t->wrapV != 10497)
                        j.writeFieldInt("wrapT", t->wrapV);
                    j.closeScope();
                    vecTextures.push_back(t);
                }
                j.closeArray();
                // textures
                j.openArray("textures");
                unsigned int i = 0;
                for (auto const &[k, t] : textures) {
                    j.openScope();
                    j.writeFieldString("name", t->name);
                    j.writeFieldInt("sampler", i);
                    j.writeFieldInt("source", i);
                    j.closeScope();
                    i++;
                }
                j.closeArray();
                // images
                j.openArray("images");
                for (auto const &[k, t] : textures) {
                    j.openScope();
                    j.writeFieldString("name", t->name);
                    j.writeFieldString("mimeType", t->mimeType);
                    j.writeFieldString("uri", t->source);
                    j.closeScope();

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
                            pngBuf.WriteToFile(outPath.parent_path() / t->source);
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
                            jpegBuf.WriteToFile(outPath.parent_path() / t->source);
                        }
                    }
                }
                j.closeArray();
            }
            // materials
            bool hasSpecOrEnvMap = false;
            if (!materials.empty()) {
                j.openArray("materials");
                for (unsigned int i = 0; i < materials.size(); i++) {
                    j.openScope();
                    string matOptionsStr = materials[i].shader;
                    if (options().keepTex0InMatOptions && materials[i].textures[0])
                        matOptionsStr += ",tex0:" + materials[i].textures[1]->name;
                    if (materials[i].textures[1])
                        matOptionsStr += ",tex1:" + materials[i].textures[1]->name;
                    if (materials[i].textures[2])
                        matOptionsStr += ",tex2:" + materials[i].textures[2]->name;
                    if (materials[i].textures[3])
                        matOptionsStr += ",tex3:" + materials[i].textures[3]->name;
                    j.writeFieldString("name", string("material") + Format("%02d", i) + " [" + matOptionsStr + "]");
                    if (materials[i].doubleSided)
                        j.writeFieldBool("doubleSided", true);
                    if (!materials[i].alphaMode.empty())
                        j.writeFieldString("alphaMode", "BLEND");
                    j.openScope("pbrMetallicRoughness");
                    j.writeFieldFloat("metallicFactor", materials[i].metallicFactor);
                    j.writeFieldFloat("roughnessFactor", materials[i].roughnessFactor);
                    if (!options().noTextures) {
                        int diffuseTexId = -1, specTexId = -1;
                        if (materials[i].textures[0]) {
                            for (unsigned int ti = 0; ti < vecTextures.size(); ti++) {
                                if (materials[i].textures[0] == vecTextures[ti]) {
                                    diffuseTexId = ti;
                                    break;
                                }
                            }
                        }
                        /* disabled
                        if (materials[i].textures[1]) {
                            for (unsigned int ti = 0; ti < vecTextures.size(); ti++) {
                                if (materials[i].textures[1] == vecTextures[ti]) {
                                    specTexId = ti;
                                    break;
                                }
                            }
                        }
                        */
                        if (specTexId != -1) {
                            j.closeScope();
                            j.openScope("extensions");
                            j.openScope("KHR_materials_pbrSpecularGlossiness");
                            if (diffuseTexId != -1) {
                                j.openScope("diffuseTexture");
                                j.writeFieldInt("index", diffuseTexId);
                                //j.writeFieldInt("texCoord", 0);
                                j.closeScope();
                            }
                            j.openScope("specularGlossinessTexture");
                            j.writeFieldInt("index", specTexId);
                            //j.writeFieldInt("texCoord", 0);
                            j.closeScope();
                            j.closeScope();
                            j.closeScope();
                            if (!hasSpecOrEnvMap)
                                hasSpecOrEnvMap = true;
                        }
                        else if (diffuseTexId != -1) {
                            j.openScope("baseColorTexture");
                            j.writeFieldInt("index", diffuseTexId);
                            //j.writeFieldInt("texCoord", 0);
                            j.closeScope();
                            j.closeScope();
                        }
                        else
                            j.closeScope();
                    }
                    else
                        j.closeScope();
                    j.closeScope();
                }
                j.closeArray();
            }
            // extensions
            if (hasSpecOrEnvMap) {
                j.openArray("extensionsUsed");
                j.writeValueString("KHR_materials_pbrSpecularGlossiness");
                j.closeArray();
                j.openArray("extensionsRequired");
                j.writeValueString("KHR_materials_pbrSpecularGlossiness");
                j.closeArray();
            }
            // accessors
            if (accessors.size() > 0) {
                j.openArray("accessors");
                
                for (auto const &a : accessors) {
                    j.openScope();
                    j.writeFieldInt("bufferView", a.buffer);
                    j.writeFieldInt("componentType", a.componentType);
                    j.writeFieldInt("count", a.count);
                    j.writeFieldString("type", a.type);
                    j.writeFieldInt("byteOffset", a.offset);
                    if (a.normalized)
                        j.writeFieldBool("normalized", true);
                    if (a.usesMinMax) {
                        j.openArray("min");
                        j.writeValueFloat(a.min.x);
                        j.writeValueFloat(a.min.y);
                        j.writeValueFloat(a.min.z);
                        j.closeArray();
                        j.openArray("max");
                        j.writeValueFloat(a.max.x);
                        j.writeValueFloat(a.max.y);
                        j.writeValueFloat(a.max.z);
                        j.closeArray();
                    }
                    j.closeScope();
                }
                j.closeArray();
            }
            // bufferViews
            if (buffers.size() > 0) {
                unsigned int i = 0;
                j.openArray("bufferViews");
                for (auto const &b : buffers) {
                    j.openScope();
                    j.writeFieldInt("buffer", i++);
                    j.writeFieldInt("byteLength", b.length);
                    if (b.type == Buffer::Vertex || b.type == Buffer::VertexSkin)
                        j.writeFieldInt("byteStride", b.stride);
                    else if (b.type == Buffer::Index)
                        j.writeFieldInt("target", 34963);
                    j.closeScope();
                }
                j.closeArray();
                // buffers
                j.openArray("buffers");
                for (auto const &b : buffers) {
                    j.openScope();
                    j.writeFieldInt("byteLength", b.length);
                    j.writeFieldString("uri", "data:application/octet-stream;base64," + j.base64_encode((unsigned char *)b.data, b.length));
                    j.closeScope();
                }
                j.closeArray();
            }
        }
        j.endScope();

        for (auto const &v : vertexSkinBuffers)
            delete[] v;
        delete[] skinMatrices;
        for (auto const &e : textures)
            delete e.second;
        delete skeletonFileData;
    }

    void convert_o_to_gltf(path const &inPath, path const &outPath) {
        auto fileData = readofile(inPath);
        if (fileData.first) {
            convert_o_to_gltf(fileData.first, fileData.second, outPath, inPath);
            delete[] fileData.first;
        }
    }
};

void oexport(path const &out, path const &in) {
    exporter e;
    e.convert_o_to_gltf(in, out);
}
