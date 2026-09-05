// Copyright 2025-2026 Softleafgame Alvinsat. All rights reserved.
#pragma once

#include <array>
#include <numeric>

#include <CryEntitySystem/IEntityComponent.h>
#include <CryMath/Cry_Camera.h>

#include <ICryMannequin.h>

#include <DefaultComponents/Physics/CharacterControllerComponent.h>
#include <DefaultComponents/Geometry/AdvancedAnimationComponent.h>

class CAIHumanBotComponent final : public IEntityComponent
{
	template<typename T, size_t SAMPLES_COUNT>
	class MovingAverage
	{
		static_assert(SAMPLES_COUNT > 0, "SAMPLES_COUNT shall be larger than zero!");

	public:
		MovingAverage()
			: m_values()
			, m_cursor(SAMPLES_COUNT)
			, m_accumulator()
		{
		}

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
		void Reset() { m_cursor = SAMPLES_COUNT; }

	private:
		std::array<T, SAMPLES_COUNT> m_values;
		size_t m_cursor;
		T m_accumulator;
	};

public:
	CAIHumanBotComponent() = default;
	virtual ~CAIHumanBotComponent() override = default;

	static void ReflectType(Schematyc::CTypeDesc<CAIHumanBotComponent>& desc);

	virtual void Initialize() override;
	virtual Cry::Entity::EventFlags GetEventMask() const override;
	virtual void ProcessEvent(const SEntityEvent& event) override;

	Cry::DefaultComponents::CCharacterControllerComponent* GetCharacterController() const { return m_pCharacterController; }
	void SetMoveInput(const Vec2& moveInput);
	void SetLookOrientation(const Quat& orientation);
	void StopMovement();

private:
	void ResetState();
	void UpdateMovementRequest(float frameTime);
	void UpdateLookDirectionRequest(float frameTime);
	void UpdateAnimation(float frameTime);

private:
	Cry::DefaultComponents::CCharacterControllerComponent* m_pCharacterController = nullptr;
	Cry::DefaultComponents::CAdvancedAnimationComponent* m_pAnimationComponent = nullptr;

	FragmentID m_idleFragmentId = FRAGMENT_ID_INVALID;
	FragmentID m_walkFragmentId = FRAGMENT_ID_INVALID;
	TagID m_rotateTagId = TAG_ID_INVALID;
	FragmentID m_activeFragmentId = FRAGMENT_ID_INVALID;

	Vec2 m_moveInput = ZERO;
	Quat m_lookOrientation = IDENTITY;
	float m_horizontalAngularVelocity = 0.0f;
	MovingAverage<float, 10> m_averagedHorizontalAngularVelocity;

	float m_moveSpeed = 20.5f;
	float m_walkSpeed = 1.4f;
};
