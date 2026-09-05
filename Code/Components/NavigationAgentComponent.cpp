// Copyright 2016-2020 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "NavigationAgentComponent.h"

#include <CryCore/StaticInstanceList.h>
#include <CryCore/functor.h>
#include <CrySchematyc/Env/IEnvRegistrar.h>
#include <CrySchematyc/Env/IEnvRegistry.h>
#include <CrySchematyc/Env/Elements/EnvComponent.h>

namespace
{
	static void RegisterNavigationAgentComponent(Schematyc::IEnvRegistrar& registrar)
	{
		Schematyc::CEnvRegistrationScope scope = registrar.Scope(IEntity::GetEntityScopeGUID());
		{
			Schematyc::CEnvRegistrationScope componentScope = scope.Register(SCHEMATYC_MAKE_ENV_COMPONENT(CNavigationAgentComponent));
		}
	}

	CRY_STATIC_AUTO_REGISTER_FUNCTION(&RegisterNavigationAgentComponent);
}

CNavigationAgentComponent::~CNavigationAgentComponent()
{
	CancelPathRequest();

	if (gEnv != nullptr && gEnv->pAISystem != nullptr && m_pAIObject != nullptr)
	{
		gEnv->pAISystem->GetAIObjectManager()->RemoveObject(m_pAIObject->GetAIObjectID());
		m_pAIObject = nullptr;
	}
}

void CNavigationAgentComponent::ReflectType(Schematyc::CTypeDesc<CNavigationAgentComponent>& desc)
{
	desc.SetGUID("{4BC24FE1-DFF6-44D3-990C-84A5DE149BB8}"_cry_guid);
	desc.SetEditorCategory("AI");
	desc.SetLabel("Navigation Agent");
	desc.SetDescription("Moves an entity along the NavMesh to a destination.");
	desc.SetComponentFlags({ IEntityComponent::EFlags::Transform });

	desc.AddMember(&CNavigationAgentComponent::m_moveSpeed, 'mspd', "MoveSpeed", "Move Speed", "Movement speed in meters per second", 3.5f);
	desc.AddMember(&CNavigationAgentComponent::m_stoppingDistance, 'stop', "StoppingDistance", "Stopping Distance", "Distance from the destination where movement stops", 0.5f);
	desc.AddMember(&CNavigationAgentComponent::m_agentTypeName, 'agnt', "AgentType", "Agent Type", "Navigation agent type name from Navigation.xml", Schematyc::CSharedString("MediumSizedCharacters"));
}

void CNavigationAgentComponent::Initialize()
{
	if (gEnv == nullptr || gEnv->pAISystem == nullptr)
	{
		return;
	}

	AIObjectParams aiObjectParams(0, nullptr, GetEntity()->GetId());
	m_pAIObject = gEnv->pAISystem->GetAIObjectManager()->CreateAIObject(aiObjectParams);
}

Cry::Entity::EventFlags CNavigationAgentComponent::GetEventMask() const
{
	return Cry::Entity::EEvent::Update;
}

void CNavigationAgentComponent::ProcessEvent(const SEntityEvent& event)
{
	if (event.event == Cry::Entity::EEvent::Update)
	{
		Update(event.fParam[0]);
	}
}

void CNavigationAgentComponent::SetDestination(const Vec3& pos)
{
	m_destination = pos;
	RequestPath();
}

void CNavigationAgentComponent::Update(float deltaTime)
{
	FollowPath(deltaTime);
}

void CNavigationAgentComponent::RequestPath()
{
	CancelPathRequest();

	m_pathPoints.clear();
	m_currentPathIndex = 0;
	m_hasPath = false;

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
		CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_WARNING, "[NavigationAgent] Unknown agent type '%s'.", m_agentTypeName.c_str());
		return;
	}

	MNMPathRequest request;
	request.startLocation = GetEntity()->GetWorldPos();
	request.endLocation = m_destination;
	request.endDirection = (m_destination - request.startLocation).GetNormalizedSafe(FORWARD_DIRECTION);
	request.agentTypeID = agentTypeId;
	request.requesterEntityId = GetEntity()->GetId();
	request.resultCallback = functor(*this, &CNavigationAgentComponent::OnPathRequestResult);

	m_currentPathRequestId = pPathfinder->RequestPathTo(GetEntity()->GetId(), request);
}

void CNavigationAgentComponent::CancelPathRequest()
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

void CNavigationAgentComponent::OnPathRequestResult(const MNM::QueuedPathID& requestId, MNMPathRequestResult& result)
{
	if (requestId != m_currentPathRequestId)
	{
		return;
	}

	m_currentPathRequestId = MNM::Constants::eQueuedPathID_InvalidID;
	m_pathPoints.clear();
	m_currentPathIndex = 0;
	m_hasPath = false;

	if (!result.HasPathBeenFound() || result.pPath == nullptr)
	{
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

void CNavigationAgentComponent::FollowPath(float deltaTime)
{
	if (!m_hasPath || m_currentPathIndex >= m_pathPoints.size() || deltaTime <= 0.0f)
	{
		return;
	}

	const Vec3 currentPos = GetEntity()->GetWorldPos();
	if (currentPos.GetDistance(m_destination) <= m_stoppingDistance)
	{
		m_hasPath = false;
		m_pathPoints.clear();
		m_currentPathIndex = 0;
		return;
	}

	Vec3 targetPos = m_pathPoints[m_currentPathIndex];
	if (currentPos.GetDistance(targetPos) <= 0.3f)
	{
		++m_currentPathIndex;
		if (m_currentPathIndex >= m_pathPoints.size())
		{
			m_hasPath = false;
			return;
		}

		targetPos = m_pathPoints[m_currentPathIndex];
	}

	const Vec3 direction = (targetPos - currentPos).GetNormalizedSafe(ZERO);
	if (direction.IsZero())
	{
		return;
	}

	const Vec3 newPos = currentPos + (direction * m_moveSpeed * deltaTime);
	const Quat targetRotation = Quat::CreateRotationVDir(direction);
	GetEntity()->SetPosRotScale(newPos, targetRotation, Vec3(1.0f, 1.0f, 1.0f));

	if (m_pAIObject != nullptr)
	{
		m_pAIObject->SetPos(newPos, direction);
	}
}
