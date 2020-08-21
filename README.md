**otools** - tools for importing/exporting EA Sports model format (.o)

**O** is a file extension in EA Sports games which is used for model and animation files. The file itself has an ELF structure.

The file may contain model data, skeleton information, animations, rendering information and shaders.

**O** files were used mostly in games developed by EA Vancouver, between 1999 and 2009.

![](https://i.imgur.com/KIF4gzwm.png) ![](https://i.imgur.com/WtbqZAwm.png) ![](https://i.imgur.com/tzKCIWkm.png)

## Current progress and limitations

✅ General model/mesh

✅ Skeleton and skinning

✅ Textures

❌ Morphing and shapekeys

❌ Animations

❌ Shaders and states

**By game:**

✅ FIFA series (FIFA 2003 ... FIFA 10)

🟡 FIFA Manager series (only FIFA Manager 13)

🟡 Cricket series (only Cricker 07; game-specific features are not implemented)

🟡 NHL series (only NHL 2004; some game-specific features are not implemented)

❌ NBA series

❌ MLB series

❌ Rugby series

❌ Need For Speed series

❌ Def Jam FFNY

**By platform:**

✅ PC

❌ PlayStation 2

❌ XBOX

## Overview

otools is a command-line tool (but release also includes GUI version). First argument is operation type (export or import), second argument (`-i`) is input file/folder path. All other arguments are optional, but it's recommended to set the source/target game with `-game` option.

**Usage examples:**

Extract all .o models in "test" folder to .gltf format
```
otools export -i "test" -game fifa07
```
Extract test.o to test.gltf
```
otools export -i "test.o" -game fifa07
```
Import all models (.gltf, .glb, .fbx, .dae, .obj, .3ds) in "test" folder to .o format
```
otools import -i "test" -game fifa07
```
Import test.gltf to test.o
```
otools import -i "test.gltf" -game fifa07
```

Possible operations:

`import` - converts models (.gltf, .glb, .fbx, .dae, .obj, .3ds) to .o or .ord/.orl format

`export` - converts .o, .ord/.orl files to .gltf format

`version` - outputs otools version and returns integer number of otools version

`dump` - creates a dump text file with information from .o, .ord/.orl files

`dumpshaders` - creates a dump file for shaders which are used in .o, .ord/.orl files

`unpackfsh` - extracts textures from .fsh file

`packfsh` - creates .fsh file from textures

Additional options:

`-o <output path>` - output filename (for single file as input) or output folder (for folder as input)

`-silent` - do not display error messages (by default, all messages are displayed as message boxes)

`-console` - print error messages in console window instead of message boxes

`-createSubDir` - create sub-directory for output file

`-recursive` - scan subfolders (when input is directory)

`-hwnd` - sets specific window handle (might be needed for creating D3D Device). This option is used in OTools_GUI application to pass window handle of GUI application into console application

Additional export options:

`-noTextures` - do not attach textures to materials (a temporary fix for inbuilt .gltf preview in Windows which can't display models with images in external files)

`-dummyTextures` - generate and write dummy 4x4 white textures in .png

`-jpegTextures` - dummy textures will be written in .jpeg instead of .png

`-noMeshJoin` - do not merge same materials to one material

`-keepTex0InMatOptions` - keep `tex` option in material name for texture slot with index 0 (`tex0`). By default, `tex` option is added only for texture slot indices higher than 0 

`-skeleton <filePath>` - skeleton in external file (.o)

Additional import options:

`-game <gameId>` - set the source/target game. Currently implemented games are: `fifa2003`, `fifa2004`, `fifa2005`, `fifa06`, `fifa07`, `fifa08`, `fifa09`, `fifa10`, `euro2004`, `euro2008`, `wc2006`, `cl0405`, `cl0607`, `fm13`, `cricket07`

`-scale <scaling>` - scale model by given floating-point factor

`-translate <x,y,z>` - moves the model

`-defaultVCol <color>` - default vertex color (could be RGB or RGBA in hexadecimal format, for example: `7F7F7FFF`). This color will be used when mesh in source model has no vertex colors. Default value is `7F7F7FFF`

`-vColScale <scale>` - vertex color scale (floating-point value from 0.0 to 1.0). This value is used only when mesh in source model has vertex colors. Ignored when set to 0.0. Default value is 0.0

`-setVCol <color>` - replace vertex color with new color. `defaultVCol` and `vColScale` are ignored when this option is used

`-tristrip` - convert geometry to tri-strips

`-swapYZ` - swap Y and Z axis

`-forceLighting` - force using shaders for dynamic lighting on models

`-genTexNames` - generate new 4-character names for textures which names exceeds the name length limit

`-writeFsh` - write model textures to .fsh file

`-fshOutput` - output file/folder for .fsh files (by default, an output folder for model is used)

`-fshLevels <level count>` - levels (mipmaps) count for .fsh images. When set to -1 ot 0, the count will be taken from file. When lower than -1 or greater than 13, a full mipmap chain will be generated. Full mipmap chain generation option is used by default

`-fshFormat <format>` - pixel format for .fsh images. Supported formats are: `rgb` (also `rgba`, `rgb32`, `rgba32`), `rgb16` (also `rgba16`), `dxt`, `auto`, `8888`, `888`, `dxt1`, `dxt3`, `dxt5`, `4444`, `5551`, `565`. `auto` option is used to detect format from the file. `rgb` options are used to select format depending on image transparency. `dxt` option is used to select `dxt1` or `dxt5` depending on image transparency. `dxt` option is used by default

`-fshRescale` - rescale .fsh images to power-of-two size

`-fshTextures <image names list>` - a list of comma-separated names of images which should be packed into .fsh. Images which are referenced by the model but not present in this list, will be ignored when writing to .fsh

`-fshAddTextures <image names list>` - a list of comma-separated names of images which should be additionally packed into .fsh. This option is used to add images which are not referenced by the model

`-fshDisableTextureIgnore` - disable any texture ignorance when writing to .fsh (some textures like "adba", "rwa0" are ignored by default)

`-fshIgnoreTextures <image names list>` - a list of comma-separated names of images which should be ignored when writing to .fsh

`-preTransformVertices` - pre-transform all vertices to global space

`-sortByName` - sort nodes by their names

`-sortByAlpha` - sort nodes by transparency. Can be also used with `-sortByName`

`-ignoreMatColor` - ignore material color (by default, material color will be multiplied with vertex color and stored as vertex color)

`-startsWith` - filter files by prefix (when `-i` argument is folder)

`-pad <size>` - pad resulting .o files to the specified size (in bytes). This option is ignored if the size of resulting file is greater than the specified value.

`-padFsh <size>` - pad resulting .fsh files to the specified size (in bytes). This option is ignored if the size of resulting file is greater than the specified value

`-ignoreEmbeddedTextures` - ignore any embedded textures in the input model

`-ord` - import to .ord and .orl files

`-computationIndex <index>` - set specific computation index. Default value is 2

`-forceShader <shaderName>` - set specific shader to be used on all meshes

`-boneRemap <filePath>` - file with bone remap configuration

`-skeletonData <filePath>` - file with skeleton data. Can be "none" if skeleton should not be written to .o file

`-bonesFile <filePath>` - file with bone definitions

`-maxBonesPerVertex <count>` - a limit for bones per vertex (3 by default)

`-vertexWeightPaletteSize <size>` - a limit for unique vertex weight values

`-noMetadata` - do not write metadata

`-conformant` - do not write OTools comments (also disables metadata)

FSH packing options:

`-fshWriteToParentDir` - save .fsh in parent directory

When packing .fsh files, FSH-related options like `-fshLevels`, `-fshFormat`, `-fshRescale`, `-padFsh` can be used.

FSH unpacking options:

`-fshUnpackImageFormat` - image format for unpacked textures. Possible formats: `png`, `bmp`, `tga`, `dds`, `jpg`

`-fshHash <hash>` - hash value (autogenerated by default)

`-fshId <id>` - id value (1 by default)

## Requirements

* command-line tool: Windows XP SP3 or higher, 32-bit/64-bit; DirectX 9.0c

* GUI tool: Windows 7 or higher, 32-bit/64-bit; DirectX 9.0c

## Source code dependencies

* Visual Studio 2017/2019

* MSVC v141_xp toolset

* assimp (Open Asset Import Library) in static library

* DirectX 9.0c SDK
