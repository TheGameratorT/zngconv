@echo off
cd %~dp0
mkdir libs
cd libs
git clone https://github.com/lz4/lz4.git
cd lz4
git checkout 839edadd09bd653b7e9237a2fbf405d9e8bfc14e
cd..
mkdir stb
cd stb
curl -o stb_image.h https://raw.githubusercontent.com/nothings/stb/5736b15f7ea0ffb08dd38af21067c314d6a3aae9/stb_image.h
curl -o stb_image_write.h https://raw.githubusercontent.com/nothings/stb/5736b15f7ea0ffb08dd38af21067c314d6a3aae9/stb_image_write.h
cd %~dp0
