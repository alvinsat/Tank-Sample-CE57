#pragma once

#include <CryEntitySystem/IEntityComponent.h>
#include <CryMath/Cry_Camera.h>
#include <CrySchematyc/Utils/EnumFlags.h>

#include <DefaultComponents/Cameras/CameraComponent.h>
#include <DefaultComponents/Input/InputComponent.h>

class CPlayerComponentFlyby final : public IEntityComponent
{
	enum class EInputFlag : uint8
	{
		MoveLeft    = 1 << 0,
		MoveRight   = 1 << 1,
		MoveForward = 1 << 2,
		MoveBack    = 1 << 3,
		MoveUp      = 1 << 4,
		MoveDown    = 1 << 5
	};

public:
	CPlayerComponentFlyby() = default;
	virtual ~CPlayerComponentFlyby() override = default;

	// IEntityComponent overrides
	static void ReflectType(Schematyc::CTypeDesc<CPlayerComponentFlyby>& desc);
	virtual void Initialize() override;
	virtual Cry::Entity::EventFlags GetEventMask() const override;
	virtual void ProcessEvent(const SEntityEvent& event) override;

private:
	bool IsLocalClient() const { return (m_pEntity->GetFlags() & ENTITY_FLAG_LOCAL_PLAYER) != 0 || m_pEntity->GetId() == LOCAL_PLAYER_ENTITY_ID; }
	void InitializeLocalPlayer();
	void SnapToSpawnPoint();
	bool ResolveSpawnPointTransform(Matrix34& outTransform) const;
	void Update(float deltaTime);
	bool FollowTPSCameraIfAvailable(float deltaTime);
	bool FollowCameraAnchorIfAvailable();
	void UpdateLookDirection(float deltaTime);
	void UpdateMovement(float deltaTime);
	void UpdateCamera();
	void SetCameraWorldTransformOverride(const Matrix34& worldTransform);
	void ClearCameraWorldTransformOverride();
	void LogSpawnAndFlybyPosition(float deltaTime);
	void FireViewRaycast();
	void DrawDebugHit() const;
	void HandleInputFlagChange(CEnumFlags<EInputFlag> flags, CEnumFlags<EActionActivationMode> activationMode);

private:
	Cry::DefaultComponents::CCameraComponent* m_pCameraComponent = nullptr;
	Cry::DefaultComponents::CInputComponent* m_pInputComponent = nullptr;

	CEnumFlags<EInputFlag> m_inputFlags;
	Quat m_lookOrientation = IDENTITY;
	Vec2 m_mouseDeltaRotation = ZERO;

	bool m_hasDebugHit = false;
	bool m_hasInitializedFromView = false;
	bool m_hasAppliedSpawnPoint = false;
	bool m_hasAttemptedFirstUpdateSnap = false;
	bool m_isFollowingCameraAnchor = false;
	bool m_hasSpawnPoint = false;
	bool m_hasCameraTransformOverride = false;
	Vec3 m_debugHitPosition = ZERO;
	Vec3 m_lastResolvedSpawnPosition = ZERO;
	Quat m_lastResolvedSpawnRotation = IDENTITY;
	Matrix34 m_cameraLocalTransformOverride = IDENTITY;
	float m_debugSphereRadius = 0.15f;
	float m_positionLogTimer = 0.0f;
	float m_spawnRetryLogTimer = 0.0f;

	float m_moveSpeed = 60.0f;
	float m_rotationSpeed = 0.002f;
	float m_boostMultiplier = 8.0f;
	float m_maxPitch = 1.5f;
	float m_spawnHeightOffset = 1.8f;
};
