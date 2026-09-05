// Copyright 2016-2020 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryEntitySystem/IEntityComponent.h>
#include <CryEntitySystem/IEntity.h>
#include <CryMath/Cry_Geo.h>
#include <CryMath/Cry_Color.h>
#include <CryMath/Cry_Math.h>
#include <CryAudio/IAudioSystem.h>
#include <CrySchematyc/Utils/SharedString.h>
#include <CryCore/Containers/CryArray.h>
#include <DefaultComponents/Input/InputComponent.h>
#include <DefaultComponents/Physics/Vehicles/VehicleComponent.h>
#include <vector>

struct ICharacterInstance;

#include <CrySchematyc/Env/IEnvRegistry.h>
#include <CrySchematyc/Env/IEnvRegistrar.h>
#include <CrySchematyc/Env/Elements/EnvComponent.h>
#include <CryGame/IGameFramework.h>


class CTankComponent final : public IEntityComponent
{

public:
	CTankComponent() = default;
	virtual ~CTankComponent() override = default;

	static void ReflectType(Schematyc::CTypeDesc<CTankComponent> &desc);
	static void Register(Schematyc::CEnvRegistrationScope &componentScope);

	// IEntityComponent
	virtual void Initialize() override;
	virtual Cry::Entity::EventFlags GetEventMask() const override;
	virtual void ProcessEvent(const SEntityEvent &event) override;
	// ~IEntityComponent

	void InitializeLocalPlayer();

	// Tank movement functions (exposed to Schematyc)
	void MoveForward(float speed);
	void MoveBackward(float speed);
	void SteerLeft(float amount);
	void SteerRight(float amount);
	void RotateTurret(float yaw, float pitch);
	void FireMainCannon();
	void FireCoaxialGun();
	void StopCoaxialGun();
	
	// Projectile spawning methods
	void SpawnMainCannonProjectile();
	void SpawnCoaxialGunProjectile();
	
	// Particle effect methods
	void PlayMuzzleFlashEffect(const Vec3& position, const Vec3& direction);
	void PlayImpactEffect(const Vec3& position, const Vec3& normal);
	void PlayParticleInternal(const string& effectName, const Vec3& position, const Vec3& direction = Vec3(0.0f, 0.0f, 1.0f));
	void PlayParticle(const Schematyc::CSharedString& effectName, const Vec3& position, const Vec3& direction = Vec3(0.0f, 0.0f, 1.0f));
	void PlayEngineStartEffect(const Vec3& position, const Vec3& direction);
	void PlayEngineStopEffect(const Vec3& position, const Vec3& direction);
	void PlayEngineRunningEffect(const Vec3& position, const Vec3& direction);
	void PlayBoostEffect(const Vec3& position, const Vec3& direction);
	void PlayDamageEffectInternal(const string& effectName, const Vec3& position, const Vec3& direction = Vec3(0.0f, 0.0f, 1.0f));
	void PlayDamageEffect(const Schematyc::CSharedString& effectName, const Vec3& position, const Vec3& direction = Vec3(0.0f, 0.0f, 1.0f));

	// Damage application methods (exposed to Schematyc)
	void ApplyDamage(float damage, const Schematyc::CSharedString& component);
	void ApplyHullDamage(float damage);
	void ApplyEngineDamage(float damage);
	void ApplyTurretDamage(float damage);
	void ApplyLeftTreadDamage(float damage);
	void ApplyRightTreadDamage(float damage);
	void RepairComponent(const Schematyc::CSharedString& component, float repairAmount);
	float GetCurrentSpeed() const { return m_currentSpeed; }
	float GetCurrentTurretYaw() const { return m_currentTurretYaw; }
	float GetCurrentTurretPitch() const { return m_currentTurretPitch; }
	bool IsEngineRunning() const { return m_isEngineRunning; }
	bool IsDestroyed() const { return m_isDestroyed; }
	float GetMainCannonCooldown() const { return m_mainCannonCooldown; }
	float GetCoaxialGunCooldown() const { return m_coaxialGunCooldown; }

	// Mesh part query functions
	string GetBodyMesh() const { return m_bodyMesh; }
	string GetLeftTreadMesh() const { return m_leftTreadMesh; }
	string GetRightTreadMesh() const { return m_rightTreadMesh; }

	// Helper position query functions
	Vec3 GetCannonMuzzlePos() const { return m_cannonOutPos; }
	Vec3 GetTurretCenter() const { return m_turretPos; }
	Vec3 GetEngineSmokePos() const { return m_engineSmokeOutPos; }
	Vec3 GetDriverViewPos() const { return m_driverViewPos; }
	Vec3 GetGunnerViewPos() const { return m_gunnerViewPos; }

	// Component health query functions (0-100 percentage)
	float GetHullHealth() const { return (m_hullDamageMax > 0.0f) ? ((m_hullDamageMax - m_hullCurrentDamage) / m_hullDamageMax) * 100.0f : 100.0f; }
	float GetEngineHealth() const { return (m_engineDamageMax > 0.0f) ? ((m_engineDamageMax - m_engineCurrentDamage) / m_engineDamageMax) * 100.0f : 100.0f; }
	float GetTurretHealth() const { return (m_turretDamageMax > 0.0f) ? ((m_turretDamageMax - m_turretCurrentDamage) / m_turretDamageMax) * 100.0f : 100.0f; }
	float GetLeftTreadHealth() const { return (m_leftTreadDamageMax > 0.0f) ? ((m_leftTreadDamageMax - m_leftTreadCurrentDamage) / m_leftTreadDamageMax) * 100.0f : 100.0f; }
	float GetRightTreadHealth() const { return (m_rightTreadDamageMax > 0.0f) ? ((m_rightTreadDamageMax - m_rightTreadCurrentDamage) / m_rightTreadDamageMax) * 100.0f : 100.0f; }

	// Tank properties (exposed to Schematyc editor)
	float m_maxSpeed = 20.0f;					 // m/s - based on Abrams tank specs
	float m_rotationSpeed = 40.0f;				 // degrees per second
	float m_minTurretPitch = -10.0f;			 // degrees - from Abrams config
	float m_maxTurretPitch = 35.0f;				 // degrees - from Abrams config
	float m_mainCannonReloadTime = 2.0f;		 // seconds
	float m_coaxialGunReloadTime = 0.1f;		 // seconds
	float m_additionalSteeringStationary = 0.5f; // Additional steering when stationary
	float m_additionalSteeringAtMaxSpeed = 0.2f; // Additional steering at max speed
	float m_additionalTilt = 0.1f;				 // Additional tilt for tank movement
	bool m_isEngineRunning = false;
	bool m_isDestroyed = false;

	// ==== MESH PARTS ====
	// Main body meshes
	Schematyc::AnyModelFileName  m_tankBodyMesh = "softleafmaster/models/abrams/m1a1_abrams.cga";
	string m_bodyMesh = "softleafmaster/models/abrams/m1a1_abrams.cga";  // Direct path - GameSDK style loading
	string m_bodyMeshDestroyed = "objects/vehicles/abrams_old/abrams_damaged.cga";

	// Component-based wheeled vehicle (CryDefaultEntities — not GameSDK IVehicleSystem)
	Schematyc::CSharedString m_wheelJointNameFilter = "wheel";
	float m_wheelRadius = 0.45f;
	float m_wheelWidth = 0.25f;
	float m_wheelSuspensionLength = 0.5f;
	float m_wheelSuspensionCompressed = 0.35f;
	float m_wheelMass = 80.0f;
	float m_wheelDamping = 0.7f;
	bool m_wheelRaycast = false;

	// Turret and cannon meshes
	string m_turretMesh = "";
	string m_cannonMesh = "";

	// Tread meshes
	string m_leftTreadMesh = "softleafmaster/models/abrams/tread_left.chr";
	string m_rightTreadMesh = "softleafmaster/models/abrams/tread_right.chr";

	int m_bodySlot = -1;
	int m_leftTreadSlot = -1;
	int m_rightTreadSlot = -1;
	_smart_ptr<ICharacterInstance> m_leftTreadCharacter = nullptr;
	_smart_ptr<ICharacterInstance> m_rightTreadCharacter = nullptr;

	// Attachment meshes
	string m_opticsMesh = "softleafmaster/models/abrams/m1a2_optics.cgf";
	string m_panelMesh = "softleafmaster/models/abrams/m1a2_panel.cgf";
	string m_ammoMesh = "softleafmaster/models/abrams/m1a2_ammo.cgf";
	string m_bagMesh = "softleafmaster/models/abrams/m1a2_bag.cgf";
	string m_boxMesh = "softleafmaster/models/abrams/m1a2_box.cgf";
	string m_grenadeLauncherMesh = "softleafmaster/models/abrams/grenade_launcher.cgf";
	string m_heatShieldMesh = "softleafmaster/models/abrams/m1a2_heat_shield.cgf";

	// ==== HELPER POSITIONS (from Abrams.xml) ====
	// Driver positions
	Vec3 m_driverEnterPos = Vec3(-2.5f, -0.5f, 0.5f);
	Vec3 m_driverSitPos = Vec3(-0.5f, 0.0f, 2.0f);
	Vec3 m_driverViewPos = Vec3(-0.45f, 1.75f, 2.35f);

	// Gunner positions
	Vec3 m_gunnerEnterPos = Vec3(2.5f, -0.5f, 0.5f);
	Vec3 m_gunnerSitPos = Vec3(0.45f, -0.65f, 2.0f);
	Vec3 m_gunnerViewPos = Vec3(0.45f, -0.65f, 2.85f);

	// Weapon positions
	Vec3 m_cannonStartPos = Vec3(0.0f, 3.8f, 1.9f);
	Vec3 m_cannonOutPos = Vec3(0.0f, 5.8f, 1.9f);
	Vec3 m_coaxOutPos = Vec3(0.315f, 2.4f, 2.035f);
	Vec3 m_turretPos = Vec3(-0.0013f, 0.1204f, 1.584f);

	// Effect positions
	Vec3 m_burningPos = Vec3(0.0f, -1.335f, 1.7f);
	Vec3 m_exhaustPos = Vec3(-0.7f, -4.0f, 1.1621f);
	Vec3 m_engineSmokeOutPos = Vec3(0.175f, -4.1f, 1.25f);
	Vec3 m_centerPos = Vec3(0.0f, -1.0f, 1.1682f);

	// Abrams particle effect names from GameSDK Abrams.xml
	string m_engineStartEffect = "Vehicles.Abrams.Engine.Start";
	string m_engineStopEffect = "Vehicles.Abrams.Engine.Stop";
	string m_engineRunningEffect = "Vehicles.Abrams.Engine.Running";
	string m_engineBoostEffect = "Vehicles.Abrams.Engine.Boost";
	string m_damageEngine50Effect = "Vehicles.Abrams.Damage.Engine_50";
	string m_damageEngine75Effect = "Vehicles.Abrams.Damage.Engine_75";
	string m_damageHull50Effect = "Vehicles.Abrams.Damage.Hull_50";
	string m_damageHull75Effect = "Vehicles.Abrams.Damage.Hull_75";
	string m_damageDestroyedEffect = "Vehicles.Abrams.Damage.Destroyed";
	string m_damageFlippedEffect = "Vehicles.Abrams.Damage.Flipped";

	// Light positions
	Vec3 m_headLightLeftPos = Vec3(-0.9099f, 3.55f, 1.23f);
	Vec3 m_headLightRightPos = Vec3(0.9103f, 3.55f, 1.23f);
	Vec3 m_rearLightLeftPos = Vec3(-1.6175f, -4.4f, 1.6125f);
	Vec3 m_rearLightRightPos = Vec3(1.6175f, -4.4f, 1.6125f);
	Vec3 m_reverseLightLeftPos = Vec3(-1.6175f, -4.4f, 1.515f);
	Vec3 m_reverseLightRightPos = Vec3(1.6175f, -4.4f, 1.515f);

	// Boost position
	Vec3 m_boostOriginPos = Vec3(0.0f, -4.4f, 0.08f);

	// ==== COMPONENT PROPERTIES ====
	// Hull component
	float m_hullDamageMax = 2500.0f;
	float m_hullCurrentDamage = 0.0f;

	// Engine component
	float m_engineDamageMax = 1000.0f;
	float m_engineCurrentDamage = 0.0f;

	// Turret component
	float m_turretDamageMax = 700.0f;
	float m_turretCurrentDamage = 0.0f;

	// Tread components
	float m_leftTreadDamageMax = 250.0f;
	float m_leftTreadCurrentDamage = 0.0f;
	float m_rightTreadDamageMax = 250.0f;
	float m_rightTreadCurrentDamage = 0.0f;

	// Light components
	float m_headLightDamageMax = 10.0f;
	float m_brakeLightDamageMax = 10.0f;

	// Damage multipliers
	float m_explosionDamageMult = 1.0f;
	float m_bulletDamageMult = 0.05f;
	float m_rocketDamageMult = 1.0f;
	float m_collisionDamageMult = 0.25f;

	int m_pPhysCacheId = 0;

	
private:
	// Movement state (not exposed to editor)
	float m_currentSpeed = 0.0f;
	float m_currentRotation = 0.0f;
	float m_inputThrottle = 0.0f;			// Raw input
	float m_currentPedal = 0.0f;				// Ramped pedal
	float m_inputSteer = 0.0f;				// Raw input
	float m_currentSteer = 0.0f;				// Ramped steer
	float m_targetSteer = 0.0f;				// Target for ramping
	float m_currentTurretYaw = 0.0f;
	float m_currentTurretPitch = 0.0f;

	// Engine particle spawn state (per-entity, reset on level reset)
	bool m_engineEffectsSpawned = false;
	
	// Steering input (for external input handling)
	float m_steerInput = 0.0f;				// Left/right steering input
	bool m_isAI = false;						// AI vs player distinction
	
	// Friction and slip state
	float m_currentLatFriction = 1.0f;
	float m_currentLatSlip = 0.0f;
	float m_avgLateralSlip = 0.0f;
	float m_currentSlipMin = 0.0f;
	float m_currentFricMin = 1.0f;
	float m_latFrictionMin = 0.4f;			// latFricMin
	float m_latFrictionMinSteer = 0.7f;		// latFricMinSteer (reduced when countersteering)
	float m_latFrictionMax = 1.5f;
	float m_latSlipMin = 0.1f;
	float m_latSlipMax = 5.0f;
	
	// Movement parameters (from GameSDK)
	float m_pedalSpeed = 1.5f;				// Pedal ramping speed
	float m_pedalThreshold = 0.2f;			// Pedal dead zone
	float m_steerSpeed = 2.0f;				// Steer ramping speed
	float m_steerSpeedRelax = 1.0f;			// Steer relax speed (slower)
	float m_steerLimit = 1.0f;				// Steer clamping limit
	float m_pedalAcceleration = 4.0f;		// Legacy (deprecated - use m_pedalSpeed)
	float m_steerAcceleration = 3.0f;		// Legacy (deprecated - use m_steerSpeed)
	float m_steerRelaxation = 2.0f;			// Legacy (deprecated - use m_steerSpeedRelax)
	
	// Steering impulse (optional angular correction)
	float m_steeringImpulseMin = 0.0f;
	float m_steeringImpulseMax = 0.0f;
	float m_steeringImpulseRelaxMin = 0.0f;
	float m_steeringImpulseRelaxMax = 0.0f;
	
	// Wheel and tread tracking
	int   m_wheelCount = 0;
	int   m_wheelContactsLeft = 0;
	int   m_wheelContactsRight = 0;
	int   m_wheelContacts = 0;
	int   m_blownTires = 0;						// Equivalent to destroyed treads
	std::vector<int> m_treadPartIds;				// Track tread parts (by entity slot or similar)
	int   m_drivingWheelLeft = -1;
	int   m_drivingWheelRight = -1;
	
	// Damage and health
	float m_damage = 0.0f;						// 0-1 normalized damage
	float m_damageRPMScale = 1.0f;				// Engine RPM multiplier due to damage
	
	// Weapon cooldowns
	float m_mainCannonCooldown = 0.0f;
	float m_coaxialGunCooldown = 0.0f;
	
	// Audio control IDs
	bool m_isEngineSoundPlaying = false;
	CryAudio::ControlId m_audioSpeedParamId = CryAudio::InvalidControlId;
	CryAudio::ControlId m_audioEngineRunTriggerId = CryAudio::InvalidControlId;
	CryAudio::ControlId m_audioEngineStopTriggerId = CryAudio::InvalidControlId;
	CryAudio::ControlId m_audioTurretTurnTriggerId = CryAudio::InvalidControlId;
	CryAudio::ControlId m_audioTurretRotationSpeedParamId = CryAudio::InvalidControlId;
	CryAudio::ControlId m_audioMainCannonTriggerId = CryAudio::InvalidControlId;
	CryAudio::ControlId m_audioCoaxialGunTriggerId = CryAudio::InvalidControlId;
	CryAudio::ControlId m_audioCoaxialGunStopTriggerId = CryAudio::InvalidControlId;
	
	// Physics state caching
	pe_status_vehicle m_vehicleStatus;
	Matrix34 m_worldTM;
	Vec3 m_localVelocity;
	Vec3 m_localAngularVelocity;
	
	// Physics action structure (from GameSDK)
	pe_action_drive m_action{};
	
	// Update tank physics and state
	void UpdateTankPhysics(float deltaTime);
	void InitializeTankMovement();
	void ProcessMovement(float deltaTime);
	void ProcessMovementAdvanced(float deltaTime);
	float GetWheelCondition() const;
	void SetLatFriction(float latFriction);
	void UpdateWeaponCooldowns(float deltaTime);
	void UpdateAxleFriction(float actionPedal, bool isMoving, float deltaTime);
	void UpdateSuspension(float deltaTime);
	void UpdateSounds(float deltaTime);
	void UpdateSpeedRatio(float deltaTime);
	void ApplyAirDamp(float pitchDamp, float rollDamp, float deltaTime, int threadSafe);
	void ApplyBoost(float speed, float maxSpeed, float strength, float deltaTime);
	int LoadTreadSlot(const string& filename, int slotIndex, _smart_ptr<ICharacterInstance>& outChar, const char* treadName);
	IPhysicalEntity* GetTankPhysics() const;
	IEntityAudioComponent* GetAudioProxy();
	void ApplyDriveAction(float throttle, float steer, bool handbrake);
	bool SpawnParticleEffect(const char* effectName, const Vec3& position, const Vec3& direction, float scale);
	
	// Damage and event handling
	void OnTreadDestroyed(int treadIndex);
	void OnTreadRepaired(int treadIndex);
	void SetEngineRPMMult(float mult);
	void UpdateDamageState();
	void ApplyLocalPlayerInput(float deltaTime);
	void SetupComponentVehiclePhysics();
	void EnsureVehiclePhysicalized();
	void TryFinishWheelSetup();
	void SetupWheelsFromCharacter(ICharacterInstance* pCharacter);
	bool AddWheelColliderAtJoint(const char* jointName, const Matrix34& jointLocalTM, int axleIndex);
	static bool JointNameMatchesWheelFilter(const char* jointName, const char* filter);
	void SetMoveLeftActive(bool isActive);
	void SetMoveRightActive(bool isActive);
	void SetMoveForwardActive(bool isActive);
	void SetMoveBackActive(bool isActive);
	void ProcessMovementFallback(float deltaTime);
	void DebugMass();

protected:
	_smart_ptr<ICharacterInstance>     m_pCachedCharacter = nullptr;
	Cry::DefaultComponents::CInputComponent* m_pInputComponent = nullptr;
	bool m_moveLeftPressed = false;
	bool m_moveRightPressed = false;
	bool m_moveForwardPressed = false;
	bool m_moveBackPressed = false;
	Vec2 m_mouseDeltaRotation = ZERO;
	bool m_loggedMissingWheelPhysics = false;
	bool m_loggedFallbackReason = false;
	bool m_wheelsSetupDone = false;
	bool m_pendingWheelSetup = false;
	bool m_tankMovementInitialized = false;
	bool m_loggedWheelSetupDeferred = false;

	Cry::DefaultComponents::CVehiclePhysicsComponent* m_pVehiclePhysicsComponent = nullptr;
	std::vector<int> m_wheelPhysPartIds;
	std::vector<float> m_vehicleGearRatios;
};
