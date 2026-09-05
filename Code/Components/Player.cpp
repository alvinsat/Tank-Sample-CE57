// Copyright 2016-2020 Crytek GmbH / Crytek Group. All rights reserved.
#include "StdAfx.h"
#include "Player.h"
#include "Bullet.h"
#include "SpawnPoint.h"
#include "GamePlugin.h"

#include <CryRenderer/IRenderAuxGeom.h>
#include <CrySchematyc/Env/Elements/EnvComponent.h>
#include <CryCore/StaticInstanceList.h>
#include <CryNetwork/Rmi.h>

namespace
{
	static void RegisterPlayerComponent(Schematyc::IEnvRegistrar& registrar)
	{
		Schematyc::CEnvRegistrationScope scope = registrar.Scope(IEntity::GetEntityScopeGUID());
		{
			Schematyc::CEnvRegistrationScope componentScope = scope.Register(SCHEMATYC_MAKE_ENV_COMPONENT(CPlayerComponent));
		}
	}

	 CRY_STATIC_AUTO_REGISTER_FUNCTION(&RegisterPlayerComponent);
}

// ---------------------------------------------------------------------------
void CPlayerComponent::Initialize()
{
	m_pCharacterController = m_pEntity->GetOrCreateComponent<Cry::DefaultComponents::CCharacterControllerComponent>();
	m_pCharacterController->SetTransformMatrix(Matrix34::Create(Vec3(1.f), IDENTITY, Vec3(0, 0, 1.f)));

	m_pAnimationComponent = m_pEntity->GetOrCreateComponent<Cry::DefaultComponents::CAdvancedAnimationComponent>();
	m_pAnimationComponent->SetMannequinAnimationDatabaseFile("Animations/Mannequin/ADB/FirstPerson.adb");
	m_pAnimationComponent->SetCharacterFile("Objects/Characters/SampleCharacter/thirdperson.cdf");
	m_pAnimationComponent->SetControllerDefinitionFile("Animations/Mannequin/ADB/FirstPersonControllerDefinition.xml");
	m_pAnimationComponent->SetDefaultScopeContextName("FirstPersonCharacter");
	m_pAnimationComponent->SetDefaultFragmentName("Idle");
	m_pAnimationComponent->SetAnimationDrivenMotion(true);
	m_pAnimationComponent->LoadFromDisk();

	m_idleFragmentId = m_pAnimationComponent->GetFragmentId("Idle");
	m_walkFragmentId = m_pAnimationComponent->GetFragmentId("Walk");
	m_rotateTagId    = m_pAnimationComponent->GetTagId("Rotate");

	m_pEntity->GetNetEntity()->BindToNetwork();
	SRmi<RMI_WRAP(&CPlayerComponent::RemoteReviveOnClient)>::Register(this, eRAT_NoAttach, false, eNRT_ReliableOrdered);
}

// ---------------------------------------------------------------------------
void CPlayerComponent::InitializeLocalPlayer()
{
	m_pCameraComponent        = m_pEntity->GetOrCreateComponent<Cry::DefaultComponents::CCameraComponent>();
	m_pAudioListenerComponent = m_pEntity->GetOrCreateComponent<Cry::Audio::DefaultComponents::CListenerComponent>();
	m_pInputComponent         = m_pEntity->GetOrCreateComponent<Cry::DefaultComponents::CInputComponent>();

	m_pInputComponent->RegisterAction("player", "moveleft",    [this](int activationMode, float value) { HandleInputFlagChange(EInputFlag::MoveLeft,    (EActionActivationMode)activationMode); });
	m_pInputComponent->BindAction("player", "moveleft",    eAID_KeyboardMouse, EKeyId::eKI_A);

	m_pInputComponent->RegisterAction("player", "moveright",   [this](int activationMode, float value) { HandleInputFlagChange(EInputFlag::MoveRight,   (EActionActivationMode)activationMode); });
	m_pInputComponent->BindAction("player", "moveright",   eAID_KeyboardMouse, EKeyId::eKI_D);

	m_pInputComponent->RegisterAction("player", "moveforward", [this](int activationMode, float value) { HandleInputFlagChange(EInputFlag::MoveForward, (EActionActivationMode)activationMode); });
	m_pInputComponent->BindAction("player", "moveforward", eAID_KeyboardMouse, EKeyId::eKI_W);

	m_pInputComponent->RegisterAction("player", "moveback",    [this](int activationMode, float value) { HandleInputFlagChange(EInputFlag::MoveBack,    (EActionActivationMode)activationMode); });
	m_pInputComponent->BindAction("player", "moveback",    eAID_KeyboardMouse, EKeyId::eKI_S);

	m_pInputComponent->RegisterAction("player", "mouse_rotateyaw",   [this](int activationMode, float value) { m_mouseDeltaRotation.x -= value; });
	m_pInputComponent->BindAction("player", "mouse_rotateyaw",   eAID_KeyboardMouse, EKeyId::eKI_MouseX);

	m_pInputComponent->RegisterAction("player", "mouse_rotatepitch", [this](int activationMode, float value) { m_mouseDeltaRotation.y -= value; });
	m_pInputComponent->BindAction("player", "mouse_rotatepitch", eAID_KeyboardMouse, EKeyId::eKI_MouseY);

	m_pInputComponent->RegisterAction("player", "shoot", [this](int activationMode, float value)
	{
		if (activationMode == eAAM_OnPress)
		{
			if (ICharacterInstance* pCharacter = m_pAnimationComponent->GetCharacter())
			{
				IAttachment* pBarrelOutAttachment = pCharacter->GetIAttachmentManager()->GetInterfaceByName("barrel_out");
				if (pBarrelOutAttachment != nullptr)
				{
					QuatTS bulletOrigin = pBarrelOutAttachment->GetAttWorldAbsolute();

					SEntitySpawnParams spawnParams;
					spawnParams.pClass    = gEnv->pEntitySystem->GetClassRegistry()->GetDefaultClass();
					spawnParams.vPosition = bulletOrigin.t;
					spawnParams.qRotation = bulletOrigin.q;
					spawnParams.vScale    = Vec3(0.05f);

					if (IEntity* pEntity = gEnv->pEntitySystem->SpawnEntity(spawnParams))
						pEntity->CreateComponentClass<CBulletComponent>();
				}
			}
		}
	});
	m_pInputComponent->BindAction("player", "shoot", eAID_KeyboardMouse, EKeyId::eKI_Mouse1);
}

// ---------------------------------------------------------------------------
Cry::Entity::EventFlags CPlayerComponent::GetEventMask() const
{
	return
		Cry::Entity::EEvent::BecomeLocalPlayer;
		//Cry::Entity::EEvent::Update            |
		//Cry::Entity::EEvent::Reset;
}

// ---------------------------------------------------------------------------
void CPlayerComponent::ProcessEvent(const SEntityEvent& event)
{
	switch (event.event)
	{
	case Cry::Entity::EEvent::BecomeLocalPlayer:
		//InitializeLocalPlayer();
		break;

	case Cry::Entity::EEvent::Update:
	{
		if (!m_isAlive)
			return;

		const float frameTime = event.fParam[0];

		UpdateMovementRequest(frameTime);
		UpdateLookDirectionRequest(frameTime);
		UpdateAnimation(frameTime);

		if (IsLocalClient())
			UpdateCamera(frameTime);
	}
	break;

	case Cry::Entity::EEvent::Reset:
		m_isAlive = event.nParam[0] != 0;
		break;
	}
}

// ---------------------------------------------------------------------------
bool CPlayerComponent::NetSerialize(TSerialize ser, EEntityAspects aspect, uint8 profile, int flags)
{
	if (aspect == InputAspect)
	{
		ser.BeginGroup("PlayerInput");

		const CEnumFlags<EInputFlag> prevInputFlags = m_inputFlags;
		ser.Value("m_inputFlags", m_inputFlags.UnderlyingValue(), 'ui8');

		if (ser.IsReading())
		{
			const CEnumFlags<EInputFlag> changedKeys  = prevInputFlags ^ m_inputFlags;
			const CEnumFlags<EInputFlag> pressedKeys  = changedKeys & prevInputFlags;
			const CEnumFlags<EInputFlag> releasedKeys = changedKeys & prevInputFlags;

			if (!pressedKeys.IsEmpty())  HandleInputFlagChange(pressedKeys,  eAAM_OnPress);
			if (!releasedKeys.IsEmpty()) HandleInputFlagChange(releasedKeys, eAAM_OnRelease);
		}

		ser.Value("m_lookOrientation", m_lookOrientation, 'ori3');
		ser.EndGroup();
	}
	return true;
}

// ---------------------------------------------------------------------------
void CPlayerComponent::UpdateMovementRequest(float frameTime)
{
	// Exactly the CE sample pattern — entity rotation drives movement direction.
	// Root motion handles displacement, so we still use AddVelocity here only
	// when NOT using animation-driven motion, but we keep entity orientation
	// driving the direction so the blend space root motion goes the right way.
	if (!m_pCharacterController->IsOnGround())
		return;

	Vec3 velocity = ZERO;
	const float moveSpeed = 20.5f;

	// Build velocity in entity-local space — same as CE sample.
	// Entity rotation (yaw from m_lookOrientation) is set in UpdateAnimation,
	// so movement is always relative to where the character is facing,
	// which is always camera-forward (orbit yaw drives m_lookOrientation yaw).
	if (m_inputFlags & EInputFlag::MoveLeft)    velocity.x -= moveSpeed * frameTime;
	if (m_inputFlags & EInputFlag::MoveRight)   velocity.x += moveSpeed * frameTime;
	if (m_inputFlags & EInputFlag::MoveForward) velocity.y += moveSpeed * frameTime;
	if (m_inputFlags & EInputFlag::MoveBack)    velocity.y -= moveSpeed * frameTime;

	m_pCharacterController->AddVelocity(GetEntity()->GetWorldRotation() * velocity);
}

// ---------------------------------------------------------------------------
void CPlayerComponent::UpdateLookDirectionRequest(float frameTime)
{
	// Verbatim CE sample pattern — accumulate yaw+pitch into m_lookOrientation.
	// The orbit camera reads m_orbitYaw/Pitch independently in UpdateCamera,
	// so we also sync m_orbitYaw from the look orientation yaw here to keep
	// movement direction (which uses m_orbitYaw) in sync with entity facing.

	m_mouseDeltaRotation = m_mouseDeltaSmoothingFilter.Push(m_mouseDeltaRotation).Get();

	m_horizontalAngularVelocity = (m_mouseDeltaRotation.x * m_orbitYawSpeed) / frameTime;
	m_averagedHorizontalAngularVelocity.Push(m_horizontalAngularVelocity);

	Ang3 ypr = CCamera::CreateAnglesYPR(Matrix33(m_lookOrientation));

	// Yaw — also update m_orbitYaw so camera and movement stay in sync
	ypr.x        += m_mouseDeltaRotation.x * m_orbitYawSpeed;
	m_orbitYaw    = ypr.x;

	// Pitch — clamp to configured limits
	ypr.y         = CLAMP(ypr.y + m_mouseDeltaRotation.y * m_orbitPitchSpeed, m_minOrbitPitch, m_maxOrbitPitch);
	m_orbitPitch  = ypr.y;

	ypr.z = 0;
	m_lookOrientation = Quat(CCamera::CreateOrientationYPR(ypr));

	m_mouseDeltaRotation = ZERO;
}

// ---------------------------------------------------------------------------
void CPlayerComponent::UpdateAnimation(float frameTime)
{
	// Verbatim CE sample pattern for tags, motion params, fragment switching
	// and entity rotation. This is the correct way to drive Mannequin in CE 5.7.

	const float angularVelocityTurningThreshold = 0.174f; // rad/s

	const bool isTurning = std::abs(m_averagedHorizontalAngularVelocity.Get()) > angularVelocityTurningThreshold;
	m_pAnimationComponent->SetTagWithId(m_rotateTagId, isTurning);
	if (isTurning)
	{
		const float turnDuration = 1.0f;
		m_pAnimationComponent->SetMotionParameter(eMotionParamID_TurnAngle, m_horizontalAngularVelocity * turnDuration);
	}

	// Switch fragment based on IsWalking() — CE drives this from physics velocity
	const FragmentID desiredFragmentId = m_pCharacterController->IsWalking() ? m_walkFragmentId : m_idleFragmentId;
	if (m_activeFragmentId != desiredFragmentId)
	{
		m_activeFragmentId = desiredFragmentId;
		m_pAnimationComponent->QueueFragmentWithId(m_activeFragmentId);
	}

	// Set TravelSpeed and TravelAngle for the strafe blend space
	if (m_pCharacterController->IsWalking())
	{
		m_pAnimationComponent->SetMotionParameter(eMotionParamID_TravelSpeed, m_walkSpeed);
		m_pAnimationComponent->SetMotionParameter(eMotionParamID_TravelAngle, 0.f); // always forward — entity faces movement dir
	}

	// Rotate entity to face camera forward (yaw only, zero pitch/roll).
	// Same as CE sample — we extract yaw from m_lookOrientation which is
	// kept in sync with m_orbitYaw in UpdateLookDirectionRequest.
	Ang3 ypr = CCamera::CreateAnglesYPR(Matrix33(m_lookOrientation));
	ypr.y = 0;
	ypr.z = 0;
	const Quat correctedOrientation = Quat(CCamera::CreateOrientationYPR(ypr));
	GetEntity()->SetPosRotScale(GetEntity()->GetWorldPos(), correctedOrientation, Vec3(1, 1, 1));
}

// ---------------------------------------------------------------------------
void CPlayerComponent::UpdateCamera(float frameTime)
{
	// Orbit camera — fully independent of entity rotation.
	// m_orbitYaw and m_orbitPitch are kept in sync with m_lookOrientation
	// in UpdateLookDirectionRequest so mouse input drives both correctly.

	// 1. World-space pivot at character chest height
	const Vec3 playerWorldPos = GetEntity()->GetWorldPos();
	const Vec3 pivotWorldPos  = playerWorldPos + Vec3(0.f, 0.f, 1.5f);

	// 2. Desired orbit offset from pivot
	const float cosPitch = cosf(m_orbitPitch);
	const float sinPitch = sinf(m_orbitPitch);
	const float cosYaw   = cosf(m_orbitYaw);
	const float sinYaw   = sinf(m_orbitYaw);

	Vec3 desiredOffset;
	desiredOffset.x =  m_orbitDistance * cosPitch * sinYaw;
	desiredOffset.y = -m_orbitDistance * cosPitch * cosYaw;
	desiredOffset.z =  m_orbitDistance * sinPitch;

	// 3. Smooth lag
	if (!m_cameraInitialized)
	{
		m_cameraOffset      = desiredOffset;
		m_cameraInitialized = true;
	}
	else
	{
		const float t  = 1.f - expf(-m_cameraLagSpeed * frameTime);
		m_cameraOffset = Vec3::CreateLerp(m_cameraOffset, desiredOffset, t);
	}

	// 4. World-space camera position
	const Vec3 cameraWorldPos = pivotWorldPos + m_cameraOffset;

	// 5. Look-at in world space
	const Vec3 lookDir   = (pivotWorldPos - cameraWorldPos).GetNormalized();
	const Quat cameraRot = Quat::CreateRotationVDir(lookDir);

	// 6. Convert to entity-local space for SetTransformMatrix
	const Quat  entityWorldRot = GetEntity()->GetWorldRotation();
	const Vec3  cameraLocalPos = entityWorldRot.GetInverted() * (cameraWorldPos - playerWorldPos);
	const Quat  cameraLocalRot = entityWorldRot.GetInverted() * cameraRot;

	const Matrix34 localTransform = Matrix34::Create(Vec3(1.f), cameraLocalRot, cameraLocalPos);
	m_pCameraComponent->SetTransformMatrix(localTransform);

	// 7. Smooth FOV
	{
		const float t = 1.f - expf(-m_fovLagSpeed * frameTime);
		m_currentFOV  = m_currentFOV + (m_targetFOV - m_currentFOV) * t;
		m_pCameraComponent->SetFieldOfView(CryTransform::CAngle::FromRadians(m_currentFOV));

		// TEMP DEBUG — remove once camera feels correct
		m_debugLogTimer += frameTime;
		if (m_debugLogTimer >= 1.0f)
		{
			m_debugLogTimer = 0.f;
			CryLog("[Camera] SetFieldOfView = %.2f deg  (%.4f rad)", RAD2DEG(m_currentFOV), m_currentFOV);
			CryLog("[Camera] GetFieldOfView = %.2f deg", m_pCameraComponent->GetFieldOfView().ToDegrees());
			const Vec3 lp = m_pCameraComponent->GetTransformMatrix().GetTranslation();
			CryLog("[Camera] Local slot pos = (%.2f, %.2f, %.2f)", lp.x, lp.y, lp.z);
		}
	}

	// 8. Audio listener in entity-local space
	m_pAudioListenerComponent->SetOffset(cameraLocalPos);
}

// ---------------------------------------------------------------------------
void CPlayerComponent::OnReadyForGameplayOnServer()
{
	CRY_ASSERT(gEnv->bServer, "This function should only be called on the server!");

	const Matrix34 newTransform = CSpawnPointComponent::GetFirstSpawnPointTransform();
	Revive(newTransform);

	SRmi<RMI_WRAP(&CPlayerComponent::RemoteReviveOnClient)>::InvokeOnOtherClients(this,
		RemoteReviveParams{ newTransform.GetTranslation(), Quat(newTransform) });

	const int channelId = m_pEntity->GetNetEntity()->GetChannelId();
	CGamePlugin::GetInstance()->IterateOverPlayers<CPlayerComponent>(std::function<void(CPlayerComponent&)>([this, channelId](CPlayerComponent& player)
	{
		if (player.GetEntityId() == GetEntityId()) return;
		if (!player.m_isAlive) return;

		const QuatT currentOrientation = QuatT(player.GetEntity()->GetWorldTM());
		SRmi<RMI_WRAP(&CPlayerComponent::RemoteReviveOnClient)>::InvokeOnClient(
			&player,
			RemoteReviveParams{ currentOrientation.t, currentOrientation.q },
			channelId);
	}));
}

// ---------------------------------------------------------------------------
bool CPlayerComponent::RemoteReviveOnClient(RemoteReviveParams&& params, INetChannel* pNetChannel)
{
	Revive(Matrix34::Create(Vec3(1.f), params.rotation, params.position));
	return true;
}

// ---------------------------------------------------------------------------
void CPlayerComponent::Revive(const Matrix34& transform)
{
	m_isAlive = true;

	if (!gEnv->IsEditor())
		m_pEntity->SetWorldTM(transform);

	m_pAnimationComponent->ResetCharacter();
	m_pCharacterController->Physicalize();

	m_inputFlags.Clear();
	NetMarkAspectsDirty(InputAspect);

	m_mouseDeltaRotation = ZERO;
	m_mouseDeltaSmoothingFilter.Reset();

	m_activeFragmentId = FRAGMENT_ID_INVALID;

	m_lookOrientation = IDENTITY;
	m_horizontalAngularVelocity = 0.f;
	m_averagedHorizontalAngularVelocity.Reset();

	// Reset orbit camera
	m_orbitYaw          = 0.f;
	m_orbitPitch        = 0.3f;
	m_cameraInitialized = false;
}

// ---------------------------------------------------------------------------
void CPlayerComponent::HandleInputFlagChange(const CEnumFlags<EInputFlag> flags, const CEnumFlags<EActionActivationMode> activationMode, const EInputFlagType type)
{
	switch (type)
	{
	case EInputFlagType::Hold:
		if (activationMode == eAAM_OnRelease) m_inputFlags &= ~flags;
		else                                  m_inputFlags |=  flags;
		break;

	case EInputFlagType::Toggle:
		if (activationMode == eAAM_OnRelease) m_inputFlags ^= flags;
		break;
	}

	if (IsLocalClient())
		NetMarkAspectsDirty(InputAspect);
}