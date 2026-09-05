#include "StdAfx.h"
#include "TPSCameraComponent.h"

#include <CryCore/StaticInstanceList.h>
#include <CrySchematyc/Env/Elements/EnvComponent.h>
#include <CryPhysics/physinterface.h>
#include <CryMath/Cry_Camera.h>

namespace
{
    static CTPSCameraComponent* FindFirstTPSCameraComponent()
    {
        IEntityItPtr pEntityIterator = gEnv->pEntitySystem->GetEntityIterator();
        pEntityIterator->MoveFirst();

        while (!pEntityIterator->IsEnd())
        {
            if (IEntity* pEntity = pEntityIterator->Next())
            {
                if (CTPSCameraComponent* pComponent = pEntity->GetComponent<CTPSCameraComponent>())
                {
                    return pComponent;
                }
            }
        }

        return nullptr;
    }

    static void RegisterTPSCameraComponent(Schematyc::IEnvRegistrar& registrar)
    {
        Schematyc::CEnvRegistrationScope scope = registrar.Scope(IEntity::GetEntityScopeGUID());
        {
            Schematyc::CEnvRegistrationScope componentScope = scope.Register(SCHEMATYC_MAKE_ENV_COMPONENT(CTPSCameraComponent));
        }
    }

    CRY_STATIC_AUTO_REGISTER_FUNCTION(&RegisterTPSCameraComponent);
}

void CTPSCameraComponent::ReflectType(Schematyc::CTypeDesc<CTPSCameraComponent>& desc)
{
    desc.SetGUID("{F8A4575A-3A2B-4F0B-B2CD-171D504E5E7F}"_cry_guid);
    desc.SetEditorCategory("Cameras");
    desc.SetLabel("TPS Camera");
    desc.SetDescription("Third-person camera that uses a camera anchor pivot and smooth spring-arm behavior.");
    desc.SetComponentFlags({ IEntityComponent::EFlags::Transform });

    desc.AddMember(&CTPSCameraComponent::m_targetArmLength, 'tarl', "TargetArmLength", "Target Arm Length", "Distance between the camera and the anchor pivot.", 3.25f);
    desc.AddMember(&CTPSCameraComponent::m_minArmLength, 'mnal', "MinArmLength", "Minimum Arm Length", "How close the camera is allowed to get when colliding with geometry.", 0.5f);
    desc.AddMember(&CTPSCameraComponent::m_socketOffsetRight, 'sorf', "SocketOffsetRight", "Shoulder Offset Right", "Horizontal offset from the anchor pivot for an over-the-shoulder view.", 0.35f);
    desc.AddMember(&CTPSCameraComponent::m_socketOffsetUp, 'soup', "SocketOffsetUp", "Shoulder Offset Up", "Vertical offset from the anchor pivot.", 0.25f);
    desc.AddMember(&CTPSCameraComponent::m_socketOffsetForward, 'sofw', "SocketOffsetForward", "Shoulder Offset Forward", "Forward/backward offset from the anchor pivot.", 0.0f);
    desc.AddMember(&CTPSCameraComponent::m_cameraLagSpeed, 'clag', "CameraLagSpeed", "Camera Lag Speed", "How quickly the camera catches up to the target position.", 10.0f);
    desc.AddMember(&CTPSCameraComponent::m_rotationSpeed, 'rotp', "RotationSpeed", "Rotation Sensitivity", "Mouse or stick sensitivity for TPS camera rotation.", 0.0025f);
    desc.AddMember(&CTPSCameraComponent::m_pitchMin, 'pmin', "PitchMin", "Minimum Pitch", "Lowest allowed vertical angle in degrees.", -70.0f);
    desc.AddMember(&CTPSCameraComponent::m_pitchMax, 'pmax', "PitchMax", "Maximum Pitch", "Highest allowed vertical angle in degrees.", 70.0f);
    desc.AddMember(&CTPSCameraComponent::m_collisionProbeRadius, 'cprb', "CollisionProbeRadius", "Collision Probe Radius", "Radius used when probing for geometry between the anchor and camera.", 0.25f);
    desc.AddMember(&CTPSCameraComponent::m_lockCharacterYaw, 'lcyw', "LockCharacterYaw", "Lock Character Yaw", "When enabled, the owner entity yaw follows the camera yaw (strafe/aim mode).", false);
    desc.AddMember(&CTPSCameraComponent::m_debugLogs, 'dlog', "DebugLogs", "Debug Logs", "When enabled, debug logs for the TPS camera are printed.", false);
}

void CTPSCameraComponent::Initialize()
{
    if (m_pEntity != nullptr)
    {
        const Quat entityRotation = m_pEntity->GetWorldRotation();
        const Ang3 entityYPR = CCamera::CreateAnglesYPR(Matrix33(entityRotation));
        m_currentYaw = RAD2DEG(entityYPR.x);
        m_currentPitch = clamp_tpl(RAD2DEG(entityYPR.y), m_pitchMin, m_pitchMax);
    }

    if (!ResolveAnchor())
    {
        CryLogAlways("[TPSCamera] No CameraAnchorComponent found. Add a Camera Anchor entity or attach one to this entity.");
    }
}

Cry::Entity::EventFlags CTPSCameraComponent::GetEventMask() const
{
    return {};
}

void CTPSCameraComponent::ProcessEvent(const SEntityEvent& event)
{
    (void)event;
}

bool CTPSCameraComponent::UpdateCameraOverride(float deltaTime, const Vec2& lookDelta, CCameraAnchorComponent* pPreferredAnchor, Matrix34& outCameraTransform)
{
    if (deltaTime <= 0.0f || !ResolveAnchor(pPreferredAnchor))
    {
        return false;
    }

    m_mouseDeltaRotation += lookDelta;
    UpdateRotation(deltaTime);
    ApplyOrientationToAnchor();
    UpdateCameraTransform(deltaTime);

    outCameraTransform = Matrix34::Create(Vec3(1.0f), m_currentCameraRotation, m_currentCameraPosition);

    if (m_debugLogs && m_pCameraAnchor != nullptr)
    {
        const Vec3 anchorPos = m_pCameraAnchor->GetEntity()->GetWorldPos();
        CryLogAlways("[TPSCamera] Anchor Entity=%s pos=(%.2f, %.2f, %.2f) yaw=%.2f pitch=%.2f cameraPos=(%.2f, %.2f, %.2f)",
            m_pCameraAnchor->GetEntity()->GetName(),
            anchorPos.x, anchorPos.y, anchorPos.z,
            m_currentYaw, m_currentPitch,
            m_currentCameraPosition.x, m_currentCameraPosition.y, m_currentCameraPosition.z);
    }

    return true;
}

bool CTPSCameraComponent::UsesAnchor(CCameraAnchorComponent* pAnchor)
{
    return pAnchor != nullptr && ResolveAnchor(pAnchor) && m_pCameraAnchor == pAnchor;
}

void CTPSCameraComponent::UpdateRotation(float deltaTime)
{
    (void)deltaTime;
    m_currentYaw += m_mouseDeltaRotation.x * m_rotationSpeed;
    m_currentPitch = clamp_tpl(m_currentPitch + m_mouseDeltaRotation.y * m_rotationSpeed, m_pitchMin, m_pitchMax);
    m_mouseDeltaRotation = ZERO;
}

void CTPSCameraComponent::ApplyOrientationToAnchor()
{
    if (!m_pCameraAnchor)
    {
        return;
    }

    IEntity* pAnchorEntity = m_pCameraAnchor->GetEntity();
    if (pAnchorEntity == nullptr)
    {
        return;
    }

    const Vec3 anchorPosition = pAnchorEntity->GetWorldPos();

    const Ang3 cameraAngles(DEG2RAD(m_currentYaw), DEG2RAD(m_currentPitch), 0.0f);
    const Quat anchorRotation = Quat(CCamera::CreateOrientationYPR(cameraAngles));

    if (m_debugLogs)
    {
        CryLogAlways("[TPSCamera] ApplyOrientationToAnchor anchorEntity=%s anchorId=%u pos=(%.2f, %.2f, %.2f) rot=(%.3f, %.3f, %.3f, %.3f) ownerEntity=%s ownerId=%u",
            pAnchorEntity->GetName(), static_cast<unsigned>(pAnchorEntity->GetId()),
            anchorPosition.x, anchorPosition.y, anchorPosition.z,
            anchorRotation.w, anchorRotation.v.x, anchorRotation.v.y, anchorRotation.v.z,
            m_pEntity ? m_pEntity->GetName() : "<null>",
            m_pEntity ? static_cast<unsigned>(m_pEntity->GetId()) : 0);
    }

    pAnchorEntity->SetWorldTM(Matrix34::Create(Vec3(1.0f), anchorRotation, anchorPosition));
}

void CTPSCameraComponent::UpdateCameraTransform(float deltaTime)
{
    IEntity* pAnchorEntity = m_pCameraAnchor != nullptr ? m_pCameraAnchor->GetEntity() : nullptr;
    if (pAnchorEntity == nullptr)
    {
        return;
    }

    const Vec3 anchorPosition = pAnchorEntity->GetWorldPos();
    const Quat anchorRotation = pAnchorEntity->GetWorldRotation();
    const Vec3 right = anchorRotation.GetColumn0();
    const Vec3 forward = anchorRotation.GetColumn1();
    const Vec3 up = anchorRotation.GetColumn2();

    const Vec3 pivotPosition =
        anchorPosition +
        right * m_socketOffsetRight +
        forward * m_socketOffsetForward +
        up * m_socketOffsetUp;

    const Vec3 desiredCameraPosition = pivotPosition - forward * max(m_targetArmLength, m_minArmLength);
    Vec3 resolvedCameraPosition = desiredCameraPosition;

    IPhysicalEntity* pSkipEntities[2] =
    {
        m_pEntity != nullptr ? m_pEntity->GetPhysics() : nullptr,
        pAnchorEntity->GetPhysics()
    };

    IPhysicalEntity* pValidSkipEntities[2] = {};
    int skipEntityCount = 0;
    for (IPhysicalEntity* pPhysicalEntity : pSkipEntities)
    {
        if (pPhysicalEntity != nullptr)
        {
            pValidSkipEntities[skipEntityCount++] = pPhysicalEntity;
        }
    }

    ray_hit hit;
    const Vec3 rayDirection = desiredCameraPosition - pivotPosition;
    if (!rayDirection.IsZero() && gEnv->pPhysicalWorld != nullptr)
    {
        const int hitCount = gEnv->pPhysicalWorld->RayWorldIntersection(
            pivotPosition,
            rayDirection,
            ent_all,
            rwi_stop_at_pierceable | rwi_colltype_any,
            &hit,
            1,
            skipEntityCount > 0 ? pValidSkipEntities : nullptr,
            skipEntityCount);

        if (hitCount > 0)
        {
            const float hitDistance = (hit.pt - pivotPosition).GetLength();
            const float clampedDistance = max(hitDistance - m_collisionProbeRadius, m_minArmLength);
            resolvedCameraPosition = pivotPosition - forward * clampedDistance;
        }
    }

    const float interpolation = 1.0f - expf(-m_cameraLagSpeed * deltaTime);
    if (!m_hasInitializedCameraTransform)
    {
        m_currentCameraPosition = resolvedCameraPosition;
        m_hasInitializedCameraTransform = true;
    }
    else
    {
        m_currentCameraPosition = Vec3::CreateLerp(m_currentCameraPosition, resolvedCameraPosition, interpolation);
    }

    const Vec3 lookDirection = (pivotPosition - m_currentCameraPosition).GetNormalizedSafe(forward);
    m_currentCameraRotation = Quat::CreateRotationVDir(lookDirection, 0.0f);
}

bool CTPSCameraComponent::ResolveAnchor(CCameraAnchorComponent* pPreferredAnchor)
{
    if (pPreferredAnchor != nullptr)
    {
        if (m_pCameraAnchor != pPreferredAnchor)
        {
            m_hasInitializedCameraTransform = false;
        }
        m_pCameraAnchor = pPreferredAnchor;
        m_anchorEntityId = m_pCameraAnchor->GetEntity() != nullptr ? m_pCameraAnchor->GetEntity()->GetId() : INVALID_ENTITYID;
        return true;
    }

    if (m_pCameraAnchor != nullptr)
    {
        return true;
    }

    if (m_pEntity != nullptr)
    {
        m_pCameraAnchor = m_pEntity->GetComponent<CCameraAnchorComponent>();
    }

    if (m_pCameraAnchor != nullptr)
    {
        m_anchorEntityId = m_pCameraAnchor->GetEntity() != nullptr ? m_pCameraAnchor->GetEntity()->GetId() : INVALID_ENTITYID;
        return true;
    }

    if (CTPSCameraComponent* pTPSCamera = FindFirstTPSCameraComponent())
    {
        if (pTPSCamera != this && pTPSCamera->m_pCameraAnchor != nullptr)
        {
            m_pCameraAnchor = pTPSCamera->m_pCameraAnchor;
            m_anchorEntityId = m_pCameraAnchor->GetEntity() != nullptr ? m_pCameraAnchor->GetEntity()->GetId() : INVALID_ENTITYID;
            return true;
        }
    }

    m_pCameraAnchor = CCameraAnchorComponent::FindFirstEnabledAnchor();
    if (m_pCameraAnchor == nullptr)
    {
        CryLogAlways("[TPSCamera] Failed to find a CameraAnchorComponent in the scene.");
    }
    m_anchorEntityId = m_pCameraAnchor->GetEntity() != nullptr ? m_pCameraAnchor->GetEntity()->GetId() : INVALID_ENTITYID;
    return m_pCameraAnchor != nullptr;
}
