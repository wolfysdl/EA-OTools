#include "main.h"
#include "Rx3/ExtractTextures.h"
#include "Rx3/ExtractModel.h"

void rx3export(path const &out, path const &in) {
    Rx3Container rx3(in);
    string filename = out.filename().string();
    if (filename.length() > 9 && filename.ends_with("_textures"))
        ExtractTexturesFromContainer(rx3, out.parent_path().parent_path() / filename.substr(0, filename.length() - 9));
    else {
        ExtractTexturesFromContainer(rx3, out.parent_path());
        ExtractModelFromContainer(rx3, out);
    }
}
