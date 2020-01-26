#include "d3dx9.h"
#include "main.h"
#include "commandline.h"
#include "errormsg.h"
#include "Fsh/Fsh.h"

const char *OTOOLS_VERSION = "0.133";

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
    CommandLine cmd(argc, argv, { "i", "o", "defaultVCol", "vColScale", "fshOutput", "fshLevels", "fshFormat", "fshAddTextures", "fshIgnoreTextures" }, { "keepPrimType", "noTextures", "recursive", "createSubDir", "silent", "onlyFirstTechnique", "dummyTextures", "jpegTextures", "embeddedTextures", "swapYZ", "forceLighting", "noMetadata", "genTexNames", "writeFsh", "fshRescale", "fshDisableTextureIgnore", "preTransformVertices" });
    if (cmd.HasOption("silent"))
        SetErrorDisplayType(ErrorDisplayType::ERR_NONE);
    else {
        if (!cmd.HasOption("console"))
            SetErrorDisplayType(ErrorDisplayType::ERR_MESSAGE_BOX);
        else
            SetErrorDisplayType(ErrorDisplayType::ERR_CONSOLE);
    }
    enum OperationType {
        UNKNOWN, DUMP, EXPORT, IMPORT, INFO
    } opType = OperationType::UNKNOWN;
    void (*callback)(path const &, path const &) = nullptr;
    string targetExt;
    set<string> inExt;
    if (argc >= 2) {
        string opTypeStr = argv[1];
        if (opTypeStr == "import") {
            opType = OperationType::IMPORT;
            callback = oimport;
            inExt = { ".gltf", ".glb", ".dae", ".fbx", ".obj", ".blend", ".3ds" };
            targetExt = ".o";
        }
        else if (opTypeStr == "export") {
            opType = OperationType::EXPORT;
            callback = oexport;
            inExt = { ".o" };
            targetExt = ".gltf";
        }
        else if (opTypeStr == "dump") {
            opType = OperationType::DUMP;
            callback = odump;
            inExt = { ".o" };
            targetExt = ".txt";
        }
        else if (opTypeStr == "info") {
            opType = OperationType::INFO;
            callback = oinfo;
            inExt = { ".o" };
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
    if (opType == OperationType::EXPORT) {
        if (cmd.HasOption("noTextures"))
            options().noTextures = true;
        if (cmd.HasOption("dummyTextures"))
            options().dummyTextures = true;
        if (cmd.HasOption("jpegTextures"))
            options().jpegTextures = true;
    }
    else if (opType == OperationType::IMPORT) {
        if (cmd.HasOption("noMetadata"))
            options().noMetadata = true;
        if (cmd.HasOption("tristrip"))
            options().tristrip = true;
        if (cmd.HasOption("embeddedTextures"))
            options().embeddedTextures = true;
        if (cmd.HasOption("swapYZ"))
            options().swapYZ = true;
        if (cmd.HasOption("forceLighting"))
            options().forceLighting = true;
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
        if (cmd.HasOption("genTexNames"))
            options().genTexNames = true;
        if (cmd.HasOption("writeFsh")) {
            options().writeFsh = true;
            if (cmd.HasArgument("fshOutput"))
                options().fshOutput = cmd.GetArgumentString("fshOutput");
            ea::Fsh::SetDevice(new ea::D3DDevice());
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
                    else if (format == "rgb")
                        options().fshFormat = unsigned int(-5);
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
        }
        if (cmd.HasOption("preTransformVertices"))
            options().preTransformVertices = true;
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
    
    auto processFile = [=](path const &in, bool inDir) {
        try {
            if (!inDir || (is_regular_file(in) && inExt.contains(ToLower(in.extension().string())))) {
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
        catch (exception &e) {
            ErrorMessage(in.filename().string() + ": " + e.what());
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

    if (options().writeFsh)
        ea::Fsh::ClearDevice();

    return ErrorType::NONE;
}
