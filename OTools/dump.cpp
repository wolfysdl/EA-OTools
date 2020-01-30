#include "main.h"
#include <stdio.h>
#include <fstream>

namespace dump {

struct Relocation : public Elf32_Rel {

};

struct Symbol : public Elf32_Sym {
    string name;
};

class Writer {
public:
    static unsigned int mSpacing;
    static string mResult;
    static void openScope(string const &title, unsigned int offset, string const &comment = string());
    static void closeScope();
    static string spacing();
    static void writeLine(string const &line, string const &comment = string());
    static void writeField(string const &name, string const &value, string const &comment = string());
};

string Writer::mResult;
unsigned int Writer::mSpacing = 0;
const unsigned int SPACING = 4;
map<unsigned int, Symbol> symbolRelocations;
map<string, unsigned int> codeTechniques;
void *currentData = nullptr;

void Writer::openScope(string const &title, unsigned int offset, string const &comment) {
    mResult += spacing() + title;
    if (mSpacing == 0)
        mResult += " @" + Format("%X", offset);
    mResult += " {";
    if (!comment.empty())
        mResult += " // " + comment;
    mResult += "\n";
    mSpacing += SPACING;
}

void Writer::closeScope() {
    mSpacing -= SPACING;
    mResult += spacing() + "}\n";
}

string Writer::spacing() {
    if (mSpacing > 0)
        return string(mSpacing, L' ');
    return string();
}

void Writer::writeLine(string const &line, string const &comment) {
    mResult += spacing() + line;
    if (!comment.empty())
        mResult += " // " + comment;
    mResult += "\n";
}

void Writer::writeField(string const &name, string const &value, string const &comment) {
    mResult += spacing() + name + ": " + value;
    if (!comment.empty())
        mResult += " // " + comment;
    mResult += "\n";
}

struct Struct {
    unsigned int(*mWriter)(void *, string const &, unsigned char *, unsigned int);
    unsigned int(*mArrayWriter)(void *, string const &, unsigned char *, unsigned int, unsigned int);

    Struct() {
        mWriter = nullptr;
        mArrayWriter = nullptr;
    }

    Struct(unsigned int(*writer)(void *, string const &, unsigned char *, unsigned int)) {
        mWriter = writer;
        mArrayWriter = nullptr;
    }

    Struct(unsigned int(*writer)(void *, string const &, unsigned char *, unsigned int), unsigned int(*arrayWriter)(void *, string const &, unsigned char *, unsigned int, unsigned int)) {
        mWriter = writer;
        mArrayWriter = arrayWriter;
    }
};

map<string, Struct> &GetStructs() {
    static map<string, Struct> structs;
    return structs;
}

unsigned int WriteInt8(void *, string const &name, unsigned char *data, unsigned int offset) {
    Writer::writeField(name, Format("%d", GetAt<char>(data, offset)));
    return 1;
}

unsigned int WriteUInt8(void *, string const &name, unsigned char *data, unsigned int offset) {
    Writer::writeField(name, Format("%u", GetAt<unsigned char>(data, offset)));
    return 1;
}

unsigned int WriteInt16(void *, string const &name, unsigned char *data, unsigned int offset) {
    Writer::writeField(name, Format("%d", GetAt<short>(data, offset)));
    return 2;
}

unsigned int WriteUInt16(void *, string const &name, unsigned char *data, unsigned int offset) {
    Writer::writeField(name, Format("%u", GetAt<unsigned short>(data, offset)));
    return 2;
}

unsigned int WriteInt32(void *, string const &name, unsigned char *data, unsigned int offset) {
    Writer::writeField(name, Format("%d", GetAt<int>(data, offset)));
    return 4;
}

unsigned int WriteUInt32(void *, string const &name, unsigned char *data, unsigned int offset) {
    Writer::writeField(name, Format("%u", GetAt<unsigned int>(data, offset)));
    return 4;
}

unsigned int WriteBool32(void *, string const &name, unsigned char *data, unsigned int offset) {
    Writer::writeField(name, Format("%u", GetAt<unsigned int>(data, offset)));
    return 4;
}

unsigned int WriteFloat(void *, string const &name, unsigned char *data, unsigned int offset) {
    Writer::writeField(name, Format("%.4f", GetAt<float>(data, offset)));
    return 4;
}

string VShaderDeclType(unsigned char type) {
    switch (type) {
    case 0:
        return "D3DCOLOR";
    case 1:
        return "FLOAT2";
    case 2:
        return "FLOAT3";
    case 3:
        return "FLOAT4";
    case 4:
        return "D3DCOLOR";
    case 5:
        return "";
    }
    return Format("%02X", type);
}

unsigned int WriteVShaderDecl(void *, string const &name, unsigned char *data, unsigned int offset) {
    unsigned char *decl = At<unsigned char>(data, offset);
    Writer::writeField(name, Format("%02X %02X %02X %02X", decl[0], decl[1], decl[2], decl[3]));
    return 4;
}

string CommandName(unsigned int id) {
    switch (id) {
    case 0:
        return "NO_COMMAND";
    case 1:
        return "NOP_1";
    case 2:
        return "SET_VERTEX_SHADER_CONSTANT_G";
    case 3:
        return "NOP_3";
    case 4:
        return "SET_STREAM_SOURCE";
    case 6:
        return "NOP_6";
    case 7:
        return "SET_INDEX_BUFFER";
    case 9:
        return "SET_SAMPLER_G";
    case 16:
        return "DRAW_PRIM";
    case 17:
        return "DRAW_INDEXED_PRIM_NO_Z_WRITE";
    case 18:
        return "DRAW_INDEXED_PRIM";
    case 28:
        return "SET_VERTEX_BONE_WEIGHTS";
    case 30:
        return "SET_VERTEX_SHADER_CONSTANT_L_30";
    case 31:
        return "SET_VERTEX_SHADER_CONSTANT_L_31";
    case 32:
        return "SET_SAMPLER";
    case 33:
        return "SET_GEO_PRIM_STATE";
    case 35:
        return "SET_VERTEX_SHADER_TRANSPOSED_MATRIX";
    case 40:
        return "SET_ANIMATION_BUFFER";
    case 46:
        return "SET_VERTEX_SHADER_CONSTANT_L_46";
    case 65:
        return "DRAW_INDEXED_PRIM_AND_END";
    case 69:
        return "SETUP_RENDER";
    case 72:
        return "SET_PIXEL_SHADER_CONTANT_G_72";
    case 73:
        return "SET_PIXEL_SHADER_CONTANT_G_73";
    case 75:
        return "SET_STREAM_SOURCE_SKINNED";
    }
    return Format("COMMAND_%02d", id);
}

unsigned int WriteCommand(void *, string const &name, unsigned char *data, unsigned int offset) {
    unsigned short id = GetAt<unsigned short>(data, offset + 2);
    unsigned short size = GetAt<unsigned short>(data, offset);
    string line;
    line += "{ " + CommandName(id) + ", { ";
    for (unsigned int i = 1; i < size; i++) {
        if (i != 1)
            line += ", ";
        unsigned int paramOffset = offset + i * 4;
        int paramValue = GetAt<unsigned int>(data, paramOffset);
        string paramLine;
        auto it = symbolRelocations.find(paramOffset);
        if (it != symbolRelocations.end()) {
            if ((*it).second.st_info == 0x10)
                paramLine = "(extern(" + (*it).second.name + ")";
            else
                paramLine = Format("@%X", paramValue);
        }
        else
            paramLine = Format(paramValue > 32'000 ? "0x%X" : "%d", paramValue);
        line += paramLine;
    }
    line += " } },";
    Writer::writeLine(line);
    return size * 4;
}

unsigned int WriteOffset(void *, string const &name, unsigned char *data, unsigned int offset) {
    string strValue;
    auto it = symbolRelocations.find(offset);
    if (it != symbolRelocations.end() && (*it).second.st_info == 0x10)
        strValue = (*it).second.name + " (extern)";
    else {
        auto value = GetAt<unsigned int>(data, offset);
        strValue = Format("0x%X", value);
        if (!value)
            strValue += " (null)";
    }
    Writer::writeField(name, strValue);
    return 4;
}

unsigned int WriteNameOffset(void *, string const &name, unsigned char *data, unsigned int offset) {
    auto value = GetAt<unsigned int>(data, offset);
    if (!value)
        Writer::writeField(name, "0x0 (null)");
    else {
        string strValue = string("\"") + (char *)(&data[value]) + "\"";
        strValue += Format(" @%X", value);
        Writer::writeField(name, strValue);
    }
    return 4;
}

unsigned int WriteName(void *, string const &name, unsigned char *data, unsigned int offset) {
    int len = strlen((char const *)data + offset) + 1;
    if (!Writer::mSpacing) {
        string line = string("\"") + ((char const *)(data)+offset) + "\", 00";
        Writer::writeLine(line + " @" + Format("%X", offset));
    }
    else
        Writer::writeField(name, string("\"") + ((char const *)data + offset) + "\"");
    return len;
}

unsigned int WriteNameAligned(void *, string const &name, unsigned char *data, unsigned int offset) {
    int len = strlen((char const *)data + offset) + 1;
    unsigned int padding = (-len) & 3;
    if (!Writer::mSpacing) {
        string line = string("\"") + ((char const *)data + offset) + "\", 00";
        for (unsigned int i = 0; i < padding; i++)
            line += " 00";
        Writer::writeLine(line + " @" + Format("%X", offset));
    }
    else
        Writer::writeField(name, string("\"") + ((char const *)data + offset) + "\"");
    return len + padding;
}

unsigned int WriteVector3(void *, string const &name, unsigned char *data, unsigned int offset) {
    Writer::writeField(name, Format("%.4f %.4f %.4f", GetAt<float>(data, offset), GetAt<float>(data, offset + 4), GetAt<float>(data, offset + 8)));
    return 12;
}

unsigned int WriteVector4(void *, string const &name, unsigned char *data, unsigned int offset) {
    Writer::writeField(name, Format("%.4f %.4f %.4f %.4f", GetAt<float>(data, offset), GetAt<float>(data, offset + 4), GetAt<float>(data, offset + 8), GetAt<float>(data, offset + 12)));
    return 16;
}

unsigned int WriteObject(void *baseObj, string const &type, string const &name, unsigned char *data, unsigned int offset, unsigned int count = 0) {
    if (count > 0)
        Writer::openScope(Format("Array[%d] of ", count) + type, offset); // TODO: write references
    unsigned int bytesWritten = 0;
    auto struc = GetStructs()[ToLower(type)];
    if (count > 0) {
        if (struc.mArrayWriter)
            bytesWritten = struc.mArrayWriter(baseObj, name, data, offset, count);
        else {
            if (struc.mWriter) {
                for (unsigned int i = 0; i < count; i++)
                    bytesWritten += struc.mWriter(baseObj, name + Format("[%d]", i), data, offset + bytesWritten);
            }
        }
    }
    else {
        if (struc.mWriter)
            bytesWritten = struc.mWriter(baseObj, name, data, offset);
    }
    if (count > 0)
        Writer::closeScope();
    return bytesWritten; // TODO: show error message
}

unsigned int WriteObjectWithFields(void *baseObj, string const &type, string const &name, unsigned char *data, unsigned int offset, vector<pair<string, string>> const &fields, unsigned int count = 0) {
    Writer::openScope(type + " " + name, offset); // TODO: write references
    unsigned int bytesWritten = 0;
    for (auto const &f : fields) {
        bytesWritten += WriteObject(baseObj, f.first, f.second, data, offset + bytesWritten, count);
    }
    Writer::closeScope();
    return bytesWritten;
}

unsigned int WriteMatrix4x4(void *baseObj, string const &name, unsigned char *data, unsigned int offset) {
    return WriteObjectWithFields(baseObj, "Matrix4x4", name, data, offset,
        {
            { "VECTOR4", "vec1" },
        { "VECTOR4", "vec2" },
        { "VECTOR4", "vec3" },
        { "VECTOR4", "posn" }
        });
}

unsigned int WriteBBOX(void *baseObj, string const &name, unsigned char *data, unsigned int offset) {
    return WriteObjectWithFields(baseObj, "BBOX", name, data, offset,
        {
         { "VECTOR3", "Min" },
        { "VECTOR3", "Max" }
        });
}

unsigned int WriteMorphVerticesInfo(void *baseObj, string const &name, unsigned char *data, unsigned int offset) {
    return WriteObjectWithFields(baseObj, "MorphVerticesInfo", name, data, offset,
        {
            { "UINT32", "unknown1" },
            { "NAMEOFFSET", "Name" }
        });
}

unsigned int WriteModel(void *baseObj, string const &name, unsigned char *data, unsigned int offset) {
    return WriteObjectWithFields(baseObj, "Model", name, data, offset,
        {
            { "OFFSET", "ModifiableDataDescriptors" },
        { "UINT32", "NumModifiableDataDescriptors" },
        { "UINT32", "NumVariations" },
        { "MATRIX4X4", "TransformationMatrix" },
        { "VECTOR4", "unknown1" },
        { "VECTOR4", "unknown2" },
        { "VECTOR4", "BoundingMin" },
        { "VECTOR4", "BoundingMax" },
        { "VECTOR4", "Center" },
        { "UINT32", "NumModelLayers" },
        { "OFFSET", "ModelLayersNames" },
        { "UINT32", "unknown3" },
        { "OFFSET", "unknown4" },
        { "OFFSET", "unknown5" },
        { "OFFSET", "NextModel" },
        { "NAMEOFFSET", "ModelName" },
        { "OFFSET", "MorphVerticesInfo" },
        { "OFFSET", "Textures" },
        { "UINT32", "VariationID" },
        { "OFFSET", "ModelLayersStates" },
        { "BOOL32", "IsModelRenderable" },
        { "OFFSET", "ModelLayers" },
        { "UINT32", "unknown6" },
        { "UINT32", "unknown7" },
        { "UINT32", "SkeletonVersion" },
        { "UINT32", "LastFrame" }
        });
}

unsigned int WriteRenderMethod(void *baseObj, string const &name, unsigned char *data, unsigned int offset) {
    return WriteObjectWithFields(baseObj, "RenderMethod", name, data, offset,
        {
            { "OFFSET", "CodeBlock" },
            { "OFFSET", "UsedCodeBlock" },
            { "OFFSET", "MicroCode" },
            { "OFFSET", "Effect" },
            { "OFFSET", "Parent" },
            { "OFFSET", "ParameterNames" },
            { "INT32", "unknown2" },
            { "INT32", "unknown3" },
            { "OFFSET", "unknown4" },
            { "OFFSET", "GeometryDataBuffer" },
            { "INT32", "ComputationIndexCommand" },
            { "NAMEOFFSET", "Name" },
            { "OFFSET", "EAGLModel" }
        });
}

unsigned int WriteModelTexture(void *baseObj, string const &name, unsigned char *data, unsigned int offset) {
    Writer::openScope("ModelTexture " + name, offset); // TODO: write references
    unsigned int bytesWritten = 0;
    char const *texName = GetAt<char const *>(data, offset + bytesWritten);
    bytesWritten += WriteObject(baseObj, "NAMEOFFSET", "Name", data, offset, 0);
    if (texName) {
        unsigned int tarCount = GetAt<unsigned int>(data, offset + bytesWritten);
        bytesWritten += WriteObject(baseObj, "UINT32", "TARCount", data, offset + bytesWritten, 0);
        if (tarCount > 0)
            bytesWritten += WriteObject(baseObj, "OFFSET", "Texture", data, offset + bytesWritten, tarCount);
        bytesWritten += WriteObject(baseObj, "UINT16", "unknown2", data, offset + bytesWritten, 0);
        bytesWritten += WriteObject(baseObj, "UINT16", "unknown3", data, offset + bytesWritten, 0);
    }
    Writer::closeScope();
    return bytesWritten;
}

unsigned int WriteModelTexture_OldFormat(void *baseObj, string const &name, unsigned char *data, unsigned int offset) {
    Writer::openScope("ModelTexture_OldFormat " + name, offset); // TODO: write references
    unsigned int bytesWritten = 0;
    char const *texName = GetAt<char const *>(data, offset + bytesWritten);
    bytesWritten += WriteObject(baseObj, "NAMEOFFSET", "Name", data, offset, 0);
    if (texName) {
        unsigned int tarCount = GetAt<unsigned int>(data, offset + bytesWritten);
        bytesWritten += WriteObject(baseObj, "UINT32", "TARCount", data, offset + bytesWritten, 0);
        if (tarCount > 0)
            bytesWritten += WriteObject(baseObj, "OFFSET", "Texture", data, offset + bytesWritten, tarCount);
    }
    Writer::closeScope();
    return bytesWritten;
}

unsigned int WriteBone(void *baseObj, string const &name, unsigned char *data, unsigned int offset) {
    return WriteObjectWithFields(baseObj, "Bone", name, data, offset,
        {
            { "UINT32", "Index" },
            { "UINT32", "unknown1" },
            { "UINT32", "unknown2" },
            { "UINT32", "unknown3" },
        });
}

unsigned int WriteModifiableData(void *baseObj, string const &name, unsigned char *data, unsigned int offset) {
    return WriteObjectWithFields(baseObj, "ModifiableData", name, data, offset,
        {
            { "NAMEOFFSET", "Name" },
            { "UINT16", "EntrySize" },
            { "UINT16", "unknown1" },
            { "UINT32", "EntriesCount" },
            { "OFFSET", "Entries" }
        });
}

unsigned int WriteModelLayerBounding(void *baseObj, string const &name, unsigned char *data, unsigned int offset) {
    return WriteObjectWithFields(baseObj, "ModelLayerBounding", name, data, offset,
        {
            { "VECTOR4", "Min" },
            { "VECTOR4", "Max" },
            { "VECTOR4", "Center" }
        });
}

unsigned int WriteGeoPrimState(void *baseObj, string const &name, unsigned char *data, unsigned int offset) {
    return WriteObjectWithFields(baseObj, "GeoPrimState", name, data, offset,
        {
            { "UINT32", "PrimitiveType" },
            { "UINT32", "Shading" },
            { "BOOL32", "CullingEnabled" },
            { "UINT32", "CullDirection" },
            { "UINT32", "DepthTestMethod" },
            { "UINT32", "AlphaBlendMode" },
            { "BOOL32", "AlphaTestEnable" },
            { "UINT32", "AlphaCompareValue" },
            { "UINT32", "AlphaTestMethod" },
            { "BOOL32", "TextureEnabled" },
            { "UINT32", "TransparencyMethod" },
            { "UINT32", "FillMode" },
            { "UINT32", "BlendOperation" },
            { "UINT32", "SourceBlend" },
            { "UINT32", "DestinationBlend" },
            { "FLOAT", "NumberOfPatchSegments" },
            { "INT32", "ZWritingEnabled" }
        });
}

unsigned int WriteComputationIndex(void *baseObj, string const &name, unsigned char *data, unsigned int offset) {
    return WriteObjectWithFields(baseObj, "ComputationIndex", name, data, offset,
        {
            { "UINT16", "ActiveTechnique" },
            { "UINT16", "unknown1" },
        });
}

unsigned int WriteIrradLight(void *baseObj, string const &name, unsigned char *data, unsigned int offset) {
    return WriteObjectWithFields(baseObj, "IrradLight", name, data, offset,
        {
            { "VECTOR4", "unknown1" },
            { "VECTOR4", "unknown2" },
            { "VECTOR4", "unknown3" },
            { "VECTOR4", "unknown4" },
            { "VECTOR4", "unknown5" },
            { "VECTOR4", "unknown6" },
            { "VECTOR4", "unknown7" },
            { "VECTOR4", "unknown8" },
            { "VECTOR4", "unknown9" },
            { "VECTOR4", "unknown10" }
        });
}

unsigned int WriteLight(void *baseObj, string const &name, unsigned char *data, unsigned int offset) {
    return WriteObjectWithFields(baseObj, "Light", name, data, offset,
        {
            { "MATRIX4X4", "unknown1" },
            { "VECTOR4", "unknown2" },
            { "VECTOR4", "unknown3" },
            { "VECTOR4", "unknown4" }
        });
}

unsigned int WriteModelLayerStates(void *baseObj, string const &name, unsigned char *data, unsigned int offset) {
    return WriteObjectWithFields(baseObj, "ModelLayerStates", name, data, offset,
        {
            { "UINT16", "unknown1" },
            { "UINT16", "Visibility" }
        });
}

unsigned int WriteAnimationBank(void *baseObj, string const &name, unsigned char *data, unsigned int offset) {
    return WriteObjectWithFields(baseObj, "AnimationBank", name, data, offset,
        {
            { "UINT32", "unknown1" },
            { "UINT32", "NumAnimations" },
            { "UINT32", "unknown2" },
            { "UINT32", "unknown3" },
            { "OFFSET", "Animations" },
            { "OFFSET", "AnimationNames" },
            { "UINT32", "unknown4" },
            { "UINT32", "unknown5" }
        });
}

unsigned int WriteBoneState(void *baseObj, string const &name, unsigned char *data, unsigned int offset) {
    return WriteObjectWithFields(baseObj, "BoneState", name, data, offset,
        {
            { "VECTOR3", "Scale" },
            { "INT32", "ParentBoneID" },
            { "VECTOR4", "RotationQuat" },
            { "VECTOR4", "Translation" },
            { "MATRIX4X4", "Transform" }
        });
}

unsigned int WriteCommandObjectParameter(void *baseObj, string const &name, unsigned char *data, unsigned int offset) {
    return WriteObjectWithFields(baseObj, "CommandObjectParameter", name, data, offset,
        {
            { "UINT32", "Count" },
            { "OFFSET", "Data" }
        });
}

unsigned int WriteTexture(void *baseObj, string const &name, unsigned char *data, unsigned int offset) {
    return WriteObjectWithFields(baseObj, "Texture", name, data, offset,
        {
             { "UINT32", "unknown1" },
             { "UINT32", "Tag" },
             { "UINT32", "unknown2" },
             { "FLOAT",  "unknown3" },
             { "UINT32", "unknown4" },
             { "FLOAT",  "unknown5" },
             { "UINT32", "unknown6" },
             { "UINT32", "WrapU" },
             { "UINT32", "WrapV" },
             { "UINT32", "WrapW" },
             { "UINT32", "unknown7" },
             { "UINT32", "unknown8" }
        });
}

unsigned int WriteGeometryInfo(void *baseObj, string const &name, unsigned char *data, unsigned int offset) {
    return WriteObjectWithFields(baseObj, "GeometryInfo", name, data, offset,
        {
            { "UINT32", "NumIndices" },
            { "UINT32", "NumVertices" },
            { "UINT32", "NumPrimitives" },
            { "BOOL32", "unknown1" }
        });
}

unsigned int WriteSkeleton(void *baseObj, string const &name, unsigned char *data, unsigned int offset) {
    Writer::openScope("Skeleton " + name, offset); // TODO: write references
    unsigned int bytesWritten = 0;
    unsigned int numBones = GetAt<unsigned int>(baseObj, 0x8);
    bytesWritten += WriteObject(baseObj, "UINT16", "Signature1", data, offset + bytesWritten, 0);
    bytesWritten += WriteObject(baseObj, "UINT16", "unknown1", data, offset + bytesWritten, 0);
    bytesWritten += WriteObject(baseObj, "UINT32", "Signature2", data, offset + bytesWritten, 0);
    bytesWritten += WriteObject(baseObj, "UINT32", "NumBones", data, offset + bytesWritten, 0);
    bytesWritten += WriteObject(baseObj, "OFFSET", "unknown2", data, offset + bytesWritten, 0);
    if (numBones)
        bytesWritten += WriteObject(baseObj, "BoneState", Format("BoneState", offset + bytesWritten), data, offset + bytesWritten, numBones);
    Writer::closeScope();
    return bytesWritten;
}

unsigned int WriteModelLayersStates(void *baseObj, string const &name, unsigned char *data, unsigned int offset) {
    Writer::openScope("ModelLayersStates " + name, offset); // TODO: write references
    unsigned int bytesWritten = 0;
    unsigned int numLayers = GetAt<unsigned int>(baseObj, 0x9C);
    bytesWritten += WriteObject(baseObj, "INT32", "unknown1", data, offset, 0);
    if (numLayers > 0)
        bytesWritten += WriteObject(baseObj, "ModelLayerStates", Format("ModelLayerStates", offset + bytesWritten), data, offset + bytesWritten, numLayers);
    Writer::closeScope();
    return bytesWritten;
}

unsigned int WriteModelLayer(void *baseObj, string const &name, unsigned char *data, unsigned int offset) {
    Writer::openScope("ModelLayer " + name, offset); // TODO: write references
    unsigned int bytesWritten = 0;
    bytesWritten += WriteObject(baseObj, "UINT32", "NumRenderDescriptors", data, offset, 0);
    unsigned int numRenderDescriptors = GetAt<unsigned int>(data, offset);
    bytesWritten += WriteObject(baseObj, "OFFSET", Format("RenderDescriptor", offset + bytesWritten), data, offset + bytesWritten, numRenderDescriptors);
    Writer::closeScope();
    return bytesWritten;
}

unsigned int WriteModelLayers(void *baseObj, string const &name, unsigned char *data, unsigned int offset) {
    Writer::openScope("ModelLayers " + name, offset); // TODO: write references
    unsigned int bytesWritten = 0;
    unsigned int numLayers = GetAt<unsigned int>(baseObj, 0x9C);
    bytesWritten += WriteObject(baseObj, "OFFSET", "unknown1", data, offset, 0);
    bytesWritten += WriteObject(baseObj, "ModelLayer", Format("ModelLayer", offset + bytesWritten), data, offset + bytesWritten, numLayers);
    Writer::closeScope();
    return bytesWritten;
}

unsigned int WriteEAGLMicroCode(void *baseObj, string const &name, unsigned char *data, unsigned int offset) {
    Writer::openScope("EAGLMicroCode " + name, offset); // TODO: write references
    unsigned int bytesWritten = 0;
    bytesWritten += WriteObject(baseObj, "UINT32", "MaxNumSelectedTechniques", data, offset + bytesWritten, 0);
    bytesWritten += WriteObject(baseObj, "UINT32", "unknown1", data, offset + bytesWritten, 0);
    unsigned int numVSDeclarations = GetAt<unsigned int>(baseObj, bytesWritten);
    bytesWritten += WriteObject(baseObj, "UINT32", "NumVertexShaderDeclarations", data, offset + bytesWritten, 0);
    if (numVSDeclarations)
        bytesWritten += WriteObject(baseObj, "VSDEC", "VertexShaderDeclaration", data, offset + bytesWritten, numVSDeclarations);
    bytesWritten += WriteObject(baseObj, "UINT32", "NumTechniques", data, offset + bytesWritten, 0);
    Writer::closeScope();
    return bytesWritten;
}

unsigned int WriteRenderCode(void *baseObj, string const &name, unsigned char *data, unsigned int offset) {
    unsigned int numCommands = 0;
    unsigned int commandOffset = 0;
    unsigned short id = GetAt<unsigned short>(data, offset + commandOffset + 2);
    unsigned short size = GetAt<unsigned short>(data, offset + commandOffset);
    while (id != 0) {
        //Message(Format("%d - %d", id, size));
        numCommands++;
        commandOffset += size * 4;
        id = GetAt<unsigned short>(data, offset + commandOffset + 2);
        size = GetAt<unsigned short>(data, offset + commandOffset);
    }
    unsigned int bytesWritten = 0;
    if (numCommands > 0)
        bytesWritten += WriteObject(baseObj, "COMMAND", "Command", data, offset + bytesWritten, numCommands);
    bytesWritten += WriteObject(baseObj, "UINT32", Format("RenderCodeFooter.%X", offset + bytesWritten), data, offset + bytesWritten, 0);
    return bytesWritten;
}

unsigned int WriteGeoPrimDataBuffer(void *baseObj, string const &name, unsigned char *data, unsigned int offset) {
    Writer::openScope("geoprimdatabuffer " + name, offset); // TODO: write references
    unsigned int bytesWritten = 0;
    bytesWritten += WriteObject(baseObj, "UINT32", "unknown1", data, offset + bytesWritten, 0);
    unsigned int rmCodeOffset = (unsigned char *)baseObj - data + 8;
    auto it = symbolRelocations.find(rmCodeOffset);
    if (it != symbolRelocations.end() && (*it).second.st_info == 0x10) {
        string codeName = (*it).second.name;
        if (codeName.ends_with("__EAGLMicroCode")) {
            unsigned int numTechniques = codeTechniques[codeName.substr(0, codeName.length() - 15)];
            if (numTechniques)
                bytesWritten += WriteObject(baseObj, "RenderCode", "RenderCode", data, offset + bytesWritten, options().onlyFirstTechnique ? 1 : numTechniques);
        }
    }
    Writer::closeScope();
    return bytesWritten;
}

unsigned int WriteRenderDescriptor(void *baseObj, string const &name, unsigned char *data, unsigned int offset) {
    Writer::openScope("RenderDescriptor " + name, offset); // TODO: write references
    void *renderMethod = At<void *>(data, GetAt<unsigned int>(data, offset));
    void *renderCode = At<void *>(data, GetAt<unsigned int>(renderMethod, 0));
    unsigned int numCommands = 0;
    unsigned int commandOffset = 0;
    unsigned short id = GetAt<unsigned short>(renderCode, commandOffset + 2);
    unsigned short size = GetAt<unsigned short>(renderCode, commandOffset);
    while (id != 0) {
        //Message(Format("%d - %d", id, size));
        numCommands++;
        commandOffset += size * 4;
        id = GetAt<unsigned short>(renderCode, commandOffset + 2);
        size = GetAt<unsigned short>(renderCode, commandOffset);
    }
    unsigned int bytesWritten = 0;
    bytesWritten += WriteObject(baseObj, "OFFSET", "RenderMethod", data, offset + bytesWritten, 0);
    bytesWritten += WriteObject(baseObj, "CommandObjectParameter", "CommandObjectParameter", data, offset + bytesWritten, (numCommands > 2) ? (numCommands - 2) : 1);
    Writer::closeScope();
    return bytesWritten;
}

unsigned int WriteVertexBuffer(void *baseObj, string const &name, unsigned char *data, unsigned int offset) {
    Writer::openScope("VertexBuffer " + name, offset); // TODO: write references
    unsigned int numVertices = GetAt<unsigned int>(baseObj, 28);
    unsigned int vertexSize = GetAt<unsigned int>(baseObj, 8);
    Writer::writeLine(to_string(numVertices) + " vertices (vertex stride " + to_string(vertexSize) + " bytes) [...]");
    Writer::closeScope();
    return numVertices * vertexSize;
}

unsigned int WriteIndexBuffer(void *baseObj, string const &name, unsigned char *data, unsigned int offset) {
    Writer::openScope("IndexBuffer " + name, offset); // TODO: write references
    unsigned int numIndices = GetAt<unsigned int>(baseObj, 20);
    Writer::writeLine(to_string(numIndices) + " indices [...]");
    Writer::closeScope();
    return numIndices * 2;
}

unsigned int WriteBoneWeightsBuffer(void *baseObj, string const &name, unsigned char *data, unsigned int offset) {
    Writer::openScope("BoneWeightsBuffer " + name, offset); // TODO: write references
    unsigned int numBoneWeights = GetAt<unsigned int>(baseObj, 0);
    struct boneweight { union boneref { float weight; unsigned char boneIndex; }; boneref bones[4]; };
    boneweight *weights = At<boneweight>(currentData, GetAt<unsigned int>(baseObj, 4));
    Writer::writeLine(to_string(numBoneWeights) + " bone weights [...]");
    for (unsigned int i = 0; i < numBoneWeights; i++) {
        string line;
        for (unsigned int b = 0; b < 4; b++) {
            float weight = weights[i].bones[b].weight;
            *(unsigned char *)(&weight) = 0;
            line += Format("bone %2d %.4f", weights[i].bones[b].boneIndex, weight);
            if (b != 3)
                line += "   ";
        }
        Writer::writeLine(line);
    }
    Writer::closeScope();
    return numBoneWeights * 16;
}

void InitAnalyzer() {
    GetStructs()["int8"] = WriteInt8;
    GetStructs()["uint8"] = WriteUInt8;
    GetStructs()["int16"] = WriteInt16;
    GetStructs()["uint16"] = WriteUInt16;
    GetStructs()["int32"] = WriteInt32;
    GetStructs()["uint32"] = WriteUInt32;
    GetStructs()["bool32"] = WriteBool32;
    GetStructs()["float"] = WriteFloat;
    GetStructs()["offset"] = WriteOffset;
    GetStructs()["nameoffset"] = WriteNameOffset;
    GetStructs()["name"] = WriteName;
    GetStructs()["namealigned"] = WriteNameAligned;
    GetStructs()["vector3"] = WriteVector3;
    GetStructs()["vector4"] = WriteVector4;
    GetStructs()["coordinate4"] = WriteVector4;
    GetStructs()["matrix4x4"] = WriteMatrix4x4;
    GetStructs()["bbox"] = WriteBBOX;
    GetStructs()["bone"] = WriteBone;
    GetStructs()["model"] = WriteModel;
    GetStructs()["rendermethod"] = WriteRenderMethod;
    GetStructs()["modeltexture"] = WriteModelTexture;
    GetStructs()["modeltexture_oldformat"] = WriteModelTexture_OldFormat;
    GetStructs()["modifiabledata"] = WriteModifiableData;
    GetStructs()["geoprimstate"] = WriteGeoPrimState;
    GetStructs()["computationindex"] = WriteComputationIndex;
    GetStructs()["irradlight"] = WriteIrradLight;
    GetStructs()["light"] = WriteLight;
    GetStructs()["modellayer"] = WriteModelLayer;
    GetStructs()["modellayers"] = WriteModelLayers;
    GetStructs()["modellayerstates"] = WriteModelLayerStates;
    GetStructs()["modellayersstates"] = WriteModelLayersStates;
    GetStructs()["morphverticesinfo"] = WriteMorphVerticesInfo;
    GetStructs()["modellayerbounding"] = WriteModelLayerBounding;
    GetStructs()["animationbank"] = WriteAnimationBank;
    GetStructs()["skeleton"] = WriteSkeleton;
    GetStructs()["bonestate"] = WriteBoneState;
    GetStructs()["eaglmicrocode"] = WriteEAGLMicroCode;
    GetStructs()["vsdecl"] = WriteVShaderDecl;
    GetStructs()["geoprimdatabuffer"] = WriteGeoPrimDataBuffer;
    GetStructs()["rendercode"] = WriteRenderCode;
    GetStructs()["command"] = WriteCommand;
    GetStructs()["renderdescriptor"] = WriteRenderDescriptor;
    GetStructs()["commandobjectparameter"] = WriteCommandObjectParameter;
    GetStructs()["geometryinfo"] = WriteGeometryInfo;
    GetStructs()["vertexbuffer"] = WriteVertexBuffer;
    GetStructs()["indexbuffer"] = WriteIndexBuffer;
    GetStructs()["texture"] = WriteTexture;
    GetStructs()["boneweightsbuffer"] = WriteBoneWeightsBuffer;

    codeTechniques = {
        { "PlayerIDShader", 9 },
        { "ClipTextureAlphablend", 9 },
        { "PitchLitMow", 9 },
        { "LitTextureIrradEnvmap_Skin", 3 },
        { "PostFX", 2 },
        { "Gouraud_Skin", 9 },
        { "LitTextureIrradEnvmap", 9 },
        { "LitTexture2IrradSkinSubSurfSpec", 9 },
        { "LitTextureIrradSpecMap_Skin", 9 },
        { "ClipTextureNodepthwriteWithAlpha", 9 },
        { "GouraudApt", 2 },
        { "LitGouraud", 2 },
        { "LitTexture2IrradSpecMap_Skin", 17 },
        { "LitTexture4Head_Skin", 3 },
        { "LitTexture2Hair_Skin", 18 },
        { "NewMethod2", 9 },
        { "PlanarShadow", 12 },
        { "ClipTextureModulateNodepthwrite", 9 },
        { "LitTexture2x_Skin", 9 },
        { "ClipTextureNoalphablend", 9 },
        { "LitTexture2x", 9 },
        { "Texture2x_Skin", 9 },
        { "BloomExtractHot", 2 },
        { "LitTextureEye_Skin", 9 },
        { "LitTexture2Alpha2x_Skin", 18 },
        { "BloomTint", 2 },
        { "PlanarShadow_Skin", 15 },
        { "DATexture", 2 },
        { "XFadeScrollTexture", 9 },
        { "LitTexture2IrradSkinSubSurfSpecGameface", 13 },
        { "NewMethod1", 9 },
        { "ClipTextureNodepthwrite", 9 },
        { "DAGouraud", 2 },
        { "TextureApt", 4 },
        { "ClipTextureNodepthwriteFade", 9 },
        { "Texture2x", 9 },
        { "LitTexture2Haircap_Skin", 18 },
        { "Gouraud", 2 },
        { "IrradLitTextureTransparent2x", 9 },
        { "IrradLitTextureColored2x", 6 },
        { "IrradLitTextureEnvmapMasked2x", 3 },
        { "XFadeFixTexture", 9 },
        { "XFadeDigitalScrollTexture", 3 },
        { "IrradLitTexture2x", 9 },
        { "IrradLitTextureColoured2x", 6 },
        { "ClipTextureAddNodepthwrite", 9 },
        { "IrradLitGouraud2x", 3 },
        { "IrradLitTextureColouredTransparent2x", 6 },
        { "IrradLitTextureColoredTransparent2x", 6 },
        { "IrradLitTextureEnvmapTransparent2x", 3 },
        { "XFadeTexture", 4 },
        { "FIFACrowda", 9 },
        { "FIFACrowdh", 9 },
    };
}

void AnalyzeFile(string const &filename, unsigned char *fileData, unsigned int fileDataSize, vector<Symbol> const &symbols, vector<Relocation> const &references) {
    Writer::mResult.clear();
    currentData = fileData;
    class Object {
    public:
        string mType;
        string mName;
        int mOffset = 0;
        unsigned int mCount = 0;
        void *mBaseObj = nullptr;

        Object() {}
        Object(string const &type, string const &name, int offset, unsigned int count, void *baseObj) {
            mType = type;
            mName = name;
            mOffset = offset;
            mCount = count;
            mBaseObj = baseObj;
        }
    };

    class Reference {

    };

    map<unsigned int, Object> objects;

    auto AddObjectInfo = [&](string const &type, string const &name, unsigned int offset, unsigned int count, void *baseObj) {
        if (objects.find(offset) == objects.end())
            objects[offset] = Object(type, name, offset, count, baseObj);
    };

    symbolRelocations.clear();

    for (auto const &r : references) {
        if (r.r_info_sym < symbols.size())
            symbolRelocations[r.r_offset] = symbols[r.r_info_sym];
    }

    for (auto const &s : symbols) {
        if (s.name.starts_with("__Model:::")) {
            void *model = At<void *>(fileData, s.st_value);
            AddObjectInfo("Model", s.name.substr(10), s.st_value, 0, model);
            // TODO: add symbol validation
            unsigned int texturesOffset = GetAt<unsigned int>(model, 0xBC);
            if (texturesOffset) { // TODO: replace with IsValidOffset()
                unsigned int numTextures = 0;
                unsigned int tmpTexOffset = texturesOffset;
                void *texDesc = At<void *>(fileData, tmpTexOffset);
                unsigned int texNameOffset = GetAt<unsigned int>(texDesc, 0);
                bool oldModelTextureFormat = false;
                while (texNameOffset) { // TODO: replace with IsValidOffset()
                    numTextures++;
                    unsigned int texCount = GetAt<unsigned int>(texDesc, 4);
                    unsigned int texSize = texCount * 4;
                    for (unsigned int tx = 0; tx < texCount; tx++) {
                        unsigned int texOffset = GetAt<unsigned int>(texDesc, 8 + tx * 4);
                        if (texOffset != 0) // TODO: replace with IsValidOffset()
                            AddObjectInfo("Texture", Format("Texture.%X", texOffset), texOffset, 0, model);
                    }
                    unsigned short info = GetAt<unsigned int>(texDesc, 8 + texSize);
                    AddObjectInfo("NAME", Format("Name.%X", texNameOffset), texNameOffset, 0, model);
                    if (info == 1)
                        tmpTexOffset += 12 + texSize;
                    else {
                        tmpTexOffset += 8 + texSize;
                        oldModelTextureFormat = true;
                    }
                    texDesc = At<void *>(fileData, tmpTexOffset);
                    texNameOffset = GetAt<unsigned int>(texDesc, 0);
                }
                AddObjectInfo("UINT32", Format("ModelTexturesFooter.%X", tmpTexOffset), tmpTexOffset, 0, model);
                if (numTextures > 0) {
                    if (oldModelTextureFormat)
                        AddObjectInfo("ModelTexture_OldFormat", Format("ModelTexture_OldFormat.%X", texturesOffset), texturesOffset, numTextures, model);
                    else
                        AddObjectInfo("ModelTexture", Format("ModelTexture.%X", texturesOffset), texturesOffset, numTextures, model);
                }
            }
            unsigned int numLayers = GetAt<unsigned int>(model, 0x9C);
            if (numLayers) {
                unsigned int layerNamesOffset = GetAt<unsigned int>(model, 0xA0);
                AddObjectInfo("NAMEOFFSET", Format("ModelLayerName.%X", layerNamesOffset), layerNamesOffset, numLayers, model);
                for (unsigned int i = 0; i < numLayers; i++)
                    AddObjectInfo("NAME", Format("Name.%X", GetAt<unsigned int>(fileData, layerNamesOffset + 4 * i)), GetAt<unsigned int>(fileData, layerNamesOffset + 4 * i), 0, model);
                unsigned int modelLayersOffset = GetAt<unsigned int>(model, 0xCC);
                AddObjectInfo("ModelLayers", Format("ModelLayers.%X", modelLayersOffset), modelLayersOffset, 0, model);
                void *modelLayers = At<void *>(fileData, modelLayersOffset);
                unsigned int *rd = At<unsigned int>(modelLayers, 4);
                for (unsigned int i = 0; i < numLayers; i++) {
                    unsigned int numRenderDescriptors = *rd;
                    rd++;
                    for (unsigned int d = 0; d < numRenderDescriptors; d++) {
                        unsigned int renderDescriptorOffset = *rd;
                        rd++;
                        void *renderDescriptor = At<void *>(fileData, renderDescriptorOffset);
                        AddObjectInfo("RenderDescriptor", Format("RenderDescriptor.%X", renderDescriptorOffset), renderDescriptorOffset, 0, model);
                        void *renderMethod = At<void *>(fileData, GetAt<unsigned int>(renderDescriptor, 0));
                        void *globalParameters = At<void *>(renderDescriptor, 4);
                        AddObjectInfo("GeometryInfo", Format("GeometryInfo.%X", GetAt<unsigned int>(globalParameters, 4)), GetAt<unsigned int>(globalParameters, 4), GetAt<unsigned int>(globalParameters, 0), model);
                        unsigned int rmCodeOffset = GetAt<unsigned int>(renderDescriptor, 0) + 8;
                        auto it = symbolRelocations.find(rmCodeOffset);
                        if (it != symbolRelocations.end() && (*it).second.st_info == 0x10) {
                            string codeName = (*it).second.name;
                            if (codeName.ends_with("__EAGLMicroCode")) {
                                unsigned int numTechniques = codeTechniques[codeName.substr(0, codeName.length() - 15)];
                                if (numTechniques) {
                                    void *renderCode = At<void *>(fileData, GetAt<unsigned int>(renderMethod, 0));
                                    unsigned int numCommands = 0;
                                    unsigned int commandOffset = 0;
                                    unsigned short id = GetAt<unsigned short>(renderCode, commandOffset + 2);
                                    unsigned short size = GetAt<unsigned short>(renderCode, commandOffset);
                                    while (id != 0) {
                                        switch (id) {
                                        case 4:
                                        case 75:
                                            AddObjectInfo("VertexBuffer", Format("VertexBuffer.%X", GetAt<unsigned int>(renderCode, commandOffset + 24)), GetAt<unsigned int>(renderCode, commandOffset + 24), 0, At<void *>(renderCode, commandOffset));
                                            break;
                                        case 7:
                                            AddObjectInfo("IndexBuffer", Format("IndexBuffer.%X", GetAt<unsigned int>(renderCode, commandOffset + 16)), GetAt<unsigned int>(renderCode, commandOffset + 16), 0, At<void *>(renderCode, commandOffset));
                                            break;
                                        case 28:
                                            AddObjectInfo("BoneWeightsBuffer", Format("BoneWeightsBuffer.%X", GetAt<unsigned int>(globalParameters, 4)), GetAt<unsigned int>(globalParameters, 4), 0, globalParameters);
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
                        }
                    }
                }
                AddObjectInfo("ModelLayerBounding", Format("ModelLayerBounding.%X", s.st_value - numLayers * 48), s.st_value - numLayers * 48, numLayers, model);
            }
            AddObjectInfo("ModelLayersStates", Format("ModelLayersStates.%X", GetAt<unsigned int>(model, 0xC4)), GetAt<unsigned int>(model, 0xC4), 0, model);
            AddObjectInfo("MorphVerticesInfo", Format("MorphVerticesInfo.%X", GetAt<unsigned int>(model, 0xB8)), GetAt<unsigned int>(model, 0xB8), 0, model);
            unsigned int nameOffset = GetAt<unsigned int>(model, 0xB4);
            if (nameOffset) // TODO: replace with IsValidOffset()
                AddObjectInfo("NAME", Format("Name.%X", nameOffset), nameOffset, 0, model);
            unsigned int numModifiableDatas = GetAt<unsigned int>(model, 0x04);
            if (numModifiableDatas) {
                AddObjectInfo("ModifiableData", Format("ModifiableData.%X", GetAt<unsigned int>(model, 0x0)), GetAt<unsigned int>(model, 0x0), numModifiableDatas, model);
                for (unsigned int i = 0; i < numModifiableDatas; i++) {
                    void *modData = At<void *>(fileData, GetAt<unsigned int>(model, 0x0) + 16 * i);
                    unsigned int numEntries = GetAt<unsigned int>(modData, 0x8);
                    if (numEntries) {
                        unsigned short entrySize = GetAt<unsigned short>(modData, 0x4);
                        char *name = At<char>(fileData, GetAt<unsigned int>(modData, 0x0));
                        AddObjectInfo("NAMEALIGNED", Format("Name.%X", GetAt<unsigned int>(modData, 0x0)), GetAt<unsigned int>(modData, 0x0), 0, model);
                        auto it = symbolRelocations.find(GetAt<unsigned int>(model, 0x0) + 16 * i + 0xC);
                        if (it == symbolRelocations.end() || (*it).second.st_info != 0x10) {
                            if (entrySize == 68)
                                AddObjectInfo("GeoPrimState", name + Format(".%X", GetAt<unsigned int>(modData, 0xC)), GetAt<unsigned int>(modData, 0xC), numEntries, model);
                            else if (entrySize == 4)
                                AddObjectInfo("ComputationIndex", name + Format(".%X", GetAt<unsigned int>(modData, 0xC)), GetAt<unsigned int>(modData, 0xC), numEntries, model);
                            else if (entrySize == 160)
                                AddObjectInfo("IrradLight", name + Format(".%X", GetAt<unsigned int>(modData, 0xC)), GetAt<unsigned int>(modData, 0xC), numEntries, model);
                            else if (entrySize == 112)
                                AddObjectInfo("Light", name + Format(".%X", GetAt<unsigned int>(modData, 0xC)), GetAt<unsigned int>(modData, 0xC), numEntries, model);
                            else if (entrySize == 16)
                                AddObjectInfo("Coordinate4", name + Format(".%X", GetAt<unsigned int>(modData, 0xC)), GetAt<unsigned int>(modData, 0xC), numEntries, model);
                        }
                    }
                }
            }
        }
        if (s.name.starts_with("__RenderMethod:::")) {
            void *renderMethod = At<void *>(fileData, s.st_value);
            AddObjectInfo("RenderMethod", s.name.substr(17), s.st_value, 0, renderMethod);
            unsigned int nameOffset = GetAt<unsigned int>(renderMethod, 0x2C);
            if (nameOffset) // TODO: replace with IsValidOffset()
                AddObjectInfo("NAMEALIGNED", Format("Name.%X", nameOffset), nameOffset, 0, renderMethod);
            unsigned int geoBuf = GetAt<unsigned int>(renderMethod, 0x24);
            if (geoBuf) // TODO: replace with IsValidOffset()
                AddObjectInfo("geoprimdatabuffer", Format("geoprimdatabuffer.%X", geoBuf), geoBuf, 0, renderMethod);
        }
        else if (s.name.starts_with("__BBOX:::"))
            AddObjectInfo("BBOX", s.name.substr(9), s.st_value, 0, nullptr);
        else if (s.name.starts_with("__Bone:::"))
            AddObjectInfo("Bone", s.name.substr(9), s.st_value, 0, nullptr);
        else if (s.name.starts_with("__AnimationBank:::")) {
            AddObjectInfo("AnimationBank", s.name.substr(18), s.st_value, 0, nullptr);
            void *bank = At<void *>(fileData, s.st_value);
            unsigned int numAnimations = GetAt<unsigned int>(bank, 0x4);
            if (numAnimations > 0) {
                unsigned int animNamesOffset = GetAt<unsigned int>(bank, 0x14);
                unsigned int animsOffset = GetAt<unsigned int>(bank, 0x10);
                AddObjectInfo("NAMEOFFSET", Format("AnimationName.%X", animNamesOffset), animNamesOffset, numAnimations, bank);
                AddObjectInfo("OFFSET", Format("Animation.%X", animsOffset), animsOffset, numAnimations, bank);
                for (unsigned int i = 0; i < numAnimations; i++)
                    AddObjectInfo((i == (numAnimations - 1)) ? "NAMEALIGNED" : "NAME", Format("Name.%X", GetAt<unsigned int>(fileData, animNamesOffset + 4 * i)), GetAt<unsigned int>(fileData, animNamesOffset + 4 * i), 0, bank);
            }
        }
        else if (s.name.starts_with("__Skeleton:::")) {
            void *skeleton = At<void *>(fileData, s.st_value);
            AddObjectInfo("Skeleton", s.name.substr(13), s.st_value, 0, skeleton);
        }
        else if (s.name.ends_with("__EAGLMicroCode") && s.st_info == 0x11) {
            void *microCode = At<void *>(fileData, s.st_value);
            AddObjectInfo("EAGLMicroCode", s.name.substr(0, s.name.length() - 15), s.st_value, 0, microCode);
        }
        //else if (s.name.starts_with("__geoprimdatabuffer")) {
        //    void *geoprimdatabuffer = At<void *>(fileData, s.st_value);
        //    AddObjectInfo("geoprimdatabuffer", s.name, s.st_value, 0, geoprimdatabuffer);
        //}
    }

    Writer::writeLine("// " + filename);

    int previousStructEnd = -1;

    auto WriteUnknown = [&](unsigned int endOffset) {
        if (previousStructEnd == -1)
            previousStructEnd = 0;
        unsigned int size = endOffset - previousStructEnd;
        Writer::openScope("Unknown [" + Format("%u", size) + "]", previousStructEnd); // TODO: write references
        string ary;
        for (unsigned int i = 0; i < min(size, 100u); i++) {
            if (i != 0)
                ary += " ";
            ary += Format("%02X", ((unsigned char *)fileData)[previousStructEnd + i]);
        }
        if (size > 100)
            ary += "...";
        Writer::writeLine(ary);
        Writer::closeScope();
        previousStructEnd += size;
    };

    for (auto const &[i, o] : objects) {
        if (previousStructEnd == -1)
            previousStructEnd = 0;
        if (o.mOffset != previousStructEnd)
            WriteUnknown(o.mOffset);
        previousStructEnd += WriteObject(o.mBaseObj, o.mType, o.mName, fileData, o.mOffset, o.mCount);
    }
    if (previousStructEnd != fileDataSize)
        WriteUnknown(fileDataSize);
}

}

GlobalVars &globalVars() {
    static GlobalVars gv;
    return gv;
}

void odump(path const &out, path const &in) {
    dump::InitAnalyzer();
    FILE *f = _wfopen(in.c_str(), L"rb");
    if (f) {
        fseek(f, 0, SEEK_END);
        unsigned int fileSize = ftell(f);
        fseek(f, 0, SEEK_SET);
        unsigned char *fileData = new unsigned char[fileSize];
        if (fread(fileData, 1, fileSize, f) == fileSize) {
            unsigned char *data = nullptr;
            unsigned int dataSize = 0;
            Elf32_Sym *symbols = nullptr;
            unsigned int numSymbols = 0;
            Elf32_Rel *rel = nullptr;
            unsigned int numRelocations = 0;
            char *symbolNames = nullptr;
            unsigned int symbolNamesSize = 0;

            Elf32_Ehdr *h = (Elf32_Ehdr *)fileData;
            Elf32_Shdr *s = At<Elf32_Shdr>(h, h->e_shoff);
            for (unsigned int i = 1; i < 6; i++) {
                if (s[i].sh_size > 0) {
                    if (s[i].sh_type == 1) {
                        data = At<unsigned char>(h, s[i].sh_offset);
                        dataSize = s[i].sh_size;
                    }
                    else if (s[i].sh_type == 2) {
                        symbols = At<Elf32_Sym>(h, s[i].sh_offset);
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

            vector<dump::Symbol> vecSymbols;
            vector<dump::Relocation> vecReferences;

            vecSymbols.resize(numSymbols);
            vecReferences.resize(numRelocations);

            for (unsigned int i = 0; i < numSymbols; i++) {
                vecSymbols[i].st_info = symbols[i].st_info;
                vecSymbols[i].st_name = symbols[i].st_name;
                vecSymbols[i].st_other = symbols[i].st_other;
                vecSymbols[i].st_shndx = symbols[i].st_shndx;
                vecSymbols[i].st_size = symbols[i].st_size;
                vecSymbols[i].st_value = symbols[i].st_value;
                vecSymbols[i].name = &symbolNames[symbols[i].st_name];
            }

            for (unsigned int i = 0; i < numRelocations; i++) {
                vecReferences[i].r_info_sym = rel[i].r_info_sym;
                vecReferences[i].r_info_type = rel[i].r_info_type;
                vecReferences[i].r_offset = rel[i].r_offset;
            }

            // TODO: check relocation types and symbols

            //Elf32_Rel *rel = (Elf32_Rel *)(&fileData[s[i].sh_offset]);
            //for (int ss = 0; ss < count; ss++) {
            //    if (rel->r_info_type != 2) {
            //        Error(L"rel.data info type (%d) is not 2 in elf %s", rel->r_info_type, fileNameForError.c_str());
            //    }
            //}
            AnalyzeFile(in.filename().string(), data, dataSize, vecSymbols, vecReferences);
        }
        //else
        //    Error(L"failed to read %s", inPath.c_str());
        delete[] fileData;
        fclose(f);
    }
    ofstream w(out, ios::out);
    if (w.is_open())
        w << dump::Writer::mResult;
}
