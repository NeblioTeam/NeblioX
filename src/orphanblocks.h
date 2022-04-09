#ifndef ORPHANBLOCKS_H
#define ORPHANBLOCKS_H

#include "uint256.h"
#include "primitives/block.h"
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <vector>

struct OrphanBlock
{
    std::shared_ptr<const CBlock> block;
    std::optional<int64_t> senderNodeId;

    static OrphanBlock make(const std::shared_ptr<const CBlock>& blockIn, const std::optional<int64_t>& senderNodeIdIn)
    {
        OrphanBlock result;
        result.block = blockIn;
        result.senderNodeId = senderNodeIdIn;
        return result;
    }
};

class OrphanBlocks
{
public:
    using MutexType = std::mutex;
    static const std::size_t MAX_ORPHANS_DEFAULT = 64;

private:
    mutable MutexType mtx;
    std::map<uint256, OrphanBlock> byHash;
    std::map<uint256, std::vector<OrphanBlock>> byPrevHash;
    const std::size_t maxOrphans;

    void delOneDeepestChild_unsafe(const uint256& hash);

    bool dropBlock_unsafe(const uint256& hash);

    void prune_unsafe();

public:
    OrphanBlocks(std::size_t maxOrphans = MAX_ORPHANS_DEFAULT);

    void clear();
    void clear_unsafe();

    [[nodiscard]] std::unique_ptr<std::lock_guard<MutexType>> acquireLock() const;

    [[nodiscard]] bool blockExists(const uint256& hash) const;
    [[nodiscard]] bool blockExists_unsafe(const uint256& hash) const;

    bool addBlock(const std::shared_ptr<const CBlock>& blockIn, const std::optional<int64_t>& senderNodeIdIn);
    bool addBlock_unsafe(const std::shared_ptr<const CBlock>& blockIn, const std::optional<int64_t>& senderNodeIdIn);

    [[nodiscard]] std::optional<uint256> getBlockRoot(const uint256& hash) const;
    [[nodiscard]] std::optional<uint256> getBlockRoot_unsafe(const uint256& hash) const;

    [[nodiscard]] std::vector<OrphanBlock> takeAllChildrenOf(const uint256& blockHash);
    [[nodiscard]] std::vector<OrphanBlock> takeAllChildrenOf_unsafe(const uint256& blockHash);
};

#endif // ORPHANBLOCKS_H
