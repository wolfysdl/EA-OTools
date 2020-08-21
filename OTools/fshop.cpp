#include "d3d9.h"
#include "main.h"
#include "Fsh/Fsh.h"

TextureToAdd::TextureToAdd() {};

TextureToAdd::TextureToAdd(string const &_name, string const &_filepath, unsigned int _format, int _levels, TexEmbedded const &_embedded) {
    name = _name; filepath = _filepath; format = _format; levels = _levels; embedded = _embedded;
}

void packfsh_collect(path const &out, path const &in) {
    auto &fsh = globalVars().fshToBuild[out.parent_path()];
    auto filename = in.stem().string();
    auto texkey = ToLower(filename);
    if (!fsh.contains(texkey)) {
        auto &tex = fsh[texkey];
        tex.name = filename;
        tex.filepath = filename;
    }
}

void packfsh_pack() {
    for (auto const &[fshPath, fshImages] : globalVars().fshToBuild) {
        path fshFinalPath;
        if (options().fshWriteToParentDir && fshPath.has_parent_path())
            fshFinalPath = fshPath.parent_path() / (fshPath.filename().string() + ".fsh");
        else
            fshFinalPath = fshPath / (fshPath.filename().string() + ".fsh");
        WriteFsh(fshFinalPath, fshPath, fshImages);
    }
}

void unpackfsh(path const &out, path const &in) {
    ea::Fsh fsh;
    fsh.Read(in);
    fsh.ForAllImages([&](ea::FshImage &image) {
        image.WriteToFile(out.parent_path() / (image.GetTag() + "." + options().fshUnpackImageFormat), globalVars().fshUnpackImageFormat);
    });
}

void WriteFsh(path const &fshFilePath, path const &searchDir, map<string, TextureToAdd> const &texturesToAdd) {
    static vector<string> imgExt = { ".png", ".jpg", ".jpeg", ".bmp", ".dds", ".tga" };
    path fshDir = fshFilePath.parent_path();
    string fshFileName = fshFilePath.filename().string();
    ea::Fsh fsh;
    ea::Buffer metalBinData;
    metalBinData.Allocate(64);
    Memory_Zero(metalBinData.GetData(), metalBinData.GetSize());
    strcpy((char *)metalBinData.GetData(), "EAGL64 metal bin attachment for runtime texture management");
    if (!texturesToAdd.empty()) {
        for (auto const &[k, img] : texturesToAdd) {
            ea::FshImage::LoadingInfo loadingInfo;
            if (img.embedded.data && !options().ignoreEmbeddedTextures) {
                if (img.embedded.height == 0) { // compressed data
                    auto fileFormat = ea::FshImage::DIB;
                    if (img.embedded.format == "png")
                        fileFormat = ea::FshImage::PNG;
                    else if (img.embedded.format == "jpg")
                        fileFormat = ea::FshImage::JPG;
                    else if (img.embedded.format == "bmp")
                        fileFormat = ea::FshImage::BMP;
                    else if (img.embedded.format == "tga")
                        fileFormat = ea::FshImage::TGA;
                    else if (img.embedded.format == "dds")
                        fileFormat = ea::FshImage::DDS;
                    if (fileFormat != ea::FshImage::DIB) {
                        loadingInfo.fileData = img.embedded.data;
                        loadingInfo.fileDataSize = img.embedded.width;
                        loadingInfo.fileFormat = fileFormat;
                    }
                } else { // assimp uncompressed data
                    D3DFORMAT dataFormat = D3DFMT_UNKNOWN;
                    if (img.embedded.format == "rgba8888")
                        dataFormat = D3DFMT_A8R8G8B8;
                    else if (img.embedded.format == "argb8888")
                        dataFormat = D3DFMT_A8B8G8R8;
                    else if (img.embedded.format == "rgba5650")
                        dataFormat = D3DFMT_R5G6B5;
                    else if (img.embedded.format == "rgba0010")
                        dataFormat = D3DFMT_L8;
                    if (dataFormat != D3DFMT_UNKNOWN) {
                        loadingInfo.data = img.embedded.data;
                        loadingInfo.dataWidth = img.embedded.width;
                        loadingInfo.dataHeight = img.embedded.height;
                        loadingInfo.dataFormat = unsigned int(dataFormat);
                    }
                }
            }
            if (!loadingInfo.fileData && !loadingInfo.data) {
                path imgPath = img.filepath;
                auto imgParentDir = imgPath.parent_path();
                auto imgFileName = imgPath.filename().string();
                auto atPos = imgFileName.find('@');
                unsigned int numSearchPasses = 1;
                if (atPos != string::npos && atPos != 0)
                    numSearchPasses = 2;

                for (unsigned int searchPass = 0; searchPass < numSearchPasses; searchPass++) {
                    if (searchPass == 1)
                        imgPath = imgParentDir / imgFileName.substr(0, atPos);
                    loadingInfo.filepath = imgPath;
                    bool hasExtension = false;
                    if (imgPath.has_extension()) {
                        string ext = ToLower(imgPath.extension().string());
                        for (auto const &ie : imgExt) {
                            if (ext == ie) {
                                hasExtension = true;
                                break;
                            }
                        }
                    }
                    if (hasExtension) {
                        loadingInfo.fileExists = exists(imgPath);
                        if (!loadingInfo.fileExists) {
                            loadingInfo.filepath = searchDir / imgPath;
                            loadingInfo.fileExists = exists(loadingInfo.filepath);
                            if (!loadingInfo.fileExists) {
                                imgPath.replace_extension();
                                hasExtension = false;
                            }
                        }
                    }
                    if (!loadingInfo.fileExists && !hasExtension) {
                        for (auto const &ie : imgExt) {
                            string filePathWithExt = imgPath.string() + ie;
                            loadingInfo.filepath = filePathWithExt;
                            loadingInfo.fileExists = exists(loadingInfo.filepath);
                            if (loadingInfo.fileExists)
                                break;
                            loadingInfo.filepath = searchDir / filePathWithExt;
                            loadingInfo.fileExists = exists(loadingInfo.filepath);
                            if (loadingInfo.fileExists)
                                break;
                        }
                    }
                    if (searchPass == 0 && loadingInfo.fileExists)
                        break;
                }
            }
            if (loadingInfo.fileData || loadingInfo.data || loadingInfo.fileExists) {
                auto &image = fsh.AddImage();
                image.Load(loadingInfo, img.format, img.levels, options().fshRescale);
                ea::FshPixelData *pixelsData = image.FindFirstData(ea::FshData::PIXELDATA)->As<ea::FshPixelData>();
                image.AddData(new ea::FshMetalBin(metalBinData, 0x10));
                image.SetTag(img.name);
                image.AddData(new ea::FshName(img.name));
                char comment[256];
                static char idStr[260];
                unsigned int texNameHash = 0;
                if (options().useFshHash)
                    texNameHash = options().fshHash;
                else
                    texNameHash = Hash(fshFilePath.stem().string() + "_" + img.name);
                sprintf_s(idStr, "0x%.8x", texNameHash);
                sprintf_s(comment, "TXLY,%s,%d,%d,%d,%d,%s", image.GetTag().c_str(), options().fshId, pixelsData->GetNumMipLevels() > 0 ? 1 : 0,
                    pixelsData->GetWidth(), pixelsData->GetHeight(), idStr);
                image.AddData(new ea::FshComment(comment));
            }
        }
        if (fsh.GetImagesCount() > 0) {
            fsh.ForAllImages([&](ea::FshImage &image) {
                auto hotSpot = image.AddData(new ea::FshHotSpot())->As<ea::FshHotSpot>();
                ea::FshPixelData *pixelsData = image.FindFirstData(ea::FshData::PIXELDATA)->As<ea::FshPixelData>();
                if (pixelsData) {
                    if (options().head) {
                        if (globalVars().target) {
                            string targetName = globalVars().target->Name();
                            if (targetName == "CL0405") {
                                if (image.GetTag() == "glos") {
                                    hotSpot->Regions().push_back(ea::FshHotSpot::Region('sphi', 102, 0, 154, 128));
                                    hotSpot->Regions().push_back(ea::FshHotSpot::Region('spsk', 0, 0, 102, 128));
                                }
                                else if (image.GetTag() == "face") {
                                    hotSpot->Regions().push_back(ea::FshHotSpot::Region('hifa', 102, 0, 154, 128));
                                    hotSpot->Regions().push_back(ea::FshHotSpot::Region('skin', 0, 0, 102, 128));
                                }
                                else if (image.GetTag() == "tp02")
                                    hotSpot->Regions().push_back(ea::FshHotSpot::Region('hair', 0, 0, pixelsData->GetWidth(), pixelsData->GetHeight()));
                            }
                        }
                    }
                    else if (options().fshBalls)
                        hotSpot->Regions().push_back(ea::FshHotSpot::Region('ball', 0, 0, pixelsData->GetWidth(), pixelsData->GetHeight()));
                    else if (options().fshKits) {
                        unsigned int shortsHeight = unsigned int(roundf(float(pixelsData->GetHeight()) * 0.333f));
                        hotSpot->Regions().push_back(ea::FshHotSpot::Region('jrsy', 0, shortsHeight, pixelsData->GetWidth(), pixelsData->GetHeight() - shortsHeight));
                        hotSpot->Regions().push_back(ea::FshHotSpot::Region('shrt', 0, 0, pixelsData->GetWidth(), shortsHeight));
                    }
                    else if (options().fshShoes)
                        hotSpot->Regions().push_back(ea::FshHotSpot::Region('clet', 0, 0, pixelsData->GetWidth(), pixelsData->GetHeight()));
                    else if (options().fshPatterns)
                        hotSpot->Regions().push_back(ea::FshHotSpot::Region('mowp', 0, 0, pixelsData->GetWidth(), pixelsData->GetHeight()));
                    else if (image.GetTag() == "misc") {
                        hotSpot->Regions().push_back(ea::FshHotSpot::Region('glov', 0, 0, pixelsData->GetWidth() / 2, pixelsData->GetHeight()));
                        hotSpot->Regions().push_back(ea::FshHotSpot::Region('flag', pixelsData->GetWidth() / 2, 0, pixelsData->GetWidth() / 2, pixelsData->GetHeight()));
                    }
                }
                if (hotSpot->Regions().empty()) {
                    fsh.ForAllImages([&](ea::FshImage &image2) {
                        char fourcc[4] = { 0, 0, 0, 0 };
                        auto tag = image2.GetTag();
                        for (unsigned int i = 0; i < 4; i++) {
                            if (tag.size() > i)
                                fourcc[i] = tag[i];
                        }
                        std::swap(fourcc[0], fourcc[3]);
                        std::swap(fourcc[1], fourcc[2]);
                        hotSpot->Regions().push_back(ea::FshHotSpot::Region(*((unsigned int *)fourcc), 0, 0,
                            image2.FindFirstData(ea::FshData::PIXELDATA)->As<ea::FshPixelData>()->GetWidth(),
                            image2.FindFirstData(ea::FshData::PIXELDATA)->As<ea::FshPixelData>()->GetHeight()));
                    });
                }
            });
            if (!fshDir.empty())
                create_directories(fshDir);
            fsh.SetAddBuyERTS(true);
            if (globalVars().target) {
                string targetName = globalVars().target->Name();
                if (targetName == "CL0405")
                    fsh.SetAlignment(8);
            }
            fsh.Write(fshFilePath);
            if (options().padFsh > 0) {
                FILE *fshFile = _wfopen(fshFilePath.c_str(), L"a+");
                if (fshFile) {
                    fseek(fshFile, 0, SEEK_END);
                    unsigned int fshSize = ftell(fshFile);
                    if (fshSize < options().padFsh) {
                        unsigned int numPaddingBytes = options().padFsh - fshSize;
                        static unsigned char zero = 0;
                        for (unsigned int i = 0; i < numPaddingBytes; i++)
                            fwrite(&zero, 1, 1, fshFile);
                    }
                    fclose(fshFile);
                }
            }
        }
    }
};
