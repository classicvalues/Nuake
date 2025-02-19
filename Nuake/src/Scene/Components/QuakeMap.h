#pragma once
#include <string>
#include <vector>

#include "src/Rendering/Mesh/Mesh.h"
#include "src/Resource/Serializable.h"
#include "src/Scene/Systems/QuakeMapBuilder.h"

namespace Nuake {
	class QuakeMapComponent
	{
	private:

	public:
		std::vector<Ref<Mesh>> m_Meshes;
		std::string Path;
		bool HasCollisions = false;
		void Load(std::string path, bool collisions);

		void Draw();
		void DrawEditor();

		json Serialize()
		{
			BEGIN_SERIALIZE();
			SERIALIZE_VAL(HasCollisions);
			SERIALIZE_VAL(Path);
			END_SERIALIZE();
		}

		bool Deserialize(std::string str)
		{
			BEGIN_DESERIALIZE();
			this->Path = j["Path"];
			this->HasCollisions = j["HasCollisions"];

			return true;
		}
	};
}
