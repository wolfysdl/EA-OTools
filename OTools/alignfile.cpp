#include "main.h"

void align_file(path const &out, path const &in) {
    if (in.has_extension()) {
        auto ext = ToLower(in.extension().string());
        unsigned int pad = 0;
        if (ext == ".o")
            pad = options().pad;
        else if (ext == ".fsh")
            pad = options().padFsh;
        if (pad != 0) {
            FILE *f = _wfopen(in.c_str(), L"rb");
            if (f) {
                fseek(f, 0, SEEK_END);
                unsigned int fsize = ftell(f);
                if (fsize < pad) {
                    fseek(f, 0, SEEK_SET);
                    vector<unsigned char> fdata(pad, 0);
                    fread(fdata.data(), fsize, 1, f);
                    fclose(f);
                    auto dst = out;
                    dst.replace_filename(wstring(out.stem().c_str()) + in.extension().c_str());
                    FILE *o = _wfopen(dst.c_str(), L"wb");
                    if (o) {
                        fwrite(fdata.data(), fdata.size(), 1, o);
                        fclose(o);
                    }
                }
            }
        }
    }
}
