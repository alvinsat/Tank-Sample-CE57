// Copyright 2016-2020 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "TankComponent.h"
#include <CryAnimation/ICryAnimation.h>
#include <CryInput/IInput.h>
#include <CryPhysics/physinterface.h>
#include <CryParticleSystem/IParticles.h>
#include <CryString/StringUtils.h>
#include <CryPhysics/IPhysics.h>
#include <CryEntitySystem/IEntitySystem.h>

namespace
{
	static float ClampSign(float value)
	{
		return value > 0.0f ? 1.0f : (value < 0.0f ? -1.0f : 0.0f);
	}

	static float LerpFloat(float current, float target, float rate, float deltaTime)
	{
		return current + (target - current) * min(1.0f, rate * deltaTime);
	}
}

namespace
{
	static void RegisterTankComponent(Schematyc::IEnvRegistrar &registrar)
	{
		Schematyc::CEnvRegistrationScope scope = registrar.Scope(IEntity::GetEntityScopeGUID());
		{
			Schematyc::CEnvRegistrationScope componentScope = scope.Register(SCHEMATYC_MAKE_ENV_COMPONENT(CTankComponent));
		}
	}

	CRY_STATIC_AUTO_REGISTER_FUNCTION(&RegisterTankComponent);
}

void CTankComponent::ReflectType(Schematyc::CTypeDesc<CTankComponent> &desc)
{
	desc.SetGUID("{c5687ded-2e50-4d3f-816e-f510aab326cc}"_cry_guid);
	desc.SetEditorCategory("Game");
	desc.SetLabel("Tank Component");
	desc.SetDescription("Advanced tank component with movement and weapon systems based on Abrams tank specifications.");
	desc.SetComponentFlags({IEntityComponent::EFlags::Transform});

	desc.AddMember(&CTankComponent::m_tankBodyMesh, 'file', "TankBody", "TankBody", "Main tank body CGA model path", "");
	desc.AddMember(&CTankComponent::m_wheelJointNameFilter, 'wjn', "WheelJointFilter", "Wheel Joint Name Filter", "CGA joint names containing this substring (case-insensitive) receive a Wheel Collider component", "wheel");
	desc.AddMember(&CTankComponent::m_wheelRadius, 'wrad', "WheelRadius", "Wheel Radius", "Physics cylinder radius for auto-discovered wheels", 0.45f);
	desc.AddMember(&CTankComponent::m_wheelWidth, 'wwid', "WheelWidth", "Wheel Width", "Physics cylinder width for auto-discovered wheels", 0.25f);
	desc.AddMember(&CTankComponent::m_wheelSuspensionLength, 'wsln', "WheelSuspensionLength", "Wheel Suspension Length", "Relaxed suspension length", 0.5f);
	desc.AddMember(&CTankComponent::m_wheelSuspensionCompressed, 'wscp', "WheelSuspensionCompressed", "Wheel Suspension Compressed", "Initial compressed suspension length", 0.35f);
	desc.AddMember(&CTankComponent::m_wheelMass, 'wmas', "WheelMass", "Wheel Mass", "Mass per wheel collider (kg)", 80.0f);
	desc.AddMember(&CTankComponent::m_wheelDamping, 'wdmp', "WheelDamping", "Wheel Suspension Damping", "Suspension damping 0..1", 0.7f);
	desc.AddMember(&CTankComponent::m_wheelRaycast, 'wray', "WheelRaycast", "Wheel Raycast", "Use raycast suspension tests instead of mesh sweep", false);
	// desc.AddMember(&CTankComponent::m_bodyMeshDestroyed, 'file', "TankBodyDestroyed", "TankBodyDestroyed", "Destroyed tank body CGA model path", "objects/vehicles/abrams/m1a1_abrams_destroyed.cga");

	// Expose editable properties to the editor
	desc.AddMember(&CTankComponent::m_maxSpeed, 'a', "MaxSpeed",
				   "Max Speed", "Maximum movement speed in m/s", 20.0f);
	desc.AddMember(&CTankComponent::m_rotationSpeed, 'b', "RotationSpeed",
				   "Rotation Speed", "Tank rotation speed in degrees per second", 40.0f);
	desc.AddMember(&CTankComponent::m_minTurretPitch, 'c', "MinTurretPitch",
				   "Min Turret Pitch", "Minimum turret pitch angle in degrees", -10.0f);
	desc.AddMember(&CTankComponent::m_maxTurretPitch, 'd', "MaxTurretPitch",
				   "Max Turret Pitch", "Maximum turret pitch angle in degrees", 35.0f);
	desc.AddMember(&CTankComponent::m_mainCannonReloadTime, 'e', "MainCannonReloadTime",
				   "Main Cannon Reload Time", "Reload time for main cannon in seconds", 2.0f);
	desc.AddMember(&CTankComponent::m_coaxialGunReloadTime, 'f', "CoaxialGunReloadTime",
				   "Coaxial Gun Reload Time", "Reload time for coaxial gun in seconds", 0.1f);
	desc.AddMember(&CTankComponent::m_isEngineRunning, 'g', "IsEngineRunning",
				   "Engine Running", "Whether the tank engine is running", false);
	desc.AddMember(&CTankComponent::m_isDestroyed, 'h', "IsDestroyed",
				   "Is Destroyed", "Whether the tank is destroyed", false);

	// Note: Mesh string members are not exposed to Schematyc editor reflection
	// They are available as internal properties that can be set programmatically
	// See TankComponent.h for m_bodyMesh, m_leftTreadMesh, m_rightTreadMesh, etc.

	// ===== HELPER POSITIONS: DRIVER =====
	desc.AddMember(&CTankComponent::m_driverEnterPos, 'i', "DriverEnterPos",
				   "Driver Enter Position", "Position where driver enters the tank", Vec3(-2.5f, -0.5f, 0.5f));
	desc.AddMember(&CTankComponent::m_driverSitPos, 'j', "DriverSitPos",
				   "Driver Sit Position", "Position where driver sits", Vec3(-0.5f, 0.0f, 2.0f));
	desc.AddMember(&CTankComponent::m_driverViewPos, 'k', "DriverViewPos",
				   "Driver View Position", "Driver camera view position", Vec3(-0.45f, 1.75f, 2.35f));

	// ===== HELPER POSITIONS: GUNNER =====
	desc.AddMember(&CTankComponent::m_gunnerEnterPos, 'l', "GunnerEnterPos",
				   "Gunner Enter Position", "Position where gunner enters the tank", Vec3(2.5f, -0.5f, 0.5f));
	desc.AddMember(&CTankComponent::m_gunnerSitPos, 'm', "GunnerSitPos",
				   "Gunner Sit Position", "Position where gunner sits", Vec3(0.45f, -0.65f, 2.0f));
	desc.AddMember(&CTankComponent::m_gunnerViewPos, 'n', "GunnerViewPos",
				   "Gunner View Position", "Gunner camera view position", Vec3(0.45f, -0.65f, 2.85f));

	// ===== HELPER POSITIONS: WEAPONS =====
	desc.AddMember(&CTankComponent::m_cannonStartPos, 'o', "CannonStartPos",
				   "Cannon Start Position", "Cannon projectile start position", Vec3(0.0f, 3.8f, 1.9f));
	desc.AddMember(&CTankComponent::m_cannonOutPos, 'p', "CannonOutPos",
				   "Cannon Out Position", "Cannon exit/muzzle position", Vec3(0.0f, 5.8f, 1.9f));
	desc.AddMember(&CTankComponent::m_coaxOutPos, 'q', "CoaxOutPos",
				   "Coaxial Gun Out Position", "Coaxial machine gun muzzle position", Vec3(0.315f, 2.4f, 2.035f));
	desc.AddMember(&CTankComponent::m_turretPos, 'r', "TurretPos",
				   "Turret Center Position", "Turret rotation center position", Vec3(-0.0013f, 0.1204f, 1.584f));

	// ===== HELPER POSITIONS: EFFECTS =====
	desc.AddMember(&CTankComponent::m_burningPos, 's', "BurningPos",
				   "Burning Position", "Position for burning/fire effects when destroyed", Vec3(0.0f, -1.335f, 1.7f));
	desc.AddMember(&CTankComponent::m_exhaustPos, 't', "ExhaustPos",
				   "Exhaust Position", "Engine exhaust effects position", Vec3(-0.7f, -4.0f, 1.1621f));
	desc.AddMember(&CTankComponent::m_engineSmokeOutPos, 'u', "EngineSmokeOutPos",
				   "Engine Smoke Position", "Engine smoke effects position", Vec3(0.175f, -4.1f, 1.25f));
	desc.AddMember(&CTankComponent::m_centerPos, 'v', "CenterPos",
				   "Center Position", "Tank center of mass position", Vec3(0.0f, -1.0f, 1.1682f));

	// ===== HELPER POSITIONS: LIGHTS =====
	desc.AddMember(&CTankComponent::m_headLightLeftPos, 'w', "HeadLightLeftPos",
				   "Head Light Left Position", "Left headlight position", Vec3(-0.9099f, 3.55f, 1.23f));
	desc.AddMember(&CTankComponent::m_headLightRightPos, 'x', "HeadLightRightPos",
				   "Head Light Right Position", "Right headlight position", Vec3(0.9103f, 3.55f, 1.23f));
	desc.AddMember(&CTankComponent::m_rearLightLeftPos, 'y', "RearLightLeftPos",
				   "Rear Light Left Position", "Left rear brake light position", Vec3(-1.6175f, -4.4f, 1.6125f));
	desc.AddMember(&CTankComponent::m_rearLightRightPos, 'z', "RearLightRightPos",
				   "Rear Light Right Position", "Right rear brake light position", Vec3(1.6175f, -4.4f, 1.6125f));
	desc.AddMember(&CTankComponent::m_reverseLightLeftPos, 'A', "ReverseLightLeftPos",
				   "Reverse Light Left Position", "Left reverse light position", Vec3(-1.6175f, -4.4f, 1.515f));
	desc.AddMember(&CTankComponent::m_reverseLightRightPos, 'B', "ReverseLightRightPos",
				   "Reverse Light Right Position", "Right reverse light position", Vec3(1.6175f, -4.4f, 1.515f));
	desc.AddMember(&CTankComponent::m_boostOriginPos, 'C', "BoostOriginPos",
				   "Boost Origin Position", "Position for boost effect origin", Vec3(0.0f, -4.4f, 0.08f));

	// ===== COMPONENT DAMAGE PROPERTIES =====
	// Hull component
	desc.AddMember(&CTankComponent::m_hullDamageMax, 'D', "HullDamageMax",
				   "Hull Max Damage", "Maximum damage the hull can take", 2500.0f);
	desc.AddMember(&CTankComponent::m_hullCurrentDamage, 'E', "HullCurrentDamage",
				   "Hull Current Damage", "Current accumulated damage on hull", 0.0f);

	// Engine component
	desc.AddMember(&CTankComponent::m_engineDamageMax, 'F', "EngineDamageMax",
				   "Engine Max Damage", "Maximum damage the engine can take", 1000.0f);
	desc.AddMember(&CTankComponent::m_engineCurrentDamage, 'G', "EngineCurrentDamage",
				   "Engine Current Damage", "Current accumulated damage on engine", 0.0f);

	// Turret component
	desc.AddMember(&CTankComponent::m_turretDamageMax, 'H', "TurretDamageMax",
				   "Turret Max Damage", "Maximum damage the turret can take", 700.0f);
	desc.AddMember(&CTankComponent::m_turretCurrentDamage, 'I', "TurretCurrentDamage",
				   "Turret Current Damage", "Current accumulated damage on turret", 0.0f);

	// Tread components
	desc.AddMember(&CTankComponent::m_leftTreadDamageMax, 'J', "LeftTreadDamageMax",
				   "Left Tread Max Damage", "Maximum damage the left tread can take", 250.0f);
	desc.AddMember(&CTankComponent::m_leftTreadCurrentDamage, 'K', "LeftTreadCurrentDamage",
				   "Left Tread Current Damage", "Current accumulated damage on left tread", 0.0f);
	desc.AddMember(&CTankComponent::m_rightTreadDamageMax, 'L', "RightTreadDamageMax",
				   "Right Tread Max Damage", "Maximum damage the right tread can take", 250.0f);
	desc.AddMember(&CTankComponent::m_rightTreadCurrentDamage, 'M', "RightTreadCurrentDamage",
				   "Right Tread Current Damage", "Current accumulated damage on right tread", 0.0f);

	// Light components
	desc.AddMember(&CTankComponent::m_headLightDamageMax, 'N', "HeadLightDamageMax",
				   "Head Light Max Damage", "Maximum damage headlights can take", 10.0f);
	desc.AddMember(&CTankComponent::m_brakeLightDamageMax, 'O', "BrakeLightDamageMax",
				   "Brake Light Max Damage", "Maximum damage brake lights can take", 10.0f);

	// Damage multipliers
	desc.AddMember(&CTankComponent::m_explosionDamageMult, 'P', "ExplosionDamageMult",
				   "Explosion Damage Multiplier", "Damage multiplier for explosion damage", 1.0f);
	desc.AddMember(&CTankComponent::m_bulletDamageMult, 'Q', "BulletDamageMult",
				   "Bullet Damage Multiplier", "Damage multiplier for bullet damage", 0.05f);
	desc.AddMember(&CTankComponent::m_rocketDamageMult, 'R', "RocketDamageMult",
				   "Rocket Damage Multiplier", "Damage multiplier for rocket damage", 1.0f);
	desc.AddMember(&CTankComponent::m_collisionDamageMult, 'S', "CollisionDamageMult",
				   "Collision Damage Multiplier", "Damage multiplier for collision damage", 0.25f);
}

void CTankComponent::Register(Schematyc::CEnvRegistrationScope &componentScope)
{
	 //Movement Functions
	{
		auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CTankComponent::MoveForward,
													 "{A7C3D1F2-B4E5-4A6B-9C8D-1E2F3A4B5C6D}"_cry_guid, "MoveForward");
		pFunction->SetDescription("Move the tank forward at the specified speed");
		pFunction->BindInput(1, 'a', "Speed", "Forward speed (0 to MaxSpeed)", 0.0f);
		componentScope.Register(pFunction);
	}

	{
		auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CTankComponent::MoveBackward,
													 "{B8D4E2F3-C5F6-4B7C-0D9E-2F3A4B5C6D7E}"_cry_guid, "MoveBackward");
		pFunction->SetDescription("Move the tank backward at the specified speed");
		pFunction->BindInput(1, 'a', "Speed", "Backward speed (0 to MaxSpeed)", 0.0f);
		componentScope.Register(pFunction);
	}

	{
		auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CTankComponent::SteerLeft,
													 "{C9E5F3F4-D6G7-4C8D-1E0F-3G4H5C6D7E8F}"_cry_guid, "SteerLeft");
		pFunction->SetDescription("Steer the tank left by the specified amount");
		pFunction->BindInput(1, 'a', "Amount", "Steering amount (0 to 1)", 0.0f);
		componentScope.Register(pFunction);
	}

	{
		auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CTankComponent::SteerRight,
													 "{D0F6G4I5-E7H8-4D9E-2F1G-4H5I6D7E8F9G}"_cry_guid, "SteerRight");
		pFunction->SetDescription("Steer the tank right by the specified amount");
		pFunction->BindInput(1, 'a', "Amount", "Steering amount (0 to 1)", 0.0f);
		componentScope.Register(pFunction);
	}

	{
		auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CTankComponent::RotateTurret,
													 "{C9E5F3F4-D6G7-4C8D-1E0F-3G4H5C6D7E8F}"_cry_guid, "RotateTurret");
		pFunction->SetDescription("Rotate the tank turret by specified yaw and pitch");
		pFunction->BindInput(1, 'a', "Yaw", "Yaw rotation in degrees", 0.0f);
		pFunction->BindInput(2, 'b', "Pitch", "Pitch rotation in degrees", 0.0f);
		componentScope.Register(pFunction);
	}

	 //Weapon Functions
	{
		auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CTankComponent::FireMainCannon,
													 "{D0F6G4I5-E7H8-4D9E-2F1G-4H5I6D7E8F9G}"_cry_guid, "FireMainCannon");
		pFunction->SetDescription("Fire the main cannon (respects cooldown)");
		componentScope.Register(pFunction);
	}

	{
		auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CTankComponent::FireCoaxialGun,
													 "{E1F7G5H6-F8I9-4E0F-3G2H-5I6J7E8F9G0H}"_cry_guid, "FireCoaxialGun");
		pFunction->SetDescription("Fire the coaxial machine gun (respects cooldown)");
		componentScope.Register(pFunction);
	}

	{
		auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CTankComponent::StopCoaxialGun,
													 "{F2G8H6I7-E9F0-4D1E-3F2G-5H6I7E8F9G0H}"_cry_guid, "StopCoaxialGun");
		pFunction->SetDescription("Stop the coaxial machine gun audio");
		componentScope.Register(pFunction);
	}

	// Projectile spawning functions
	{
		auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CTankComponent::SpawnMainCannonProjectile,
													 "{G3H9I7J8-F0G1-4E2F-4G3H-6I7J8F9G0H1I}"_cry_guid, "SpawnMainCannonProjectile");
		pFunction->SetDescription("Spawn a main cannon projectile at the muzzle position");
		componentScope.Register(pFunction);
	}

	{
		auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CTankComponent::SpawnCoaxialGunProjectile,
													 "{H4I0J8K9-G1H2-4F3G-5H4I-7J8K9G0H1I2J}"_cry_guid, "SpawnCoaxialGunProjectile");
		pFunction->SetDescription("Spawn a coaxial gun projectile at the muzzle position");
		componentScope.Register(pFunction);
	}

	// Particle effect functions
	{
		auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CTankComponent::PlayMuzzleFlashEffect,
													 "{I5J1K9L0-H2I3-4G4H-6I5J-8K9L0H1I2J3K}"_cry_guid, "PlayMuzzleFlashEffect");
		pFunction->SetDescription("Play muzzle flash particle effect");
		pFunction->BindInput(1, 'a', "Position", "Effect position", Vec3(ZERO));
		pFunction->BindInput(2, 'b', "Direction", "Effect direction vector", Vec3(0, 1, 0));
		componentScope.Register(pFunction);
	}

	{
		auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CTankComponent::PlayEngineStopEffect,
					 "{K7L3M1N2-O4P5-4J6K-8L7M-0N1P2J3K4L5}"_cry_guid, "PlayEngineStopEffect");
		pFunction->SetDescription("Play engine stop particle effect");
		pFunction->BindInput(1, 'a', "Position", "Effect position", Vec3(ZERO));
		pFunction->BindInput(2, 'b', "Direction", "Effect direction vector", Vec3(0, 1, 0));
		componentScope.Register(pFunction);
	}

	{
		auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CTankComponent::PlayEngineRunningEffect,
					 "{L8M4N2O3-P5Q6-4K7L-9M8N-1O2Q3K4L5M6}"_cry_guid, "PlayEngineRunningEffect");
		pFunction->SetDescription("Play engine running particle effect");
		pFunction->BindInput(1, 'a', "Position", "Effect position", Vec3(ZERO));
		pFunction->BindInput(2, 'b', "Direction", "Effect direction vector", Vec3(0, 1, 0));
		componentScope.Register(pFunction);
	}

	{
		auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CTankComponent::PlayBoostEffect,
					 "{M9N5O3P4-Q6R7-4L8M-0N9O-2P3R4L5M6N7}"_cry_guid, "PlayBoostEffect");
		pFunction->SetDescription("Play engine boost particle effect");
		pFunction->BindInput(1, 'a', "Position", "Effect position", Vec3(ZERO));
		pFunction->BindInput(2, 'b', "Direction", "Effect direction vector", Vec3(0, 1, 0));
		componentScope.Register(pFunction);
	}

	// Query Functions (Output only)
	{
		auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CTankComponent::GetCurrentSpeed,
													 "{F2H8I6K7-G9J0-4F1G-4H3I-6J7K8F9G0H1I}"_cry_guid, "GetCurrentSpeed");
		pFunction->SetDescription("Get the current tank speed");
		pFunction->BindOutput(0, 'a', "Speed", "Current speed in m/s");
		componentScope.Register(pFunction);
	}

	{
		auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CTankComponent::GetCurrentTurretYaw,
													 "{G3I9J7L8-H0K1-4G2H-5I4J-7K8L9G0H1I2J}"_cry_guid, "GetTurretYaw");
		pFunction->SetDescription("Get the current turret yaw angle");
		pFunction->BindOutput(0, 'a', "Yaw", "Current yaw in degrees");
		componentScope.Register(pFunction);
	}

	{
		auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CTankComponent::GetCurrentTurretPitch,
													 "{H4J0K8M9-I1L2-4H3I-6J5K-8L9M0H1I2J3K}"_cry_guid, "GetTurretPitch");
		pFunction->SetDescription("Get the current turret pitch angle");
		pFunction->BindOutput(0, 'a', "Pitch", "Current pitch in degrees");
		componentScope.Register(pFunction);
	}

	{
		auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CTankComponent::IsEngineRunning,
													 "{I5K1L9N0-J2M3-4I4J-7K6L-9M0N1I2J3K4L}"_cry_guid, "IsEngineRunning");
		pFunction->SetDescription("Check if the tank engine is running");
		pFunction->BindOutput(0, 'a', "Running", "Engine running status");
		componentScope.Register(pFunction);
	}

	{
		auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CTankComponent::IsDestroyed,
													 "{J6L2M0O1-K3N4-4J5K-8L7M-0N1O2J3K4L5M}"_cry_guid, "IsDestroyed");
		pFunction->SetDescription("Check if the tank is destroyed");
		pFunction->BindOutput(0, 'a', "Destroyed", "Destruction status");
		componentScope.Register(pFunction);
	}

	{
		auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CTankComponent::GetMainCannonCooldown,
													 "{K7M3N1P2-L4O5-4K6L-9M8N-1O2P3K4L5M6N}"_cry_guid, "GetMainCannonCooldown");
		pFunction->SetDescription("Get the main cannon cooldown remaining");
		pFunction->BindOutput(0, 'a', "Cooldown", "Cooldown time in seconds");
		componentScope.Register(pFunction);
	}

	{
		auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CTankComponent::GetCoaxialGunCooldown,
													 "{L8N4O2Q3-M5P6-4L7M-0N9O-2P3Q4L5M6N7O}"_cry_guid, "GetCoaxialGunCooldown");
		pFunction->SetDescription("Get the coaxial gun cooldown remaining");
		pFunction->BindOutput(0, 'a', "Cooldown", "Cooldown time in seconds");
		componentScope.Register(pFunction);
	}

	// ===== HELPER POSITION QUERY FUNCTIONS =====
	{
		auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CTankComponent::GetCannonMuzzlePos,
													 "{D4E5F6G7-H8I9-7J0K-4L5M-6N7O8P9Q0R}"_cry_guid, "GetCannonMuzzlePos");
		pFunction->SetDescription("Get the cannon muzzle (exit) position");
		pFunction->BindOutput(0, 'a', "Position", "Cannon muzzle world position");
		componentScope.Register(pFunction);
	}

	{
		auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CTankComponent::GetTurretCenter,
													 "{E5F6G7H8-I9J0-8K1L-5M6N-7O8P9Q0R1S}"_cry_guid, "GetTurretCenter");
		pFunction->SetDescription("Get the turret rotation center position");
		pFunction->BindOutput(0, 'a', "Position", "Turret center world position");
		componentScope.Register(pFunction);
	}

	{
		auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CTankComponent::GetEngineSmokePos,
													 "{F6G7H8I9-J0K1-9L2M-6N7O-8P9Q0R1S2T}"_cry_guid, "GetEngineSmokePos");
		pFunction->SetDescription("Get the engine smoke effect position");
		pFunction->BindOutput(0, 'a', "Position", "Engine smoke world position");
		componentScope.Register(pFunction);
	}

	{
		auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CTankComponent::GetDriverViewPos,
													 "{G7H8I9J0-K1L2-0M3N-7O8P-9Q0R1S2T3U}"_cry_guid, "GetDriverViewPos");
		pFunction->SetDescription("Get the driver view/camera position");
		pFunction->BindOutput(0, 'a', "Position", "Driver view world position");
		componentScope.Register(pFunction);
	}

	{
		auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CTankComponent::GetGunnerViewPos,
													 "{H8I9J0K1-L2M3-1N4O-8P9Q-0R1S2T3U4V}"_cry_guid, "GetGunnerViewPos");
		pFunction->SetDescription("Get the gunner view/camera position");
		pFunction->BindOutput(0, 'a', "Position", "Gunner view world position");
		componentScope.Register(pFunction);
	}

	// ===== COMPONENT DAMAGE QUERY FUNCTIONS =====
	{
		auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CTankComponent::GetHullHealth,
													 "{I9J0K1L2-M3N4-2O5P-9Q0R-1S2T3U4V5W}"_cry_guid, "GetHullHealth");
		pFunction->SetDescription("Get the hull health as percentage (0-100)");
		pFunction->BindOutput(0, 'a', "Health", "Hull health percentage");
		componentScope.Register(pFunction);
	}

	{
		auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CTankComponent::GetEngineHealth,
													 "{J0K1L2M3-N4O5-3P6Q-0R1S-2T3U4V5W6X}"_cry_guid, "GetEngineHealth");
		pFunction->SetDescription("Get the engine health as percentage (0-100)");
		pFunction->BindOutput(0, 'a', "Health", "Engine health percentage");
		componentScope.Register(pFunction);
	}

	{
		auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CTankComponent::GetTurretHealth,
													 "{K1L2M3N4-O5P6-4Q7R-1S2T-3U4V5W6X7Y}"_cry_guid, "GetTurretHealth");
		pFunction->SetDescription("Get the turret health as percentage (0-100)");
		pFunction->BindOutput(0, 'a', "Health", "Turret health percentage");
		componentScope.Register(pFunction);
	}

	{
		auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CTankComponent::GetLeftTreadHealth,
													 "{L2M3N4O5-P6Q7-5R8S-2T3U-4V5W6X7Y8Z}"_cry_guid, "GetLeftTreadHealth");
		pFunction->SetDescription("Get the left tread health as percentage (0-100)");
		pFunction->BindOutput(0, 'a', "Health", "Left tread health percentage");
		componentScope.Register(pFunction);
	}

	{
		auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CTankComponent::GetRightTreadHealth,
													 "{M3N4O5P6-Q7R8-6S9T-3U4V-5W6X7Y8Z9A}"_cry_guid, "GetRightTreadHealth");
		pFunction->SetDescription("Get the right tread health as percentage (0-100)");
		pFunction->BindOutput(0, 'a', "Health", "Right tread health percentage");
		componentScope.Register(pFunction);
	}

	{
		auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CTankComponent::ApplyDamage,
													 "{N4O5P6Q7-R8S9-7T0U-4V5W-6X7Y8Z9A0B}"_cry_guid, "ApplyDamage");
		pFunction->SetDescription("Apply damage to a specific tank component");
		pFunction->BindInput(1, 'a', "Damage", "Amount of damage to apply", 0.0f);
		pFunction->BindInput(2, 'b', "Component", "Component to damage (hull/engine/turret/left_tread/right_tread)", Schematyc::CSharedString(""));
		componentScope.Register(pFunction);
	}

	{
		auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CTankComponent::ApplyHullDamage,
													 "{O5P6Q7R8-S9T0-8U1V-5W6X-7Y8Z9A0B1C}"_cry_guid, "ApplyHullDamage");
		pFunction->SetDescription("Apply damage to the tank hull");
		pFunction->BindInput(1, 'a', "Damage", "Amount of damage to apply", 0.0f);
		componentScope.Register(pFunction);
	}

	{
		auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CTankComponent::ApplyEngineDamage,
													 "{P6Q7R8S9-T0U1-9V2W-6X7Y-8Z9A0B1C2D}"_cry_guid, "ApplyEngineDamage");
		pFunction->SetDescription("Apply damage to the tank engine");
		pFunction->BindInput(1, 'a', "Damage", "Amount of damage to apply", 0.0f);
		componentScope.Register(pFunction);
	}

	{
		auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CTankComponent::ApplyTurretDamage,
													 "{Q7R8S9T0-U1V2-0W3X-7Y8Z-9A0B1C2D3E}"_cry_guid, "ApplyTurretDamage");
		pFunction->SetDescription("Apply damage to the tank turret");
		pFunction->BindInput(1, 'a', "Damage", "Amount of damage to apply", 0.0f);
		componentScope.Register(pFunction);
	}

	{
		auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CTankComponent::ApplyLeftTreadDamage,
													 "{R8S9T0U1-V2W3-1X4Y-8Z9A-0B1C2D3E4F}"_cry_guid, "ApplyLeftTreadDamage");
		pFunction->SetDescription("Apply damage to the left tank tread");
		pFunction->BindInput(1, 'a', "Damage", "Amount of damage to apply", 0.0f);
		componentScope.Register(pFunction);
	}

	{
		auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CTankComponent::ApplyRightTreadDamage,
													 "{S9T0U1V2-W3X4-2Y5Z-9A0B-1C2D3E4F5G}"_cry_guid, "ApplyRightTreadDamage");
		pFunction->SetDescription("Apply damage to the right tank tread");
		pFunction->BindInput(1, 'a', "Damage", "Amount of damage to apply", 0.0f);
		componentScope.Register(pFunction);
	}

	{
		auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CTankComponent::RepairComponent,
													 "{T0U1V2W3-X4Y5-3Z6A-0B1C-2D3E4F5G6H}"_cry_guid, "RepairComponent");
		pFunction->SetDescription("Repair a specific tank component");
		pFunction->BindInput(1, 'a', "Component", "Component to repair (hull/engine/turret/left_tread/right_tread)", Schematyc::CSharedString(""));
		pFunction->BindInput(2, 'b', "RepairAmount", "Amount of damage to repair", 0.0f);
		componentScope.Register(pFunction);
	}
}

IEntityAudioComponent* CTankComponent::GetAudioProxy()
{
	IEntityAudioComponent* pAudioComponent = GetEntity()->GetOrCreateComponent<IEntityAudioComponent>();
	assert(pAudioComponent);
	return pAudioComponent;
}

int CTankComponent::LoadTreadSlot(const string& filename, int slotIndex, _smart_ptr<ICharacterInstance>& outChar, const char* treadName)
{
	if (filename.empty())
	{
		CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_WARNING, "TankComponent: No filename specified for %s tread slot", treadName);
		return -1;
	}

	int slot = GetEntity()->LoadCharacter(slotIndex, filename);
	if (slot < 0)
	{
		CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_WARNING, "TankComponent: Failed to load %s tread character '%s'", treadName, filename.c_str());
		return -1;
	}

	outChar = GetEntity()->GetCharacter(slot);
	if (!outChar)
	{
		CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_WARNING, "TankComponent: Loaded %s tread slot %d but character instance is null", treadName, slot);
		return -1;
	}

	GetEntity()->SetSlotFlags(slot, ENTITY_SLOT_RENDER | ENTITY_SLOT_CAST_SHADOW);

	return slot;
}

bool CTankComponent::SpawnParticleEffect(const char* effectName, const Vec3& position, const Vec3& direction, float scale = 1.0f)
{
	if (gEnv) {
		if (!gEnv->pParticleManager) {
			return false;
		}
		if (!effectName || effectName[0] == '\0') {
			return false;
		}
	}

	IParticleEffect* pEffect = gEnv->pParticleManager->FindEffect(effectName);
	if (!pEffect)
    {
        if (gEnv && gEnv->pSystem && gEnv->pSystem->GetILog())
		{
			CryLog("[Tank] Particle effect '%s' not found (FindEffect returned null)", effectName);
		}
		return false;
	}

	// Ensure direction is valid and normalized to avoid invalid rotations/emitters
	Vec3 dir = direction;
	float len = sqrtf(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
	if (len < 1e-6f)
	{
		dir = Vec3(0.0f, 0.0f, 1.0f);
	}
	else
	{
		dir /= len;
	}


	//
	// 1. Find the "ParticleEffect" class. This is the same class Sandbox uses.
	IEntityClass* pParticleClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass("ParticleEffect");
	if (pParticleClass)
	{
		// 2. Set up the spawn parameters
		SEntitySpawnParams spawnParams;
		spawnParams.pClass = pParticleClass;
		spawnParams.sName = "Effect_Entity"; // Now it has a name in the Entity List
		spawnParams.vPosition = position;
		spawnParams.qRotation = Quat::CreateRotationVDir(direction);
		spawnParams.vScale = Vec3(scale);

			int particleSlot = -1;
			
			// 2. Load the particle emitter directly into the tank entity
			particleSlot = GetEntity()->LoadParticleEmitter(particleSlot, pEffect);

			if (particleSlot != -1)
			{
				// 3. Get the Tank's current World Transform and invert it
				Matrix34 tankWorldTM = GetEntity()->GetWorldTM();
				Matrix34 tankInvertedTM = tankWorldTM.GetInvertedFast();

				// 4. Transform World Position -> Local Position using the inverted matrix
				Vec3 localOffset = tankInvertedTM.TransformPoint(position);

				// 5. Transform World Direction -> Local Direction using the inverted matrix
				Vec3 localDir = tankInvertedTM.TransformVector(dir);
				Quat localRotation = Quat::CreateRotationVDir(localDir);

				// 6. Build the final Local Transformation Matrix for the slot
				Matrix34 localTM = Matrix34(Matrix33(localRotation), localOffset);

				// 7. Apply uniform scaling natively using ScaleColumn
				localTM.ScaleColumn(Vec3(scale, scale, scale));

				// 8. Commit to the slot
				GetEntity()->SetSlotLocalTM(particleSlot, localTM);
			}

	}

	return true;
}

IPhysicalEntity* CTankComponent::GetTankPhysics() const
{
	// Primary: Get the physical entity directly from the Entity
	IPhysicalEntity* pEntityPhys = GetEntity()->GetPhysicalEntity();
	if (pEntityPhys)
	{
		return pEntityPhys;
	}

	// Fallback: Check the cached ID if the entity pointer is temporarily null
	if (m_pPhysCacheId != 0)
	{
		IPhysicalEntity* pCached = gEnv->pPhysicalWorld->GetPhysicalEntityById(m_pPhysCacheId);
		if (pCached) return pCached;
	}



	if (IPhysicalEntity* pEntityPhysics = GetEntity()->GetPhysics())
	{
		return pEntityPhysics;
	}

    // Try to find physics attached to a character instance (skeleton physics).
    ICharacterInstance* pCharacter = nullptr;

    // First try the known body slot
    if (m_bodySlot >= 0)
    {
        pCharacter = GetEntity()->GetCharacter(m_bodySlot);
    }

    // Then try the cached character pointer
    if (pCharacter == nullptr)
    {
        pCharacter = m_pCachedCharacter;
    }

    // Probe other common slots for a character as a fallback
    if (pCharacter == nullptr)
    {
        for (int i = 0; i < 8; ++i) // 0..7 covers most typical setups
        {
            ICharacterInstance* pChar = GetEntity()->GetCharacter(i);
            if (pChar)
            {
                pCharacter = pChar;
                break;
            }
        }
    }

    if (pCharacter != nullptr)
    {
        if (ISkeletonPose* pSkeletonPose = pCharacter->GetISkeletonPose())
        {
            if (IPhysicalEntity* pCharPhys = pSkeletonPose->GetCharacterPhysics())
            {
                CryLogAlways("[Tank] GetTankPhysics: using character skeleton physics for entity '%s'", GetEntity()->GetName());
                return pCharPhys;
            }
        }
    }

    CryLogAlways("[Tank] GetTankPhysics: no physics found for entity '%s' (entity-level and character fallbacks failed)", GetEntity()->GetName());
    return nullptr;
}

bool CTankComponent::JointNameMatchesWheelFilter(const char* jointName, const char* filter)
{
	if (!jointName || !jointName[0])
	{
		return false;
	}

	const char* filterText = (filter && filter[0]) ? filter : "wheel";
	return CryStringUtils::stristr(jointName, filterText) != nullptr;
}

void CTankComponent::SetupComponentVehiclePhysics()
{
	m_pVehiclePhysicsComponent = m_pEntity->GetComponent<Cry::DefaultComponents::CVehiclePhysicsComponent>();

	// Editor-placed component may have initialized before the CGA was loaded, leaving no physics entity.
	if (m_pVehiclePhysicsComponent && !GetEntity()->GetPhysicalEntity())
	{
		m_pEntity->RemoveComponent<Cry::DefaultComponents::CVehiclePhysicsComponent>(false);
		m_pVehiclePhysicsComponent = nullptr;
	}

	if (!m_pVehiclePhysicsComponent)
	{
		m_pVehiclePhysicsComponent = m_pEntity->CreateComponent<Cry::DefaultComponents::CVehiclePhysicsComponent>();
	}

	if (!m_pVehiclePhysicsComponent)
	{
		CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_ERROR,
			"TankComponent: failed to create Vehicle Physics component on '%s'", GetEntity()->GetName());
		return;
	}

	//EnsureVehiclePhysicalized();

	if (gEnv && gEnv->pSystem && gEnv->pSystem->GetILog())
	{
		CryLogAlways("[Tank] SetupComponentVehiclePhysics: Vehicle Physics component ready on '%s' (physics entity %s).",
			GetEntity()->GetName(), GetTankPhysics() ? "present" : "MISSING");
	}
}

void CTankComponent::EnsureVehiclePhysicalized()
{
	IPhysicalEntity* pPhysics = GetEntity()->GetPhysicalEntity();
	if (pPhysics && pPhysics->GetType() == PE_WHEELEDVEHICLE)
	{
		return; // Already physicalized correctly
	}

	m_vehicleGearRatios = { -1.f, 0.f, 1.f, 2.f };

	SEntityPhysicalizeParams physParams;
	physParams.type = PE_WHEELEDVEHICLE;

	pe_params_car carParams;

	// 1. CRITICAL: Explicitly set the vehicle mass (60,000 kg / 60 tons)
	physParams.mass = 60000.0f;

	// 2. CRITICAL: Point nSlot directly to your CGA model slot (usually 0)
	// FIXED: Keep the valid body slot so CryPhysics binds to the loaded CGA asset
	physParams.nSlot = m_bodySlot >= 0 ? m_bodySlot : 0;

	carParams.enginePower = 80000.f;
	carParams.engineMaxRPM = 3000.f;
	carParams.engineMinRPM = 60.f;
	carParams.engineIdleRPM = 800.f;
	carParams.engineStartRPM = 600.f;
	carParams.nGears = static_cast<int>(m_vehicleGearRatios.size());
	carParams.gearRatios = m_vehicleGearRatios.data();
	carParams.engineShiftUpRPM = 2200.f;
	carParams.engineShiftDownRPM = 1200.f;
	carParams.wheelMassScale = 1.f;
	physParams.pCar = &carParams;


	GetEntity()->Physicalize(physParams);



	if (gEnv && gEnv->pSystem && gEnv->pSystem->GetILog())
	{
		CryLogAlways("[Tank] EnsureVehiclePhysicalized: %s on '%s'.",
			GetTankPhysics() ? "[Tank] created wheeled physics" : "FAILED",
			GetEntity()->GetName());
	}
}

void CTankComponent::TryFinishWheelSetup()
{
	if (m_wheelsSetupDone || !m_pendingWheelSetup || !m_pCachedCharacter)
	{
		return;
	}

	if (!GetTankPhysics())
	{
		return;
	}

	if (!m_pVehiclePhysicsComponent)
	{
		SetupComponentVehiclePhysics();
	}

	SetupWheelsFromCharacter(m_pCachedCharacter);

	if (!m_wheelsSetupDone)
	{
		return;
	}

	m_pendingWheelSetup = false;

	if (!m_tankMovementInitialized)
	{
		InitializeTankMovement();
		m_tankMovementInitialized = true;
	}
}

bool CTankComponent::AddWheelColliderAtJoint(const char* jointName, const Matrix34& jointLocalTM, int axleIndex)
{
	IPhysicalEntity* pPhysics = GetEntity()->GetPhysicalEntity();
	IGeomManager* pGeomManager = gEnv->pPhysicalWorld->GetGeomManager();
	if (!pPhysics || !pGeomManager)
	{
		return false;
	}

	primitives::cylinder cylinderPrim;
	cylinderPrim.center = ZERO;
	cylinderPrim.axis = Vec3(1, 0, 0);
	cylinderPrim.r = m_wheelRadius;
	cylinderPrim.hh = m_wheelWidth * 0.5f;

	IGeometry* pPrimGeom = pGeomManager->CreatePrimitive(primitives::cylinder::type, &cylinderPrim);
	if (!pPrimGeom)
	{
		return false;
	}

	phys_geometry* pPhysGeom = pGeomManager->RegisterGeometry(pPrimGeom, 0);
	pPrimGeom->Release();
	if (!pPhysGeom)
	{
		return false;
	}

	pe_cargeomparams wheelParams;
	wheelParams.mass = m_wheelMass;
	wheelParams.density = -1.0f;
	wheelParams.pos = jointLocalTM.GetTranslation();
	wheelParams.q = Quat(jointLocalTM);
	wheelParams.scale = jointLocalTM.GetUniformScale();
	wheelParams.bDriving = 1;
	wheelParams.iAxle = axleIndex;
	wheelParams.bCanBrake = 1;
	wheelParams.bRayCast = m_wheelRaycast ? 1 : 0;
	wheelParams.flagsCollider = geom_colltype_vehicle;
	wheelParams.flags &= ~geom_floats;
	wheelParams.pivot = jointLocalTM.GetTranslation() + Vec3(0, 0, m_wheelSuspensionLength);
	wheelParams.lenMax = m_wheelSuspensionLength;
	wheelParams.lenInitial = m_wheelSuspensionCompressed;
	wheelParams.kStiffness = 0.f;
	wheelParams.kDamping = -m_wheelDamping;

	const int partId = 100 + static_cast<int>(m_wheelPhysPartIds.size());
	const int addedPartId = pPhysics->AddGeometry(pPhysGeom, &wheelParams, partId);
	if (addedPartId < 0)
	{
		CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_WARNING,
			"TankComponent: AddGeometry failed for wheel joint '%s' on '%s'",
			jointName, GetEntity()->GetName());
		return false;
	}

	m_wheelPhysPartIds.push_back(addedPartId);
	return true;
}
void CTankComponent::SetupWheelsFromCharacter(ICharacterInstance* pCharacter)
{
	if (m_wheelsSetupDone || !pCharacter || m_bodySlot < 0)
	{
		return;
	}

	if (!GetEntity()->GetPhysicalEntity())
	{
		if (!m_loggedWheelSetupDeferred && gEnv && gEnv->pSystem && gEnv->pSystem->GetILog())
		{
			m_loggedWheelSetupDeferred = true;
			CryLogAlways("[Tank] SetupWheelsFromCharacter: physics not ready on '%s', deferring wheel setup.",
				GetEntity()->GetName());
		}
		return;
	}

	if (!m_pVehiclePhysicsComponent)
	{
		SetupComponentVehiclePhysics();
		if (!m_pVehiclePhysicsComponent)
		{
			return;
		}
	}

	ISkeletonPose* pSkeletonPose = pCharacter->GetISkeletonPose();
	if (!pSkeletonPose)
	{
		return;
	}

	const IDefaultSkeleton& defaultSkeleton = pCharacter->GetIDefaultSkeleton();
	const uint32 jointCount = defaultSkeleton.GetJointCount();
	const Matrix34 slotTM = GetEntity()->GetSlotLocalTM(m_bodySlot, false);
	const char* filter = m_wheelJointNameFilter.c_str();

	// Sized up to 9 to match your 0-8 axle indices securely
	int axleCounters[9] = { 0 };

	m_wheelPhysPartIds.clear();
	m_wheelPhysPartIds.reserve(18); // Reserved for all 18 tank wheels

	for (uint32 jointIndex = 0; jointIndex < jointCount; ++jointIndex)
	{
		const char* pRawJointName = defaultSkeleton.GetJointNameByID(static_cast<int32>(jointIndex));
		if (!JointNameMatchesWheelFilter(pRawJointName, filter))
		{
			continue;
		}

		// CRITICAL: Instantly copy the raw pointer to a safe local string 
		// before any other engine state or matrix calls mutate the buffer.
		string safeJointName = pRawJointName;

		const QuatT& absJoint = pSkeletonPose->GetAbsJointByID(static_cast<int32>(jointIndex));
		Matrix34 jointLocalTM = slotTM * Matrix34(absJoint);

		// ===== CLEAN STRING SCANNING (NO POINTER MUTATION) =====
		int wheelNumber = 1;
		for (size_t i = 0; i < safeJointName.length(); ++i)
		{
			if (safeJointName[i] >= '0' && safeJointName[i] <= '9')
			{
				wheelNumber = atoi(safeJointName.c_str() + i);
				break;
			}
		}

		// Map 1-9 and 10-18 cleanly down to 0-8 axle rows
		int axleIndex = (wheelNumber - 1) % 9;

		if (axleIndex >= 0 && axleIndex < 9)
		{
			axleCounters[axleIndex]++;
		}

		// Pass the safe immutable string copy to the physicalizer and logs
		if (AddWheelColliderAtJoint(safeJointName.c_str(), jointLocalTM, axleIndex) && gEnv && gEnv->pSystem && gEnv->pSystem->GetILog())
		{
			const Vec3& pos = jointLocalTM.GetTranslation();
			CryLogAlways("[Tank] Added wheel collider on joint '%s' at local (%.2f, %.2f, %.2f), axle %d",
				safeJointName.c_str(), pos.x, pos.y, pos.z, axleIndex);
		}
	}

	m_wheelsSetupDone = true;

	if (gEnv && gEnv->pSystem && gEnv->pSystem->GetILog())
	{
		CryLogAlways("[Tank] SetupWheelsFromCharacter: registered %d wheel collider(s) on '%s' (filter '%s').",
			static_cast<int>(m_wheelPhysPartIds.size()), GetEntity()->GetName(), filter);
	}
}

void CTankComponent::InitializeTankMovement()
{
	CryLogAlways("[Tank] InitializeTankMovement called for '%s'.", GetEntity()->GetName());

	IPhysicalEntity* pPhysics = GetTankPhysics();
	if (!pPhysics)
	{
		if (!m_loggedMissingWheelPhysics && gEnv && gEnv->pSystem && gEnv->pSystem->GetILog())
		{
			m_loggedMissingWheelPhysics = true;
			CryLogAlways("[Tank] InitializeTankMovement: no physics entity on '%s', wheeled movement cannot initialize.",
				GetEntity()->GetName());
		}
		return;
	}

	m_wheelCount = 0;
	if (!m_wheelPhysPartIds.empty())
	{
		m_wheelCount = static_cast<int>(m_wheelPhysPartIds.size());
	}
	else
	{
		pe_status_wheel wheelStatus;
		for (int i = 0;; ++i)
		{
			wheelStatus = pe_status_wheel();
			wheelStatus.iWheel = i;
			if (!pPhysics->GetStatus(&wheelStatus))
			{
				break;
			}
			m_wheelCount = i + 1;
		}
	}

	if (gEnv && gEnv->pSystem && gEnv->pSystem->GetILog())
	{
		if (m_wheelCount > 0)
		{
			CryLogAlways("[Tank] InitializeTankMovement: discovered %d wheel(s) on '%s'. Advanced wheeled movement is available.",
				m_wheelCount, GetEntity()->GetName());
		}
		else
		{
			CryLogAlways("[Tank] InitializeTankMovement: discovered 0 wheels on '%s'. The model/physics setup is not exposing PE_WHEELEDVEHICLE wheels, so fallback movement will be used.",
				GetEntity()->GetName());
		}
	}
	
	// Track available visual tread slots instead of placeholder IDs.
	m_treadPartIds.clear();
	if (m_leftTreadSlot >= 0)
	{
		m_treadPartIds.push_back(m_leftTreadSlot);
	}
	if (m_rightTreadSlot >= 0)
	{
		m_treadPartIds.push_back(m_rightTreadSlot);
	}
	
	// pe_status requires type_id set; memset would clear it and break GetStatus.
	m_vehicleStatus = pe_status_vehicle();
	memset(&m_action, 0, sizeof(m_action));
	m_action.type = pe_action_drive::type_id;
}


float CTankComponent::GetWheelCondition() const
{
	float leftCondition = 1.0f;
	float rightCondition = 1.0f;

	if (m_leftTreadDamageMax > 0.0f)
		leftCondition = max(0.0f, 1.0f - m_leftTreadCurrentDamage / m_leftTreadDamageMax);
	if (m_rightTreadDamageMax > 0.0f)
		rightCondition = max(0.0f, 1.0f - m_rightTreadCurrentDamage / m_rightTreadDamageMax);

	return min(leftCondition, rightCondition);
}

void CTankComponent::SetLatFriction(float latFriction)
{
	IPhysicalEntity* pPhysics = GetTankPhysics();
	if (!pPhysics || pPhysics->GetType() != PE_WHEELEDVEHICLE || m_wheelCount <= 0)
	{
		return;
	}

	// Tanks with many CGA-matched wheel bones: use vehicle-wide axle friction (GameSDK tread-style).
	// Per-wheel pe_params_wheel can crash if suspension slots are not fully initialized yet.
	pe_params_car carParams;
	if (pPhysics->GetParams(&carParams))
	{
		carParams.axleFriction = latFriction;
		if (pPhysics->SetParams(&carParams, 1))
		{
			m_currentLatFriction = latFriction;
			return;
		}
	}

	const int numWheels = min(m_wheelCount, static_cast<int>(m_wheelPhysPartIds.size() > 0 ? m_wheelPhysPartIds.size() : m_wheelCount));
	for (int i = 0; i < numWheels; ++i)
	{
		pe_status_wheel wheelStatus;
		wheelStatus.iWheel = i;
		if (!pPhysics->GetStatus(&wheelStatus))
		{
			break;
		}

		pe_params_wheel wheelParams;
		wheelParams.iWheel = i;
		wheelParams.kLatFriction = latFriction;
		pPhysics->SetParams(&wheelParams, 1);
	}

	m_currentLatFriction = latFriction;
}

void CTankComponent::ProcessMovement(float deltaTime)
{
	if (!m_isEngineRunning || m_isDestroyed)
	{
		m_currentPedal = 0.0f;
		return;
	}

	IPhysicalEntity* pPhysics = GetTankPhysics();
	if (!pPhysics)
		return;

	// Use GameSDK-style advanced movement when wheeled-vehicle data exists,
	// otherwise fall back to simple rigid-body movement so default setups still drive.
	IPhysicalEntity* pPhys = GetTankPhysics();

	pe_status_dynamics dynamicsStatus;
	// Ensure you have a valid pointer to your physical entity (IPhysicalEntity*)
	if (pPhys)
	{


		// GetStatus returns non-zero if successful
		if (pPhys->GetStatus(&dynamicsStatus) != 0)
		{
			float entityMass = dynamicsStatus.mass;

			// Safety check: Static objects or objects with infinite mass 
			// may return 0 or near-infinite values depending on the context.
			if (entityMass > 0.0f)
			{
				// Use your mass info here
			}
		}
	}

	CryLogAlways("[Tank] Initialize complete for TankBody. Legacy body path field is '%s'. mass: %.2f",
		m_bodyMesh.c_str(), pPhys ? dynamicsStatus.mass : 0.0f);
	m_vehicleStatus = pe_status_vehicle();
	const bool hasVehicleStatus = (m_wheelCount > 0) && pPhysics->GetStatus(&m_vehicleStatus);
	if (hasVehicleStatus)
	{
		m_loggedFallbackReason = false;
		ProcessMovementAdvanced(deltaTime);
	}
	else
	{
		if (!m_loggedFallbackReason && gEnv && gEnv->pSystem && gEnv->pSystem->GetILog())
		{
			m_loggedFallbackReason = true;

			if (m_wheelCount <= 0)
			{
				CryLogAlways("[Tank] ProcessMovement: '%s' cannot enter advanced wheeled movement because wheel discovery found 0 wheels.",
					GetEntity()->GetName());
			}
			else
			{
				CryLogAlways("[Tank] ProcessMovement: '%s' discovered %d wheel(s) but pe_status_vehicle is unavailable, so advanced wheeled movement is not achieved.",
					GetEntity()->GetName(), m_wheelCount);
			}
		}

		ProcessMovementFallback(deltaTime);
	}
}

void CTankComponent::Initialize()
{
	// Initialize tank state
	m_isEngineRunning = true;
	m_isDestroyed = false;
	m_currentSpeed = 0.0f;
	m_currentRotation = 0.0f;
	m_currentTurretYaw = 0.0f;
	m_currentTurretPitch = 0.0f;

	// Log initialization
	if (gEnv && gEnv->pSystem && gEnv->pSystem->GetILog())
	{
		CryLogAlways("[Tank] Initialize called for entity: %s", GetEntity()->GetName());
	}

	// Resolve the primary tank body asset using the GameSDK default if no explicit Schematyc model was provided.
	const string bodyAsset = (!m_tankBodyMesh.value.empty() ? m_tankBodyMesh.value : m_bodyMesh);

	if (!bodyAsset.empty())
	{
		CryLogAlways("[Tank] Initialize: attempting to load TankBody asset '%s' on entity '%s'.",
			bodyAsset.c_str(), GetEntity()->GetName());

		// CORRECT: Load character INTO entity slot (not via CharacterManager directly)
		int slot = GetEntity()->LoadCharacter(0, bodyAsset);
		
		if (slot >= 0)
		{
			CryLogAlways("[Tank] Initialize: LoadCharacter succeeded for '%s' with slot %d.",
			bodyAsset.c_str(), slot);
			// Get character instance from entity
			ICharacterInstance* pCharInstance = GetEntity()->GetCharacter(slot);
			
			if (pCharInstance)
			{
				// Cache for later access
				m_pCachedCharacter = pCharInstance;
				
				// Set rendering flag
				GetEntity()->SetSlotFlags(slot, ENTITY_SLOT_RENDER | ENTITY_SLOT_CAST_SHADOW);

				// Component-based wheeled vehicle: Vehicle Physics + Wheel Colliders from CGA joints
				SetupComponentVehiclePhysics();


				// Access skeleton for animations
				ISkeletonAnim* pSkeletonAnim = pCharInstance->GetISkeletonAnim();
				if (pSkeletonAnim)
					pSkeletonAnim->StopAnimationsAllLayers();
				
				m_bodySlot = slot;
				m_leftTreadSlot = LoadTreadSlot(m_leftTreadMesh, 1, m_leftTreadCharacter, "left");
				m_rightTreadSlot = LoadTreadSlot(m_rightTreadMesh, 2, m_rightTreadCharacter, "right");

			

				m_pendingWheelSetup = true;
				SetupWheelsFromCharacter(pCharInstance);
				TryFinishWheelSetup();

				// Cache audio control IDs for movement-related updates
				m_audioSpeedParamId = CryAudio::StringToId("vehicle_speed");
				m_audioEngineRunTriggerId = CryAudio::StringToId("Play_vehicle_run");
				m_audioEngineStopTriggerId = CryAudio::StringToId("Stop_vehicle_run");
				
				// Turret audio
				m_audioTurretTurnTriggerId = CryAudio::StringToId("Play_abrams_cannon_turn");
				m_audioTurretRotationSpeedParamId = CryAudio::StringToId("vehicle_rotation_speed");
				
				// Weapon audio
				m_audioMainCannonTriggerId = CryAudio::StringToId("Play_w_tank_cannon_fire");
				m_audioCoaxialGunTriggerId = CryAudio::StringToId("Play_w_tank_machinegun_fire");
				m_audioCoaxialGunStopTriggerId = CryAudio::StringToId("Stop_w_tank_machinegun_fire");
				
				if (!m_tankMovementInitialized)
				{
					InitializeTankMovement();
					m_tankMovementInitialized = true;
				}

				if (m_isEngineRunning)
				{
					IEntityAudioComponent* pAudio = GetAudioProxy();
					if (pAudio && m_audioEngineRunTriggerId != CryAudio::InvalidControlId)
					{
						pAudio->ExecuteTrigger(m_audioEngineRunTriggerId);
						m_isEngineSoundPlaying = true;
					}
				}

				IPhysicalEntity* pPhys = GetEntity()->GetPhysicalEntity(); // Cache the physics pointer for later use in movement and effects
				m_pPhysCacheId = gEnv->pPhysicalWorld->GetPhysicalEntityId(pPhys); // Cache the physics entity ID for quick access in GetTankPhysics()
				pPhys = GetTankPhysics(); // Validate that GetTankPhysics can retrieve the entity's physics using the cached ID
				pe_status_dynamics dynamicsStatus;
				// Ensure you have a valid pointer to your physical entity (IPhysicalEntity*)
				if (pPhys)
				{
					

					// GetStatus returns non-zero if successful
					if (pPhys->GetStatus(&dynamicsStatus) != 0)
					{
						float entityMass = dynamicsStatus.mass;

						// Safety check: Static objects or objects with infinite mass 
						// may return 0 or near-infinite values depending on the context.
						if (entityMass > 0.0f)
						{
							// Use your mass info here
						}
					}
				}
				
				CryLogAlways("[Tank] Initialize complete for TankBody asset '%s'. Legacy body path field is '%s'. mass: %.2f",
					bodyAsset.c_str(), m_bodyMesh.c_str(), pPhys ? dynamicsStatus.mass : 0.0f);
				InitializeLocalPlayer();
			}
			else
			{
				CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_ERROR, "Failed to get character instance for %s on entity %s", bodyAsset.c_str(), GetEntity()->GetName());
			}
		}
		else
		{
			CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_ERROR, "Failed to load character %s on entity %s (slot=%d)", bodyAsset.c_str(), GetEntity()->GetName(), slot);
		}
	}
	else
	{
		CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_WARNING, "TankComponent: no tank body asset specified on entity '%s'. Specify m_tankBodyMesh or use the built-in GameSDK path.", GetEntity()->GetName());
	}
}

void CTankComponent::DebugMass() {
	IPhysicalEntity* pPhys = gEnv->pPhysicalWorld->GetPhysicalEntityById(m_pPhysCacheId); // Validate that GetTankPhysics can retrieve the entity's physics using the cached ID
	pe_status_dynamics dynamicsStatus;
	
	// Ensure you have a valid pointer to your physical entity (IPhysicalEntity*)
	if (pPhys)
	{
		// Cross-reference: Find the gameplay entity from the physics pointer
		IEntity* pEntity = gEnv->pEntitySystem->GetEntityFromPhysics(pPhys);
		const char* entityName;

		if (pEntity)
		{
			entityName = pEntity->GetName();
			//CryLogAlways("[Physics] This physical entity belongs to gameplay entity: %s", entityName);
		}
		else
		{
			// This can happen for static map geometry, terrain, or pure physics particles
			CryLogAlways("[Physics] This physical entity has no owning gameplay entity.");
		}

		// GetStatus returns non-zero if successful
		if (pPhys->GetStatus(&dynamicsStatus) != 0)
		{
			float entityMass = dynamicsStatus.mass;

			// Safety check: Static objects or objects with infinite mass 
			// may return 0 or near-infinite values depending on the context.
			if (entityMass > 0.0f)
			{
				// Use your mass info here
			}
		}
		//CryLogAlways("[Tank] Initialize complete for entity '%s'mass: %.2f", entityName ? entityName : "Unknown", pPhys ? dynamicsStatus.mass : 0.0f);

	}

}

void CTankComponent::InitializeLocalPlayer()
{
	m_pInputComponent = m_pEntity->GetOrCreateComponent<Cry::DefaultComponents::CInputComponent>();

	m_pInputComponent->RegisterAction("tank", "moveleft", [this](int activationMode, float value)
	{
		SetMoveLeftActive(activationMode != eAAM_OnRelease);
	});
	m_pInputComponent->BindAction("tank", "moveleft", eAID_KeyboardMouse, EKeyId::eKI_A);

	m_pInputComponent->RegisterAction("tank", "moveright", [this](int activationMode, float value)
	{
		SetMoveRightActive(activationMode != eAAM_OnRelease);
	});
	m_pInputComponent->BindAction("tank", "moveright", eAID_KeyboardMouse, EKeyId::eKI_D);

	m_pInputComponent->RegisterAction("tank", "moveforward", [this](int activationMode, float value)
	{
		SetMoveForwardActive(activationMode != eAAM_OnRelease);
	});
	m_pInputComponent->BindAction("tank", "moveforward", eAID_KeyboardMouse, EKeyId::eKI_W);

	m_pInputComponent->RegisterAction("tank", "moveback", [this](int activationMode, float value)
	{
		SetMoveBackActive(activationMode != eAAM_OnRelease);
	});
	m_pInputComponent->BindAction("tank", "moveback", eAID_KeyboardMouse, EKeyId::eKI_S);

	m_pInputComponent->RegisterAction("tank", "mouse_rotateyaw", [this](int activationMode, float value)
	{
		m_mouseDeltaRotation.x -= value;
	});
	m_pInputComponent->BindAction("tank", "mouse_rotateyaw", eAID_KeyboardMouse, EKeyId::eKI_MouseX);

	m_pInputComponent->RegisterAction("tank", "mouse_rotatepitch", [this](int activationMode, float value)
	{
		m_mouseDeltaRotation.y -= value;
	});
	m_pInputComponent->BindAction("tank", "mouse_rotatepitch", eAID_KeyboardMouse, EKeyId::eKI_MouseY);

	m_pInputComponent->RegisterAction("tank", "shoot", [this](int activationMode, float value)
	{
		if (activationMode == eAAM_OnPress)
		{
			FireMainCannon();
		}
	});
	m_pInputComponent->BindAction("tank", "shoot", eAID_KeyboardMouse, EKeyId::eKI_Mouse1);

	m_pInputComponent->RegisterAction("tank", "altshoot", [this](int activationMode, float value)
	{
		if (activationMode == eAAM_OnPress)
		{
			FireCoaxialGun();
		}
		else if (activationMode == eAAM_OnRelease)
		{
			StopCoaxialGun();
		}
	});
	m_pInputComponent->BindAction("tank", "altshoot", eAID_KeyboardMouse, EKeyId::eKI_Mouse2);
}

Cry::Entity::EventFlags CTankComponent::GetEventMask() const
{
	// Subscribe to lifecycle events so we can reset per-entity state and
	// properly spawn one-shot effects on level start / load.
    return 
		Cry::Entity::EEvent::BecomeLocalPlayer
		| Cry::Entity::EEvent::Update
		| ENTITY_EVENT_RESET
		| ENTITY_EVENT_LEVEL_LOADED
		| ENTITY_EVENT_START_GAME
		| ENTITY_EVENT_PHYSICAL_TYPE_CHANGED;
}

void CTankComponent::ProcessEvent(const SEntityEvent &event)
{
	switch (event.event)
	{

    case ENTITY_EVENT_RESET:
		// Reset is called on level unload/load � clear transient flags and
		// destroy/clear any emitters here if you cache them.
		m_engineEffectsSpawned = false;
		m_wheelsSetupDone = false;
		m_pendingWheelSetup = false;
		m_tankMovementInitialized = false;
		m_loggedWheelSetupDeferred = false;
		m_wheelPhysPartIds.clear();
		m_pVehiclePhysicsComponent = nullptr;
		break;

	case ENTITY_EVENT_PHYSICAL_TYPE_CHANGED:
		break;

    case ENTITY_EVENT_LEVEL_LOADED:
	case ENTITY_EVENT_START_GAME:
		// On level start/load we want to ensure one-shot engine effects are spawned
		// if engine is running and they haven't been spawned yet.
		if (m_isEngineRunning && !m_engineEffectsSpawned)
		{
			const Vec3 worldPos = GetEntity()->GetWorldPos();
			PlayEngineStartEffect(worldPos + GetEngineSmokePos(), Vec3(0, -1, 0));
			PlayEngineRunningEffect(worldPos + GetEngineSmokePos(), Vec3(0, -1, 0));
			m_engineEffectsSpawned = true;
		}
		break;

	case Cry::Entity::EEvent::Update:
	{
		const float deltaTime = event.fParam[0];
		ApplyLocalPlayerInput(deltaTime);
		//UpdateTankPhysics(deltaTime);
		DebugMass();

		// If engine was turned on after initialization, ensure engine effects spawn once
		if (m_isEngineRunning && !m_engineEffectsSpawned)
		{
			const Vec3 worldPos = GetEntity()->GetWorldPos();
			PlayEngineStartEffect(worldPos + GetEngineSmokePos(), Vec3(0, -1, 0));
			PlayEngineRunningEffect(worldPos + GetEngineSmokePos(), Vec3(0, -1, 0));
			m_engineEffectsSpawned = true;
		}
		break;
	}
	}
}

void CTankComponent::ApplyLocalPlayerInput(float)
{
	const float forwardInput = m_moveForwardPressed ? 1.0f : 0.0f;
	const float backInput = m_moveBackPressed ? 1.0f : 0.0f;
	const float steerLeftInput = m_moveLeftPressed ? 1.0f : 0.0f;
	const float steerRightInput = m_moveRightPressed ? 1.0f : 0.0f;

	if (forwardInput > 0.0f)
	{
		MoveForward(forwardInput);
	}
	else if (backInput > 0.0f)
	{
		MoveBackward(m_maxSpeed * backInput);
	}
	else
	{
		m_inputThrottle = 0.0f;
	}

	if (steerLeftInput > 0.0f && steerRightInput <= 0.0f)
	{
		SteerLeft(steerLeftInput);
	}
	else if (steerRightInput > 0.0f && steerLeftInput <= 0.0f)
	{
		SteerRight(steerRightInput);
	}
	else
	{
		m_inputSteer = 0.0f;
	}

	if (m_mouseDeltaRotation.x != 0.0f || m_mouseDeltaRotation.y != 0.0f)
	{
		const float turretYawSpeed = 0.15f;
		const float turretPitchSpeed = 0.10f;
		RotateTurret(m_mouseDeltaRotation.x * turretYawSpeed, m_mouseDeltaRotation.y * turretPitchSpeed);
		CryLogAlways("Mouse delta rotationX %.2f, rotation Y %.2f", m_mouseDeltaRotation.x, m_mouseDeltaRotation.y);
		m_mouseDeltaRotation = ZERO;
	}
}

void CTankComponent::SetMoveLeftActive(bool isActive)
{
	m_moveLeftPressed = isActive;
}

void CTankComponent::SetMoveRightActive(bool isActive)
{
	m_moveRightPressed = isActive;
}

void CTankComponent::SetMoveForwardActive(bool isActive)
{
	m_moveForwardPressed = isActive;
}

void CTankComponent::SetMoveBackActive(bool isActive)
{
	m_moveBackPressed = isActive;
}

// X input: forward throttle (0..1), positive only. Use MoveBackward for reverse movement.
void CTankComponent::MoveForward(float xInput)
{
	if (m_isDestroyed || !m_isEngineRunning)
		return;

	// Clamp throttle to [0, 1] range
	xInput = clamp_tpl(xInput, 0.0f, 1.0f);
	m_inputThrottle = xInput;
	m_inputSteer = 0.0f;
}

void CTankComponent::MoveBackward(float speed)
{
	if (m_isDestroyed || !m_isEngineRunning)
		return;

	// Reverse throttle is expressed as a negative fraction of max speed
	speed = clamp_tpl(speed, 0.0f, m_maxSpeed);
	m_inputThrottle = -speed / m_maxSpeed;
	m_inputSteer = 0.0f;
}

void CTankComponent::SteerLeft(float amount)
{
	if (m_isDestroyed || !m_isEngineRunning)
		return;

	amount = clamp_tpl(amount, 0.0f, 1.0f);
	m_inputSteer = -amount;
}

void CTankComponent::SteerRight(float amount)
{
	if (m_isDestroyed || !m_isEngineRunning)
		return;

	amount = clamp_tpl(amount, 0.0f, 1.0f);
	m_inputSteer = amount;
}

void CTankComponent::ApplyDriveAction(float throttle, float steer, bool handbrake)
{
	IPhysicalEntity* pPhysics = GetTankPhysics();
	if (!pPhysics)
		return;

	pe_action_drive driveAction;
	memset(&driveAction, 0, sizeof(driveAction));
	driveAction.pedal = throttle;
	driveAction.steer = steer;
	driveAction.bHandBrake = handbrake ? 1 : 0;

	pPhysics->Action(&driveAction, 1);
}

void CTankComponent::ApplyDamage(float damage, const Schematyc::CSharedString& component)
{
	string componentStr = component.c_str();
	if (componentStr == "hull")
		ApplyHullDamage(damage);
	else if (componentStr == "engine")
		ApplyEngineDamage(damage);
	else if (componentStr == "turret")
		ApplyTurretDamage(damage);
	else if (componentStr == "left_tread")
		ApplyLeftTreadDamage(damage);
	else if (componentStr == "right_tread")
		ApplyRightTreadDamage(damage);
}

void CTankComponent::ApplyHullDamage(float damage)
{
	m_hullCurrentDamage = min(m_hullDamageMax, m_hullCurrentDamage + damage);
	UpdateDamageState();
}

void CTankComponent::ApplyEngineDamage(float damage)
{
	m_engineCurrentDamage = min(m_engineDamageMax, m_engineCurrentDamage + damage);
	UpdateDamageState();
}

void CTankComponent::ApplyTurretDamage(float damage)
{
	m_turretCurrentDamage = min(m_turretDamageMax, m_turretCurrentDamage + damage);
	UpdateDamageState();
}

void CTankComponent::ApplyLeftTreadDamage(float damage)
{
	float prevDamage = m_leftTreadCurrentDamage;
	m_leftTreadCurrentDamage = min(m_leftTreadDamageMax, m_leftTreadCurrentDamage + damage);

	// Send tread destroyed event if tread was destroyed
	if (prevDamage < m_leftTreadDamageMax && m_leftTreadCurrentDamage >= m_leftTreadDamageMax)
	{
		OnTreadDestroyed(0);  // Left tread index
	}

	UpdateDamageState();
}

void CTankComponent::ApplyRightTreadDamage(float damage)
{
	float prevDamage = m_rightTreadCurrentDamage;
	m_rightTreadCurrentDamage = min(m_rightTreadDamageMax, m_rightTreadCurrentDamage + damage);

	// Send tread destroyed event if tread was destroyed
	if (prevDamage < m_rightTreadDamageMax && m_rightTreadCurrentDamage >= m_rightTreadDamageMax)
	{
		OnTreadDestroyed(1);  // Right tread index
	}

	UpdateDamageState();
}

void CTankComponent::RepairComponent(const Schematyc::CSharedString& component, float repairAmount)
{
	string componentStr = component.c_str();
	if (componentStr == "hull")
		m_hullCurrentDamage = max(0.0f, m_hullCurrentDamage - repairAmount);
	else if (componentStr == "engine")
		m_engineCurrentDamage = max(0.0f, m_engineCurrentDamage - repairAmount);
	else if (componentStr == "turret")
		m_turretCurrentDamage = max(0.0f, m_turretCurrentDamage - repairAmount);
	else if (componentStr == "left_tread")
	{
		float prevDamage = m_leftTreadCurrentDamage;
		m_leftTreadCurrentDamage = max(0.0f, m_leftTreadCurrentDamage - repairAmount);

		// Send tread repaired event if tread was repaired
		if (prevDamage >= m_leftTreadDamageMax && m_leftTreadCurrentDamage < m_leftTreadDamageMax)
		{
			OnTreadRepaired(0);  // Left tread index
		}
	}
	else if (componentStr == "right_tread")
	{
		float prevDamage = m_rightTreadCurrentDamage;
		m_rightTreadCurrentDamage = max(0.0f, m_rightTreadCurrentDamage - repairAmount);

		// Send tread repaired event if tread was repaired
		if (prevDamage >= m_rightTreadDamageMax && m_rightTreadCurrentDamage < m_rightTreadDamageMax)
		{
			OnTreadRepaired(1);  // Right tread index
		}
	}

	UpdateDamageState();
}

void CTankComponent::RotateTurret(float yaw, float pitch)
{
	if (m_isDestroyed)
		return;

	// Update turret rotation with limits
	m_currentTurretYaw += yaw;
	m_currentTurretPitch += pitch;

	// Apply pitch limits (from Abrams tank specs)
	m_currentTurretPitch = clamp_tpl(m_currentTurretPitch, m_minTurretPitch, m_maxTurretPitch);

	// Play turret turning audio
	IEntityAudioComponent* pAudio = GetAudioProxy();
	if (pAudio && m_audioTurretTurnTriggerId != CryAudio::InvalidControlId)
	{
		pAudio->ExecuteTrigger(m_audioTurretTurnTriggerId);
	}

	// Update turret rotation speed for RTPC
	if (pAudio && m_audioTurretRotationSpeedParamId != CryAudio::InvalidControlId && fabsf(yaw) > 0.001f)
	{
		float rotationSpeedRatio = clamp_tpl(fabsf(yaw) / m_rotationSpeed, 0.0f, 1.0f);
		pAudio->SetParameter(m_audioTurretRotationSpeedParamId, rotationSpeedRatio);
	}

	// Log turret rotation
	if (gEnv && gEnv->pSystem && gEnv->pSystem->GetILog())
	{
		CryLog("Turret rotated - Yaw: %.1f, Pitch: %.1f", m_currentTurretYaw, m_currentTurretPitch);
	}
}

void CTankComponent::FireMainCannon()
{
	if (m_isDestroyed || m_mainCannonCooldown > 0.0f)
		return;

	// Fire main cannon
	m_mainCannonCooldown = m_mainCannonReloadTime;

	// Spawn projectile
	SpawnMainCannonProjectile();

	// Play main cannon fire audio
	IEntityAudioComponent* pAudio = GetAudioProxy();
	if (pAudio && m_audioMainCannonTriggerId != CryAudio::InvalidControlId)
	{
		pAudio->ExecuteTrigger(m_audioMainCannonTriggerId);
	}

	// Log firing
	if (gEnv && gEnv->pSystem && gEnv->pSystem->GetILog())
	{
		CryLog("Tank main cannon fired!");
	}
}

void CTankComponent::FireCoaxialGun()
{
	if (m_isDestroyed || m_coaxialGunCooldown > 0.0f)
		return;

	// Fire coaxial machine gun
	m_coaxialGunCooldown = m_coaxialGunReloadTime;

	// Spawn projectile
	SpawnCoaxialGunProjectile();

	// Play coaxial gun fire audio
	IEntityAudioComponent* pAudio = GetAudioProxy();
	if (pAudio && m_audioCoaxialGunTriggerId != CryAudio::InvalidControlId)
	{
		pAudio->ExecuteTrigger(m_audioCoaxialGunTriggerId);
	}

	// Log firing
	if (gEnv && gEnv->pSystem && gEnv->pSystem->GetILog())
	{
		CryLog("Tank coaxial gun fired!");
	}
}

void CTankComponent::StopCoaxialGun()
{
	// Stop coaxial gun fire audio
	IEntityAudioComponent* pAudio = GetAudioProxy();
	if (pAudio && m_audioCoaxialGunStopTriggerId != CryAudio::InvalidControlId)
	{
		pAudio->ExecuteTrigger(m_audioCoaxialGunStopTriggerId);
	}

	// Log stopping
	if (gEnv && gEnv->pSystem && gEnv->pSystem->GetILog())
	{
		CryLog("Tank coaxial gun stopped!");
	}
}

void CTankComponent::SpawnMainCannonProjectile()
{
	if (m_isDestroyed)
		return;

	// Get muzzle position in world space
	const Matrix34 tankTM = GetEntity()->GetWorldTM();
	Vec3 muzzleWorldPos = tankTM.TransformPoint(m_cannonOutPos);
	
	// Get forward direction from tank's current turret rotation
	const Quat turretRot = GetEntity()->GetWorldRotation();
	Vec3 projectileDirection = turretRot.GetColumn1();  // Forward direction
	
	// Apply turret pitch to direction
	float pitchRad = DEG2RAD(m_currentTurretPitch);
	projectileDirection.z += sinf(pitchRad);
	projectileDirection.Normalize();
	
	// Projectile velocity (m/s) - typical tank cannon muzzle velocity
	float projectileSpeed = 900.0f;
	Vec3 projectileVelocity = projectileDirection * projectileSpeed;
	
	// Spawn projectile entity
	SEntitySpawnParams spawnParams;
	spawnParams.pClass = gEnv->pEntitySystem->GetClassRegistry()->GetDefaultClass();
	spawnParams.vPosition = muzzleWorldPos;
	spawnParams.qRotation = Quat(Matrix33::CreateRotationVDir(projectileDirection));
	spawnParams.sName = "TankCannonProjectile";
	spawnParams.nFlags |= ENTITY_FLAG_NO_SAVE;  // Don't save in editor
	
	if (IEntity* pProjectile = gEnv->pEntitySystem->SpawnEntity(spawnParams))
	{
		// Setup physics
		SEntityPhysicalizeParams physParams;
		physParams.type = PE_RIGID;
		physParams.mass = 50.0f;  // Projectile mass in kg
		physParams.nSlot = 0;
		pProjectile->Physicalize(physParams);
		
		// Set velocity
		if (IPhysicalEntity* pPhysEntity = pProjectile->GetPhysics())
		{
			pe_action_set_velocity velAction;
			velAction.v = projectileVelocity;
			pPhysEntity->Action(&velAction);
			
			// Enable collision and post-step callbacks
			pe_params_flags flags;
			flags.flagsOR = pef_log_collisions | pef_monitor_poststep;
			pPhysEntity->SetParams(&flags);
		}
		
		if (gEnv && gEnv->pSystem && gEnv->pSystem->GetILog())
		{
			CryLog("Main cannon projectile spawned at %.2f, %.2f, %.2f with velocity %.2f m/s (EntityID: %u)", 
										  muzzleWorldPos.x, muzzleWorldPos.y, muzzleWorldPos.z, projectileSpeed, pProjectile->GetId());
		}
	}
	
	// Play muzzle flash effect
	PlayMuzzleFlashEffect(muzzleWorldPos, projectileDirection);
}

void CTankComponent::SpawnCoaxialGunProjectile()
{
	if (m_isDestroyed)
		return;

	// Get muzzle position in world space
	const Matrix34 tankTM = GetEntity()->GetWorldTM();
	Vec3 muzzleWorldPos = tankTM.TransformPoint(m_coaxOutPos);
	
	// Get forward direction from tank's current turret rotation
	const Quat turretRot = GetEntity()->GetWorldRotation();
	Vec3 projectileDirection = turretRot.GetColumn1();  // Forward direction
	
	// Apply turret pitch to direction
	float pitchRad = DEG2RAD(m_currentTurretPitch);
	projectileDirection.z += sinf(pitchRad);
	projectileDirection.Normalize();
	
	// Coaxial gun velocity (m/s) - machine gun rounds slower than cannon
	float projectileSpeed = 850.0f;  // 7.62mm machine gun muzzle velocity
	Vec3 projectileVelocity = projectileDirection * projectileSpeed;
	
	// Spawn projectile entity
	SEntitySpawnParams spawnParams;
	spawnParams.pClass = gEnv->pEntitySystem->GetClassRegistry()->GetDefaultClass();
	spawnParams.vPosition = muzzleWorldPos;
	spawnParams.qRotation = Quat(Matrix33::CreateRotationVDir(projectileDirection));
	spawnParams.sName = "TankCoaxialProjectile";
	spawnParams.nFlags |= ENTITY_FLAG_NO_SAVE;  // Don't save in editor
	
	if (IEntity* pProjectile = gEnv->pEntitySystem->SpawnEntity(spawnParams))
	{
		// Setup physics
		SEntityPhysicalizeParams physParams;
		physParams.type = PE_RIGID;
		physParams.mass = 10.0f;  // Machine gun round mass in kg (lighter than cannon)
		physParams.nSlot = 0;
		pProjectile->Physicalize(physParams);
		
		// Set velocity
		if (IPhysicalEntity* pPhysEntity = pProjectile->GetPhysics())
		{
			pe_action_set_velocity velAction;
			velAction.v = projectileVelocity;
			pPhysEntity->Action(&velAction);
			
			// Enable collision and post-step callbacks
			pe_params_flags flags;
			flags.flagsOR = pef_log_collisions | pef_monitor_poststep;
			pPhysEntity->SetParams(&flags);
		}
		
		if (gEnv && gEnv->pSystem && gEnv->pSystem->GetILog())
		{
			CryLog("Coaxial gun projectile spawned at %.2f, %.2f, %.2f with velocity %.2f m/s (EntityID: %u)", 
										  muzzleWorldPos.x, muzzleWorldPos.y, muzzleWorldPos.z, projectileSpeed, pProjectile->GetId());
		}
	}
	
	// Play muzzle flash effect
	PlayMuzzleFlashEffect(muzzleWorldPos, projectileDirection);
}

void CTankComponent::PlayMuzzleFlashEffect(const Vec3& position, const Vec3& direction)
{
	// Spawn muzzle flash particle effect using GameSDK patterns
	// Pattern from: CRYENGINE_Source/Code/GameSDK/GameDll/MuzzleEffect.cpp
	
	if (!gEnv || !gEnv->pParticleManager)
		return;
	
	// Try to spawn a generic muzzle flash particle effect
	// Using commonly available effect names from GameSDK
	const char* muzzleFlashEffects[] = 
	{
		"Weapons.Muzzle.Flashes.LargeCaliberDefault",  // Main cannon - large caliber
		"Weapons.Muzzle.Flashes.Assault",               // Fallback
	};
	
	// Try each effect until one is found
	for (const char* effectName : muzzleFlashEffects)
	{
		IParticleEffect* pEffect = gEnv->pParticleManager->FindEffect(effectName);
		if (pEffect)
		{
			// Spawn particle effect at muzzle position with direction orientation
			// Pattern from: CRYENGINE_Source/Code/GameSDK/GameDll/Projectile.cpp line 757
			pEffect->Spawn(true, IParticleEffect::ParticleLoc(position, direction, 1.0f));
			
			if (gEnv && gEnv->pSystem && gEnv->pSystem->GetILog())
			{
				CryLog("[Tank] Muzzle flash effect spawned at (%.2f, %.2f, %.2f) - Effect: %s", 
										  position.x, position.y, position.z, effectName);
			}
			return;
		}
	}
	
	// Fallback: log if no particle effects found
	if (gEnv && gEnv->pSystem && gEnv->pSystem->GetILog())
	{
		CryLog("[Tank] Muzzle flash - No particle effect found, spawning at (%.2f, %.2f, %.2f)", 
									  position.x, position.y, position.z);
	}
}

void CTankComponent::PlayImpactEffect(const Vec3& position, const Vec3& normal)
{
	// Spawn impact particle effect using GameSDK patterns
	// Pattern from: CRYENGINE_Source/Code/GameSDK/GameDll/Projectile.cpp lines 750-758
	// This follows the exact pattern used by CProjectile for collision effects
	
	if (!gEnv || !gEnv->pParticleManager)
		return;
	
	// Try different impact effects based on surface type
	// These are commonly available in GameSDK
	const char* impactEffects[] = 
	{
		"Collisions.projectile.impact.default",  // Default impact
		"Weapons.ImpactEffects.Impact_GroundSmall", // Small ground impact
		"Weapons.ImpactEffects.Impact_Default",  // Generic impact
	};
	
	// Try each effect until one is found
	for (const char* effectName : impactEffects)
	{
		IParticleEffect* pEffect = gEnv->pParticleManager->FindEffect(effectName);
		if (pEffect)
		{
			// Spawn impact effect at collision point with surface normal
			// Scale factor 1.0f for normal projectile impacts
			pEffect->Spawn(true, IParticleEffect::ParticleLoc(position, normal, 1.0f));
			
			if (gEnv && gEnv->pSystem && gEnv->pSystem->GetILog())
			{
				CryLog("[Tank] Impact effect spawned at (%.2f, %.2f, %.2f) with normal (%.2f, %.2f, %.2f) - Effect: %s", 
										  position.x, position.y, position.z, normal.x, normal.y, normal.z, effectName);
			}
			return;
		}
	}
	
	// Fallback: log if no particle effects found
	if (gEnv && gEnv->pSystem && gEnv->pSystem->GetILog())
	{
		CryLog("[Tank] Impact effect - No particle effect found at (%.2f, %.2f, %.2f) with normal (%.2f, %.2f, %.2f)", 
									  position.x, position.y, position.z, normal.x, normal.y, normal.z);
	}
}

void CTankComponent::PlayParticleInternal(const string& effectName, const Vec3& position, const Vec3& direction)
{
	if (!SpawnParticleEffect(effectName.c_str(), position, direction))
	{
		if (gEnv && gEnv->pSystem && gEnv->pSystem->GetILog())
		{
			CryLog("[Particle Effect] Particle effect not found: %s", effectName.c_str());
		}
	}
}

void CTankComponent::PlayParticle(const Schematyc::CSharedString& effectName, const Vec3& position, const Vec3& direction)
{
	PlayParticleInternal(string(effectName.c_str()), position, direction);
}

void CTankComponent::PlayEngineStartEffect(const Vec3& position, const Vec3& direction)
{
	PlayParticleInternal(m_engineStartEffect, position, direction);
}

void CTankComponent::PlayEngineStopEffect(const Vec3& position, const Vec3& direction)
{
	PlayParticleInternal(m_engineStopEffect, position, direction);
}

void CTankComponent::PlayEngineRunningEffect(const Vec3& position, const Vec3& direction)
{
	PlayParticleInternal(m_engineRunningEffect, position, direction);
}

void CTankComponent::PlayBoostEffect(const Vec3& position, const Vec3& direction)
{
	PlayParticleInternal(m_engineBoostEffect, position, direction);
}

void CTankComponent::PlayDamageEffectInternal(const string& effectName, const Vec3& position, const Vec3& direction)
{
	PlayParticleInternal(effectName, position, direction);
}

void CTankComponent::PlayDamageEffect(const Schematyc::CSharedString& effectName, const Vec3& position, const Vec3& direction)
{
	PlayDamageEffectInternal(string(effectName.c_str()), position, direction);
}

// Update functions
void CTankComponent::UpdateTankPhysics(float deltaTime)
{
	UpdateWeaponCooldowns(deltaTime);
	ProcessMovement(deltaTime);

	IPhysicalEntity* pPhysics = GetTankPhysics();
	if (pPhysics)
	{
		pe_status_dynamics dyn;
		if (pPhysics->GetStatus(&dyn) != 0)
		{
			const Quat entityRot = GetEntity()->GetWorldRotation();
			const Vec3 forwardDir = entityRot.GetColumn1();
			m_currentSpeed = forwardDir.Dot(dyn.v);
		}

	}
	else
	{
		m_currentSpeed = 0.0f;
	}

	// Audio parameter updates for vehicle movement
	IEntityAudioComponent* pAudio = GetAudioProxy();
	if (pAudio)
	{
		if (m_isEngineRunning && !m_isEngineSoundPlaying && m_audioEngineRunTriggerId != CryAudio::InvalidControlId)
		{
			pAudio->ExecuteTrigger(m_audioEngineRunTriggerId);
			m_isEngineSoundPlaying = true;
		}
		else if (!m_isEngineRunning && m_isEngineSoundPlaying && m_audioEngineStopTriggerId != CryAudio::InvalidControlId)
		{
			pAudio->ExecuteTrigger(m_audioEngineStopTriggerId);
			m_isEngineSoundPlaying = false;
		}

		if (m_audioSpeedParamId != CryAudio::InvalidControlId)
		{
			float speedRatio = clamp_tpl(fabsf(m_currentSpeed) / m_maxSpeed, 0.0f, 1.0f);
			pAudio->SetParameter(m_audioSpeedParamId, speedRatio);
		}
	}
}

void CTankComponent::UpdateWeaponCooldowns(float deltaTime)
{
	// Update main cannon cooldown
	if (m_mainCannonCooldown > 0.0f)
	{
		m_mainCannonCooldown -= deltaTime;
		if (m_mainCannonCooldown < 0.0f)
			m_mainCannonCooldown = 0.0f;
	}

	// Update coaxial gun cooldown
	if (m_coaxialGunCooldown > 0.0f)
	{
		m_coaxialGunCooldown -= deltaTime;
		if (m_coaxialGunCooldown < 0.0f)
			m_coaxialGunCooldown = 0.0f;
	}
}

void CTankComponent::OnTreadDestroyed(int treadIndex)
{
	// Handle tread destruction - reduce speed based on number of destroyed treads
	m_blownTires = min(2, m_blownTires + 1);  // Max 2 treads for a tank
	SetEngineRPMMult(GetWheelCondition());
}

void CTankComponent::OnTreadRepaired(int treadIndex)
{
	// Handle tread repair - restore speed
	m_blownTires = max(0, m_blownTires - 1);
	SetEngineRPMMult(GetWheelCondition());
}

void CTankComponent::SetEngineRPMMult(float mult)
{
	m_damageRPMScale = mult;
}

void CTankComponent::UpdateDamageState()
{
	// Update overall damage state based on component damage
	float totalDamage = (m_hullCurrentDamage + m_engineCurrentDamage + m_turretCurrentDamage + 
						m_leftTreadCurrentDamage + m_rightTreadCurrentDamage);
	float maxTotalDamage = (m_hullDamageMax + m_engineDamageMax + m_turretDamageMax + 
						   m_leftTreadDamageMax + m_rightTreadDamageMax);
	
	if (maxTotalDamage > 0.0f)
	{
		float damageRatio = totalDamage / maxTotalDamage;
		m_isDestroyed = (damageRatio >= 1.0f);
		
		// Stop engine if heavily damaged
		if (damageRatio >= 0.8f && m_isEngineRunning)
		{
			m_isEngineRunning = false;
			IEntityAudioComponent* pAudio = GetAudioProxy();
			if (pAudio && m_audioEngineStopTriggerId != CryAudio::InvalidControlId)
			{
				pAudio->ExecuteTrigger(m_audioEngineStopTriggerId);
				m_isEngineSoundPlaying = false;
			}
		}
	}
}

void CTankComponent::UpdateAxleFriction(float actionPedal, bool isMoving, float deltaTime)
{
	// Placeholder for axle friction distribution
	// GameSDK manages grip distribution across wheels based on load
	// For basic implementation, friction is uniform (handled in SetLatFriction)
}

void CTankComponent::UpdateSuspension(float deltaTime)
{
	// Placeholder for suspension spring compression
	// Physics engine handles this automatically with PE_WHEELEDVEHICLE
}

void CTankComponent::UpdateSounds(float deltaTime)
{
	IEntityAudioComponent* pAudio = GetAudioProxy();
	if (!pAudio)
		return;

	// Engine sound (already handled in UpdateTankPhysics)
	
	// Speed ratio for engine pitch
	UpdateSpeedRatio(deltaTime);
}

void CTankComponent::UpdateSpeedRatio(float deltaTime)
{
	// Update audio parameter for engine pitch based on speed
	if (m_audioSpeedParamId != CryAudio::InvalidControlId)
	{
		float speedRatio = clamp_tpl(fabsf(m_currentSpeed) / m_maxSpeed, 0.0f, 1.0f);
		IEntityAudioComponent* pAudio = GetAudioProxy();
		if (pAudio)
		{
			pAudio->SetParameter(m_audioSpeedParamId, speedRatio);
		}
	}
}

void CTankComponent::ApplyAirDamp(float pitchDamp, float rollDamp, float deltaTime, int threadSafe)
{
	// Apply damping when airborne to reduce flipping
	IPhysicalEntity* pPhysics = GetTankPhysics();
	if (!pPhysics)
		return;

	pe_status_dynamics dyn;
	if (!pPhysics->GetStatus(&dyn))
		return;

	Vec3 localW = m_worldTM.GetInvertedFast().TransformVector(dyn.w);
	
	pe_action_impulse imp;
	imp.iApplyTime = 0;
	
	// Dampen pitch and roll rotations
	float pitchCorr = -localW.x * pitchDamp * dyn.mass * deltaTime;
	float rollCorr = -localW.z * rollDamp * dyn.mass * deltaTime;
	
	imp.angImpulse = m_worldTM.GetColumn1() * pitchCorr + m_worldTM.GetColumn2() * rollCorr;
	pPhysics->Action(&imp, threadSafe);
}

void CTankComponent::ApplyBoost(float speed, float maxSpeed, float strength, float deltaTime)
{
	// Placeholder for boost system
	// Not implemented in current scope
}

void CTankComponent::ProcessMovementFallback(float deltaTime)
{
	IPhysicalEntity* pPhysics = GetTankPhysics();
	if (!pPhysics)
		return;

	const float throttleInput = clamp_tpl(m_inputThrottle, -1.0f, 1.0f);
	const float steerInput = clamp_tpl(m_inputSteer, -1.0f, 1.0f);

	m_currentPedal = throttleInput;
	m_currentSteer = steerInput;

	const float forwardSpeed = throttleInput * m_maxSpeed;
	const float yawDeltaRadians = DEG2RAD(m_rotationSpeed * steerInput * deltaTime);

	if (fabs_tpl(yawDeltaRadians) > 0.0001f)
	{
		Quat worldRotation = GetEntity()->GetWorldRotation();
		const Quat yawRotation = Quat::CreateRotationZ(yawDeltaRadians);
		worldRotation = worldRotation * yawRotation;
		GetEntity()->SetRotation(worldRotation.GetNormalized());
	}

	const Vec3 forwardDir = GetEntity()->GetWorldRotation().GetColumn1();
	const Vec3 desiredVelocity = forwardDir * forwardSpeed;

	pe_action_set_velocity velocityAction;
	velocityAction.v = desiredVelocity;
	pPhysics->Action(&velocityAction, 1);

	m_currentSpeed = forwardSpeed;
}

void CTankComponent::ProcessMovementAdvanced(float deltaTime)
{
	pe_action_drive m_actionNow{};
	// GameSDK-style ProcessMovement with full featured steering/friction logic
	if (!m_isEngineRunning || m_isDestroyed)
	{
		m_currentPedal = 0.0f;
		return;
	}

	IPhysicalEntity* pPhysics = GetTankPhysics();
	m_vehicleStatus = pe_status_vehicle();
	if (!pPhysics || !pPhysics->GetStatus(&m_vehicleStatus))
		return;

	// Update world TM for local-to-world conversions
	m_worldTM = Matrix34(GetEntity()->GetWorldRotation());
	m_worldTM.AddTranslation(GetEntity()->GetWorldPos());
	Matrix34 invWTM = m_worldTM.GetInvertedFast();

	pe_status_dynamics dyn;
	if (pPhysics->GetStatus(&dyn))
	{
		m_localVelocity = invWTM.TransformVector(dyn.v);
		m_localAngularVelocity = invWTM.TransformVector(dyn.w);
	}

	// debug
	// ===== ADVANCED VEHICLE DIAGNOSTICS =====
	if (gEnv && gEnv->pSystem && gEnv->pSystem->GetILog())
	{
		pe_status_dynamics dyn;
		if (pPhysics->GetStatus(&dyn))
		{
			// We know from your initialization logs that the tank has exactly 18 wheels
			const int totalWheels = 18;
			int activeContacts = 0;

			for (int i = 0; i < totalWheels; ++i)
			{
				pe_status_wheel ws;
				ws.iWheel = i;
				// Query the status of each individual wheel slot
				if (pPhysics->GetStatus(&ws) && ws.bContact)
				{
					activeContacts++;
				}
			}

			CryLogAlways("[Tank Diagnostics] Mass: %.1f | Velocity: %.2f | Engine RPM: %.1f | Current Gear: %d | Active Ground Contacts: %d/18",
				dyn.mass,
				dyn.v.len(),
				m_vehicleStatus.engineRPM,
				m_vehicleStatus.iCurGear,
				activeContacts
			);

			// Let's look closely at the very first road wheel (Axle 0)
			pe_status_wheel firstWheelStatus;
			firstWheelStatus.iWheel = 0;
			if (pPhysics->GetStatus(&firstWheelStatus))
			{
				CryLogAlways("[Tank Diagnostics] Wheel 0 -> Suspension Len: %.3f | Contact Normal Z: %.3f | Torque Angular Vel: %.2f",
					firstWheelStatus.suspLen,
					firstWheelStatus.normContact, // Ground surface normal vector pointing up
					firstWheelStatus.w    // Actual rotational spin speed of the wheel proxy
				);
			}
		}
	}
	//

	float speed = dyn.v.len();
	float speedRatio = min(1.f, speed / m_maxSpeed);

	// ===== INPUT PROCESSING =====
	float actionPedal = m_inputThrottle;
	float actionSteer = m_inputSteer;
	float absSteer = abs(actionSteer);

	// Steering ramping with dead zone
	float steerSpeed = (absSteer < 0.01f && abs(m_currentSteer) > 0.01f) ? m_steerSpeedRelax : m_steerSpeed;
	
	if (steerSpeed == 0.f)
	{
		m_currentSteer = (float)ClampSign(actionSteer);
	}
	else
	{
		if (m_isAI)
		{
			m_currentSteer = actionSteer;
		}
		else
		{
			m_currentSteer += min(abs(actionSteer - m_currentSteer), deltaTime * steerSpeed) * ClampSign(actionSteer - m_currentSteer);
		}
	}
	Limit(m_currentSteer, -m_steerLimit, m_steerLimit);

	// Steering forces full throttle
	if (abs(m_currentSteer) > 0.0001f)
	{
		actionPedal = (float)ClampSign(actionPedal);
		
		if (actionPedal == 0.f)
		{
			// Allow steering-on-spot only above max reverse speed
			const float maxReverseSpeed = -1.5f;
			actionPedal = max(0.f, min(1.f, 1.f - (m_localVelocity.y / maxReverseSpeed)));
		}
	}

	int currGear = m_vehicleStatus.iCurGear - 1;

	// ===== PEDAL RAMPING =====
	UpdateAxleFriction(actionPedal, true, deltaTime);
	UpdateSuspension(deltaTime);

	float absPedal = abs(actionPedal);

	if (m_pedalSpeed == 0.f)
	{
		m_currentPedal = actionPedal;
	}
	else
	{
		m_currentPedal += deltaTime * m_pedalSpeed * ClampSign(actionPedal - m_currentPedal);
		m_currentPedal = clamp_tpl(m_currentPedal, -absPedal, absPedal);
	}

	// Pedal threshold (only in neutral)
	if (currGear == 0 && fabsf(m_currentPedal) < m_pedalThreshold)
	{
		m_actionNow.pedal = 0;
	}
	else
	{
		m_actionNow.pedal = m_currentPedal;
	}

	// ===== DAMAGE MULTIPLIER =====
	float damageMul = 0.0f;
	{
		if (m_isAI)
		{
			damageMul = 1.0f - 0.30f * m_damage;
			m_actionNow.pedal *= damageMul;
		}
		else
		{
			// Damage affects forward less than reverse
			float effectiveDamage = m_damage;
			if (m_actionNow.pedal < -0.1f)
				effectiveDamage = 0.4f * m_damage;

			m_actionNow.pedal *= GetWheelCondition();
			damageMul = 1.0f - 0.7f * effectiveDamage;
			m_actionNow.pedal *= damageMul;
		}
	}

	// Reverse steering for backward driving
	float effSteer = m_currentSteer * ClampSign(actionPedal);

	// ===== LATERAL FRICTION UPDATE =====
	float latSlipMinGoal = 0.f;
	float latFricMinGoal = m_latFrictionMin;

	if (abs(effSteer) > 0.01f && !m_isAI)  // No brake simulation here
	{
		latSlipMinGoal = m_latSlipMin;
		
		// Use steering friction if not countersteering
		if (ClampSign(effSteer) != ClampSign(m_localAngularVelocity.z))
			latFricMinGoal = m_latFrictionMinSteer;
	}

	// Smooth slip min transition
	m_currentSlipMin = LerpFloat(m_currentSlipMin, latSlipMinGoal, 3.0f, deltaTime);

	// Smooth friction min transition
	if (latFricMinGoal < m_currentFricMin)
	{
		m_currentFricMin = latFricMinGoal;
	}
	else
	{
		m_currentFricMin = LerpFloat(m_currentFricMin, latFricMinGoal, 3.0f, deltaTime);
	}

	// Friction based on slip fraction
	float fractionSpeed = min(1.f, max(0.f, m_avgLateralSlip - m_currentSlipMin) / (m_latSlipMax - m_currentSlipMin));
	float latFric = fractionSpeed * (m_latFrictionMax - m_currentFricMin) + m_currentFricMin;

	if (latFric != m_currentLatFriction)
	{
		SetLatFriction(latFric);
	}

	// ===== STEERING ANGLE =====
	const static float maxSteer = gf_PI / 4.f;
	m_actionNow.steer = m_currentSteer * maxSteer;

	// ===== STEERING IMPULSE (OPTIONAL) =====
	if (m_steeringImpulseMin > 0.f && m_wheelContactsLeft != 0 && m_wheelContactsRight != 0)
	{
		const float maxW = 0.3f * gf_PI;
		float steer = abs(m_currentSteer) > 0.001f ? m_currentSteer : 0.f;
		float desired = steer * maxW;
		float curr = -m_localAngularVelocity.z;
		float err = desired - curr;
		Limit(err, -maxW, maxW);

		if (abs(err) > 0.01f)
		{
			float amount = m_steeringImpulseMin + speedRatio * (m_steeringImpulseMax - m_steeringImpulseMin);

			if (desired == 0.f || (desired * curr > 0 && abs(desired) < abs(curr)))
				amount = m_steeringImpulseRelaxMin + speedRatio * (m_steeringImpulseRelaxMax - m_steeringImpulseRelaxMin);

			float corr = -err * amount * dyn.mass * deltaTime;

			pe_action_impulse imp;
			imp.iApplyTime = 0;
			imp.angImpulse = m_worldTM.GetColumn2() * corr;
			pPhysics->Action(&imp, 1);
		}
	}

	m_actionNow.bHandBrake = 0;
	m_actionNow.iGear = 10;  // GameSDK handles gear shifting internally, so we can set this to a default value
	//if (gEnv && gEnv->pSystem && gEnv->pSystem->GetILog())
	//{
	//	CryLogAlways("[Tank Physics Update] Entity: %s | Pedal: %.3f | Steer: %.3f | Gear: %d  | Handbrake: %d",
	//		GetEntity()->GetName(),
	//		m_actionNow.pedal,
	//		m_actionNow.steer,
	//		m_actionNow.iGear,
	//		m_actionNow.bHandBrake);
	//}
	pPhysics->Action(&m_actionNow, 1);

	// Air dynamics
	if (m_wheelContacts <= 1 && speed > 5.f)
	{
		ApplyAirDamp(DEG2RAD(20.f), DEG2RAD(10.f), deltaTime, 1);
	}
}

