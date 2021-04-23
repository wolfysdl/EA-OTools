#pragma once
#include <string>
#include <vector>
#include <set>
#include <map>
#include <filesystem>
#include "elf.h"
#include "memory.h"
#include "utils.h"
#include <assimp/color4.h>
#include <assimp/vector3.h>
#include <assimp/aabb.h>
#include "target.h"
#include "Fsh/Fsh.h"

using namespace std;
using namespace std::filesystem;

struct VColMergeLayerConfig {
    float bottomRange = 0.0f;
    float topRange = 1.0f;
};

struct BoneRemapTarget {
    string boneName;
    int boneIndex = -1;
    float factor = 1.0f;
    aiAABB bound;
};

struct BoneTargets {
    bool hasBounds = false;
    vector<BoneRemapTarget> targetBones;
};

struct GlobalOptions {
    bool processingFolders = false;
    bool stadium = false;
    bool srgb = false;
    bool fshForceAlphaCheck = false;
    // import options
    unsigned int hwnd = 0;
    bool conformant = false;
    aiVector3D scale = { 1.0f, 1.0f, 1.0f };
    bool scaleXYZ = false;
    aiVector3D translate = { 0.0f, 0.0f, 0.0f };
    bool tristrip = false;
    bool embeddedTextures = false;
    bool swapYZ = false;
    bool forceLighting = false;
    bool noMetadata = false;
    aiColor4D defaultVCol = { 0.5f, 0.5f, 0.5f, 1.0f };
    bool hasDefaultVCol = false;
    aiColor4D setVCol = { 0.0f, 0.0f, 0.0f, 1.0f };
    bool hasSetVCol = false;
    float vColScale = 0.0f;
    aiColor4D maxVCol = { 0.0f, 0.0f, 0.0f, 1.0f };
    bool hasMaxVCol = false;
    aiColor4D minVCol = { 0.0f, 0.0f, 0.0f, 1.0f };
    bool hasMinVCol = false;
    bool mergeVCols = false;
    map<unsigned int, VColMergeLayerConfig> vColMergeConfig;
    bool genTexNames = false;
    bool writeFsh = false;
    string fshOutput;
    int fshLevels = 0;
    bool hasFshFormat = false;
    unsigned int fshFormat = 0;
    bool fshRescale = false;
    vector<string> fshTextures;
    vector<string> fshAddTextures;
    bool fshDisableTextureIgnore = false;
    set<string> fshIgnoreTextures;
    bool fshUniqueHashForEachTexture = false;
    bool preTransformVertices = false;
    bool sortByName = false;
    bool sortByAlpha = false;
    bool useMatColor = false;
    bool head = false;
    bool hd = false;
    unsigned int pad = 0;
    unsigned int padFsh = 0;
    bool ignoreEmbeddedTextures = false;
    unsigned int instances = 0;
    int computationIndex = -1;
    bool keepTex0InMatOptions = false;
    string forceShader;
    path boneRemap;
    path skeletonData;
    path skeleton;
    path bonesFile;
    unsigned int maxBonesPerVertex = 0; // default
    unsigned int vertexWeightPaletteSize = 0; // default
    float bboxScale = 1.0f;
    unsigned int layerFlags = 0;
    unsigned int uid = 0;
    bool flipNormals = false;
    // export options
    bool noTextures = false;
    bool dummyTextures = false;
    bool jpegTextures = false;
    bool noMeshJoin = false;
    bool updateOldStadium = false;
    bool stadium07to10 = false;
    bool stadium10to07 = false;
    // dump options
    bool onlyFirstTechnique = false;
    // fsh unpack options
    string fshUnpackImageFormat = "png";
    // fsh pack options
    bool fshWriteToParentDir = false;
    bool fshBalls = false;
    bool fshKits = false;
    bool fshShoes = false;
    bool fshPatterns = false;
    unsigned int fshId = 1;
    unsigned int fshHash = 0;
    bool useFshHash = false;
    bool fshName = false;
};

GlobalOptions &options();

struct TexEmbedded {
    unsigned int width = 0;
    unsigned int height = 0;
    string format;
    void *data = nullptr;
};

struct TextureToAdd {
    string name;
    string filepath;
    unsigned int format;
    int levels;
    TexEmbedded embedded;

    TextureToAdd();
    TextureToAdd(string const &_name, string const &_filepath, unsigned int _format, int _levels, TexEmbedded const &_embedded = TexEmbedded());
};

struct GlobalVars {
    Target *target = nullptr;
    map<string, BoneTargets> boneRemap;
    map<string, unsigned char> customBones;
    map<string, pair<unsigned char, string>> maxColorValue;
    map<string, map<vector<unsigned char>, vector<string>>> shaders;
    ea::FshImage::FileFormat fshUnpackImageFormat = ea::FshImage::PNG;
    map<path, map<string, TextureToAdd>> fshToBuild;
    path currentFilePath;
};

GlobalVars &globalVars();

extern const char *OTOOLS_VERSION;

pair<unsigned char *, unsigned int> readofile(path const &inPath);

void WriteFsh(path const &fshFilePath, path const &searchDir, map<string, TextureToAdd> const &texturesToAdd);

void odump(path const &out, path const &in);
void oexport(path const &out, path const &in);
void oimport(path const &out, path const &in);
void oinfo(path const &out, path const &in);
void dumpshaders(path const &out, path const &in);
void packfsh_collect(path const &out, path const &in);
void unpackfsh(path const &out, path const &in);
void packfsh_pack();
void align_file(path const &out, path const &in);
