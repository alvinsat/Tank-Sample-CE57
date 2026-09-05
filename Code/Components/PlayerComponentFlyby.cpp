#include "StdAfx.h"
#include "PlayerComponentFlyby.h"
#include "AINavigationBot.h"
#include "CameraAnchor.h"
#include "NavigationAgentComponent.h"
#include "SpawnPoint.h"
#include "TPSCameraComponent.h"

#include <CryCore/StaticInstanceList.h>
#include <CryInput/IInput.h>
#include <CryPhysics/physinterface.h>
#include <CryRenderer/IRenderAuxGeom.h>
#include <CrySchematyc/Env/Elements/EnvComponent.h>

namespace
{
	static void RegisterPlayerComponentFlyby(Schematyc::IEnvRegistrar& registrar)
	{
		Schematyc::CEnvRegistrationScope scope = registrar.Scope(IEntity::GetEntityScopeGUID());
		{
			Schematyc::CEnvRegistrationScope componentScope = scope.Register(SCHEMATYC_MAKE_ENV_COMPONENT(CPlayerComponentFlyby));
		}
	}

	CRY_STATIC_AUTO_REGISTER_FUNCTION(&RegisterPlayerComponentFlyby);

	static CSpawnPointComponent* FindFirstSpawnPointComponent()
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

			if (CSpawnPointComponent* pSpawnPoint = pEntity->GetComponent<CSpawnPointComponent>())
			{
				return pSpawnPoint;
			}
		}

		return nullptr;
	}

	static CTPSCameraComponent* FindTPSCameraForAnchor(CCameraAnchorComponent* pAnchor)
	{
		if (pAnchor == nullptr)
		{
			return nullptr;
		}

		IEntityItPtr pEntityIterator = gEnv->pEntitySystem->GetEntityIterator();
		pEntityIterator->MoveFirst();

		while (!pEntityIterator->IsEnd())
		{
			IEntity* pEntity = pEntityIterator->Next();
			if (pEntity == nullptr)
			{
				continue;
			}

			if (CTPSCameraComponent* pTPSCamera = pEntity->GetComponent<CTPSCameraComponent>())
			{
				if (pTPSCamera->UsesAnchor(pAnchor))
				{
					return pTPSCamera;
				}
			}
		}

		return nullptr;
	}
}

void CPlayerComponentFlyby::ReflectType(Schematyc::CTypeDesc<CPlayerComponentFlyby>& desc)
{
	desc.SetGUID("{A1B2C3D4-E5F6-4A7B-8C9D-0E1F2A3B4C5D}"_cry_guid);
	desc.SetEditorCategory("Gameplay");
	desc.SetLabel("Player Flyby Camera");
	desc.SetDescription("A free-look flying camera component with click raycast debug.");
	desc.SetComponentFlags({ IEntityComponent::EFlags::Transform });

	desc.AddMember(&CPlayerComponentFlyby::m_moveSpeed, 'msp', "MoveSpeed", "Movement Speed", "Base speed of the flyby camera", 60.0f);
	desc.AddMember(&CPlayerComponentFlyby::m_rotationSpeed, 'rsp', "RotationSpeed", "Rotation Sensitivity", "Mouse look sensitivity", 0.002f);
	desc.AddMember(&CPlayerComponentFlyby::m_boostMultiplier, 'bst', "BoostMultiplier", "Boost Multiplier", "Speed multiplier while holding shift", 8.0f);
	desc.AddMember(&CPlayerComponentFlyby::m_debugSphereRadius, 'dsr', "DebugSphereRadius", "Debug Sphere Radius", "Radius of the hit marker sphere", 0.15f);
	desc.AddMember(&CPlayerComponentFlyby::m_spawnHeightOffset, 'sho', "SpawnHeightOffset", "Spawn Height Offset", "Extra vertical offset applied on top of the spawn point", 1.8f);
}

void CPlayerComponentFlyby::Initialize()
{
	m_lookOrientation = GetEntity()->GetWorldRotation();

	if (IsLocalClient())
	{
		InitializeLocalPlayer();
	}
}

Cry::Entity::EventFlags CPlayerComponentFlyby::GetEventMask() const
{
	return Cry::Entity::EEvent::BecomeLocalPlayer | Cry::Entity::EEvent::Update | Cry::Entity::EEvent::Reset;
}

void CPlayerComponentFlyby::ProcessEvent(const SEntityEvent& event)
{
	switch (event.event)
	{
	case Cry::Entity::EEvent::BecomeLocalPlayer:
		InitializeLocalPlayer();
		break;

	case Cry::Entity::EEvent::Update:
		Update(event.fParam[0]);
		break;

	case Cry::Entity::EEvent::Reset:
		if (IsLocalClient())
		{
			CryLog("[Flyby] Reset event received. Re-resolving spawn point for local player.");
			m_hasAppliedSpawnPoint = false;
			m_hasAttemptedFirstUpdateSnap = false;
			m_hasSpawnPoint = false;
			m_hasInitializedFromView = false;
			m_spawnRetryLogTimer = 0.0f;
			m_positionLogTimer = 0.0f;
			InitializeLocalPlayer();
		}
		break;
	}
}

void CPlayerComponentFlyby::Update(float deltaTime)
{
	if (!IsLocalClient())
	{
		return;
	}

	ClearCameraWorldTransformOverride();

	if (!m_hasAttemptedFirstUpdateSnap)
	{
		m_hasAttemptedFirstUpdateSnap = true;
		m_hasAppliedSpawnPoint = false;
		CryLog("[Flyby] First Update() reached. Snapping to spawn point once.");
		SnapToSpawnPoint();
	}

	if (FollowTPSCameraIfAvailable(deltaTime))
	{
		UpdateCamera();
		FireViewRaycast();
		DrawDebugHit();
		return;
	}

	if (FollowCameraAnchorIfAvailable())
	{
		UpdateCamera();
		FireViewRaycast();
		DrawDebugHit();
		return;
	}

	UpdateLookDirection(deltaTime);
	UpdateMovement(deltaTime);
	UpdateCamera();
	LogSpawnAndFlybyPosition(deltaTime);
	FireViewRaycast();
	DrawDebugHit();
}

void CPlayerComponentFlyby::InitializeLocalPlayer()
{
	if (m_pCameraComponent == nullptr)
	{
		m_pCameraComponent = m_pEntity->GetOrCreateComponent<Cry::DefaultComponents::CCameraComponent>();
		m_pCameraComponent->SetFieldOfView(CryTransform::CAngle::FromDegrees(60.0f));
	}

	if (!m_hasInitializedFromView)
	{
		SnapToSpawnPoint();
		m_hasInitializedFromView = true;
	}

	if (m_pInputComponent != nullptr)
	{
		return;
	}

	m_pInputComponent = m_pEntity->GetOrCreateComponent<Cry::DefaultComponents::CInputComponent>();

	m_pInputComponent->RegisterAction("flyby", "moveleft", [this](int activationMode, float value)
	{
		HandleInputFlagChange(EInputFlag::MoveLeft, (EActionActivationMode)activationMode);
	});
	m_pInputComponent->BindAction("flyby", "moveleft", eAID_KeyboardMouse, EKeyId::eKI_A);

	m_pInputComponent->RegisterAction("flyby", "moveright", [this](int activationMode, float value)
	{
		HandleInputFlagChange(EInputFlag::MoveRight, (EActionActivationMode)activationMode);
	});
	m_pInputComponent->BindAction("flyby", "moveright", eAID_KeyboardMouse, EKeyId::eKI_D);

	m_pInputComponent->RegisterAction("flyby", "moveforward", [this](int activationMode, float value)
	{
		HandleInputFlagChange(EInputFlag::MoveForward, (EActionActivationMode)activationMode);
	});
	m_pInputComponent->BindAction("flyby", "moveforward", eAID_KeyboardMouse, EKeyId::eKI_W);

	m_pInputComponent->RegisterAction("flyby", "moveback", [this](int activationMode, float value)
	{
		HandleInputFlagChange(EInputFlag::MoveBack, (EActionActivationMode)activationMode);
	});
	m_pInputComponent->BindAction("flyby", "moveback", eAID_KeyboardMouse, EKeyId::eKI_S);

	m_pInputComponent->RegisterAction("flyby", "moveup", [this](int activationMode, float value)
	{
		HandleInputFlagChange(EInputFlag::MoveUp, (EActionActivationMode)activationMode);
	});
	m_pInputComponent->BindAction("flyby", "moveup", eAID_KeyboardMouse, EKeyId::eKI_Space);

	m_pInputComponent->RegisterAction("flyby", "movedown", [this](int activationMode, float value)
	{
		HandleInputFlagChange(EInputFlag::MoveDown, (EActionActivationMode)activationMode);
	});
	m_pInputComponent->BindAction("flyby", "movedown", eAID_KeyboardMouse, EKeyId::eKI_C);

	m_pInputComponent->RegisterAction("flyby", "mouse_rotateyaw", [this](int activationMode, float value)
	{
		m_mouseDeltaRotation.x -= value;
	});
	m_pInputComponent->BindAction("flyby", "mouse_rotateyaw", eAID_KeyboardMouse, EKeyId::eKI_MouseX);

	m_pInputComponent->RegisterAction("flyby", "mouse_rotatepitch", [this](int activationMode, float value)
	{
		m_mouseDeltaRotation.y -= value;
	});
	m_pInputComponent->BindAction("flyby", "mouse_rotatepitch", eAID_KeyboardMouse, EKeyId::eKI_MouseY);

	m_pInputComponent->RegisterAction("flyby", "raycast", [this](int activationMode, float value)
	{
		(void)value;

		if (activationMode == eAAM_OnPress)
		{
			FireViewRaycast();
			if (!m_hasDebugHit)
			{
				CryLog("[Flyby] Raycast did not hit anything. No navigation command sent.");
				return;
			}

			if (IEntity* pAgentEntity = gEnv->pEntitySystem->FindEntityByName("MyAIAgent"))
			{
				if (CAINavigationBotComponent* pNavBot = pAgentEntity->GetComponent<CAINavigationBotComponent>())
				{
					pNavBot->SetDestination(m_debugHitPosition);
					CryLog("[Flyby] Commanding AINavigationBot to move to: %.2f, %.2f, %.2f", m_debugHitPosition.x, m_debugHitPosition.y, m_debugHitPosition.z);
				}
				else if (CNavigationAgentComponent* pNavAgent = pAgentEntity->GetComponent<CNavigationAgentComponent>())
				{
					pNavAgent->SetDestination(m_debugHitPosition);
					CryLog("[Flyby] Commanding Agent to move to: %.2f, %.2f, %.2f", m_debugHitPosition.x, m_debugHitPosition.y, m_debugHitPosition.z);
				}
			}
		}
	});
	m_pInputComponent->BindAction("flyby", "raycast", eAID_KeyboardMouse, EKeyId::eKI_Mouse1);

	UpdateCamera();
}

bool CPlayerComponentFlyby::FollowTPSCameraIfAvailable(float deltaTime)
{
	CCameraAnchorComponent* pCameraAnchor = CCameraAnchorComponent::FindFirstEnabledAnchor();
	if (pCameraAnchor == nullptr)
	{
		return false;
	}

	CTPSCameraComponent* pTPSCamera = FindTPSCameraForAnchor(pCameraAnchor);
	if (pTPSCamera == nullptr)
	{
		return false;
	}

	Matrix34 cameraTransform = IDENTITY;
	if (!pTPSCamera->UpdateCameraOverride(deltaTime, m_mouseDeltaRotation, pCameraAnchor, cameraTransform))
	{
		return false;
	}

	m_mouseDeltaRotation = ZERO;
	m_lookOrientation = Quat(cameraTransform);

	IEntity* pAnchorEntity = pCameraAnchor->GetEntity();
	if (pAnchorEntity == GetEntity())
	{
		SetCameraWorldTransformOverride(cameraTransform);
		m_lookOrientation = pAnchorEntity->GetWorldRotation();
	}
	else
	{
		GetEntity()->SetPosRotScale(cameraTransform.GetTranslation(), m_lookOrientation, Vec3(1.0f, 1.0f, 1.0f));
	}

	if (m_pCameraComponent != nullptr)
	{
		pCameraAnchor->ApplyToCamera(*m_pCameraComponent);
	}

	m_isFollowingCameraAnchor = true;
	return true;
}

bool CPlayerComponentFlyby::ResolveSpawnPointTransform(Matrix34& outTransform) const
{
	if (CSpawnPointComponent* pSpawnPoint = FindFirstSpawnPointComponent())
	{
		IEntity* pSpawnEntity = pSpawnPoint->GetEntity();
		outTransform = pSpawnPoint->GetWorldTransformMatrix();

		if (pSpawnEntity != nullptr)
		{
			const Vec3 worldPosition = pSpawnEntity->GetWorldPos();
			CryLog("[Flyby] Resolved CSpawnPointComponent on entity '%s' (id=%u) at world pos=(%.2f, %.2f, %.2f)",
				pSpawnEntity->GetName(),
				static_cast<unsigned>(pSpawnEntity->GetId()),
				worldPosition.x, worldPosition.y, worldPosition.z);
		}
		else
		{
			const Vec3 worldPosition = outTransform.GetTranslation();
			CryLog("[Flyby] Resolved CSpawnPointComponent with null owner entity at world pos=(%.2f, %.2f, %.2f)",
				worldPosition.x, worldPosition.y, worldPosition.z);
		}

		return true;
	}

	outTransform = IDENTITY;
	CryLog("[Flyby] CSpawnPointComponent not found while resolving spawn transform.");
	return false;
}

void CPlayerComponentFlyby::SnapToSpawnPoint()
{
	if (CCameraAnchorComponent* pCameraAnchor = CCameraAnchorComponent::FindFirstEnabledAnchor())
	{
		const Matrix34 anchorTransform = pCameraAnchor->GetWorldTransformMatrix();
		const Vec3 anchorPosition = anchorTransform.GetTranslation();
		const Quat anchorRotation = Quat(anchorTransform);

		m_isFollowingCameraAnchor = true;
		m_lookOrientation = anchorRotation;
		m_lastResolvedSpawnPosition = anchorPosition;
		m_lastResolvedSpawnRotation = anchorRotation;
		m_positionLogTimer = 0.0f;

		GetEntity()->SetPosRotScale(anchorPosition, anchorRotation, Vec3(1.0f, 1.0f, 1.0f));

		if (m_pCameraComponent != nullptr)
		{
			pCameraAnchor->ApplyToCamera(*m_pCameraComponent);
		}

		CryLog("[Flyby] CameraAnchor found. Starting flyby at anchor pos=(%.2f, %.2f, %.2f)",
			anchorPosition.x, anchorPosition.y, anchorPosition.z);
		return;
	}

	Matrix34 spawnTransform = IDENTITY;
	m_hasSpawnPoint = ResolveSpawnPointTransform(spawnTransform);
	Vec3 spawnPosition = spawnTransform.GetTranslation();
	Quat spawnRotation = Quat(spawnTransform);
	m_isFollowingCameraAnchor = false;

	if (!m_hasSpawnPoint)
	{
		const CCamera& viewCamera = gEnv->pSystem->GetViewCamera();
		spawnPosition = viewCamera.GetPosition();
		spawnRotation = Quat(Matrix33(viewCamera.GetMatrix()));
		CryLog("[Flyby] No SpawnPoint found. Falling back to current view camera at pos=(%.2f, %.2f, %.2f)", spawnPosition.x, spawnPosition.y, spawnPosition.z);
		m_hasAppliedSpawnPoint = false;
	}
	else
	{
		// CryLog("[Flyby] SpawnPoint found at pos=(%.2f, %.2f, %.2f)", spawnPosition.x, spawnPosition.y, spawnPosition.z);
		m_hasAppliedSpawnPoint = true;
	}

	// spawnPosition.z += m_spawnHeightOffset;
	m_lookOrientation = spawnRotation;
	m_lastResolvedSpawnPosition = spawnPosition;
	m_lastResolvedSpawnRotation = spawnRotation;
	m_positionLogTimer = 0.0f;

	GetEntity()->SetPosRotScale(spawnPosition, m_lookOrientation, Vec3(1.0f, 1.0f, 1.0f));

	CryLog("[Flyby] PlayerComponentFlyby initial pos=(%.2f, %.2f, %.2f) rot=(%.3f, %.3f, %.3f, %.3f)",
		spawnPosition.x, spawnPosition.y, spawnPosition.z,
		m_lookOrientation.w, m_lookOrientation.v.x, m_lookOrientation.v.y, m_lookOrientation.v.z);

	if (m_pCameraComponent != nullptr)
	{
		m_pCameraComponent->SetFieldOfView(CryTransform::CAngle::FromDegrees(60.0f));
	}
}

bool CPlayerComponentFlyby::FollowCameraAnchorIfAvailable()
{
	CCameraAnchorComponent* pCameraAnchor = CCameraAnchorComponent::FindFirstEnabledAnchor();
	if (pCameraAnchor == nullptr)
	{
		m_isFollowingCameraAnchor = false;
		return false;
	}

	const Matrix34 anchorTransform = pCameraAnchor->GetWorldTransformMatrix();
	m_lookOrientation = Quat(anchorTransform);
	m_mouseDeltaRotation = ZERO;
	GetEntity()->SetPosRotScale(anchorTransform.GetTranslation(), m_lookOrientation, Vec3(1.0f, 1.0f, 1.0f));

	if (m_pCameraComponent != nullptr)
	{
		pCameraAnchor->ApplyToCamera(*m_pCameraComponent);
	}

	m_isFollowingCameraAnchor = true;
	return true;
}

void CPlayerComponentFlyby::UpdateLookDirection(float deltaTime)
{
	if (deltaTime <= 0.0f)
	{
		m_mouseDeltaRotation = ZERO;
		return;
	}

	Ang3 ypr = CCamera::CreateAnglesYPR(Matrix33(m_lookOrientation));
	ypr.x += m_mouseDeltaRotation.x * m_rotationSpeed;
	ypr.y = CLAMP(ypr.y + m_mouseDeltaRotation.y * m_rotationSpeed, -m_maxPitch, m_maxPitch);
	ypr.z = 0.0f;

	m_lookOrientation = Quat(CCamera::CreateOrientationYPR(ypr));
	m_mouseDeltaRotation = ZERO;

	GetEntity()->SetPosRotScale(GetEntity()->GetWorldPos(), m_lookOrientation, Vec3(1.0f, 1.0f, 1.0f));
}

void CPlayerComponentFlyby::UpdateMovement(float deltaTime)
{
	Vec3 localMovement = ZERO;

	if (m_inputFlags & EInputFlag::MoveLeft)    localMovement.x -= 1.0f;
	if (m_inputFlags & EInputFlag::MoveRight)   localMovement.x += 1.0f;
	if (m_inputFlags & EInputFlag::MoveForward) localMovement.y += 1.0f;
	if (m_inputFlags & EInputFlag::MoveBack)    localMovement.y -= 1.0f;
	if (m_inputFlags & EInputFlag::MoveUp)      localMovement.z += 1.0f;
	if (m_inputFlags & EInputFlag::MoveDown)    localMovement.z -= 1.0f;

	if (gEnv->pInput != nullptr)
	{
		if (gEnv->pInput->InputState("a", eIS_Down))     localMovement.x -= 1.0f;
		if (gEnv->pInput->InputState("d", eIS_Down))     localMovement.x += 1.0f;
		if (gEnv->pInput->InputState("w", eIS_Down))     localMovement.y += 1.0f;
		if (gEnv->pInput->InputState("s", eIS_Down))     localMovement.y -= 1.0f;
		if (gEnv->pInput->InputState("space", eIS_Down)) localMovement.z += 1.0f;
		if (gEnv->pInput->InputState("c", eIS_Down))     localMovement.z -= 1.0f;
	}

	if (!localMovement.IsZero())
	{
		localMovement.NormalizeSafe();
	}

	float speed = m_moveSpeed;
	if (gEnv->pInput != nullptr && gEnv->pInput->InputState("lshift", eIS_Down))
	{
		speed *= m_boostMultiplier;
	}

	const Vec3 worldMovement = m_lookOrientation * (localMovement * speed * deltaTime);
	GetEntity()->SetPosRotScale(GetEntity()->GetWorldPos() + worldMovement, m_lookOrientation, Vec3(1.0f, 1.0f, 1.0f));
}

void CPlayerComponentFlyby::UpdateCamera()
{
	if (m_pCameraComponent == nullptr)
	{
		return;
	}

	if (m_hasCameraTransformOverride)
	{
		m_pCameraComponent->SetTransformMatrix(m_cameraLocalTransformOverride);
		return;
	}

	m_pCameraComponent->SetTransformMatrix(Matrix34::Create(Vec3(1.0f), IDENTITY, ZERO));
}

void CPlayerComponentFlyby::SetCameraWorldTransformOverride(const Matrix34& worldTransform)
{
	const Matrix34 entityWorldTransform = GetEntity()->GetWorldTM();
	m_cameraLocalTransformOverride = entityWorldTransform.GetInverted() * worldTransform;
	m_hasCameraTransformOverride = true;
}

void CPlayerComponentFlyby::ClearCameraWorldTransformOverride()
{
	m_hasCameraTransformOverride = false;
	m_cameraLocalTransformOverride = IDENTITY;
}

void CPlayerComponentFlyby::LogSpawnAndFlybyPosition(float deltaTime)
{
	m_positionLogTimer += deltaTime;
	// if (m_positionLogTimer < 1.0f)
	// {
	// 	return;
	// }

	// m_positionLogTimer = 0.0f;

	// const Vec3 currentPosition = GetEntity()->GetWorldPos();

	// if (m_hasSpawnPoint)
	// {
	// 	CryLog("[Flyby] SpawnPoint pos=(%.2f, %.2f, %.2f) | Flyby pos=(%.2f, %.2f, %.2f)",
	// 		m_lastResolvedSpawnPosition.x, m_lastResolvedSpawnPosition.y, m_lastResolvedSpawnPosition.z,
	// 		currentPosition.x, currentPosition.y, currentPosition.z);
	// }
	// else
	// {
	// 	CryLog("[Flyby] No SpawnPoint in world | Fallback start pos=(%.2f, %.2f, %.2f) | Flyby pos=(%.2f, %.2f, %.2f)",
	// 		m_lastResolvedSpawnPosition.x, m_lastResolvedSpawnPosition.y, m_lastResolvedSpawnPosition.z,
	// 		currentPosition.x, currentPosition.y, currentPosition.z);
	// }
}

void CPlayerComponentFlyby::FireViewRaycast()
{
	const CCamera& viewCamera = gEnv->pSystem->GetViewCamera();
	const Vec3 rayOrigin = viewCamera.GetPosition();
	const Vec3 rayDirection = viewCamera.GetViewdir() * 1000.0f;
	IPhysicalEntity* pSkipEntities[1] = { m_pEntity->GetPhysics() };
	IPhysicalEntity** pSkipEntityList = m_pEntity->GetPhysics() != nullptr ? pSkipEntities : nullptr;
	const int skipEntityCount = m_pEntity->GetPhysics() != nullptr ? 1 : 0;

	ray_hit hit;
	const int hitCount = gEnv->pPhysicalWorld->RayWorldIntersection(
		rayOrigin,
		rayDirection,
		ent_all,
		rwi_stop_at_pierceable | rwi_colltype_any,
		&hit,
		1,
		pSkipEntityList,
		skipEntityCount);

	if (hitCount > 0)
	{
		m_hasDebugHit = true;
		m_debugHitPosition = hit.pt;
	}
	else
	{
		m_hasDebugHit = false;
	}
}

void CPlayerComponentFlyby::DrawDebugHit() const
{
	if (!m_hasDebugHit)
	{
		return;
	}

	if (IRenderAuxGeom* pAuxGeom = gEnv->pRenderer != nullptr ? gEnv->pRenderer->GetIRenderAuxGeom() : nullptr)
	{
		pAuxGeom->DrawSphere(m_debugHitPosition, m_debugSphereRadius, ColorB(255, 64, 64, 255), true);
	}
}

void CPlayerComponentFlyby::HandleInputFlagChange(const CEnumFlags<EInputFlag> flags, const CEnumFlags<EActionActivationMode> activationMode)
{
	if (activationMode == eAAM_OnRelease)
	{
		m_inputFlags &= ~flags;
	}
	else
	{
		m_inputFlags |= flags;
	}
}
