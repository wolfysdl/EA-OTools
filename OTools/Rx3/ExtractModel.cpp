#include "d3dx9.h"
#include "ExtractModel.h"
#include "ExtractNames.h"
#include "..\jsonwriter.h"
#include "..\utils.h"
#include "..\half_float\umHalf.h"

const bool SwapYZ = false;

enum DataType {
    dt_unknown,
    dt_2f32,  // 2x float32
    dt_3f32,  // 3x float32
    dt_4f32,  // 4x float32
    dt_3s10n, // 3x signed 10 bit normalized
    dt_2f16,  // 2x half-float
    dt_4f16,  // 4x half-float
    dt_4u8,   // 4x unsigned 8 bit
    dt_4u16,  // 4x unsigned 16 bit
    dt_4u8n,  // 4x unsigned 8 bit normalized
    dt_4u16n  // 4x unsigned 16 bit normalized
};

struct Matrix {
    float f[16];
};

struct Bone {
    unsigned char parent = 0;
    unsigned char sibling = 0;
    vector<unsigned char> children;
};

DataType DataTypeFromTypeName(string const &name) {
    if (name == "2f32")
        return dt_2f32;
    if (name == "3f32")
        return dt_3f32;
    if (name == "4f32")
        return dt_4f32;
    if (name == "3s10n")
        return dt_3s10n;
    if (name == "2f16")
        return dt_2f16;
    if (name == "4f16")
        return dt_4f16;
    if (name == "4u8")
        return dt_4u8;
    if (name == "4u16")
        return dt_4u16;
    if (name == "4u8n")
        return dt_4u8n;
    if (name == "4u16n")
        return dt_4u16n;
    return dt_unknown;
}

template<size_t D, typename DstT, typename SrcT>
void convert_attr(void *dst, void const *src) {
    if constexpr (is_same_v<DstT, SrcT>)
        memcpy(dst, src, sizeof(DstT) * D);
    else {
        DstT *fDst = (DstT *)dst;
        SrcT const *hSrc = (SrcT const *)src;
        for (unsigned int i = 0; i < D; i++)
            fDst[i] = hSrc[i];
    }
}

void convert_attr_3s10n_to_float3(void *dst, void const *src) {
    struct _3s10n {
        int a : 10;
        int b : 10;
        int c : 10;
    };
    float *fDst = (float *)dst;
    _3s10n const *hSrc = (_3s10n const *)src;
    fDst[0] = float(hSrc->a) / 511;
    fDst[1] = float(hSrc->b) / 511;
    fDst[2] = float(hSrc->c) / 511;
}

void convert_attr_3s10n_to_float4(void *dst, void const *src) {
    float *fDst = (float *)dst;
    convert_attr_3s10n_to_float3(dst, src);
    fDst[3] = 1;
}

void ExtractModelFromContainer(Rx3Container const &container, path const &outputPath) {
    struct Accessor {
        unsigned int componentType = 0;
        unsigned int count = 0;
        string type;
        string usage;
        unsigned int offset = 0;
        unsigned int stride = 0;
        unsigned int target = 0;
        bool normalized = false;
        bool usesMinMax = false;
        float min[3] = {};
        float max[3] = {};
        bool minMaxProcessedAnyVertex = false;
        void (*convert_fn)(void *, void const *) = nullptr;
        vector<unsigned char> data;
    };
    struct Material {
        string name;
        string texName;
    };
    create_directories(outputPath.parent_path());
    auto indexBufferSections = container.FindAllSections(RX3_SECTION_INDEX_BUFFER);
    auto vertexBufferSections = container.FindAllSections(RX3_SECTION_VERTEX_BUFFER);
    auto vertexDeclarationSections = container.FindAllSections(RX3_SECTION_VERTEX_DECLARATION);
    auto boneMatricesSection = container.FindFirstSection(RX3_SECTION_BONE_MATRICES);
    auto skeletonsSections = container.FindAllSections(RX3_SECTION_SKELETON);
    if (
        !indexBufferSections.empty() &&
        indexBufferSections.size() == vertexBufferSections.size() &&
        indexBufferSections.size() == vertexDeclarationSections.size() &&
        (skeletonsSections.empty() || indexBufferSections.size() == skeletonsSections.size())
        )
    {
        bool hasSkeleton = boneMatricesSection != nullptr;
        vector<string> primNames;
        auto geometryNameSections = container.FindFirstSection(RX3_SECTION_NAMES);
        if (geometryNameSections) {
            auto names = ExtractNamesFromSection(geometryNameSections);
            for (auto const &[id, name] : names) {
                if (id == 3566041216)
                    primNames.push_back(name);
            }
        }
        unsigned int numPrimitives = indexBufferSections.size();
        unsigned int numMeshNodes = 1;
        unsigned int numBones = 0;
        primNames.resize(numPrimitives);
        vector<Accessor> accessors;
        vector<Material> materials;
        vector<Matrix> boneMatrices;
        vector<Bone> bones;
        path gltfPath = outputPath;
        gltfPath.replace_extension(".gltf");
        JsonWriter j(gltfPath);
        j.startScope();
        j.openScope("asset");
        j.writeFieldString("generator", string("RX3>GLTF extractor"));
        j.writeFieldString("version", "2.0");
        j.closeScope();
        j.writeFieldInt("scene", 0);

        j.openArray("scenes");
        j.openScope();
        j.openArray("nodes");
        for (unsigned int i = 0; i < (numMeshNodes + (hasSkeleton ? 1 : 0)); i++)
            j.writeValueInt(i);
        j.closeArray();
        j.closeScope();
        j.closeArray();
        // nodes
        j.openArray("nodes");
        for (unsigned int i = 0; i < numMeshNodes; i++) {
            j.openScope();
            j.writeFieldInt("mesh", i);
            if (hasSkeleton)
                j.writeFieldInt("skin", 0);
            j.closeScope();
        }
        if (hasSkeleton) {
            Rx3Reader boneMatricesReader(boneMatricesSection);
            boneMatricesReader.Skip(4);
            numBones = boneMatricesReader.Read<unsigned int>();
            boneMatricesReader.Skip(8);
            if (numBones > 0) {
                boneMatrices.resize(numBones);
                Accessor a;
                a.componentType = 5126;
                a.count = numBones;
                a.type = "MAT4";
                a.stride = 64;
                a.data.resize(a.count * 64);
                memcpy(a.data.data(), boneMatricesReader.GetCurrentPtr(), a.data.size());
                Matrix *m = (Matrix *)a.data.data();
                for (unsigned int b = 0; b < numBones; b++) {
                    m[b].f[15] = 1.0f;
                    //Matrix im;
                    //D3DXMatrixInverse((D3DXMATRIX *)&im, NULL, (D3DXMATRIX *)m);
                    //m[b] = im;
                }
                accessors.push_back(a);
                memcpy(boneMatrices.data(), boneMatricesReader.GetCurrentPtr(), boneMatrices.size() * sizeof(Matrix));
                for (unsigned int b = 0; b < numBones; b++) {
                    //boneMatrices[b].f[15] = 1.0f;
                    Matrix im;
                    D3DXMatrixInverse((D3DXMATRIX *)&im, NULL, (D3DXMATRIX *)&boneMatrices[b]);
                    boneMatrices[b] = im;
                }
                bones.resize(numBones);
                for (unsigned int s = 0; s < skeletonsSections.size(); s++) {
                    Rx3Reader skeletonReader(skeletonsSections[s]);
                    skeletonReader.Skip(16);
                    for (unsigned int b = 0; b < numBones; b++) {
                        if (!bones[b].parent)
                            bones[b].parent = skeletonReader.Read<unsigned char>();
                        else
                            skeletonReader.Skip(1);
                        if (!bones[b].sibling)
                            bones[b].sibling = skeletonReader.Read<unsigned char>();
                        else
                            skeletonReader.Skip(1);
                    }
                }
            }
            j.openScope();
            j.writeFieldString("name", "Skeleton");
            j.openArray("children");
            for (unsigned int b = 0; b < numBones; b++)
                j.writeValueInt(numMeshNodes + 1 + b);
            j.closeArray();
            j.closeScope();
        }
        for (unsigned int b = 0; b < numBones; b++) {
            j.openScope();
            j.writeFieldString("name", "bone_" + to_string(b));
            //j.openArray("matrix");
            //for (unsigned int m = 0; m < 16; m++) {
            //    j.writeValueFloat(boneMatrices[b].f[m]);
            //}
            //j.closeArray();
            j.closeScope();
        }
        j.closeArray();
        if (hasSkeleton) { 
            j.openArray("skins");
            j.openScope();
            if (numBones > 0)
                j.writeFieldInt("inverseBindMatrices", 0);
            unsigned int skelRootNodeIndex = numMeshNodes;
            j.writeFieldInt("skeleton", skelRootNodeIndex);
            if (numBones > 0) {
                j.openArray("joints");
                for (unsigned int i = 0; i < numBones; i++)
                    j.writeValueInt(skelRootNodeIndex + 1 + i);
                j.closeArray();
            }
            j.closeScope();
            j.closeArray();
        }
        j.openArray("meshes");
        j.openScope();
        j.openArray("primitives");
        for (unsigned int i = 0; i < numPrimitives; i++) {
            j.openScope();
            Rx3Reader vertexDeclReader(vertexDeclarationSections[i]);
            Rx3Reader vertexBufferReader(vertexBufferSections[i]);
            Rx3Reader indexBufferReader(indexBufferSections[i]);
            vertexDeclReader.Skip(4);
            unsigned int declStrLen = vertexDeclReader.Read<unsigned int>();
            if (declStrLen > 0) {
                vertexDeclReader.Skip(8);
                auto declElements = Split(vertexDeclReader.GetString(), ' ');
                if (!declElements.empty()) {
                    vertexBufferReader.Skip(4);
                    unsigned int numVertices = vertexBufferReader.Read<unsigned int>();
                    unsigned int vertexStride = vertexBufferReader.Read<unsigned int>();
                    vertexBufferReader.Skip(4);
                    auto vertexBufferData = vertexBufferReader.GetCurrentPtr();
                    indexBufferReader.Skip(4);
                    unsigned int numIndices = indexBufferReader.Read<unsigned int>();
                    unsigned char indexStride = indexBufferReader.Read<unsigned char>();
                    indexBufferReader.Skip(7);
                    auto indexBufferData = indexBufferReader.GetCurrentPtr();
                    unsigned int accessorStartIndex = accessors.size();
                    for (unsigned int d = 0; d < declElements.size(); d++) {
                        auto elementInfo = Split(declElements[d], ':');
                        if (elementInfo.size() == 5 && elementInfo[0].size() == 2 && elementInfo[1].size() == 2) {
                            DataType t = DataTypeFromTypeName(elementInfo[4]);
                            Accessor a;
                            sscanf_s(elementInfo[1].c_str(), "%02X", &a.offset);
                            if (elementInfo[0] == "p0") {
                                a.usage = "POSITION";
                                a.type = "VEC3";
                                a.componentType = 5126;
                                a.stride = 12;
                                a.usesMinMax = true;
                                if (t == dt_3f32)
                                    a.convert_fn = convert_attr<3, float, float>;
                                else if (t == dt_4f16)
                                    a.convert_fn = convert_attr<3, float, half>;
                            }
                            else if (elementInfo[0] == "n0") {
                                a.usage = "NORMAL";
                                a.type = "VEC3";
                                a.componentType = 5126;
                                a.stride = 12;
                                if (t == dt_3s10n)
                                    a.convert_fn = convert_attr_3s10n_to_float3;
                            }
                            else if (elementInfo[0] == "g0") {
                                a.usage = "TANGENT";
                                a.type = "VEC4";
                                a.componentType = 5126;
                                a.stride = 16;
                                if (t == dt_3s10n)
                                    a.convert_fn = convert_attr_3s10n_to_float4;
                            }
                            else if (elementInfo[0][0] == 't') {
                                a.usage = string("TEXCOORD_") + elementInfo[0][1];
                                a.type = "VEC2";
                                a.componentType = 5126;
                                a.stride = 8;
                                if (t == dt_2f32)
                                    a.convert_fn = convert_attr<2, float, float>;
                                else if (t == dt_2f16)
                                    a.convert_fn = convert_attr<2, float, half>;
                            }
                            else if (elementInfo[0][0] == 'c') {
                                a.usage = string("COLOR_") + elementInfo[0][1];
                                a.type = "VEC4";
                                a.componentType = 5121;
                                a.stride = 4;
                                a.normalized = true;
                                if (t == dt_4u8n)
                                    a.convert_fn = convert_attr<4, unsigned char, unsigned char>;
                            }
                            else if (elementInfo[0][0] == 'i') {
                                a.usage = string("JOINTS_") + elementInfo[0][1];
                                a.type = "VEC4";
                                if (t == dt_4u8) {
                                    a.componentType = 5121;
                                    a.stride = 4;
                                    a.convert_fn = convert_attr<4, unsigned char, unsigned char>;
                                }
                                else if (t == dt_4u16) {
                                    a.componentType = 5123;
                                    a.stride = 16;
                                    a.convert_fn = convert_attr<4, unsigned short, unsigned short>;
                                }
                            }
                            else if (elementInfo[0][0] == 'w') {
                                a.usage = string("WEIGHTS_") + elementInfo[0][1];
                                a.type = "VEC4";
                                a.componentType = 5121;
                                a.stride = 4;
                                a.normalized = true;
                                if (t == dt_4u8n)
                                    a.convert_fn = convert_attr<4, unsigned char, unsigned char>;
                            }
                            if (a.convert_fn)
                                accessors.push_back(a);
                        }
                    }
                    if (accessors.size() > accessorStartIndex) {
                        j.openScope("attributes");
                        for (unsigned int ai = accessorStartIndex; ai < accessors.size(); ai++) {
                            auto &a = accessors[ai];
                            j.writeFieldInt(a.usage, ai);
                            a.count = numVertices;
                            a.data.resize(a.count * a.stride);
                        }
                        for (unsigned int vi = 0; vi < numVertices; vi++) {
                            unsigned char const *vertexData = (unsigned char const *)vertexBufferData + vi * vertexStride;
                            for (unsigned int ai = accessorStartIndex; ai < accessors.size(); ai++) {
                                auto &a = accessors[ai];
                                a.convert_fn(&a.data[vi * a.stride], &vertexData[a.offset]);
                                if (a.usesMinMax) {
                                    float *pos = (float *)&a.data[vi * a.stride];
                                    if (!a.minMaxProcessedAnyVertex) {
                                        memcpy(a.min, pos, 12);
                                        memcpy(a.max, pos, 12);
                                        a.minMaxProcessedAnyVertex = true;
                                    }
                                    else {
                                        for (unsigned int ip = 0; ip < 3; ip++) {
                                            if (pos[ip] < a.min[ip])
                                                a.min[ip] = pos[ip];
                                            if (pos[ip] > a.max[ip])
                                                a.max[ip] = pos[ip];
                                        }
                                    }
                                }
                                if (SwapYZ && a.stride == 12 && a.componentType == 5126) {
                                    float *pos = (float *)&a.data[vi * a.stride];
                                    swap(pos[1], pos[2]);
                                    pos[1] *= -1.0f;
                                }
                            }
                        }
                        j.closeScope();
                        if (indexStride == 2 || indexStride == 4) {
                            j.writeFieldInt("indices", accessors.size());
                            Accessor ia;
                            ia.componentType = (indexStride == 2) ? 5123 : 5125;
                            ia.count = numIndices;
                            ia.type = "SCALAR";
                            ia.stride = indexStride;
                            ia.target = 34963;
                            ia.data.resize(ia.count *ia.stride);
                            memcpy(ia.data.data(), indexBufferData, ia.data.size());
                            accessors.push_back(ia);
                        }
                        j.writeFieldInt("material", materials.size());
                        Material mat;
                        if (i < primNames.size() && !primNames[i].empty()) {
                            mat.name = primNames[i];
                            mat.texName = primNames[i];
                            auto dp = mat.texName.find("_.");
                            if (dp != string::npos)
                                mat.texName = mat.texName.substr(0, dp) /*+ "_cm"*/;
                        }
                        else
                            mat.name = "Material_" + to_string(i);
                        materials.push_back(mat);
                    }

                }
            }
            j.closeScope();
        }
        j.closeArray();
        j.closeScope();
        j.closeArray();
        vector<string> textures;
        if (!materials.empty()) {
            j.openArray("materials");
            for (unsigned int i = 0; i < materials.size(); i++) {
                j.openScope();
                j.writeFieldString("name", materials[i].name);
                j.openScope("pbrMetallicRoughness");
                j.writeFieldFloat("metallicFactor", 0.0f);
                j.writeFieldFloat("roughnessFactor", 1.0f);
                if (!materials[i].texName.empty()) {
                    j.openScope("baseColorTexture");
                    j.writeFieldInt("index", textures.size());
                    textures.push_back(materials[i].texName);
                    j.closeScope();
                }
                j.closeScope();
                j.closeScope();
            }
            j.closeArray();
        }
        if (!textures.empty()) {
            j.openArray("textures");
            for (unsigned int i = 0; i < textures.size(); i++) {
                j.openScope();
                j.writeFieldString("name", textures[i]);
                j.writeFieldInt("source", i);
                j.closeScope();
            }
            j.closeArray();
            j.openArray("images");
            for (unsigned int i = 0; i < textures.size(); i++) {
                j.openScope();
                j.writeFieldString("name", textures[i]);
                j.writeFieldString("mimeType", "image/png");
                j.writeFieldString("uri", textures[i] + ".png");
                j.closeScope();
            }
            j.closeArray();
        }
        if (!accessors.empty()) {
            j.openArray("accessors");
            for (unsigned int i = 0; i < accessors.size(); i++) {
                auto const &a = accessors[i];
                j.openScope();
                j.writeFieldInt("bufferView", i);
                j.writeFieldInt("componentType", a.componentType);
                j.writeFieldInt("count", a.count);
                j.writeFieldString("type", a.type);
                if (a.normalized)
                    j.writeFieldBool("normalized", true);
                if (a.usesMinMax) {
                    j.openArray("min");
                    j.writeValueFloat(a.min[0]);
                    j.writeValueFloat(a.min[1]);
                    j.writeValueFloat(a.min[2]);
                    j.closeArray();
                    j.openArray("max");
                    j.writeValueFloat(a.max[0]);
                    j.writeValueFloat(a.max[1]);
                    j.writeValueFloat(a.max[2]);
                    j.closeArray();
                }
                j.closeScope();
            }
            j.closeArray();
            j.openArray("bufferViews");
            for (unsigned int i = 0; i < accessors.size(); i++) {
                auto const &a = accessors[i];
                j.openScope();
                j.writeFieldInt("buffer", i);
                j.writeFieldInt("byteLength", a.data.size());
                if (a.target != 0)
                    j.writeFieldInt("target", a.target);
                j.closeScope();
            }
            j.closeArray();
            j.openArray("buffers");
            for (auto const &a : accessors) {
                j.openScope();
                j.writeFieldInt("byteLength", a.data.size());
                j.writeFieldString("uri", "data:application/octet-stream;base64," + j.base64_encode(a.data.data(), a.data.size()));
                j.closeScope();
            }
            j.closeArray();
        }
        j.endScope();
    }
}

void ExtractModelFromRX3(path const &rx3path, path const &outputPath) {
    Rx3Container rx3(rx3path);
    ExtractModelFromContainer(rx3, outputPath);
}
