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
#include "target.h"
#include "Fsh/Fsh.h"

using namespace std;
using namespace std::filesystem;

struct GlobalOptions {
    bool processingFolders = false;
    // import options
    unsigned int hwnd = 0;
    float scale = 1.0f;
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
    bool genTexNames = false;
    bool writeFsh = false;
    string fshOutput;
    int fshLevels = 0;
    unsigned int fshFormat = 0;
    bool fshRescale = false;
    vector<string> fshTextures;
    vector<string> fshAddTextures;
    bool fshDisableTextureIgnore = false;
    set<string> fshIgnoreTextures;
    bool preTransformVertices = false;
    bool sortByName = false;
    bool sortByAlpha = false;
    bool ignoreMatColor = false;
    bool head = false;
    unsigned int pad = 0;
    unsigned int padFsh = 0;
    bool ignoreEmbeddedTextures = false;
    unsigned int instances = 0;
    int computationIndex = -1;
    bool keepTex0InMatOptions = false;
    // export options
    bool noTextures = false;
    bool dummyTextures = false;
    bool jpegTextures = false;
    bool noMeshJoin = false;
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
    TexEmbedded embedded;

    TextureToAdd();
    TextureToAdd(string const &_name, string const &_filepath, TexEmbedded const &_embedded = TexEmbedded());
};

struct GlobalVars {
    Target *target = nullptr;
    map<string, pair<unsigned char, string>> maxColorValue;
    map<string, map<vector<unsigned char>, vector<string>>> shaders;
    ea::FshImage::FileFormat fshUnpackImageFormat = ea::FshImage::PNG;
    map<path, map<string, TextureToAdd>> fshToBuild;
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
