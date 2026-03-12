#include "model.h"

#include <filesystem>
#include <iostream>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtx/quaternion.hpp> 
#include <glm/gtx/transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define TINYGLTF_NOEXCEPTION
#include <tiny_gltf/tiny_gltf.h>

#include <core/log.h>
#include <engine.h>
#include <app_framework/model_3d_component.h>
#include <app_framework/transform_component.h>
#include <app_framework/render_component.h>
#include <app_framework/relationship_component.h>

#include "image_2d.h"

ruya::Model3DNode::Model3DNode(bool isRootNode, RyID parentModelAssetID)
	:rootNode(isRootNode),
	hasMesh(false),
	transform(1.0f),
	parentModelAssetID(parentModelAssetID),
	meshId(RyID()),
	materialId(RyID())
{
}

ruya::Model3DNode::~Model3DNode()
{
}

ruya::Model3DNode* ruya::Model3DNode::CreateChildNode()
{
	children.push_back(std::make_unique<Model3DNode>(false, parentModelAssetID));
	return children.back().get();
}

ruya::Model3D::Model3D(RyID modelAssetID) : modelAssetID(modelAssetID)
{
	if (!modelAssetID.IsValid())
	{
		RUYA_LOG_ERROR("[Model] Model asset id not valid");
		return;
	}

	RyAsset* modelAsset = engine->GetAssetManager()->GetAsset(modelAssetID);

	std::string fullPath = ASSETS_DIR + modelAsset->path + "/" + modelAsset->name + ".gltf";

	if (fullPath.empty())
	{
		RUYA_LOG_ERROR("[Model] Empty file path provided {}", fullPath.c_str());
		return;
	}

	if (!std::filesystem::exists(fullPath))
	{
		RUYA_LOG_ERROR("[Model] File does not exist: {}", fullPath.c_str());
		return;
	}

	tinygltf::TinyGLTF loader;
	std::string err;
	std::string warn;

	bool ret = false;
	std::filesystem::path path(fullPath);
	std::string extension = path.extension().string();
	std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);

	if (extension == ".glb")
	{
		ret = loader.LoadBinaryFromFile(&gltfModel, &err, &warn, fullPath);
	}
	else if (extension == ".gltf")
	{
		ret = loader.LoadASCIIFromFile(&gltfModel, &err, &warn, fullPath);
	}
	else
	{
		RUYA_LOG_ERROR("[Model] Unsupported file format: {}", extension.c_str());
		return;
	}

	if (!warn.empty())
	{
		RUYA_LOG_ERROR("[TinyGLTF] {}", warn.c_str());
	}

	if (!err.empty())
	{
		RUYA_LOG_ERROR("[TinyGLTF] {}", err.c_str());
		return;
	}

	if (!ret)
	{
		RUYA_LOG_ERROR("[Model] Failed to parse glTF file: {}", fullPath.c_str());
		return;
	}

	root = std::make_unique<Model3DNode>(true, modelAssetID);

	int sceneIndex = gltfModel.defaultScene;
	if (sceneIndex >= 0 && sceneIndex < static_cast<int>(gltfModel.scenes.size()))
	{
		const tinygltf::Scene& scene = gltfModel.scenes[sceneIndex];

		if (scene.nodes.size() == 1)
		{
			root->name = std::to_string(modelAssetID);
			root->transform = glm::mat4(1.0f);
			ProcessNode(gltfModel, scene.nodes[0], *root, glm::mat4(1.0f), "");
		}
		else
		{
			root->name = std::to_string(modelAssetID);
			root->transform = glm::mat4(1.0f);

			for (int nodeIndex : scene.nodes)
			{
				Model3DNode* childNode = root->CreateChildNode();
				ProcessNode(gltfModel, nodeIndex, *childNode, root->transform, root->name);
			}
		}
	}
	else if (gltfModel.scenes.empty())
	{
		RUYA_LOG_ERROR("[Model] No scenes found in glTF file");
	}
	else
	{
		RUYA_LOG_ERROR("[Model] Invalid default scene index: {}", sceneIndex);
	}
}

ruya::Model3D::~Model3D()
{
}

ruya::EntityID ruya::Model3D::CreateEntityFromModel(Scene& scene)
{
	if (!root)
	{
		RUYA_LOG_ERROR("[Model] Cannot create entity from null root");
		return entt::null;
	}
	return root->CreateEntity(scene);
}

glm::mat4 ruya::Model3D::GetNodeTransform(const tinygltf::Node& node)
{
	glm::mat4 transform(1.0f);

	if (node.matrix.size() == 16)
	{
		transform = glm::make_mat4(node.matrix.data());
	}
	else
	{
		glm::vec3 translation(0.0f);
		glm::quat rotation(1.0f, 0.0f, 0.0f, 0.0f);
		glm::vec3 scale(1.0f);

		if (node.translation.size() == 3)
		{
			translation = glm::vec3(
				static_cast<float>(node.translation[0]),
				static_cast<float>(node.translation[1]),
				static_cast<float>(node.translation[2])
			);
		}

		if (node.rotation.size() == 4)
		{
			rotation = glm::quat(
				static_cast<float>(node.rotation[3]),
				static_cast<float>(node.rotation[0]),
				static_cast<float>(node.rotation[1]),
				static_cast<float>(node.rotation[2])
			);
		}

		if (node.scale.size() == 3)
		{
			scale = glm::vec3(
				static_cast<float>(node.scale[0]),
				static_cast<float>(node.scale[1]),
				static_cast<float>(node.scale[2])
			);
		}

		transform = glm::translate(translation) * glm::mat4_cast(rotation) * glm::scale(scale);
	}

	return transform;
}

void ruya::Model3D::ProcessNode(const tinygltf::Model& model, int nodeIndex, Model3DNode& modelNode, const glm::mat4& parentTransform, const std::string& parentName)
{
	if (nodeIndex < 0 || nodeIndex >= static_cast<int>(model.nodes.size()))
	{
		RUYA_LOG_ERROR("[Model] Invalid node index: {}", nodeIndex);
		return;
	}

	const tinygltf::Node& node = model.nodes[nodeIndex];

	glm::mat4 nodeTransform = GetNodeTransform(node);
	modelNode.transform = nodeTransform;
	modelNode.name = parentName + "/" + node.name;

	if (node.mesh >= 0 && node.mesh < static_cast<int>(model.meshes.size()))
	{
		const tinygltf::Mesh& mesh = model.meshes[node.mesh];
		ProcessMesh(model, mesh, modelNode);
	}
	else
	{
		modelNode.hasMesh = false;
	}

	for (int childIndex : node.children)
	{
		Model3DNode* childNode = modelNode.CreateChildNode();
		ProcessNode(model, childIndex, *childNode, modelNode.transform, modelNode.name);
	}
}

void ruya::Model3D::ProcessMesh(const tinygltf::Model& model, const tinygltf::Mesh& mesh, Model3DNode& modelNode)
{
	if (mesh.primitives.empty())
	{
		RUYA_LOG_ERROR("[Model] Mesh has no primitives");
		return;
	}

	if (mesh.primitives.size() == 1)
	{
		modelNode.hasMesh = true;
		std::string meshName = modelNode.name;
		if (!mesh.name.empty())
		{
			meshName += "/" + mesh.name;
		}
		ProcessPrimitive(model, mesh.primitives[0], modelNode, meshName);
	}
	else
	{
		RUYA_LOG_ERROR("[Model] Mesh has multiple primitives (%zu), only processing first", mesh.primitives.size());
		modelNode.hasMesh = true;
		std::string meshName = modelNode.name;
		if (!mesh.name.empty())
		{
			meshName += "/" + mesh.name;
		}
		ProcessPrimitive(model, mesh.primitives[0], modelNode, meshName);
	}
}

void ruya::Model3D::ProcessPrimitive(const tinygltf::Model& model, const tinygltf::Primitive& primitive, Model3DNode& modelNode, const std::string& meshName)
{
	std::vector<Vertex> vertices;
	std::vector<uint32_t> indices;

	ProcessPositions(model, primitive, vertices);

	if (vertices.empty())
	{
		RUYA_LOG_ERROR("[Model] No vertices found in primitive");
		return;
	}

	ProcessNormals(model, primitive, vertices);
	ProcessTexCoords(model, primitive, vertices);
	ProcessTangents(model, primitive, vertices);
	ProcessIndices(model, primitive, indices);

	if (!HasTangentData(model, primitive))
	{
		CalculateTangents(vertices, indices);
	}

	modelNode.meshId = engine->GetGraphics()->CreateMesh(vertices, indices, meshName);

	RyID albedoImageId = engine->GetGraphics()->CreateImage2D(ASSETS_DIR + "ruya_files/images/default_albedo.png", ImageType::SRGB, ImageSampler::Linear, "default_albedo");
	RyID normalImageId = engine->GetGraphics()->CreateImage2D(ASSETS_DIR + "ruya_files/images/default_normal.png", ImageType::NonColor, ImageSampler::Linear, "default_normal");
	RyID metallicRoughnessImageId = engine->GetGraphics()->CreateImage2D(ASSETS_DIR + "ruya_files/images/default_metallicRoughness.png", ImageType::NonColor, ImageSampler::Linear, "default_metallicRoughness");

	std::string materialName = "default_material";
	if (primitive.material >= 0 && primitive.material < static_cast<int>(model.materials.size()))
	{
		const tinygltf::Material& gltfMaterial = model.materials[primitive.material];
		materialName = gltfMaterial.name.empty() ? ("material_" + std::to_string(primitive.material)) : gltfMaterial.name;
	}

	modelNode.materialId = engine->GetGraphics()->CreatePBROpaqueMaterial(materialName, albedoImageId, normalImageId, metallicRoughnessImageId);

	if (primitive.material >= 0)
	{
		ProcessMaterial(model, primitive.material, modelNode);
	}
}

bool ruya::Model3D::HasTangentData(const tinygltf::Model& model, const tinygltf::Primitive& primitive)
{
	return primitive.attributes.find("TANGENT") != primitive.attributes.end();
}

void ruya::Model3D::ProcessTangents(const tinygltf::Model& model, const tinygltf::Primitive& primitive, std::vector<Vertex>& vertices)
{
	auto it = primitive.attributes.find("TANGENT");

	if (it == primitive.attributes.end())
	{
		for (auto& vertex : vertices)
		{
			vertex.tangent = glm::vec3(0.0f);
			vertex.bitangent = glm::vec3(0.0f);
		}
		return;
	}

	const float* tangents = GetAccessorData<float>(model, it->second);
	if (!tangents)
	{
		RUYA_LOG_ERROR("[Model] Failed to get tangent data");
		return;
	}

	size_t vertexCount = GetAccessorCount(model, it->second);

	for (size_t i = 0; i < std::min(vertices.size(), vertexCount); ++i)
	{
		glm::vec3 tangent = glm::vec3(tangents[i * 4], tangents[i * 4 + 1], tangents[i * 4 + 2]);
		float w = tangents[i * 4 + 3];

		vertices[i].tangent = tangent;
		vertices[i].bitangent = glm::cross(vertices[i].normal, tangent) * w;
	}
}

void ruya::Model3D::CalculateTangents(std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices)
{
	for (auto& vertex : vertices)
	{
		vertex.tangent = glm::vec3(0.0f);
		vertex.bitangent = glm::vec3(0.0f);
	}

	for (size_t i = 0; i < indices.size(); i += 3)
	{
		if (i + 2 >= indices.size()) break;

		uint32_t i0 = indices[i];
		uint32_t i1 = indices[i + 1];
		uint32_t i2 = indices[i + 2];

		if (i0 >= vertices.size() || i1 >= vertices.size() || i2 >= vertices.size())
			continue;

		Vertex& v0 = vertices[i0];
		Vertex& v1 = vertices[i1];
		Vertex& v2 = vertices[i2];

		glm::vec3 edge1 = v1.position - v0.position;
		glm::vec3 edge2 = v2.position - v0.position;

		glm::vec2 deltaUV1 = v1.texCoord - v0.texCoord;
		glm::vec2 deltaUV2 = v2.texCoord - v0.texCoord;

		float denominator = (deltaUV1.x * deltaUV2.y - deltaUV2.x * deltaUV1.y);

		if (std::abs(denominator) < 1e-6f)
			continue;

		float f = 1.0f / denominator;

		glm::vec3 tangent = f * (deltaUV2.y * edge1 - deltaUV1.y * edge2);
		glm::vec3 bitangent = f * (deltaUV1.x * edge2 - deltaUV2.x * edge1);

		v0.tangent += tangent;
		v1.tangent += tangent;
		v2.tangent += tangent;

		v0.bitangent += bitangent;
		v1.bitangent += bitangent;
		v2.bitangent += bitangent;
	}

	for (auto& vertex : vertices)
	{
		if (glm::length(vertex.tangent) > 1e-6f && glm::length(vertex.normal) > 1e-6f)
		{
			vertex.tangent = glm::normalize(vertex.tangent - vertex.normal * glm::dot(vertex.normal, vertex.tangent));

			glm::vec3 calculatedBitangent = glm::cross(vertex.normal, vertex.tangent);

			if (glm::length(calculatedBitangent) > 1e-6f)
			{
				vertex.bitangent = glm::normalize(calculatedBitangent);
			}
			else
			{
				vertex.bitangent = glm::vec3(0.0f, 0.0f, 1.0f);
			}
		}
		else
		{
			vertex.tangent = glm::vec3(1.0f, 0.0f, 0.0f);
			vertex.bitangent = glm::vec3(0.0f, 0.0f, 1.0f);
		}
	}
}

void ruya::Model3D::ProcessPositions(const tinygltf::Model& model, const tinygltf::Primitive& primitive, std::vector<Vertex>& vertices)
{
	auto it = primitive.attributes.find("POSITION");

	if (it == primitive.attributes.end())
	{
		RUYA_LOG_ERROR("[Model] No POSITION attribute found");
		return;
	}

	const float* positions = GetAccessorData<float>(model, it->second);
	if (!positions)
	{
		RUYA_LOG_ERROR("[Model] Failed to get position data");
		return;
	}

	size_t vertexCount = GetAccessorCount(model, it->second);

	vertices.resize(vertexCount);

	for (size_t i = 0; i < vertexCount; ++i)
	{
		vertices[i].position = glm::vec3(positions[i * 3], positions[i * 3 + 1], positions[i * 3 + 2]);
	}
}

void ruya::Model3D::ProcessNormals(const tinygltf::Model& model, const tinygltf::Primitive& primitive, std::vector<Vertex>& vertices)
{
	auto it = primitive.attributes.find("NORMAL");

	if (it == primitive.attributes.end())
	{
		for (auto& vertex : vertices)
		{
			vertex.normal = glm::vec3(0.0f, 1.0f, 0.0f);
		}
		return;
	}

	const float* normals = GetAccessorData<float>(model, it->second);
	if (!normals)
	{
		RUYA_LOG_ERROR("[Model] Failed to get normal data");
		return;
	}

	size_t vertexCount = GetAccessorCount(model, it->second);

	for (size_t i = 0; i < std::min(vertices.size(), vertexCount); ++i)
	{
		vertices[i].normal = glm::vec3(normals[i * 3], normals[i * 3 + 1], normals[i * 3 + 2]);
	}
}

void ruya::Model3D::ProcessTexCoords(const tinygltf::Model& model, const tinygltf::Primitive& primitive, std::vector<Vertex>& vertices)
{
	auto it = primitive.attributes.find("TEXCOORD_0");

	if (it == primitive.attributes.end())
	{
		for (auto& vertex : vertices)
		{
			vertex.texCoord = glm::vec2(0.0f, 0.0f);
		}
		return;
	}

	const float* texCoords = GetAccessorData<float>(model, it->second);
	if (!texCoords)
	{
		RUYA_LOG_ERROR("[Model] Failed to get texcoord data");
		return;
	}

	size_t vertexCount = GetAccessorCount(model, it->second);

	for (size_t i = 0; i < std::min(vertices.size(), vertexCount); ++i)
	{
		vertices[i].texCoord = glm::vec2(texCoords[i * 2], texCoords[i * 2 + 1]);
	}
}

void ruya::Model3D::ProcessIndices(const tinygltf::Model& model, const tinygltf::Primitive& primitive, std::vector<uint32_t>& indices)
{
	if (primitive.indices < 0)
		return;

	if (primitive.indices >= static_cast<int>(model.accessors.size()))
	{
		RUYA_LOG_ERROR("[Model] Invalid indices accessor index");
		return;
	}

	const tinygltf::Accessor& accessor = model.accessors[primitive.indices];

	if (accessor.bufferView < 0 || accessor.bufferView >= static_cast<int>(model.bufferViews.size()))
	{
		RUYA_LOG_ERROR("[Model] Invalid buffer view index");
		return;
	}

	const tinygltf::BufferView& bufferView = model.bufferViews[accessor.bufferView];

	if (bufferView.buffer < 0 || bufferView.buffer >= static_cast<int>(model.buffers.size()))
	{
		RUYA_LOG_ERROR("[Model] Invalid buffer index");
		return;
	}

	const tinygltf::Buffer& buffer = model.buffers[bufferView.buffer];

	const unsigned char* data = buffer.data.data() + bufferView.byteOffset + accessor.byteOffset;

	if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT)
	{
		const uint16_t* indices16 = reinterpret_cast<const uint16_t*>(data);
		indices.reserve(accessor.count);
		for (size_t i = 0; i < accessor.count; ++i)
		{
			indices.push_back(static_cast<uint32_t>(indices16[i]));
		}
	}
	else if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT)
	{
		const uint32_t* indices32 = reinterpret_cast<const uint32_t*>(data);
		indices.reserve(accessor.count);
		for (size_t i = 0; i < accessor.count; ++i)
		{
			indices.push_back(indices32[i]);
		}
	}
	else
	{
		RUYA_LOG_ERROR("[Model] Unsupported index component type: {}", accessor.componentType);
	}
}

void ruya::Model3D::ProcessMaterial(const tinygltf::Model& model, int materialIndex, Model3DNode& modelNode)
{
	if (materialIndex < 0 || materialIndex >= static_cast<int>(model.materials.size()))
		return;

	const tinygltf::Material& material = model.materials[materialIndex];

	RyID albedoImageId = engine->GetGraphics()->CreateImage2D(ASSETS_DIR + "ruya_files/images/default_albedo.png", ImageType::SRGB, ImageSampler::Linear, "default_albedo");
	RyID normalImageId = engine->GetGraphics()->CreateImage2D(ASSETS_DIR + "ruya_files/images/default_normal.png", ImageType::NonColor, ImageSampler::Linear, "default_normal");
	RyID metallicRoughnessImageId = engine->GetGraphics()->CreateImage2D(ASSETS_DIR + "ruya_files/images/default_metallicRoughness.png", ImageType::NonColor, ImageSampler::Linear, "default_metallicRoughness");

	if (material.pbrMetallicRoughness.baseColorTexture.index >= 0)
	{
		int textureIndex = material.pbrMetallicRoughness.baseColorTexture.index;
		std::string name = GetTextureName(model, textureIndex, "albedo", materialIndex);

		RyID loadedAlbedo = LoadTexture(model, textureIndex, ImageType::SRGB, name);
		if (loadedAlbedo.IsValid())
		{
			albedoImageId = loadedAlbedo;
		}
	}

	if (material.normalTexture.index >= 0)
	{
		int textureIndex = material.normalTexture.index;
		std::string name = GetTextureName(model, textureIndex, "normal", materialIndex);

		RyID loadedNormal = LoadTexture(model, textureIndex, ImageType::NonColor, name);
		if (loadedNormal.IsValid())
		{
			normalImageId = loadedNormal;
		}
	}

	if (material.pbrMetallicRoughness.metallicRoughnessTexture.index >= 0)
	{
		int textureIndex = material.pbrMetallicRoughness.metallicRoughnessTexture.index;
		std::string name = GetTextureName(model, textureIndex, "metallicRoughness", materialIndex);

		RyID loadedMR = LoadTexture(model, textureIndex, ImageType::NonColor, name);
		if (loadedMR.IsValid())
		{
			metallicRoughnessImageId = loadedMR;
		}
	}

	engine->GetGraphics()->UpdatePBROpaqueMaterial(modelNode.materialId, albedoImageId, normalImageId, metallicRoughnessImageId);
}

std::string ruya::Model3D::GetTextureName(const tinygltf::Model& model, int textureIndex, const std::string& textureType, int materialIndex)
{
	if (textureIndex < 0 || textureIndex >= static_cast<int>(model.textures.size()))
		return textureType + "_" + std::to_string(materialIndex);

	std::string name;

	if (!model.textures[textureIndex].name.empty())
	{
		name = model.textures[textureIndex].name;
	}
	else if (model.textures[textureIndex].source >= 0 &&
		model.textures[textureIndex].source < static_cast<int>(model.images.size()) &&
		!model.images[model.textures[textureIndex].source].name.empty())
	{
		name = model.images[model.textures[textureIndex].source].name;
	}
	else if (model.textures[textureIndex].source >= 0 &&
		model.textures[textureIndex].source < static_cast<int>(model.images.size()) &&
		!model.images[model.textures[textureIndex].source].uri.empty())
	{
		std::string uri = model.images[model.textures[textureIndex].source].uri;
		size_t lastSlash = uri.find_last_of("/\\");
		if (lastSlash != std::string::npos)
			name = uri.substr(lastSlash + 1);
		else
			name = uri;
		size_t lastDot = name.find_last_of(".");
		if (lastDot != std::string::npos)
			name = name.substr(0, lastDot);
	}
	else
	{
		name = textureType + "_" + std::to_string(materialIndex);
	}

	return name;
}

ruya::RyID ruya::Model3D::LoadTexture(const tinygltf::Model& model, int textureIndex, ImageType imageType, const std::string& name)
{
	if (textureIndex < 0 || textureIndex >= static_cast<int>(model.textures.size()))
	{
		RUYA_LOG_ERROR("[Model] Invalid texture index: {}", textureIndex);
		return RyID();
	}

	const tinygltf::Texture& texture = model.textures[textureIndex];

	if (texture.source < 0 || texture.source >= static_cast<int>(model.images.size()))
	{
		RUYA_LOG_ERROR("[Model] Invalid image source index: {}", texture.source);
		return RyID();
	}

	const tinygltf::Image& image = model.images[texture.source];

	if (!image.uri.empty())
	{
		std::string imagePath = ASSETS_DIR + engine->GetAssetManager()->GetAsset(modelAssetID)->path + image.uri;

		if (!std::filesystem::exists(imagePath))
		{
			RUYA_LOG_ERROR("[Model] Texture file does not exist: {}", imagePath.c_str());
			return RyID();
		}

		return engine->GetGraphics()->CreateImage2D(imagePath, imageType, ImageSampler::Linear, name);
	}
	else
	{
		RUYA_LOG_ERROR("[Model] Embedded images not yet supported for texture: {}", name.c_str());
		return RyID();
	}
}

template<typename T>
const T* ruya::Model3D::GetAccessorData(const tinygltf::Model& model, int accessorIndex)
{
	if (accessorIndex < 0 || accessorIndex >= static_cast<int>(model.accessors.size()))
		return nullptr;

	const tinygltf::Accessor& accessor = model.accessors[accessorIndex];

	if (accessor.bufferView < 0 || accessor.bufferView >= static_cast<int>(model.bufferViews.size()))
		return nullptr;

	const tinygltf::BufferView& bufferView = model.bufferViews[accessor.bufferView];

	if (bufferView.buffer < 0 || bufferView.buffer >= static_cast<int>(model.buffers.size()))
		return nullptr;

	const tinygltf::Buffer& buffer = model.buffers[bufferView.buffer];

	return reinterpret_cast<const T*>(buffer.data.data() + bufferView.byteOffset + accessor.byteOffset);
}

size_t ruya::Model3D::GetAccessorCount(const tinygltf::Model& model, int accessorIndex)
{
	if (accessorIndex < 0 || accessorIndex >= static_cast<int>(model.accessors.size()))
		return 0;

	return model.accessors[accessorIndex].count;
}

ruya::EntityID ruya::Model3DNode::CreateEntity(Scene& scene, ruya::EntityID parent)
{
	ruya::EntityID entity = scene.CreateEntity();

	if (parent != entt::null)
		scene.TryGetComponent<RelationshipComponent>(entity)->parentEntity = parent;

	scene.AddComponent<Model3DComponent>(entity);
	scene.TryGetComponent<Model3DComponent>(entity)->modelAssetID = parentModelAssetID;
	scene.TryGetComponent<Model3DComponent>(entity)->isLoaded = true;

	engine->GetGraphics()->LoadModel3D(parentModelAssetID);

	glm::vec3 scale;
	glm::quat rotation;
	glm::vec3 translation;
	glm::vec3 skew;
	glm::vec4 perspective;
	glm::decompose(transform, scale, rotation, translation, skew, perspective);

	TransformComponent* transformComp = scene.TryGetComponent<TransformComponent>(entity);
	transformComp->position = translation;
	transformComp->rotation = glm::eulerAngles(rotation);
	transformComp->scale = scale;

	if (hasMesh)
	{
		scene.AddComponent<RenderComponent>(entity);
		RenderComponent* renderComp = scene.TryGetComponent<RenderComponent>(entity);
		renderComp->meshID = meshId;
		renderComp->materialID = materialId;
	}

	EntityID previousChild = entt::null;
	for (std::unique_ptr<Model3DNode>& childNode : children)
	{
		EntityID childEntity = childNode->CreateEntity(scene, entity);
		RelationshipComponent* parentRelationship = scene.TryGetComponent<RelationshipComponent>(entity);
		RelationshipComponent* childRelationship = scene.TryGetComponent<RelationshipComponent>(childEntity);

		if (previousChild == entt::null)
		{
			parentRelationship->firstChildEntity = childEntity;
		}
		else
		{
			scene.TryGetComponent<RelationshipComponent>(previousChild)->nextEntity = childEntity;
		}

		previousChild = childEntity;
	}

	return entity;
}

ruya::EntityID ruya::Model3DNode::CreateEntity(Scene& scene)
{
	return CreateEntity(scene, entt::null);
}