**otools** - tools for importing/exporting EA Sports model format (.o)

![](https://i.imgur.com/KIF4gzwm.png) ![](https://i.imgur.com/EZOPG6im.png) ![](https://i.imgur.com/ALQGlXzm.png)

**Current progress and limitations**:

* Import of skinned meshes (body, heads, hair) is not implemented (only export is possible)

* Only shaders from FIFA Manager 13 are implemented, they are not guaranteed to work with any other games

otools is a command-line tools. First argument is operation type (export or import), second argument is input file/folder path.

**Usage examples:**

Extract all .o models in "test" folder to .gltf format
```
otools export "test"
```
Extract test.o to test.gltf
```
otools export "test.o"
```
Import all models (.gltf, .glb, .fbx, .dae, .obj, .3ds) in "test" folder to .o format
```
otools import "test"
```
Import test.gltf to test.o
```
otools import "test.gltf"
```

Additional options:

`-o <output path>` - output filename (for single file as input) or output folder (for folder as input)

`-silent` - do not display error messages

`-console` - print error messages in console window

`-createSubDir` - create sub-directory for output file

Additional export options:

`-noTextures` - do not attach textures to materials

`-dummyTextures` - generate and write dummy 4x4 white textures in .png

`-jpegTextures` - dummy textures will be written in .jpeg instead of .png

Additional import options:

`-defaultVCol <color>` - default vertex color (could be RGB or RGBA in hexadecimal format, for example: `7F7F7FFF`)

`-tristrip` - convert geometry to tri-strips

`-swapYZ` - swap Y and Z axis

`-forceLighting` - force using shaders for dynamic lighting on models

`-genTexNames` - generate new 4-character names for textures

`-writeFsh` - write model textures to .fsh file

`-fshOutput` - output file/folder for .fsh files (by default, an output folder for model is used)

`-fshLevels <level count>` - levels (mipmaps) count for .fsh images. When set to -1 ot 0, the count will be taken from file

`-fshFormat <format>` - pixel format for .fsh images. Supported formats are: `rgb`, `dxt`, `auto`, `8888`, `888`, `dxt1`, `dxt3`, `dxt5`, `4444`, `5551`, `565`

`-fshRescale` - rescale .fsh images to power-of-two size

**Source code dependencies**

* Visual Studio 2017/2019

* MSVC v141_xp toolset

* assimp (Open Asset Import Library) in static library

* DirectX 9.0c SDK