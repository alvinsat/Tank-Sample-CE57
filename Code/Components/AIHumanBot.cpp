// Copyright 2025-2026 Softleafgame Alvinsat. All rights reserved.

#include "StdAfx.h"
#include "AIHumanBot.h"

#include <CryCore/StaticInstanceList.h>
#include <CrySchematyc/Env/IEnvRegistrar.h>
#include <CrySchematyc/Env/Elements/EnvComponent.h>

namespace
{
	static void RegisterAIComponent(Schematyc::IEnvRegistrar& registrar)
	{
		Schematyc::CEnvRegistrationScope scope = registrar.Scope(IEntity::GetEntityScopeGUID());
		{
			Schematyc::CEnvRegistrationScope componentScope = scope.Register(SCHEMATYC_MAKE_ENV_COMPONENT(CAIHumanBotComponent));
		}
	}

	CRY_STATIC_AUTO_REGISTER_FUNCTION(&RegisterAIComponent);
}

void CAIHumanBotComponent::ReflectType(Schematyc::CTypeDesc<CAIHumanBotComponent>& desc)
{
	desc.SetGUID("{F5B04EAF-5B1E-4D99-A9A8-570A2C4B4C8F}"_cry_guid);
	desc.SetEditorCategory("AI");
	desc.SetLabel("AIHumanBot");
	desc.SetDescription("Character controller driven by callable movement input instead of player input.");
	desc.SetComponentFlags({ IEntityComponent::EFlags::Transform });

	desc.AddMember(&CAIHumanBotComponent::m_moveSpeed, 'mspd', "MoveSpeed", "Move Speed", "Character movement speed", 20.5f);
	desc.AddMember(&CAIHumanBotComponent::m_walkSpeed, 'wspd', "WalkSpeed", "Walk Speed", "Animation travel speed parameter", 1.4f);
}

void CAIHumanBotComponent::Initialize()
{
	m_pCharacterController = m_pEntity->GetOrCreateComponent<Cry::DefaultComponents::CCharacterControllerComponent>();
	m_pCharacterController->SetTransformMatrix(Matrix34::Create(Vec3(1.f), IDENTITY, Vec3(0, 0, 1.f)));

	// 1. Get the pointer to the entity
	IEntity *pEntity = GetEntity();

	// 2. Add the trigger area flag
	// This tells the engine: "This entity should trigger Area events"
	pEntity->AddFlags(ENTITY_FLAG_TRIGGER_AREAS);

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
	m_rotateTagId = m_pAnimationComponent->GetTagId("Rotate");

	ResetState();
}

Cry::Entity::EventFlags CAIHumanBotComponent::GetEventMask() const
{
	return Cry::Entity::EEvent::Update | Cry::Entity::EEvent::Reset;
}

void CAIHumanBotComponent::ProcessEvent(const SEntityEvent& event)
{
	switch (event.event)
	{
	case Cry::Entity::EEvent::Update:
	{
		const float frameTime = event.fParam[0];
		UpdateMovementRequest(frameTime);
		UpdateLookDirectionRequest(frameTime);
		UpdateAnimation(frameTime);
	}
	break;

	case Cry::Entity::EEvent::Reset:
		ResetState();
		break;
	}
}

void CAIHumanBotComponent::SetMoveInput(const Vec2& moveInput)
{
	m_moveInput.x = CLAMP(moveInput.x, -1.0f, 1.0f);
	m_moveInput.y = CLAMP(moveInput.y, -1.0f, 1.0f);
}

void CAIHumanBotComponent::SetLookOrientation(const Quat& orientation)
{
	m_lookOrientation = orientation.GetNormalized();
}

void CAIHumanBotComponent::StopMovement()
{
	m_moveInput = ZERO;
}

void CAIHumanBotComponent::ResetState()
{
	m_moveInput = ZERO;
	m_horizontalAngularVelocity = 0.0f;
	m_averagedHorizontalAngularVelocity.Reset();
	m_lookOrientation = GetEntity()->GetWorldRotation();

	// Start in the default idle fragment even if no movement input is ever sent.
	m_activeFragmentId = m_idleFragmentId;
	if (m_pAnimationComponent != nullptr && m_activeFragmentId != FRAGMENT_ID_INVALID)
	{
		m_pAnimationComponent->QueueFragmentWithId(m_activeFragmentId);
	}
}

void CAIHumanBotComponent::UpdateMovementRequest(float frameTime)
{
	if (!m_pCharacterController->IsOnGround())
	{
		return;
	}

	Vec3 velocity = ZERO;

	if (m_moveInput.x != 0.0f)
	{
		velocity.x += m_moveInput.x * m_moveSpeed * frameTime;
	}

	if (m_moveInput.y != 0.0f)
	{
		velocity.y += m_moveInput.y * m_moveSpeed * frameTime;
	}

	m_pCharacterController->AddVelocity(GetEntity()->GetWorldRotation() * velocity);
}

void CAIHumanBotComponent::UpdateLookDirectionRequest(float frameTime)
{
	if (frameTime <= 0.0f)
	{
		m_horizontalAngularVelocity = 0.0f;
		m_averagedHorizontalAngularVelocity.Push(0.0f);
		return;
	}

	Ang3 currentYpr = CCamera::CreateAnglesYPR(Matrix33(GetEntity()->GetWorldRotation()));
	Ang3 targetYpr = CCamera::CreateAnglesYPR(Matrix33(m_lookOrientation));
	targetYpr.y = 0.0f;
	targetYpr.z = 0.0f;

	float deltaYaw = targetYpr.x - currentYpr.x;
	while (deltaYaw > gf_PI)
	{
		deltaYaw -= gf_PI2;
	}
	while (deltaYaw < -gf_PI)
	{
		deltaYaw += gf_PI2;
	}

	m_horizontalAngularVelocity = deltaYaw / frameTime;
	m_averagedHorizontalAngularVelocity.Push(m_horizontalAngularVelocity);
}

void CAIHumanBotComponent::UpdateAnimation(float frameTime)
{
	(void)frameTime;

	const float angularVelocityTurningThreshold = 0.174f;
	const bool isTurning = std::abs(m_averagedHorizontalAngularVelocity.Get()) > angularVelocityTurningThreshold;
	m_pAnimationComponent->SetTagWithId(m_rotateTagId, isTurning);
	if (isTurning)
	{
		const float turnDuration = 1.0f;
		m_pAnimationComponent->SetMotionParameter(eMotionParamID_TurnAngle, m_horizontalAngularVelocity * turnDuration);
	}

	const FragmentID desiredFragmentId = m_pCharacterController->IsWalking() ? m_walkFragmentId : m_idleFragmentId;
	if (m_activeFragmentId != desiredFragmentId)
	{
		m_activeFragmentId = desiredFragmentId;
		m_pAnimationComponent->QueueFragmentWithId(m_activeFragmentId);
	}

	if (m_pCharacterController->IsWalking())
	{
		m_pAnimationComponent->SetMotionParameter(eMotionParamID_TravelSpeed, m_walkSpeed);
		m_pAnimationComponent->SetMotionParameter(eMotionParamID_TravelAngle, 0.0f);
	}

	Ang3 ypr = CCamera::CreateAnglesYPR(Matrix33(m_lookOrientation));
	ypr.y = 0.0f;
	ypr.z = 0.0f;
	const Quat correctedOrientation = Quat(CCamera::CreateOrientationYPR(ypr));
	GetEntity()->SetPosRotScale(GetEntity()->GetWorldPos(), correctedOrientation, Vec3(1.0f, 1.0f, 1.0f));
}
