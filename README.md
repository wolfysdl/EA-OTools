**otools** - tools for importing/exporting EA Sports model format (.o)

**Current progress and limitations**:

* Export of skinned meshes (body, heads, hair) is not implemented

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

Additional export options:

`-noTextures` - do not attach textures to materials

`-dummyTextures` - generate and write dummy 4x4 white textures in .png

`-jpegTextures` - dummy textures will be written in .jpeg instead of .png

Additional import options:

`-defaultVCol <color>` - default vertex color (could be RGB or RGBA in hexadecimal format, for example: `7F7F7FFF`)

`-tristrip` - convert geometry to tri-strips

`-swapYZ` - swap Y and Z axis

`-forceLighting` - force using shaders for dynamic lighting on models
