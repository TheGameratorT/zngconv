# zngconv
A tool to convert ZNG files from the game Sakura Dungeon.

ZNG is a custom image container format that aims to be faster at decompressing than a PNG at the cost of size. \
The format runs LZ4 compression twice on the data.

## Credits

[@Bahncard25](https://www.github.com/Bahncard25) for the help in reverse engineering the file format. \
[nothings/stb](https://github.com/nothings/stb) for image processing. \
[lz4/lz4](https://github.com/lz4/lz4) for LZ4 compression and decompression.

## Building

### Windows
Make sure that CMake and a compatible compiler (MSVC, GCC or Clang) is installed. MSVC is recommended. \
Then run the following commands:

Example for MSVC (2022):
```bat
git clone https://github.com/TheGameratorT/zngconv.git
setup.bat
mkdir build && cd build
cmake ../ -G "Visual Studio 17 2022" -A x64
cmake --build . --config Release
```
Note: You might need to change the version or the architecture used in the example. \
The output files can be found in the `build` directory.

### Linux and MacOS
Make sure that the following packages `cmake` and `build-essential` are installed on the system and run these commands:
```sh
git clone https://github.com/TheGameratorT/zngconv.git
./script.sh
mkdir build && cd build
cmake ../ -DCMAKE_BUILD_TYPE=Release
make
```
The output files can be found in the `build` directory.

## Running

If the input image is a ZNG file, run the command:
```bat
zngconv -i PATH_TO_ZNG_FILE -o PATH_TO_OUTPUT_FILE
```
In case it succeeds, a PNG will be created on the specified output path.

If the input image is a PNG file, run the command:
```bat
zngconv -i PATH_TO_PNG_FILE -o PATH_TO_OUTPUT_FILE
```
In case it succeeds, a ZNG will be created on the specified output path.

The arguments --c1 and --c2 can be used to specify on a scale of 1 to 12, how much each LZ4 pass should compress.
The argument --save-passes (-s) saves each LZ4 pass either after decompression or after compression.
