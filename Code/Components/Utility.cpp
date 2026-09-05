// Copyright 2016-2020 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Utility.h"

#include <CryCore/StaticInstanceList.h>
#include <CrySchematyc/Env/Elements/EnvComponent.h>
#include <CrySchematyc/Env/Elements/EnvFunction.h>
#include <CrySchematyc/Env/Elements/EnvSignal.h>

namespace
{
    static void RegisterUtilityComponent(Schematyc::IEnvRegistrar &registrar)
    {
        Schematyc::CEnvRegistrationScope scope = registrar.Scope(IEntity::GetEntityScopeGUID());
        {
            Schematyc::CEnvRegistrationScope componentScope = scope.Register(SCHEMATYC_MAKE_ENV_COMPONENT(CUtilityComponent));

            {
                auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CUtilityComponent::FunctionIsStringParam, "{2149481a-2e65-4a66-b7be-11fe3da27970}"_cry_guid, "FunctionIsStringParam");
                pFunction->SetDescription("Processes a message string from Schematyc");
                pFunction->BindInput(1, 'msg', "Message");
                componentScope.Register(pFunction);
            }
            
            // Sample: Function with primitive type inputs
            {
                auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CUtilityComponent::FunctionWithPrimitiveInput, "{3a4b5c6d-7e8f-4a5b-8c9d-2e3f4a5b6c7d}"_cry_guid, "FunctionWithPrimitiveInput");
                pFunction->SetDescription("Receives primitive type inputs");
                pFunction->BindInput(1, 'val', "Value");
                pFunction->BindInput(2, 'rat', "Ratio");
                pFunction->BindInput(3, 'ena', "Enabled");
                componentScope.Register(pFunction);
            }
            
            // Sample: Function with entity type input
            {
                auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CUtilityComponent::FunctionWithEntityInput, "{4b5c6d7e-8f9a-4b5c-8d9e-3f4a5b6c7d8e}"_cry_guid, "FunctionWithEntityInput");
                pFunction->SetDescription("Receives an entity reference");
                pFunction->BindInput(1, 'ent', "Target Entity");
                componentScope.Register(pFunction);
            }
            
            // Sample: Function with return value
            {
                auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CUtilityComponent::FunctionWithReturnValue, "{5c6d7e8f-9aab-4c5d-8e9f-4a5b6c7d8e9f}"_cry_guid, "FunctionWithReturnValue");
                pFunction->SetDescription("Returns computed integer value");
                pFunction->BindInput(1, 'a', "A");
                pFunction->BindInput(2, 'b', "B");
                pFunction->BindOutput(0, 'res', "Result");
                componentScope.Register(pFunction);
            }
            
            // Sample: Function with in/out parameter (now takes input and returns output)
            {
                auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CUtilityComponent::FunctionWithInOutParam, "{6d7e8f9a-abbc-4d5e-8f9a-5b6c7d8e9fa0}"_cry_guid, "FunctionWithInOutParam");
                pFunction->SetDescription("Takes a float value and returns the doubled value");
                pFunction->BindInput(1, 'val', "Value");
                pFunction->BindOutput(0, 'res', "Result");
                componentScope.Register(pFunction);
            }
            
            // Sample: Function with all primitive types
            {
                auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CUtilityComponent::FunctionWithAllPrimitives, "{7e8f9aab-bccd-4e5f-9fa0-6b7c8d9e0fa1}"_cry_guid, "FunctionWithAllPrimitives");
                pFunction->SetDescription("Demonstrates all primitive types as inputs");
                pFunction->BindInput(1, 'i32', "Int32");
                pFunction->BindInput(2, 'u32', "UInt32");
                pFunction->BindInput(3, 'i64', "Int64");
                pFunction->BindInput(4, 'u64', "UInt64");
                pFunction->BindInput(5, 'flt', "Float");
                pFunction->BindInput(6, 'dbl', "Double");
                pFunction->BindInput(7, 'bol', "Bool");
                componentScope.Register(pFunction);
            }
            
            // Sample: Function with math types
            {
                auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CUtilityComponent::FunctionWithMathTypes, "{8f9aabcc-ddee-4f6a-0fb1-7c8d9e0fa1b2}"_cry_guid, "FunctionWithMathTypes");
                pFunction->SetDescription("Demonstrates math types (vectors and colors)");
                pFunction->BindInput(1, 'v2', "Vector2");
                pFunction->BindInput(2, 'v3', "Vector3");
                pFunction->BindInput(3, 'cf', "ColorF");
                pFunction->BindInput(4, 'cb', "ColorB");
                componentScope.Register(pFunction);
            }
            
            // Sample: Function with string and entity
            {
                auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CUtilityComponent::FunctionWithStringAndEntity, "{9aabccdd-eeff-4a7b-1fc2-8d9e0fa1b2c3}"_cry_guid, "FunctionWithStringAndEntity");
                pFunction->SetDescription("Demonstrates string and entity reference types");
                pFunction->BindInput(1, 'msg', "Message");
                pFunction->BindInput(2, 'ent', "Entity");
                componentScope.Register(pFunction);
            }
            
            // Sample: Function with arrays
            {
                auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CUtilityComponent::FunctionWithArrays, "{aabbccdd-ffaa-4b8c-2fd3-9e0fa1b2c3d4}"_cry_guid, "FunctionWithArrays");
                pFunction->SetDescription("Demonstrates array types");
                pFunction->BindInput(1, 'iarr', "IntArray");
                pFunction->BindInput(2, 'farr', "FloatArray");
                componentScope.Register(pFunction);
            }
            
            // Sample: Function returning custom status struct
            {
                auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CUtilityComponent::GetStatusInfo, "{bbccddee-aabb-4c9d-3fe4-0fa1b2c3d4e5}"_cry_guid, "GetStatusInfo");
                pFunction->SetDescription("Returns status information as a custom struct");
                pFunction->BindOutput(0, 'stat', "Status");
                componentScope.Register(pFunction);
            }
        }
    }

    CRY_STATIC_AUTO_REGISTER_FUNCTION(&RegisterUtilityComponent);
}

void CUtilityComponent::ReflectType(Schematyc::CTypeDesc<CUtilityComponent> &desc)
{
    desc.SetGUID("{A1B2C3D4-E5F6-4A5B-8C9D-1E2F3A4B5C6D}"_cry_guid);
    desc.SetEditorCategory("Game");
    desc.SetLabel("Utility Component");
    desc.SetDescription("Basic utility entity component.");
    desc.SetComponentFlags({IEntityComponent::EFlags::Transform});
}

void CUtilityComponent::Initialize()
{
}

void CUtilityComponent::FunctionIsStringParam(Schematyc::CSharedString myMessage)
{
    CryLogAlways("Message: %s", myMessage.c_str());
}

void CUtilityComponent::FunctionWithPrimitiveInput(int value, float ratio, bool enabled)
{
    CryLogAlways("[Utility] Primitive Input - Value: %d, Ratio: %.2f, Enabled: %s", 
                 value, ratio, enabled ? "true" : "false");
}

void CUtilityComponent::FunctionWithEntityInput(EntityId targetEntity)
{
    if (IEntity* pEntity = gEnv->pEntitySystem->GetEntity(targetEntity))
    {
        CryLogAlways("[Utility] Entity Input - Target: %s (ID: %u)", pEntity->GetName(), targetEntity);
    }
    else
    {
        CryLogAlways("[Utility] Entity Input - No valid entity");
    }
}

int CUtilityComponent::FunctionWithReturnValue(int a, int b)
{
    int result = a + b;
    CryLogAlways("[Utility] Return Value - %d + %d = %d", a, b, result);
    return result;
}

float CUtilityComponent::FunctionWithInOutParam(float value)
{
    float result = value * 2.0f;  // Double the value
    CryLogAlways("[Utility] In/Out Param - %.2f -> %.2f", value, result);
    return result;
}

void CUtilityComponent::FunctionWithAllPrimitives(int32 int32Val, uint32 uint32Val, int64 int64Val, uint64 uint64Val, float floatVal, double doubleVal, bool boolVal)
{
    CryLogAlways("[Utility] All Primitives - int32:%" PRIi32 ", uint32:%" PRIu32 ", int64:%" PRIi64 ", uint64:%" PRIu64 ", float:%.2f, double:%.2f, bool:%s",
                 int32Val, uint32Val, int64Val, uint64Val, floatVal, doubleVal, boolVal ? "true" : "false");
}

void CUtilityComponent::FunctionWithMathTypes(Vec2 vec2Val, Vec3 vec3Val, ColorF colorFVal, ColorB colorBVal)
{
    CryLogAlways("[Utility] Math Types - Vec2:(%.2f,%.2f), Vec3:(%.2f,%.2f,%.2f), ColorF:(%.2f,%.2f,%.2f,%.2f), ColorB:(%d,%d,%d,%d)",
                 vec2Val.x, vec2Val.y, vec3Val.x, vec3Val.y, vec3Val.z,
                 colorFVal.r, colorFVal.g, colorFVal.b, colorFVal.a,
                 colorBVal.r, colorBVal.g, colorBVal.b, colorBVal.a);
}

void CUtilityComponent::FunctionWithStringAndEntity(Schematyc::CSharedString message, EntityId entityId)
{
    if (IEntity* pEntity = gEnv->pEntitySystem->GetEntity(entityId))
    {
        CryLogAlways("[Utility] String & Entity - Message:'%s', Entity:'%s' (ID:%u)",
                     message.c_str(), pEntity->GetName(), entityId);
    }
    else
    {
        CryLogAlways("[Utility] String & Entity - Message:'%s', Entity: Invalid (ID:%u)",
                     message.c_str(), entityId);
    }
}

void CUtilityComponent::FunctionWithArrays(Schematyc::CArray<int32> intArray, Schematyc::CArray<float> floatArray)
{
    CryLogAlways("[Utility] Arrays - IntArray size:%u, FloatArray size:%u", 
                 intArray.Size(), floatArray.Size());
    
    // Log first few elements
    for (uint32 i = 0; i < std::min(uint32(3), intArray.Size()); ++i)
    {
        CryLogAlways("[Utility] IntArray[%u]: %" PRIi32, i, intArray.At(i));
    }
    for (uint32 i = 0; i < std::min(uint32(3), floatArray.Size()); ++i)
    {
        CryLogAlways("[Utility] FloatArray[%u]: %.2f", i, floatArray.At(i));
    }
}

::SStatusInfo CUtilityComponent::GetStatusInfo()
{
    SStatusInfo status;
    status.isActive = true;
    status.health = 100;
    status.statusMessage = "Component is functioning normally";
    status.position = GetEntity()->GetWorldPos();
    
    CryLogAlways("[Utility] Status Info - Active:%s, Health:%" PRIi32 ", Message:'%s', Position:(%.2f,%.2f,%.2f)",
                 status.isActive ? "true" : "false", status.health, status.statusMessage.c_str(),
                 status.position.x, status.position.y, status.position.z);
    
    return status;
}

Cry::Entity::EventFlags CUtilityComponent::GetEventMask() const
{
    return {};
}

void CUtilityComponent::ProcessEvent(const SEntityEvent &event)
{
    (void)event;
}
