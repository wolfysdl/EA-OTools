#include "d3dx9.h"
#include "main.h"
#include "commandline.h"
#include "errormsg.h"
#include "Fsh/Fsh.h"

const char *OTOOLS_VERSION = "0.149";

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
        "fshAddTextures", "fshIgnoreTextures", "startsWith", "pad", "padFsh", "instances", "computationIndex", "hwnd" },
        { "tristrip", "noTextures", "recursive", "createSubDir", "silent", "console", "onlyFirstTechnique", "dummyTextures", "jpegTextures", "embeddedTextures", 
        "swapYZ", "forceLighting", "noMetadata", "genTexNames", "writeFsh", "fshRescale", "fshDisableTextureIgnore", "preTransformVertices", "sortByName", 
        "sortByAlpha", "ignoreMatColor", "noMeshJoin", "head", "ignoreEmbeddedTextures", "ord", "gl20" });
    if (cmd.HasOption("silent"))
        SetErrorDisplayType(ErrorDisplayType::ERR_NONE);
    else {
        if (cmd.HasOption("console"))
            SetErrorDisplayType(ErrorDisplayType::ERR_CONSOLE);
        else
            SetErrorDisplayType(ErrorDisplayType::ERR_MESSAGE_BOX);  
    }
    enum OperationType {
        UNKNOWN, DUMP, EXPORT, IMPORT, INFO, DUMPSHADERS
    } opType = OperationType::UNKNOWN;
    void (*callback)(path const &, path const &) = nullptr;
    bool isCustom = false;
    string targetExt;
    set<string> inExt;
    if (argc >= 2) {
        string opTypeStr = argv[1];
        if (opTypeStr == "import") {
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
    }
    if (opType == OperationType::UNKNOWN) {
        ErrorMessage("Unknown operation type");
        return ErrorType::UNKNOWN_OPERATION_TYPE;
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
    }
    else if (opType == OperationType::IMPORT) {
        if (cmd.HasOption("noMetadata"))
            options().noMetadata = true;
        if (cmd.HasArgument("scale"))
            options().scale = cmd.GetArgumentFloat("scale");
        if (cmd.HasOption("tristrip"))
            options().tristrip = true;
        if (cmd.HasOption("embeddedTextures"))
            options().embeddedTextures = true;
        if (cmd.HasOption("swapYZ"))
            options().swapYZ = true;
        if (cmd.HasOption("forceLighting"))
            options().forceLighting = true;
        if (cmd.HasArgument("setVCol")) {
            string setVCol = cmd.GetArgumentString("setVCol");
            unsigned int r = 255, g = 255, b = 255, a = 255;
            if (setVCol.length() == 6)
                options().hasSetVCol = (sscanf(setVCol.c_str(), "%2X%2X%2X", &r, &g, &b) == 3);
            else if (setVCol.length() == 8)
                options().hasSetVCol = (sscanf(setVCol.c_str(), "%2X%2X%2X%2X", &r, &g, &b, &a) == 4);
            if (options().hasSetVCol) {
                options().setVCol.r = float(r) / 255.0f;
                options().setVCol.g = float(g) / 255.0f;
                options().setVCol.b = float(b) / 255.0f;
                options().setVCol.a = float(a) / 255.0f;
            }
        }
        else {
            if (cmd.HasArgument("defaultVCol")) {
                string defaultVCol = cmd.GetArgumentString("defaultVCol");
                unsigned int r = 255, g = 255, b = 255, a = 255;
                if (defaultVCol.length() == 6)
                    options().hasDefaultVCol = (sscanf(defaultVCol.c_str(), "%2X%2X%2X", &r, &g, &b) == 3);
                else if (defaultVCol.length() == 8)
                    options().hasDefaultVCol = (sscanf(defaultVCol.c_str(), "%2X%2X%2X%2X", &r, &g, &b, &a) == 4);
                if (options().hasDefaultVCol) {
                    options().defaultVCol.r = float(r) / 255.0f;
                    options().defaultVCol.g = float(g) / 255.0f;
                    options().defaultVCol.b = float(b) / 255.0f;
                    options().defaultVCol.a = float(a) / 255.0f;
                }
            }
            if (cmd.HasArgument("vColScale"))
                options().vColScale = cmd.GetArgumentFloat("vColScale");
        }
        if (cmd.HasOption("genTexNames"))
            options().genTexNames = true;
        try {
            if (cmd.HasOption("writeFsh")) {
                options().writeFsh = true;
                if (cmd.HasArgument("fshOutput"))
                    options().fshOutput = cmd.GetArgumentString("fshOutput");
                static D3DDevice device(options().hwnd ? options().hwnd : unsigned int(GetConsoleWindow()));
                ea::Fsh::SetDevice(&device);
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
            }
        }
        catch (exception e) {
            ErrorMessage(e.what());
            options().writeFsh = false;
        }
        if (cmd.HasOption("preTransformVertices"))
            options().preTransformVertices = true;
        if (cmd.HasOption("sortByName"))
            options().sortByName = true;
        if (cmd.HasOption("sortByAlpha"))
            options().sortByAlpha = true;
        if (cmd.HasOption("ignoreMatColor"))
            options().ignoreMatColor = true;
        if (cmd.HasOption("head"))
            options().head = true;
        if (cmd.HasArgument("pad"))
            options().pad = cmd.GetArgumentInt("pad");
        if (cmd.HasOption("ignoreEmbeddedTextures"))
            options().ignoreEmbeddedTextures = true;
        if (cmd.HasArgument("instances"))
            options().instances = cmd.GetArgumentInt("instances");
        if (cmd.HasArgument("computationIndex"))
            options().computationIndex = cmd.GetArgumentInt("computationIndex");
        if (cmd.HasOption("gl20"))
            options().gl20 = true;
    }
    else if (opType == OperationType::DUMP) {
        if (cmd.HasOption("onlyFirstTechnique"))
            options().onlyFirstTechnique = true;
    }
    path o;
    bool hasOutput = cmd.HasArgument("o");
    if (hasOutput)
        o = cmd.GetArgumentString("o");
    bool createSubDir = cmd.HasOption("createSubDir");
    string startsWith;
    if (cmd.HasArgument("startsWith"))
        startsWith = cmd.GetArgumentString("startsWith");

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
    else
        callback(o, i);

    if (options().writeFsh)
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
