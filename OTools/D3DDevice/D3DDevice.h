#pragma once

struct IDirect3D9;
struct IDirect3DDevice9;

class D3DDevice {
	IDirect3D9 *mDirect3D = nullptr;
	IDirect3DDevice9 *mDevice = nullptr;
public:
	D3DDevice(unsigned int hWnd);
	~D3DDevice();
	IDirect3DDevice9 *Interface();
};
