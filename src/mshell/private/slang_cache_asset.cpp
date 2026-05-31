// TU header --------------------------------------------
#include "slang_cache_asset.h"

// c++ headers ------------------------------------------
#include <filesystem>
#include <functional>

// public project headers -------------------------------
#include "mbase/public/log.h"

#include "masset/public/masset.h"

namespace mshell {

// ----------------------------------------------------------------------------------------------------
// AssetSlangCodeProvider
//

bool AssetSlangCodeProvider::ProvideSlangCode(
  std::string_view parent_path,
  std::string_view module_name,
  std::string* out_slang_code,
  uint64_t& out_slang_code_timestamp
) {
  masset::IAssetManager* asset_manager = masset::IAssetManager::Get();

  // Generate a full path to the `.slang` file.
  std::filesystem::path slang_full_path = std::filesystem::path(parent_path) / std::filesystem::path(module_name);
  slang_full_path.replace_extension(".slang");

  if (out_slang_code != nullptr) {
    std::vector<std::byte> slang_bytes;
    uint64_t slang_last_modified_time = 0;
    if (!asset_manager->LoadAssetEx(slang_full_path.string().c_str(), slang_bytes, slang_last_modified_time)) {
      MBASE_LOG_ERROR("AssetSlangCodeProvider::ProvideSlangCode: Failed to load asset: \"{}\"", slang_full_path.string());
      return false;
    }

    *out_slang_code = std::string(reinterpret_cast<char const*>(slang_bytes.data()), slang_bytes.size());
    out_slang_code_timestamp = slang_last_modified_time;
    return true;
  }
  else {
    uint64_t slang_last_modified_time = 0;
    if (!asset_manager->GetAssetLastModifiedTime(slang_full_path.string().c_str(), slang_last_modified_time)) {
      MBASE_LOG_ERROR("AssetSlangCodeProvider::ProvideSlangCode: Failed to get last modified time of asset: \"{}\"", slang_full_path.string());
      return false;
    }

    out_slang_code_timestamp = slang_last_modified_time;
    return true;
  }
}

bool AssetSlangCodeProvider::ProvideSlangCodeTimestampResolvedPath(
  std::string_view resolved_path,
  uint64_t& out_slang_code_timestamp
) {
  masset::IAssetManager* asset_manager = masset::IAssetManager::Get();

  std::string resolved_path_str(resolved_path);

  if (!asset_manager->GetAssetLastModifiedTime(resolved_path_str.c_str(), out_slang_code_timestamp)) {
    MBASE_LOG_ERROR("AssetSlangCodeProvider::ProvideSlangCodeTimestampResolvedPath: Failed to get last modified time of asset: \"{}\"", resolved_path);
    return false;
  }

  return true;
}

// ----------------------------------------------------------------------------------------------------
// AssetSlangIncludeHandler
//

namespace {

bool TryResolveIncludePath(std::string const& src_path, std::function<bool(std::string const& path)> f) {
  // Current directory first.
  if (f(src_path)) {
    return true;
  }

  // Then try "mnexus/" directory.
  std::string resolved_path = "mnexus/" + src_path;
  if (f(resolved_path)) {
    return true;
  }

  return false;
}

} // namespace

bool AssetSlangIncludeHandler::HandleInclude(
  char const* path,
  mslang::SlangIncludeResult& out_result
) {
  masset::IAssetManager* asset_manager = masset::IAssetManager::Get();

  std::vector<std::byte> bytes;
  uint64_t last_modified_time = 0;

  std::string resolved_path;
  bool result = TryResolveIncludePath(
    path,
    [&](std::string const& p) {
      bool ok = asset_manager->LoadAssetEx(p.c_str(), bytes, last_modified_time);
      if (ok) {
        resolved_path = p;
      }
      return ok;
    }
  );

  if (!result) {
    MBASE_LOG_WARN("AssetSlangIncludeHandler: Failed to find asset with requested path: \"{}\"", path);
    return false;
  }

  out_result = mslang::SlangIncludeResult {
    .bytes = bytes,
    .resolved_path = resolved_path,
    .timestamp = last_modified_time,
  };
  return true;
}

} // namespace mshell
