#include "D3DDevice.h"
#include "..\D3DInclude.h"
#include <stdexcept>

using namespace std;

D3DDevice::D3DDevice(unsigned int hWnd) {
	auto mDirect3D = Direct3DCreate9(D3D_SDK_VERSION);
	if (!mDirect3D)
		throw runtime_error("D3DDevice: failed to create direct3d");
	static D3DPRESENT_PARAMETERS d3dpp;
	ZeroMemory(&d3dpp, sizeof(d3dpp));
	d3dpp.Windowed = TRUE;
	d3dpp.SwapEffect = D3DSWAPEFFECT_COPY;
	IDirect3DDevice9 *device = nullptr;
	auto devResult = mDirect3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, (HWND)hWnd, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &d3dpp, &mDevice);
	if (FAILED(devResult)) {
		mDirect3D->Release();
		mDirect3D = nullptr;
		static char errMsg[256];
		sprintf(errMsg, "D3DDevice: failed to create direct3d device (Error %X)", devResult);
		throw runtime_error(errMsg);
	}
}

D3DDevice::~D3DDevice() {
	if (mDevice)
		mDevice->Release();
	if (mDirect3D)
		mDirect3D->Release();
}

IDirect3DDevice9 * D3DDevice::Interface() { return mDevice; }
