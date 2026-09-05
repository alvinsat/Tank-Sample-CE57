// Copyright 2016-2020 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryEntitySystem/IEntityComponent.h>
#include <CryEntitySystem/IEntity.h>

class CTemplateComponent final : public IEntityComponent
{
public:
	CTemplateComponent() = default;
	virtual ~CTemplateComponent() override = default;

	static void ReflectType(Schematyc::CTypeDesc<CTemplateComponent>& desc);

	// IEntityComponent
	virtual void Initialize() override;
	virtual Cry::Entity::EventFlags GetEventMask() const override;
	virtual void ProcessEvent(const SEntityEvent& event) override;
	// ~IEntityComponent
};
