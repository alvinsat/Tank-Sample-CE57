// Copyright 2016-2020 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "TemplateComponent.h"

#include <CryCore/StaticInstanceList.h>
#include <CrySchematyc/Env/Elements/EnvComponent.h>

namespace
{
	static void RegisterTemplateComponent(Schematyc::IEnvRegistrar& registrar)
	{
		Schematyc::CEnvRegistrationScope scope = registrar.Scope(IEntity::GetEntityScopeGUID());
		{
			Schematyc::CEnvRegistrationScope componentScope = scope.Register(SCHEMATYC_MAKE_ENV_COMPONENT(CTemplateComponent));
		}
	}

	CRY_STATIC_AUTO_REGISTER_FUNCTION(&RegisterTemplateComponent);
}

void CTemplateComponent::ReflectType(Schematyc::CTypeDesc<CTemplateComponent>& desc)
{
	desc.SetGUID("{7D5A7C5D-6167-4B38-8E39-7DF1E6C27C11}"_cry_guid);
	desc.SetEditorCategory("Game");
	desc.SetLabel("Template Component");
	desc.SetDescription("Basic entity component template.");
	desc.SetComponentFlags({ IEntityComponent::EFlags::Transform });
}

void CTemplateComponent::Initialize()
{
}

Cry::Entity::EventFlags CTemplateComponent::GetEventMask() const
{
	return {};
}

void CTemplateComponent::ProcessEvent(const SEntityEvent& event)
{
	(void)event;
}
