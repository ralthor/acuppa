#include <Jolt/Jolt.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemSingleThreaded.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/ContactListener.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/PhysicsSystem.h>

#include "threepoint/physics_world.hpp"

#include <glm/geometric.hpp>

#include <cmath>
#include <map>
#include <stdexcept>

namespace threepoint {
namespace {

struct JoltRuntime {
    JoltRuntime() {
        JPH::RegisterDefaultAllocator();
        JPH::Factory::sInstance = new JPH::Factory();
        JPH::RegisterTypes();
    }
    ~JoltRuntime() {
        JPH::UnregisterTypes();
        delete JPH::Factory::sInstance;
        JPH::Factory::sInstance = nullptr;
    }
};

constexpr JPH::ObjectLayer kStatic = 0;
constexpr JPH::ObjectLayer kMoving = 1;

class BroadPhaseLayers final : public JPH::BroadPhaseLayerInterface {
public:
    JPH::uint GetNumBroadPhaseLayers() const override { return 2; }
    JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer layer) const override {
        return JPH::BroadPhaseLayer(static_cast<JPH::BroadPhaseLayer::Type>(layer));
    }
#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
    const char* GetBroadPhaseLayerName(JPH::BroadPhaseLayer layer) const override {
        return layer == JPH::BroadPhaseLayer(kStatic) ? "static" : "moving";
    }
#endif
};

class ObjectPairs final : public JPH::ObjectLayerPairFilter {
public:
    bool ShouldCollide(JPH::ObjectLayer a, JPH::ObjectLayer b) const override {
        return a == kMoving || b == kMoving;
    }
};

class BroadPhasePairs final : public JPH::ObjectVsBroadPhaseLayerFilter {
public:
    bool ShouldCollide(JPH::ObjectLayer a, JPH::BroadPhaseLayer b) const override {
        return a == kMoving || b == JPH::BroadPhaseLayer(kMoving);
    }
};

bool finite(glm::vec3 v) {
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}

}  // namespace

struct PhysicsWorld::Impl : JPH::ContactListener {
    JoltRuntime& runtime;
    BroadPhaseLayers layers;
    BroadPhasePairs broad_pairs;
    ObjectPairs object_pairs;
    JPH::PhysicsSystem system;
    JPH::TempAllocatorImpl allocator{10 * 1024 * 1024};
    JPH::JobSystemSingleThreaded jobs{JPH::cMaxPhysicsJobs};
    std::map<uint64_t, JPH::BodyID> bodies;
    std::vector<PhysicsContact> events;

    Impl() : runtime([]() -> JoltRuntime& { static JoltRuntime value; return value; }()) {
        system.Init(8192, 0, 8192, 4096, layers, broad_pairs, object_pairs);
        system.SetContactListener(this);
        system.SetGravity(JPH::Vec3(0.0f, -9.81f, 0.0f));
    }

    ~Impl() override {
        system.SetContactListener(nullptr);
        auto& iface = system.GetBodyInterface();
        for (const auto& [id, body] : bodies) {
            iface.RemoveBody(body);
            iface.DestroyBody(body);
        }
    }

    void OnContactAdded(const JPH::Body& a, const JPH::Body& b,
                        const JPH::ContactManifold&, JPH::ContactSettings&) override {
        events.push_back({a.GetUserData(), b.GetUserData(), a.IsSensor() || b.IsSensor()});
    }
};

PhysicsWorld::PhysicsWorld() : impl_(std::make_unique<Impl>()) {}
PhysicsWorld::~PhysicsWorld() = default;

void PhysicsWorld::add(const PhysicsBodyDesc& desc) {
    if (desc.id == 0 || impl_->bodies.count(desc.id))
        throw std::invalid_argument("physics body ID must be nonzero and unique");
    if (!finite(desc.position) || !finite(desc.velocity) ||
        !std::isfinite(desc.rotation.x) || !std::isfinite(desc.rotation.y) ||
        !std::isfinite(desc.rotation.z) || !std::isfinite(desc.rotation.w) ||
        !std::isfinite(glm::length(desc.rotation)) ||
        glm::length(desc.rotation) < 0.0001f)
        throw std::invalid_argument("physics body transform or velocity is invalid");
    if (desc.shape == ColliderShape::Box &&
        (!finite(desc.half_extents) || desc.half_extents.x <= 0.0f ||
         desc.half_extents.y <= 0.0f || desc.half_extents.z <= 0.0f))
        throw std::invalid_argument("box half extents must be finite and positive");
    if (desc.shape == ColliderShape::Sphere &&
        (!std::isfinite(desc.radius) || desc.radius <= 0.0f))
        throw std::invalid_argument("sphere radius must be finite and positive");
    if (desc.motion == PhysicsMotion::Static && glm::length(desc.velocity) != 0.0f)
        throw std::invalid_argument("static physics body cannot have velocity");

    JPH::RefConst<JPH::Shape> shape;
    if (desc.shape == ColliderShape::Box)
        shape = new JPH::BoxShape(JPH::Vec3(desc.half_extents.x, desc.half_extents.y,
                                            desc.half_extents.z));
    else if (desc.shape == ColliderShape::Sphere)
        shape = new JPH::SphereShape(desc.radius);
    else
        throw std::invalid_argument("unknown collider shape");

    JPH::EMotionType motion;
    switch (desc.motion) {
    case PhysicsMotion::Static: motion = JPH::EMotionType::Static; break;
    case PhysicsMotion::Dynamic: motion = JPH::EMotionType::Dynamic; break;
    case PhysicsMotion::Kinematic: motion = JPH::EMotionType::Kinematic; break;
    default: throw std::invalid_argument("unknown physics motion type");
    }
    const auto rotation = glm::normalize(desc.rotation);
    JPH::BodyCreationSettings settings(
        shape, JPH::RVec3(desc.position.x, desc.position.y, desc.position.z),
        JPH::Quat(rotation.x, rotation.y, rotation.z, rotation.w), motion,
        motion == JPH::EMotionType::Static ? kStatic : kMoving);
    settings.mIsSensor = desc.trigger;
    settings.mUserData = desc.id;
    settings.mLinearVelocity = JPH::Vec3(desc.velocity.x, desc.velocity.y, desc.velocity.z);
    const JPH::BodyID body = impl_->system.GetBodyInterface().CreateAndAddBody(
        settings, motion == JPH::EMotionType::Static ? JPH::EActivation::DontActivate
                                               : JPH::EActivation::Activate);
    if (body.IsInvalid()) throw std::runtime_error("physics body capacity exhausted");
    impl_->bodies.emplace(desc.id, body);
}

bool PhysicsWorld::remove(uint64_t id) {
    auto it = impl_->bodies.find(id);
    if (it == impl_->bodies.end()) return false;
    auto& iface = impl_->system.GetBodyInterface();
    iface.RemoveBody(it->second);
    iface.DestroyBody(it->second);
    impl_->bodies.erase(it);
    return true;
}

std::optional<PhysicsBodyState> PhysicsWorld::state(uint64_t id) const {
    auto it = impl_->bodies.find(id);
    if (it == impl_->bodies.end()) return std::nullopt;
    const auto& iface = impl_->system.GetBodyInterface();
    const auto p = iface.GetPosition(it->second);
    const auto r = iface.GetRotation(it->second);
    const auto v = iface.GetLinearVelocity(it->second);
    return PhysicsBodyState{{p.GetX(), p.GetY(), p.GetZ()},
                            {r.GetW(), r.GetX(), r.GetY(), r.GetZ()},
                            {v.GetX(), v.GetY(), v.GetZ()}, iface.IsActive(it->second)};
}

void PhysicsWorld::step() {
    impl_->events.clear();
    if (impl_->system.Update(1.0f / 60.0f, 1, &impl_->allocator, &impl_->jobs) !=
        JPH::EPhysicsUpdateError::None)
        throw std::runtime_error("physics simulation exceeded contact capacity");
}

const std::vector<PhysicsContact>& PhysicsWorld::contacts() const { return impl_->events; }

}  // namespace threepoint
