#include "shadow_layout.hpp"

#include <stdexcept>

void expect(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

int main() {
    std::vector<acuppa::LightInstance> lights(3);
    auto layout = acuppa::plan_shadow_atlas(lights);
    expect(layout.rows == 0 && layout.bytes() == 0, "empty shadow atlas consumed memory");
    lights[1].shadows = true;
    layout = acuppa::plan_shadow_atlas(lights);
    expect(layout.rows == 1 && layout.row_for_light[0] == -1 &&
           layout.row_for_light[1] == 0 && layout.row_for_light[2] == -1,
           "shadowed light was not packed into the first row");
    expect(layout.bytes() == 24ull * 1024 * 1024,
           "one 1024-pixel shadow row should use 24 MiB");
    lights[2].shadows = true;
    layout = acuppa::plan_shadow_atlas(lights);
    expect(layout.rows == 2 && layout.row_for_light[2] == 1 &&
           layout.bytes() == 48ull * 1024 * 1024,
           "second shadowed light did not get a separate row");
    lights.resize(9);
    bool rejected = false;
    try { (void)acuppa::plan_shadow_atlas(lights); }
    catch (const std::invalid_argument&) { rejected = true; }
    expect(rejected, "more than eight lights passed the atlas limit");
}
