#pragma once

// public project headers -------------------------------
#include "mbase/public/access.h"

#include "mslang/public/slang_cache.h"

namespace mshell {

class AssetSlangCodeProvider : public mslang::ISlangCodeProvider {
public:
  AssetSlangCodeProvider() = default;
  ~AssetSlangCodeProvider() override = default;
  MBASE_DISALLOW_COPY_MOVE(AssetSlangCodeProvider);

  bool ProvideSlangCode(
    std::string_view parent_path,
    std::string_view module_name,
    std::string* out_slang_code,
    uint64_t& out_slang_code_timestamp
  ) override;

  bool ProvideSlangCodeTimestampResolvedPath(
    std::string_view resolved_path,
    uint64_t& out_slang_code_timestamp
  ) override;
};

class AssetSlangIncludeHandler : public mslang::ISlangDependencyIncludeHandler {
public:
  AssetSlangIncludeHandler() = default;
  ~AssetSlangIncludeHandler() override = default;
  MBASE_DISALLOW_COPY_MOVE(AssetSlangIncludeHandler);

  bool HandleInclude(
    char const* path,
    mslang::SlangIncludeResult& out_result
  ) override;
};

} // namespace wentos2
