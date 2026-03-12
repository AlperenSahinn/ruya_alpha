#pragma once
#include <memory>
#include <filesystem>
#include <string>
#include <cstdint>

#include <volk/volk.h>

#include <graphics/image_2d.h>
#include <assets_system/ry_asset.h>

#include "widget.h"

#include <im_file_browser/im_file_browser.h>

namespace editor
{
	struct RyAssetView
	{
		std::string name;
		std::string extension;
		ruya::RyAssetType type;
		ruya::RyID assetID;
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
		void RefreshAssets();
		void ProcessDoubleClick(const RyAssetView& assetView);
		void ImportGLTF(std::string path);
		void SaveScene();

	private:
		std::unique_ptr<ImGui::FileBrowser> pFileDialog;
		std::vector<RyAssetView> assetView;
		std::filesystem::path currentPath;

		bool openCreateScenePopup;
		std::string sceneNameBuffer;
		std::string createdSceneName;

		std::string newFolderName;
		bool openCreateFolderPopup;

		std::filesystem::path folderToDelete;
		bool openDeleteFolderPopup;

		std::filesystem::path folderToRename;
		std::string renameBuffer;
		bool openRenamePopup;
	};
}