// BarrelComponent.h
#pragma once

#include <CryEntitySystem/IEntityComponent.h>
#include <CrySchematyc/Utils/SharedString.h>
#include <CrySchematyc/ResourceTypes.h>

class CSimpleTankComponent;

class CBarrelComponent : public IEntityComponent
{
public:
    CBarrelComponent() = default;
    virtual ~CBarrelComponent() override = default;

    static void ReflectType(Schematyc::CTypeDesc<CBarrelComponent>& desc);

    // IEntityComponent
    virtual void Initialize() override;
    virtual Cry::Entity::EventFlags GetEventMask() const override;
    virtual void ProcessEvent(const SEntityEvent& event) override;
    // ~IEntityComponent

    void DebugDrawRay();

protected:
    void LoadBarrelMesh();
    void FireMainCannon(bool isPreviewLine);
    Matrix34 CreateSlotTM(float yawDegrees, float pitchDegrees, const Vec3& offset, bool applyPitch) const;

    Schematyc::GeomFileName m_barrelMesh;
    Vec3 m_barrelSlotOffset = Vec3(0.0f, 0.0f, 0.0f);
    bool m_useCameraForward = false;
    Schematyc::CSharedString m_tankEntityName = "";
    int m_barrelSlot = -1;

    // weapon informatino
    bool m_barrelHasWeapon = false;        // Invert pitch rotation
    Vec3 m_barrelWeaponOffset = Vec3(0.0f, 0.0f, 0.0f);


    // Rotation offsets and constraints
    float m_yawOffset = 0.0f;          // Yaw rotation offset (degrees)
    float m_pitchOffset = 0.0f;        // Pitch rotation offset (degrees)
    float m_minYaw = -180.0f;          // Minimum yaw rotation (degrees)
    float m_maxYaw = 180.0f;           // Maximum yaw rotation (degrees)
    float m_minPitch = -45.0f;         // Minimum pitch rotation (degrees)
    float m_maxPitch = 45.0f;          // Maximum pitch rotation (degrees)
    bool m_invertYaw = false;          // Invert yaw rotation
    bool m_invertPitch = false;        // Invert pitch rotation


    float m_currentYaw = 0.0f;
    float m_currentPitch = 0.0f;

    CSimpleTankComponent* m_pTank = nullptr;
};
