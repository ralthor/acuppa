#include "threepoint/camera.hpp"

#include <glm/vec4.hpp>

#include <cmath>
#include <initializer_list>
#include <limits>
#include <stdexcept>

namespace {

void expect(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

}  // namespace

int main() {
    threepoint::PerspectiveCamera camera;
    const glm::mat4 view_projection = camera.view_projection(16.0f / 9.0f);
    const glm::vec4 center = view_projection * glm::vec4(0, 0, 0, 1);
    expect(std::abs(center.x / center.w) < 0.0001f, "camera center X is off axis");
    expect(std::abs(center.y / center.w) < 0.0001f, "camera center Y is off axis");
    expect(center.z > 0.0f && center.z < center.w, "center has invalid depth");
    const glm::vec4 right = view_projection * glm::vec4(1, 0, 0, 1);
    expect(right.x / right.w > 0.0f, "camera X orientation is wrong");
    const glm::vec4 up = view_projection * glm::vec4(0, 1, 0, 1);
    expect(up.y / up.w > 0.0f, "camera Y orientation is wrong"); // nvrhi flips viewport Y.
    for (float aspect : {0.0f, -1.0f}) {
        bool rejected = false;
        try { camera.view_projection(aspect); }
        catch (const std::invalid_argument&) { rejected = true; }
        expect(rejected, "invalid aspect ratio was accepted");
    }
    camera.near_plane = camera.far_plane;
    bool rejected = false;
    try { camera.view_projection(1.0f); }
    catch (const std::invalid_argument&) { rejected = true; }
    expect(rejected, "invalid near plane was accepted");
    camera = threepoint::PerspectiveCamera{};
    camera.eye.x = std::numeric_limits<float>::quiet_NaN();
    rejected = false;
    try { camera.view_projection(1.0f); }
    catch (const std::invalid_argument&) { rejected = true; }
    expect(rejected, "nonfinite camera position was accepted");
}
