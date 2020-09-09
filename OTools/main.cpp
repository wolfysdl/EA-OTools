#include "d3dx9.h"
#include "main.h"
#include "commandline.h"
#include "message.h"
#include "Fsh/Fsh.h"

const char *OTOOLS_VERSION = "0.163";
const unsigned int OTOOLS_VERSION_INT = 163;

GlobalOptions &options() {
    static GlobalOptions go;
    return go;
}

enum ErrorType {
    NONE = 0,
    NOT_ENOUGHT_ARGUMENTS = 1,
    UNKNOWN_OPERATION_TYPE = 2,
    NO_INPUT_PATH = 3,
    INVALID_INPUT_PATH = 4,
    ERROR_OTHER = 5
};

int main(int argc, char *argv[]) {
    CommandLine cmd(argc, argv, { "i", "o", "game", "scale", "defaultVCol", "setVCol", "vColScale", "fshOutput", "fshLevels", "fshFormat", "fshTextures",
        "fshAddTextures", "fshIgnoreTextures", "startsWith", "pad", "padFsh", "instances", "computationIndex", "hwnd", "fshUnpackImageFormat",
        "forceShader", "fshHash", "fshId", "boneRemap", "skeletonData", "skeleton", "bonesFile", "maxBonesPerVertex", "vertexWeightPaletteSize",
        "translate", "minVCol", "maxVCol" },
        { "tristrip", "noTextures", "recursive", "createSubDir", "silent", "console", "onlyFirstTechnique", "dummyTextures", "jpegTextures", "embeddedTextures", 
        "swapYZ", "forceLighting", "noMetadata", "genTexNames", "writeFsh", "fshRescale", "fshDisableTextureIgnore", "preTransformVertices", "sortByName", 
        "sortByAlpha", "useMatColor", "noMeshJoin", "head", "hd", "ignoreEmbeddedTextures", "ord", "keepTex0InMatOptions", "fshWriteToParentDir",
        "conformant", "fshUniqueHashForEachTexture", "updateOldStadium", "stadium", "srgb", "fshForceAlphaCheck" });
    if (cmd.HasOption("silent"))
        SetMessageDisplayType(MessageDisplayType::MSG_NONE);
    else {
        if (cmd.HasOption("console"))
            SetMessageDisplayType(MessageDisplayType::MSG_CONSOLE);
        else
            SetMessageDisplayType(MessageDisplayType::MSG_MESSAGE_BOX);
    }
    enum OperationType {
        UNKNOWN, VERSION, DUMP, EXPORT, IMPORT, INFO, DUMPSHADERS, PACKFSH, UNPACKFSH
    } opType = OperationType::UNKNOWN;
    void (*callback)(path const &, path const &) = nullptr;
    bool isCustom = false;
    bool createDevice = false;
    string targetExt;
    set<string> inExt;
    if (argc >= 2) {
        string opTypeStr = argv[1];
        if (opTypeStr == "version") {
            opType = OperationType::VERSION;
        }
        else if (opTypeStr == "import") {
            opType = OperationType::IMPORT;
            callback = oimport;
            inExt = { ".gltf", ".glb", ".dae", ".fbx", ".obj", ".blend", ".3ds" };
            if (cmd.HasOption("ord"))
                targetExt = ".ord";
            else
                targetExt = ".o";
        }
        else if (opTypeStr == "export") {
            opType = OperationType::EXPORT;
            callback = oexport;
            inExt = { ".o", ".ord" };
            targetExt = ".gltf";
        }
        else if (opTypeStr == "dump") {
            opType = OperationType::DUMP;
            callback = odump;
            inExt = { ".o", ".ord" };
            targetExt = ".txt";
        }
        else if (opTypeStr == "info") {
            opType = OperationType::INFO;
            callback = oinfo;
            inExt = { ".o", ".ord" };
        }
        else if (opTypeStr == "dumpshaders") {
            opType = OperationType::DUMPSHADERS;
            callback = dumpshaders;
            isCustom = true;
        }
        else if (opTypeStr == "unpackfsh" || opTypeStr == "extractfsh") {
            opType = OperationType::UNPACKFSH;
            callback = unpackfsh;
            inExt = { ".fsh" };
            createDevice = true;
        }
        else if (opTypeStr == "packfsh" || opTypeStr == "buildfsh" || opTypeStr == "makefsh") {
            opType = OperationType::PACKFSH;
            callback = packfsh_collect;
            inExt = { ".png", ".jpg", ".jpeg", ".bmp", ".tga", ".dds" };
            createDevice = true;
        }
    }
    if (opType == OperationType::UNKNOWN) {
        ErrorMessage("Unknown operation type\nPlease use OTools_GUI if you don't understand how to work with command-line tool");
        return ErrorType::UNKNOWN_OPERATION_TYPE;
    }
    if (opType == OperationType::VERSION) {
        InfoMessage(string("OTools version: ") + OTOOLS_VERSION);
        return OTOOLS_VERSION_INT;
    }
    if (!cmd.HasArgument("i")) {
        ErrorMessage("Input path is not specified");
        return ErrorType::NO_INPUT_PATH;
    }
    path i = cmd.GetArgumentString("i");
    if (!exists(i)) {
        ErrorMessage("Input path does not exist");
        return ErrorType::INVALID_INPUT_PATH;
    }
    if (cmd.HasArgument("hwnd"))
        options().hwnd = cmd.GetArgumentInt("hwnd");
    string game = ToLower(cmd.GetArgumentString("game"));
    if (game == "fm") {
        static TargetFM13 defaultFMTarget;
        globalVars().target = &defaultFMTarget;
    }
    else if (game == "fifa") {
        static TargetFIFA10 defaultFIFATarget;
        globalVars().target = &defaultFIFATarget;
    }
    else if (game == "cricket") {
        static TargetCRICKET07 defaultCricketTarget;
        globalVars().target = &defaultCricketTarget;
    }
    else if (game == "nhl") {
        static TargetNHL04 defaultNHLTarget;
        globalVars().target = &defaultNHLTarget;
    }
    else if (game == "fm13") {
        static TargetFM13 targetFM13;
        globalVars().target = &targetFM13;
    }
    else if (game == "fifa10") {
        static TargetFIFA10 targetFIFA10;
        globalVars().target = &targetFIFA10;
    }
    else if (game == "fifa09") {
        static TargetFIFA09 targetFIFA09;
        globalVars().target = &targetFIFA09;
    }
    else if (game == "fifa08") {
        static TargetFIFA08 targetFIFA08;
        globalVars().target = &targetFIFA08;
    }
    else if (game == "fifa07") {
        static TargetFIFA07 targetFIFA07;
        globalVars().target = &targetFIFA07;
    }
    else if (game == "fifa06") {
        static TargetFIFA06 targetFIFA06;
        globalVars().target = &targetFIFA06;
    }
    else if (game == "fifa05" || game == "fifa2005") {
        static TargetFIFA05 targetFIFA05;
        globalVars().target = &targetFIFA05;
    }
    else if (game == "fifa04" || game == "fifa2004") {
        static TargetFIFA04 targetFIFA04;
        globalVars().target = &targetFIFA04;
    }
    else if (game == "fifa03" || game == "fifa2003") {
        static TargetFIFA03 targetFIFA03;
        globalVars().target = &targetFIFA03;
    }
    else if (game == "cl0607" || game == "cl07" || game == "uefacl0607" || game == "uefacl07") {
        static TargetCL0607 targetCL0607;
        globalVars().target = &targetCL0607;
    }
    else if (game == "cl0405" || game == "cl05" || game == "uefacl0405" || game == "uefacl05") {
        static TargetCL0405 targetCL0405;
        globalVars().target = &targetCL0405;
    }
    else if (game == "wc06" || game == "fifawc06" || game == "wc2006" || game == "fifawc2006") {
        static TargetWC06 targetWC06;
        globalVars().target = &targetWC06;
    }
    else if (game == "euro08" || game == "uefaeuro08" || game == "euro2008" || game == "uefaeuro2008") {
        static TargetEURO08 targetEURO08;
        globalVars().target = &targetEURO08;
    }
    else if (game == "euro04" || game == "uefaeuro04" || game == "euro2004" || game == "uefaeuro2004") {
        static TargetEURO04 targetEURO04;
        globalVars().target = &targetEURO04;
    }
    else if (game == "cricket07") {
        static TargetCRICKET07 targetCRICKET07;
        globalVars().target = &targetCRICKET07;
    }
    else if (game == "nhl04" || game == "nhl2004") {
        static TargetNHL04 targetNHL04;
        globalVars().target = &targetNHL04;
    }
    else {
        static TargetFM13 defaultTarget;
        globalVars().target = &defaultTarget;
    }
    if (opType == OperationType::EXPORT) {
        if (cmd.HasOption("noTextures"))
            options().noTextures = true;
        if (cmd.HasOption("dummyTextures"))
            options().dummyTextures = true;
        if (cmd.HasOption("jpegTextures"))
            options().jpegTextures = true;
        if (cmd.HasOption("noMeshJoin"))
            options().noMeshJoin = true;
        if (cmd.HasOption("keepTex0InMatOptions"))
            options().keepTex0InMatOptions = true;
        if (cmd.HasArgument("skeleton"))
            options().skeleton = cmd.GetArgumentString("skeleton");
        if (cmd.HasOption("updateOldStadium"))
            options().updateOldStadium = true;
    }
    else if (opType == OperationType::IMPORT) {
        if (cmd.HasOption("conformant"))
            options().conformant = true;
        if (cmd.HasOption("noMetadata"))
            options().noMetadata = true;
        if (cmd.HasArgument("scale"))
            options().scale = cmd.GetArgumentFloat("scale");
        if (cmd.HasArgument("translate")) {
            auto translateLine = cmd.GetArgumentString("translate");
            auto translateParts = Split(translateLine, ',');
            if (translateParts.size() == 3) {
                options().translate.x = SafeConvertFloat(translateParts[0]);
                options().translate.y = SafeConvertFloat(translateParts[1]);
                options().translate.z = SafeConvertFloat(translateParts[2]);
            }
        }
        if (cmd.HasOption("tristrip"))
            options().tristrip = true;
        if (cmd.HasOption("embeddedTextures"))
            options().embeddedTextures = true;
        if (cmd.HasOption("swapYZ"))
            options().swapYZ = true;
        if (cmd.HasOption("forceLighting"))
            options().forceLighting = true;
        auto readVCol = [&](string const &argName, aiColor4D &outCol, bool &outHas) {
            if (cmd.HasArgument(argName)) {
                string vcol = cmd.GetArgumentString(argName);
                unsigned int r = 255, g = 255, b = 255, a = 255;
                if (vcol.length() == 6)
                    outHas = (sscanf(vcol.c_str(), "%2X%2X%2X", &r, &g, &b) == 3);
                else if (vcol.length() == 8)
                    outHas = (sscanf(vcol.c_str(), "%2X%2X%2X%2X", &r, &g, &b, &a) == 4);
                if (outHas) {
                    outCol.r = float(r) / 255.0f;
                    outCol.g = float(g) / 255.0f;
                    outCol.b = float(b) / 255.0f;
                    outCol.a = float(a) / 255.0f;
                }
            }
        };
        readVCol("setVCol", options().setVCol, options().hasSetVCol);
        if (!options().hasSetVCol) {
            readVCol("defaultVCol", options().defaultVCol, options().hasDefaultVCol);
            readVCol("minVCol", options().minVCol, options().hasMinVCol);
            readVCol("maxVCol", options().maxVCol, options().hasMaxVCol);
            if (cmd.HasArgument("vColScale"))
                options().vColScale = cmd.GetArgumentFloat("vColScale");
        }
        if (cmd.HasOption("genTexNames"))
            options().genTexNames = true;
        if (cmd.HasOption("preTransformVertices"))
            options().preTransformVertices = true;
        if (cmd.HasOption("sortByName"))
            options().sortByName = true;
        if (cmd.HasOption("sortByAlpha"))
            options().sortByAlpha = true;
        if (cmd.HasOption("useMatColor"))
            options().useMatColor = true;
        if (cmd.HasOption("head"))
            options().head = true;
        if (cmd.HasOption("hd"))
            options().hd = true;
        if (cmd.HasArgument("pad"))
            options().pad = cmd.GetArgumentInt("pad");
        if (cmd.HasOption("ignoreEmbeddedTextures"))
            options().ignoreEmbeddedTextures = true;
        if (cmd.HasArgument("instances"))
            options().instances = cmd.GetArgumentInt("instances");
        if (cmd.HasArgument("computationIndex"))
            options().computationIndex = cmd.GetArgumentInt("computationIndex");
        if (cmd.HasArgument("forceShader"))
            options().forceShader = cmd.GetArgumentString("forceShader");
        if (cmd.HasArgument("boneRemap"))
            options().boneRemap = cmd.GetArgumentString("boneRemap");
        if (cmd.HasArgument("skeletonData"))
            options().skeletonData = cmd.GetArgumentString("skeletonData");
        if (cmd.HasArgument("bonesFile"))
            options().bonesFile = cmd.GetArgumentString("bonesFile");
        if (cmd.HasArgument("maxBonesPerVertex"))
            options().maxBonesPerVertex = cmd.GetArgumentInt("maxBonesPerVertex");
        if (cmd.HasArgument("vertexWeightPaletteSize"))
            options().vertexWeightPaletteSize = cmd.GetArgumentInt("vertexWeightPaletteSize");
    }
    else if (opType == OperationType::DUMP) {
        if (cmd.HasOption("onlyFirstTechnique"))
            options().onlyFirstTechnique = true;
    }
    else if (opType == OperationType::PACKFSH) {
        if (cmd.HasOption("fshWriteToParentDir"))
            options().fshWriteToParentDir = true;
        if (cmd.HasOption("fshBalls"))
            options().fshBalls = true;
        if (cmd.HasOption("fshKits"))
            options().fshKits = true;
        if (cmd.HasOption("fshShoes"))
            options().fshShoes = true;
        if (cmd.HasOption("fshPatterns"))
            options().fshPatterns = true;
    }
    else if (opType == OperationType::UNPACKFSH) {
        if (cmd.HasArgument("fshUnpackImageFormat")) {
            auto imFormat = cmd.GetArgumentString("fshUnpackImageFormat");
            if (imFormat.starts_with('.'))
                imFormat = imFormat.substr(1);
            if (!imFormat.empty()) {
                imFormat = ToLower(imFormat);
                if (imFormat == "png") {
                    options().fshUnpackImageFormat = imFormat;
                    globalVars().fshUnpackImageFormat = ea::FshImage::PNG;
                }
                else if (imFormat == "bmp") {
                    options().fshUnpackImageFormat = imFormat;
                    globalVars().fshUnpackImageFormat = ea::FshImage::BMP;
                }
                else if (imFormat == "tga") {
                    options().fshUnpackImageFormat = imFormat;
                    globalVars().fshUnpackImageFormat = ea::FshImage::TGA;
                }
                else if (imFormat == "dds") {
                    options().fshUnpackImageFormat = imFormat;
                    globalVars().fshUnpackImageFormat = ea::FshImage::DDS;
                }
                else if (imFormat == "jpg") {
                    options().fshUnpackImageFormat = imFormat;
                    globalVars().fshUnpackImageFormat = ea::FshImage::JPG;
                }
            }
        }
    }
    if (opType == PACKFSH || (opType == IMPORT && cmd.HasOption("writeFsh"))) {
        options().writeFsh = true;
        if (cmd.HasArgument("fshOutput"))
            options().fshOutput = cmd.GetArgumentString("fshOutput");
        createDevice = true;
        options().fshLevels = D3DX_DEFAULT;
        if (cmd.HasArgument("fshLevels")) {
            options().fshLevels = cmd.GetArgumentInt("fshLevels");
            if (options().fshLevels == -1 || options().fshLevels == 0)
                options().fshLevels = D3DX_FROM_FILE;
            else if (options().fshLevels < -1 || options().fshLevels > 13)
                options().fshLevels = D3DX_DEFAULT;
        }
        options().fshFormat = unsigned int(-4);
        if (cmd.HasArgument("fshFormat")) {
            string format = ToLower(cmd.GetArgumentString("fshFormat"));
            if (!format.empty()) {
                if (format == "dxt")
                    options().fshFormat = unsigned int(-4);
                else if (format == "rgb" || format == "rgba" || format == "rgb32")
                    options().fshFormat = unsigned int(-5);
                else if (format == "rgb16" || format == "rgba16")
                    options().fshFormat = unsigned int(-6);
                else if (format == "auto")
                    options().fshFormat = unsigned int(-3);
                else if (format == "dxt1")
                    options().fshFormat = D3DFMT_DXT1;
                else if (format == "dxt3")
                    options().fshFormat = D3DFMT_DXT3;
                else if (format == "dxt5")
                    options().fshFormat = D3DFMT_DXT5;
                else if (format == "8888")
                    options().fshFormat = D3DFMT_A8R8G8B8;
                else if (format == "888")
                    options().fshFormat = D3DFMT_R8G8B8;
                else if (format == "4444")
                    options().fshFormat = D3DFMT_A4R4G4B4;
                else if (format == "5551")
                    options().fshFormat = D3DFMT_A1R5G5B5;
                else if (format == "565")
                    options().fshFormat = D3DFMT_R5G6B5;
            }
        }
        if (cmd.HasOption("fshRescale"))
            options().fshRescale = true;
        if (cmd.HasArgument("fshTextures"))
            options().fshTextures = Split(cmd.GetArgumentString("fshTextures"), ',', true, true);
        if (cmd.HasArgument("fshAddTextures"))
            options().fshAddTextures = Split(cmd.GetArgumentString("fshAddTextures"), ',', true, true);
        if (cmd.HasOption("fshDisableTextureIgnore"))
            options().fshDisableTextureIgnore = true;
        else {
            if (cmd.HasArgument("fshIgnoreTextures")) {
                auto ignoredTexturesList = Split(cmd.GetArgumentString("fshIgnoreTextures"), ',', true, true);
                if (!ignoredTexturesList.empty()) {
                    for (auto const &it : ignoredTexturesList)
                        options().fshIgnoreTextures.insert(ToLower(it));
                }
            }
        }
        if (cmd.HasArgument("padFsh"))
            options().padFsh = cmd.GetArgumentInt("padFsh");
        if (cmd.HasArgument("fshHash")) {
            options().fshHash = cmd.GetArgumentInt("fshHash");
            options().useFshHash = true;
        }
        if (cmd.HasArgument("fshId"))
            options().fshId = cmd.GetArgumentInt("fshId");
        if (cmd.HasOption("fshUniqueHashForEachTexture"))
            options().fshUniqueHashForEachTexture = true;
        if (cmd.HasOption("fshForceAlphaCheck"))
            options().fshForceAlphaCheck = true;
    }
    if (opType == EXPORT || opType == IMPORT) {
        if (cmd.HasOption("stadium"))
            options().stadium = true;
        if (cmd.HasOption("srgb"))
            options().srgb = true;
    }

    path o;
    bool hasOutput = cmd.HasArgument("o");
    if (hasOutput)
        o = cmd.GetArgumentString("o");
    bool createSubDir = cmd.HasOption("createSubDir");
    string startsWith;
    if (cmd.HasArgument("startsWith"))
        startsWith = cmd.GetArgumentString("startsWith");
    if (createDevice) {
        static D3DDevice device(options().hwnd ? options().hwnd : unsigned int(GetConsoleWindow()));
        ea::Fsh::SetDevice(&device);
    }

    auto errCode = ErrorType::NONE;
    
    if (!isCustom) {
        auto processFile = [&](path const &in, bool inDir) {
            try {
                if (!inDir || (is_regular_file(in) && inExt.contains(ToLower(in.extension().string())))) {
                    if (startsWith.empty() || in.filename().string().starts_with(startsWith)) {
                        path out;
                        if (hasOutput)
                            out = o;
                        else
                            out = in.parent_path();
                        if (!hasOutput || inDir) {
                            string targetFileName = in.stem().string();
                            string targetFileNameWithExt = targetFileName + targetExt;
                            if (createSubDir)
                                out = out / targetFileName / targetFileNameWithExt;
                            else
                                out = out / targetFileNameWithExt;
                        }
                        create_directories(out.parent_path());
                        globalVars().currentFilePath = in;
                        callback(out, in);
                    }
                }
            }
            catch (exception & e) {
                ErrorMessage(in.filename().string() + ": " + e.what());
                errCode = ErrorType::ERROR_OTHER;
            }
        };

        if (is_directory(i)) {
            options().processingFolders = true;
            if (cmd.HasOption("recursive")) {
                for (auto const &p : recursive_directory_iterator(i))
                    processFile(p.path(), true);
            }
            else {
                for (auto const &p : directory_iterator(i))
                    processFile(p.path(), true);
            }
        }
        else
            processFile(i, false);
    }
    else {
        globalVars().currentFilePath = i;
        callback(o, i);
    }

    if (opType == PACKFSH)
        packfsh_pack();

    if (createDevice)
        ea::Fsh::ClearDevice();

    return errCode;
}

pair<unsigned char *, unsigned int> readofile(path const &inPath) {
    pair<unsigned char *, unsigned int> result = { nullptr, 0 };
    if (ToLower(inPath.extension().string()) != ".ord") {
        FILE *f = _wfopen(inPath.c_str(), L"rb");
        if (f) {
            fseek(f, 0, SEEK_END);
            unsigned int fileSize = ftell(f);
            fseek(f, 0, SEEK_SET);
            result.first = new unsigned char[fileSize];
            if (fread(result.first, 1, fileSize, f) != fileSize) {
                delete[] result.first;
                result.first = nullptr;
            }
            else
                result.second = fileSize;
            fclose(f);
        }
    }
    else {
        auto orlPath = inPath;
        orlPath.replace_extension(".orl");
        if (exists(orlPath) && is_regular_file(orlPath)) {
            FILE *ordFile = _wfopen(inPath.c_str(), L"rb");
            FILE *orlFile = _wfopen(orlPath.c_str(), L"rb");
            if (ordFile && orlFile) {
                fseek(ordFile, 0, SEEK_END);
                unsigned int ordFileSize = ftell(ordFile);
                fseek(ordFile, 0, SEEK_SET);
                fseek(orlFile, 0, SEEK_END);
                unsigned int orlFileSize = ftell(orlFile);
                fseek(orlFile, 0, SEEK_SET);
                result.first = new unsigned char[ordFileSize + orlFileSize];
                if (fread(result.first, 1, ordFileSize, ordFile) != ordFileSize || fread(result.first + ordFileSize, 1, orlFileSize, orlFile) != orlFileSize) {
                    delete[] result.first;
                    result.first = nullptr;
                }
                else
                    result.second = ordFileSize + orlFileSize;
            }
            if (ordFile)
                fclose(ordFile);
            if (orlFile)
                fclose(orlFile);
        }
    }
    return result;
}
