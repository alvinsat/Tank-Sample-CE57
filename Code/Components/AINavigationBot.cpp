// Copyright 2025-2026 Softleafgame Alvinsat. All rights reserved.

#include "StdAfx.h"
#include "AINavigationBot.h"
#include "AIHumanBot.h"

#include <CryCore/StaticInstanceList.h>
#include <CryCore/functor.h>
#include <CrySchematyc/Env/IEnvRegistrar.h>
#include <CrySchematyc/Env/Elements/EnvComponent.h>

namespace
{
	static void RegisterAINavigationBotComponent(Schematyc::IEnvRegistrar& registrar)
	{
		Schematyc::CEnvRegistrationScope scope = registrar.Scope(IEntity::GetEntityScopeGUID());
		{
			Schematyc::CEnvRegistrationScope componentScope = scope.Register(SCHEMATYC_MAKE_ENV_COMPONENT(CAINavigationBotComponent));
		}
	}

	CRY_STATIC_AUTO_REGISTER_FUNCTION(&RegisterAINavigationBotComponent);
}

CAINavigationBotComponent::~CAINavigationBotComponent()
{
	CancelPathRequest();

	if (gEnv != nullptr && gEnv->pAISystem != nullptr && m_pAIObject != nullptr)
	{
		gEnv->pAISystem->GetAIObjectManager()->RemoveObject(m_pAIObject->GetAIObjectID());
		m_pAIObject = nullptr;
	}
}

void CAINavigationBotComponent::ReflectType(Schematyc::CTypeDesc<CAINavigationBotComponent>& desc)
{
	desc.SetGUID("{81B1ED4F-6A4F-42D2-9DBD-62179F344F11}"_cry_guid);
	desc.SetEditorCategory("AI");
	desc.SetLabel("AINavigationBot");
	desc.SetDescription("Bridges navigation pathfinding to AIHumanBot movement and facing.");
	desc.SetComponentFlags({ IEntityComponent::EFlags::Transform });

	desc.AddMember(&CAINavigationBotComponent::m_stoppingDistance, 'stop', "StoppingDistance", "Stopping Distance", "Distance from the destination where movement stops", 0.5f);
	desc.AddMember(&CAINavigationBotComponent::m_waypointTolerance, 'way', "WaypointTolerance", "Waypoint Tolerance", "Distance used to advance to the next waypoint", 0.3f);
	desc.AddMember(&CAINavigationBotComponent::m_agentTypeName, 'agnt', "AgentType", "Agent Type", "Navigation agent type name from Navigation.xml", Schematyc::CSharedString("MediumSizedCharacters"));
}

void CAINavigationBotComponent::Initialize()
{
	m_pHumanBot = GetEntity()->GetComponent<CAIHumanBotComponent>();
	if (m_pHumanBot == nullptr)
	{
		CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_WARNING, "[AINavigationBot] Entity '%s' is missing CAIHumanBotComponent.", GetEntity()->GetName());
	}

	if (gEnv == nullptr || gEnv->pAISystem == nullptr)
	{
		return;
	}

	AIObjectParams aiObjectParams(0, nullptr, GetEntity()->GetId());
	m_pAIObject = gEnv->pAISystem->GetAIObjectManager()->CreateAIObject(aiObjectParams);
	SyncAIObject(GetEntity()->GetWorldPos(), FORWARD_DIRECTION);
}

Cry::Entity::EventFlags CAINavigationBotComponent::GetEventMask() const
{
	return Cry::Entity::EEvent::Update | Cry::Entity::EEvent::Reset;
}

void CAINavigationBotComponent::ProcessEvent(const SEntityEvent& event)
{
	switch (event.event)
	{
	case Cry::Entity::EEvent::Update:
		Update(event.fParam[0]);
		break;

	case Cry::Entity::EEvent::Reset:
		StopNavigation();
		break;
	}
}

void CAINavigationBotComponent::SetDestination(const Vec3& pos)
{
	m_destination = pos;
	RequestPath();
}

void CAINavigationBotComponent::StopNavigation()
{
	CancelPathRequest();
	ClearPath();

	if (m_pHumanBot != nullptr)
	{
		m_pHumanBot->StopMovement();
	}
}

void CAINavigationBotComponent::Update(float deltaTime)
{
	FollowPath(deltaTime);
}

void CAINavigationBotComponent::RequestPath()
{
	CancelPathRequest();
	ClearPath();

	if (gEnv == nullptr || gEnv->pAISystem == nullptr)
	{
		return;
	}

	IMNMPathfinder* pPathfinder = gEnv->pAISystem->GetMNMPathfinder();
	INavigationSystem* pNavigationSystem = gEnv->pAISystem->GetNavigationSystem();
	if (pPathfinder == nullptr || pNavigationSystem == nullptr)
	{
		return;
	}

	const NavigationAgentTypeID agentTypeId = pNavigationSystem->GetAgentTypeID(m_agentTypeName.c_str());
	if (agentTypeId == NavigationAgentTypeID())
	{
		CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_WARNING, "[AINavigationBot] Unknown agent type '%s'.", m_agentTypeName.c_str());
		return;
	}

	MNMPathRequest request;
	request.startLocation = GetEntity()->GetWorldPos();
	request.endLocation = m_destination;
	request.endDirection = (m_destination - request.startLocation).GetNormalizedSafe(FORWARD_DIRECTION);
	request.agentTypeID = agentTypeId;
	request.requesterEntityId = GetEntity()->GetId();
	request.resultCallback = functor(*this, &CAINavigationBotComponent::OnPathRequestResult);

	m_currentPathRequestId = pPathfinder->RequestPathTo(GetEntity()->GetId(), request);
}

void CAINavigationBotComponent::CancelPathRequest()
{
	if (m_currentPathRequestId == MNM::Constants::eQueuedPathID_InvalidID)
	{
		return;
	}

	if (gEnv != nullptr && gEnv->pAISystem != nullptr)
	{
		if (IMNMPathfinder* pPathfinder = gEnv->pAISystem->GetMNMPathfinder())
		{
			pPathfinder->CancelPathRequest(m_currentPathRequestId);
		}
	}

	m_currentPathRequestId = MNM::Constants::eQueuedPathID_InvalidID;
}

void CAINavigationBotComponent::ClearPath()
{
	m_pathPoints.clear();
	m_currentPathIndex = 0;
	m_hasPath = false;
}

void CAINavigationBotComponent::FollowPath(float deltaTime)
{
	if (deltaTime <= 0.0f || m_pHumanBot == nullptr)
	{
		if (m_pHumanBot != nullptr)
		{
			m_pHumanBot->StopMovement();
		}
		return;
	}

	if (!m_hasPath || m_currentPathIndex >= m_pathPoints.size())
	{
		m_pHumanBot->StopMovement();
		SyncAIObject(GetEntity()->GetWorldPos(), FORWARD_DIRECTION);
		return;
	}

	Cry::DefaultComponents::CCharacterControllerComponent* pCharacterController = m_pHumanBot->GetCharacterController();
	if (pCharacterController == nullptr || !pCharacterController->IsOnGround())
	{
		m_pHumanBot->StopMovement();
		SyncAIObject(GetEntity()->GetWorldPos(), FORWARD_DIRECTION);
		return;
	}

	const Vec3 currentPos = GetEntity()->GetWorldPos();
	if (currentPos.GetDistance(m_destination) <= m_stoppingDistance)
	{
		m_pHumanBot->StopMovement();
		ClearPath();
		SyncAIObject(currentPos, FORWARD_DIRECTION);
		return;
	}

	Vec3 targetPos = m_pathPoints[m_currentPathIndex];
	if (currentPos.GetDistance(targetPos) <= m_waypointTolerance)
	{
		++m_currentPathIndex;
		if (m_currentPathIndex >= m_pathPoints.size())
		{
			m_pHumanBot->StopMovement();
			ClearPath();
			SyncAIObject(currentPos, FORWARD_DIRECTION);
			return;
		}

		targetPos = m_pathPoints[m_currentPathIndex];
	}

	const Vec3 direction = (targetPos - currentPos).GetNormalizedSafe(FORWARD_DIRECTION);
	if (direction.IsZero())
	{
		m_pHumanBot->StopMovement();
		SyncAIObject(currentPos, FORWARD_DIRECTION);
		return;
	}

	m_pHumanBot->SetLookOrientation(Quat::CreateRotationVDir(direction));
	m_pHumanBot->SetMoveInput(Vec2(0.0f, 1.0f));
	SyncAIObject(currentPos, direction);
}

void CAINavigationBotComponent::OnPathRequestResult(const MNM::QueuedPathID& requestId, MNMPathRequestResult& result)
{
	if (requestId != m_currentPathRequestId)
	{
		return;
	}

	m_currentPathRequestId = MNM::Constants::eQueuedPathID_InvalidID;
	ClearPath();

	if (!result.HasPathBeenFound() || result.pPath == nullptr)
	{
		if (m_pHumanBot != nullptr)
		{
			m_pHumanBot->StopMovement();
		}
		return;
	}

	for (const PathPointDescriptor& point : result.pPath->GetPath())
	{
		m_pathPoints.push_back(point.vPos);
	}

	if (!m_pathPoints.empty())
	{
		m_hasPath = true;
	}
}

void CAINavigationBotComponent::SyncAIObject(const Vec3& position, const Vec3& direction) const
{
	if (m_pAIObject != nullptr)
	{
		m_pAIObject->SetPos(position, direction);
	}
}
