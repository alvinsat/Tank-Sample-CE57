// Copyright 2016-2019 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

#include <array>
#include <numeric>

#include <CryEntitySystem/IEntityComponent.h>
#include <CryMath/Cry_Camera.h>

#include <ICryMannequin.h>
#include <CrySchematyc/Utils/EnumFlags.h>

#include <DefaultComponents/Cameras/CameraComponent.h>
#include <DefaultComponents/Physics/CharacterControllerComponent.h>
#include <DefaultComponents/Geometry/AdvancedAnimationComponent.h>
#include <DefaultComponents/Input/InputComponent.h>
#include <DefaultComponents/Audio/ListenerComponent.h>

////////////////////////////////////////////////////////
// Represents a player participating in gameplay
////////////////////////////////////////////////////////
class CPlayerComponent final : public IEntityComponent
{
	enum class EInputFlagType
	{
		Hold = 0,
		Toggle
	};

	enum class EInputFlag : uint8
	{
		MoveLeft    = 1 << 0,
		MoveRight   = 1 << 1,
		MoveForward = 1 << 2,
		MoveBack    = 1 << 3
	};

	static constexpr EEntityAspects InputAspect = eEA_GameClientD;

	template<typename T, size_t SAMPLES_COUNT>
	class MovingAverage
	{
		static_assert(SAMPLES_COUNT > 0, "SAMPLES_COUNT shall be larger than zero!");

	public:
		MovingAverage()
			: m_values()
			, m_cursor(SAMPLES_COUNT)
			, m_accumulator()
		{}

		MovingAverage& Push(const T& value)
		{
			if (m_cursor == SAMPLES_COUNT)
			{
				m_values.fill(value);
				m_cursor = 0;
				m_accumulator = std::accumulate(m_values.begin(), m_values.end(), T(0));
			}
			else
			{
				m_accumulator -= m_values[m_cursor];
				m_values[m_cursor] = value;
				m_accumulator += m_values[m_cursor];
				m_cursor = (m_cursor + 1) % SAMPLES_COUNT;
			}
			return *this;
		}

		T Get() const { return m_accumulator / T(SAMPLES_COUNT); }
		void Reset()  { m_cursor = SAMPLES_COUNT; }

	private:
		std::array<T, SAMPLES_COUNT> m_values;
		size_t m_cursor;
		T m_accumulator;
	};

public:
	CPlayerComponent() = default;
	virtual ~CPlayerComponent() {}

	virtual void Initialize() override;
	virtual Cry::Entity::EventFlags GetEventMask() const override;
	virtual void ProcessEvent(const SEntityEvent& event) override;

	virtual bool NetSerialize(TSerialize ser, EEntityAspects aspect, uint8 profile, int flags) override;
	virtual NetworkAspectType GetNetSerializeAspectMask() const override { return InputAspect; }

	static void ReflectType(Schematyc::CTypeDesc<CPlayerComponent>& desc)
	{
		desc.SetGUID("{63F4C0C6-32AF-4ACB-8FB0-57D45DD14725}"_cry_guid);
	}

	void OnReadyForGameplayOnServer();
	bool IsLocalClient() const { return (m_pEntity->GetFlags() & ENTITY_FLAG_LOCAL_PLAYER) != 0; }

	// Set target FOV in degrees — camera smoothly interpolates to it.
	// Call SetFOV(m_baseFOV) to return to default (e.g. on aim release).
	void SetFOV(float degrees) { m_targetFOV = DEG2RAD(degrees); }

protected:
	void Revive(const Matrix34& transform);

	void UpdateMovementRequest(float frameTime);
	void UpdateLookDirectionRequest(float frameTime);
	void UpdateAnimation(float frameTime);
	void UpdateCamera(float frameTime);

	void HandleInputFlagChange(CEnumFlags<EInputFlag> flags, CEnumFlags<EActionActivationMode> activationMode, EInputFlagType type = EInputFlagType::Hold);
	void InitializeLocalPlayer();

protected:
	struct RemoteReviveParams
	{
		void SerializeWith(TSerialize ser)
		{
			ser.Value("pos", position, 'wrld');
			ser.Value("rot", rotation, 'ori0');
		}
		Vec3 position;
		Quat rotation;
	};
	bool RemoteReviveOnClient(RemoteReviveParams&& params, INetChannel* pNetChannel);

protected:
	bool m_isAlive = false;

	Cry::DefaultComponents::CCameraComponent*                  m_pCameraComponent        = nullptr;
	Cry::DefaultComponents::CCharacterControllerComponent*     m_pCharacterController    = nullptr;
	Cry::DefaultComponents::CAdvancedAnimationComponent*       m_pAnimationComponent     = nullptr;
	Cry::DefaultComponents::CInputComponent*                   m_pInputComponent         = nullptr;
	Cry::Audio::DefaultComponents::CListenerComponent*         m_pAudioListenerComponent = nullptr;

	FragmentID m_idleFragmentId;
	FragmentID m_walkFragmentId;
	TagID      m_rotateTagId;

	CEnumFlags<EInputFlag>   m_inputFlags;
	Vec2                     m_mouseDeltaRotation;
	MovingAverage<Vec2, 10>  m_mouseDeltaSmoothingFilter;

	FragmentID m_activeFragmentId;

	// CE sample pattern — yaw+pitch accumulated here, drives entity rotation and orbit camera
	Quat  m_lookOrientation         = IDENTITY;
	float m_horizontalAngularVelocity = 0.f;
	MovingAverage<float, 10> m_averagedHorizontalAngularVelocity;

	// -------------------------------------------------------
	// Orbit camera
	// -------------------------------------------------------
	float m_orbitYaw          = 0.f;
	float m_orbitPitch        = 0.3f;
	float m_orbitDistance     = 3.0f;

	float m_orbitYawSpeed     = 0.002f;
	float m_orbitPitchSpeed   = 0.002f;

	const float m_minOrbitPitch    = -0.6f;
	const float m_maxOrbitPitch    =  1.0f;
	const float m_minOrbitDistance =  1.5f;
	const float m_maxOrbitDistance = 15.0f;

	Vec3  m_cameraOffset      = Vec3(0.f, 0.f, 0.f);
	bool  m_cameraInitialized = false;
	float m_cameraLagSpeed    = 5.0f;

	// -------------------------------------------------------
	// FOV
	// -------------------------------------------------------
	const float m_baseFOV    = 55.f;
	float m_targetFOV        = DEG2RAD(55.f);
	float m_currentFOV       = DEG2RAD(55.f);
	float m_fovLagSpeed      = 8.0f;
	float m_debugLogTimer    = 0.f;

	// -------------------------------------------------------
	// Blend space
	// -------------------------------------------------------
	// TravelSpeed range 0.2-2.5
	float m_walkSpeed        = 1.4f;
};