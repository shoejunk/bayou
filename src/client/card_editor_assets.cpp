#include "card_editor_assets.hpp"

#include "client_string.hpp"

#include <system_error>

namespace bayou::client::card_editor_assets
{
namespace
{
std::filesystem::path AssetRoot = "assets";

bool escapesAssetsRoot(const std::filesystem::path& path)
{
    for (const std::filesystem::path& component : path)
    {
        if (component.string() == "..")
        {
            return true;
        }
    }
    return false;
}

std::filesystem::path normalizedAbsolutePath(const std::filesystem::path& path)
{
    std::error_code error;
    std::filesystem::path normalized = std::filesystem::weakly_canonical(path, error);
    if (!error)
    {
        return normalized.lexically_normal();
    }

    error.clear();
    normalized = std::filesystem::absolute(path, error);
    if (!error)
    {
        return normalized.lexically_normal();
    }
    return path.lexically_normal();
}

bool isInsideAssetsRoot(const std::filesystem::path& path)
{
    const std::filesystem::path assetsRoot = normalizedAbsolutePath(AssetRoot);
    const std::filesystem::path normalizedPath = normalizedAbsolutePath(path);
    const std::filesystem::path relativePath = normalizedPath.lexically_relative(assetsRoot);
    if (relativePath.empty())
    {
        return normalizedPath == assetsRoot;
    }
    if (relativePath.has_root_path())
    {
        return false;
    }
    const auto firstComponent = relativePath.begin();
    return firstComponent == relativePath.end() || firstComponent->string() != "..";
}

} // namespace

void setAssetRoot(std::filesystem::path assetRoot)
{
    AssetRoot = normalizedAbsolutePath(std::move(assetRoot));
}

std::string assetRelativeImagePath(const std::string& value)
{
    const std::string trimmed = bayou::client::trim(value);
    if (trimmed.empty())
    {
        return "";
    }

    std::filesystem::path path(trimmed);
    if (path.is_absolute() || path.has_root_name() || path.has_root_directory())
    {
        return path.generic_string();
    }

    std::filesystem::path normalizedPath;
    bool checkedFirstComponent = false;
    for (const std::filesystem::path& component : path)
    {
        if (!checkedFirstComponent)
        {
            checkedFirstComponent = true;
            if (bayou::client::lowerKey(component.string()) == "assets")
            {
                continue;
            }
        }
        normalizedPath /= component;
    }

    return normalizedPath.lexically_normal().generic_string();
}

std::optional<std::filesystem::path> resolveAssetImagePath(const std::string& value)
{
    const std::string relativeValue = assetRelativeImagePath(value);
    if (relativeValue.empty())
    {
        return std::nullopt;
    }

    const std::filesystem::path relativePath(relativeValue);
    if (relativePath.is_absolute() || relativePath.has_root_name() || relativePath.has_root_directory() ||
        escapesAssetsRoot(relativePath))
    {
        return std::nullopt;
    }

    const std::filesystem::path resolvedPath = (AssetRoot / relativePath).lexically_normal();
    if (!isInsideAssetsRoot(resolvedPath))
    {
        return std::nullopt;
    }
    return resolvedPath;
}

void normalizeCardImagePath(card_data::Card& card)
{
    card.imagePath = assetRelativeImagePath(card.imagePath);
}

} // namespace bayou::client::card_editor_assets
