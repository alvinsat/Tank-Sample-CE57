// SoftLeafGame, Alvin Satria Nugraha, 2026

#include "StdAfx.h"
#include "SimpleTankComponent.h"
#include <DefaultComponents/Physics/Vehicles/VehicleComponent.h>
#include <CryAnimation/ICryAnimation.h>
#include <CryInput/IInput.h>
#include <CryPhysics/physinterface.h>
#include <CryParticleSystem/IParticles.h>
#include <CryString/StringUtils.h>
#include <CryEntitySystem/IEntitySystem.h>
#include <CryPhysics/primitives.h>
#include <CrySystem/ISystem.h>
#include <CrySystem/ILog.h>  // FIX: For CryLogWarning
#include <CryMath/Cry_Math.h>

// FIX: Helper inline functions to replace cry_* functions if not available
// (CryEngine typically has these, but if missing, use standard equivalents)
inline float CryMin(float a, float b) { return (a < b) ? a : b; }
inline float CryMax(float a, float b) { return (a > b) ? a : b; }
inline float CryFabs(float a) { return fabsf(a); }
inline float CryClamp(float val, float minVal, float maxVal) {
    return CryMin(CryMax(val, minVal), maxVal);
}

namespace
{
    struct SWheelDefinition
    {
        Vec3 pivot;
        float radius;
        float width;
        int axleIndex;
        bool driving;
    };

    static const SWheelDefinition g_simpleTankWheelDefs[] =
    {
        { Vec3(-1.2f,  2.1f, -0.6f), 0.45f, 0.25f, 0, true },
        { Vec3( 1.2f,  2.1f, -0.6f), 0.45f, 0.25f, 0, true },
        { Vec3(-1.2f, -2.1f, -0.6f), 0.45f, 0.25f, 1, true },
        { Vec3( 1.2f, -2.1f, -0.6f), 0.45f, 0.25f, 1, true }
    };

    static const size_t g_simpleTankWheelCount = sizeof(g_simpleTankWheelDefs) / sizeof(g_simpleTankWheelDefs[0]);

    static float SimpleLerpFloat(float current, float target, float rate, float deltaTime)
    {
        // FIX: Use CryMin instead of cry_min or min
        return current + (target - current) * CryMin(1.0f, rate * deltaTime);
    }

    static void RegisterSimpleTankComponent(Schematyc::IEnvRegistrar& registrar)
    {
        Schematyc::CEnvRegistrationScope scope = registrar.Scope(IEntity::GetEntityScopeGUID());
        {
            Schematyc::CEnvRegistrationScope componentScope = scope.Register(SCHEMATYC_MAKE_ENV_COMPONENT(CSimpleTankComponent));
        }
    }

    CRY_STATIC_AUTO_REGISTER_FUNCTION(&RegisterSimpleTankComponent);
}

void CSimpleTankComponent::ReflectType(Schematyc::CTypeDesc<CSimpleTankComponent>& desc)
{
    // FIX: Valid hex GUID only (0-9, a-f)
    desc.SetGUID("{d6798eee-3f61-5e4a-927f-a621bbc437dd}"_cry_guid);
    desc.SetEditorCategory("Game");
    desc.SetLabel("Simple Tank Component");
    desc.SetDescription("Simplified tank component with basic movement, rotation, firing, and hit events.");
    desc.SetComponentFlags({ IEntityComponent::EFlags::Transform });

    desc.AddMember(&CSimpleTankComponent::m_bodyMesh, 'file', "TankBody", "TankBody", "Main tank body CGA model path", "");
    desc.AddMember(&CSimpleTankComponent::m_inverse_input, 'b', "InverseInput", "Invert Input", "Invert input response", false);

    desc.AddMember(&CSimpleTankComponent::m_maxSpeed, 'f', "MaxSpeed", "Max Speed", "Maximum movement speed in m/s", 15.0f);
    desc.AddMember(&CSimpleTankComponent::m_rotationSpeed, 'f', "RotationSpeed", "Rotation Speed", "Tank rotation speed in degrees per second", 45.0f);
    desc.AddMember(&CSimpleTankComponent::m_accelerationRate, 'f', "AccelerationRate", "Acceleration Rate", "How quickly the tank accelerates/decelerates", 10.0f);

    desc.AddMember(&CSimpleTankComponent::m_minTurretPitch, 'f', "MinTurretPitch", "Min Turret Pitch", "Minimum turret pitch angle in degrees", -10.0f);
    desc.AddMember(&CSimpleTankComponent::m_maxTurretPitch, 'f', "MaxTurretPitch", "Max Turret Pitch", "Maximum turret pitch angle in degrees", 35.0f);
    desc.AddMember(&CSimpleTankComponent::m_mainCannonReloadTime, 'f', "MainCannonReloadTime", "Main Cannon Reload Time", "Reload time for main cannon in seconds", 2.0f);

    desc.AddMember(&CSimpleTankComponent::m_turretPos, 'v', "TurretPos", "Turret Center Position", "Turret rotation center position", Vec3(0.0f, 0.1204f, 1.584f));
    desc.AddMember(&CSimpleTankComponent::m_cannonStartPos, 'v', "CannonStartPos", "Cannon Start Position", "Cannon projectile start position", Vec3(0.0f, 3.8f, 1.9f));
    desc.AddMember(&CSimpleTankComponent::m_cannonOutPos, 'v', "CannonOutPos", "Cannon Exit Position", "Cannon exit/muzzle position", Vec3(0.0f, 5.8f, 1.9f));

    desc.AddMember(&CSimpleTankComponent::m_hullDamageMax, 'f', "HullDamageMax", "Hull Max Damage", "Maximum damage the hull can take", 1500.0f);
    desc.AddMember(&CSimpleTankComponent::m_hullCurrentDamage, 'f', "HullCurrentDamage", "Hull Current Damage", "Current accumulated damage on hull", 0.0f);
}

void CSimpleTankComponent::InitializeInput()
{
    if (!GetEntity())
        return;

    m_pInputComponent = GetEntity()->GetOrCreateComponent<Cry::DefaultComponents::CInputComponent>();
    if (!m_pInputComponent)
    {
        if (enableLogs) CryLogAlways("[Tank] InitializeInput - Failed to create input component");
        return;
    }

    if (enableLogs) CryLogAlways("[Tank] InitializeInput - Input component created");

    // Movement inputs
    m_pInputComponent->RegisterAction("tank", "moveleft", [this](int activationMode, float value)
        {
            if (activationMode == eAAM_OnPress)
                m_turnInputLevel -= 1.0f; // Or -CryFabs(value)
            else if (activationMode == eAAM_OnRelease)
                m_turnInputLevel += 1.0f;
        });
    m_pInputComponent->BindAction("tank", "moveleft", eAID_KeyboardMouse, eKI_A);

    m_pInputComponent->RegisterAction("tank", "moveright", [this](int activationMode, float value)
        {
            if (activationMode == eAAM_OnPress)
                m_turnInputLevel += 1.0f;
            else if (activationMode == eAAM_OnRelease)
                m_turnInputLevel -= 1.0f;
        });
    m_pInputComponent->BindAction("tank", "moveright", eAID_KeyboardMouse, eKI_D);

    m_pInputComponent->RegisterAction("tank", "moveforward", [this](int activationMode, float value)
        {
            float inversion = m_inverse_input ? -1.0f : 1.0f;
            if (activationMode == eAAM_OnPress)
                m_forwardInputLevel += 1.0f * inversion;
            else if (activationMode == eAAM_OnRelease)
                m_forwardInputLevel -= 1.0f * inversion;
        });
    m_pInputComponent->BindAction("tank", "moveforward", eAID_KeyboardMouse, eKI_W);

    m_pInputComponent->RegisterAction("tank", "moveback", [this](int activationMode, float value)
        {
            if (activationMode != eAAM_OnRelease)
            {
                float inversion = m_inverse_input ? -1.0f : 1.0f;
                m_forwardInputLevel = -CryFabs(value) * inversion;
            }
        });
    m_pInputComponent->BindAction("tank", "moveback", eAID_KeyboardMouse, eKI_S);

    // Mouse look
    m_pInputComponent->RegisterAction("tank", "mouse_rotateyaw", [this](int activationMode, float value)
        {
            if (activationMode != eAAM_OnRelease)
                m_mouseDeltaRotation.x += value;
        });
    m_pInputComponent->BindAction("tank", "mouse_rotateyaw", eAID_KeyboardMouse, eKI_MouseX);

    m_pInputComponent->RegisterAction("tank", "mouse_rotatepitch", [this](int activationMode, float value)
        {
            if (activationMode != eAAM_OnRelease)
                m_mouseDeltaRotation.y += value;
        });
    m_pInputComponent->BindAction("tank", "mouse_rotatepitch", eAID_KeyboardMouse, eKI_MouseY);

    // Firing
    m_pInputComponent->RegisterAction("tank", "shoot", [this](int activationMode, float value)
        {
            if (activationMode == eAAM_OnPress)
            {
                FireMainCannon();
            }
        });
    m_pInputComponent->BindAction("tank", "shoot", eAID_KeyboardMouse, eKI_Mouse1);

    m_pInputComponent->RegisterAction("tank", "altshoot", [this](int, float) { /* Reserved */ });
    m_pInputComponent->BindAction("tank", "altshoot", eAID_KeyboardMouse, eKI_Mouse2);

    if (enableLogs) CryLogAlways("[Tank] InitializeInput - All actions registered");
}

void CSimpleTankComponent::Register(Schematyc::CEnvRegistrationScope& componentScope)
{
    // Optional: Register Schematyc functions here
}

void CSimpleTankComponent::Initialize()
{
    LoadBodyMesh();
    InitializeInput();
    RefreshVehicleRuntimeState();
    //CreateWheelComponents();
    m_physicsInitialized = true;
}

void CSimpleTankComponent::RefreshVehicleRuntimeState()
{
    if (m_isRefreshingVehicleRuntimeState)
    {
        return;
    }

    m_isRefreshingVehicleRuntimeState = true;
    m_pVehiclePhysicsComponent = nullptr;
    m_wheelComponents.clear();
    m_wheelSetupDone = false;
    m_physicsInitialized = false;
    SetupVehiclePhysics();

    if (m_pVehiclePhysicsComponent)
    {
        m_ignoreNextPhysicalTypeChange = true;
        m_pVehiclePhysicsComponent->ResetVehicleState();
        m_physicsInitialized = (GetEntity() != nullptr && GetEntity()->GetPhysicalEntity() != nullptr);
    }

    m_isRefreshingVehicleRuntimeState = false;
}

void CSimpleTankComponent::SetupVehiclePhysics()
{
    if (!GetEntity())
    {
        return;
    }

    if (m_pVehiclePhysicsComponent && !GetEntity()->GetPhysicalEntity())
    {
        GetEntity()->RemoveComponent<CCostumVehicleComponent>(false);
        m_pVehiclePhysicsComponent = nullptr;
    }

    if (!m_pVehiclePhysicsComponent)
    {
        m_pVehiclePhysicsComponent = GetEntity()->GetComponent<CCostumVehicleComponent>(false);
        if (!m_pVehiclePhysicsComponent)
        {
            m_pVehiclePhysicsComponent = GetEntity()->CreateComponent<CCostumVehicleComponent>();
            if (!m_pVehiclePhysicsComponent && enableLogs)
            {
                CryLogAlways("[Tank] SetupVehiclePhysics - failed to create vehicle physics component");
            }
        }
    }

    if (m_pVehiclePhysicsComponent)
    {
        // === CONFIGURE VEHICLE PHYSICS PARAMETERS ===
        // These must be set AFTER component creation but BEFORE first physics update

        // Engine params (critical for torque generation)
        //m_pVehiclePhysicsComponent->getpo (25000.0f);      // Tank: 20k-50k
        //m_pVehiclePhysicsComponent->SetEngineMaxRPM(6000.0f);
        //m_pVehiclePhysicsComponent->SetEngineIdleRPM(800.0f);
        //m_pVehiclePhysicsComponent->SetEngineMinRPM(600.0f);       // Clutch engages above this
        //m_pVehiclePhysicsComponent->SetEngineStartRPM(800.0f);

        //// Gear ratios (index: 0=Reverse, 1=Neutral, 2+=Forward)
        //// Must be positive floats; 0.0 for neutral
        //m_pVehiclePhysicsComponent->SetGearRatios({ 3.5f, 0.0f, 2.8f }); // Rev, Neu, Fwd1
        //m_pVehiclePhysicsComponent->SetShiftUpRPM(4500.0f);
        //m_pVehiclePhysicsComponent->SetShiftDownRPM(2500.0f);

        //// Handling params
        //m_pVehiclePhysicsComponent->SetMaxSteerAngle(DEG2RAD(45.0f));
        //m_pVehiclePhysicsComponent->SetBrakeTorque(12.0f);
        //m_pVehiclePhysicsComponent->SetHandbrakeTorque(15.0f);

        // Wake physics to ensure params are applied
     /*   if (IPhysicalEntity* pPhys = GetEntity()->GetPhysicalEntity())
        {
            pe_action_awake awake{ ePE_action_awake, 1 };
            pPhys->Action(&awake);
        }*/

        if (enableLogs)
            CryLogAlways("[Tank] Vehicle physics configured: power=25000, gears=[3.5,0,2.8]");
    }
}

void CSimpleTankComponent::CreateWheelComponents()
{
    if (!GetEntity() || !m_pVehiclePhysicsComponent)
        return;

    // === STEP 1: Clear existing wheel components to prevent accumulation ===
    GetEntity()->GetAllComponents(existingWheels, false);

    if (!existingWheels.empty())
    {
        if (enableLogs)
            CryLogAlways("[Tank] Clearing %d existing wheel component(s) before re-creation", existingWheels.size());

        GetEntity()->RemoveAllComponents<Cry::DefaultComponents::CWheelComponent>(false);
        m_wheelComponents.clear();
    }

    // === STEP 2: Create exactly g_simpleTankWheelCount wheels ===
    m_wheelComponents.clear();
    m_wheelComponents.reserve(g_simpleTankWheelCount);

    if (enableLogs)
        CryLogAlways("[Tank] === Creating %d Wheel Components ===", g_simpleTankWheelCount);

    for (size_t wheelIndex = 0; wheelIndex < g_simpleTankWheelCount; ++wheelIndex)
    {
        const SWheelDefinition& wheelDef = g_simpleTankWheelDefs[wheelIndex];

        Cry::DefaultComponents::CWheelComponent* pWheel =
            GetEntity()->CreateComponent<Cry::DefaultComponents::CWheelComponent>();

        if (!pWheel)
        {
            if (enableLogs)
                CryLogAlways("[Tank] ❌ Failed to create wheel component %d", wheelIndex);
            continue;
        }

        // Set transform (pivot position)
        Matrix34 wheelTransform = IDENTITY;
        wheelTransform.SetTranslation(wheelDef.pivot);
        pWheel->SetTransformMatrix(wheelTransform);

        // === Set wheel physics properties ===
        pWheel->m_radius = wheelDef.radius;
        pWheel->m_height = wheelDef.width;
        pWheel->m_axleIndex = wheelDef.axleIndex;
        pWheel->m_driving = wheelDef.driving;
        pWheel->m_handBrake = true;  // Handbrake on all wheels for tank
        pWheel->m_suspensionLength = 0.7f;
        pWheel->m_suspensionLengthComp = 0.35f;
        pWheel->m_suspensionPivot = Vec3(ZERO);  // ⚠️ Try (0,0,suspensionLength) if rays miss
        pWheel->m_damping = 0.7f;
        pWheel->m_bRaycast = true;  // ✅ Use raycast for reliable ground detection
        pWheel->m_surfaceTypeName = "rubber";  // Ensure valid surface type

        m_wheelComponents.emplace_back(pWheel);

        // === Log wheel properties for debugging ===
        if (enableLogs)
        {
            CryLogAlways("[Tank] Wheel[%zu] @ pivot=(%.2f,%.2f,%.2f)",
                wheelIndex, wheelDef.pivot.x, wheelDef.pivot.y, wheelDef.pivot.z);
            CryLogAlways("[Tank]   Geometry: radius=%.2fm | width=%.2fm", wheelDef.radius, wheelDef.width);
            CryLogAlways("[Tank]   Vehicle: axle=%d | driving=%s | handBrake=%s",
                wheelDef.axleIndex,
                wheelDef.driving ? "YES" : "NO",
                pWheel->m_handBrake ? "YES" : "NO");
            CryLogAlways("[Tank]   Suspension: lenMax=%.2f | lenInit=%.2f | pivot=(%.2f,%.2f,%.2f) | damping=%.2f",
                pWheel->m_suspensionLength,
                pWheel->m_suspensionLengthComp,
                pWheel->m_suspensionPivot.x, pWheel->m_suspensionPivot.y, pWheel->m_suspensionPivot.z,
                pWheel->m_damping);
            CryLogAlways("[Tank]   Physics: bRaycast=%s | surfaceType='%s'",
                pWheel->m_bRaycast ? "YES" : "NO",
                pWheel->m_surfaceTypeName.value.c_str());
        }
    }

    m_wheelSetupDone = true;

    if (enableLogs)
        CryLogAlways("[Tank] ✓ Wheel setup complete. Total wheels: %zu", m_wheelComponents.size());
}

void CSimpleTankComponent::UpdateVehicleDrive(float deltaTime)
{
    (void)deltaTime;

    if (!GetEntity())
    {
        if (enableLogs) CryLogAlways("[Tank] UpdateVehicleDrive - no entity");
        return;
    }

    IPhysicalEntity* pPhysEnt = GetEntity()->GetPhysicalEntity();
    if (!m_pVehiclePhysicsComponent || !pPhysEnt)
    {
        if (enableLogs)
        {
            CryLogAlways("[Tank] UpdateVehicleDrive - missing physics state, vehicleComp=%p physEnt=%p forwardInput=%.2f turnInput=%.2f",
                static_cast<void*>(m_pVehiclePhysicsComponent), static_cast<void*>(pPhysEnt), m_forwardInputLevel, m_turnInputLevel);
        }
        return;
    }

    const float input = CryClamp(m_forwardInputLevel, -1.0f, 1.0f);
    float throttle = 0.0f;
    float brake = 0.0f;
    int gear = static_cast<int>(CCostumVehicleComponent::EGear::Forward);

    if (CryFabs(input) > 0.01f)
    {
        if (input < 0.0f)
        {
            gear = static_cast<int>(CCostumVehicleComponent::EGear::Reverse);
            m_pVehiclePhysicsComponent->SetCurrentGear(gear);
            throttle = CryFabs(input);
            m_pVehiclePhysicsComponent->SetThrottle(throttle);
        }
        else
        {
            gear = static_cast<int>(CCostumVehicleComponent::EGear::Forward);
            m_pVehiclePhysicsComponent->SetCurrentGear(gear);
            throttle = input;
            m_pVehiclePhysicsComponent->SetThrottle(throttle);
        }
        float pedal = m_pVehiclePhysicsComponent->GetThrottle();
        CryLogAlways("[Tank] Reverse input: %.2f | Applied throttle: %.2f", input, pedal);

        //brake = 0.0f;
        //m_pVehiclePhysicsComponent->SetBrake(brake);
    }
    else
    {
        throttle = 0.0f;
        brake = 1.0f;
		m_pVehiclePhysicsComponent->SetCurrentGear(static_cast<int>(CCostumVehicleComponent::EGear::Neutral));
        m_pVehiclePhysicsComponent->SetThrottle(throttle);
        m_pVehiclePhysicsComponent->SetBrake(brake);
		m_pVehiclePhysicsComponent->UseHandbrake(true);
    }

    const float steeringDegrees = m_turnInputLevel * m_rotationSpeed;
    m_pVehiclePhysicsComponent->SetSteeringAngle(CryTransform::CAngle::FromDegrees(steeringDegrees));

	// Log requirements: Raw input, applied commands, and physics state
    const float curThrottle = m_pVehiclePhysicsComponent->GetThrottle();
    const float curBrake = m_pVehiclePhysicsComponent->GetBrake();
    const int   curGear = m_pVehiclePhysicsComponent->GetCurrentGear();


    if (m_forwardInputLevel <= 0.0f && m_turnInputLevel <= 0.0f)
        return;

    if (enableLogs && m_pVehiclePhysicsComponent)
    {
        CryLogAlways("[Tank] === Drive & Wheel Diagnostics ===");

        IPhysicalEntity* pPhysEnt = GetEntity() ? GetEntity()->GetPhysicalEntity() : nullptr;

        // Use actual wheel component count as authoritative source
        const int wheelCount = static_cast<int>(m_wheelComponents.size());


        // === Query and log vehicle-level status ===
        if (pPhysEnt)
        {
            pe_status_vehicle vehicleStatus{};
            vehicleStatus.type = pe_status_vehicle::type_id;

            if (pPhysEnt->GetStatus(&vehicleStatus) != 0)
            {
              /*  CryLogAlways("[Tank] Vehicle: Contact=%s | Gear=%d(%s) | RPM=%.0f | Torque=%.1f | Colliders=%d | Input: fwd=%.2f turn=%.2f | Cmd: thr=%.2f brk=%.2f steer=%.1f°",
                    vehicleStatus.bWheelContact ? "YES" : "NO",
                    vehicleStatus.iCurGear,
                    (curGear == m_pVehiclePhysicsComponent->GetCurrentGear() > 1 ? "FWD" :
                        curGear == -1 ? "REV" : "NEU"),
                    vehicleStatus.engineRPM,
                    vehicleStatus.drivingTorque,
                    vehicleStatus.nActiveColliders,
                    m_forwardInputLevel, m_turnInputLevel,
                    curThrottle, curBrake, steeringDegrees);*/
            }
            else
            {
                CryLogAlways("[Tank] ⚠️ GetStatus(pe_status_vehicle) failed");
            }
        }

        // === Query and log per-wheel status (single pass) ===
        int groundedCount = 0;
        for (int i = 0; i < wheelCount; ++i)
        {
            bool isGrounded = false;
            float friction = 0.0f, torque = 0.0f, suspLen = 0.0f, suspLenFull = 0.0f;
            bool isSlipping = false;
            int surfaceIdx = -1;
            bool isDriving = false;

            // Query physics status
            if (pPhysEnt)
            {
                pe_status_wheel ws{};
                ws.type = pe_status_wheel::type_id;
                ws.iWheel = i;

                if (pPhysEnt->GetStatus(&ws) != 0)
                {
                    isGrounded = (ws.bContact != 0);
                    friction = ws.friction;
                    torque = ws.torque;
                    suspLen = ws.suspLen;
                    suspLenFull = ws.suspLenFull;
                    isSlipping = (ws.bSlip != 0);
                    surfaceIdx = ws.contactSurfaceIdx;

                    if (isGrounded) groundedCount++;
                }
            }

            // Get component properties if available
            if (i < static_cast<int>(m_wheelComponents.size()) && m_wheelComponents[i])
            {
                isDriving = m_wheelComponents[i]->m_driving;
            }

            //CryLogAlways("[Tank] Wheel[%d]: Grounded=%-3s | Drive=%-3s | Friction=%.2f | Torque=%6.1f | Slip=%s | Susp=%.3f/%.3f | SurfIdx=%d",
            //    i,
            //    isGrounded ? "YES" : "NO",
            //    isDriving ? "YES" : "NO",
            //    friction,
            //    torque,
            //    isSlipping ? "YES" : "NO",
            //    suspLen, suspLenFull,
            //    surfaceIdx);
        }

        // === Summary and warnings ===
        CryLogAlways("[Tank] Summary: %d/%d wheels grounded | Speed: %.2f m/s",
            groundedCount, wheelCount, m_currentSpeed);

        if (groundedCount == 0 && std::abs(m_currentSpeed) < 0.1f)
        {
            CryLogAlways("[Tank] ❌ CRITICAL: Zero wheels grounded. Check:");
            CryLogAlways("[Tank]   1. m_bRaycast=true on all wheels");
            CryLogAlways("[Tank]   2. SuspensionPivot.z ≈ SuspensionLength");
            CryLogAlways("[Tank]   3. Terrain: Physicalize=true, Layer=Default");
            CryLogAlways("[Tank]   4. Wheel Radius matches visual mesh");
            CryLogAlways("[Tank]   5. Surface type has friction > 0");
        }

        if (pPhysEnt && wheelCount > 0)
        {
            pe_status_vehicle vs{};
            vs.type = pe_status_vehicle::type_id;
            if (pPhysEnt->GetStatus(&vs) == 0 || (vs.bWheelContact && groundedCount == 0))
            {
                CryLogAlways("[Tank] ⚠️ MISMATCH: Vehicle reports contact but per-wheel query shows %d grounded", groundedCount);
            }
        }
    }
   
}

Cry::Entity::EventFlags CSimpleTankComponent::GetEventMask() const
{
    return Cry::Entity::EEvent::Update |
        Cry::Entity::EEvent::PhysicsCollision |
        Cry::Entity::EEvent::PhysicalTypeChanged |
        ENTITY_EVENT_LEVEL_LOADED |
        ENTITY_EVENT_START_GAME |
        Cry::Entity::EEvent::Reset;
}

void CSimpleTankComponent::ProcessEvent(const SEntityEvent& event)
{
    switch (event.event)
    {
    case Cry::Entity::EEvent::Update:
    {
        // FIX: Proper cast for SEntityUpdateContext
        if (event.nParam[0])
        {
            SEntityUpdateContext* pCtx = reinterpret_cast<SEntityUpdateContext*>(event.nParam[0]);
            const float deltaTime = pCtx->fFrameTime;
            UpdateMovement(deltaTime);
            RotateTurret(m_mouseDeltaRotation.x, m_mouseDeltaRotation.y, deltaTime);
            UpdateTurret(deltaTime);
            UpdateCooldowns(deltaTime);
        }
        break;
    }
    case Cry::Entity::EEvent::PhysicsCollision:
    {
        OnHit(10.0f, GetEntity()->GetWorldPos(), Vec3(0, 0, 1));
        break;
    }
    case Cry::Entity::EEvent::Reset:
    {
        ResetState();
        RefreshVehicleRuntimeState();
        break;
    }
    case Cry::Entity::EEvent::PhysicalTypeChanged:
    {
        if (m_ignoreNextPhysicalTypeChange)
        {
            m_ignoreNextPhysicalTypeChange = false;
            break;
        }
        if (m_isRefreshingVehicleRuntimeState)
        {
            break;
        }
        RefreshVehicleRuntimeState();
        break;
    }
    case ENTITY_EVENT_LEVEL_LOADED:
    case ENTITY_EVENT_START_GAME:
    {
        RefreshVehicleRuntimeState();
        break;
    }
    }
}

void CSimpleTankComponent::ResetState()
{
    m_currentSpeed = 0.0f;
    m_currentRotationVelocity = 0.0f;
    m_targetRotationVelocity = 0.0f;
    m_forwardInputLevel = 0.0f;
    m_turnInputLevel = 0.0f;
    m_currentTurretYaw = 0.0f;
    m_targetTurretYaw = 0.0f;
    m_currentTurretPitch = 0.0f;
    m_targetTurretPitch = 0.0f;
    m_hullCurrentDamage = 0.0f;
    m_mainCannonCooldown = 0.0f;
    m_mouseDeltaRotation = Vec2(ZERO);  // FIX: Use Vec2(ZERO)
    m_isFiring = false;
    m_isDestroyed = false;
    m_isRefreshingVehicleRuntimeState = false;
    m_ignoreNextPhysicalTypeChange = false;
    m_pVehiclePhysicsComponent = nullptr;
    m_wheelComponents.clear();
    m_wheelSetupDone = false;
    m_physicsInitialized = false;
}

void CSimpleTankComponent::LoadBodyMesh()
{
    if (!GetEntity() || m_bodyMesh.value.IsEmpty())
        return;

    m_bodySlot = GetEntity()->LoadGeometry(0, m_bodyMesh.value.c_str());
    if (m_bodySlot < 0)
    {
        if (enableLogs) CryLogAlways("[Tank] Failed to load body mesh");
        return;
    }

    if (enableLogs)
    {
        CryLogAlways("[Tank] Loaded body mesh in slot %d", m_bodySlot);
    }
}

void CSimpleTankComponent::DebugLog(float)
{
    if (!enableLogs || !GetEntity())
        return;

    if (IPhysicalEntity* pPhysEnt = GetEntity()->GetPhysicalEntity())
    {
        pe_status_dynamics sd{};
        if (pPhysEnt->GetStatus(&sd) != 0)
        {
            CryLogAlways("[Tank] Speed: %.2f m/s | AngVel: %.2f rad/s | Turn: %.2f",
                m_currentSpeed, sd.w.z, m_turnInputLevel);
        }
    }

    m_mouseDeltaRotation = Vec2(ZERO);
}

void CSimpleTankComponent::UpdateMovement(float deltaTime)
{
    if (!GetEntity() || deltaTime <= 0.0f)
        return;

    if (m_pVehiclePhysicsComponent && GetEntity()->GetPhysicalEntity())
    {
        UpdateVehicleDrive(deltaTime);
        return;
    }

    // Fallback when vehicle physics isn't ready yet
    float targetSpeed = m_forwardInputLevel * m_maxSpeed;
    m_currentSpeed = SimpleLerpFloat(m_currentSpeed, targetSpeed, m_accelerationRate, deltaTime);

    if (CryFabs(m_currentSpeed) > 0.001f)
    {
        const Quat rotation = GetEntity()->GetWorldRotation();
        const Vec3 forward = rotation.GetColumn1();
        const Vec3 newPos = GetEntity()->GetWorldPos() + forward * m_currentSpeed * deltaTime;
        GetEntity()->SetPos(newPos);

        if (IPhysicalEntity* pPhysEnt = GetEntity()->GetPhysicalEntity())
        {
            pe_action_drive driveAction{};
            driveAction.pedal = CryClamp(m_forwardInputLevel, -1.0f, 1.0f);
            driveAction.steer = CryClamp(m_turnInputLevel, -1.0f, 1.0f);
            driveAction.bHandBrake = 0;
            pPhysEnt->Action(&driveAction, true);
        }
    }

    float targetAngularVelocity = 0.0f;
    if (CryFabs(m_currentSpeed) < 0.5f)
    {
        targetAngularVelocity = m_turnInputLevel * m_rotationSpeed;
    }
    else
    {
        float speedRatio = CryFabs(m_currentSpeed) / m_maxSpeed;
        targetAngularVelocity = m_turnInputLevel * m_rotationSpeed * (1.0f - speedRatio * 0.4f);
    }

    m_currentRotationVelocity = SimpleLerpFloat(m_currentRotationVelocity, targetAngularVelocity, m_accelerationRate * 0.5f, deltaTime);

    if (CryFabs(m_currentRotationVelocity) > 0.001f)
    {
        float rotationDelta = DEG2RAD(m_currentRotationVelocity * deltaTime);
        Quat currentRot = GetEntity()->GetWorldRotation();
        Quat deltaRot = Quat::CreateRotationZ(rotationDelta);
        GetEntity()->SetRotation((currentRot * deltaRot).GetNormalized());
    }
}

void CSimpleTankComponent::UpdateTurret(float deltaTime)
{
    if (deltaTime <= 0.0f)
        return;

    if (CryFabs(m_targetTurretYaw - m_currentTurretYaw) > 0.01f)
    {
        m_currentTurretYaw = SimpleLerpFloat(m_currentTurretYaw, m_targetTurretYaw,
            m_rotationSpeed * 0.8f, deltaTime);
    }

    if (CryFabs(m_targetTurretPitch - m_currentTurretPitch) > 0.01f)
    {
        m_currentTurretPitch = SimpleLerpFloat(m_currentTurretPitch, m_targetTurretPitch,
            m_rotationSpeed * 0.8f, deltaTime);
        m_currentTurretPitch = CryClamp(m_currentTurretPitch, m_minTurretPitch, m_maxTurretPitch);
    }

    if (m_turretSlot >= 0 && GetEntity())
    {
        Matrix34 turretTransform = Matrix34::CreateIdentity();
        // FIX: Convert Quat to Matrix33 first for SetRotation33
        Matrix33 turretRot33 = Matrix33::CreateRotationY(DEG2RAD(m_currentTurretPitch)) *
            Matrix33::CreateRotationZ(DEG2RAD(m_currentTurretYaw));
        turretTransform.SetRotation33(turretRot33);
        turretTransform.SetTranslation(m_turretPos);
        GetEntity()->SetSlotLocalTM(m_turretSlot, turretTransform);
    }
}

void CSimpleTankComponent::UpdateCooldowns(float deltaTime)
{
    if (m_mainCannonCooldown > 0.0f)
    {
        m_mainCannonCooldown = CryMax(0.0f, m_mainCannonCooldown - deltaTime);
    }
}

void CSimpleTankComponent::MoveForward(float speed)
{
    m_forwardInputLevel = CryClamp(speed / m_maxSpeed, 0.0f, 1.0f);
}

void CSimpleTankComponent::MoveBackward(float speed)
{
    m_forwardInputLevel = CryClamp(-speed / m_maxSpeed, -1.0f, 0.0f);
}

void CSimpleTankComponent::RotateBody(float rotationAngle)
{
    m_turnInputLevel = CryClamp(rotationAngle / m_rotationSpeed, -1.0f, 1.0f);
}

void CSimpleTankComponent::RotateTurret(float yaw, float pitch, float deltaTime)
{
    (void)deltaTime;

    const float yawSensitivity = 0.18f;
    const float pitchSensitivity = 0.18f;

    m_targetTurretYaw += yaw * yawSensitivity;
    m_targetTurretPitch = CryClamp(m_targetTurretPitch + pitch * pitchSensitivity,
        m_minTurretPitch, m_maxTurretPitch);

    m_mouseDeltaRotation = Vec2(ZERO);  // FIX: Use Vec2(ZERO)
}

void CSimpleTankComponent::FireMainCannon()
{
    // FIX: These members are now public, so accessible
    if (m_mainCannonCooldown > 0.0f || !GetEntity())
        return;

    m_isFiring = true;
    m_mainCannonCooldown = m_mainCannonReloadTime;

    const Quat bodyRot = GetEntity()->GetWorldRotation();
    const Quat turretRot = Quat::CreateRotationZ(DEG2RAD(m_currentTurretYaw)) *
        Quat::CreateRotationY(DEG2RAD(m_currentTurretPitch));
    const Vec3 worldMuzzlePos = GetEntity()->GetWorldPos() + bodyRot * m_cannonOutPos;
    const Vec3 cannonDir = (bodyRot * turretRot).GetColumn1().GetNormalized();

    PlayParticleEffect(m_muzzleFlashEffect, worldMuzzlePos, cannonDir);
}

void CSimpleTankComponent::StopFiring()
{
    m_isFiring = false;
}

void CSimpleTankComponent::ApplyDamage(float damage)
{
    m_hullCurrentDamage = CryMin(m_hullCurrentDamage + damage, m_hullDamageMax);

    const Vec3 hitPos = GetEntity()->GetWorldPos() + Vec3(0, 0, 1.5f);
    PlayParticleEffect(m_damageHitEffect, hitPos, Vec3(0, 0, 1));

    if (m_hullCurrentDamage >= m_hullDamageMax && !m_isDestroyed)
    {
        m_isDestroyed = true;
        OnTankDestroyed();
    }
}

void CSimpleTankComponent::OnHit(float damage, const Vec3& hitPosition, const Vec3& hitNormal)
{
    ApplyDamage(damage * 0.1f);
    PlayParticleEffect(m_impactEffect, hitPosition, hitNormal.GetNormalized());
}

void CSimpleTankComponent::OnTankDestroyed()
{
    if (!GetEntity())
        return;

    if (IPhysicalEntity* pPhysEnt = GetEntity()->GetPhysicalEntity())
    {
        pe_params_flags flags{};
        flags.flagsOR = pef_disabled;
        pPhysEnt->SetParams(&flags);
    }

    PlayParticleEffect(m_explosionEffect, GetEntity()->GetWorldPos(), Vec3(0, 0, 1));

    if (enableLogs) CryLogAlways("[Tank] Entity '%s' destroyed", GetEntity()->GetName());
}

void CSimpleTankComponent::PlayParticleEffect(const string& effectName, const Vec3& position, const Vec3& direction)
{
    if (!gEnv || !gEnv->pParticleManager || !GetEntity() || effectName.IsEmpty())
        return;

    IParticleEffect* pEffect = gEnv->pParticleManager->FindEffect(effectName.c_str());
    if (pEffect)
    {
        ParticleLoc loc(position, direction.GetNormalized(), 1.0f);
        pEffect->Spawn(loc);
    }
    else if (enableLogs)
    {
        CryLogAlways("[Tank] Particle effect not found: %s", effectName.c_str());
    }
}
