#pragma once
#include <string>
#include <vector>

enum ShaderCommand {
    NOP_1 = 1,
    SET_VERTEX_SHADER_CONSTANT_G = 2,
    NOP_3 = 3,
    SET_STREAM_SOURCE = 4,
    NOP_6 = 6,
    SET_INDEX_BUFFER = 7,
    SET_SAMPLER_G = 9,
    DRAW_PRIM = 16,
    DRAW_INDEXED_PRIM_NO_Z_WRITE = 17,
    DRAW_INDEXED_PRIM = 18,
    SET_VERTEX_BONE_WEIGHTS = 28,
    SET_VERTEX_SHADER_CONSTANT_L_30 = 30,
    SET_VERTEX_SHADER_CONSTANT_L_31 = 31,
    SET_SAMPLER = 32,
    SET_GEO_PRIM_STATE = 33,
    SET_VERTEX_SHADER_TRANSPOSED_MATRIX = 35,
    SET_ANIMATION_BUFFER = 40,
    SET_VERTEX_SHADER_CONSTANT_L_46 = 46,
    DRAW_INDEXED_PRIM_AND_END = 65,
    SETUP_RENDER = 69,
    SET_PIXEL_SHADER_CONTANT_G_72 = 72,
    SET_PIXEL_SHADER_CONTANT_G_73 = 73,
    SET_STREAM_SOURCE_SKINNED = 75
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

        unsigned int Size() const;
    };

    enum Argument {
        VertexData = 0x70000001,
        VertexCount,
        IndexData,
        IndexCount,
        VertexSkinData,
        GeometryInfo,
        ModelMatrix,
        ViewMatrix,
        ProjectionMatrix,
        ModelViewProjectionMatrix,
        ModelViewMatrix,
        FogParameters0,
        FogParameters1,
        FogParameters2,
        FogParameters3,
        Sampler0,
        Sampler1,
        Sampler2,
        GeoPrimState,
        RuntimeGeoPrimState,
        RuntimeGeoPrimState2,
        ComputationIndex,
        Light,
        IrradLight,
        ZeroOneTwoThree,
        ZeroOneTwoThreeLocal,
        EnvMapConstants,
        EnvMapConstantsLocal,
        EnvmapColour,
        FogParameters,
        UVOffset0,
        UVOffset1,
        XFade,
        BaseColour,
        ShadowColour,
        ShadowColour2,
        RMGrass_PSConstants,
        RMGrass_VSConstants,
        RMGrass_CameraPosition,
        EAGLAnimationBuffer,
        ViewVector,
        RimLightCol
    };

    struct Command {
        unsigned short id;
        std::vector<int> arguments;

        Command(unsigned char _id, std::vector<int> const &_arguments);
    };

    struct GlobalArgument {
        unsigned int type;
        char const *format;

        GlobalArgument(unsigned int _type, char const *_format = nullptr);
    };

    std::string name;
    std::string nameLowered;
    unsigned int numTechniques;
    int computationCommandIndex;
    std::vector<VertexDeclElement> declaration;
    unsigned int vertexSize;
    std::vector<Command> commands;
    std::vector<GlobalArgument> globalArguments;

    Shader();
    Shader(char const *_name, unsigned int _numTechniques, std::vector<VertexDeclElement> const &_declaration, std::vector<Command> _commands, std::vector<GlobalArgument> _globalArguments);
    unsigned int VertexSize() const;
    int ComputationCommandIndex() const;
    bool HasAttribute(DataUsage attribute) const;
};

extern Shader *Shaders;
extern unsigned int NumShaders;
Shader *FindShader(std::string const &name);
