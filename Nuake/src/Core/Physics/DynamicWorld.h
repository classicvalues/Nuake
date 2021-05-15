#pragma once
#include <btBulletDynamicsCommon.h>
#include <glm/ext/vector_float3.hpp>
#include "Rigibody.h"
#include "../Timestep.h"
#include "../Core/Core.h"
#include <map>
#include "RaycastResult.h"
#include "CharacterController.h"
namespace Physics {
	class DynamicWorld {
	private:
		btDiscreteDynamicsWorld* dynamicsWorld;
		std::map<btRigidBody*, Ref<RigidBody>> m_Bodies;
	public:
		DynamicWorld();

		void DrawDebug();

		void SetGravity(glm::vec3 g);
		void AddRigidbody(Ref<RigidBody> rb);

		void AddCharacterController(Ref < CharacterController> cc);

		RaycastResult Raycast(glm::vec3 from, glm::vec3 to);
		void StepSimulation(Timestep ts);
		void Clear();

		btDiscreteDynamicsWorld* GetDynamicWorld() { return dynamicsWorld; }
	};
}