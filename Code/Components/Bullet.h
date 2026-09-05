// Copyright 2016-2019 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

////////////////////////////////////////////////////////
// Physicalized bullet shot from weaponry, expires on collision with another object
////////////////////////////////////////////////////////
#include <Cry3DEngine/ISurfaceType.h>
#include <CryGame/IGameFramework.h>
#include <CryRenderer/IRenderer.h>          // Required for ColorF structures
#include <CryRenderer/IRenderAuxGeom.h>
#include "SimpleTankComponent.h"
class CBulletComponent final : public IEntityComponent
{
private:
    // Helper function to find the top-most vehicle/mech body
    IEntity* FindRootEntity(IEntity* pEntity) const
    {
        if (!pEntity) return nullptr;

        IEntity* pCurrent = pEntity;
        while (IEntity* pParent = pCurrent->GetParent())
        {
            pCurrent = pParent;
        }
        return pCurrent;
    }

public:
	virtual ~CBulletComponent() {}

	// IEntityComponent
	virtual void Initialize() override
	{
		// Set the model
		const int geometrySlot = 0;
		m_pEntity->LoadGeometry(geometrySlot, "%ENGINE%/EngineAssets/Objects/primitive_sphere.cgf");

		// Load the custom bullet material.
		auto* pBulletMaterial = gEnv->p3DEngine->GetMaterialManager()->LoadMaterial("Materials/bullet");
		m_pEntity->SetMaterial(pBulletMaterial);
		float scaleInput = 1.0f; // Example scale input, adjust as needed

		// SET THE SCALE HERE (4.0f uniform scale for your big projectile)
		Vec3 bulletScale = Vec3(scaleInput, scaleInput, scaleInput);
		m_pEntity->SetScale(bulletScale);

		// Now create the physical representation of the entity
		SEntityPhysicalizeParams physParams;
		physParams.type = PE_RIGID;
		physParams.mass = 20000.f;
		m_pEntity->Physicalize(physParams);

		// Make sure that bullets are always rendered regardless of distance
		GetEntity()->SetViewDistRatio(255);

		// REPLACE THE OLD IMPULSE BLOCK WITH THIS VELOCITY BLOCK HERE:
        if (auto* pPhysics = GetEntity()->GetPhysics())
        {
            // 1. Set velocity with inverted Y-axis column to fix the backward flight direction
            pe_action_set_velocity velocityAction;
            const float speedMetersPerSecond = 50.0f;

            // Inverting Column1 (-GetColumn1) forces the bullet to fly forward relative to the model orientation
            velocityAction.v = -(GetEntity()->GetWorldRotation().GetColumn1()) * speedMetersPerSecond;
            pPhysics->Action(&velocityAction);

            // 2. Set simulation params safely
            // We must zero-initialize the struct memory so we don't accidentally overwrite mass/density with garbage values
            pe_simulation_params simParams;
            memset(&simParams, 0, sizeof(simParams));

            // Disable gravity for this specific projectile so it maintains its slow trajectory without dropping
            simParams.gravity = Vec3(0, 0, 0);
            pPhysics->SetParams(&simParams);
        }
	}

	// Reflect type to set a unique identifier for this component
	static void ReflectType(Schematyc::CTypeDesc<CBulletComponent>& desc)
	{
		desc.SetGUID("{B53A9A5F-F27A-42CB-82C7-B1E379C41A2A}"_cry_guid);
	}

	virtual Cry::Entity::EventFlags GetEventMask() const override { return ENTITY_EVENT_COLLISION; }
	virtual void ProcessEvent(const SEntityEvent& event) override
	{
        if (event.event == ENTITY_EVENT_COLLISION)
        {
            const EventPhysCollision* pCollision = reinterpret_cast<const EventPhysCollision*>(event.nParam[0]);

            if (pCollision)
            {
                int targetIdx = (pCollision->pEntity[0] == GetEntity()->GetPhysics()) ? 1 : 0;
                IPhysicalEntity* pTargetPhysics = pCollision->pEntity[targetIdx];

                if (pTargetPhysics)
                {
                    IEntity* pTargetEntity = gEnv->pEntitySystem->GetEntityFromPhysics(pTargetPhysics);

                    if (pTargetEntity)
                    {
                        // FIX: Manually resolve the top-most parent root
                        IEntity* pMyRoot = FindRootEntity(GetEntity());
                        IEntity* pTargetRoot = FindRootEntity(pTargetEntity);

                        if (pTargetEntity == GetEntity() || pTargetRoot == pMyRoot)
                        {
                            return; // Ignore collision with your own gun hierarchy/vehicle body
                        }

                        if (pTargetEntity->GetComponent<CBulletComponent>() != nullptr)
                        {
                            return; // Ignore overlapping giant bullets hitting each other
                        }

                        const char* targetName = pTargetEntity->GetName();
                        EntityId targetId = pTargetEntity->GetId();
                        CryLogAlways("[Bullet] Hit active entity: %s (ID: %u)", (targetName && targetName[0] != '\0') ? targetName : "Unnamed Attachment", targetId);
                    }
                    else
                    {
                        // Handle Static Environment
                        int surfaceId = pCollision->idmat[targetIdx];

                        ISurfaceTypeManager* pSurfaceTypeManager = gEnv->p3DEngine->GetMaterialManager()->GetSurfaceTypeManager();
                        if (pSurfaceTypeManager)
                        {
                            ISurfaceType* pSurfaceType = pSurfaceTypeManager->GetSurfaceType(surfaceId);
                            if (pSurfaceType)
                            {
                                Vec3 impactPos = pCollision->pt;

                                CryLogAlways("[Bullet] Hit Static Environment! Material: '%s' | Type: '%s' AT Position: Vec3(%.2f, %.2f, %.2f)",
                                    pSurfaceType->GetName(),
                                    pSurfaceType->GetType(),
                                    impactPos.x,
                                    impactPos.y,
                                    impactPos.z);

                                if (gEnv->pRenderer && gEnv->pRenderer->GetIRenderAuxGeom())
                                {
                                    Vec3 markerPos = impactPos + Vec3(0.0f, 0.0f, 0.5f);
                                    gEnv->pRenderer->GetIRenderAuxGeom()->DrawCone(
                                        markerPos,
                                        Vec3(0.0f, 0.0f, -1.0f),
                                        0.8f,
                                        1.5f,
                                        ColorB(255, 255, 0)
                                    );
                                }
                            }
                        }
                    }
                }
            }

            // Clean up the bullet from the scene now that it hit something valid
            gEnv->pEntitySystem->RemoveEntity(GetEntityId());
        }
	}
};