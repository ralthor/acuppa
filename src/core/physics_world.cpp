#include <Jolt/Jolt.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/IssueReporting.h>
#include <Jolt/Core/JobSystemSingleThreaded.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Collision/ContactListener.h>
#include <Jolt/Physics/Collision/CollisionCollectorImpl.h>
#include <Jolt/Physics/Collision/CollideShape.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/NarrowPhaseQuery.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/OffsetCenterOfMassShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Vehicle/VehicleConstraint.h>
#include <Jolt/Physics/Vehicle/VehicleCollisionTester.h>
#include <Jolt/Physics/Vehicle/WheeledVehicleController.h>

#include "acuppa/physics_world.hpp"

#include <glm/geometric.hpp>

#include <cmath>
#include <cstdio>
#include <map>
#include <set>
#include <stdexcept>

namespace acuppa {
namespace {

struct JoltRuntime {
    JoltRuntime() {
        JPH::RegisterDefaultAllocator();
#ifdef JPH_ENABLE_ASSERTS
        JPH::AssertFailed = [](const char* expression, const char* message,
                               const char* file, JPH::uint line) -> bool {
            std::fprintf(stderr, "Jolt assertion at %s:%u: %s%s%s\n", file, line,
                         expression, message ? ": " : "", message ? message : "");
            return true;
        };
#endif
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
    std::map<uint64_t, JPH::Ref<JPH::VehicleConstraint>> vehicles;
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
        for (const auto& [id, vehicle] : vehicles) {
            system.RemoveStepListener(vehicle);
            system.RemoveConstraint(vehicle);
        }
        vehicles.clear();
        auto& iface = system.GetBodyInterface();
        for (const auto& [id, body] : bodies) {
            iface.RemoveBody(body);
            iface.DestroyBody(body);
        }
    }

    void OnContactAdded(const JPH::Body& a, const JPH::Body& b,
                        const JPH::ContactManifold& manifold, JPH::ContactSettings&) override {
        const std::pair<uint64_t, uint64_t> pair{
            std::min(a.GetUserData(), b.GetUserData()),
            std::max(a.GetUserData(), b.GetUserData())};
        if (active_contacts.emplace(pair, a.IsSensor() || b.IsSensor()).second) {
            const JPH::Vec3 normal = manifold.mWorldSpaceNormal;
            const JPH::Vec3 relative = b.GetLinearVelocity() - a.GetLinearVelocity();
            const float speed = std::max(0.0f, -relative.Dot(normal));
            const float sign = a.GetUserData() == pair.first ? 1.0f : -1.0f;
            events.push_back({pair.first, pair.second, a.IsSensor() || b.IsSensor(),
                              true, {sign * normal.GetX(), sign * normal.GetY(),
                                     sign * normal.GetZ()}, speed});
        }
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
    if (!std::isfinite(desc.density) || desc.density <= 0.0f)
        throw std::invalid_argument("physics density must be finite and positive");
    if (!std::isfinite(desc.friction) || desc.friction < 0.0f)
        throw std::invalid_argument("physics friction must be finite and non-negative");
    if (!std::isfinite(desc.restitution) || desc.restitution < 0.0f ||
        desc.restitution > 1.0f)
        throw std::invalid_argument("physics restitution must be between 0 and 1");
    if (!std::isfinite(desc.linear_damping) || desc.linear_damping < 0.0f ||
        !std::isfinite(desc.angular_damping) || desc.angular_damping < 0.0f)
        throw std::invalid_argument("physics damping must be finite and non-negative");

    JPH::RefConst<JPH::Shape> shape;
    if (desc.shape == ColliderShape::Box) {
        auto* box = new JPH::BoxShape(JPH::Vec3(desc.half_extents.x, desc.half_extents.y,
                                                desc.half_extents.z));
        box->SetDensity(desc.density);
        shape = box;
    } else if (desc.shape == ColliderShape::Sphere) {
        auto* sphere = new JPH::SphereShape(desc.radius);
        sphere->SetDensity(desc.density);
        shape = sphere;
    } else {
        throw std::invalid_argument("unknown collider shape");
    }

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
    settings.mFriction = desc.friction;
    settings.mRestitution = desc.restitution;
    settings.mLinearDamping = desc.linear_damping;
    settings.mAngularDamping = desc.angular_damping;
    settings.mMotionQuality = desc.ccd ? JPH::EMotionQuality::LinearCast
                                       : JPH::EMotionQuality::Discrete;
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
    if (const auto vehicle = impl_->vehicles.find(id); vehicle != impl_->vehicles.end()) {
        impl_->system.RemoveStepListener(vehicle->second);
        impl_->system.RemoveConstraint(vehicle->second);
        impl_->vehicles.erase(vehicle);
    }
    auto& iface = impl_->system.GetBodyInterface();
    // Jolt leaves sleeping bodies resting on a removed body asleep, so they would
    // hang in mid-air; wake everything touching its slightly grown bounds.
    JPH::AABox bounds = iface.GetTransformedShape(it->second).GetWorldSpaceBounds();
    bounds.ExpandBy(JPH::Vec3::sReplicate(0.05f));
    iface.RemoveBody(it->second);
    iface.ActivateBodiesInAABox(bounds, {}, {});
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

void PhysicsWorld::add_vehicle(uint64_t id, const VehicleConfig& config) {
    const auto body = impl_->bodies.find(id);
    if (body == impl_->bodies.end() || impl_->vehicles.count(id))
        throw std::invalid_argument("vehicle needs a unique existing body");
    if (impl_->system.GetBodyInterface().GetMotionType(body->second) != JPH::EMotionType::Dynamic)
        throw std::invalid_argument("vehicle body must be dynamic");
    if (!std::isfinite(config.half_track) || config.half_track <= 0.0f ||
        !std::isfinite(config.half_wheelbase) || config.half_wheelbase <= 0.0f ||
        !std::isfinite(config.wheel_radius) || config.wheel_radius <= 0.0f ||
        !std::isfinite(config.engine_torque) || config.engine_torque <= 0.0f ||
        !std::isfinite(config.rear_grip) || config.rear_grip <= 0.0f ||
        config.rear_grip > 2.0f || !std::isfinite(config.center_of_mass_y) ||
        config.center_of_mass_y < -1.0f || config.center_of_mass_y > 0.0f)
        throw std::invalid_argument("invalid vehicle dimensions, torque, grip, or center of mass");
    if (config.center_of_mass_y != 0.0f) {
        auto& iface = impl_->system.GetBodyInterface();
        const JPH::RefConst<JPH::Shape> shape = iface.GetShape(body->second);
        iface.SetShape(body->second,
                       new JPH::OffsetCenterOfMassShape(shape.GetPtr(),
                                                          JPH::Vec3(0, config.center_of_mass_y, 0)),
                       true, JPH::EActivation::Activate);
    }
    JPH::BodyLockWrite lock(impl_->system.GetBodyLockInterface(), body->second);
    if (!lock.Succeeded()) throw std::runtime_error("vehicle body lock failed");
    JPH::VehicleConstraintSettings settings;
    settings.mForward = JPH::Vec3(0, 0, 1);
    settings.mMaxPitchRollAngle = JPH::DegreesToRadians(65.0f);
    for (int i = 0; i < 4; ++i) {
        auto* wheel = new JPH::WheelSettingsWV;
        wheel->mPosition = JPH::Vec3(i % 2 == 0 ? -config.half_track : config.half_track,
                                    -0.1f, i < 2 ? config.half_wheelbase : -config.half_wheelbase);
        wheel->mWheelForward = settings.mForward;
        wheel->mRadius = config.wheel_radius;
        wheel->mWidth = 0.16f;
        wheel->mSuspensionMinLength = 0.15f;
        wheel->mSuspensionMaxLength = 0.4f;
        wheel->mSuspensionPreloadLength = 0.0f;
        wheel->mSuspensionSpring.mFrequency = 1.5f;
        wheel->mSuspensionSpring.mDamping = 0.9f;
        wheel->mMaxSteerAngle = i < 2 ? 0.55f : 0.0f;
        wheel->mMaxHandBrakeTorque = i < 2 ? 0.0f : 4000.0f;
        settings.mWheels.push_back(wheel);
    }
    auto* controller_settings = new JPH::WheeledVehicleControllerSettings;
    controller_settings->mEngine.mMaxTorque = config.engine_torque;
    controller_settings->mDifferentials.resize(1);
    controller_settings->mDifferentials[0].mLeftWheel = 2;
    controller_settings->mDifferentials[0].mRightWheel = 3;
    settings.mController = controller_settings;
    JPH::Ref<JPH::VehicleConstraint> vehicle = new JPH::VehicleConstraint(lock.GetBody(), settings);
    vehicle->SetVehicleCollisionTester(new JPH::VehicleCollisionTesterRay(kMoving));
    auto* controller = static_cast<JPH::WheeledVehicleController*>(vehicle->GetController());
    controller->SetTireMaxImpulseCallback(
        [config, controller](JPH::uint wheel, float& longitudinal, float& lateral,
                             float suspension, float longitudinal_friction, float lateral_friction,
                             float, float, float) {
            longitudinal = 10.0f * longitudinal_friction * suspension;
            const float rear = wheel >= 2 ? config.rear_grip : 1.0f;
            const float handbrake = wheel >= 2 ? 1.0f - 0.7f * controller->GetHandBrakeInput() : 1.0f;
            // Ordinary turns need lateral traction; the handbrake still breaks rear grip.
            lateral = 2.5f * rear * handbrake * lateral_friction * suspension;
        });
    impl_->system.AddConstraint(vehicle);
    impl_->system.AddStepListener(vehicle);
    impl_->vehicles.emplace(id, vehicle);
}

void PhysicsWorld::set_vehicle_input(uint64_t id, float throttle, float steer,
                                      float brake, float handbrake) {
    if (!std::isfinite(throttle) || throttle < -1 || throttle > 1 ||
        !std::isfinite(steer) || steer < -1 || steer > 1 ||
        !std::isfinite(brake) || brake < 0 || brake > 1 ||
        !std::isfinite(handbrake) || handbrake < 0 || handbrake > 1)
        throw std::invalid_argument("vehicle input must be finite and within its control range");
    const auto found = impl_->vehicles.find(id);
    if (found == impl_->vehicles.end()) throw std::invalid_argument("unknown vehicle ID");
    static_cast<JPH::WheeledVehicleController*>(found->second->GetController())
        ->SetDriverInput(throttle, steer, brake, handbrake);
    if (throttle || steer || brake || handbrake)
        impl_->system.GetBodyInterface().ActivateBody(impl_->bodies.at(id));
}

std::optional<VehicleState> PhysicsWorld::vehicle_state(uint64_t id) const {
    const auto found = impl_->vehicles.find(id);
    if (found == impl_->vehicles.end()) return std::nullopt;
    VehicleState result;
    result.engine_rpm = static_cast<const JPH::WheeledVehicleController*>(
        found->second->GetController())->GetEngine().GetCurrentRPM();
    for (int i = 0; i < 4; ++i) {
        const auto* wheel = static_cast<const JPH::WheelWV*>(found->second->GetWheel(i));
        result.wheels[i] = {wheel->HasContact(), wheel->GetSteerAngle(),
                            wheel->GetRotationAngle(), wheel->mLateralSlip};
    }
    return result;
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

}  // namespace acuppa
