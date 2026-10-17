#include "threepoint/world.hpp"

#include <cmath>
#include <stdexcept>

namespace {
void expect(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
}

int main() {
    threepoint::World world;
    const auto first = world.create("first");
    const auto second = world.create("second");
    expect(first != second && world.entities().size() == 2, "entity IDs are not unique");
    expect(world.name(first) == "first", "entity name was lost");

    threepoint::TransformComponent transform;
    transform.position = {1, 2, 3};
    world.set_transform(first, transform);
    expect(world.transform(first)->position.x == 1, "transform did not persist");
    world.set_mesh(first, {"builtin:cube", true});
    expect(world.snapshot({}).cubes.size() == 1, "mesh component did not render");
    world.set_mesh(second, {"assets/triangle.gltf", true});
    expect(world.snapshot({}).imported.size() == 1 &&
           world.snapshot({}).imported[0].source == "assets/triangle.gltf",
           "imported mesh component did not reach render snapshot");
    threepoint::TransformComponent parent_transform;
    parent_transform.position = {4, 0, 0};
    world.set_transform(second, parent_transform);
    transform.parent = second;
    world.set_transform(first, transform);
    expect(std::abs(world.snapshot({}).cubes[0].parent_transform[3].x - 4.0f) < 0.001f,
           "parent transform was not applied to rendered child");
    parent_transform.parent = first;
    bool cycle_rejected = false;
    try { world.set_transform(second, parent_transform); }
    catch (const std::invalid_argument&) { cycle_rejected = true; }
    expect(cycle_rejected, "transform parenting cycle was accepted");
    world.remove_transform(second);
    expect(!world.transform(first)->parent, "removing a parent did not detach child");

    threepoint::CameraComponent camera;
    camera.camera.eye = {0, 1, 5};
    camera.active = true;
    world.set_camera(first, camera);
    expect(world.active_camera() == first && world.snapshot({}).camera.eye.y == 1,
           "active camera did not reach render snapshot");
    camera.camera.eye = {0, 2, 5};
    world.set_camera(second, camera);
    expect(!world.camera(first)->active && world.active_camera() == second,
           "second active camera did not deactivate the first");

    threepoint::LightComponent light;
    light.type = threepoint::LightComponent::Type::Directional;
    light.intensity = 2;
    world.set_light(first, light);
    expect(world.light(first)->intensity == 2, "light component did not persist");
    expect(world.snapshot({}).lights.size() == 1 &&
           world.snapshot({}).lights[0].directional &&
           world.snapshot({}).lights[0].intensity == 2,
           "light component did not reach the renderer");
    for (int i = 0; i < 7; ++i) world.set_light(world.create(), light);
    bool light_limit_rejected = false;
    try { world.set_light(world.create(), light); }
    catch (const std::invalid_argument&) { light_limit_rejected = true; }
    expect(light_limit_rejected, "ninth light was silently accepted");
    world.set_script(first, {"scripts/main.lua", true});
    expect(world.script(first)->enabled, "script component did not persist");

    expect(world.remove_transform(first) && !world.transform(first), "transform remove failed");
    expect(world.remove_camera(first) && !world.camera(first), "camera remove failed");
    expect(world.remove_light(first) && !world.light(first), "light remove failed");
    expect(world.remove_mesh(first) && !world.mesh(first), "mesh remove failed");
    expect(world.remove_script(first) && !world.script(first), "script remove failed");
    expect(!world.remove_script(first), "removing absent script should report false");

    transform.scale.x = 0;
    bool rejected = false;
    try { world.set_transform(first, transform); }
    catch (const std::invalid_argument&) { rejected = true; }
    expect(rejected, "zero transform scale was accepted");
    rejected = false;
    try { world.set_mesh(first, {"missing:mesh", true}); }
    catch (const std::invalid_argument&) { rejected = true; }
    expect(rejected, "unsupported mesh was accepted");

    world.destroy(first);
    expect(!world.exists(first) && world.exists(second), "destroy removed wrong entity");
    const auto third = world.create();
    expect(third > second, "entity ID was reused after destruction");
    rejected = false;
    try { world.transform(first); }
    catch (const std::invalid_argument&) { rejected = true; }
    expect(rejected, "destroyed entity was still queryable");
}
