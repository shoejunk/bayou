#include "client_ui_capture.hpp"
#include "client_build_identity.hpp"
#include "client_story.hpp"

#include "../shared/game_data.hpp"

#include "client_config.hpp"

#include <mbedtls/sha256.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <set>
#include <sstream>
#include <string>
#include <string_view>

namespace bayou::client::ui_capture
{
namespace
{

bool startsWith(std::string_view value, std::string_view prefix)
{
    return value.size() >= prefix.size() && value.compare(0, prefix.size(), prefix) == 0;
}

std::string genericUtf8(const std::filesystem::path& path)
{
    const std::u8string value = path.generic_u8string();
    return std::string(
        reinterpret_cast<const char*>(value.data()), value.size());
}

std::vector<std::string> splitList(std::string_view value)
{
    std::vector<std::string> items;
    std::string current;
    for (const char ch : value)
    {
        if (ch == ',' || ch == ';')
        {
            if (!current.empty())
            {
                items.push_back(current);
                current.clear();
            }
            continue;
        }
        current.push_back(ch);
    }
    if (!current.empty())
    {
        items.push_back(current);
    }
    return items;
}

std::string jsonQuoted(std::string_view value)
{
    std::ostringstream escaped;
    escaped << '"';
    for (const unsigned char ch : value)
    {
        switch (ch)
        {
        case '"': escaped << "\\\""; break;
        case '\\': escaped << "\\\\"; break;
        case '\b': escaped << "\\b"; break;
        case '\f': escaped << "\\f"; break;
        case '\n': escaped << "\\n"; break;
        case '\r': escaped << "\\r"; break;
        case '\t': escaped << "\\t"; break;
        default:
            if (ch < 0x20)
            {
                escaped << "\\u" << std::hex << std::setw(4) << std::setfill('0')
                        << static_cast<int>(ch) << std::dec;
            }
            else
            {
                escaped << static_cast<char>(ch);
            }
            break;
        }
    }
    escaped << '"';
    return escaped.str();
}

std::string readFirstLine(const std::filesystem::path& path)
{
    std::ifstream input(path);
    std::string line;
    if (input && std::getline(input, line) && !line.empty() && line.back() == '\r')
    {
        line.pop_back();
    }
    return line;
}

std::optional<std::filesystem::path> repositoryRootFrom(std::filesystem::path candidate)
{
    std::error_code error;
    candidate = std::filesystem::absolute(candidate, error);
    if (error)
    {
        return std::nullopt;
    }
    if (!std::filesystem::is_directory(candidate, error))
    {
        candidate = candidate.parent_path();
    }

    while (!candidate.empty())
    {
        error.clear();
        if (std::filesystem::exists(candidate / ".git", error) && !error)
        {
            return candidate;
        }
        const std::filesystem::path parent = candidate.parent_path();
        if (parent == candidate)
        {
            break;
        }
        candidate = parent;
    }
    return std::nullopt;
}

std::optional<bool> inspectWorkingTreeDirty(const std::filesystem::path& root)
{
#ifdef _WIN32
    const std::string command = "git -C \"" + root.string() +
        "\" status --porcelain=v1 --untracked-files=all --ignore-submodules=none 2>NUL";
    FILE* pipe = _popen(command.c_str(), "r");
#else
    std::string quotedRoot = root.string();
    std::size_t quote = 0;
    while ((quote = quotedRoot.find('\'', quote)) != std::string::npos)
    {
        quotedRoot.replace(quote, 1, "'\\''");
        quote += 4;
    }
    const std::string command = "git -C '" + quotedRoot +
        "' status --porcelain=v1 --untracked-files=all --ignore-submodules=none 2>/dev/null";
    FILE* pipe = popen(command.c_str(), "r");
#endif
    if (!pipe)
    {
        return std::nullopt;
    }

    bool dirty = false;
    char buffer[4096];
    while (std::fgets(buffer, sizeof(buffer), pipe))
    {
        dirty = true;
    }
#ifdef _WIN32
    const int result = _pclose(pipe);
#else
    const int result = pclose(pipe);
#endif
    if (result != 0)
    {
        return std::nullopt;
    }
    return dirty;
}

CheckoutIdentity inspectGitIdentity(const std::filesystem::path& executablePath)
{
    std::error_code error;
    std::optional<std::filesystem::path> root =
        repositoryRootFrom(std::filesystem::current_path(error));
    if (!root)
    {
        root = repositoryRootFrom(executablePath);
    }
    if (!root)
    {
        return {};
    }

    CheckoutIdentity identity;
    identity.repositoryRoot = root->generic_string();
    std::filesystem::path gitDirectory = *root / ".git";
    if (!std::filesystem::is_directory(gitDirectory, error))
    {
        const std::string gitFile = readFirstLine(gitDirectory);
        constexpr std::string_view Prefix = "gitdir: ";
        if (!startsWith(gitFile, Prefix))
        {
            return identity;
        }
        gitDirectory = std::filesystem::path(gitFile.substr(Prefix.size()));
        if (gitDirectory.is_relative())
        {
            gitDirectory = *root / gitDirectory;
        }
    }

    std::filesystem::path commonDirectory = gitDirectory;
    const std::string commonDirectoryText = readFirstLine(gitDirectory / "commondir");
    if (!commonDirectoryText.empty())
    {
        commonDirectory = std::filesystem::path(commonDirectoryText);
        if (commonDirectory.is_relative())
        {
            commonDirectory = gitDirectory / commonDirectory;
        }
    }

    const std::string head = readFirstLine(gitDirectory / "HEAD");
    constexpr std::string_view RefPrefix = "ref: ";
    if (!startsWith(head, RefPrefix))
    {
        identity.commit = head;
    }
    else
    {
        identity.reference = head.substr(RefPrefix.size());
        identity.commit = readFirstLine(gitDirectory / identity.reference);
        if (identity.commit.empty())
        {
            identity.commit = readFirstLine(commonDirectory / identity.reference);
        }
        if (identity.commit.empty())
        {
            std::ifstream packedRefs(commonDirectory / "packed-refs");
            std::string line;
            while (std::getline(packedRefs, line))
            {
                if (line.empty() || line.front() == '#' || line.front() == '^')
                {
                    continue;
                }
                const std::size_t separator = line.find(' ');
                if (separator != std::string::npos &&
                    line.substr(separator + 1) == identity.reference)
                {
                    identity.commit = line.substr(0, separator);
                    break;
                }
            }
        }
    }
    if (const std::optional<bool> dirty = inspectWorkingTreeDirty(*root))
    {
        identity.dirty = *dirty;
        identity.dirtyKnown = true;
    }
    return identity;
}

std::string hexDigest(const std::array<unsigned char, 32>& digest)
{
    std::ostringstream formatted;
    formatted << std::hex << std::setfill('0');
    for (const unsigned char byte : digest)
    {
        formatted << std::setw(2) << static_cast<unsigned int>(byte);
    }
    return formatted.str();
}

class Sha256Stream
{
public:
    Sha256Stream()
    {
        mbedtls_sha256_init(&context);
        valid = mbedtls_sha256_starts(&context, 0) == 0;
    }

    Sha256Stream(const Sha256Stream&) = delete;
    Sha256Stream& operator=(const Sha256Stream&) = delete;

    ~Sha256Stream()
    {
        mbedtls_sha256_free(&context);
    }

    bool update(const void* bytes, std::size_t size)
    {
        if (!valid || (size != 0 && bytes == nullptr))
        {
            valid = false;
            return false;
        }
        if (size != 0 &&
            mbedtls_sha256_update(
                &context,
                static_cast<const unsigned char*>(bytes),
                size) != 0)
        {
            valid = false;
        }
        return valid;
    }

    bool update(std::string_view text)
    {
        return update(text.data(), text.size());
    }

    std::optional<std::string> finish()
    {
        if (!valid || finished)
        {
            return std::nullopt;
        }
        std::array<unsigned char, 32> digest{};
        if (mbedtls_sha256_finish(&context, digest.data()) != 0)
        {
            valid = false;
            return std::nullopt;
        }
        finished = true;
        return hexDigest(digest);
    }

private:
    mbedtls_sha256_context context{};
    bool valid = false;
    bool finished = false;
};

std::optional<std::string> sha256Text(std::string_view text)
{
    Sha256Stream hash;
    if (!hash.update(text))
    {
        return std::nullopt;
    }
    return hash.finish();
}

struct FileDigest
{
    std::uintmax_t size = 0;
    std::string sha256;
};

std::optional<FileDigest> sha256File(
    const std::filesystem::path& path,
    std::string& error)
{
    error.clear();
    std::error_code filesystemError;
    const std::filesystem::file_status status =
        std::filesystem::symlink_status(path, filesystemError);
    if (filesystemError || !std::filesystem::is_regular_file(status) ||
        std::filesystem::is_symlink(status))
    {
        error = "Snapshot input is missing, unreadable, or not a regular file: " +
            path.string();
        return std::nullopt;
    }

    const std::uintmax_t sizeBefore =
        std::filesystem::file_size(path, filesystemError);
    if (filesystemError)
    {
        error = "Could not read snapshot input size: " + path.string();
        return std::nullopt;
    }
    const std::filesystem::file_time_type writeTimeBefore =
        std::filesystem::last_write_time(path, filesystemError);
    if (filesystemError)
    {
        error = "Could not read snapshot input timestamp: " + path.string();
        return std::nullopt;
    }

    std::ifstream input(path, std::ios::binary);
    if (!input)
    {
        error = "Could not open snapshot input: " + path.string();
        return std::nullopt;
    }

    Sha256Stream hash;
    std::array<char, 64 * 1024> buffer{};
    while (input)
    {
        input.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        const std::streamsize count = input.gcount();
        if (count > 0 &&
            !hash.update(buffer.data(), static_cast<std::size_t>(count)))
        {
            error = "SHA-256 failed while reading snapshot input: " + path.string();
            return std::nullopt;
        }
    }
    if (input.bad())
    {
        error = "Could not finish reading snapshot input: " + path.string();
        return std::nullopt;
    }

    const std::uintmax_t sizeAfter =
        std::filesystem::file_size(path, filesystemError);
    if (filesystemError)
    {
        error = "Could not re-read snapshot input size: " + path.string();
        return std::nullopt;
    }
    const std::filesystem::file_time_type writeTimeAfter =
        std::filesystem::last_write_time(path, filesystemError);
    if (filesystemError || sizeBefore != sizeAfter || writeTimeBefore != writeTimeAfter)
    {
        error = "Snapshot input changed while it was being hashed: " + path.string();
        return std::nullopt;
    }

    const std::optional<std::string> digest = hash.finish();
    if (!digest)
    {
        error = "Could not finish SHA-256 for snapshot input: " + path.string();
        return std::nullopt;
    }
    return FileDigest{sizeAfter, *digest};
}

bool appendSnapshotFile(
    const std::filesystem::path& repositoryRoot,
    const std::filesystem::path& relativePath,
    std::set<std::string>& files,
    std::string& error)
{
    const std::filesystem::path absolutePath =
        (repositoryRoot / relativePath).lexically_normal();
    std::error_code filesystemError;
    const std::filesystem::file_status status =
        std::filesystem::symlink_status(absolutePath, filesystemError);
    if (filesystemError || !std::filesystem::is_regular_file(status) ||
        std::filesystem::is_symlink(status))
    {
        error = "Snapshot input is missing, unreadable, or not a regular file: " +
            absolutePath.string();
        return false;
    }
    files.insert(genericUtf8(relativePath.lexically_normal()));
    return true;
}

bool appendSnapshotTree(
    const std::filesystem::path& repositoryRoot,
    const std::filesystem::path& relativeRoot,
    std::set<std::string>& files,
    std::string& error)
{
    const std::filesystem::path absoluteRoot =
        (repositoryRoot / relativeRoot).lexically_normal();
    std::error_code filesystemError;
    const std::filesystem::file_status rootStatus =
        std::filesystem::symlink_status(absoluteRoot, filesystemError);
    if (filesystemError || !std::filesystem::is_directory(rootStatus) ||
        std::filesystem::is_symlink(rootStatus))
    {
        error = "Snapshot root is missing, unreadable, or not a directory: " +
            absoluteRoot.string();
        return false;
    }

    std::filesystem::recursive_directory_iterator iterator(absoluteRoot, filesystemError), end;
    while (!filesystemError && iterator != end)
    {
        const std::filesystem::file_status status =
            iterator->symlink_status(filesystemError);
        if (filesystemError)
        {
            break;
        }
        if (std::filesystem::is_symlink(status))
        {
            error = "Snapshot scope may not contain a symbolic link: " +
                iterator->path().string();
            return false;
        }
        if (std::filesystem::is_regular_file(status))
        {
            const std::filesystem::path relativePath =
                iterator->path().lexically_relative(repositoryRoot);
            const std::string relativeText = genericUtf8(relativePath);
            if (relativeText.empty() || relativeText == ".." ||
                startsWith(relativeText, "../"))
            {
                error = "Snapshot input escaped the repository root: " +
                    iterator->path().string();
                return false;
            }
            files.insert(genericUtf8(relativePath.lexically_normal()));
        }
        else if (!std::filesystem::is_directory(status))
        {
            error = "Snapshot scope contains an unsupported filesystem entry: " +
                iterator->path().string();
            return false;
        }
        iterator.increment(filesystemError);
    }
    if (filesystemError)
    {
        error = "Could not enumerate snapshot root '" + absoluteRoot.string() +
            "': " + filesystemError.message();
        return false;
    }
    return true;
}

InputSnapshotGroup inspectSnapshotGroup(
    const std::filesystem::path& repositoryRoot,
    std::string name,
    const std::set<std::string>& files,
    std::string& error)
{
    InputSnapshotGroup group;
    group.name = std::move(name);
    Sha256Stream groupHash;
    for (const std::string& relativePath : files)
    {
        const std::optional<FileDigest> file =
            sha256File(repositoryRoot / std::filesystem::path(relativePath), error);
        if (!file)
        {
            return group;
        }
        const std::string record =
            "P" + std::to_string(relativePath.size()) + ":" + relativePath +
            "S" + std::to_string(file->size) + ":H" + file->sha256 + "\n";
        if (!groupHash.update(record))
        {
            error = "Could not update the SHA-256 snapshot for " + group.name + ".";
            return group;
        }
        ++group.fileCount;
        group.totalBytes += file->size;
    }
    const std::optional<std::string> digest = groupHash.finish();
    if (!digest)
    {
        error = "Could not finish the SHA-256 snapshot for " + group.name + ".";
        return group;
    }
    group.sha256 = *digest;
    return group;
}

InputSnapshot inspectInputSnapshot(const std::filesystem::path& repositoryRoot)
{
    InputSnapshot snapshot;
    const std::optional<std::string> selfTest = sha256Text("abc");
    if (!selfTest ||
        *selfTest != "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad")
    {
        snapshot.error = "SHA-256 known-answer test failed.";
        return snapshot;
    }

    std::set<std::string> projectFiles;
    for (const std::filesystem::path& tree : {
             std::filesystem::path("cmake"),
             std::filesystem::path("src/client"),
             std::filesystem::path("src/shared")})
    {
        if (!appendSnapshotTree(repositoryRoot, tree, projectFiles, snapshot.error))
        {
            return snapshot;
        }
    }
    for (const std::filesystem::path& file : {
             std::filesystem::path("CMakeLists.txt"),
             std::filesystem::path("Directory.Build.props"),
             std::filesystem::path("client_debug.cfg"),
             std::filesystem::path("client_release.cfg"),
             std::filesystem::path("deploy/ca/isrg-root-x1.pem"),
             std::filesystem::path("src/gameserver/ai_player.cpp"),
             std::filesystem::path("src/gameserver/ai_player.hpp"),
             std::filesystem::path("src/gameserver/game_engine.hpp")})
    {
        if (!appendSnapshotFile(repositoryRoot, file, projectFiles, snapshot.error))
        {
            return snapshot;
        }
    }

    std::set<std::string> assetFiles;
    if (!appendSnapshotTree(
            repositoryRoot, std::filesystem::path("assets"), assetFiles, snapshot.error))
    {
        return snapshot;
    }

    snapshot.projectInputs = inspectSnapshotGroup(
        repositoryRoot, "projectInputs", projectFiles, snapshot.error);
    if (!snapshot.error.empty())
    {
        return snapshot;
    }
    snapshot.runtimeAssets = inspectSnapshotGroup(
        repositoryRoot, "runtimeAssets", assetFiles, snapshot.error);
    if (!snapshot.error.empty())
    {
        return snapshot;
    }

    const std::string overallCanonical =
        std::string(build_identity::SnapshotSchema) + "\n" +
        snapshot.projectInputs.name + ":" +
        std::to_string(snapshot.projectInputs.fileCount) + ":" +
        std::to_string(snapshot.projectInputs.totalBytes) + ":" +
        snapshot.projectInputs.sha256 + "\n" +
        snapshot.runtimeAssets.name + ":" +
        std::to_string(snapshot.runtimeAssets.fileCount) + ":" +
        std::to_string(snapshot.runtimeAssets.totalBytes) + ":" +
        snapshot.runtimeAssets.sha256 + "\n";
    const std::optional<std::string> overall = sha256Text(overallCanonical);
    if (!overall)
    {
        snapshot.error = "Could not finish the overall SHA-256 input snapshot.";
        return snapshot;
    }
    snapshot.overallSha256 = *overall;
    snapshot.fileCount =
        snapshot.projectInputs.fileCount + snapshot.runtimeAssets.fileCount;
    snapshot.totalBytes =
        snapshot.projectInputs.totalBytes + snapshot.runtimeAssets.totalBytes;
    snapshot.available = true;
    return snapshot;
}

InputSnapshot embeddedBuildSnapshot()
{
    InputSnapshot snapshot;
    snapshot.available = std::string_view(build_identity::SnapshotSha256).size() == 64;
    snapshot.overallSha256 = build_identity::SnapshotSha256;
    snapshot.fileCount = build_identity::SnapshotFileCount;
    snapshot.totalBytes = build_identity::SnapshotTotalBytes;
    snapshot.projectInputs = {
        build_identity::ProjectInputs.name,
        build_identity::ProjectInputs.sha256,
        build_identity::ProjectInputs.fileCount,
        build_identity::ProjectInputs.totalBytes};
    snapshot.runtimeAssets = {
        build_identity::RuntimeAssets.name,
        build_identity::RuntimeAssets.sha256,
        build_identity::RuntimeAssets.fileCount,
        build_identity::RuntimeAssets.totalBytes};
    if (!snapshot.available)
    {
        snapshot.error = "The executable does not contain a complete build input snapshot.";
    }
    return snapshot;
}

bool snapshotGroupEqual(
    const InputSnapshotGroup& left,
    const InputSnapshotGroup& right)
{
    return left.name == right.name &&
        left.sha256 == right.sha256 &&
        left.fileCount == right.fileCount &&
        left.totalBytes == right.totalBytes;
}

bool snapshotsEqual(const InputSnapshot& left, const InputSnapshot& right)
{
    return left.available && right.available &&
        left.overallSha256 == right.overallSha256 &&
        left.fileCount == right.fileCount &&
        left.totalBytes == right.totalBytes &&
        snapshotGroupEqual(left.projectInputs, right.projectInputs) &&
        snapshotGroupEqual(left.runtimeAssets, right.runtimeAssets);
}

std::vector<std::string> snapshotMismatchGroups(
    const InputSnapshot& expected,
    const InputSnapshot& actual)
{
    std::vector<std::string> mismatches;
    if (!snapshotGroupEqual(expected.projectInputs, actual.projectInputs))
    {
        mismatches.push_back("projectInputs");
    }
    if (!snapshotGroupEqual(expected.runtimeAssets, actual.runtimeAssets))
    {
        mismatches.push_back("runtimeAssets");
    }
    if (mismatches.empty() && !snapshotsEqual(expected, actual))
    {
        mismatches.push_back("overall");
    }
    return mismatches;
}

std::string joined(const std::vector<std::string>& values)
{
    std::ostringstream text;
    for (std::size_t index = 0; index < values.size(); ++index)
    {
        if (index != 0)
        {
            text << ", ";
        }
        text << values[index];
    }
    return text.str();
}

void writeSnapshotGroupJson(
    std::ostringstream& output,
    const InputSnapshotGroup& group,
    std::string_view indentation)
{
    output << indentation << "{\"sha256\": " << jsonQuoted(group.sha256)
           << ", \"fileCount\": " << group.fileCount
           << ", \"totalBytes\": " << group.totalBytes << "}";
}

void writeSnapshotJson(
    std::ostringstream& output,
    const InputSnapshot& snapshot,
    std::string_view indentation)
{
    output << indentation << "{\n"
           << indentation << "  \"available\": "
           << (snapshot.available ? "true" : "false") << ",\n"
           << indentation << "  \"overallSha256\": "
           << jsonQuoted(snapshot.overallSha256) << ",\n"
           << indentation << "  \"fileCount\": " << snapshot.fileCount << ",\n"
           << indentation << "  \"totalBytes\": " << snapshot.totalBytes << ",\n"
           << indentation << "  \"groups\": {\n"
           << indentation << "    \"projectInputs\": ";
    writeSnapshotGroupJson(output, snapshot.projectInputs, indentation);
    output << ",\n" << indentation << "    \"runtimeAssets\": ";
    writeSnapshotGroupJson(output, snapshot.runtimeAssets, indentation);
    output << "\n" << indentation << "  }\n"
           << indentation << "}";
}

struct ExecutableIdentity
{
    std::string path;
    std::uintmax_t size = 0;
    std::string fnv1a64;
    std::string sha256;
};

ExecutableIdentity inspectExecutable(const std::filesystem::path& executablePath)
{
    ExecutableIdentity identity;
    std::error_code error;
    const std::filesystem::path absolute =
        std::filesystem::absolute(executablePath, error);
    const std::filesystem::path resolved = error ? executablePath : absolute;
    identity.path = resolved.generic_string();
    error.clear();
    identity.size = std::filesystem::file_size(resolved, error);
    if (error)
    {
        identity.size = 0;
    }

    std::ifstream input(resolved, std::ios::binary);
    if (input)
    {
        std::uint64_t hash = 14695981039346656037ull;
        Sha256Stream sha256;
        char buffer[64 * 1024];
        while (input)
        {
            input.read(buffer, sizeof(buffer));
            const std::streamsize count = input.gcount();
            if (count > 0)
            {
                sha256.update(buffer, static_cast<std::size_t>(count));
            }
            for (std::streamsize i = 0; i < count; ++i)
            {
                hash ^= static_cast<unsigned char>(buffer[i]);
                hash *= 1099511628211ull;
            }
        }
        std::ostringstream formatted;
        formatted << std::hex << std::setw(16) << std::setfill('0') << hash;
        identity.fnv1a64 = formatted.str();
        if (!input.bad())
        {
            identity.sha256 = sha256.finish().value_or("");
        }
    }
    return identity;
}

std::string buildConfiguration()
{
#ifdef NDEBUG
    return "Release";
#else
    return "Debug";
#endif
}

std::string compilerIdentity()
{
#if defined(_MSC_FULL_VER)
    return "MSVC " + std::to_string(_MSC_FULL_VER);
#elif defined(__clang_version__)
    return "Clang " __clang_version__;
#elif defined(__GNUC__)
    return "GCC " + std::to_string(__GNUC__) + "." + std::to_string(__GNUC_MINOR__);
#else
    return "unknown";
#endif
}

struct SampleCard
{
    const char* title;
    const char* type;
    const char* art;
    int cost;     // resource cost; 0 for heroes
    int heroCost; // hero cost; 0 for everything else
    int health;
    int attack;
    int range;
    // The first trait must be one of game_data::CardTraitLabels, because that is
    // what the collection's trait filters match and what real catalogue cards
    // carry. The court follows it as flavour.
    const char* trait;
    // A second canonical trait, used by heroes so a coherent faction list is not
    // flagged for trait mismatch against its own units. Empty for everything else.
    const char* trait2;
    const char* court;
    const char* rarity;
    const char* keyword;
    const char* text;
};

// A cross section of the real catalogue: two heroes and a spread of units per
// court, costs from 1 to 7, and every rarity represented, so the collection
// screens have enough shape to design against: a curve that is not flat, rarity
// gems that differ, and enough owned cards per court to build a legal deck.
constexpr SampleCard SampleCards[] = {
    // --- heroes ------------------------------------------------------------
    {"Sylvara", "Hero", "cards/Sylvara.png", 0, 34, 22, 4, 1, "Honorable", "Fey", "Seelie", "legendary", "Regal",
     "While Sylvara holds a home square, friendly Seelie units gain +1 attack."},
    {"Nettle Starbright", "Hero", "cards/nettleStarbright.png", 0, 28, 20, 3, 2, "Fey", "Arcane", "Seelie", "rare", "Swift",
     "Deploy: draw a card. Nettle may act again after a kill."},
    {"Thaeron Baelstone", "Hero", "cards/thaeronBaelstone.png", 0, 36, 24, 5, 1, "Corrupt", "Undead", "Unseelie", "legendary", "Relentless",
     "Relentless. When Thaeron destroys a unit, he may move once more."},
    {"Prince Vesper", "Hero", "cards/princeVesper.png", 0, 32, 21, 4, 2, "Arcane", "Corrupt", "Unseelie", "legendary", "Hex",
     "Ability: an enemy unit cannot act on its owner's next turn."},
    {"Reed Baelstone", "Hero", "cards/reedBaelstone.png", 0, 26, 23, 3, 1, "Wild", "Honorable", "Mirewatch", "rare", "Dig In",
     "Reed takes 1 less damage while standing on marsh."},
    {"Maggie Mudroot", "Hero", "cards/maggieMudroot.png", 0, 30, 19, 3, 2, "Ancient", "Wild", "Mirewatch", "legendary", "Rootbound",
     "Ability: heal every friendly unit within range 2 for 2."},
    {"Braun Stonefist", "Hero", "cards/braunStonefist.png", 0, 30, 26, 5, 1, "Civilized", "Mechanical", "Blackthorn", "rare", "Bruiser",
     "Braun deals 2 extra damage to structures and mechanical units."},
    {"Victor Greyshard", "Hero", "cards/victorGreyshard.png", 0, 34, 20, 4, 1, "Mechanical", "Civilized", "Blackthorn", "legendary", "Levy",
     "Levy. Gain 2 coins whenever an enemy unit is destroyed."},

    // --- the Seelie Court --------------------------------------------------
    {"Quinberry Lark", "Unit", "cards/quinberryLark.png", 1, 0, 2, 1, 2, "Fey", "", "Seelie", "starter", "Swift",
     "Swift. Quinberry Lark may move after attacking."},
    {"Fey Messenger", "Unit", "cards/feyMessenger.png", 1, 0, 2, 1, 1, "Fey", "", "Seelie", "common", "Courier",
     "Deploy: look at the top card of your deck. You may put it on the bottom."},
    {"Grove Sister", "Unit", "cards/groveSister.png", 2, 0, 4, 2, 1, "Fey", "", "Seelie", "common", "Mend",
     "Ability: heal an adjacent friendly unit for 2."},
    {"Heartwood Sister", "Unit", "cards/heartwoodSister.png", 3, 0, 5, 1, 1, "Honorable", "", "Seelie", "common", "Mend",
     "Ability: heal an adjacent friendly unit for 3."},
    {"Sylvan Enchantress", "Unit", "cards/sylvanEnchantress.png", 4, 0, 5, 2, 2, "Arcane", "", "Seelie", "rare", "Bewitch",
     "Ability: an enemy unit at range 2 cannot attack on its owner's next turn."},
    {"Duchess Dewbell", "Unit", "cards/duchessDewbell.png", 4, 0, 7, 3, 1, "Honorable", "", "Seelie", "rare", "Command",
     "Command: an adjacent friendly unit may act immediately after Dewbell."},
    {"Starbloom Knight", "Unit", "cards/starbloomKnight.png", 5, 0, 8, 4, 1, "Honorable", "", "Seelie", "common", "Charge",
     "Charge. Starbloom Knight may move two squares before attacking."},
    {"Sylvan Champion", "Unit", "cards/sylvanChampion.png", 6, 0, 9, 5, 1, "Honorable", "", "Seelie", "rare", "Rally",
     "Friendly Fey units within range 2 gain +1 attack."},
    {"Crystal Unicorn", "Unit", "cards/crystalUnicorn.png", 7, 0, 9, 4, 1, "Arcane", "", "Seelie", "legendary", "Ward",
     "Ward. The first time Crystal Unicorn would be damaged each turn, prevent it."},
    {"Hidden Path", "Spell", "cards/hiddenPath.png", 2, 0, 0, 0, 0, "Arcane", "", "Seelie", "common", "Instant",
     "Move a friendly unit up to three squares. It may not attack this turn."},
    {"Heartshoot", "Spell", "cards/heartshoot.png", 3, 0, 0, 0, 0, "Fey", "", "Seelie", "rare", "Instant",
     "Deal 4 damage to a unit at range 3. Heal your hero for 2."},
    {"Heartwood", "Structure", "cards/heartwood.png", 4, 0, 12, 0, 0, "Ancient", "", "Seelie", "rare", "Grow",
     "Grow 2. After two turns Heartwood produces a Grove Sister on an adjacent square."},

    // --- the Unseelie Court ------------------------------------------------
    {"Blightling", "Unit", "cards/blightling.png", 1, 0, 2, 1, 1, "Corrupt", "", "Unseelie", "starter", "Spore",
     "When Blightling dies, adjacent enemy units take 1 damage."},
    {"Gloom Fairy", "Unit", "cards/gloomFairy.png", 2, 0, 3, 2, 2, "Corrupt", "", "Unseelie", "uncommon", "Swift",
     "Swift. Gloom Fairy may move after attacking."},
    {"Eyeblight", "Unit", "cards/eyeblight.png", 3, 0, 4, 2, 3, "Undead", "", "Unseelie", "common", "Watcher",
     "Enemy units cannot use Ambush while Eyeblight is on the board."},
    {"Ashenfang", "Unit", "cards/Ashenfang.png", 5, 0, 7, 5, 1, "Undead", "", "Unseelie", "rare", "Rebirth",
     "Rebirth 1. Ashenfang returns once with 1 health when destroyed."},
    {"Thorn Griffin", "Unit", "cards/thornGriffin.png", 5, 0, 8, 4, 1, "Wild", "", "Unseelie", "rare", "Flying",
     "Flying. Thorn Griffin ignores holes and may hop over other pieces."},
    {"Erevan the Shadow", "Unit", "cards/erevanTheShadow.png", 6, 0, 7, 5, 1, "Corrupt", "", "Unseelie", "legendary", "Ambush",
     "Ambush. Erevan deals double damage to units that have not yet acted."},

    // --- the Mirewatch Resistance ------------------------------------------
    {"Mirewatch Informant", "Unit", "cards/mirewatchInformant.png", 1, 0, 2, 1, 1, "Wild", "", "Mirewatch", "starter", "Scout",
     "Deploy: reveal the top card of your opponent's deck."},
    {"Swamp Tracker", "Unit", "cards/swampTracker.png", 2, 0, 4, 2, 2, "Wild", "", "Mirewatch", "common", "Tracker",
     "Swamp Tracker ignores movement penalties from marsh."},
    {"Bog Spearman", "Unit", "cards/bogSpearman.png", 3, 0, 5, 3, 2, "Wild", "", "Mirewatch", "common", "Reach",
     "Reach. Bog Spearman strikes at range 2 without moving."},
    {"Archivist Mosswake", "Unit", "cards/archivistMosswake.png", 4, 0, 5, 2, 1, "Ancient", "", "Mirewatch", "rare", "Insight",
     "Deploy: look at the top two cards of your deck and keep one."},
    {"Marshland Veteran", "Unit", "cards/marshlandVeteran.png", 4, 0, 6, 3, 1, "Honorable", "", "Mirewatch", "common", "Steadfast",
     "Steadfast. Marshland Veteran cannot be pushed or stunned."},
    {"Vanya Bluewater", "Unit", "cards/vanyaBluewater.png", 5, 0, 7, 3, 2, "Arcane", "", "Mirewatch", "rare", "Tide",
     "Ability: push an enemy unit one square away from Vanya."},
    {"Donella of the Marsh", "Unit", "cards/donellaOfTheMarsh.png", 6, 0, 8, 4, 1, "Ancient", "", "Mirewatch", "legendary", "Warden",
     "Friendly units adjacent to Donella take 1 less damage."},
    // Real catalogue title and artwork, intentionally carrying no supported
    // Spell effect.  The undefined-spell captures resolve this centralized
    // fixture so they exercise the same fail-closed client path without
    // inventing an inline card in main.cpp.
    {"Hidden Camp", "Spell", "cards/hiddenCamp.png", 10, 0, 0, 0, 0, "Wild", "", "Mirewatch", "common", "Concealed Deployment",
     "Friendly companions may be deployed beside Hidden Camp."},

    // --- the Blackthorns ---------------------------------------------------
    {"Blackthorn Lumberjack", "Unit", "cards/blackthornLumberjack.png", 2, 0, 4, 3, 1, "Civilized", "", "Blackthorn", "starter", "Fell",
     "Blackthorn Lumberjack deals 3 extra damage to structures."},
    {"Goblin Ambusher", "Unit", "cards/goblinAmbusher.png", 2, 0, 3, 3, 1, "Corrupt", "", "Blackthorn", "common", "Ambush",
     "Ambush. Deals double damage to units that have not yet acted."},
    {"Goblin Sharpshooter", "Unit", "cards/goblinSharpshooter.png", 3, 0, 4, 2, 3, "Civilized", "", "Blackthorn", "common", "Line of Sight",
     "Line of sight. Deals 2 damage at range 3 along an unobstructed rank."},
    {"Blackthorn Debt Collector", "Unit", "cards/blackthornDebtCollector.png", 3, 0, 5, 2, 1, "Civilized", "", "Blackthorn", "common", "Collect",
     "When Debt Collector destroys a unit, gain 2 coins."},
    {"Blackthorn Alchemist", "Unit", "cards/blackthornAlchemist.png", 4, 0, 5, 2, 2, "Mechanical", "", "Blackthorn", "rare", "Reagent",
     "Ability: deal 2 damage to a unit at range 2 and 1 to yourself."},
    {"Blackthorn Foreman", "Unit", "cards/blackthornForeman.png", 5, 0, 8, 3, 1, "Mechanical", "", "Blackthorn", "rare", "Tax",
     "Tax 1. Gain 1 extra coin at the start of each of your turns."},
    {"Grask", "Unit", "cards/Grask.png", 7, 0, 11, 6, 1, "Mechanical", "", "Blackthorn", "legendary", "Siege",
     "Siege. Grask cannot be blocked by structures and destroys them on contact."},
};

// The board draws standing tokens from assets/characters, not card art, so a
// captured piece falls back to a placeholder without one. Token art almost
// always shares its basename with the card art; these are the ones that do not.
std::string boardTokenFor(std::string_view art)
{
    struct Exception
    {
        std::string_view art;
        std::string_view token;
    };
    static constexpr Exception Exceptions[] = {
        {"cards/crystalUnicorn.png", "characters/crystallineUnicorn.png"},
        {"cards/nettleStarbright.png", "characters/nettleStarbright_unmounted.png"},
        {"cards/heartwood.png", "characters/heartwoodTree.png"},
        {"cards/mirewatchInformant.png", "characters/resistanceInformant.png"},
    };

    std::string candidate;
    for (const Exception& exception : Exceptions)
    {
        if (art == exception.art)
        {
            candidate = std::string(exception.token);
            break;
        }
    }
    if (candidate.empty())
    {
        const std::size_t slash = art.rfind('/');
        if (slash == std::string_view::npos)
        {
            return {};
        }
        candidate = "characters/" + std::string(art.substr(slash + 1));
    }

    // Spells have no token; only claim one that is actually on disk.
    const std::optional<std::filesystem::path> resolved = resolveAssetPath(candidate);
    std::error_code error;
    if (!resolved || !std::filesystem::exists(*resolved, error))
    {
        return {};
    }
    return candidate;
}

card_data::Card makeCard(const SampleCard& source)
{
    card_data::Card card;
    card.title = source.title;
    card.type = source.type;
    card.imagePath = source.art;
    // Canonical trait first so the collection's trait filters bite; the court
    // second so a row still reads as belonging somewhere.
    card.traits.emplace_back(source.trait);
    if (source.trait2 && *source.trait2)
    {
        card.traits.emplace_back(source.trait2);
    }
    card.traits.emplace_back(source.court);
    card.keywords.emplace_back(source.keyword);
    card.integerValues.push_back({"cost", source.cost});
    if (source.heroCost > 0)
    {
        card.integerValues.push_back({"heroCost", source.heroCost});
    }
    card.integerValues.push_back({"health", source.health});
    card.integerValues.push_back({"attack", source.attack});
    card.integerValues.push_back({"attackRange", std::max(1, source.range)});
    card.integerValues.push_back({"moveRange", 1});
    // toGameCard synthesizes a move/attack action pair from these legacy keys
    // when a card carries no explicit action list, which is how these samples
    // acquire the ranges the board highlights are drawn from.
    card.integerValues.push_back({"move", 2});
    card.integerValues.push_back({"range", std::max(1, source.range)});
    // Named explicitly, because the synthesized fallback names read as debug
    // strings ("Bog Spearman Move") in the inspect popup.
    if (std::string_view(source.type) == "Unit" || std::string_view(source.type) == "Hero")
    {
        card_data::Action advance;
        advance.name = "Advance";
        advance.pattern = "omni";
        advance.minRange = 1;
        advance.maxRange = 2;
        advance.canMove = true;
        advance.canAttack = false;
        card.actions.push_back(advance);
        card.actionNames.emplace_back(advance.name);
        card.actionDisplayNames.emplace_back("Advance");

        if (source.attack > 0)
        {
            card_data::Action strike;
            strike.name = source.range > 1 ? "Loose" : "Strike";
            strike.kind = "ranged";
            strike.pattern = "omni";
            strike.minRange = 1;
            strike.maxRange = std::max(1, source.range);
            strike.damage = source.attack;
            strike.canMove = false;
            strike.canAttack = true;
            strike.lineOfSight = true;
            card.actions.push_back(strike);
            card.actionNames.emplace_back(strike.name);
            card.actionDisplayNames.emplace_back(strike.name);
        }
    }

    const std::string_view title = source.title;
    if (title == "Goblin Sharpshooter")
    {
        card.actions.clear();
        card.actionNames.clear();
        card.actionDisplayNames.clear();

        card_data::Action advance;
        advance.name = "Advance";
        advance.state = 0;
        advance.pattern = "omni";
        advance.minRange = 1;
        advance.maxRange = 2;
        advance.canMove = true;
        advance.canAttack = false;
        card.actions.push_back(advance);
        card.actionNames.emplace_back(advance.name);
        card.actionDisplayNames.emplace_back(advance.name);

        card_data::Action fire;
        fire.name = "Fire";
        fire.state = 1;
        fire.kind = "ranged";
        fire.pattern = "orthogonal";
        fire.minRange = 1;
        fire.maxRange = 3;
        fire.damage = source.attack;
        fire.canMove = false;
        fire.canAttack = true;
        fire.lineOfSight = true;
        fire.nextState = 1;
        card.actions.push_back(fire);
        card.actionNames.emplace_back(fire.name);
        card.actionDisplayNames.emplace_back(fire.name);
        card.stringValues.push_back({"ability", "transform"});
        card.stringValues.push_back({"State1Token", "characters/goblinSharpshooter_aim.png"});
        card.stringLists.push_back({"abilityLabels", {"Aim", "Lower Weapon"}});
    }
    else if (title == "Goblin Ambusher")
    {
        const std::vector<card_data::Action> materializedActions = card.actions;
        const std::vector<std::string> materializedNames = card.actionDisplayNames;
        for (std::size_t index = 0; index < materializedActions.size(); ++index)
        {
            card_data::Action hiddenAction = materializedActions[index];
            hiddenAction.name += " Hidden";
            hiddenAction.state = 1;
            if (hiddenAction.canAttack)
            {
                hiddenAction.nextState = 0;
            }
            card.actions.push_back(hiddenAction);
            card.actionNames.push_back(hiddenAction.name);
            card.actionDisplayNames.push_back(
                index < materializedNames.size() ? materializedNames[index] : hiddenAction.name);
        }
        card.stringValues.push_back({"ability", "dematerialize"});
        card.stringLists.push_back({"abilityLabels", {"Hide", "Reveal"}});
    }
    else if (title == "Blackthorn Foreman")
    {
        card.stringValues.push_back({"ability", "summon"});
        card.stringValues.push_back({"summon", "Blackthorn Lumberjack"});
        card.stringLists.push_back({"abilityLabels", {"Create Lumberjack"}});
    }
    card.integerValues.push_back({"width", 1});
    card.integerValues.push_back({"height", 1});
    card.stringValues.push_back({"rarity", source.rarity});
    card.stringValues.push_back({"description", source.text});
    card.stringValues.push_back({"movePattern", "omni"});
    if (const std::string token = boardTokenFor(source.art); !token.empty())
    {
        card.stringValues.push_back({"Token", token});
    }
    return card;
}

// Titles of a court's non-hero cards, cheapest first, so a fabricated deck has a
// plausible curve rather than the same four cards repeated.
std::vector<std::string> courtCards(
    const std::vector<card_data::Card>& library,
    std::string_view court,
    bool heroes)
{
    std::vector<std::string> titles;
    for (const SampleCard& source : SampleCards)
    {
        if (source.court != court)
        {
            continue;
        }
        const bool isHero = source.heroCost > 0;
        if (isHero != heroes)
        {
            continue;
        }
        const auto found = std::find_if(library.begin(), library.end(), [&](const card_data::Card& card) {
            return card.title == source.title;
        });
        if (found != library.end())
        {
            titles.emplace_back(source.title);
        }
    }
    return titles;
}

// Two copies apiece of the cheapest uniques until the deck holds exactly
// game_data::DeckCardCount non-hero cards, plus the named heroes.
deck_data::Deck buildDeck(
    const std::vector<card_data::Card>& library,
    const std::string& name,
    std::string_view court,
    std::size_t heroCount,
    int nonHeroCards)
{
    deck_data::Deck deck;
    deck.name = name;

    const std::vector<std::string> heroes = courtCards(library, court, true);
    for (std::size_t i = 0; i < heroCount && i < heroes.size(); ++i)
    {
        deck.cardTitles.push_back(heroes[i]);
    }

    const std::vector<std::string> cards = courtCards(library, court, false);
    int placed = 0;
    for (std::size_t pass = 0; pass < 2 && placed < nonHeroCards; ++pass)
    {
        for (const std::string& title : cards)
        {
            if (placed >= nonHeroCards)
            {
                break;
            }
            deck.cardTitles.push_back(title);
            ++placed;
        }
    }
    return deck;
}

} // namespace

const std::vector<std::string>& knownScreens()
{
    static const std::vector<std::string> screens = [] {
        std::vector<std::string> values = {
        "title-screen",
        "login",
        "login-error",
        "create-account",
        "create-account-invalid",
        "options",
        "options-audio",
        "options-account",
        "main-menu",
        "main-menu-hover",
        "main-menu-exit",
        "deck-select",
        "matchmaking",
        "deck-editor",
        "deck-editor-cards",
        // Collection states that are otherwise only reachable by clicking through
        // a live account: a finished deck, an empty roster, the inspect popup and
        // the unsaved-changes dialog.
        "deck-editor-full",
        "deck-editor-empty",
        "deck-editor-popup",
        "deck-editor-unsaved",
        "shop",
        "shop-reveal",
        "starter-decks",
        "starter-decks-pick",
        "admin-users",
        "admin-tools",
        "card-editor",
        "conquest",
        "story-select",
        "story-seelie-spoiler-warning",
        "story-mission-select",
        "story-mirewatch-mission-select",
        "story-seelie-mission-select",
        "story-blackthorn-open-mastery-choice",
        "game",
        // Match states beyond the dedicated Story Mode captures above.
        "game-bases",
        "game-bases-blue-large",
        "game-midgame",
        "game-hand-hover",
        "game-selected",
        "game-popup",
        "game-popup-tooltip",
        "game-action-choice",
        "game-undefined-spell-popup",
        "game-undefined-spell-rejected",
        "game-resign-confirmation",
        "game-victory",
        // Admin / card-editor / Conquest review states. These screens are all
        // but empty without a service behind them, so each key seeds the state
        // that makes its layout worth looking at.
        "admin-users-selected",
        "admin-users-popup",
        "card-editor-loaded",
        "conquest-events",
        "conquest-map",
        "conquest-loadouts",
        "conquest-preview",
        "conquest-registration",
        "conquest-registration-ready",
        "conquest-join-confirm",
        "conquest-joined",
        "conquest-loadouts-empty",
        "conquest-many-decks",
        "conquest-deck-edit",
        "conquest-orders-unsaved",
        "conquest-orders-exit",
        "conquest-interactions"};

        const auto appendScenarioArtScreens =
            [&](StoryCampaign campaign, std::string_view prefix) {
                for (const StoryMission& mission : storyMissions(campaign))
                {
                    values.emplace_back(std::string(prefix) + std::string(mission.id));
                }
            };
        const auto appendScenarioArtPopupScreens =
            [&](StoryCampaign campaign, std::string_view prefix) {
                for (const StoryMission& mission : storyMissions(campaign))
                {
                    if (mission.objectiveSpec.kind != StoryObjectiveKind::StoryOnly)
                    {
                        values.emplace_back(
                            std::string(prefix) + std::string(mission.id));
                    }
                }
            };
        const auto campaignKey = [](StoryCampaign campaign) -> std::string_view {
            switch (campaign)
            {
            case StoryCampaign::Mirewatch: return "mw";
            case StoryCampaign::Blackthorn: return "bt";
            case StoryCampaign::Seelie: return "se";
            }
            return "unknown";
        };
        const auto numbered = [](std::size_t index) {
            std::string result = std::to_string(index + 1);
            if (result.size() < 2)
            {
                result.insert(result.begin(), '0');
            }
            return result;
        };
        const auto briefingPageKey = [&](StoryCampaign campaign,
                                          const StoryMission& mission,
                                          std::size_t panelIndex) {
            return std::string("story-page-briefing-") +
                std::string(campaignKey(campaign)) + "-" +
                std::string(mission.id) + "-p" + numbered(panelIndex);
        };
        const auto beforeStepPageKey = [&](StoryCampaign campaign,
                                           const StoryMission& mission,
                                           std::size_t stepIndex,
                                           std::size_t panelIndex) {
            return std::string("story-page-beat-before-") +
                std::string(campaignKey(campaign)) + "-" +
                std::string(mission.id) + "-s" + numbered(stepIndex) +
                "-p" + numbered(panelIndex);
        };
        const auto actionPageKey = [&](StoryCampaign campaign,
                                       const StoryMission& mission,
                                       std::size_t stepIndex) {
            return std::string("story-page-action-") +
                std::string(campaignKey(campaign)) + "-" +
                std::string(mission.id) + "-s" + numbered(stepIndex);
        };
        const auto aftermathPageKey = [&](StoryCampaign campaign,
                                          const StoryMission& mission,
                                          std::size_t panelIndex) {
            return std::string("story-page-beat-aftermath-") +
                std::string(campaignKey(campaign)) + "-" +
                std::string(mission.id) + "-p" + numbered(panelIndex);
        };
        const auto appendStoryPageScreens = [&](StoryCampaign campaign) {
            int tacticalOrdinal = 0;
            const std::string_view boardPrefix = campaign == StoryCampaign::Mirewatch
                ? "story-mirewatch-game-"
                : campaign == StoryCampaign::Blackthorn ? "story-game-" : "story-seelie-game-";
            for (const StoryMission& mission : storyMissions(campaign))
            {
                for (std::size_t panelIndex = 0;
                     panelIndex < mission.briefing.size();
                     ++panelIndex)
                {
                    values.emplace_back(
                        briefingPageKey(campaign, mission, panelIndex));
                }

                if (mission.objectiveSpec.kind == StoryObjectiveKind::StoryOnly)
                {
                    continue;
                }
                values.emplace_back(std::string(boardPrefix) +
                    std::to_string(++tacticalOrdinal) + "-board");
                // Scripted missions expose every authored action immediately
                // before it is taken. Scriptless tactical missions expose the
                // one production state in which their open objective begins.
                const std::size_t actionCount = mission.script.empty()
                    ? 1
                    : mission.script.size();
                for (std::size_t stepIndex = 0;
                     stepIndex < actionCount;
                     ++stepIndex)
                {
                    values.emplace_back(
                        actionPageKey(campaign, mission, stepIndex));
                }
                for (std::size_t stepIndex = 0;
                     stepIndex < mission.script.size();
                     ++stepIndex)
                {
                    const StoryScriptAction& step = mission.script[stepIndex];
                    for (std::size_t panelIndex = 0;
                         panelIndex < step.panelsBefore.size();
                         ++panelIndex)
                    {
                        values.emplace_back(beforeStepPageKey(
                            campaign, mission, stepIndex, panelIndex));
                    }
                }
                for (std::size_t panelIndex = 0;
                     panelIndex < mission.aftermath.size();
                     ++panelIndex)
                {
                    values.emplace_back(
                        aftermathPageKey(campaign, mission, panelIndex));
                }
            }
        };

        // Story fixtures are generated from the active routes so removed scenes
        // and newly added practices cannot leave stale aliases behind. Every numbered briefing page uses StoryIntro; every authored
        // action, before-step panel, and aftermath panel for a tactical mission
        // uses its production gameplay renderer and state path.
        appendStoryPageScreens(StoryCampaign::Mirewatch);
        appendStoryPageScreens(StoryCampaign::Blackthorn);
        appendStoryPageScreens(StoryCampaign::Seelie);
        appendScenarioArtScreens(StoryCampaign::Mirewatch, "story-art-mw-");
        appendScenarioArtScreens(StoryCampaign::Blackthorn, "story-art-bt-");
        appendScenarioArtScreens(StoryCampaign::Seelie, "story-art-se-");
        // Every tactical scenario also exercises the same commissioned art in
        // the production in-mission popup renderer. Story-only entries have no
        // board state, so their supported layout is the briefing renderer above.
        appendScenarioArtPopupScreens(
            StoryCampaign::Mirewatch, "story-art-popup-mw-");
        appendScenarioArtPopupScreens(
            StoryCampaign::Blackthorn, "story-art-popup-bt-");
        appendScenarioArtPopupScreens(
            StoryCampaign::Seelie, "story-art-popup-se-");
        return values;
    }();
    return screens;
}

std::optional<Request> parseCommandLine(int argc, char** argv)
{
    Request request;
    bool requested = false;

    for (int i = 1; i < argc; ++i)
    {
        const std::string_view argument(argv[i] ? argv[i] : "");
        if (startsWith(argument, "--ui-capture="))
        {
            request.outputDirectory = std::string(argument.substr(std::string_view("--ui-capture=").size()));
            requested = true;
        }
        else if (startsWith(argument, "--ui-capture-screens="))
        {
            request.screens =
                splitList(argument.substr(std::string_view("--ui-capture-screens=").size()));
        }
        else if (startsWith(argument, "--ui-capture-size="))
        {
            const std::string value(argument.substr(std::string_view("--ui-capture-size=").size()));
            const std::size_t separator = value.find('x');
            if (separator != std::string::npos)
            {
                const int width = std::atoi(value.substr(0, separator).c_str());
                const int height = std::atoi(value.substr(separator + 1).c_str());
                if (width > 0 && height > 0)
                {
                    request.width = static_cast<unsigned int>(width);
                    request.height = static_cast<unsigned int>(height);
                }
            }
        }
        else if (startsWith(argument, "--ui-capture-warmup="))
        {
            const int frames =
                std::atoi(std::string(argument.substr(std::string_view("--ui-capture-warmup=").size())).c_str());
            request.warmupFrames = std::clamp(frames, 1, 600);
        }
    }

    if (!requested)
    {
        return std::nullopt;
    }

    if (request.screens.empty())
    {
        request.screens = knownScreens();
    }
    return request;
}

bool prepareOutputDirectory(
    Request& request,
    const std::filesystem::path& executablePath,
    std::string& error)
{
    error.clear();
    request.captureCheckout = inspectGitIdentity(executablePath);
    if (request.screens.empty())
    {
        error = "Capture request contains no screen keys.";
        return false;
    }
    for (const std::string& screen : request.screens)
    {
        if (std::find(knownScreens().begin(), knownScreens().end(), screen) ==
            knownScreens().end())
        {
            error = "Capture request contains an unknown screen key: " + screen;
            return false;
        }
    }
    if (request.outputDirectory.empty())
    {
        error = "Capture output directory is empty.";
        return false;
    }

    std::error_code filesystemError;
    const bool exists = std::filesystem::exists(request.outputDirectory, filesystemError);
    if (filesystemError)
    {
        error = "Could not inspect capture output directory '" +
            request.outputDirectory.string() + "': " + filesystemError.message();
        return false;
    }

    if (exists)
    {
        if (!std::filesystem::is_directory(request.outputDirectory, filesystemError) ||
            filesystemError)
        {
            error = "Capture output path is not a directory: " +
                request.outputDirectory.string();
            return false;
        }
        const bool empty = std::filesystem::is_empty(request.outputDirectory, filesystemError);
        if (filesystemError)
        {
            error = "Could not inspect capture output directory '" +
                request.outputDirectory.string() + "': " + filesystemError.message();
            return false;
        }
        if (!empty)
        {
            error = "Capture output directory is not empty; refusing to overwrite or mix evidence: " +
                request.outputDirectory.string();
            return false;
        }
    }

    if (request.captureCheckout.repositoryRoot.empty())
    {
        error = "Could not locate the repository root for the capture input snapshot.";
        return false;
    }
    const InputSnapshot buildSnapshot = embeddedBuildSnapshot();
    if (!buildSnapshot.available)
    {
        error = "Build input snapshot is unavailable: " + buildSnapshot.error;
        return false;
    }
    request.captureStartSnapshot = inspectInputSnapshot(
        std::filesystem::path(request.captureCheckout.repositoryRoot));
    if (!request.captureStartSnapshot.available)
    {
        error = "Capture-start input snapshot is unavailable: " +
            request.captureStartSnapshot.error;
        return false;
    }
    if (!snapshotsEqual(buildSnapshot, request.captureStartSnapshot))
    {
        error = "Capture inputs differ from the executable's embedded build snapshot";
        const std::vector<std::string> mismatches =
            snapshotMismatchGroups(buildSnapshot, request.captureStartSnapshot);
        if (!mismatches.empty())
        {
            error += " (groups: " + joined(mismatches) + ")";
        }
        error += "; rebuild the client before capturing.";
        return false;
    }

    if (!exists &&
        (!std::filesystem::create_directories(request.outputDirectory, filesystemError) ||
         filesystemError))
    {
        error = "Could not create capture output directory '" +
            request.outputDirectory.string() + "': " + filesystemError.message();
        return false;
    }
    return true;
}

std::string captureFileName(std::size_t index, std::string_view screen)
{
    std::ostringstream name;
    name << std::setw(2) << std::setfill('0') << (index + 1)
         << '-' << screen << ".png";
    return name.str();
}

bool saveWindow(const sf::RenderWindow& window, const std::filesystem::path& path)
{
    const sf::Vector2u size = window.getSize();
    if (size.x == 0 || size.y == 0)
    {
        return false;
    }

    sf::Texture texture;
    if (!texture.resize(size))
    {
        return false;
    }
    texture.update(window);

    return texture.copyToImage().saveToFile(path);
}

bool writeCompletionManifest(
    const Request& request,
    const std::vector<std::string>& successfulFiles,
    const std::filesystem::path& executablePath,
    std::string& error)
{
    error.clear();
    std::vector<std::string> expectedFiles;
    expectedFiles.reserve(request.screens.size());
    for (std::size_t i = 0; i < request.screens.size(); ++i)
    {
        expectedFiles.push_back(captureFileName(i, request.screens[i]));
    }
    if (successfulFiles != expectedFiles)
    {
        error = "Successful capture file list does not exactly match the requested screens.";
        return false;
    }

    const std::set<std::string> expectedSet(expectedFiles.begin(), expectedFiles.end());
    std::set<std::string> actualSet;
    std::error_code filesystemError;
    for (std::filesystem::directory_iterator iterator(request.outputDirectory, filesystemError), end;
         !filesystemError && iterator != end;
         iterator.increment(filesystemError))
    {
        if (!iterator->is_regular_file(filesystemError) || filesystemError)
        {
            error = "Capture output contains an unexpected non-file entry: " +
                iterator->path().filename().string();
            return false;
        }
        const std::string filename = iterator->path().filename().string();
        if (!expectedSet.contains(filename))
        {
            error = "Capture output contains an unexpected file: " + filename;
            return false;
        }
        if (iterator->file_size(filesystemError) == 0 || filesystemError)
        {
            error = "Capture PNG is empty or unreadable: " + filename;
            return false;
        }
        actualSet.insert(filename);
    }
    if (filesystemError)
    {
        error = "Could not enumerate capture output directory '" +
            request.outputDirectory.string() + "': " + filesystemError.message();
        return false;
    }
    if (actualSet != expectedSet || actualSet.size() != request.screens.size())
    {
        error = "Produced PNG count or names do not exactly match the requested screens.";
        return false;
    }

    const bool exactRegisteredScreenSet = request.screens == knownScreens();
    const auto storyActionCount = [](std::string_view campaignPrefix) {
        return static_cast<std::size_t>(std::count_if(
            knownScreens().begin(),
            knownScreens().end(),
            [&](const std::string& screen) {
                return screen.rfind(campaignPrefix, 0) == 0;
            }));
    };
    const std::size_t mirewatchStoryActionCount =
        storyActionCount("story-page-action-mw-");
    const std::size_t blackthornStoryActionCount =
        storyActionCount("story-page-action-bt-");
    const std::size_t seelieStoryActionCount =
        storyActionCount("story-page-action-se-");
    const CheckoutIdentity& captureCheckout = request.captureCheckout;
    const InputSnapshot buildSnapshot = embeddedBuildSnapshot();
    const InputSnapshot captureEndSnapshot = captureCheckout.repositoryRoot.empty()
        ? InputSnapshot{}
        : inspectInputSnapshot(std::filesystem::path(captureCheckout.repositoryRoot));
    if (!buildSnapshot.available)
    {
        error = "Build input snapshot is unavailable: " + buildSnapshot.error;
        return false;
    }
    if (!request.captureStartSnapshot.available)
    {
        error = "Capture-start input snapshot is unavailable: " +
            request.captureStartSnapshot.error;
        return false;
    }
    if (!captureEndSnapshot.available)
    {
        error = "Capture-end input snapshot is unavailable: " +
            captureEndSnapshot.error;
        return false;
    }
    const bool startMatchesBuild =
        snapshotsEqual(buildSnapshot, request.captureStartSnapshot);
    const bool endMatchesBuild =
        snapshotsEqual(buildSnapshot, captureEndSnapshot);
    const bool stableDuringCapture =
        snapshotsEqual(request.captureStartSnapshot, captureEndSnapshot);
    std::vector<std::string> mismatchGroups =
        snapshotMismatchGroups(buildSnapshot, request.captureStartSnapshot);
    for (const std::string& group :
         snapshotMismatchGroups(buildSnapshot, captureEndSnapshot))
    {
        if (std::find(mismatchGroups.begin(), mismatchGroups.end(), group) ==
            mismatchGroups.end())
        {
            mismatchGroups.push_back(group);
        }
    }
    if (!startMatchesBuild || !endMatchesBuild || !stableDuringCapture)
    {
        error = stableDuringCapture
            ? "Capture inputs do not match the executable's embedded build snapshot"
            : "Capture inputs changed while screenshots were being produced";
        if (!mismatchGroups.empty())
        {
            error += " (groups: " + joined(mismatchGroups) + ")";
        }
        error += "; refusing to publish a completion manifest.";
        return false;
    }

    const ExecutableIdentity executable = inspectExecutable(executablePath);
    if (executable.sha256.empty())
    {
        error = "Could not compute SHA-256 for the capture executable.";
        return false;
    }
    const bool commitMatch = build_identity::SourceAvailable &&
        !captureCheckout.commit.empty() &&
        build_identity::SourceCommit == captureCheckout.commit;
    const bool sourceBinaryMatch =
        startMatchesBuild && endMatchesBuild && stableDuringCapture;
    const std::string sourceBinaryMatchReason =
        sourceBinaryMatch
            ? "verified-content-snapshot-match"
            : "build-and-capture-inputs-differ";

    std::ostringstream manifest;
    manifest << "{\n"
             << "  \"schemaVersion\": 3,\n"
             << "  \"completionMarker\": \"ui-capture-complete\",\n"
             << "  \"completed\": true,\n"
             << "  \"resolution\": {\"width\": " << request.width
             << ", \"height\": " << request.height << "},\n"
             << "  \"requestedScreenCount\": " << request.screens.size() << ",\n"
             << "  \"registeredScreenCount\": " << knownScreens().size() << ",\n"
             << "  \"registeredStoryActionScreenCount\": "
             << (mirewatchStoryActionCount + blackthornStoryActionCount +
                 seelieStoryActionCount)
             << ",\n"
             << "  \"registeredStoryActionScreenCounts\": {\"mirewatch\": "
             << mirewatchStoryActionCount << ", \"blackthorn\": "
             << blackthornStoryActionCount << ", \"seelie\": "
             << seelieStoryActionCount << "},\n"
             << "  \"exactRegisteredScreenSet\": "
             << (exactRegisteredScreenSet ? "true" : "false") << ",\n"
             << "  \"successCount\": " << successfulFiles.size() << ",\n"
             << "  \"requestedScreens\": [\n";
    for (std::size_t i = 0; i < request.screens.size(); ++i)
    {
        manifest << "    " << jsonQuoted(request.screens[i])
                 << (i + 1 == request.screens.size() ? "\n" : ",\n");
    }
    manifest << "  ],\n"
             << "  \"files\": [\n";
    for (std::size_t i = 0; i < successfulFiles.size(); ++i)
    {
        manifest << "    {\"screen\": " << jsonQuoted(request.screens[i])
                 << ", \"file\": " << jsonQuoted(successfulFiles[i]) << "}"
                 << (i + 1 == successfulFiles.size() ? "\n" : ",\n");
    }
    manifest << "  ],\n"
             << "  \"executable\": {\n"
             << "    \"path\": " << jsonQuoted(executable.path) << ",\n"
             << "    \"sizeBytes\": " << executable.size << ",\n"
             << "    \"fnv1a64\": " << jsonQuoted(executable.fnv1a64) << ",\n"
             << "    \"sha256\": " << jsonQuoted(executable.sha256) << "\n"
             << "  },\n"
             << "  \"build\": {\n"
             << "    \"configuration\": " << jsonQuoted(buildConfiguration()) << ",\n"
             << "    \"compiler\": " << jsonQuoted(compilerIdentity()) << ",\n"
             << "    \"compiledAt\": " << jsonQuoted(std::string(__DATE__) + " " + __TIME__) << "\n"
             << "  },\n"
             << "  \"buildSource\": {\n"
             << "    \"identityAvailable\": "
             << (build_identity::SourceAvailable ? "true" : "false") << ",\n"
             << "    \"gitCommit\": " << jsonQuoted(build_identity::SourceCommit) << ",\n"
             << "    \"dirty\": " << (build_identity::SourceDirty ? "true" : "false") << ",\n"
             << "    \"method\": " << jsonQuoted(build_identity::SourceMethod) << ",\n"
             << "    \"identityGeneratedUtc\": "
             << jsonQuoted(build_identity::GeneratedUtc) << "\n"
             << "  },\n"
             << "  \"inputSnapshot\": {\n"
             << "    \"schema\": " << jsonQuoted(build_identity::SnapshotSchema) << ",\n"
             << "    \"algorithm\": " << jsonQuoted(build_identity::SnapshotAlgorithm) << ",\n"
             << "    \"scope\": " << jsonQuoted(build_identity::SnapshotScope) << ",\n"
             << "    \"canonicalization\": \"sorted-length-framed-path-size-content-digest-v1\",\n"
             << "    \"build\": \n";
    writeSnapshotJson(manifest, buildSnapshot, "    ");
    manifest << ",\n    \"captureStart\": \n";
    writeSnapshotJson(manifest, request.captureStartSnapshot, "    ");
    manifest << ",\n    \"captureEnd\": \n";
    writeSnapshotJson(manifest, captureEndSnapshot, "    ");
    manifest << ",\n"
             << "    \"startMatchesBuild\": "
             << (startMatchesBuild ? "true" : "false") << ",\n"
             << "    \"endMatchesBuild\": "
             << (endMatchesBuild ? "true" : "false") << ",\n"
             << "    \"stableDuringCapture\": "
             << (stableDuringCapture ? "true" : "false") << ",\n"
             << "    \"match\": "
             << (sourceBinaryMatch ? "true" : "false") << ",\n"
             << "    \"mismatchGroups\": [";
    for (std::size_t index = 0; index < mismatchGroups.size(); ++index)
    {
        manifest << (index == 0 ? "" : ", ") << jsonQuoted(mismatchGroups[index]);
    }
    manifest << "]\n"
             << "  },\n"
             << "  \"captureCheckout\": {\n"
             << "    \"repositoryRoot\": " << jsonQuoted(captureCheckout.repositoryRoot) << ",\n"
             << "    \"gitRef\": " << jsonQuoted(captureCheckout.reference) << ",\n"
             << "    \"gitCommit\": " << jsonQuoted(captureCheckout.commit) << ",\n"
             << "    \"dirtyKnown\": "
             << (captureCheckout.dirtyKnown ? "true" : "false") << ",\n"
             << "    \"dirty\": ";
    if (captureCheckout.dirtyKnown)
    {
        manifest << (captureCheckout.dirty ? "true" : "false");
    }
    else
    {
        manifest << "null";
    }
    manifest << ",\n"
             << "    \"commitMethod\": \"direct-git-metadata\",\n"
             << "    \"dirtyMethod\": "
             << jsonQuoted(captureCheckout.dirtyKnown
                    ? "git-status-porcelain-v1"
                    : "unavailable") << ",\n"
             << "    \"capturedAt\": \"capture-start-before-output-admission\"\n"
             << "  },\n"
             << "  \"buildAndCaptureCommitMatch\": "
             << (commitMatch ? "true" : "false") << ",\n"
             << "  \"sourceBinaryMatch\": "
             << (sourceBinaryMatch ? "true" : "false") << ",\n"
             << "  \"sourceBinaryMatchReason\": "
             << jsonQuoted(sourceBinaryMatchReason) << "\n"
             << "}\n";

    const std::filesystem::path finalPath =
        request.outputDirectory / CompletionManifestFileName;
    const std::filesystem::path temporaryPath =
        request.outputDirectory / ".capture-manifest.json.tmp";
    if (std::filesystem::exists(finalPath, filesystemError) || filesystemError)
    {
        error = "Completion manifest path already exists or cannot be inspected: " +
            finalPath.string();
        return false;
    }

    std::ofstream output(temporaryPath, std::ios::binary | std::ios::trunc);
    output << manifest.str();
    output.flush();
    if (!output)
    {
        output.close();
        std::error_code cleanupError;
        std::filesystem::remove(temporaryPath, cleanupError);
        error = "Could not write temporary capture manifest: " + temporaryPath.string();
        return false;
    }
    output.close();

    std::filesystem::rename(temporaryPath, finalPath, filesystemError);
    if (filesystemError)
    {
        std::error_code cleanupError;
        std::filesystem::remove(temporaryPath, cleanupError);
        error = "Could not publish capture completion manifest '" +
            finalPath.string() + "': " + filesystemError.message();
        return false;
    }
    return true;
}

std::vector<card_data::Card> sampleCardLibrary()
{
    std::vector<card_data::Card> library;
    library.reserve(std::size(SampleCards));
    for (const SampleCard& source : SampleCards)
    {
        library.push_back(makeCard(source));
    }
    return library;
}

std::vector<deck_data::Deck> sampleDecks(const std::vector<card_data::Card>& library)
{
    // Three decks at deliberately different stages, so one capture run shows a
    // finished deck, a half-built one and a barely-started one.
    std::vector<deck_data::Deck> decks;
    decks.push_back(buildDeck(library, "Seelie Court Tempo", "Seelie", 2, game_data::DeckCardCount));
    decks.push_back(buildDeck(library, "Mirewatch Attrition", "Mirewatch", 1, 13));
    decks.push_back(buildDeck(library, "Blackthorn Toll Road", "Blackthorn", 0, 7));
    return decks;
}

deck_data::Deck sampleLegalDeck(const std::vector<card_data::Card>& library)
{
    return buildDeck(library, "Seelie Court Tempo", "Seelie", 2, game_data::DeckCardCount);
}

std::vector<account_data::CollectionCard> sampleCollection(
    const std::vector<card_data::Card>& library)
{
    std::vector<account_data::CollectionCard> collection;
    collection.reserve(library.size());
    // Mostly playsets, so a fabricated deck is legal against the collection, with
    // a few singletons so the "at deck limit" and "only one held" states are both
    // visible in a capture.
    int step = 0;
    for (const card_data::Card& card : library)
    {
        const int copies = step % 7 == 3 ? 1 : (step % 3 == 0 ? 3 : 2);
        collection.push_back({card.title, copies});
        ++step;
    }
    return collection;
}

} // namespace bayou::client::ui_capture
