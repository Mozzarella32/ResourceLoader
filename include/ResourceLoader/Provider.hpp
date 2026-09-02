#pragma once

#include "ResourceLoader/Paths.hpp"
#include "ResourceLoader/private/ResourcePtr.hpp"

#include <stb_image.h>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <mutex>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <tuple>
#include <unordered_map>
#include <utility>

namespace ResourceLoader {
class Provider {
  private:
    Paths resourcePaths;

  public:
    Provider(Paths resourcePaths) : resourcePaths(std::move(resourcePaths)) {}

    Provider(const Provider &) noexcept = delete;
    Provider(Provider &&) noexcept = delete;
    auto operator=(const Provider &) noexcept = delete;
    auto operator=(Provider &&) noexcept = delete;

    ~Provider();

    // key is in format [name]_[type]
    auto getShaderSpirV(std::string_view key) -> std::span<const uint32_t>;

    auto spirVGetter() -> std::function<std::span<const uint32_t>(std::string_view)>;

    auto getTextureData(std::string_view key)
        -> std::tuple<std::pair<uint32_t, uint32_t>, std::span<const std::byte>>;

    auto textureGetter() -> std::function<
        std::tuple<std::pair<uint32_t, uint32_t>, std::span<const std::byte>>(std::string_view)>;

  private:
    std::unordered_map<std::string, ResourcePtr> data;

    std::mutex resouceMutex;
    std::thread worker;

    bool terminate : 1 = false;
    bool running : 1 = false;

    bool shadersNeedUpdating = false;
    bool texturesNeedUpdating = false;
    bool meshesNeedUpdate = false;

    std::chrono::milliseconds refreshTime = defaultRefreshTime;

    void updateResources();
    void work();

  public:
    static const constexpr auto defaultRefreshTime = std::chrono::milliseconds(16);

    void startUpdater(std::chrono::milliseconds refreshTime = defaultRefreshTime);
    void stopUpdater();

    auto shadersNeedUpdatingClear() -> bool;
    auto texturesNeedUpdatingClear() -> bool;
    auto meshesNeedUpdateClear() -> bool;
};
} // namespace ResourceLoader
