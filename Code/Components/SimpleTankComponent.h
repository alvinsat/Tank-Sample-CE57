// SoftLeafGame, Alvin Satria Nugraha, 2026

#pragma once

#include <CryEntitySystem/IEntityComponent.h>
#include <CryEntitySystem/IEntity.h>
#include <CryMath/Cry_Geo.h>
#include <CryMath/Cry_Color.h>
#include <CryMath/Cry_Math.h>
#include <CrySchematyc/Utils/SharedString.h>
#include <CryCore/Containers/CryArray.h>
#include <DefaultComponents/Input/InputComponent.h>
#include <CrySchematyc/Env/IEnvRegistry.h>
#include <CrySchematyc/Env/IEnvRegistrar.h>
#include <CrySchematyc/Env/Elements/EnvComponent.h>
#include <CryGame/IGameFramework.h>
#include <CryPhysics/IPhysics.h>
#include <CrySystem/ILog.h>
#include <DefaultComponents/Physics/Vehicles/VehicleComponent.h>
#include <DefaultComponents/Physics/Vehicles/WheelComponent.h>
#include "CostumVehicleComponent.h"
#include <vector>

// Forward declarations
struct IPhysicalEntity;
struct IParticleEffect;

class CSimpleTankComponent : public IEntityComponent
{
    // CRY_DECLARE_COMPONENT_RTTI is not available in this CryEngine version.

public:  // ← FIX: All members that need external access must be public
    CSimpleTankComponent() = default;
    virtual ~CSimpleTankComponent() override = default;

    // Schematyc reflection
    static void ReflectType(Schematyc::CTypeDesc<CSimpleTankComponent>& desc);
    static void Register(Schematyc::CEnvRegistrationScope& componentScope);

    // IEntityComponent interface
    virtual void Initialize() override;
    virtual Cry::Entity::EventFlags GetEventMask() const override;
    virtual void ProcessEvent(const SEntityEvent& event) override;

    // === Public Movement API ===
    void MoveForward(float speed);
    void MoveBackward(float speed);
    void RotateBody(float rotationAngle);
    virtual void RotateTurret(float yaw, float pitch, float deltaTime);

    // === Query Functions ===
    float GetCurrentSpeed() const { return m_currentSpeed; }
    float GetCurrentTurretYaw() const { return m_currentTurretYaw; }
    float GetCurrentTurretPitch() const { return m_currentTurretPitch; }
    float GetMainCannonCooldown() const { return m_mainCannonCooldown; }
    float GetHullHealth() const {
        return (m_hullDamageMax > 0.0f) ?
            ((m_hullDamageMax - m_hullCurrentDamage) / m_hullDamageMax) * 100.0f : 100.0f;
    }
    bool IsDestroyed() const { return m_isDestroyed; }

    // === Damage Handling ===
    void ApplyDamage(float damage);
    void OnHit(float damage, const Vec3& hitPosition, const Vec3& hitNormal);
    void DebugLog(float deltaTime);

    // === Editor-Exposed Properties ===
    Schematyc::AnyModelFileName m_bodyMesh = "softleafmaster/models/abrams/m1a1_abrams.cga";
    DynArray<Cry::DefaultComponents::CWheelComponent*> existingWheels;


    // Movement
    float m_maxSpeed = 15.0f;
    float m_rotationSpeed = 45.0f;
    float m_accelerationRate = 10.0f;
    float m_minTurretPitch = -10.0f;
    float m_maxTurretPitch = 35.0f;
    float m_mainCannonReloadTime = 2.0f;
    bool  m_inverse_input = false;

    // Positions
    Vec3 m_turretPos = Vec3(0.0f, 0.1204f, 1.584f);
    Vec3 m_cannonStartPos = Vec3(0.0f, 3.8f, 1.9f);
    Vec3 m_cannonOutPos = Vec3(0.0f, 5.8f, 1.9f);

    // Hull damage
    float m_hullDamageMax = 1500.0f;
    float m_hullCurrentDamage = 0.0f;

    // Effects
    string m_muzzleFlashEffect = "Vehicles.Abrams.Weapon.MuzzleFlash";
    string m_impactEffect = "Vehicles.Abrams.Weapon.Impact";
    string m_damageHitEffect = "Vehicles.Abrams.Damage.Impact";
    string m_explosionEffect = "Vehicles.Abrams.Destroy.Explosion";

    // Debug
    bool enableLogs = true;

    // Combat state exposed for barrel and other external components
    bool m_isFiring = false;
    float m_mainCannonCooldown = 0.0f;

protected:
    // === Physics & Geometry ===
    int m_bodySlot = -1;
    int m_turretSlot = -1;
    bool m_physicsInitialized = false;
    bool m_wheelSetupDone = false;
    bool m_isRefreshingVehicleRuntimeState = false;
    bool m_ignoreNextPhysicalTypeChange = false;
    CCostumVehicleComponent* m_pVehiclePhysicsComponent = nullptr;
    std::vector<Cry::DefaultComponents::CWheelComponent*> m_wheelComponents;

    // === Input State ===
    Cry::DefaultComponents::CInputComponent* m_pInputComponent = nullptr;
    float m_forwardInputLevel = 0.0f;
    float m_turnInputLevel = 0.0f;
    Vec2 m_mouseDeltaRotation = Vec2(ZERO);

    // === Movement State ===
    float m_currentSpeed = 0.0f;
    float m_currentRotationVelocity = 0.0f;
    float m_targetRotationVelocity = 0.0f;

    // === Turret State ===
    float m_currentTurretYaw = 0.0f;
    float m_targetTurretYaw = 0.0f;
    float m_currentTurretPitch = 0.0f;
    float m_targetTurretPitch = 0.0f;

    // === Combat State (MOVED TO PUBLIC ABOVE) ===
    // bool m_isFiring = false;        ← Now public
    // bool m_isDestroyed = false;     ← Now public  
    // float m_mainCannonCooldown = 0.0f; ← Now public

    // === Internal Helpers ===
    void LoadBodyMesh();
    void InitializeInput();
    void ResetState();
    void UpdateMovement(float deltaTime);
    void UpdateTurret(float deltaTime);
    void UpdateCooldowns(float deltaTime);
    void SetupVehiclePhysics();
    void RefreshVehicleRuntimeState();
    void CreateWheelComponents();
    void UpdateVehicleDrive(float deltaTime);
    virtual void FireMainCannon();
    virtual void StopFiring();
    void OnTankDestroyed();
    void PlayParticleEffect(const string& effectName, const Vec3& position, const Vec3& direction);

private:
    // Combat state - kept private if not needed externally
    bool m_isDestroyed = false;

    // Prevent copying
    CSimpleTankComponent(const CSimpleTankComponent&) = delete;
    CSimpleTankComponent& operator=(const CSimpleTankComponent&) = delete;
};
