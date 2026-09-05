// Copyright 2016-2020 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <vector>

#include <CryAISystem/IAISystem.h>
#include <CryAISystem/IAIObjectManager.h>
#include <CryAISystem/IPathfinder.h>
#include <CryAISystem/INavigationSystem.h>
#include <CryEntitySystem/IEntity.h>
#include <CryEntitySystem/IEntityComponent.h>
#include <CrySchematyc/Utils/SharedString.h>

class CNavigationAgentComponent final : public IEntityComponent
{
public:
	CNavigationAgentComponent() = default;
	virtual ~CNavigationAgentComponent() override;

	static void ReflectType(Schematyc::CTypeDesc<CNavigationAgentComponent>& desc);

	virtual void Initialize() override;
	virtual Cry::Entity::EventFlags GetEventMask() const override;
	virtual void ProcessEvent(const SEntityEvent& event) override;

	void SetDestination(const Vec3& pos);

private:
	void Update(float deltaTime);
	void RequestPath();
	void CancelPathRequest();
	void FollowPath(float deltaTime);
	void OnPathRequestResult(const MNM::QueuedPathID& requestId, MNMPathRequestResult& result);

private:
	IAIObject* m_pAIObject = nullptr;
	MNM::QueuedPathID m_currentPathRequestId = MNM::Constants::eQueuedPathID_InvalidID;
	std::vector<Vec3> m_pathPoints;
	size_t m_currentPathIndex = 0;
	bool m_hasPath = false;

	Vec3 m_destination = ZERO;
	float m_moveSpeed = 3.5f;
	float m_stoppingDistance = 0.5f;
	Schematyc::CSharedString m_agentTypeName = "MediumSizedCharacters";
};
