#pragma once
#include <src/Scene/Systems/ScriptingSystem.h>
#include <src/Scene/Systems/PhysicsSystem.h>


#include <src/Scene/Systems/QuakeMapBuilder.h>
#include <src/Scene/Components/BSPBrushComponent.h>

#include "Scene.h"
#include "Entities/Entity.h"

#include "../Rendering/Renderer.h"
#include "../Core/MaterialManager.h"
#include "../Core/Physics/PhysicsManager.h"
#include "../Core/Core.h"

#include <GL/glew.h>

#include "../../Engine.h"
#include "../Core/FileSystem.h"
#include "Components/Components.h"
#include "Components/BoxCollider.h"
#include "Components/LuaScriptComponent.h"
#include "Components/WrenScriptComponent.h"

#include <fstream>
#include <streambuf>
#include <chrono>
#include <src/Core/OS.h>

Ref<Scene> Scene::New()
{
	return CreateRef<Scene>();
}

Scene::Scene()
{
	m_Systems = std::vector<Ref<System>>();
	m_EditorCamera = CreateRef<EditorCamera>();
	m_Environement = CreateRef<Environment>();
	m_Interfaces = std::vector<Ref<UI::UserInterface>>();
    
	// Adding systems
	m_Systems.push_back(CreateRef<ScriptingSystem>(this));
    m_Systems.push_back(CreateRef<PhysicsSystem>(this));
}

Scene::~Scene() {}

std::string Scene::GetName()
{
	return this->Name;
}

bool Scene::SetName(std::string& newName)
{
	if (newName == "")
		return false;
    
	this->Name = newName;
	return true;
}

Entity Scene::GetEntity(int handle)
{
	return Entity((entt::entity)handle, this);
}

Entity Scene::GetEntityByID(int id)
{
	auto idView = m_Registry.view<NameComponent>();
	for (auto e : idView) {
		NameComponent& nameC = idView.get<NameComponent>(e);
		if (nameC.ID == id)
			return Entity{ e, this };
	}
}

void Scene::OnInit()
{
	for (auto& system : m_Systems)
		system->Init();
}

void Scene::OnExit()
{
	for (auto& system : m_Systems)
		system->Exit();
}

void Scene::Update(Timestep ts)
{
	UpdatePositions();

	for (auto& system : m_Systems)
		system->Update(ts);
}

void Scene::FixedUpdate(Timestep ts)
{
	for (auto& system : m_Systems)
		system->FixedUpdate(ts);
}

void Scene::EditorUpdate(Timestep ts)
{
	UpdatePositions();

	m_EditorCamera->Update(ts);
    
	for (auto i : m_Interfaces)
		i->Update(ts);
}

void Scene::UpdatePositions()
{
	auto transformView = m_Registry.view<ParentComponent, TransformComponent>();
	for (auto e : transformView) {
		auto [parent, transform] = transformView.get<ParentComponent, TransformComponent>(e);
		Entity currentParent = Entity{ e, this };
		Vector3 globalPos = Vector3();
		if (parent.HasParent)
		{
			while (currentParent.GetComponent<ParentComponent>().HasParent) {
				currentParent = currentParent.GetComponent<ParentComponent>().Parent;

				globalPos += currentParent.GetComponent<TransformComponent>().Translation;
			}

			transform.GlobalTranslation = globalPos + transform.Translation;
		}
		else
		{
			transform.GlobalTranslation = transform.Translation;
		}
	}
}

void Scene::DrawShadows()
{
	auto modelView = m_Registry.view<TransformComponent, ModelComponent>();
	auto quakeView = m_Registry.view<TransformComponent, QuakeMapComponent>();
	auto view = m_Registry.view<TransformComponent, LightComponent>();
    
	Ref<Camera> cam = m_EditorCamera;
	if (Engine::IsPlayMode) {
		auto view = m_Registry.view<TransformComponent, CameraComponent>();
		for (auto e : view) {
			auto [transform, camera] = view.get<TransformComponent, CameraComponent>(e);
			cam = camera.CameraInstance;
			break;
		}
	}
    
	glm::mat4 perspective = cam->GetPerspective();
	Renderer::m_ShadowmapShader->Bind();
	for (auto l : view) {
		auto [lightTransform, light] = view.get<TransformComponent, LightComponent>(l);
		if (light.Type != LightType::Directional || !light.CastShadows)
			continue;
        
		light.CalculateViewProjection(cam->GetTransform(), cam->GetPerspective());

		for (int i = 0; i < 4; i++)
		{
			light.m_Framebuffers[i]->Bind();
			Renderer::m_ShadowmapShader->SetUniformMat4f("lightSpaceMatrix", light.mViewProjections[i]);
			for (auto e : modelView)
			{
				auto [transform, model] = modelView.get<TransformComponent, ModelComponent>(e);

				Renderer::m_ShadowmapShader->SetUniformMat4f("model", transform.GetTransform());
				model.Draw();
			}

			Renderer::m_ShadowmapShader->SetUniformMat4f("lightSpaceMatrix", light.mViewProjections[i]);
			auto quakeView = m_Registry.view<TransformComponent, BSPBrushComponent>();
			for (auto e : quakeView) {
				auto [transform, model] = quakeView.get<TransformComponent, BSPBrushComponent>(e);
				Renderer::m_ShadowmapShader->SetUniformMat4f("model", transform.GetTransform());
				if (model.IsTransparent)
					continue;
				for (auto& e : model.Meshes) {
					e->Draw(false);
				}
			}
		}

	}
}

void Scene::DrawInterface(Vector2 screensize)
{
	for (auto i : m_Interfaces)
	{
		i->Draw(screensize);
	}
}

void Scene::Draw()
{
	// Find the camera of the scene.
	Ref<Camera> cam = nullptr;
	{
		auto view = m_Registry.view<TransformComponent, CameraComponent, ParentComponent>();
		for (auto e : view) {
			auto [transform, camera, parent] = view.get<TransformComponent, CameraComponent, ParentComponent>(e);
			cam = camera.CameraInstance;
			cam->Translation = transform.GlobalTranslation;
			break;
		}
	}
	glDisable(GL_CULL_FACE);
	glDisable(GL_DEPTH_TEST);
	Ref<Environment> env = GetEnvironment();
    
	if (env->ProceduralSkybox)
	{
		env->ProceduralSkybox->Draw(cam);
        
	}
	Renderer::m_Shader->Bind();
	env->Push();
	glEnable(GL_DEPTH_TEST);
    
	// Push lights
	{
		auto view = m_Registry.view<TransformComponent, LightComponent, ParentComponent>();
		for (auto l : view) {
			auto [transform, light, parent] = view.get<TransformComponent, LightComponent, ParentComponent>(l);
            
			if (light.SyncDirectionWithSky)
				light.Direction = GetEnvironment()->ProceduralSkybox->GetSunDirection();
            
			light.Draw(transform, m_EditorCamera);
		}
	}
	glEnable(GL_CULL_FACE);
	glCullFace(GL_FRONT);
	Renderer::m_Shader->Bind();
	Renderer::m_Shader->SetUniform3f("u_EyePosition", cam->GetTranslation().x, cam->GetTranslation().y, cam->GetTranslation().z);
	Renderer::m_Shader->SetUniform1i("u_ShowNormal", 0);
	if (cam)
	{
		Renderer::m_Shader->SetUniform1f("u_Exposure", cam->Exposure);
        
		auto view = m_Registry.view<TransformComponent, ModelComponent, ParentComponent>();
		for (auto e : view) {
			auto [transform, model, parent] = view.get<TransformComponent, ModelComponent, ParentComponent>(e);            
			Renderer::m_Shader->SetUniformMat4f("u_View", cam->GetTransform());
			Renderer::m_Shader->SetUniformMat4f("u_Projection", cam->GetPerspective());
			Renderer::m_Shader->SetUniformMat4f("u_Model", transform.GetTransform());
			model.Draw();
		}
        
		auto quakeView = m_Registry.view<TransformComponent, BSPBrushComponent, ParentComponent>();
		for (auto e : quakeView) {
			auto [transform, model, parent] = quakeView.get<TransformComponent, BSPBrushComponent, ParentComponent>(e);
			if (model.IsTransparent)
				continue;

			Renderer::m_Shader->SetUniformMat4f("u_View", cam->GetTransform());
			Renderer::m_Shader->SetUniformMat4f("u_Projection", cam->GetPerspective());
			Renderer::m_Shader->SetUniformMat4f("u_Model", transform.GetTransform());

			for (auto& e : model.Meshes)
			{
				e->Draw();
			}
		}
        
		Renderer::m_DebugShader->SetUniformMat4f("u_View", cam->GetTransform());
		Renderer::m_DebugShader->SetUniformMat4f("u_Projection", cam->GetPerspective());

		PhysicsManager::Get()->DrawDebug();
	}
}

void Scene::EditorDraw()
{
	glDisable(GL_DEPTH_TEST);
	Ref<Environment> env = GetEnvironment();
    
	glDisable(GL_CULL_FACE);
	if (env->ProceduralSkybox)
	{
		env->ProceduralSkybox->Draw(m_EditorCamera);
	}
    
	Renderer::m_Shader->Bind();
	env->Push();
	glEnable(GL_DEPTH_TEST);

	// Push lights
	{
		auto view = m_Registry.view<TransformComponent, LightComponent, ParentComponent>();
		for (auto l : view) {
			auto [transform, light, parent] = view.get<TransformComponent, LightComponent, ParentComponent>(l);

			if (light.SyncDirectionWithSky)
				light.Direction = GetEnvironment()->ProceduralSkybox->GetSunDirection();
            
			light.Draw(transform, m_EditorCamera);
		}
	}
	glEnable(GL_CULL_FACE);
	glCullFace(GL_FRONT);
	Renderer::m_Shader->Bind();
	Renderer::m_Shader->SetUniform1i("u_ShowNormal", 0);
	if (m_EditorCamera)
	{
		Renderer::m_Shader->SetUniform1f("u_Exposure", m_EditorCamera->Exposure);
		Renderer::m_Shader->SetUniform3f("u_EyePosition", m_EditorCamera->GetTranslation().x, m_EditorCamera->GetTranslation().y, m_EditorCamera->GetTranslation().z);
		auto view = m_Registry.view<TransformComponent, ModelComponent, ParentComponent>();
		for (auto e : view) {
			auto [transform, model, parent] = view.get<TransformComponent, ModelComponent, ParentComponent>(e);

			Renderer::m_Shader->SetUniformMat4f("u_View", m_EditorCamera->GetTransform());
			Renderer::m_Shader->SetUniformMat4f("u_Projection", m_EditorCamera->GetPerspective());
			Renderer::m_Shader->SetUniformMat4f("u_Model", transform.GetTransform());
			model.Draw();
		}
        
		auto quakeView = m_Registry.view<TransformComponent, BSPBrushComponent, ParentComponent>();
		for (auto e : quakeView) {
			auto [transform, model, parent] = quakeView.get<TransformComponent, BSPBrushComponent, ParentComponent>(e);

			Renderer::m_Shader->SetUniformMat4f("u_View", m_EditorCamera->GetTransform());
			Renderer::m_Shader->SetUniformMat4f("u_Projection", m_EditorCamera->GetPerspective());
			Renderer::m_Shader->SetUniformMat4f("u_Model", transform.GetTransform());

			for (auto& e : model.Meshes) {
				e->Draw();
			}
		}
        
		Renderer::m_DebugShader->Bind();
		auto boxCollider = m_Registry.view<TransformComponent, BoxColliderComponent, ParentComponent>();
		for (auto e : boxCollider) {
			auto [transform, box, parent] = boxCollider.get<TransformComponent, BoxColliderComponent, ParentComponent>(e);
			Renderer::m_DebugShader->SetUniformMat4f("u_View", m_EditorCamera->GetTransform());
			Renderer::m_DebugShader->SetUniformMat4f("u_Projection", m_EditorCamera->GetPerspective());

			Renderer::m_DebugShader->SetUniformMat4f("u_Model", transform.GetTransform());
			
			TransformComponent t = transform;
			t.Scale = (box.Size * 2.f) * transform.Scale;
			Renderer::DrawCube(t, glm::vec4(0.0f, 0.0f, 0.9f, 0.2f));
		}
        
		auto sphereCollider = m_Registry.view<TransformComponent, SphereColliderComponent, ParentComponent>();
		for (auto e : sphereCollider) {
			auto [transform, box, parent] = sphereCollider.get<TransformComponent, SphereColliderComponent, ParentComponent>(e);
            
			Renderer::m_DebugShader->SetUniformMat4f("u_View", m_EditorCamera->GetTransform());
			Renderer::m_DebugShader->SetUniformMat4f("u_Projection", m_EditorCamera->GetPerspective());
			Renderer::m_DebugShader->SetUniformMat4f("u_Model", transform.GetTransform());
            
			TransformComponent t = transform;
			t.Scale = (box.Radius * 2.f) * transform.Scale;
			Renderer::DrawSphere(t, glm::vec4(0.0f, 0.0f, 0.9f, 0.2f));
		}
	}
}

std::vector<Entity> Scene::GetAllEntities() {
	std::vector<Entity> allEntities;
	auto view = m_Registry.view<NameComponent>();
	for (auto e : view) {
		Entity newEntity(e, this);

		// Check if valid for deleted entities.
		if(newEntity.IsValid())
			allEntities.push_back(newEntity);
	}
	return allEntities;
}

glm::vec3 Scene::GetGlobalPosition(Entity ent)
{
	glm::vec3 globalPos = ent.GetComponent<TransformComponent>().Translation;
	Entity currentParent = ent.AddComponent<ParentComponent>().Parent;
	while (currentParent.GetHandle() != -1)
	{
		globalPos += currentParent.GetComponent<TransformComponent>().Translation;
		currentParent = currentParent.GetComponent<ParentComponent>().Parent;
	}
    
	return globalPos;
}


Entity Scene::GetEntity(const std::string& name)
{
	std::vector<Entity> allEntities;
	auto view = m_Registry.view<TransformComponent, NameComponent>();
	for (auto e : view) {
		auto [transform, namec] = view.get<TransformComponent, NameComponent>(e);
		if (namec.Name == name)
			return Entity{ e, this };
	}
}

Entity Scene::CreateEmptyEntity() {
	Entity entity = { m_Registry.create(), this };
	return entity;
}

Entity Scene::CreateEntity(const std::string& name) {
	Entity entity = { m_Registry.create(), this };
    
	// Must have transform
	entity.AddComponent<TransformComponent>();
    
	NameComponent& nameComponent = entity.AddComponent<NameComponent>();
	nameComponent.Name = name;
	nameComponent.ID = (int)OS::GetTime();
    
	ParentComponent& parentComponent = entity.AddComponent<ParentComponent>();
    
	Logger::Log("Created entity: " + nameComponent.Name + "\n");
	return entity;
}

void Scene::DestroyEntity(Entity entity) 
{
	ParentComponent& parentC = entity.GetComponent<ParentComponent>();
	std::vector<Entity> copyChildrens = parentC.Children;
	if (parentC.HasParent) {  // Remove self from parents children lists.
		ParentComponent& parent = parentC.Parent.GetComponent<ParentComponent>();
		parent.RemoveChildren(entity);
	}
	for (auto& c : copyChildrens) {
		Logger::Log("Deleting... entity" + std::to_string(c.GetHandle()));
		DestroyEntity(c);
	}

	Logger::Log("Deleted entity" + std::to_string(entity.GetHandle()) + " - " + entity.GetComponent<NameComponent>().Name);
	entity.Destroy();
	m_Registry.shrink_to_fit();
}

// Getter
Ref<Camera> Scene::GetCurrentCamera()
{
	if (Engine::IsPlayMode)
	{
		Ref<Camera> cam = nullptr;
		{
			auto view = m_Registry.view<TransformComponent, CameraComponent>();
			for (auto e : view) {
				auto [transform, camera] = view.get<TransformComponent, CameraComponent>(e);
				cam = camera.CameraInstance;
				break;
			}
		}
		if (!cam)
			cam = m_EditorCamera;
		return cam;
	}
    
	return m_EditorCamera;
}

Ref<Environment> Scene::GetEnvironment() {
	return m_Environement;
}

bool Scene::Save()
{
	if (Path == "")
		Path = FileSystem::AbsoluteToRelative(FileDialog::SaveFile("*.scene") + ".scene");
    
	return SaveAs(Path);
}

bool Scene::SaveAs(const std::string& path)
{
	std::ofstream sceneFile;
	sceneFile.open(FileSystem::Root + path);
	sceneFile << Serialize().dump(4);
	sceneFile.close();
    
	Logger::Log("Scene saved successfully");
	return true;
}


void Scene::ReloadInterfaces()
{
	for (auto& i : m_Interfaces)
		i->Reload();
}


void Scene::AddInterface(Ref<UI::UserInterface> interface)
{
	this->m_Interfaces.push_back(interface);
}


json Scene::Serialize()
{
	BEGIN_SERIALIZE();
    SERIALIZE_VAL(Name);
    SERIALIZE_OBJECT(m_Environement)
		std::vector<json> entities = std::vector<json>();
    for (Entity e : GetAllEntities())
        entities.push_back(e.Serialize());
    SERIALIZE_VAL_LBL("Entities", entities);
	END_SERIALIZE();
}


bool Scene::Deserialize(const std::string& str)
{
	if (str == "")
		return false;
    
	BEGIN_DESERIALIZE();
	if (!j.contains("Name"))
		return false;
    
	Name = j["Name"];
    
	m_Environement = CreateRef<Environment>();
	if (j.contains("m_Environement"))
	{
		m_Environement = CreateRef<Environment>();
		std::string env = j["m_Environement"].dump();
		m_Environement->Deserialize(env);
	}
    
	// Parse entities
	{
		if (j.contains("Entities"))
		{
			for (json e : j["Entities"])
			{
				std::string name = e["NameComponent"]["Name"];
				Entity ent = CreateEmptyEntity();
				ent.Deserialize(e.dump());
			}

			auto view = m_Registry.view<ParentComponent>();
			for (auto e : view) {
				auto parentC = view.get<ParentComponent>(e);
				if (!parentC.HasParent)
					continue;
				auto& p = Entity{ e, this };
		
				auto parent = GetEntityByID(parentC.ParentID);
				parent.AddChild(p);
			}
		}
	}
    
	return true;
}

void Scene::Snapshot()
{
	const auto view = m_Registry.view<TransformComponent>();

}

void Scene::LoadSnapshot()
{
}
