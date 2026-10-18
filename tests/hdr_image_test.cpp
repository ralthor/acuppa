#include "acuppa/hdr_image.hpp"

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

namespace fs = std::filesystem;

void expect(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

int main(int argc, char* argv[]) {
    if (argc != 3) throw std::runtime_error("fixture root and work path required");
    const fs::path fixture = fs::path(argv[1]) / "assets/test_gradient.hdr";
    const auto image = acuppa::load_hdr_image(fixture);
    expect(image.width == 16 && image.height == 8 && image.rgba.size() == 16 * 8 * 4,
           "Radiance HDR image dimensions or channels were lost");
    const size_t bright = (2 * 16 + 7) * 4;
    const size_t ground = (6 * 16 + 7) * 4;
    expect(image.rgba[bright] > 10.0f && image.rgba[bright + 2] > 5.0f &&
           image.rgba[ground] > image.rgba[ground + 2],
           "HDR radiance or equirectangular rows decoded incorrectly");

    const fs::path work = argv[2];
    fs::create_directories(work);
    bool missing = false;
    try { acuppa::load_hdr_image(work / "missing.hdr"); }
    catch (const std::runtime_error& error) {
        missing = std::string(error.what()).find("missing.hdr") != std::string::npos;
    }
    expect(missing, "missing HDRI error did not name the file");

    std::ofstream(work / "invalid.hdr", std::ios::binary) << "not a Radiance image";
    bool invalid = false;
    try { acuppa::load_hdr_image(work / "invalid.hdr"); }
    catch (const std::runtime_error& error) {
        invalid = std::string(error.what()).find("invalid Radiance") != std::string::npos;
    }
    expect(invalid, "malformed HDRI was accepted");

    std::ifstream input(fixture, std::ios::binary);
    std::string bytes((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    const auto header = bytes.find("-Y 8 +X 16");
    expect(header != std::string::npos, "fixture resolution header changed");
    bytes.replace(header, 10, "-Y 8 +X 15");
    std::ofstream bad_size(work / "bad_aspect.hdr", std::ios::binary);
    bad_size.write(bytes.data(), std::streamsize(bytes.size()));
    bad_size.close();
    bool aspect = false;
    try { acuppa::load_hdr_image(work / "bad_aspect.hdr"); }
    catch (const std::runtime_error& error) {
        aspect = std::string(error.what()).find("2:1") != std::string::npos;
    }
    expect(aspect, "non-equirectangular HDRI was accepted");
    fs::remove_all(work);
}
