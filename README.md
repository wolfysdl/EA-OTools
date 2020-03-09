**otools** - tools for importing/exporting EA Sports model format (.o)

![](https://i.imgur.com/KIF4gzwm.png) ![](https://i.imgur.com/EZOPG6im.png) ![](https://i.imgur.com/ALQGlXzm.png)

**Current progress and limitations**:

* Shaders from FIFA (2003 - 2010), FIFA Manager 13 and Cricket 07 are implemented. Most of them are untested and may contain errors. Also they are not guaranteed to work with any other games

otools is a command-line tool. First argument is operation type (export or import), second argument (`-i`) is input file/folder path. All other arguments are optional.

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

Additional options:

`-o <output path>` - output filename (for single file as input) or output folder (for folder as input)

`-silent` - do not display error messages (by default, all messages are displayed as message boxes)

`-console` - print error messages in console window instead of message boxes

`-createSubDir` - create sub-directory for output file

`-recursive` - scan subfolders (when input is directory)

Additional export options:

`-noTextures` - do not attach textures to materials (a temporary fix for inbuilt .gltf preview in Windows which can't display models with images in external files)

`-dummyTextures` - generate and write dummy 4x4 white textures in .png

`-jpegTextures` - dummy textures will be written in .jpeg instead of .png

`-noMeshJoin` - do not merge same materials to one material

Additional import options:

`-game <gameId>` - set the source/target game. Currently implemented games are: `fifa2003`, `fifa2004`, `fifa2005`, `fifa06`, `fifa07`, `fifa08`, `fifa09`, `fifa10`, `euro2004`, `euro2008`, `wc2006`, `cl0405`, `cl0607`, `fm13`, `cricket07`

`-scale <scaling>` - scale model by given floating-point factor

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

**Source code dependencies**

* Visual Studio 2017/2019

* MSVC v141_xp toolset

* assimp (Open Asset Import Library) in static library

* DirectX 9.0c SDK