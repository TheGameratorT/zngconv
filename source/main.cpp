#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION

#include <iostream>
#include <fstream>
#include <filesystem>
#include <functional>
#include <stdint.h>
#include <lz4.h>
#include <lz4hc.h>
#include <stb_image.h>
#include <stb_image_write.h>

#include "args.h"

#define ZNG_MAGIC_NUMBER 0x0000007A

class DestructorGuard {
public:
    inline DestructorGuard(std::function<void()> destroyer) : m_destroyer(destroyer) {}

    inline ~DestructorGuard() {
		m_destroyer();
    }

private:
	std::function<void()> m_destroyer;
};

struct LZ4 {
	std::uint32_t uncompressedSize;
	std::uint32_t compressedSize;
	char data[];
};

struct ZNG {
	std::uint32_t magic; // 0x0000007A
	std::uint32_t bpp; // 0x20
	std::uint32_t width;
	std::uint32_t height;
	LZ4 lz4;
};

inline void freeZNG(ZNG* ptr) {
	delete[] ptr;
}

ZNG* loadZNG(std::ifstream& inputFile, std::size_t inputFileSize) {
	auto zngFile = reinterpret_cast<ZNG*>(new char[inputFileSize]);
	inputFile.read(reinterpret_cast<char*>(zngFile), inputFileSize);

	if (zngFile->magic != ZNG_MAGIC_NUMBER) {
		freeZNG(zngFile);
		return nullptr;
	}

	if (zngFile->bpp != 32) {
		if (zngFile->bpp == 8) {
			std::cerr << "Not supported 8bpp zng" << std::endl;
			freeZNG(zngFile);
			return nullptr;
		}
		std::cerr << "Not supported image color format" << std::endl;
		freeZNG(zngFile);
		return nullptr;
	}

	return zngFile;
}

inline void freeLZ4(void* ptr) {
	delete[] ptr;
}

char* decompressLZ4(const char* compressedData, std::size_t uncompressedSize, std::size_t compressedSize) {
	char* decompressedBuffer = new char[uncompressedSize];

	int decompressedSize = LZ4_decompress_safe(compressedData, decompressedBuffer, compressedSize, uncompressedSize);

	if (decompressedSize != uncompressedSize) {
		std::cerr << "Error decompressing, expected " << uncompressedSize << " bytes but got " << decompressedSize << " bytes instead." << std::endl;
		delete[] decompressedBuffer;
        return nullptr;
	}

    return decompressedBuffer;
}

LZ4* compressLZ4(const char* data, std::size_t dataSize, int compressionLevel) {
	int maxSize = LZ4_compressBound(dataSize);
	LZ4* compressedData = reinterpret_cast<LZ4*>(new char[8 + maxSize]);

	int compSize = LZ4_compress_HC(data, compressedData->data, dataSize, maxSize, compressionLevel);
	if (compSize == 0) {
		delete[] compressedData;
		return nullptr;
	}

	compressedData->uncompressedSize = dataSize;
	compressedData->compressedSize = compSize;

	return compressedData;
}

std::size_t openInputFile(std::ifstream& inputFile, const std::filesystem::path& inputPath, bool& isZNG) {
	inputFile.open(inputPath, std::ios::binary);
	if (!inputFile.is_open()) {
		std::cerr << "Failed to open input file: " << inputPath.string() << std::endl;
		return -1;
	}

	int magic;
	inputFile.read(reinterpret_cast<char*>(&magic), 4);
	isZNG = magic == ZNG_MAGIC_NUMBER;

	inputFile.seekg(0, std::ios::beg);

	return std::filesystem::file_size(inputPath);
}

bool saveOutputFile(const std::filesystem::path& outputPath, const void* data, std::size_t dataSize) {
	std::ofstream outputFile = std::ofstream(outputPath, std::ios::binary);
	if (!outputFile.is_open()) {
		std::cerr << "Failed to open output file: " << outputPath.string() << std::endl;
		return false;
	}

	outputFile.write(reinterpret_cast<const char*>(data), dataSize);
	outputFile.close();
	return true;
}

bool savePNG(const std::filesystem::path& pngPath, const char* rgbaData, int width, int height) {
	int pngDataLen;
	unsigned char* pngData = stbi_write_png_to_mem(reinterpret_cast<const unsigned char *>(rgbaData), width * 4, width, height, 4, &pngDataLen);
	DestructorGuard pngData_guard([&](){ STBI_FREE(pngData); });
	return saveOutputFile(pngPath, pngData, pngDataLen);
}

bool convert_zng_to_png(std::ifstream& inputFile, std::size_t inputFileSize, const std::filesystem::path& outputFilePath) {
	ZNG* zng = loadZNG(inputFile, inputFileSize);
	if (zng == nullptr) {
		return false;
	}
	DestructorGuard zng_guard([&](){ freeZNG(zng); });

	LZ4* lz4 = &zng->lz4;
	LZ4* pass1res = reinterpret_cast<LZ4*>(decompressLZ4(lz4->data, lz4->uncompressedSize, lz4->compressedSize));
	if (pass1res == nullptr) {
		std::cerr << "First pass of decompression failed." << std::endl;
		return false;
	}
	DestructorGuard pass1res_guard([&](){ freeLZ4(pass1res); });

	char* pass2res = decompressLZ4(pass1res->data, pass1res->uncompressedSize, pass1res->compressedSize);
	if (pass2res == nullptr) {
		std::cerr << "Second pass of decompression failed." << std::endl;
		return false;
	}
	DestructorGuard pass2res_guard([&](){ freeLZ4(pass2res); });

	if (!savePNG(outputFilePath, pass2res, zng->width, zng->height)) {
		return false;
	}

	return true;
}

bool convert_img_to_zng(std::ifstream& inputFile, std::size_t inputFileSize, const std::filesystem::path& outputFilePath, int c1, int c2) {
	unsigned char* inputData = new unsigned char[inputFileSize];
	inputFile.read(reinterpret_cast<char*>(inputData), inputFileSize);

	int width, height, channels;
	char* rgbaData = reinterpret_cast<char*>(stbi_load_from_memory(inputData, inputFileSize, &width, &height, &channels, STBI_rgb_alpha));
	if (rgbaData == nullptr) {
		std::cerr << "Image loading failed. You might be using an unsupported image format." << std::endl;
		return false;
	}
	DestructorGuard rgbaData_guard([&](){ STBI_FREE(rgbaData); });

	int rgbaDataSize = width * height * 4;

	LZ4* pass1res = compressLZ4(rgbaData, rgbaDataSize, c1);
	if (pass1res == nullptr) {
		std::cerr << "First pass of compression failed." << std::endl;
		return false;
	}
	DestructorGuard pass1res_guard([&](){ freeLZ4(pass1res); });

	LZ4* pass2res = compressLZ4(reinterpret_cast<char*>(pass1res), 8 + pass1res->compressedSize, c2);
	if (pass2res == nullptr) {
		std::cerr << "Second pass of compression failed." << std::endl;
		return false;
	}
	DestructorGuard pass2res_guard([&](){ freeLZ4(pass2res); });

	std::size_t zngFileSize = 16 + 8 + pass2res->compressedSize;
	auto zngFile = reinterpret_cast<ZNG*>(new char[zngFileSize]);
	DestructorGuard zngFile_guard([&](){ freeZNG(zngFile); });

	zngFile->magic = ZNG_MAGIC_NUMBER;
	zngFile->bpp = 32;
	zngFile->width = width;
	zngFile->height = height;

	std::memcpy(&zngFile->lz4, pass2res, pass2res->compressedSize + 8);

	return saveOutputFile(outputFilePath, zngFile, zngFileSize);
}

int main(int argc, char* argv[]) {
	gengetopt_args_info args_info;
	if (cmdline_parser(argc, argv, &args_info) != 0) {
		return 1;
	}
	DestructorGuard args_info_guard([&](){ cmdline_parser_free(&args_info); });

	std::ifstream inputFile;
	bool inputIsZNG;
	std::size_t inputFileSize = openInputFile(inputFile, args_info.input_arg, inputIsZNG);
	if (inputFileSize == -1) {
		return 1;
	}
	DestructorGuard inputFile_guard([&](){ inputFile.close(); });

	if (inputIsZNG) {
		if (!convert_zng_to_png(inputFile, inputFileSize, args_info.output_arg)) {
			return 1;
		}
	} else {
		if (!convert_img_to_zng(inputFile, inputFileSize, args_info.output_arg, args_info.c1_arg, args_info.c2_arg)) {
			return 1;
		}
	}

	return 0;
}
