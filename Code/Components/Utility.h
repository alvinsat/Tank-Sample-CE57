// Copyright 2016-2020 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryEntitySystem/IEntityBasicTypes.h>
#include <CrySchematyc/Utils/SharedString.h>
#include <CrySchematyc/Utils/Array.h>

// Status info struct for return type sample
struct SStatusInfo
{
    bool isActive;
    int32 health;
    Schematyc::CSharedString statusMessage;
    Vec3 position;
    
    SStatusInfo() : isActive(false), health(0) {}
};

class CUtilityComponent final : public IEntityComponent
{
public:
	CUtilityComponent() = default;
	virtual ~CUtilityComponent() override = default;

	static void ReflectType(Schematyc::CTypeDesc<CUtilityComponent>& desc);

    void FunctionIsStringParam(Schematyc::CSharedString myMessage);
    
    // Sample: Primitive input types (int, float, bool, etc.)
    void FunctionWithPrimitiveInput(int value, float ratio, bool enabled);
    
    // Sample: Entity input type
    void FunctionWithEntityInput(EntityId targetEntity);
    
    // Sample: Function with return value
    int FunctionWithReturnValue(int a, int b);
    
    // Sample: Function with in/out parameter (modified to work with current engine)
    float FunctionWithInOutParam(float value);

    // Sample: Function with all primitive types
    void FunctionWithAllPrimitives(int32 int32Val, uint32 uint32Val, int64 int64Val, uint64 uint64Val, float floatVal, double doubleVal, bool boolVal);
    
    // Sample: Function with math types
    void FunctionWithMathTypes(Vec2 vec2Val, Vec3 vec3Val, ColorF colorFVal, ColorB colorBVal);
    
    // Sample: Function with string and entity
    void FunctionWithStringAndEntity(Schematyc::CSharedString message, EntityId entityId);
    
    // Sample: Function with array types
    void FunctionWithArrays(Schematyc::CArray<int32> intArray, Schematyc::CArray<float> floatArray);
    
    // Sample: Function returning a custom status struct
    ::SStatusInfo GetStatusInfo();

	// IEntityComponent
	virtual void Initialize() override;
	virtual Cry::Entity::EventFlags GetEventMask() const override;
	virtual void ProcessEvent(const SEntityEvent& event) override;
	// ~IEntityComponent
};

// Reflection for the status info struct
inline void ReflectType(Schematyc::CTypeDesc<SStatusInfo>& desc)
{
    desc.SetGUID("{B8C9D0E1-F2A3-4B5C-8D9E-1F2A3B4C5D6E}"_cry_guid);
    desc.SetLabel("Status Info");
    desc.SetDescription("Status information structure");
    desc.AddMember(&SStatusInfo::isActive, 'actv', "IsActive", "Is Active", "Whether the component is active", false);
    desc.AddMember(&SStatusInfo::health, 'hlth', "Health", "Health", "Health value", 0);
    desc.AddMember(&SStatusInfo::statusMessage, 'msg', "StatusMessage", "Status Message", "Status message", Schematyc::CSharedString(""));
    desc.AddMember(&SStatusInfo::position, 'pos', "Position", "Position", "Current position", Vec3(ZERO));
}
