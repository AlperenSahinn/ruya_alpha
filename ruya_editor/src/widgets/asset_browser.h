#pragma once
#include <memory>
#include <filesystem>
#include <string>
#include <cstdint>
#include <unordered_set>

#include <assets_system/ry_asset.h>

#include "widget.h"

namespace editor
{
	enum class RyAssetViewType
	{
		Folder,
		GLTF,
		Texture2D,
		Material,
		Scene
	};

	enum class RyTextureFormatView
	{
		Unknown,
		SRGB,
		Linear,
		NormalMap
	};

	struct RyAssetView
	{
		std::string name;
		RyAssetViewType type;
		ruya::UUID assetUUID;
		RyTextureFormatView textureFormat = RyTextureFormatView::Unknown;
	};

	class AssetBrowser : public Widget
	{
	public:
		AssetBrowser();
		~AssetBrowser() override;

		AssetBrowser(const AssetBrowser&) = delete;
		AssetBrowser& operator=(const AssetBrowser&) = delete;

		void Draw() override;

	private:
		void DrawFolderTree(const std::filesystem::path& path, int depth);
		void DrawAssetGrid();
		void RefreshAssets();
		void ProcessDoubleClick(const RyAssetView& assetView);

		ImVec4 GetAssetTypeColor(RyAssetViewType type) const;
		const char* GetAssetTypeIcon(RyAssetViewType type) const;
		const char* GetAssetTypeName(RyAssetViewType type) const;
		const char* GetTextureFormatName(RyTextureFormatView format) const;
		RyTextureFormatView DetectTextureFormat(ruya::UUID assetUUID) const;

		std::string GetRelativeDirectory() const;

		bool AssetNameExistsInCurrentDir(const std::string& stemName) const;

	private:
		std::vector<RyAssetView> assetView;
		std::filesystem::path currentPath;

		float thumbnailSize = 54.0f;
		char assetSearchFilter[128] = {};
		int selectedAssetIndex = -1;
		std::unordered_set<int> selectedAssetIndices;
		int lastClickedAssetIndex = -1;

		int pressedAssetIndex = -1;

		bool openCreateScenePopup = false;
		std::string sceneNameBuffer;
		std::string createdSceneName;

		std::string newFolderName;
		bool openCreateFolderPopup = false;

		std::filesystem::path folderToDelete;
		bool openDeleteFolderPopup = false;

		std::filesystem::path folderToRename;
		std::string renameBuffer;
		bool openRenamePopup = false;

		bool openDeleteAssetPopup = false;
		ruya::UUID assetToDeleteUUID = ruya::UUID(0);
		std::string assetToDeleteName;

		bool openDuplicateWarningPopup = false;
		std::string duplicateWarningMessage;
	};
}