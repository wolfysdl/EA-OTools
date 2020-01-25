#include "D3DDevice.h"
#include "d3d9.h"

ea::D3DDevice::D3DDevice() {
	auto mDirect3D = Direct3DCreate9(D3D_SDK_VERSION);
	if (!mDirect3D)
		throw Exception("D3DDevice: failed to create direct3d");
	static D3DPRESENT_PARAMETERS d3dpp;
	ZeroMemory(&d3dpp, sizeof(d3dpp));
	d3dpp.Windowed = TRUE;
	d3dpp.SwapEffect = D3DSWAPEFFECT_COPY;
	IDirect3DDevice9 *device = nullptr;
	if (FAILED(mDirect3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, GetConsoleWindow(), D3DCREATE_SOFTWARE_VERTEXPROCESSING, &d3dpp, &mDevice))) {
		mDirect3D->Release();
		mDirect3D = nullptr;
		throw Exception("D3DDevice: failed to create direct3d device");
	}
}

ea::D3DDevice::~D3DDevice() {
	if (mDevice)
		mDevice->Release();
	if (mDirect3D)
		mDirect3D->Release();
}

IDirect3DDevice9 * ea::D3DDevice::Interface() { return mDevice; }
