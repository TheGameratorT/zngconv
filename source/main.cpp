#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION

#include <iostream>
#include <fstream>
#include <filesystem>
#include <functional>
#include <memory>
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

static std::unique_ptr<char[]> decompressLZ4(const void* compressedData, std::size_t uncompressedSize, std::size_t compressedSize) {
	auto decompressedBuffer = std::make_unique<char[]>(uncompressedSize);

	int decompressedSize = LZ4_decompress_safe(reinterpret_cast<const char*>(compressedData), decompressedBuffer.get(), compressedSize, uncompressedSize);

	if (decompressedSize != uncompressedSize) {
		std::cerr << "Error decompressing, expected " << uncompressedSize << " bytes but got " << decompressedSize << " bytes instead." << std::endl;
        return nullptr;
	}

    return decompressedBuffer;
}

static std::unique_ptr<char[]> compressLZ4(const void* data, std::size_t dataSize, int compressionLevel) {
	int maxSize = LZ4_compressBound(dataSize);
	auto compressedData = std::make_unique<char[]>(8 + maxSize);
	LZ4* compressed = reinterpret_cast<LZ4*>(compressedData.get());

	int compSize = LZ4_compress_HC(static_cast<const char*>(data), compressed->data, dataSize, maxSize, compressionLevel);
	if (compSize == 0) {
		return nullptr;
	}

	compressed->uncompressedSize = dataSize;
	compressed->compressedSize = compSize;

	return compressedData;
}

static bool saveOutputFile(const std::filesystem::path& outputPath, const void* data, std::size_t dataSize) {
	std::ofstream outputFile = std::ofstream(outputPath, std::ios::binary);
	if (!outputFile.is_open()) {
		std::cerr << "Failed to open output file: " << outputPath.string() << std::endl;
		return false;
	}

	outputFile.write(reinterpret_cast<const char*>(data), dataSize);
	outputFile.close();
	return true;
}

static bool savePNG(const std::filesystem::path& pngPath, const void* rgbaData, int width, int height) {
	int pngDataLen;
	unsigned char* pngData = stbi_write_png_to_mem(reinterpret_cast<const unsigned char *>(rgbaData), width * 4, width, height, 4, &pngDataLen);
	DestructorGuard pngData_guard([&](){ STBI_FREE(pngData); });
	return saveOutputFile(pngPath, pngData, pngDataLen);
}

static int convert_zng_to_png(const void* inputData, std::size_t inputFileSize, gengetopt_args_info& args_info) {
	std::filesystem::path outputFilePath = args_info.output_arg;
	bool savePasses = args_info.save_passes_given;

	const ZNG* zng = reinterpret_cast<const ZNG*>(inputData);

	/*if (zng->magic != ZNG_MAGIC_NUMBER) {
		return 1;
	}*/

	if (zng->bpp != 32) {
		if (zng->bpp == 8) {
			std::cerr << "Not supported 8bpp zng" << std::endl;
			return 1;
		}
		std::cerr << "Not supported image color format" << std::endl;
		return 1;
	}

	const LZ4* lz4 = &zng->lz4;

	auto pass1resData = decompressLZ4(lz4->data, lz4->uncompressedSize, lz4->compressedSize);
	LZ4* pass1res = reinterpret_cast<LZ4*>(pass1resData.get());
	if (pass1res == nullptr) {
		std::cerr << "First pass of decompression failed." << std::endl;
		return 1;
	}

	if (savePasses) {
		saveOutputFile(outputFilePath.string() + ".pass1", pass1res, lz4->uncompressedSize);
	}

	auto pass2res = decompressLZ4(pass1res->data, pass1res->uncompressedSize, pass1res->compressedSize);
	if (pass2res == nullptr) {
		std::cerr << "Second pass of decompression failed." << std::endl;
		return 1;
	}

	if (savePasses) {
		saveOutputFile(outputFilePath.string() + ".pass2", pass2res.get(), pass1res->uncompressedSize);
	}

	return savePNG(outputFilePath, pass2res.get(), zng->width, zng->height) ? 0 : 1;
}

static int convert_img_to_zng(const void* inputData, std::size_t inputFileSize, gengetopt_args_info& args_info) {
	std::filesystem::path outputFilePath = args_info.output_arg;
	bool savePasses = args_info.save_passes_given;

	int width, height, channels;
	stbi_uc* rgbaData = stbi_load_from_memory(reinterpret_cast<const stbi_uc*>(inputData), inputFileSize, &width, &height, &channels, STBI_rgb_alpha);
	if (rgbaData == nullptr) {
		std::cerr << "Image loading failed. You might be using an unsupported image format." << std::endl;
		return 1;
	}
	DestructorGuard rgbaData_guard([&](){ STBI_FREE(rgbaData); });

	int rgbaDataSize = width * height * 4;

	auto pass1resData = compressLZ4(rgbaData, rgbaDataSize, args_info.c1_arg);
	LZ4* pass1res = reinterpret_cast<LZ4*>(pass1resData.get());
	if (pass1res == nullptr) {
		std::cerr << "First pass of compression failed." << std::endl;
		return 1;
	}

	if (savePasses) {
		saveOutputFile(outputFilePath.string() + ".pass1", pass1res, 8 + pass1res->compressedSize);
	}

	auto pass2resData = compressLZ4(pass1res, 8 + pass1res->compressedSize, args_info.c2_arg);
	LZ4* pass2res = reinterpret_cast<LZ4*>(pass2resData.get());
	if (pass2res == nullptr) {
		std::cerr << "Second pass of compression failed." << std::endl;
		return 1;
	}

	if (savePasses) {
		saveOutputFile(outputFilePath.string() + ".pass2", pass2res, 8 + pass2res->compressedSize);
	}

	std::size_t zngFileSize = 16 + 8 + pass2res->compressedSize;
	auto zngFileData = std::make_unique<char[]>(zngFileSize);
	auto zngFile = reinterpret_cast<ZNG*>(zngFileData.get());

	zngFile->magic = ZNG_MAGIC_NUMBER;
	zngFile->bpp = 32;
	zngFile->width = width;
	zngFile->height = height;

	std::memcpy(&zngFile->lz4, pass2res, pass2res->compressedSize + 8);

	return saveOutputFile(outputFilePath, zngFile, zngFileSize) ? 0 : 1;
}

int main(int argc, char* argv[]) {
	gengetopt_args_info args_info;
	if (cmdline_parser(argc, argv, &args_info) != 0) {
		return 1;
	}
	DestructorGuard args_info_guard([&](){ cmdline_parser_free(&args_info); });

	std::filesystem::path inputFilePath = args_info.input_arg;

	std::ifstream inputFile(inputFilePath, std::ios::binary);
	if (!inputFile.is_open()) {
		std::cerr << "Failed to open input file: " << inputFilePath.string() << std::endl;
		return 1;
	}

	std::size_t inputFileSize = std::filesystem::file_size(inputFilePath);

	auto inputData = std::make_unique<char[]>(inputFileSize);
	inputFile.read(inputData.get(), inputFileSize);
	inputFile.close();

	std::uint32_t magic;
	std::memcpy(&magic, inputData.get(), 4);

	return (magic == ZNG_MAGIC_NUMBER) ?
		convert_zng_to_png(inputData.get(), inputFileSize, args_info) :
		convert_img_to_zng(inputData.get(), inputFileSize, args_info);
}
