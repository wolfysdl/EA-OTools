**otools** - tools for importing/exporting EA Sports model format (.o)

![](https://i.imgur.com/KIF4gzwm.png) ![](https://i.imgur.com/EZOPG6im.png) ![](https://i.imgur.com/ALQGlXzm.png)

**Current progress and limitations**:

* Import of skinned meshes (body, heads, hair) to the game is not implemented (only export is possible)

* Only shaders from FIFA Manager 13 are implemented, they are not guaranteed to work with any other games

otools is a command-line tool. First argument is operation type (export or import), second argument (`-i`) is input file/folder path. All other arguments are optional.

**Usage examples:**

Extract all .o models in "test" folder to .gltf format
```
otools export -i "test"
```
Extract test.o to test.gltf
```
otools export -i "test.o"
```
Import all models (.gltf, .glb, .fbx, .dae, .obj, .3ds) in "test" folder to .o format
```
otools import -i "test"
```
Import test.gltf to test.o
```
otools import -i "test.gltf"
```

Additional options:

`-o <output path>` - output filename (for single file as input) or output folder (for folder as input)

`-silent` - do not display error messages (by default, all messages are displayed as message boxes)

`-console` - print error messages in console window instead of message boxes

`-createSubDir` - create sub-directory for output file

Additional export options:

`-noTextures` - do not attach textures to materials (a temporary fix for inbuilt .gltf preview in Windows which can't display models with images in external files)

`-dummyTextures` - generate and write dummy 4x4 white textures in .png

`-jpegTextures` - dummy textures will be written in .jpeg instead of .png

Additional import options:

`-defaultVCol <color>` - default vertex color (could be RGB or RGBA in hexadecimal format, for example: `7F7F7FFF`). This color will be used when mesh in source model has no vertex colors. Default value is `7F7F7FFF`

`-vColScale <scale>` - vertex color scale (floating-point value from 0.0 to 1.0). This value is used only when mesh in source model has vertex colors. Ignored when set to 0.0. Default value is 0.0

`-tristrip` - convert geometry to tri-strips

`-swapYZ` - swap Y and Z axis

`-forceLighting` - force using shaders for dynamic lighting on models

`-genTexNames` - generate new 4-character names for textures

`-writeFsh` - write model textures to .fsh file

`-fshOutput` - output file/folder for .fsh files (by default, an output folder for model is used)

`-fshLevels <level count>` - levels (mipmaps) count for .fsh images. When set to -1 ot 0, the count will be taken from file. When lower than -1 or greater than 13, a full mipmap chain will be generated. Full mipmap chain generation option is used by default

`-fshFormat <format>` - pixel format for .fsh images. Supported formats are: `rgb`, `dxt`, `auto`, `8888`, `888`, `dxt1`, `dxt3`, `dxt5`, `4444`, `5551`, `565`. `auto` option is used to detect format from the file. `rgb` option is used to select `888` or `8888` depending on image transparency. `dxt` option is used to select `dxt1` or `dxt5` depending on image transparency. `dxt` option is used by default

`-fshRescale` - rescale .fsh images to power-of-two size

`-fshAddTextures <image names list>` - a list of comma-separated image names which should be additionally packed into .fsh

**Source code dependencies**

* Visual Studio 2017/2019

* MSVC v141_xp toolset

* assimp (Open Asset Import Library) in static library

* DirectX 9.0c SDK