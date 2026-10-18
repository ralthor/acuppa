#include <Jolt/Jolt.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemSingleThreaded.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/ContactListener.h>
#include <Jolt/Physics/Collision/CollisionCollectorImpl.h>
#include <Jolt/Physics/Collision/CollideShape.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/NarrowPhaseQuery.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/PhysicsSystem.h>

#include "threepoint/physics_world.hpp"

#include <glm/geometric.hpp>

#include <cmath>
#include <map>
#include <set>
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
    std::map<uint32_t, uint64_t> entity_ids;
    std::map<std::pair<uint64_t, uint64_t>, bool> active_contacts;
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
        const std::pair<uint64_t, uint64_t> pair{
            std::min(a.GetUserData(), b.GetUserData()),
            std::max(a.GetUserData(), b.GetUserData())};
        if (active_contacts.emplace(pair, a.IsSensor() || b.IsSensor()).second)
            events.push_back({pair.first, pair.second, a.IsSensor() || b.IsSensor(), true});
    }

    void OnContactRemoved(const JPH::SubShapeIDPair& pair) override {
        const auto a = entity_ids.find(pair.GetBody1ID().GetIndexAndSequenceNumber());
        const auto b = entity_ids.find(pair.GetBody2ID().GetIndexAndSequenceNumber());
        if (a == entity_ids.end() || b == entity_ids.end()) return;
        const std::pair<uint64_t, uint64_t> key{
            std::min(a->second, b->second), std::max(a->second, b->second)};
        const auto active = active_contacts.find(key);
        if (active != active_contacts.end()) {
            events.push_back({key.first, key.second, active->second, false});
            active_contacts.erase(active);
        }
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
    impl_->entity_ids.emplace(body.GetIndexAndSequenceNumber(), desc.id);
}

bool PhysicsWorld::remove(uint64_t id) {
    auto it = impl_->bodies.find(id);
    if (it == impl_->bodies.end()) return false;
    auto& iface = impl_->system.GetBodyInterface();
    iface.RemoveBody(it->second);
    impl_->entity_ids.erase(it->second.GetIndexAndSequenceNumber());
    for (auto contact = impl_->active_contacts.begin();
         contact != impl_->active_contacts.end();) {
        if (contact->first.first == id || contact->first.second == id)
            contact = impl_->active_contacts.erase(contact);
        else ++contact;
    }
    iface.DestroyBody(it->second);
    impl_->bodies.erase(it);
    return true;
}

void PhysicsWorld::set_velocity(uint64_t id, glm::vec3 velocity) {
    if (!finite(velocity)) throw std::invalid_argument("physics velocity must be finite");
    const auto it = impl_->bodies.find(id);
    if (it == impl_->bodies.end()) throw std::invalid_argument("unknown physics body ID");
    if (impl_->system.GetBodyInterface().GetMotionType(it->second) == JPH::EMotionType::Static)
        throw std::invalid_argument("static physics body cannot have velocity");
    impl_->system.GetBodyInterface().SetLinearVelocity(
        it->second, JPH::Vec3(velocity.x, velocity.y, velocity.z));
}

void PhysicsWorld::move(uint64_t id, glm::vec3 position) {
    if (!finite(position)) throw std::invalid_argument("physics position must be finite");
    const auto it = impl_->bodies.find(id);
    if (it == impl_->bodies.end()) throw std::invalid_argument("unknown physics body ID");
    impl_->system.GetBodyInterface().SetPosition(
        it->second, JPH::RVec3(position.x, position.y, position.z), JPH::EActivation::Activate);
}

void PhysicsWorld::set_pose(uint64_t id, glm::vec3 position, glm::quat rotation) {
    if (!finite(position) || !std::isfinite(rotation.x) || !std::isfinite(rotation.y) ||
        !std::isfinite(rotation.z) || !std::isfinite(rotation.w) ||
        !std::isfinite(glm::length(rotation)) || glm::length(rotation) < 0.0001f)
        throw std::invalid_argument("physics pose must be finite with nonzero rotation");
    const auto it = impl_->bodies.find(id);
    if (it == impl_->bodies.end()) throw std::invalid_argument("unknown physics body ID");
    rotation = glm::normalize(rotation);
    impl_->system.GetBodyInterface().SetPositionAndRotation(
        it->second, JPH::RVec3(position.x, position.y, position.z),
        JPH::Quat(rotation.x, rotation.y, rotation.z, rotation.w), JPH::EActivation::Activate);
}

void PhysicsWorld::apply_force(uint64_t id, glm::vec3 force) {
    if (!finite(force)) throw std::invalid_argument("physics force must be finite");
    const auto it = impl_->bodies.find(id);
    if (it == impl_->bodies.end()) throw std::invalid_argument("unknown physics body ID");
    if (impl_->system.GetBodyInterface().GetMotionType(it->second) != JPH::EMotionType::Dynamic)
        throw std::invalid_argument("force requires a dynamic physics body");
    impl_->system.GetBodyInterface().AddForce(it->second, JPH::Vec3(force.x, force.y, force.z));
}

void PhysicsWorld::apply_impulse(uint64_t id, glm::vec3 impulse) {
    if (!finite(impulse)) throw std::invalid_argument("physics impulse must be finite");
    const auto it = impl_->bodies.find(id);
    if (it == impl_->bodies.end()) throw std::invalid_argument("unknown physics body ID");
    if (impl_->system.GetBodyInterface().GetMotionType(it->second) != JPH::EMotionType::Dynamic)
        throw std::invalid_argument("impulse requires a dynamic physics body");
    impl_->system.GetBodyInterface().AddImpulse(it->second,
        JPH::Vec3(impulse.x, impulse.y, impulse.z));
}

std::optional<PhysicsRayHit> PhysicsWorld::raycast(glm::vec3 origin, glm::vec3 direction,
                                                  float distance) const {
    if (!finite(origin) || !finite(direction) || !std::isfinite(distance) ||
        distance <= 0.0f || glm::length(direction) < 0.0001f)
        throw std::invalid_argument("physics ray requires finite origin, direction, and positive distance");
    const glm::vec3 delta = glm::normalize(direction) * distance;
    const JPH::RRayCast ray(JPH::RVec3(origin.x, origin.y, origin.z),
                            JPH::Vec3(delta.x, delta.y, delta.z));
    JPH::RayCastResult hit;
    if (!impl_->system.GetNarrowPhaseQuery().CastRay(ray, hit)) return std::nullopt;
    const auto found = impl_->entity_ids.find(hit.mBodyID.GetIndexAndSequenceNumber());
    if (found == impl_->entity_ids.end()) return std::nullopt;
    const glm::vec3 position = origin + delta * hit.mFraction;
    return PhysicsRayHit{found->second, position, distance * hit.mFraction};
}

std::vector<uint64_t> PhysicsWorld::overlap_sphere(glm::vec3 center, float radius) const {
    if (!finite(center) || !std::isfinite(radius) || radius <= 0.0f)
        throw std::invalid_argument("physics overlap sphere requires finite center and positive radius");
    JPH::RefConst<JPH::Shape> shape = new JPH::SphereShape(radius);
    JPH::AllHitCollisionCollector<JPH::CollideShapeCollector> hits;
    impl_->system.GetNarrowPhaseQuery().CollideShape(
        shape, JPH::Vec3::sReplicate(1.0f),
        JPH::RMat44::sTranslation(JPH::RVec3(center.x, center.y, center.z)),
        JPH::CollideShapeSettings{}, JPH::RVec3::sZero(), hits);
    std::set<uint64_t> ids;
    for (const auto& hit : hits.mHits) {
        const auto found = impl_->entity_ids.find(hit.mBodyID2.GetIndexAndSequenceNumber());
        if (found != impl_->entity_ids.end()) ids.insert(found->second);
    }
    return {ids.begin(), ids.end()};
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
