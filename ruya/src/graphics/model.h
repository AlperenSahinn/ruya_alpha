#pragma once
#include <unordered_map>
#include <vector>
#include <string>
#include <memory>

#include <glm/glm.hpp>
#include <tiny_gltf/tiny_gltf.h>

#include <core/ry_id.h>

#include <app_framework/scene.h>

#include "image_2d.h"
#include "mesh.h"
#include "material.h"

namespace ruya
{
	class Model3D;

	class Model3DNode
	{
		friend class Model3D;
	public:
		Model3DNode(bool isRootNode, RyID parentModelAssetID);
		~Model3DNode();
		Model3DNode(const Model3DNode&) = delete;
		Model3DNode& operator=(const Model3DNode&) = delete;
		Model3DNode(Model3DNode&&) = default;
		Model3DNode& operator=(Model3DNode&&) = default;

		const glm::mat4& GetLocalTransform() const { return transform; }
		bool HasMesh() const { return hasMesh; }
		const RyID GetMeshId() const { return meshId; }
		const std::vector<std::unique_ptr<Model3DNode>>& GetChildren() const { return children; }
		const std::string& GetName() const { return name; }

		EntityID CreateEntity(Scene& scene, EntityID parent);
		EntityID CreateEntity(Scene& scene);

	private:
		Model3DNode* CreateChildNode();

	private:
		std::string name;
		RyID parentModelAssetID;
		std::vector<std::unique_ptr<Model3DNode>> children;
		bool rootNode;
		bool hasMesh;
		RyID materialId;
		RyID meshId;
		glm::mat4 transform;
	};

	class Model3D
	{
	public:
		Model3D(RyID modelAssetID);
		~Model3D();
		Model3D(const Model3D&) = delete;
		Model3D& operator=(const Model3D&) = delete;
		Model3D(Model3D&&) = default;
		Model3D& operator=(Model3D&&) = default;

		Model3DNode* GetRootNode() const { return root.get(); }
		EntityID CreateEntityFromModel(Scene& scene);

	private:
		glm::mat4 GetNodeTransform(const tinygltf::Node& node);
		void ProcessNode(const tinygltf::Model& model, int nodeIndex, Model3DNode& modelNode, const glm::mat4& parentTransform, const std::string& parentName);
		void ProcessMesh(const tinygltf::Model& model, const tinygltf::Mesh& mesh, Model3DNode& modelNode);
		void ProcessPrimitive(const tinygltf::Model& model, const tinygltf::Primitive& primitive, Model3DNode& modelNode, const std::string& meshName);

		void ProcessPositions(const tinygltf::Model& model, const tinygltf::Primitive& primitive, std::vector<Vertex>& vertices);
		void ProcessNormals(const tinygltf::Model& model, const tinygltf::Primitive& primitive, std::vector<Vertex>& vertices);
		void ProcessTexCoords(const tinygltf::Model& model, const tinygltf::Primitive& primitive, std::vector<Vertex>& vertices);
		void ProcessTangents(const tinygltf::Model& model, const tinygltf::Primitive& primitive, std::vector<Vertex>& vertices);
		bool HasTangentData(const tinygltf::Model& model, const tinygltf::Primitive& primitive);
		void CalculateTangents(std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices);
		void ProcessIndices(const tinygltf::Model& model, const tinygltf::Primitive& primitive, std::vector<uint32_t>& indices);

		void ProcessMaterial(const tinygltf::Model& model, int materialIndex, Model3DNode& modelNode);
		std::string GetTextureName(const tinygltf::Model& model, int textureIndex, const std::string& textureType, int materialIndex);
		RyID LoadTexture(const tinygltf::Model& model, int textureIndex, ImageType imageType, const std::string& name);

		template<typename T>
		const T* GetAccessorData(const tinygltf::Model& model, int accessorIndex);
		size_t GetAccessorCount(const tinygltf::Model& model, int accessorIndex);

	private:
		RyID modelAssetID;
		std::unique_ptr<Model3DNode> root;
		tinygltf::Model gltfModel;
	};
}