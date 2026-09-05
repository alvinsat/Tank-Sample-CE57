// BarrelComponent.cpp
#include "StdAfx.h"
#include "BarrelComponent.h"
#include "Bullet.h"
#include <CryGame/IGameFramework.h>        // Required for gEnv->pGameFramework
#include <CryRenderer/IRenderer.h>          // Required for ColorF structures
#include <CryRenderer/IRenderAuxGeom.h>
#include "SimpleTankComponent.h"
#include <CrySchematyc/Env/IEnvRegistrar.h>

namespace
{
    static void RegisterBarrelComponent(Schematyc::IEnvRegistrar& registrar)
    {
        Schematyc::CEnvRegistrationScope scope = registrar.Scope(IEntity::GetEntityScopeGUID());
        {
            Schematyc::CEnvRegistrationScope componentScope = scope.Register(SCHEMATYC_MAKE_ENV_COMPONENT(CBarrelComponent));
        }
    }

    CRY_STATIC_AUTO_REGISTER_FUNCTION(&RegisterBarrelComponent);
}

void CBarrelComponent::ReflectType(Schematyc::CTypeDesc<CBarrelComponent>& desc)
{
    desc.SetGUID("{b23c9d8e-21cd-4f3a-9b01-0123456789ab}"_cry_guid);
    desc.SetEditorCategory("Game");
    desc.SetLabel("Barrel Component");
    desc.SetDescription("Barrel geometry that applies pitch (and turret yaw) driven by SimpleTankComponent.");
    desc.SetComponentFlags({IEntityComponent::EFlags::Transform});

    desc.AddMember(&CBarrelComponent::m_barrelHasWeapon, 'w', "BarrelHasWeapon", "Barrel Has Weapon", "Fire when Barrel has weapon", false);
    desc.AddMember(&CBarrelComponent::m_barrelMesh, 'a', "BarrelMesh", "Barrel Geometry", "Path to barrel mesh (.cgf/.cga)", "");
    desc.AddMember(&CBarrelComponent::m_barrelSlotOffset, 'b', "BarrelSlotOffset", "Barrel Slot Offset", "Local offset applied to barrel slot", Vec3(0.0f, 0.0f, 0.0f));
    desc.AddMember(&CBarrelComponent::m_barrelWeaponOffset, 'bw', "BarrelWeaponOffset", "Barrel Weapon Offset", "Local offset applied to barrel weapon slot", Vec3(0.0f, 0.0f, 0.0f));
    desc.AddMember(&CBarrelComponent::m_useCameraForward, 'c', "UseCameraForward", "Align With Camera Forward", "When enabled, barrel aims along the main camera forward vector instead of the tank's turret angles.", false);
    desc.AddMember(&CBarrelComponent::m_tankEntityName, 'd', "TankEntityName", "Tank Entity Name", "Name of the tank entity to get turret angles from (if empty, uses same entity)", Schematyc::CSharedString(""));
    desc.AddMember(&CBarrelComponent::m_yawOffset, 'e', "YawOffset", "Yaw Offset", "Rotation offset applied to yaw (degrees)", 0.0f);
    desc.AddMember(&CBarrelComponent::m_pitchOffset, 'f', "PitchOffset", "Pitch Offset", "Rotation offset applied to pitch (degrees)", 0.0f);
    desc.AddMember(&CBarrelComponent::m_minYaw, 'g', "MinYaw", "Min Yaw", "Minimum yaw rotation constraint (degrees)", -180.0f);
    desc.AddMember(&CBarrelComponent::m_maxYaw, 'h', "MaxYaw", "Max Yaw", "Maximum yaw rotation constraint (degrees)", 180.0f);
    desc.AddMember(&CBarrelComponent::m_minPitch, 'i', "MinPitch", "Min Pitch", "Minimum pitch rotation constraint (degrees)", -45.0f);
    desc.AddMember(&CBarrelComponent::m_maxPitch, 'j', "MaxPitch", "Max Pitch", "Maximum pitch rotation constraint (degrees)", 45.0f);
    desc.AddMember(&CBarrelComponent::m_invertYaw, 'k', "InvertYaw", "Invert Yaw", "Invert yaw rotation direction", false);
    desc.AddMember(&CBarrelComponent::m_invertPitch, 'l', "InvertPitch", "Invert Pitch", "Invert pitch rotation direction", false);
}

void CBarrelComponent::Initialize()
{
    m_pTank = nullptr;

    // Try to find tank component
    if (!m_tankEntityName.empty())
    {
        // Find entity by name from entity system
        if (IEntity* pTankEntity = gEnv->pEntitySystem->FindEntityByName(m_tankEntityName.c_str()))
        {
            m_pTank = pTankEntity->GetComponent<CSimpleTankComponent>();
            if (m_pTank)
            {
                CryLogAlways("[BarrelComponent] Found tank entity '%s' with SimpleTankComponent", m_tankEntityName.c_str());
            }
            else
            {
                CryLogAlways("[BarrelComponent] Found tank entity '%s' but it has no SimpleTankComponent", m_tankEntityName.c_str());
            }
        }
        else
        {
            CryLogAlways("[BarrelComponent] Failed to find tank entity by name '%s'", m_tankEntityName.c_str());
        }
    }
    else if (m_pEntity)
    {
        // Fall back to same entity
        m_pTank = m_pEntity->GetComponent<CSimpleTankComponent>();
        if (m_pTank)
        {
            CryLogAlways("[BarrelComponent] Using SimpleTankComponent from same entity");
        }
    }

    LoadBarrelMesh();
}

Cry::Entity::EventFlags CBarrelComponent::GetEventMask() const
{
    return Cry::Entity::EEvent::Update;
}

bool isFired = false;
void CBarrelComponent::ProcessEvent(const SEntityEvent& event)
{
    if (event.event == Cry::Entity::EEvent::Update)
    {
        SEntityUpdateContext* pCtx = (SEntityUpdateContext*)event.nParam[0];
        const float deltaTime = pCtx->fFrameTime;

        if (m_pTank)
        {
            if (m_useCameraForward && gEnv->pSystem)
            {
                const CCamera& viewCamera = gEnv->pSystem->GetViewCamera();
                const Vec3 cameraForward = viewCamera.GetViewdir().GetNormalized();
                const Quat worldRot = GetEntity()->GetWorldRotation();
                const Quat invWorldRot = worldRot.GetInverted();
                const Vec3 localDir = invWorldRot * cameraForward;

                m_currentYaw = RAD2DEG(atan2f(localDir.x, localDir.y));
                m_currentPitch = RAD2DEG(atan2f(localDir.z, sqrtf(localDir.x * localDir.x + localDir.y * localDir.y)));
            }
            else
            {
                m_currentYaw = m_pTank->GetCurrentTurretYaw();
                m_currentPitch = m_pTank->GetCurrentTurretPitch();
            }

            // Apply inversion to raw values first
            float yawInput = m_invertYaw ? -m_currentYaw : m_currentYaw;
            float pitchInput = m_invertPitch ? -m_currentPitch : m_currentPitch;

            // Apply offsets
            float finalYaw = yawInput + m_yawOffset;
            float finalPitch = pitchInput + m_pitchOffset;

            // Apply constraints
            finalYaw = clamp_tpl(finalYaw, m_minYaw, m_maxYaw);
            finalPitch = clamp_tpl(finalPitch, m_minPitch, m_maxPitch);

            if (m_barrelSlot >= 0 && GetEntity())
            {
                GetEntity()->SetSlotLocalTM(m_barrelSlot, CreateSlotTM(finalYaw, finalPitch, m_barrelSlotOffset, true));
            }

			bool allowFire = (m_pTank->m_isFiring && m_pTank->m_mainCannonCooldown <= 0.0f); // Check if the tank allows firing (e.g., cooldowns, ammo)

            if (allowFire && m_barrelHasWeapon)
            {
                    FireMainCannon(false);
					m_pTank->m_isFiring = false; // Reset firing state immediately to prevent multiple shots in one update

                    CryLogAlways("[BarrelComponent] Firing - Current Yaw: %.2f, Current Pitch: %.2f, Final Yaw: %.2f, Final Pitch: %.2f",
                        m_currentYaw, m_currentPitch, finalYaw, finalPitch);
                    m_pTank->m_isFiring = false;

            }

            if (m_barrelHasWeapon) {
                FireMainCannon(true);
            }

        }
    }
}

void CBarrelComponent::DebugDrawRay()
{
    if (m_barrelSlot >= 0 && GetEntity())
    {
        Matrix34 worldTM = GetEntity()->GetSlotWorldTM(m_barrelSlot);
        Vec3 barrelPos = worldTM.GetTranslation();
        Vec3 barrelDir = worldTM.GetColumn1().GetNormalized(); // Assuming forward is along Y-axis of the slot TM
        gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine(barrelPos, ColorB(255, 0, 0), barrelPos + barrelDir * 5.0f, ColorB(255, 0, 0));
    }
}
void CBarrelComponent::FireMainCannon(bool isPreviewLine)
{
    if (m_barrelSlot >= 0)
    {
        Matrix34 worldTM = GetEntity()->GetSlotWorldTM(m_barrelSlot);

        // Transform the local offset into the world position using the matrix
        // This handles both the position AND the rotation automatically
        Vec3 bulletSpawnPos = worldTM.TransformPoint(m_barrelWeaponOffset);

        Quat barrelWorldRot = Quat(worldTM);

        if (!isPreviewLine)
        {
            SEntitySpawnParams spawnParams;
            spawnParams.pClass = gEnv->pEntitySystem->GetClassRegistry()->GetDefaultClass();
            float previewLength = 10.0f;
            Vec3 bulletForwardDir = barrelWorldRot * Vec3(0.0f, -1.0f, 0.0f);

            // Use the correctly transformed position
			spawnParams.vPosition = bulletSpawnPos + (bulletForwardDir * previewLength); // Spawn a bit in front of the barrel to avoid collisions
            spawnParams.qRotation = barrelWorldRot;
            spawnParams.vScale = Vec3(0.05f);

            if (IEntity* pBulletEntity = gEnv->pEntitySystem->SpawnEntity(spawnParams))
            {
                pBulletEntity->CreateComponentClass<CBulletComponent>();
            }
        }
        else
        {
            // Calculate trajectory direction (Y-axis is forward in CryEngine)
            Vec3 bulletForwardDir = barrelWorldRot * Vec3(0.0f, -1.0f, 0.0f);

            // Define how far you want the preview line to reach (e.g., 30 meters)
            float previewLength = 30.0f;
            Vec3 rayEndPos = bulletSpawnPos + (bulletForwardDir * previewLength);

            // Draw the line into the scene using AuxGeom
            if (gEnv->pRenderer && gEnv->pRenderer->GetIRenderAuxGeom())
            {
                gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine(
                    bulletSpawnPos,       // Starting at the muzzle offset
                    ColorB(255, 0, 0),    // Solid Red
                    rayEndPos,            // Ending down the vector
                    ColorB(255, 0, 0),
                    2.0f                  // Line thickness
                );
            }
        }
    }
}

Matrix34 CBarrelComponent::CreateSlotTM(float yawDegrees, float pitchDegrees, const Vec3& offset, bool applyPitch) const
{
    Quat rotation = Quat::CreateRotationZ(DEG2RAD(yawDegrees));
    if (applyPitch)
    {
        rotation = rotation * Quat::CreateRotationX(DEG2RAD(pitchDegrees));
    }

    Matrix34 slotTM(rotation);
    slotTM.SetTranslation(rotation * offset);
    return slotTM;
}

void CBarrelComponent::LoadBarrelMesh()
{
    if (!GetEntity())
        return;

    if (!m_barrelMesh.value.empty())
    {
        m_barrelSlot = GetEntity()->LoadGeometry(2, m_barrelMesh.value.c_str());
        if (m_barrelSlot < 0)
        {
            CryLogAlways("[BarrelComponent] Failed to load barrel mesh '%s'", m_barrelMesh.value.c_str());
        }
        else
        {
            GetEntity()->SetSlotLocalTM(m_barrelSlot, CreateSlotTM(m_currentYaw, m_currentPitch, m_barrelSlotOffset, true));
        }
    }
}
