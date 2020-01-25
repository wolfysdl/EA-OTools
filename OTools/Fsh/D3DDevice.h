#pragma once
#include "Exception.h"

struct IDirect3D9;
struct IDirect3DDevice9;

namespace ea {

	class D3DDevice {
		IDirect3D9 *mDirect3D = nullptr;
		IDirect3DDevice9 *mDevice = nullptr;
	public:
		D3DDevice();
		~D3DDevice();
		IDirect3DDevice9 *Interface();
	};
}
