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

using namespace std;
using namespace std::filesystem;

struct GlobalOptions {
    bool processingFolders = false;
    // import options
    bool tristrip = false;
    bool embeddedTextures = false;
    bool swapYZ = false;
    bool forceLighting = false;
    bool noMetadata = false;
    aiColor4D defaultVCol = { 0.5f, 0.5f, 0.5f, 1.0f };
    bool hasDefaultVCol = false;
    float vColScale = 0.0f;
    bool genTexNames = false;
    bool writeFsh = false;
    string fshOutput;
    int fshLevels = 0;
    unsigned int fshFormat = 0;
    bool fshRescale = false;
    // export options
    bool noTextures = false;
    bool dummyTextures = false;
    bool jpegTextures = false;
    // dump options
    bool onlyFirstTechnique = false;
};

GlobalOptions &options();

struct GlobalVars {
    map<string, pair<unsigned char, string>> maxColorValue;
};

GlobalVars &globalVars();

extern const char *OTOOLS_VERSION;

void odump(path const &out, path const &in);
void oexport(path const &out, path const &in);
void oimport(path const &out, path const &in);
void oinfo(path const &out, path const &in);
