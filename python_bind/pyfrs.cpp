#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <fstream>
#include <chrono>
#include <cmath>
#include <algorithm>
#include <string>
#include <vector>
#include <utility>

#include "frs.h"
#include "encoder.h"
#include "image_io.h"
#include "stb_image.h"

namespace py = pybind11;

// اقرأ ملف كـ bytes
static std::vector<uint8_t> readBytes(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return {};
    return std::vector<uint8_t>(
        (std::istreambuf_iterator<char>(f)),
        std::istreambuf_iterator<char>());
}

// حمّل صورة من bytes (PNG/JPG/...)
static codec::Image loadFromMemory(const std::vector<uint8_t>& bytes) {
    int w = 0, h = 0, ch = 0;
    unsigned char* data = stbi_load_from_memory(
        bytes.data(), (int)bytes.size(), &w, &h, &ch, 0);
    if (!data) return {};

    codec::Image img;
    img.width  = w;
    img.height = h;

    if (ch == 1 || ch == 2) {
        img.channels = 1;
        img.pixels.resize((size_t)w * h);
        for (int i = 0; i < w * h; i++) img.pixels[i] = data[i * ch];
    } else {
        img.channels = 3;
        img.pixels.resize((size_t)w * h * 3);
        for (int i = 0; i < w * h; i++) {
            img.pixels[i*3 + 0] = data[i*ch + 0];
            img.pixels[i*3 + 1] = data[i*ch + 1];
            img.pixels[i*3 + 2] = data[i*ch + 2];
        }
    }
    stbi_image_free(data);
    return img;
}

// PSNR
static double computePSNR(const codec::Image& a, const codec::Image& b) {
    if (a.width != b.width || a.height != b.height ||
        a.channels != b.channels || a.pixels.size() != b.pixels.size())
        return -1.0;
    double mse = 0.0;
    for (size_t i = 0; i < a.pixels.size(); i++) {
        double d = (double)a.pixels[i] - (double)b.pixels[i];
        mse += d * d;
    }
    mse /= (double)a.pixels.size();
    if (mse == 0.0) return 999.0;
    return 10.0 * std::log10(255.0 * 255.0 / mse);
}

PYBIND11_MODULE(frs_python, m) {
    m.doc() = "FRS image compression library - Python bindings";

    // ============ الأساسية ============

    m.def("compress", &frs::compress,
          "Compress an image file to .frs",
          py::arg("input"), py::arg("output"), py::arg("quality") = 85);

    m.def("decompress", &frs::decompress,
          "Decompress a .frs file to image",
          py::arg("input"), py::arg("output"));

    // ============ الذاكرة ============

    m.def("encode", [](const std::string& image_path, int quality) {
        auto bytes = readBytes(image_path);
        if (bytes.empty()) return std::vector<uint8_t>{};
        auto img = loadFromMemory(bytes);
        if (img.pixels.empty()) return std::vector<uint8_t>{};
        return codec::encodeImage(img, quality);
    }, "Encode image file to FRS bytes",
       py::arg("image_path"), py::arg("quality") = 85);

    m.def("decode", [](const std::vector<uint8_t>& frs_bytes,
                       const std::string& output_path) {
        try {
            auto img = codec::decodeImage(frs_bytes);
            codec::writeImageAuto(output_path, img);
            return true;
        } catch (...) { return false; }
    }, "Decode FRS bytes to image file",
       py::arg("frs_bytes"), py::arg("output_path"));

    m.def("encode_bytes", [](const std::vector<uint8_t>& image_bytes, int quality) {
        auto img = loadFromMemory(image_bytes);
        if (img.pixels.empty()) return std::vector<uint8_t>{};
        return codec::encodeImage(img, quality);
    }, "Encode image bytes to FRS bytes",
       py::arg("image_bytes"), py::arg("quality") = 85);

    m.def("decode_bytes", [](const std::vector<uint8_t>& frs_bytes) {
        py::tuple result;
        try {
            auto img = codec::decodeImage(frs_bytes);
            result = py::make_tuple(
                (int)img.width, (int)img.height,
                (int)img.channels, img.pixels);
        } catch (...) {
            result = py::make_tuple(
                (int)0, (int)0, (int)0, std::vector<uint8_t>{});
        }
        return result;
    }, "Decode FRS bytes to (width, height, channels, pixels)",
       py::arg("frs_bytes"));

    // ============ المعلومات ============

    m.def("version", &frs::version, "Return version string");

    m.def("info", [](const std::string& frs_file) {
        py::dict d;
        auto hdr = frs::read_header(frs_file);
        if (hdr.size() < 10) return d;
        uint32_t w = (uint32_t)hdr[0] | ((uint32_t)hdr[1]<<8)
                   | ((uint32_t)hdr[2]<<16) | ((uint32_t)hdr[3]<<24);
        uint32_t h = (uint32_t)hdr[4] | ((uint32_t)hdr[5]<<8)
                   | ((uint32_t)hdr[6]<<16) | ((uint32_t)hdr[7]<<24);
        d["width"]    = w;
        d["height"]   = h;
        d["quality"]  = (int)hdr[8];
        d["channels"] = (int)hdr[9];
        return d;
    }, "Return metadata dict of an FRS file", py::arg("frs_file"));

    m.def("is_frs", [](const std::string& path) {
        auto hdr = frs::read_header(path);
        return !hdr.empty();
    }, "Check if a file is in FRS format", py::arg("path"));

    // ============ الأداء ============

    m.def("benchmark", [](const std::string& image_path, int quality) {
        py::dict d;
        auto orig_bytes = readBytes(image_path);
        if (orig_bytes.empty()) return d;
        auto img = loadFromMemory(orig_bytes);
        if (img.pixels.empty()) return d;

        auto t0 = std::chrono::high_resolution_clock::now();
        auto compressed = codec::encodeImage(img, quality);
        auto t1 = std::chrono::high_resolution_clock::now();
        double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

        d["original_size"]   = (uint64_t)orig_bytes.size();
        d["compressed_size"] = (uint64_t)compressed.size();
        d["ratio"]           = (double)orig_bytes.size() / (double)compressed.size();
        d["time_ms"]         = ms;
        d["width"]           = (int)img.width;
        d["height"]          = (int)img.height;
        d["channels"]        = (int)img.channels;
        return d;
    }, "Benchmark compression of an image",
       py::arg("image_path"), py::arg("quality") = 85);

    m.def("compare", [](const std::string& original_path,
                        const std::string& frs_path) {
        py::dict d;
        auto orig_bytes = readBytes(original_path);
        if (orig_bytes.empty()) return d;
        auto orig = loadFromMemory(orig_bytes);
        if (orig.pixels.empty()) return d;

        auto frs_bytes = readBytes(frs_path);
        if (frs_bytes.empty()) return d;

        try {
            auto decoded = codec::decodeImage(frs_bytes);
            d["original_size"] = (uint64_t)orig_bytes.size();
            d["frs_size"]      = (uint64_t)frs_bytes.size();
            d["ratio"]         = (double)orig_bytes.size() / (double)frs_bytes.size();
            d["psnr"]          = computePSNR(orig, decoded);
        } catch (...) {}
        return d;
    }, "Compare original with FRS compressed version",
       py::arg("original_path"), py::arg("frs_path"));

    // ============ مجلد (list من Python) ============

    m.def("batch_compress", [](const std::vector<std::string>& files,
                               const std::string& output_dir,
                               int quality) {
        std::vector<std::pair<std::string, bool>> results;
        for (const auto& f : files) {
            // اسم الملف بدون امتداد
            size_t slash = f.find_last_of("/\\");
            std::string base = (slash == std::string::npos) ? f : f.substr(slash + 1);
            size_t dot = base.find_last_of('.');
            if (dot != std::string::npos) base = base.substr(0, dot);

            std::string out = output_dir;
            if (!out.empty() && out.back() != '/' && out.back() != '\\')
                out += "/";
            out += base + ".frs";

            bool ok = frs::compress(f, out, quality);
            results.push_back(std::make_pair(base, ok));
        }
        return results;
    }, "Compress a list of files to a folder",
       py::arg("files"), py::arg("output_dir"), py::arg("quality") = 85);

    m.def("get_default_quality", []() { return 85; },
          "Return default quality");
}
