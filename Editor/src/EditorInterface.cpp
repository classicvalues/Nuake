#include "EditorInterface.h"
#include "src/Vendors/imgui/imgui.h"
#include <src/Vendors/imgui/ImGuizmo.h>
#include <src/Rendering/Textures/Texture.h>
#include <Engine.h>
#include <src/Scene/Entities/Components/LightComponent.h>
#include <src/Core/Physics/PhysicsManager.h>
#include <src/Scene/Entities/Components.h>
#include <src/Vendors/glm/gtc/type_ptr.hpp>
#include <src/Vendors/glm/gtx/matrix_decompose.hpp>
#include "src/Scene/Entities/ImGuiHelper.h"
#include <map>
#include <src/Core/MaterialManager.h>

#include "../Resource/FontAwesome5.h"
#include <src/Scene/Entities/Components/BoxCollider.h>
#include <algorithm>
#include <Dependencies/GLEW/include/GL/glew.h>

ImFont* normalFont;
ImFont* bigIconFont;
void EditorInterface::Init()
{
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
    static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_None;
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->GetWorkPos());
    ImGui::SetNextWindowSize(viewport->GetWorkSize());
    ImGui::SetNextWindowViewport(viewport->ID);

    ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
    ImGui::DockSpaceOverViewport(viewport, dockspace_flags);
}

void EditorInterface::DrawViewport()
{
    ImGui::Begin("ShadowMap");
    {
        ImVec2 regionAvail = ImGui::GetContentRegionAvail();
        glm::vec2 viewportPanelSize = glm::vec2(regionAvail.x, regionAvail.y);
        if (m_IsEntitySelected && m_SelectedEntity.HasComponent<LightComponent>()) {
            auto& light = m_SelectedEntity.GetComponent<LightComponent>();
            Ref<Texture> texture = light.m_Framebuffer->GetTexture(GL_DEPTH_ATTACHMENT);
            ImGui::Image((void*)texture->GetID(), regionAvail, ImVec2(0, 1), ImVec2(1, 0));
        }
        else {
            ImGui::Text("Please select a light entity");
        }
        ImGui::End();
    }
    ImGui::Begin("Viewport");
    {
        Overlay();
        ImGuizmo::BeginFrame();

        ImGuizmo::SetOrthographic(false);
        ImVec2 regionAvail = ImGui::GetContentRegionAvail();
        glm::vec2 viewportPanelSize = glm::vec2(regionAvail.x, regionAvail.y);

        Ref<Texture> texture = Engine::GetCurrentWindow()->GetSceneRenderTexture();
        ImGui::Image((void*)texture->GetID(), regionAvail, ImVec2(0, 1), ImVec2(1, 0));

        ImGuizmo::SetDrawlist();
        ImGuizmo::SetRect(ImGui::GetWindowPos().x, ImGui::GetWindowPos().y, ImGui::GetWindowWidth(), ImGui::GetWindowHeight());

        if (m_DrawGrid)
        {
            ImGuizmo::DrawGrid(glm::value_ptr(Engine::GetCurrentScene()->GetCurrentCamera()->GetTransform()),
                glm::value_ptr(Engine::GetCurrentScene()->GetCurrentCamera()->GetPerspective()),
                glm::value_ptr(glm::identity<glm::mat4>()), 100.f);
        }

        if (m_IsEntitySelected)
        {
            TransformComponent& tc = m_SelectedEntity.GetComponent<TransformComponent>();
            glm::mat4 oldTransform = tc.GetTransform();
            ImGuizmo::Manipulate(
                glm::value_ptr(Engine::GetCurrentScene()->GetCurrentCamera()->GetTransform()),
                glm::value_ptr(Engine::GetCurrentScene()->GetCurrentCamera()->GetPerspective()),
                CurrentOperation, CurrentMode, glm::value_ptr(oldTransform), 0, 0
            );
            if (ImGuizmo::IsUsing())
            {
                glm::vec3 scale;
                glm::quat rotation;
                glm::vec3 translation;
                glm::vec3 skew;
                glm::vec4 perspective;
                glm::decompose(oldTransform, scale, rotation, translation, skew, perspective);
                //rotation = glm::conjugate(rotation);
                if (scale.x < 0 && scale.y < 0 && scale.z < 0)
                    scale = glm::vec3(0, 0, 0);
                rotation = glm::conjugate(rotation);
                glm::vec3 euler = glm::eulerAngles(rotation);
                tc.Translation = translation;
                tc.Rotation = glm::vec3(glm::degrees(euler.x), glm::degrees(euler.y), glm::degrees(euler.z));
                tc.Scale = scale;
            }
            
        }
        

        ImGui::End();
    }
}

static int selected = 0;
void EditorInterface::DrawEntityTree(Entity e)
{
    ImGuiTreeNodeFlags base_flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth;
    
    std::string name = e.GetComponent<NameComponent>().Name;
    ParentComponent& parent = e.GetComponent<ParentComponent>();

    if (m_SelectedEntity == e)
        base_flags |= ImGuiTreeNodeFlags_Selected;

    // Write in normal font.
    ImGui::PushFont(normalFont);

    // If has no childrens draw tree node leaf
    if (parent.Children.size() <= 0)
        base_flags |= ImGuiTreeNodeFlags_Leaf;

    bool open = ImGui::TreeNodeEx(name.c_str(), base_flags);

    if (ImGui::BeginDragDropSource())
    {
        ImGui::SetDragDropPayload("ENTITY", (void*)&e, sizeof(Entity));
        ImGui::Text(name.c_str());
        ImGui::EndDragDropSource();
    }

    if (ImGui::BeginDragDropTarget())
    {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ENTITY"))
        {
            Entity payload_entity = *(const Entity*)payload->Data;
            
            // Check if entity is already parent.
            ParentComponent& parentPayload = payload_entity.GetComponent<ParentComponent>();
            if (!EntityContainsItself(payload_entity, e) && parentPayload.Parent != e && std::count(parent.Children.begin(), parent.Children.end(), payload_entity) == 0)
            {
                if (parentPayload.HasParent)
                {
                    // Erase remove idiom.
                    ParentComponent& childOfParent = parentPayload.Parent.GetComponent<ParentComponent>();
                    childOfParent.Children.erase(std::remove(childOfParent.Children.begin(), childOfParent.Children.end(), payload_entity), childOfParent.Children.end());
                }

                parentPayload.Parent = e;
                parentPayload.HasParent = true;
                parent.Children.push_back(payload_entity);
            }
        }
    }

    // Click.
    if (ImGui::IsItemClicked())
    {
        m_SelectedEntity = e;
        m_IsEntitySelected = true;
    }

    if (open)
    {
        for (auto child : parent.Children)
            DrawEntityTree(child);
        
        ImGui::TreePop();
    }

    ImGui::PopFont();
}




void EditorInterface::DrawSceneTree()
{
    Ref<Scene> scene = Engine::GetCurrentScene();

    if (ImGui::Begin("Environnement"))
    {
        auto env = Engine::GetCurrentScene()->GetEnvironment();
        if (ImGui::CollapsingHeader("Procedural Sky"))
        {
            ImGui::DragFloat("Sun Intensity", &env->ProceduralSkybox->SunIntensity, 0.1f, 0.0f);

            glm::vec3 currentDirection = env->ProceduralSkybox->SunDirection;
            ImGuiHelper::DrawVec3("Sun Direction", &currentDirection);
            env->ProceduralSkybox->SunDirection = glm::normalize(currentDirection);

            glm::vec3 mieScattering = env->ProceduralSkybox->MieScattering * 10000.0f;
            ImGuiHelper::DrawVec3("Mie Scattering", &mieScattering);
            env->ProceduralSkybox->MieScattering = mieScattering / 10000.0f;
        }
        if (ImGui::CollapsingHeader("Fog"))
        {
            ImGui::DragFloat("Volumetric scattering", &env->VolumetricFog, .01f, 0.0f, 1.0f);
            ImGui::DragFloat("Volumetric step count", &env->VolumetricStepCount, 1.f, 0.0f);
        }
    }
    
    ImGui::Begin("Scene");
    {
        // Buttons to add and remove entity.
        ImGui::BeginChild("Buttons", ImVec2(300, 20), false);
        {

            // Add entity.
            if (ImGui::Button("Add"))
                Engine::GetCurrentScene()->CreateEntity("Entity");

            ImGui::SameLine();

            // Remove Entity
            if (ImGui::Button("Remove"))
            {
                scene->DestroyEntity(m_SelectedEntity);

                // Unselect delted entity.
                m_SelectedEntity = scene->GetAllEntities().at(0);
            }

            ImGui::EndChild();
        }

        // Draw a tree of entities.
        for (Entity e : scene->GetAllEntities()) 
        {
            ImGuiTreeNodeFlags base_flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth;
            std::string name = e.GetComponent<NameComponent>().Name;

            // If selected add selected flag.
            if (m_SelectedEntity == e)
                base_flags |= ImGuiTreeNodeFlags_Selected;

            // Write in normal font.
            ImGui::PushFont(normalFont);

            // Small icons + name.
            std::string label = ICON_FA_CIRCLE + std::string(" ") + name;
            
            // Draw all entity without parents.
            if (!e.GetComponent<ParentComponent>().HasParent)
            {
                // Recursively draw childrens.
                DrawEntityTree(e);
            }

            // Pop font.
            ImGui::PopFont();

            // Right click menu
            //if (ImGui::BeginPopupContextItem())
            //    ImGui::EndPopup();
        }

        ImGui::End();
    }
    ImGui::End();
}



void EditorInterface::DrawEntityPropreties()
{
    ImGui::Begin("Propreties");
    {
        if (!m_IsEntitySelected) 
        {
            ImGui::Text("No entity selected");
            ImGui::End();
            return;
        }
        

        if(m_SelectedEntity.HasComponent<NameComponent>()) {
            auto& name = m_SelectedEntity.GetComponent<NameComponent>().Name;

            char buffer[256];
            memset(buffer, 0, sizeof(buffer));
            std::strncpy(buffer, name.c_str(), sizeof(buffer));
            if (ImGui::InputText("##Name", buffer, sizeof(buffer)))
            {
                name = std::string(buffer);
            }
            ImGui::SameLine();
            if (ImGui::Button("Add component"))
            {
                ImGui::OpenPopup("add_component_popup");

            }
            if (ImGui::BeginPopup("add_component_popup"))
            {
                if (ImGui::MenuItem("Light Component") && !m_SelectedEntity.HasComponent<LightComponent>())
                    m_SelectedEntity.AddComponent<LightComponent>();
                if (ImGui::MenuItem("Mesh Component") && !m_SelectedEntity.HasComponent<MeshComponent>())
                    m_SelectedEntity.AddComponent<MeshComponent>();
                if (ImGui::MenuItem("Camera Component") && !m_SelectedEntity.HasComponent<CameraComponent>())
                    m_SelectedEntity.AddComponent<CameraComponent>();
                if (ImGui::MenuItem("Quake map Component") && !m_SelectedEntity.HasComponent<QuakeMap>())
                    m_SelectedEntity.AddComponent<QuakeMap>();
                if (ImGui::MenuItem("Rigidbody Component") && !m_SelectedEntity.HasComponent<RigidBodyComponent>())
                {
                    m_SelectedEntity.AddComponent<RigidBodyComponent>();
                    auto transformComponent = m_SelectedEntity.GetComponent<TransformComponent>();
                }
                if (ImGui::MenuItem("Box collider Component") && !m_SelectedEntity.HasComponent<BoxColliderComponent>())
                {
                    m_SelectedEntity.AddComponent<BoxColliderComponent>();
                }
                if (ImGui::MenuItem("Sphere collider Component") && !m_SelectedEntity.HasComponent<SphereColliderComponent>())
                {
                    m_SelectedEntity.AddComponent<SphereColliderComponent>();
                }
                if (ImGui::MenuItem("Mesh collider Component") && !m_SelectedEntity.HasComponent<MeshColliderComponent>())
                {
                    m_SelectedEntity.AddComponent<MeshColliderComponent>();
                }

                // your popup code
                ImGui::EndPopup();
            }
            ImGui::Separator();
        }

        ImGui::PushFont(normalFont);
        std::string iconTransform = ICON_FA_MAP_MARKER;
        if(ImGui::CollapsingHeader((iconTransform + " Transform").c_str(), ImGuiTreeNodeFlags_DefaultOpen))
        {
            //ImGui::InputText("Name:", selectedEntity->m_Name.data(), 12);
            TransformComponent& component = m_SelectedEntity.GetComponent<TransformComponent>();
            component.DrawEditor();
        }

        if (m_SelectedEntity.HasComponent<LightComponent>()) {
            std::string icon = ICON_FA_LIGHTBULB;
            if (ImGui::CollapsingHeader((icon + " " + "Light").c_str(), ImGuiTreeNodeFlags_DefaultOpen))
            {
                m_SelectedEntity.GetComponent<LightComponent>().DrawEditor();
                ImGui::Separator();
            }

        }

        if (m_SelectedEntity.HasComponent<CameraComponent>()) {

            std::string icon = ICON_FA_LIGHTBULB;
            if (ImGui::CollapsingHeader((icon + " " + "Camera").c_str(), ImGuiTreeNodeFlags_DefaultOpen))
            {
                m_SelectedEntity.GetComponent<CameraComponent>().DrawEditor();
                ImGui::Separator();
            }
           
        }

        if (m_SelectedEntity.HasComponent<MeshComponent>()) {

            std::string icon = ICON_FA_TREE;
            if (ImGui::CollapsingHeader((icon + " " + "Mesh").c_str(), ImGuiTreeNodeFlags_DefaultOpen))
            {
                ImGui::Button("Drag Material");
                if (ImGui::BeginDragDropTarget())
                {
                    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("DND_DEMO_CELL"))
                    {
                        IM_ASSERT(payload->DataSize == sizeof(int));
                        int payload_n = *(const int*)payload->Data;
                        
                    }
                    ImGui::EndDragDropTarget();
                }
            }

        }

        if (m_SelectedEntity.HasComponent<RigidBodyComponent>())
        {
            std::string icon = ICON_FA_BOWLING_BALL ;
            if (ImGui::CollapsingHeader((icon + " Rigidbody").c_str(), ImGuiTreeNodeFlags_DefaultOpen))
            {
                RigidBodyComponent& rbComponent = m_SelectedEntity.GetComponent<RigidBodyComponent>();
                ImGui::DragFloat("Mass", &rbComponent.mass, 0.1, 0.0);
                ImGui::Separator();
            }
        }

        if (m_SelectedEntity.HasComponent<BoxColliderComponent>())
        {
            std::string icon = ICON_FA_BOX;
            if (ImGui::CollapsingHeader((icon + " Box collider").c_str(), ImGuiTreeNodeFlags_DefaultOpen))
            {
                BoxColliderComponent& component = m_SelectedEntity.GetComponent<BoxColliderComponent>();
                ImGuiHelper::DrawVec3("Size", &component.Size);
                ImGui::Checkbox("Is trigger", &component.IsTrigger);

                ImGui::Separator();
            }
        }
        if (m_SelectedEntity.HasComponent<SphereColliderComponent>())
        {
            std::string icon = ICON_FA_CIRCLE;
            if (ImGui::CollapsingHeader((icon + " Sphere collider").c_str(), ImGuiTreeNodeFlags_DefaultOpen))
            {
                SphereColliderComponent& component = m_SelectedEntity.GetComponent<SphereColliderComponent>();
                ImGui::DragFloat("Radius", &component.Radius, 0.1f, 0.0f, 100.0f);
                ImGui::Checkbox("Is trigger", &component.IsTrigger);

                ImGui::Separator();
            }
        }
        if (m_SelectedEntity.HasComponent<QuakeMap>())
        {
            std::string icon = ICON_FA_BROOM ;
            if (ImGui::CollapsingHeader((icon + " " + "Quake map").c_str(), ImGuiTreeNodeFlags_DefaultOpen))
            {
                auto& component = m_SelectedEntity.GetComponent<QuakeMap>();
                std::string path = component.Path;


                char pathBuffer[256];
                memset(pathBuffer, 0, sizeof(pathBuffer));
                std::strncpy(pathBuffer, path.c_str(), sizeof(pathBuffer));
                if (ImGui::InputText("##MapPath", pathBuffer, sizeof(pathBuffer)))
                {
                    path = std::string(pathBuffer);
                }
                ImGui::SameLine();
                if (ImGui::Button("Browse"))
                {
                    path = FileDialog::OpenFile(".map");
                }

                component.Path = path;

                ImGui::Checkbox("Build collisions?", &component.HasCollisions);
                if (ImGui::Button("Build"))
                    component.Build();
                ImGui::Separator();
            }
        }

        ImGui::PopFont();
        ImGui::End();
    }
}
void EditorInterface::DrawGizmos()
{
    Ref<Scene> scene = Engine::GetCurrentScene();
    
    
    if (!m_IsEntitySelected)
        return;

}

void EditorInterface::EditorInterfaceDrawFiletree(Ref<Directory> dir)
{
    for (auto d : dir->Directories)
    {
        ImGuiTreeNodeFlags base_flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth;
        bool is_selected = m_CurrentDirectory == d;
        if (is_selected)
            base_flags |= ImGuiTreeNodeFlags_Selected;

        if (d->Directories.size() == 0)
        {
            base_flags = ImGuiTreeNodeFlags_Leaf;
        }
        std::string icon = ICON_FA_FOLDER;
        if (is_selected)
            icon = ICON_FA_FOLDER_OPEN;
        bool open = ImGui::TreeNodeEx((icon + " " + d->name).c_str(), base_flags);
       
        if (ImGui::IsItemClicked())
            m_CurrentDirectory = d;
        if (open)
        {
            if (d->Directories.size() > 0)
                EditorInterfaceDrawFiletree(d);
            ImGui::TreePop();
        }
       
    }
}

void EditorInterface::DrawFileSystem()
{
    Ref<Directory> rootDirectory = FileSystem::GetFileTree();
    ImGui::Begin("Tree browser");
    {
        if (ImGui::BeginPopupContextItem("item context menu"))
        {
            if (ImGui::Selectable("Set to zero"));
            if (ImGui::Selectable("Set to PI"));
            ImGui::SetNextItemWidth(-1);
            ImGui::EndPopup();
        }
        ImGuiTreeNodeFlags base_flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth;
        bool is_selected = m_CurrentDirectory == FileSystem::RootDirectory;
        if (is_selected)
            base_flags |= ImGuiTreeNodeFlags_Selected;
        std::string icon = ICON_FA_FOLDER;

        bool open = ImGui::TreeNodeEx((icon + " " + rootDirectory->name).c_str(), base_flags);
        if (ImGui::IsItemClicked())
        {
            m_CurrentDirectory = FileSystem::RootDirectory;
        }
        if (open)
        {
            EditorInterfaceDrawFiletree(rootDirectory);
            ImGui::TreePop();
        }

        ImGui::End();
    }


}

void EditorInterface::DrawDirectory(Ref<Directory> directory)
{
    ImGui::PushFont(bigIconFont);
    std::string id = ICON_FA_FOLDER + std::string("##") + directory->name;
    if(ImGui::Button(id.c_str(), ImVec2(100, 100)))
        m_CurrentDirectory = directory;
    ImGui::Text(directory->name.c_str());
    ImGui::PopFont();
}

bool EditorInterface::EntityContainsItself(Entity source, Entity target)
{
    ParentComponent& targeParentComponent = target.GetComponent<ParentComponent>();
    if (!targeParentComponent.HasParent)
        return false;

    Entity currentParent = target.GetComponent<ParentComponent>().Parent;
    while (currentParent != source)
    {
        if (currentParent.GetComponent<ParentComponent>().HasParent)
            currentParent = currentParent.GetComponent<ParentComponent>().Parent;
        else
            return false;

        if (currentParent == source)
            return true;
    }
    return true;
}

void EditorInterface::DrawFile(Ref<File> file)
{
    ImGui::PushFont(bigIconFont);
    if (file->Type == ".png" || file->Type == ".jpg")
    {
        Ref<Texture> texture = TextureManager::Get()->GetTexture(file->fullPath);
        ImGui::ImageButton((void*)texture->GetID(), ImVec2(100, 100), ImVec2(0, 1), ImVec2(1, 0));
    }
    else
    {
        const char* icon = ICON_FA_FILE;
        if (file->Type == ".shader")
            icon = ICON_FA_FILE_CODE;
        if (file->Type == ".map")
            icon = ICON_FA_BROOM;
        if (file->Type == ".ogg" || file->Type == ".mp3" || file->Type == ".wav" || file->Type == ".flac")
            icon = ICON_FA_FILE_AUDIO;
        if(file->Type == ".cpp" || file->Type == ".h" || file->Type == ".cs" || file->Type == ".py" || file->Type == ".lua")
            icon = ICON_FA_FILE_CODE;
        if (ImGui::Button(icon, ImVec2(100, 100)))
        {
            if (ImGui::BeginPopupContextItem("item context menu"))
            {
                if (ImGui::Selectable("Set to zero"));
                if (ImGui::Selectable("Set to PI"));
                ImGui::EndPopup();
            }
        }
    }
    ImGui::Text(file->name.c_str());
    ImGui::PopFont();

}

void EditorInterface::DrawDirectoryExplorer()
{
    ImGui::Begin("File browser");
    {

        // Wrapping.
        int width = ImGui::GetWindowWidth();
        ImVec2 buttonSize = ImVec2(100, 100);
        int amount = (width / 100); // -2 because button overflow width + ... button.
        int i = 1; // current amount of item per row.
        if (ImGui::BeginTable("ssss", amount))
        {
            // Button to go up a level.
            if (m_CurrentDirectory != FileSystem::RootDirectory)
            {
                ImGui::TableNextColumn();
                if (ImGui::Button("..", ImVec2(100, 100)))
                    m_CurrentDirectory = m_CurrentDirectory->Parent;
                ImGui::TableNextColumn();
                // Increment item per row tracker.
                i++;
            }
            // Exit if no current directory.
            if (!m_CurrentDirectory) {
                ImGui::EndTable();
                ImGui::End();
                return;
            }

            if (m_CurrentDirectory && m_CurrentDirectory->Directories.size() > 0)
            {
                for (auto d : m_CurrentDirectory->Directories)
                {
                    DrawDirectory(d);
                    if (i - 1% amount != 0) 
                        ImGui::TableNextColumn();
                    else
                        ImGui::TableNextRow();
                    i++;
                }
                
            
            }
            if (m_CurrentDirectory && m_CurrentDirectory->Files.size() > 0)
            {
                for (auto f : m_CurrentDirectory->Files)
                {
                    DrawFile(f);
                    if (i - 1 % amount != 0)
                        ImGui::TableNextColumn();
                    else
                        ImGui::TableNextRow();
                    i++;
                }
            }


            ImGui::EndTable();
        }

       
        

       

        // Display directories first.
       

        // Then files.
       

        ImGui::End();
    }
}



void EditorInterface::Overlay()
{
    // FIXME-VIEWPORT: Select a default viewport
    const float DISTANCE = 10.0f;
    static int corner = 0;
    ImGuiIO& io = ImGui::GetIO();
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav;
    if (corner != -1)
    {
        window_flags |= ImGuiWindowFlags_NoMove;
        ImGuiViewport* viewport = ImGui::GetWindowViewport();
        ImVec2 work_area_pos = ImGui::GetCurrentWindow()->Pos;   // Instead of using viewport->Pos we use GetWorkPos() to avoid menu bars, if any!
        ImVec2 work_area_size = ImGui::GetCurrentWindow()->Size;
        ImVec2 window_pos = ImVec2((corner & 1) ? (work_area_pos.x + work_area_size.x - DISTANCE) : (work_area_pos.x + DISTANCE), (corner & 2) ? (work_area_pos.y + work_area_size.y - DISTANCE) : (work_area_pos.y + DISTANCE));
        ImVec2 window_pos_pivot = ImVec2((corner & 1) ? 1.0f : 0.0f, (corner & 2) ? 1.0f : 0.0f);
        ImGui::SetNextWindowPos(window_pos, ImGuiCond_Always, window_pos_pivot);
        ImGui::SetNextWindowViewport(viewport->ID);
    }
    ImGui::SetNextWindowBgAlpha(0.35f); // Transparent background
    if (ImGui::Begin("Example: Simple overlay", &m_ShowOverlay, window_flags))
    {
        ImGui::Text("Developpement build 0.1");
        ImGui::Separator();
        ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
        if (ImGui::BeginPopupContextWindow())
        {
            if (ImGui::MenuItem("Custom", NULL, corner == -1)) corner = -1;
            if (ImGui::MenuItem("Top-left", NULL, corner == 0)) corner = 0;
            if (ImGui::MenuItem("Top-right", NULL, corner == 1)) corner = 1;
            if (ImGui::MenuItem("Bottom-left", NULL, corner == 2)) corner = 2;
            if (ImGui::MenuItem("Bottom-right", NULL, corner == 3)) corner = 3;
            if (m_ShowOverlay && ImGui::MenuItem("Close")) m_ShowOverlay = false;
            ImGui::EndPopup();
        }
    }
    ImGui::End();
}


void EditorInterface::DrawRessourceWindow()
{
    std::map<std::string, Ref<Material>> materials = MaterialManager::Get()->GetAllMaterials();

    ImGui::Begin("Materials");
    {
        int width = ImGui::GetWindowWidth();
        ImVec2 buttonSize = ImVec2(100, 100);
        int amount = (width / 100) - 1;
        if (amount > 0)
        {
            int i = 1;
            for (auto m : materials)
            {
                unsigned int textureID = 0;
                if (m.second->HasAlbedo())
                    textureID = m.second->m_Albedo->GetID();
                std::string id = "materialButton" +  std::to_string(i);
                if (ImGui::ImageButtonEx(ImGui::GetCurrentWindow()->GetID(id.c_str()), (void*)textureID, ImVec2(100, 100), ImVec2(0, 1), ImVec2(1, 0), ImVec2(2, 2), ImVec4(0, 0, 0, 0), ImVec4(1, 1, 1, 1)))
                {
                    m_SelectedMaterial = m.second;
                    m_IsMaterialSelected = true;
                }
                if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None))
                {
                    // Set payload to carry the index of our item (could be anything)
                    ImGui::SetDragDropPayload("MaterialName", &m.first, sizeof(int));

                    // Display preview (could be anything, e.g. when dragging an image we could decide to display
                    // the filename and a small preview of the image, etc.)
                    ImGui::Text("hello");
                    ImGui::EndDragDropSource();
                }
                if (i % amount != 0)
                    ImGui::SameLine();
                i++;
            }
        }
        
        ImGui::End();
    }

    ImGui::Begin("Material property");
    {
        if (m_IsMaterialSelected)
        {
            if (ImGui::CollapsingHeader("Albedo", ImGuiTreeNodeFlags_DefaultOpen))
            {
                unsigned int textureID = 0;
                if (m_SelectedMaterial->HasAlbedo())
                    textureID = m_SelectedMaterial->m_Albedo->GetID();
                if (ImGui::ImageButtonEx(ImGui::GetCurrentWindow()->GetID("#image1"), (void*)textureID, ImVec2(80, 80), ImVec2(0, 1), ImVec2(1, 0), ImVec2(2, 2), ImVec4(0, 0, 0, 0), ImVec4(1, 1, 1, 1)))
                {
                    std::string texture = FileDialog::OpenFile("*.png | *.jpg");
                }
                ImGui::SameLine();
                ImGui::Checkbox("Use##1", &m_SelectedMaterial->UseAlbedo);
                ImGui::SameLine();
                ImGui::ColorPicker4("Color", &m_SelectedMaterial->m_AlbedoColor.r);
            }
            if (ImGui::CollapsingHeader("AO", ImGuiTreeNodeFlags_DefaultOpen))
            {
                unsigned int textureID = 0;
                if (m_SelectedMaterial->HasAO())
                    textureID = m_SelectedMaterial->m_AO->GetID();
                if (ImGui::ImageButtonEx(ImGui::GetCurrentWindow()->GetID("#image2"), (void*)textureID, ImVec2(80, 80), ImVec2(0, 1), ImVec2(1, 0), ImVec2(2, 2), ImVec4(1, 1, 1, 1), ImVec4(1, 1, 1, 1)))
                {
                    std::string texture = FileDialog::OpenFile("Image files (*.png) | *.png | Image files (*.jpg) | *.jpg");
                    if (texture != "")
                    {
                        m_SelectedMaterial->SetAO(TextureManager::Get()->GetTexture(texture));
                    }
                }
                ImGui::SameLine();
                ImGui::Checkbox("Use##2", &m_SelectedMaterial->UseAO);
                ImGui::SameLine();
                ImGui::DragFloat("Value##2", &m_SelectedMaterial->m_AOValue, 0.01f, 0.0f, 1.0f);
            }
            if (ImGui::CollapsingHeader("Normal", ImGuiTreeNodeFlags_DefaultOpen))
            {
                unsigned int textureID = 0;
                if (m_SelectedMaterial->HasNormal())
                    textureID = m_SelectedMaterial->m_Normal->GetID();
                if (ImGui::ImageButtonEx(ImGui::GetCurrentWindow()->GetID("#image3"), (void*)textureID, ImVec2(80, 80), ImVec2(0, 1), ImVec2(1, 0), ImVec2(2,2), ImVec4(0,0,0,1), ImVec4(1, 1, 1, 1)))
                {
                    std::string texture = FileDialog::OpenFile("*.png|*.jpg");
                    if (texture != "")
                    {
                        m_SelectedMaterial->SetNormal(TextureManager::Get()->GetTexture(texture));
                    }
                }
                ImGui::SameLine();
                ImGui::Checkbox("Use##3", &m_SelectedMaterial->UseNormal);
            }
            if (ImGui::CollapsingHeader("Metalness", ImGuiTreeNodeFlags_DefaultOpen))
            {
                unsigned int textureID = 0;
                if (m_SelectedMaterial->HasMetalness())
                    textureID = m_SelectedMaterial->m_Metalness->GetID();
                if (ImGui::ImageButtonEx(ImGui::GetCurrentWindow()->GetID("#image4"), (void*)textureID, ImVec2(80, 80), ImVec2(0, 1), ImVec2(1, 0), ImVec2(2, 2), ImVec4(0, 0, 0, 1), ImVec4(1, 1, 1, 1)))
                {
                    std::string texture = FileDialog::OpenFile("*.png | *.jpg");
                }
                ImGui::SameLine();
                ImGui::Checkbox("Use##4", &m_SelectedMaterial->UseMetalness);
                ImGui::SameLine();
                ImGui::DragFloat("Value##4", &m_SelectedMaterial->m_MetalnessValue, 0.01f, 0.0f, 1.0f);
            }
            if (ImGui::CollapsingHeader("Roughness", ImGuiTreeNodeFlags_DefaultOpen))
            {
                unsigned int textureID = 0;
                if (m_SelectedMaterial->HasRougness())
                    textureID = m_SelectedMaterial->m_Roughness->GetID();
                if (ImGui::ImageButtonEx(ImGui::GetCurrentWindow()->GetID("#image5"), (void*)textureID, ImVec2(80, 80), ImVec2(0, 1), ImVec2(1, 0), ImVec2(2, 2), ImVec4(0, 0, 0, 0), ImVec4(1, 1, 1, 1)))
                {
                    std::string texture = FileDialog::OpenFile("*.png | *.jpg");
                }
                ImGui::SameLine();
                ImGui::Checkbox("Use##5", &m_SelectedMaterial->UseRoughness);
                ImGui::SameLine();
                ImGui::DragFloat("Value##5", &m_SelectedMaterial->m_RoughnessValue, 0.01f, 0.0f, 1.0f);
            }
        }
        else
        {
            ImGui::Text("No material selected.");
        }

        ImGui::End();
    }

}


void EditorInterface::Draw()
{
    Init();
    if (ImGui::BeginMainMenuBar())
    {
        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem("New project", "CTRL+N")) {}
            if (ImGui::MenuItem("Open...", "CTRL+O")) {
                FileDialog::OpenFile(".project");
            }
            if (ImGui::MenuItem("Save", "CTRL+S")) {}
            if (ImGui::MenuItem("Save as...", "CTRL+SHIFT+S")) {
                FileDialog::SaveFile(".project");
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Edit"))
        {
            if (ImGui::MenuItem("Undo", "CTRL+Z")) {}
            if (ImGui::MenuItem("Redo", "CTRL+Y", false, false)) {}  // Disabled item
            ImGui::Separator();
            if (ImGui::MenuItem("Cut", "CTRL+X")) {}
            if (ImGui::MenuItem("Copy", "CTRL+C")) {}
            if (ImGui::MenuItem("Paste", "CTRL+V")) {}
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("View"))
        {
            if (ImGui::MenuItem("Lighting", NULL, true)) {}
            if (ImGui::MenuItem("Draw grid", NULL, m_DrawGrid))
                m_DrawGrid = !m_DrawGrid;
            if (ImGui::MenuItem("Draw collisions", NULL, m_DebugCollisions))
            {
                m_DebugCollisions = !m_DebugCollisions;
                PhysicsManager::Get()->SetDrawDebug(m_DebugCollisions);
            }
                
            if (ImGui::MenuItem("Settings", NULL)) {}
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Entity"))
        {
            if (ImGui::BeginMenu("Create new"))
            {
                if (ImGui::MenuItem("Empty")) {
                    auto ent = Engine::GetCurrentScene()->CreateEntity("Empty entity");
                    

                }
                if (ImGui::MenuItem("Light")) {
                    auto ent = Engine::GetCurrentScene()->CreateEntity("Light");
                    ent.AddComponent<LightComponent>();
                }
                if (ImGui::MenuItem("Camera")) {
                    auto ent = Engine::GetCurrentScene()->CreateEntity("Camera");
                    ent.AddComponent<CameraComponent>();
                }
                if (ImGui::MenuItem("Rigidbody"))
                {
                    auto ent = Engine::GetCurrentScene()->CreateEntity("Rigidbody");
                    ent.AddComponent<RigidBodyComponent>();
                }
                if (ImGui::MenuItem("Trenchbroom map"))
                {
                    auto ent = Engine::GetCurrentScene()->CreateEntity("Trenchbroom map");
                    ent.AddComponent<QuakeMap>();
                }
                if (ImGui::MenuItem("Mesh")) {
                    auto ent = Engine::GetCurrentScene()->CreateEntity("Mesh");
                    ent.AddComponent<MeshComponent>();
                }
                ImGui::EndMenu();
            }
            if (ImGui::MenuItem("Delete selected", NULL));

            if (ImGui::MenuItem("Duplicate selected", NULL)) {}
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Debug"))
        {
            if (ImGui::MenuItem("Show ImGui demo", NULL, m_ShowImGuiDemo)) {
                m_ShowImGuiDemo = !m_ShowImGuiDemo;
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Quit"))
        {
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }

    DrawGizmos();
    
    DrawRessourceWindow();
    DrawViewport();
    DrawSceneTree();
    DrawFileSystem();
    DrawDirectoryExplorer();
    DrawEntityPropreties();

    if(m_ShowImGuiDemo)
        ImGui::ShowDemoWindow();

    ImGui::Begin("Physics");
    {
        if (ImGui::Button(ICON_FA_HAND_POINTER))
            CurrentOperation = ImGuizmo::OPERATION::TRANSLATE;
        ImGui::SameLine();
        if (ImGui::Button(ICON_FA_ARROWS_ALT))
            CurrentOperation = ImGuizmo::OPERATION::TRANSLATE;
        ImGui::SameLine();
        if (ImGui::Button(ICON_FA_SYNC_ALT))
            CurrentOperation = ImGuizmo::OPERATION::ROTATE;
        ImGui::SameLine();
        if (ImGui::Button(ICON_FA_EXPAND_ALT))
            CurrentOperation = ImGuizmo::OPERATION::SCALE;
        ImGui::SameLine();
        if (ImGui::Button(ICON_FA_PLAY))
        {
            Engine::EnterPlayMode();
        }
        ImGui::SameLine();
        if (ImGui::Button(ICON_FA_STOP))
        {
            Engine::ExitPlayMode();
        }
        
        ImGui::End();
    }
}


void EditorInterface::BuildFonts()
{
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    static const ImWchar icons_ranges[] = { ICON_MIN_FA, ICON_MAX_FA, 0 };
    ImFontConfig icons_config; icons_config.MergeMode = true; icons_config.PixelSnapH = true;
    normalFont = io.Fonts->AddFontFromFileTTF("resources/Fonts/fa-solid-900.ttf", 11.0f, &icons_config, icons_ranges);

    ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.5f, 0.5f));
    ImGui::GetIO().Fonts->AddFontDefault();
    icons_config.MergeMode = true;
    bigIconFont = io.Fonts->AddFontFromFileTTF("resources/Fonts/fa-solid-900.ttf", 42.0f, &icons_config, icons_ranges);
}