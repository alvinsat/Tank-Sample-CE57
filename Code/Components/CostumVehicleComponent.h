#pragma once

#include <CryRenderer/IRenderer.h>
#include <CryRenderer/IShader.h>

#include <CrySchematyc/ResourceTypes.h>
#include <CrySchematyc/MathTypes.h>

#include <CryMath/Angle.h>
#include <CryPhysics/physinterface.h>

#include <CrySchematyc/Env/IEnvRegistry.h>
#include <CrySchematyc/Env/IEnvRegistrar.h>
#include <CrySchematyc/Env/Elements/EnvComponent.h>
#include <CryGame/IGameFramework.h>

#include <CrySchematyc/Env/IEnvRegistrar.h>
#include <CrySchematyc/Env/Elements/EnvComponent.h>
#include <CrySchematyc/Env/Elements/EnvFunction.h>
#include <CrySchematyc/Env/Elements/EnvSignal.h>

/*
TODO : Use singular place for local pedal and so on, player input will fill this then on update will fetch this data and apply to the vehicle, this way we can have multiple input sources (player, ai, network) without changing the vehicle logic
so flow would be = player input tell to constum vehicle -> on update vehicle component listens to this input
*/


class CCostumVehicleComponent : public IEntityComponent
{
protected:
    static void Register(Schematyc::CEnvRegistrationScope &componentScope);

    // IEntityComponent
    virtual void Initialize() final;

    virtual void ProcessEvent(const SEntityEvent &event) final;
    virtual Cry::Entity::EventFlags GetEventMask() const final;
    // ~IEntityComponent

public:
    float userElapsedPedal = 0.0f;
    int userElapsedGear = 0;
	int userElapsedHandbrake = 0;
    float m_cachedBrakeRatio = 0.0f;
    float m_cachedSteering = 0.0f;

    struct SCollisionSignal
    {
        EntityId otherEntity = INVALID_ENTITYID;
        Schematyc::SurfaceTypeName surfaceType;

        SCollisionSignal() {};
        SCollisionSignal(EntityId id, const Schematyc::SurfaceTypeName &srfType) : otherEntity(id), surfaceType(srfType) {}
    };

    struct SEngineParams
    {
        inline bool operator==(const SEngineParams &rhs) const { return 0 == memcmp(this, &rhs, sizeof(rhs)); }

        static void ReflectType(Schematyc::CTypeDesc<SEngineParams> &desc)
        {
            desc.SetGUID("{273E1519-CD3E-4C46-9D9F-65E0690120FD}"_cry_guid);
            desc.SetLabel("Engine Parameters");
            desc.AddMember(&CCostumVehicleComponent::SEngineParams::m_power, 'powe', "Power", "Power", "Power of the engine", 10000.f);
            desc.AddMember(&CCostumVehicleComponent::SEngineParams::m_maxRPM, 'maxr', "MaxRPM", "Maximum RPM", "engine torque decreases to 0 after reaching this rotation speed", 1200.f);
            desc.AddMember(&CCostumVehicleComponent::SEngineParams::m_minRPM, 'minr', "MinRPM", "Minimum RPM", "disengages the clutch when falling behind this limit, if braking with the engine", 60.f);
            desc.AddMember(&CCostumVehicleComponent::SEngineParams::m_idleRPM, 'idle', "IdleRPM", "Idle RPM", "RPM for idle state", 120.f);
            desc.AddMember(&CCostumVehicleComponent::SEngineParams::m_startRPM, 'star', "StartRPM", "Start RPM", "RPM when the engine is started", 400.f);
        }

        Schematyc::Range<0, 10000000> m_power = 50000.f;
        float m_maxRPM = 1200.f;
        float m_minRPM = 60.f;
        float m_idleRPM = 120.f;
        float m_startRPM = 400.f;
    };

    struct SGearParams
    {
        inline bool operator==(const SGearParams &rhs) const { return 0 == memcmp(this, &rhs, sizeof(rhs)); }

        struct SGear
        {
            inline bool operator==(const SGear &rhs) const { return 0 == memcmp(this, &rhs, sizeof(rhs)); }
            inline bool operator!=(const SGear &rhs) const { return 0 != memcmp(this, &rhs, sizeof(rhs)); }

            void Serialize(Serialization::IArchive &archive)
            {
                archive(m_ratio, "Ratio", "Ratio");
            }

            static void ReflectType(Schematyc::CTypeDesc<SGear> &desc)
            {
                desc.SetGUID("{6DC3D110-9CB3-4E9F-AD40-BCA28033080B}"_cry_guid);
                desc.SetLabel("Gear");
                desc.AddMember(&CCostumVehicleComponent::SGearParams::SGear::m_ratio, 'rati', "Ratio", "Ratio", "assumes 0-backward gear, 1-neutral, 2 and above - forward", 2.f);
            }

            Schematyc::Range<0, 2> m_ratio;
        };

        static void ReflectType(Schematyc::CTypeDesc<SGearParams> &desc)
        {
            desc.SetGUID("{D61B5ACC-86A6-445B-AB21-A6B7AD713161}"_cry_guid);
            desc.SetLabel("Gear Parameters");
            desc.AddMember(&CCostumVehicleComponent::SGearParams::m_gears, 'gear', "Gears", "Gears", "Specifies number of gears, and their parameters", Schematyc::CArray<CCostumVehicleComponent::SGearParams::SGear>());
            desc.AddMember(&CCostumVehicleComponent::SGearParams::m_shiftUpRPM, 'shiu', "ShiftUpRPM", "Shift Up RPM", "RPM threshold for for automatic gear switching", 600.f);
            desc.AddMember(&CCostumVehicleComponent::SGearParams::m_shiftDownRPM, 'shid', "ShiftDownRPM", "Shift Down RPM", "RPM threshold for for automatic gear switching", 240.f);
            desc.AddMember(&CCostumVehicleComponent::SGearParams::m_directionSwitchRPM, 'dirs', "DirectionSwitchRPM", "Direction Switch RPM", "RPM threshold for switching back and forward gears", 10.f);
        }

        Schematyc::CArray<SGear> m_gears;

        float m_shiftUpRPM = 600.f;
        float m_shiftDownRPM = 240.f;

        float m_directionSwitchRPM = 10.f;
    };

    enum class EGear
    {
        Reverse = -1,
        Neutral,
        // First forward gear, note how there can be more specified by the user
        Forward,

        DefaultCount = Forward + 2
    };

    CCostumVehicleComponent();
    virtual ~CCostumVehicleComponent();
    void ResetVehicleState();

    static void ReflectType(Schematyc::CTypeDesc<CCostumVehicleComponent> &desc)
    {
        desc.SetGUID("{D9B627BC-0224-4796-91DE-E22D8ACE1105}"_cry_guid);
        desc.SetEditorCategory("Game");
        desc.SetLabel("Costum Vehicle Physics");
        desc.SetDescription("");
        desc.SetIcon("icons:ObjectTypes/object.ico");
        desc.SetComponentFlags({IEntityComponent::EFlags::Socket, IEntityComponent::EFlags::Attach, IEntityComponent::EFlags::Singleton});

        // Mark the Character Controller component as incompatible
        desc.AddComponentInteraction(SEntityComponentRequirements::EType::Incompatibility, "{103E5A23-61DB-4773-9EE9-9A3B748E3964}"_cry_guid);
        // Mark the RigidBody component as incompatible
        desc.AddComponentInteraction(SEntityComponentRequirements::EType::Incompatibility, "{3BED4BFA-CFD6-4EAA-8612-AF195CA90D37}"_cry_guid);
        // Mark the Area component as incompatible
        desc.AddComponentInteraction(SEntityComponentRequirements::EType::Incompatibility, "{1CAA18EC-43B2-416A-B639-9F5ABD5BB059}"_cry_guid);

        desc.AddMember(&CCostumVehicleComponent::m_engineParams, 'engn', "EngineParams", "Engine Parameters", nullptr, CCostumVehicleComponent::SEngineParams());
        desc.AddMember(&CCostumVehicleComponent::m_gearParams, 'gear', "GearParams", "Gear Parameters", nullptr, CCostumVehicleComponent::SGearParams());
        desc.AddMember(&CCostumVehicleComponent::m_bSendCollisionSignal, 'send', "SendCollisionSignal", "Send Collision Signal", "Whether or not this component should listen for collisions and report them", false);
    }

    virtual void UseHandbrake(bool bSet)
    {
        userElapsedHandbrake = bSet ? 1 : 0;
        ApplyCachedDriveState();
    }
    bool IsUsingHandbrake() const { return userElapsedHandbrake != 0; }

    virtual void SetThrottle(Schematyc::Range<0, 1> ratio)
    {
        if (ratio > 0.0f)
        {
            UseHandbrake(false);
            m_cachedBrakeRatio = 0.0f;
        }
		userElapsedPedal = ratio;
        ApplyCachedDriveState();
    }
    float GetThrottle() const { return userElapsedPedal; }

    void SetBrake(Schematyc::Range<0, 1> ratio)
    {
        m_cachedBrakeRatio = ratio;
        if (ratio > 0.0f)
        {
            userElapsedPedal = 0.0f;
        }
        ApplyCachedDriveState();
    }
    Schematyc::Range<0, 1> GetBrake() const { return m_cachedBrakeRatio; }

    virtual void SetCurrentGear(int gearId)
    {
        userElapsedGear = gearId;
        ApplyCachedDriveState();
    }
    int GetCurrentGear() const { return userElapsedGear; }

    virtual void GearUp() { SetCurrentGear(min(GetCurrentGear() + 1, (int)m_gearParams.m_gears.Size() - 2)); }
    virtual void GearDown() { SetCurrentGear(max(GetCurrentGear() - 1, (int)EGear::Reverse)); }

    virtual void SetSteeringAngle(CryTransform::CAngle angle)
    {
        m_cachedSteering = angle.ToRadians();
        ApplyCachedDriveState();
    }
    CryTransform::CAngle GetSteeringAngle() const { return CryTransform::CAngle::FromRadians(m_cachedSteering); }

    float GetEngineRPM() const { return m_vehicleStatus.engineRPM; }
    float GetTorque() const { return m_vehicleStatus.drivingTorque; }
    bool HasWheelContact() const { return m_vehicleStatus.bWheelContact != 0; }

    virtual void SetVelocity(const Vec3 &velocity)
    {
        if (IPhysicalEntity *pPhysicalEntity = m_pEntity->GetPhysics())
        {
            pe_action_set_velocity action_set_velocity;
            action_set_velocity.v = velocity;
            pPhysicalEntity->Action(&action_set_velocity);
        }
    }

    Vec3 GetVelocity() const
    {
        if (IPhysicalEntity *pPhysicalEntity = m_pEntity->GetPhysics())
        {
            pe_status_dynamics dynStatus;
            if (pPhysicalEntity->GetStatus(&dynStatus))
            {
                return dynStatus.v;
            }
        }

        return ZERO;
    }

    virtual void SetAngularVelocity(const Vec3 &angularVelocity)
    {
        if (IPhysicalEntity *pPhysicalEntity = m_pEntity->GetPhysics())
        {
            pe_action_set_velocity action_set_velocity;
            action_set_velocity.w = angularVelocity;
            pPhysicalEntity->Action(&action_set_velocity);
        }
    }

    Vec3 GetAngularVelocity() const
    {
        if (IPhysicalEntity *pPhysicalEntity = m_pEntity->GetPhysics())
        {
            pe_status_dynamics dynStatus;
            if (pPhysicalEntity->GetStatus(&dynStatus))
            {
                return dynStatus.w;
            }
        }

        return ZERO;
    }

    virtual void ApplyImpulse(const Vec3 &force)
    {
        // Only dispatch the impulse to physics if one was provided
        if (!force.IsZero())
        {
            if (IPhysicalEntity *pPhysicalEntity = m_pEntity->GetPhysics())
            {
                pe_action_impulse impulseAction;
                impulseAction.impulse = force;

                pPhysicalEntity->Action(&impulseAction);
            }
        }
    }

    virtual void ApplyAngularImpulse(const Vec3 &force)
    {
        // Only dispatch the impulse to physics if one was provided
        if (!force.IsZero())
        {
            if (IPhysicalEntity *pPhysicalEntity = m_pEntity->GetPhysics())
            {
                pe_action_impulse impulseAction;
                impulseAction.angImpulse = force;

                pPhysicalEntity->Action(&impulseAction);
            }
        }
    }

protected:
    void ClearRuntimeCache();
    void PhysicalizeVehicle();
    void DephysicalizeVehicle();
    void ApplyCachedDriveState();

    // Needs to be persistent since physics is on another thread
    std::vector<float> m_gearRatios;

    SEngineParams m_engineParams;
    SGearParams m_gearParams;

    pe_status_vehicle m_vehicleStatus;
    bool m_bSendCollisionSignal = false;
};
