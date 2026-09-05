// Copyright 2017-2026 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryEntitySystem/IEntitySystem.h>
#include <CrySchematyc/MathTypes.h>

#include <DefaultComponents/Cameras/CameraComponent.h>

class CCameraAnchorComponent final : public IEntityComponent
{
public:
	CCameraAnchorComponent() = default;
	virtual ~CCameraAnchorComponent() override = default;

	static void ReflectType(Schematyc::CTypeDesc<CCameraAnchorComponent>& desc)
	{
		desc.SetGUID("{FC4DDCA2-311A-4B29-A52C-50A4E3F7FB4A}"_cry_guid);
		desc.SetEditorCategory("Cameras");
		desc.SetLabel("Camera Anchor");
		desc.SetDescription("World camera anchor that the flyby camera can follow.");
		desc.SetComponentFlags({ IEntityComponent::EFlags::Transform, IEntityComponent::EFlags::Socket, IEntityComponent::EFlags::Attach });

		desc.AddMember(&CCameraAnchorComponent::m_bEnabled, 'enbl', "Enabled", "Enabled", "Whether this anchor can drive the flyby camera", true);
		desc.AddMember(&CCameraAnchorComponent::m_bActivateFlybyCamera, 'actv', "ActivateFlybyCamera", "Activate Flyby Camera", "Activates the flyby camera while this anchor is used", true);
		desc.AddMember(&CCameraAnchorComponent::m_nearPlane, 'near', "NearPlane", "Near Plane", "Near clipping plane for the flyby camera", 0.25f);
		desc.AddMember(&CCameraAnchorComponent::m_farPlane, 'far', "FarPlane", "Far Plane", "Far clipping plane for the flyby camera", 1024.0f);
		desc.AddMember(&CCameraAnchorComponent::m_fieldOfView, 'fov', "FieldOfView", "Field of View", "Field of view for the flyby camera", 70.0_degrees);
	}

	bool IsEnabled() const { return m_bEnabled; }
	bool ShouldActivateFlybyCamera() const { return m_bActivateFlybyCamera; }
	float GetNearPlane() const { return m_nearPlane; }
	float GetFarPlane() const { return m_farPlane; }
	CryTransform::CAngle GetFieldOfView() const { return m_fieldOfView; }

	void ApplyToCamera(Cry::DefaultComponents::CCameraComponent& cameraComponent) const
	{
		cameraComponent.EnableAutomaticActivation(m_bActivateFlybyCamera);
		cameraComponent.SetNearPlane(m_nearPlane);
		cameraComponent.SetFarPlane(m_farPlane);
		cameraComponent.SetFieldOfView(m_fieldOfView);

		if (m_bActivateFlybyCamera && !cameraComponent.IsActive())
		{
			cameraComponent.Activate();
		}
	}

	static CCameraAnchorComponent* FindFirstEnabledAnchor()
	{
		IEntityItPtr pEntityIterator = gEnv->pEntitySystem->GetEntityIterator();
		pEntityIterator->MoveFirst();

		while (!pEntityIterator->IsEnd())
		{
			IEntity* pEntity = pEntityIterator->Next();
			if (pEntity == nullptr)
			{
				continue;
			}

			if (CCameraAnchorComponent* pAnchor = pEntity->GetComponent<CCameraAnchorComponent>())
			{
				if (pAnchor->IsEnabled())
				{
					return pAnchor;
				}
			}
		}

		return nullptr;
	}

private:
	bool m_bEnabled = true;
	bool m_bActivateFlybyCamera = true;
	Schematyc::Range<0, 32768> m_nearPlane = 0.25f;
	Schematyc::Range<0, 32768> m_farPlane = 1024.0f;
	CryTransform::CClampedAngle<20, 360> m_fieldOfView = 70.0_degrees;
};
