#include "Fsh.h"
#include "Exception.h"
#include "d3dx9.h"
#include "..\utils.h"

D3DDevice *ea::Fsh::GlobalDevice;

ea::FshData::~FshData() {}

ea::FshData::DataType ea::FshName::GetDataType() const { return FshData::NAME; }

size_t ea::FshName::GetDataSize() const { return mName.length() + 1; }

ea::FshName::FshName(std::string const & name) { mName = name; }

std::string const & ea::FshName::GetName() const { return mName; }

void ea::FshName::SetName(std::string const & name) { mName = name; }

ea::FshData::DataType ea::FshMetalBin::GetDataType() const { return FshData::METALBIN; }

size_t ea::FshMetalBin::GetDataSize() const { return 12 + mBuffer.GetSize(); }

ea::FshMetalBin::FshMetalBin(ea::Buffer const & buffer, unsigned short flags, unsigned int unknown1, unsigned int unknown2) { mBuffer = buffer; mFlags = flags; mUnknown1 = unknown1; mUnknown2 = unknown2; }

ea::FshMetalBin::FshMetalBin(ea::Buffer && buffer, unsigned short flags, unsigned int unknown1, unsigned int unknown2) { mBuffer = std::move(buffer); mFlags = flags; mUnknown1 = unknown1; mUnknown2 = unknown2; }

unsigned short ea::FshMetalBin::GetFlags() const { return mFlags; }

void ea::FshMetalBin::SetFlags(unsigned short flags) { mFlags = flags; }

unsigned int ea::FshMetalBin::GetUnknown1() const { return mUnknown1; }

void ea::FshMetalBin::SetUnknown1(unsigned int unknown1) { mUnknown1 = unknown1; }

unsigned int ea::FshMetalBin::GetUnknown2() const { return mUnknown2; }

void ea::FshMetalBin::SetUnknown2(unsigned int unknown2) { mUnknown2 = unknown2; }

ea::Buffer & ea::FshMetalBin::Buffer() { return mBuffer; }

ea::FshData::DataType ea::FshComment::GetDataType() const { return FshData::COMMENT; }

size_t ea::FshComment::GetDataSize() const { return 4 + mComment.length() + 1; }

ea::FshComment::FshComment(std::string const & comment) { mComment = comment; }

std::string const & ea::FshComment::GetComment() const { return mComment; }

void ea::FshComment::SetComment(std::string const & comment) { mComment = comment; }

ea::FshData::DataType ea::FshHotSpot::GetDataType() const { return FshData::HOTSPOT; }

size_t ea::FshHotSpot::GetDataSize() const { return 4 + mRegions.size() * 24; }

std::vector<ea::FshHotSpot::Region>& ea::FshHotSpot::Regions() { return mRegions; }

ea::FshData::DataType ea::FshUnknown::GetDataType() const { return FshData::UNKNOWN; }

size_t ea::FshUnknown::GetDataSize() const { return mBuffer.GetSize(); }

ea::FshUnknown::FshUnknown(unsigned char id, ea::Buffer const & buffer) { mId = id; mBuffer = buffer; }

ea::FshUnknown::FshUnknown(unsigned char id, ea::Buffer && buffer) { mId = id; mBuffer = std::move(buffer); }

unsigned char ea::FshUnknown::GetId() const { return mId; }

void ea::FshUnknown::SetId(unsigned char id) { mId = id; }

ea::Buffer & ea::FshUnknown::Buffer() { return mBuffer; }

std::string ea::FshImage::GetTag() const { return std::string(mTag, 4); }

void ea::FshImage::SetTag(std::string const & tag) { strncpy(mTag, tag.c_str(), 4); }

void ea::FshImage::Clear() {
	for (FshData *d : mDatas)
		delete d;
	mDatas.clear();
}

ea::FshImage::~FshImage() {
	Clear();
}

ea::FshImage::FshImage() {

}

ea::FshImage::FshImage(FshImage &&rhs) {
    memcpy(mTag, rhs.mTag, 4);
    mDatas = rhs.mDatas;
    rhs.mDatas.clear();
}

std::vector<ea::FshData*> ea::FshImage::FindAllDatas(FshData::DataType dataType) {
	std::vector<FshData *> result;
	for (FshData *d : mDatas) {
		if (d->GetDataType() == dataType)
			result.push_back(d);
	}
	return result;
}

ea::FshData * ea::FshImage::FindFirstData(FshData::DataType dataType) {
	for (FshData *d : mDatas) {
		if (d->GetDataType() == dataType)
			return d;
	}
	return nullptr;
}

void ea::FshImage::RemoveAllDatas(FshData::DataType dataType) {
	mDatas.erase(std::remove_if(mDatas.begin(), mDatas.end(), [=](FshData *data) {
		if (data->GetDataType() == dataType) {
			delete data;
			return true;
		}
		return false;
	}), mDatas.end());
}

void ea::FshImage::SetData(FshData * data) {
	bool dataReplaced = false;
	bool clearFreeSpaces = false;
	for (size_t i = 0; i < mDatas.size(); i++) {
		if (mDatas[i]->GetDataType() == data->GetDataType()) {
			delete mDatas[i];
			if (!dataReplaced) {
				mDatas[i] = data;
				dataReplaced = true;
			}
			else {
				mDatas[i] = nullptr;
				if (!clearFreeSpaces)
					clearFreeSpaces = true;
			}
		}
	}
	if (clearFreeSpaces)
		mDatas.erase(std::remove(mDatas.begin(), mDatas.end(), nullptr));
	if (!dataReplaced)
		AddData(data);
}

ea::FshData * ea::FshImage::AddData(FshData * data) {
	return mDatas.emplace_back(data);
}

unsigned int ea::FshImage::GetPixelD3DFormat(unsigned char format) {
	switch (format) {
	case FshPixelData::PIXEL_DXT1:
		return D3DFMT_DXT1;
	case FshPixelData::PIXEL_DXT3:
		return D3DFMT_DXT3;
	case FshPixelData::PIXEL_DXT5:
		return D3DFMT_DXT5;
	case FshPixelData::PIXEL_8888:
		return D3DFMT_A8R8G8B8;
	case FshPixelData::PIXEL_888:
		return D3DFMT_X8R8G8B8;
	case FshPixelData::PIXEL_4444:
		return D3DFMT_A4R4G4B4;
	case FshPixelData::PIXEL_5551:
		return D3DFMT_A1R5G5B5;
	case FshPixelData::PIXEL_565:
		return D3DFMT_R5G6B5;
	}
	return D3DFMT_UNKNOWN;
}

unsigned char ea::FshImage::GetPixelFormat(unsigned int format) {
	switch (format) {
	case D3DFMT_DXT1:
		return FshPixelData::PIXEL_DXT1;
	case D3DFMT_DXT3:
		return FshPixelData::PIXEL_DXT3;
	case D3DFMT_DXT5:
		return FshPixelData::PIXEL_DXT5;
	case D3DFMT_A8R8G8B8:
		return FshPixelData::PIXEL_8888;
	case D3DFMT_X8R8G8B8:
		return FshPixelData::PIXEL_8888;
	case D3DFMT_A4R4G4B4:
		return FshPixelData::PIXEL_4444;
	case D3DFMT_A1R5G5B5:
		return FshPixelData::PIXEL_5551;
	case D3DFMT_R5G6B5:
		return FshPixelData::PIXEL_565;
	}
	return 0;
}

unsigned int ea::FshImage::GetPixelDataSize(unsigned short width, unsigned short height, unsigned char format) {
	if (format == FshPixelData::PIXEL_DXT1 || format == FshPixelData::PIXEL_DXT3 || format == FshPixelData::PIXEL_DXT5) {
		if (width < 4)
			width = 4;
		if (height < 4)
			height = 4;
	}
	switch (format) {
	case FshPixelData::PIXEL_DXT1:
		return width * height / 2;
	case FshPixelData::PIXEL_DXT3:
	case FshPixelData::PIXEL_DXT5:
	case FshPixelData::PIXEL_PAL4:
	case FshPixelData::PIXEL_PAL8:
		return width * height;
	case FshPixelData::PIXEL_8888:
		return width * height * 4;
	case FshPixelData::PIXEL_888:
		return width * height * 3;
	case FshPixelData::PIXEL_4444:
	case FshPixelData::PIXEL_5551:
	case FshPixelData::PIXEL_565:
		return width * height * 2;
	}
	return 0;
}

#pragma pack(push, 1)
struct clr_x8r8g8b8 { unsigned char b, g, r, x; };
struct clr_b8g8r8 { unsigned char b, g, r; };
#pragma pack(pop)

void ea::FshImage::WriteToFile(std::filesystem::path const &filepath, FileFormat fileFormat) {
    auto pixelDatas = FindAllDatas(FshData::PIXELDATA);
    if (pixelDatas.empty())
        throw Exception("WriteToFile: image has no pixels data");
    FshPixelData *imgData = pixelDatas.front()->As<FshPixelData>();
    D3DFORMAT format = D3DFORMAT(GetPixelD3DFormat(imgData->GetFormat()));
    if (format == D3DFMT_UNKNOWN)
        throw Exception("WriteToFile: unsupported pixels format");
    IDirect3DTexture9 *texture = nullptr;
    unsigned short w = imgData->GetWidth();
    unsigned short h = imgData->GetHeight();
    unsigned char numLevels = imgData->GetNumMipLevels() + 1;
    if (FAILED(Fsh::GlobalDevice->Interface()->CreateTexture(w, h, numLevels, D3DUSAGE_DYNAMIC, format, D3DPOOL_SYSTEMMEM, &texture, NULL)))
        throw Exception("WriteToFile: failed to create direct3d texture");
	D3DSURFACE_DESC desc;
	if (FAILED(texture->GetLevelDesc(0, &desc))) {
		texture->Release();
		throw Exception("WriteToFile: failed to retrieve texture format");
	}
	if (desc.Format != format) {
		texture->Release();
		throw Exception(FormatStatic("WriteToFile: unsupported texture format (input format: %d, result format: %d)", format, desc.Format));
	}
    unsigned char *pixels = (unsigned char *)imgData->Pixels().GetData();
    for (unsigned int i = 0; i < numLevels; i++) {
        size_t pixelsDataSize = GetPixelDataSize(w, h, imgData->GetFormat());
        D3DLOCKED_RECT rect;
        if (FAILED(texture->LockRect(i, &rect, NULL, D3DLOCK_DISCARD))) {
            texture->Release();
            throw Exception("WriteToFile: failed to lock texture");
        }
		if (format == D3DFMT_X8R8G8B8) {
			clr_x8r8g8b8 *xrgb = (clr_x8r8g8b8 *)rect.pBits;
			clr_b8g8r8 *bgr = (clr_b8g8r8 *)pixels;
			unsigned int numPixels = w * h;
			for (unsigned int p = 0; p < numPixels; p++) {
				xrgb[p].r = bgr[p].r;
				xrgb[p].g = bgr[p].g;
				xrgb[p].b = bgr[p].b;
				xrgb[p].x = 255;
			}
		}
		else
            memcpy(rect.pBits, pixels, pixelsDataSize);
        if (FAILED(texture->UnlockRect(i))) {
            texture->Release();
            throw Exception("WriteToFile: failed to unlock texture");
        }
        pixels = &pixels[pixelsDataSize];
        w /= 2;
        h /= 2;
    }
    if (FAILED(D3DXSaveTextureToFileW(filepath.c_str(), static_cast<D3DXIMAGE_FILEFORMAT>(fileFormat), texture, NULL))) {
        texture->Release();
        throw Exception("WriteToFile: failed to save texture");
    }
    texture->Release();
}

void ea::FshImage::ReadFromFile(std::filesystem::path const &filepath, unsigned int d3dformat, unsigned int levels, bool rescale) {
    RemoveAllDatas(FshData::PIXELDATA);
    IDirect3DTexture9 *texture = nullptr;
	D3DXIMAGE_INFO imageInfo;
	if (FAILED(D3DXGetImageInfoFromFileW(filepath.c_str(), &imageInfo)))
		throw Exception("ReadFromFile: unable to get image info from file");
	if (d3dformat == unsigned int(-4) || d3dformat == unsigned int(-5) || d3dformat == unsigned int(-6)) {
		switch (imageInfo.Format) {
		case D3DFMT_A1:
		case D3DFMT_A4L4:
		case D3DFMT_A8:
		case D3DFMT_A8L8:
		case D3DFMT_A8P8:
		case D3DFMT_A4R4G4B4:
		case D3DFMT_A1R5G5B5:
		case D3DFMT_A8R3G3B2:
		case D3DFMT_DXT2:
		case D3DFMT_DXT3:
		case D3DFMT_DXT4:
		case D3DFMT_DXT5:
		case D3DFMT_A8B8G8R8:
		case D3DFMT_A8R8G8B8:
		case D3DFMT_A2B10G10R10:
		case D3DFMT_A2R10G10B10:
		case D3DFMT_A16B16G16R16:
		case D3DFMT_A16B16G16R16F:
		case D3DFMT_A32B32G32R32F:
		    {
			    if (d3dformat == unsigned int(-4))
					d3dformat = D3DFMT_DXT5;
				else if (d3dformat == unsigned int(-6))
					d3dformat = D3DFMT_A4R4G4B4;
				else
					d3dformat = D3DFMT_A8R8G8B8;
		    }
			break;
		default:
			{
			    if (d3dformat == unsigned int(-4))
					d3dformat = D3DFMT_DXT1;
				else if (d3dformat == unsigned int(-6))
					d3dformat = D3DFMT_R5G6B5;
				else
					d3dformat = D3DFMT_A8R8G8B8;
		    }
			break;
		}
	}
	else if (d3dformat == D3DFMT_FROM_FILE) {
		unsigned char testFormat = GetPixelFormat(imageInfo.Format);
		if (testFormat == 0 || testFormat == FshPixelData::PIXEL_888)
			d3dformat = D3DFMT_A8R8G8B8;
	}
    if (FAILED(D3DXCreateTextureFromFileExW(Fsh::GlobalDevice->Interface(), filepath.c_str(),
		rescale ? D3DX_DEFAULT : D3DX_DEFAULT_NONPOW2, rescale ? D3DX_DEFAULT : D3DX_DEFAULT_NONPOW2,
        levels, D3DUSAGE_DYNAMIC, D3DFORMAT(d3dformat), D3DPOOL_SYSTEMMEM, D3DX_FILTER_TRIANGLE | D3DX_FILTER_DITHER, D3DX_FILTER_BOX, 0,
        NULL, NULL, &texture)))
    {
        throw Exception("ReadFromFile: failed to create direct3d texture");
    }
    D3DSURFACE_DESC desc;
    if (FAILED(texture->GetLevelDesc(0, &desc))) {
        texture->Release();
        throw Exception("ReadFromFile: failed to retrieve texture format");
    }
    unsigned char format = GetPixelFormat(desc.Format);
    if (format == 0) {
        texture->Release();
		throw Exception(FormatStatic("ReadFromFile: unsupported texture format (input format: %d, result format: %d)", d3dformat, desc.Format));
    }
    unsigned char numLevels = (unsigned char)texture->GetLevelCount();
    size_t pixelsSize = 0;
    unsigned short w = desc.Width;
    unsigned short h = desc.Height;
    for (unsigned int i = 0; i < numLevels; i++) {
        pixelsSize += GetPixelDataSize(w, h, format);
        w /= 2;
        h /= 2;
    }
    Buffer pixels;
    pixels.Allocate(pixelsSize);
    w = desc.Width;
    h = desc.Height;
    unsigned char *pixelsPtr = (unsigned char *)pixels.GetData();
    for (unsigned int i = 0; i < numLevels; i++) {
        size_t pixelsDataSize = GetPixelDataSize(w, h, format);
        D3DLOCKED_RECT rect;
        if (FAILED(texture->LockRect(i, &rect, NULL, D3DLOCK_READONLY))) {
            texture->Release();
            throw Exception("ReadFromFile: failed to lock texture");
        }
		if (desc.Format == D3DFMT_X8R8G8B8) {
			clr_x8r8g8b8 *xrgb = (clr_x8r8g8b8 *)rect.pBits;
			clr_b8g8r8 *bgr = (clr_b8g8r8 *)pixelsPtr;
			unsigned int numPixels = w * h;
			for (unsigned int p = 0; p < numPixels; p++) {
				bgr[p].r = xrgb[p].r;
				bgr[p].g = xrgb[p].g;
				bgr[p].b = xrgb[p].b;
			}
		}
		else
            memcpy(pixelsPtr, rect.pBits, pixelsDataSize);
        if (FAILED(texture->UnlockRect(i))) {
            texture->Release();
            throw Exception("ReadFromFile: failed to unlock texture");
        }
        pixelsPtr = &pixelsPtr[pixelsDataSize];
        w /= 2;
        h /= 2;
    }
    AddData(new FshPixelData(format, pixels, desc.Width, desc.Height, (unsigned char)texture->GetLevelCount() - 1, 0, 0, 0, 0, 0));
    texture->Release();
}

enum AlphaCheckState {
	NoAlpha,
	HasAlpha1Bit,
	HasAlpha
};

AlphaCheckState GetTextureAlpha(IDirect3DTexture9 *tex) {
	IDirect3DSurface9 *surface = nullptr;
	D3DLOCKED_RECT rect;
	D3DSURFACE_DESC desc;
	AlphaCheckState alphaCheckState = NoAlpha;
	if (SUCCEEDED(tex->GetSurfaceLevel(0, &surface))
		&& SUCCEEDED(surface->GetDesc(&desc))
		&& desc.Format == D3DFMT_A8R8G8B8
		&& SUCCEEDED(surface->LockRect(&rect, 0, D3DLOCK_READONLY)))
	{
		for (unsigned int i = 0; i < desc.Height; i++) {
			unsigned char *pSrcData = (unsigned char *)rect.pBits + i * rect.Pitch;
			for (unsigned int j = 0; j < desc.Width; j++) {
				if (pSrcData[3] == 0)
					alphaCheckState = HasAlpha1Bit;
				else if (pSrcData[3] != 255) {
					alphaCheckState = HasAlpha;
					break;
				}
				pSrcData += 4;
			}
			if (alphaCheckState == HasAlpha)
				break;
		}
		surface->UnlockRect();
	}
	if (surface)
		surface->Release();
	return alphaCheckState;
}

AlphaCheckState GetTextureAlpha(IDirect3DDevice9 *device, void *data, unsigned int dataSize) {
	IDirect3DTexture9 *texture = nullptr;
	if (FAILED(D3DXCreateTextureFromFileInMemoryEx(device, data, dataSize, D3DX_DEFAULT, D3DX_DEFAULT, 0, D3DUSAGE_DYNAMIC,
		D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM, D3DX_FILTER_TRIANGLE | D3DX_FILTER_DITHER, D3DX_FILTER_BOX, 0, NULL, NULL, &texture)))
	{
		return AlphaCheckState::NoAlpha;
	}
	AlphaCheckState alphaCheckState = GetTextureAlpha(texture);
	texture->Release();
	return alphaCheckState;
}

AlphaCheckState GetTextureAlpha(IDirect3DDevice9 *device, wchar_t const *filename) {
	IDirect3DTexture9 *texture = nullptr;
	if (FAILED(D3DXCreateTextureFromFileExW(device, filename, D3DX_DEFAULT, D3DX_DEFAULT, 0, D3DUSAGE_DYNAMIC,
		D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM, D3DX_FILTER_TRIANGLE | D3DX_FILTER_DITHER, D3DX_FILTER_BOX, 0, NULL, NULL, &texture)))
	{
		return AlphaCheckState::NoAlpha;
	}
	AlphaCheckState alphaCheckState = GetTextureAlpha(texture);
	texture->Release();
	return alphaCheckState;
}

bool FormatHasAlpha(D3DFORMAT format) {
	switch (format) {
	case D3DFMT_A1:
	case D3DFMT_A4L4:
	case D3DFMT_A8:
	case D3DFMT_A8L8:
	case D3DFMT_A8P8:
	case D3DFMT_A4R4G4B4:
	case D3DFMT_A1R5G5B5:
	case D3DFMT_A8R3G3B2:
	case D3DFMT_DXT1:
	case D3DFMT_DXT2:
	case D3DFMT_DXT3:
	case D3DFMT_DXT4:
	case D3DFMT_DXT5:
	case D3DFMT_A8B8G8R8:
	case D3DFMT_A8R8G8B8:
	case D3DFMT_A2B10G10R10:
	case D3DFMT_A2R10G10B10:
	case D3DFMT_A16B16G16R16:
	case D3DFMT_A16B16G16R16F:
	case D3DFMT_A32B32G32R32F:
	case D3DFMT_P8:
		return true;
	}
	return false;
}

unsigned int FormatFromAlphaState(AlphaCheckState alphaCheckState, unsigned int autoFormat) {
	if (alphaCheckState == HasAlpha) {
		if (autoFormat == unsigned int(-4))
			return D3DFMT_DXT5;
		else if (autoFormat == unsigned int(-6))
			return D3DFMT_A4R4G4B4;
	}
	else if (alphaCheckState == HasAlpha1Bit) {
		if (autoFormat == unsigned int(-4))
			return D3DFMT_DXT1;
		else if (autoFormat == unsigned int(-6))
			return D3DFMT_A1R5G5B5;
	}
	else {
		if (autoFormat == unsigned int(-4))
			return D3DFMT_DXT1;
		else if (autoFormat == unsigned int(-6))
			return D3DFMT_R5G6B5;
	}
	return D3DFMT_A8R8G8B8;
}

void ea::FshImage::Load(LoadingInfo const &loadingInfo, unsigned int d3dformat, unsigned int levels, bool rescale, bool forceAlphaCheck) {
	RemoveAllDatas(FshData::PIXELDATA);
	IDirect3DTexture9 *texture = nullptr;
	D3DSURFACE_DESC desc;
	if (loadingInfo.data) {
		struct DDS_HEADER {
			DWORD           dwSize;
			DWORD           dwFlags;
			DWORD           dwHeight;
			DWORD           dwWidth;
			DWORD           dwPitchOrLinearSize;
			DWORD           dwDepth;
			DWORD           dwMipMapCount;
			DWORD           dwReserved1[11];
			struct DDS_PIXELFORMAT {
				DWORD dwSize;
				DWORD dwFlags;
				DWORD dwFourCC;
				DWORD dwRGBBitCount;
				DWORD dwRBitMask;
				DWORD dwGBitMask;
				DWORD dwBBitMask;
				DWORD dwABitMask;
			} ddspf;
			DWORD           dwCaps;
			DWORD           dwCaps2;
			DWORD           dwCaps3;
			DWORD           dwCaps4;
			DWORD           dwReserved2;
		} ddsHeader;
		static_assert(sizeof(DDS_HEADER) == 124, "Invalid size of DDS header");
		memset(&ddsHeader, 0, sizeof(DDS_HEADER));
		unsigned int dataSize = loadingInfo.dataWidth * loadingInfo.dataHeight;
		unsigned int ddsFourcc = 0x20534444;
		ddsHeader.dwSize = sizeof(DDS_HEADER);
		ddsHeader.dwFlags = 0x1 | 0x2 | 0x4 | 0x8 | 0x1000;
		ddsHeader.dwHeight = loadingInfo.dataHeight;
		ddsHeader.dwWidth = loadingInfo.dataWidth;
		ddsHeader.dwCaps = 0x1000;
		ddsHeader.ddspf.dwSize = sizeof(DDS_HEADER::DDS_PIXELFORMAT);
		switch (loadingInfo.dataFormat) {
		case  D3DFMT_A8R8G8B8:
			ddsHeader.ddspf.dwFlags = 0x1 | 0x40;
			ddsHeader.ddspf.dwRGBBitCount = 32;
			ddsHeader.ddspf.dwRBitMask = 0x00ff0000;
			ddsHeader.ddspf.dwGBitMask = 0x0000ff00;
			ddsHeader.ddspf.dwBBitMask = 0x000000ff;
			ddsHeader.ddspf.dwABitMask = 0xff000000;
			break;
		case D3DFMT_A8B8G8R8:
			ddsHeader.ddspf.dwFlags = 0x1 | 0x40;
			ddsHeader.ddspf.dwRGBBitCount = 32;
			ddsHeader.ddspf.dwRBitMask = 0x000000ff;
			ddsHeader.ddspf.dwGBitMask = 0x0000ff00;
			ddsHeader.ddspf.dwBBitMask = 0x00ff0000;
			ddsHeader.ddspf.dwABitMask = 0xff000000;
			break;
		case D3DFMT_R5G6B5:
			ddsHeader.ddspf.dwFlags = 0x40;
			ddsHeader.ddspf.dwRGBBitCount = 16;
			ddsHeader.ddspf.dwRBitMask = 0xf800;
			ddsHeader.ddspf.dwGBitMask = 0x7e00;
			ddsHeader.ddspf.dwBBitMask = 0x001f;
			break;
		case D3DFMT_L8:
			ddsHeader.ddspf.dwFlags = 0x20000;
			ddsHeader.ddspf.dwRGBBitCount = 8;
			ddsHeader.ddspf.dwRBitMask = 0xff;
			break;
		default:
			throw Exception("FshImage::Load: unknown format in LoadingInfo");
			break;
		}
		ddsHeader.dwPitchOrLinearSize = (ddsHeader.dwWidth * ddsHeader.ddspf.dwRGBBitCount + 7) / 8;
		unsigned int ddsDataSize = dataSize + sizeof(DDS_HEADER) + 4;
		unsigned char *ddsData = new unsigned char[ddsDataSize];
		memcpy(ddsData, &ddsFourcc, 4);
		memcpy(ddsData + 4, &ddsHeader, sizeof(DDS_HEADER));
		memcpy(ddsData + 4 + sizeof(DDS_HEADER), loadingInfo.data, dataSize);
		D3DXIMAGE_INFO imageInfo;
		if (FAILED(D3DXGetImageInfoFromFileInMemory(ddsData, ddsDataSize, &imageInfo))) {
			delete[] ddsData;
			throw Exception("FshImage::Load: unable to get image info from memory");
		}
		if (d3dformat == unsigned int(-4) || d3dformat == unsigned int(-5) || d3dformat == unsigned int(-6)) {
			AlphaCheckState alphaCheckState = FormatHasAlpha(imageInfo.Format) ? HasAlpha : NoAlpha;
			if (forceAlphaCheck && alphaCheckState == HasAlpha)
				alphaCheckState = GetTextureAlpha(Fsh::GlobalDevice->Interface(), ddsData, ddsDataSize);
			d3dformat = FormatFromAlphaState(alphaCheckState, d3dformat);
		}
		else if (d3dformat == D3DFMT_FROM_FILE) {
			unsigned char testFormat = GetPixelFormat(imageInfo.Format);
			if (testFormat == 0 || testFormat == FshPixelData::PIXEL_888)
				d3dformat = D3DFMT_A8R8G8B8;
		}
		if (FAILED(D3DXCreateTextureFromFileInMemoryEx(Fsh::GlobalDevice->Interface(), ddsData, ddsDataSize,
			rescale ? D3DX_DEFAULT : D3DX_DEFAULT_NONPOW2, rescale ? D3DX_DEFAULT : D3DX_DEFAULT_NONPOW2,
			levels, D3DUSAGE_DYNAMIC, D3DFORMAT(d3dformat), D3DPOOL_SYSTEMMEM, D3DX_FILTER_TRIANGLE | D3DX_FILTER_DITHER, D3DX_FILTER_BOX, 0,
			NULL, NULL, &texture)))
		{
			delete[] ddsData;
			throw Exception("FshImage::Load: failed to create direct3d texture from memory");
		}
		delete[] ddsData;
		if (FAILED(texture->GetLevelDesc(0, &desc))) {
			texture->Release();
			throw Exception("FshImage::Load: failed to retrieve texture format from memory");
		}
	}
	else if (loadingInfo.fileData) {
		D3DXIMAGE_INFO imageInfo;
		if (FAILED(D3DXGetImageInfoFromFileInMemory(loadingInfo.fileData, loadingInfo.fileDataSize, &imageInfo)))
			throw Exception("FshImage::Load: unable to get image info from file in memory");
		if (d3dformat == unsigned int(-4) || d3dformat == unsigned int(-5) || d3dformat == unsigned int(-6)) {
			AlphaCheckState alphaCheckState = FormatHasAlpha(imageInfo.Format) ? HasAlpha : NoAlpha;
			if (forceAlphaCheck && alphaCheckState == HasAlpha)
				alphaCheckState = GetTextureAlpha(Fsh::GlobalDevice->Interface(), loadingInfo.fileData, loadingInfo.fileDataSize);
			d3dformat = FormatFromAlphaState(alphaCheckState, d3dformat);
		}
		else if (d3dformat == D3DFMT_FROM_FILE) {
			unsigned char testFormat = GetPixelFormat(imageInfo.Format);
			if (testFormat == 0 || testFormat == FshPixelData::PIXEL_888)
				d3dformat = D3DFMT_A8R8G8B8;
		}
		if (FAILED(D3DXCreateTextureFromFileInMemoryEx(Fsh::GlobalDevice->Interface(), loadingInfo.fileData, loadingInfo.fileDataSize,
			rescale ? D3DX_DEFAULT : D3DX_DEFAULT_NONPOW2, rescale ? D3DX_DEFAULT : D3DX_DEFAULT_NONPOW2,
			levels, D3DUSAGE_DYNAMIC, D3DFORMAT(d3dformat), D3DPOOL_SYSTEMMEM, D3DX_FILTER_TRIANGLE | D3DX_FILTER_DITHER, D3DX_FILTER_BOX, 0,
			NULL, NULL, &texture)))
		{
			throw Exception("FshImage::Load: failed to create direct3d texture from file in memory");
		}
		if (FAILED(texture->GetLevelDesc(0, &desc))) {
			texture->Release();
			throw Exception("FshImage::Load: failed to retrieve texture format from file in memory");
		}
	}
	else if (loadingInfo.fileExists) {
		D3DXIMAGE_INFO imageInfo;
		if (FAILED(D3DXGetImageInfoFromFileW(loadingInfo.filepath.c_str(), &imageInfo)))
			throw Exception("FshImage::Load: unable to get image info from file");
		if (d3dformat == unsigned int(-4) || d3dformat == unsigned int(-5) || d3dformat == unsigned int(-6)) {
			AlphaCheckState alphaCheckState = FormatHasAlpha(imageInfo.Format) ? HasAlpha : NoAlpha;
			if (forceAlphaCheck && alphaCheckState == HasAlpha)
				alphaCheckState = GetTextureAlpha(Fsh::GlobalDevice->Interface(), loadingInfo.filepath.c_str());
			d3dformat = FormatFromAlphaState(alphaCheckState, d3dformat);
		}
		else if (d3dformat == D3DFMT_FROM_FILE) {
			unsigned char testFormat = GetPixelFormat(imageInfo.Format);
			if (testFormat == 0 || testFormat == FshPixelData::PIXEL_888)
				d3dformat = D3DFMT_A8R8G8B8;
		}
		if (FAILED(D3DXCreateTextureFromFileExW(Fsh::GlobalDevice->Interface(), loadingInfo.filepath.c_str(),
			rescale ? D3DX_DEFAULT : D3DX_DEFAULT_NONPOW2, rescale ? D3DX_DEFAULT : D3DX_DEFAULT_NONPOW2,
			levels, D3DUSAGE_DYNAMIC, D3DFORMAT(d3dformat), D3DPOOL_SYSTEMMEM, D3DX_FILTER_TRIANGLE | D3DX_FILTER_DITHER, D3DX_FILTER_BOX, 0,
			NULL, NULL, &texture)))
		{
			throw Exception("FshImage::Load: failed to create direct3d texture");
		}
		if (FAILED(texture->GetLevelDesc(0, &desc))) {
			texture->Release();
			throw Exception("FshImage::Load: failed to retrieve texture format");
		}
	}
	else
		throw Exception("FshImage::Load: Empty LoadingInfo");
	unsigned char format = GetPixelFormat(desc.Format);
	if (format == 0) {
		texture->Release();
		throw Exception(FormatStatic("FshImage::Load: unsupported texture format (input format: %d, result format: %d)", d3dformat, desc.Format));
	}
	unsigned char numLevels = (unsigned char)texture->GetLevelCount();
	size_t pixelsSize = 0;
	unsigned short w = desc.Width;
	unsigned short h = desc.Height;
	for (unsigned int i = 0; i < numLevels; i++) {
		pixelsSize += GetPixelDataSize(w, h, format);
		w /= 2;
		h /= 2;
	}
	Buffer pixels;
	pixels.Allocate(pixelsSize);
	w = desc.Width;
	h = desc.Height;
	unsigned char *pixelsPtr = (unsigned char *)pixels.GetData();
	for (unsigned int i = 0; i < numLevels; i++) {
		size_t pixelsDataSize = GetPixelDataSize(w, h, format);
		D3DLOCKED_RECT rect;
		if (FAILED(texture->LockRect(i, &rect, NULL, D3DLOCK_READONLY))) {
			texture->Release();
			throw Exception("FshImage::Load: failed to lock texture");
		}
		if (desc.Format == D3DFMT_X8R8G8B8) {
			clr_x8r8g8b8 *xrgb = (clr_x8r8g8b8 *)rect.pBits;
			clr_b8g8r8 *bgr = (clr_b8g8r8 *)pixelsPtr;
			unsigned int numPixels = w * h;
			for (unsigned int p = 0; p < numPixels; p++) {
				bgr[p].r = xrgb[p].r;
				bgr[p].g = xrgb[p].g;
				bgr[p].b = xrgb[p].b;
			}
		}
		else
			memcpy(pixelsPtr, rect.pBits, pixelsDataSize);
		if (FAILED(texture->UnlockRect(i))) {
			texture->Release();
			throw Exception("FshImage::Load: failed to unlock texture");
		}
		pixelsPtr = &pixelsPtr[pixelsDataSize];
		w /= 2;
		h /= 2;
	}
	AddData(new FshPixelData(format, pixels, desc.Width, desc.Height, (unsigned char)texture->GetLevelCount() - 1, 0, 0, 0, 0, 0));
	texture->Release();
}

ea::FshData::DataType ea::FshPixelData::GetDataType() const { return FshData::PIXELDATA; }

size_t ea::FshPixelData::GetDataSize() const { return 12 + mPixels.GetSize(); }

ea::FshPixelData::FshPixelData(unsigned char format, Buffer const & pixels, unsigned short width, unsigned short height, unsigned char numMipLevels, unsigned short centerX, unsigned short centerY, unsigned short left, unsigned short top, unsigned char flags) {
    mFormat = format; mPixels = pixels; mHeight = height; mWidth = width; mNumMipLevels = numMipLevels; mCenterX = centerX; mCenterY = centerY; mLeft = left; mTop = top; mFlags = flags;
}

ea::FshPixelData::FshPixelData(unsigned char format, Buffer && pixels, unsigned short width, unsigned short height, unsigned char numMipLevels, unsigned short centerX, unsigned short centerY, unsigned short left, unsigned short top, unsigned char flags) {
	mFormat = format; mPixels = std::move(pixels); mHeight = height; mWidth = width; mNumMipLevels = numMipLevels; mCenterX = centerX; mCenterY = centerY; mLeft = left; mTop = top; mFlags = flags;
}

ea::Buffer & ea::FshPixelData::Pixels() { return mPixels; }

unsigned char ea::FshPixelData::GetFormat() const { return mFormat; }

void ea::FshPixelData::SetFormat(unsigned char format) { mFormat = format; }

unsigned short ea::FshPixelData::GetWidth() const { return mWidth; }

void ea::FshPixelData::SetWidth(unsigned short width) { mWidth = width; }

unsigned short ea::FshPixelData::GetHeight() const { return mHeight; }

void ea::FshPixelData::SetHeight(unsigned short height) { mHeight = height; }

unsigned short ea::FshPixelData::GetCenterX() const { return mCenterX; }

void ea::FshPixelData::SetCenterX(unsigned short centerX) { mCenterX = centerX; }

unsigned short ea::FshPixelData::GetCenterY() const { return mCenterY; }

void ea::FshPixelData::SetCenterY(unsigned short centerY) { mCenterY = centerY; }

unsigned short ea::FshPixelData::GetLeft() const { return mLeft; }

void ea::FshPixelData::SetLeft(unsigned short left) { mLeft = left; }

unsigned short ea::FshPixelData::GetTop() const { return mTop; }

void ea::FshPixelData::SetTop(unsigned short top) { mTop = top; }

unsigned char ea::FshPixelData::GetNumMipLevels() const { return mNumMipLevels; }

void ea::FshPixelData::SetNumMipLevels(unsigned char numMipLevels) { mNumMipLevels = numMipLevels; }

unsigned char ea::FshPixelData::GetFlags() const { return mFlags; }

void ea::FshPixelData::SetFlags(unsigned char flags) { mFlags = flags; }

ea::FshHotSpot::Region::Region() {}

ea::FshHotSpot::Region::Region(unsigned int id, unsigned int left, unsigned int top, unsigned int width, unsigned int height, unsigned int unknown) {
	mId = id; mUnknown = unknown; mLeft = left; mTop = top; mWidth = width; mHeight = height;
}

void ea::Fsh::SetDevice(D3DDevice *device) {
	GlobalDevice = device;
}

void ea::Fsh::ClearDevice() {
	GlobalDevice = nullptr;
}

std::string ea::Fsh::GetTag() const { return std::string(mTag, 4); }

void ea::Fsh::SetTag(std::string const & tag) { strncpy(mTag, tag.c_str(), 4); }

bool ea::Fsh::GetAddBuyERTS() const { return mWritingOptions.mAddBuyERTS; }

void ea::Fsh::SetAddBuyERTS(bool set) { mWritingOptions.mAddBuyERTS = set; }

size_t ea::Fsh::GetAlignment() const { return mWritingOptions.mAlignment; }

void ea::Fsh::SetAlignment(size_t alignment) { mWritingOptions.mAlignment = alignment; }

void ea::Fsh::Clear() { mImages.clear(); }

ea::FshImage & ea::Fsh::AddImage() { return mImages.emplace_back(FshImage()); }

size_t ea::Fsh::GetImagesCount() { return mImages.size(); }

void ea::Fsh::ForAllImages(std::function<void(FshImage&)> callback) {
	for (auto &i : mImages)
		callback(i);
}

void ea::Fsh::Read(std::filesystem::path const & filepath) {
	File f(filepath, File::READ);
	auto signature = f.Read<unsigned int>();
	f.Read<unsigned int>();
	if (signature != 'IPHS')
		throw Exception("not a correct shape file (" + filepath.string() + ")");
	auto numImages = f.Read<size_t>();
	f.Read(mTag, 4);
	bool doBuyErtsCheck = false;
	std::vector<size_t> offsets;
	if (numImages > 0) {
		mImages.resize(numImages);
		offsets.resize(numImages);
		for (size_t i = 0; i < numImages; i++) {
			f.Read(mImages[i].mTag, 4);
			offsets[i] = f.Read<size_t>();
		}
		doBuyErtsCheck = ((static_cast<size_t>(f.Position()) + 8) <= offsets.front())
			&& ((f.Position() + 8) <= f.FileSize());
	}
	else
		doBuyErtsCheck = (f.Position() + 8) <= f.FileSize();
	if (doBuyErtsCheck) {
		char buyErtsCheckData[8] = {};
		f.Read(buyErtsCheckData, 8);
		if (!strncmp(buyErtsCheckData, "Buy ERTS", 8))
			mWritingOptions.mAddBuyERTS = true;
	}
	for (size_t i = 0; i < numImages; i++) {
		size_t currentSectionPosition = offsets[i];
		FshImage &image = mImages[i];
		while (1) {
			f.JumpTo(currentSectionPosition);
			auto sectionHeader = f.Read<unsigned int>();
			unsigned char sectionId = sectionHeader & 0xFF;
			unsigned int nextSectionOffset = (sectionHeader >> 8) & 0xFFFFFF;
			size_t sectionSize = 0;
			if (nextSectionOffset > 0) {
				if (nextSectionOffset > 4)
					sectionSize = nextSectionOffset - 4;
			}
			else {
				if (i == (numImages - 1)) {
					size_t distance = sectionSize = f.FileSize() - currentSectionPosition;
					if (distance > 4)
						sectionSize = distance - 4;
				}
				else {
					size_t distance = sectionSize = offsets[i + 1] - currentSectionPosition;
					if (distance > 4)
						sectionSize = distance - 4;
				}
			}
			switch (sectionId) {
			case 0x6F: {
				auto commentSize = f.Read<size_t>();
				std::string commentStr;
				if (commentSize > 0) {
					commentStr.resize(commentSize - 1);
					f.Read(&commentStr[0], commentSize - 1);
				}
				image.AddData(new FshComment(commentStr));
			} break;
			case 0x70: {
				std::string nameStr = f.ReadNullTerminated();
				image.AddData(new FshName(nameStr));
			} break;
			case 0x69: {
				auto binSize = f.Read<unsigned short>();
				auto flags = f.Read<unsigned short>();
				auto unknown1 = f.Read<unsigned int>();
				auto unknown2 = f.Read<unsigned int>();
				Buffer buffer;
				if (binSize > 0) {
					buffer.Allocate(binSize);
					f.Read(buffer.GetData(), binSize);
				}
				image.AddData(new FshMetalBin(std::move(buffer), flags, unknown1, unknown2));
			} break;
			case 0x7C: {
				auto numIntPairs = f.Read<size_t>();
				if (numIntPairs > 0) {
					if (numIntPairs % 3)
						throw Exception("unsupported hotspot section format in " + filepath.string());
					auto &regions = image.AddData(new FshHotSpot())->As<FshHotSpot>()->Regions();
					size_t numRegions = numIntPairs / 3;
					regions.resize(numRegions);
					for (size_t r = 0; r < numRegions; r++) {
						auto &region = regions[r];
						f.Read(region.mId);
						f.Read(region.mUnknown);
						f.Read(region.mLeft);
						f.Read(region.mTop);
						f.Read(region.mWidth);
						f.Read(region.mHeight);
					}
				}
				else
					image.AddData(new FshHotSpot());
			} break;
			default: {
				if ((sectionId & 0x80) == 0) {
					auto width = f.Read<unsigned short>();
					auto height = f.Read<unsigned short>();
					auto centerX = f.Read<unsigned short>();
					auto centerY = f.Read<unsigned short>();
					auto packedValues = f.Read<unsigned int>();
					unsigned short left = packedValues & 0xFFF;
					unsigned char flags = (packedValues >> 12) & 0xF;
					unsigned short top = (packedValues >> 16) & 0xFFF;
					unsigned char numMipLevels = (packedValues >> 28) & 0xF;
					Buffer pixels;
					if (sectionSize > 16) {
						pixels.Allocate(sectionSize - 12);
						f.Read(pixels.GetData(), sectionSize - 12);
					}
					image.AddData(new FshPixelData(sectionId, std::move(pixels), width, height, numMipLevels, centerX, centerY, left, top, flags));
				}
				else {
					Buffer buffer;
					if (sectionSize > 0) {
						buffer.Allocate(sectionSize);
						f.Read(buffer.GetData(), sectionSize);
					}
					image.AddData(new FshUnknown(sectionId, std::move(buffer)));
				}
			} break;
			}
			if (nextSectionOffset == 0)
				break;
			else
				currentSectionPosition += nextSectionOffset;
		}
	}
}

void ea::Fsh::Write(std::filesystem::path const & filepath) {
	File f(filepath, File::WRITE);
	size_t fileHeaderSizeNotAligned = 16 + mImages.size() * 8;
	size_t fileHeaderSize = File::GetAlignedSize(fileHeaderSizeNotAligned, 16);
	size_t totalFileSize = fileHeaderSize;
	std::vector<size_t> sectionSizes(mImages.size(), 0);
	for (size_t i = 0; i < mImages.size(); i++) {
		unsigned int sectionSize = 0;
		for (auto s : mImages[i].mDatas)
			sectionSize += File::GetAlignedSize(4 + s->GetDataSize(), mWritingOptions.mAlignment);
		sectionSizes[i] = sectionSize;
		totalFileSize += sectionSize;
	}
	f.Write("SHPI", 4);
	f.Write(totalFileSize);
	f.Write(mImages.size());
	f.Write(mTag, 4);
	size_t imageOffset = fileHeaderSize;
	for (size_t i = 0; i < mImages.size(); i++) {
		f.Write(mImages[i].mTag, 4);
		f.Write(imageOffset);
		imageOffset += sectionSizes[i];
	}
	if (fileHeaderSize != fileHeaderSizeNotAligned) {
		size_t alignmentSize = fileHeaderSize - fileHeaderSizeNotAligned;
		if (mWritingOptions.mAddBuyERTS && alignmentSize >= 8) {
			f.Write("Buy ERTS", 8);
			if (mWritingOptions.mAddBuyERTS && alignmentSize > 8)
				f.WriteNull(alignmentSize - 8);
		}
		else
			f.WriteNull(alignmentSize);
	}
	for (size_t i = 0; i < mImages.size(); i++) {
		for (size_t d = 0; d < mImages[i].mDatas.size(); d++) {
			auto const &data = mImages[i].mDatas[d];
			unsigned int dataSize = data->GetDataSize();
			unsigned int sectionSize = dataSize + 4;
			unsigned int sectionAlignment = File::Alignment(sectionSize, mWritingOptions.mAlignment);
			unsigned int totalSectionSize = sectionSize + sectionAlignment;
			unsigned char sectionId = 0;
			switch (data->GetDataType()) {
			case FshData::COMMENT:
				sectionId = 0x6F;
				break;
			case FshData::NAME:
				sectionId = 0x70;
				break;
			case FshData::METALBIN:
				sectionId = 0x69;
				break;
			case FshData::HOTSPOT:
				sectionId = 0x7C;
				break;
			case FshData::PIXELDATA:
				sectionId = data->As<FshPixelData>()->GetFormat();
				break;
			case FshData::UNKNOWN:
				sectionId = data->As<FshUnknown>()->GetId();
				break;
			}
			unsigned int nextSectionOffset = (d == mImages[i].mDatas.size() - 1) ? 0 : totalSectionSize;
			f.Write(sectionId | (nextSectionOffset << 8));
			switch (data->GetDataType()) {
			case FshData::COMMENT: {
				size_t commentSize = data->As<FshComment>()->GetComment().size() + 1;
				f.Write(commentSize);
				f.Write(data->As<FshComment>()->GetComment().c_str(), commentSize);
			} break;
			case FshData::NAME: {
				f.Write(data->As<FshName>()->GetName().c_str(), data->As<FshName>()->GetName().length() + 1);
			} break;
			case FshData::METALBIN: {
				unsigned short binSize = static_cast<unsigned short>(data->As<FshMetalBin>()->Buffer().GetSize());
				f.Write(binSize);
				f.Write(data->As<FshMetalBin>()->GetFlags());
				f.Write(data->As<FshMetalBin>()->GetUnknown1());
				f.Write(data->As<FshMetalBin>()->GetUnknown2());
				if (binSize > 0)
					f.Write(data->As<FshMetalBin>()->Buffer().GetData(), binSize);
			} break;
			case FshData::HOTSPOT: {
				f.Write(data->As<FshHotSpot>()->Regions().size() * 3);
				for (auto const &r : data->As<FshHotSpot>()->Regions()) {
					f.Write(r.mId);
					f.Write(r.mUnknown);
					f.Write(r.mLeft);
					f.Write(r.mTop);
					f.Write(r.mWidth);
					f.Write(r.mHeight);
				}
			} break;
			case FshData::PIXELDATA: {
				f.Write(data->As<FshPixelData>()->GetWidth());
				f.Write(data->As<FshPixelData>()->GetHeight());
				f.Write(data->As<FshPixelData>()->GetCenterX());
				f.Write(data->As<FshPixelData>()->GetCenterY());
				f.Write((data->As<FshPixelData>()->GetLeft() & 0xFFF)
					| ((data->As<FshPixelData>()->GetFlags() & 0xF) << 12)
					| ((data->As<FshPixelData>()->GetTop() & 0xFFF) << 16)
					| ((data->As<FshPixelData>()->GetNumMipLevels() & 0xF) << 28));
				if (data->As<FshPixelData>()->Pixels().GetSize() > 0)
					f.Write(data->As<FshPixelData>()->Pixels().GetData(), data->As<FshPixelData>()->Pixels().GetSize());
			} break;
			case FshData::UNKNOWN: {
				f.Write(data->As<FshUnknown>()->Buffer().GetData(), data->As<FshUnknown>()->Buffer().GetSize());
			} break;
			}
			if (sectionAlignment > 0)
				f.WriteNull(sectionAlignment);
		}
	}
}
