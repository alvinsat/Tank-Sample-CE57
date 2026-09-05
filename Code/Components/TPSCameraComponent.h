#pragma once

#include <CryEntitySystem/IEntityComponent.h>
#include <CryMath/Cry_Camera.h>
#include <CrySchematyc/MathTypes.h>
#include <DefaultComponents/Cameras/CameraComponent.h>
#include <DefaultComponents/Input/InputComponent.h>

#include "CameraAnchor.h"

class CTPSCameraComponent final : public IEntityComponent
{
public:
    CTPSCameraComponent() = default;
    virtual ~CTPSCameraComponent() override = default;

    static void ReflectType(Schematyc::CTypeDesc<CTPSCameraComponent>& desc);
    virtual void Initialize() override;
    virtual Cry::Entity::EventFlags GetEventMask() const override;
    virtual void ProcessEvent(const SEntityEvent& event) override;

    bool UpdateCameraOverride(float deltaTime, const Vec2& lookDelta, CCameraAnchorComponent* pPreferredAnchor, Matrix34& outCameraTransform);
    bool UsesAnchor(CCameraAnchorComponent* pAnchor);

private:
    void UpdateRotation(float deltaTime);
    void ApplyOrientationToAnchor();
    bool ResolveAnchor(CCameraAnchorComponent* pPreferredAnchor = nullptr);
    void UpdateCameraTransform(float deltaTime);

private:
    CCameraAnchorComponent* m_pCameraAnchor = nullptr;
    EntityId m_anchorEntityId = INVALID_ENTITYID;

    Vec2 m_mouseDeltaRotation = ZERO;
    float m_currentYaw = 0.0f;
    float m_currentPitch = 0.0f;
    Vec3 m_currentCameraPosition = ZERO;
    Quat m_currentCameraRotation = IDENTITY;
    bool m_hasInitializedCameraTransform = false;
    bool m_debugLogs = true;

    float m_targetArmLength = 3.25f;
    float m_minArmLength = 0.5f;
    float m_cameraLagSpeed = 10.0f;
    float m_rotationSpeed = 0.0025f;
    float m_pitchMin = -70.0f;
    float m_pitchMax = 70.0f;
    float m_socketOffsetRight = 0.35f;
    float m_socketOffsetUp = 0.25f;
    float m_socketOffsetForward = 0.0f;
    float m_collisionProbeRadius = 0.25f;
    bool m_lockCharacterYaw = false;
};
