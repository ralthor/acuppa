#include "threepoint/gltf_asset.hpp"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

namespace fs = std::filesystem;

void expect(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

int main(int argc, char* argv[]) {
    if (argc != 3) throw std::runtime_error("source root and temporary directory required");
    const fs::path root = argv[1];
    const auto asset = threepoint::load_gltf(root, "assets/triangle.gltf");
    expect(asset.primitives.size() == 1, "glTF scene triangle was not imported");
    const auto& triangle = asset.primitives[0];
    expect(triangle.vertices.size() == 3 && triangle.indices.size() == 3,
           "glTF vertex or index data was lost");
    expect(triangle.indices[0] == 0 && triangle.indices[1] == 1 && triangle.indices[2] == 2,
           "glTF index order changed");
    expect(std::abs(triangle.node_transform[3].x - 0.25f) < 0.001f &&
           std::abs(triangle.node_transform[3].y - 0.25f) < 0.001f,
           "glTF parent and child transforms were not composed");
    expect(triangle.vertices[0].color[0] > 0.9f,
           "glTF vertex color was not imported");
    const auto external = threepoint::load_gltf(root, "assets/triangle_external.gltf");
    const auto binary = threepoint::load_gltf(root, "assets/triangle.glb");
    expect(external.primitives.size() == 1 && binary.primitives.size() == 1 &&
           external.primitives[0].indices == triangle.indices &&
           binary.primitives[0].indices == triangle.indices,
           "external-buffer glTF or GLB geometry differs from embedded glTF");
    const auto textured = threepoint::load_gltf(root, "assets/material_quad.gltf");
    expect(textured.primitives.size() == 1 && textured.images.size() == 4,
           "textured glTF material or images were not imported");
    const auto& material = textured.primitives[0].material;
    expect(std::abs(material.metallic - 0.5f) < 0.001f &&
           std::abs(material.roughness - 0.4f) < 0.001f &&
           material.base_color_image == 0 && material.metallic_roughness_image == 1 &&
           material.normal_image == 2 && material.occlusion_image == 1 &&
           material.emissive_image == 3,
           "glTF PBR factors or texture bindings were lost");
    expect(textured.images[0].width == 2 && textured.images[0].height == 2 &&
           textured.images[0].rgba.size() == 16 && textured.images[0].rgba[0] == 255,
           "PNG pixels were not decoded to RGBA");
    expect(textured.primitives[0].vertices[0].normal[2] == 1.0f &&
           textured.primitives[0].vertices[0].uv[1] == 1.0f,
           "glTF normals or UVs were not imported");

    bool missing_rejected = false;
    try { threepoint::load_gltf(root, "assets/missing.gltf"); }
    catch (const std::runtime_error& error) {
        missing_rejected = std::string(error.what()).find("assets/missing.gltf") !=
                           std::string::npos;
    }
    expect(missing_rejected, "missing asset error did not name the file");
    bool escape_rejected = false;
    try { threepoint::load_gltf(root, "../outside.gltf"); }
    catch (const std::runtime_error& error) {
        escape_rejected = std::string(error.what()).find("escapes project") !=
                          std::string::npos;
    }
    expect(escape_rejected, "outside-project asset was accepted");

    const fs::path work = argv[2];
    fs::create_directories(work / "assets");
    std::ifstream input(root / "assets/triangle.gltf");
    const std::string original((std::istreambuf_iterator<char>(input)),
                               std::istreambuf_iterator<char>());
    std::string malformed = original;
    const auto size = malformed.find("\"byteLength\": 36");
    expect(size != std::string::npos, "fixture format changed");
    malformed.replace(size, std::string("\"byteLength\": 36").size(),
                      "\"byteLength\": 1");
    std::ofstream(work / "assets/bad.gltf") << malformed;
    bool bounds_rejected = false;
    try { threepoint::load_gltf(work, "assets/bad.gltf"); }
    catch (const std::runtime_error& error) {
        bounds_rejected = std::string(error.what()).find("assets/bad.gltf") !=
                          std::string::npos;
    }
    expect(bounds_rejected, "out-of-bounds accessor was accepted");
    std::ifstream external_input(root / "assets/triangle_external.gltf");
    std::string unsafe((std::istreambuf_iterator<char>(external_input)),
                       std::istreambuf_iterator<char>());
    const auto uri = unsafe.find("triangle.bin");
    expect(uri != std::string::npos, "external fixture format changed");
    unsafe.replace(uri, std::string("triangle.bin").size(), "../outside.bin");
    std::ofstream(work / "assets/unsafe.gltf") << unsafe;
    bool buffer_escape_rejected = false;
    try { threepoint::load_gltf(work, "assets/unsafe.gltf"); }
    catch (const std::runtime_error& error) {
        buffer_escape_rejected = std::string(error.what()).find("escapes project") !=
                                 std::string::npos;
    }
    expect(buffer_escape_rejected, "outside-project glTF buffer was accepted");

    std::ifstream material_input(root / "assets/material_quad.gltf");
    std::string invalid_image((std::istreambuf_iterator<char>(material_input)),
                              std::istreambuf_iterator<char>());
    const auto image_start = invalid_image.find("data:image/png;base64,");
    expect(image_start != std::string::npos, "textured fixture format changed");
    const auto image_end = invalid_image.find('"', image_start);
    invalid_image.replace(image_start, image_end - image_start,
                          "data:image/png;base64,AAAA");
    std::ofstream(work / "assets/invalid_image.gltf") << invalid_image;
    bool image_rejected = false;
    try { threepoint::load_gltf(work, "assets/invalid_image.gltf"); }
    catch (const std::runtime_error& error) {
        image_rejected = std::string(error.what()).find("image 0") != std::string::npos;
    }
    expect(image_rejected, "invalid PNG was accepted");
    fs::remove_all(work);
}
