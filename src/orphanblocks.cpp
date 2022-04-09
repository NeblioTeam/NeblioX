#include "orphanblocks.h"

#include <cassert>
#include "random.h"
#include "logging.h"

OrphanBlocks::OrphanBlocks(std::size_t maxOrphans) : maxOrphans(maxOrphans)
{
    assert(maxOrphans > 0);
}

void OrphanBlocks::clear()
{
    std::lock_guard<MutexType> lg(mtx);
    clear_unsafe();
}

void OrphanBlocks::clear_unsafe()
{
    byHash.clear();
    byPrevHash.clear();
}

std::unique_ptr<std::lock_guard<OrphanBlocks::MutexType> > OrphanBlocks::acquireLock() const
{
    return std::make_unique<std::lock_guard<OrphanBlocks::MutexType>>(mtx);
}

bool OrphanBlocks::blockExists(const uint256 &hash) const
{
    std::lock_guard<MutexType> lg(mtx);
    return blockExists_unsafe(hash);
}

bool OrphanBlocks::blockExists_unsafe(const uint256 &hash) const
{
    return byHash.find(hash) != byHash.cend();
}

bool OrphanBlocks::addBlock(const std::shared_ptr<const CBlock> &blockIn, const std::optional<int64_t> &senderNodeIdIn)
{
    std::lock_guard<MutexType> lg(mtx);
    return addBlock_unsafe(blockIn, senderNodeIdIn);
}

bool OrphanBlocks::addBlock_unsafe(const std::shared_ptr<const CBlock> &blockIn, const std::optional<int64_t> &senderNodeIdIn)
{
    if(!blockIn) {
        return false;
    }

    const uint256 hash = blockIn->GetHash();

    const OrphanBlock orphan = OrphanBlock::make(blockIn, senderNodeIdIn);

    std::lock_guard<MutexType> lg(mtx);

    if(byHash.find(hash) != byHash.cend()) {
        return false;
    }

    prune_unsafe();

    byHash[hash] = orphan;
    byPrevHash[blockIn->hashPrevBlock].push_back(orphan);

    return true;
}

std::optional<uint256> OrphanBlocks::getBlockRoot(const uint256 &hash) const
{
    std::lock_guard<MutexType> lg(mtx);
    return getBlockRoot_unsafe(hash);
}

std::optional<uint256> OrphanBlocks::getBlockRoot_unsafe(const uint256 &hash) const
{
    auto it = byHash.find(hash);
    if(it == byHash.end()) {
        return std::nullopt;
    }
    while(true) {
        const uint256& prevHash = it->second.block->hashPrevBlock;
        auto itPrev = byHash.find(prevHash);
        if(itPrev == byHash.end()) {
            return std::make_optional(it->first);
        } else {
            it = itPrev;
        }
    }
}

std::vector<OrphanBlock> OrphanBlocks::takeAllChildrenOf(const uint256 &blockHash)
{
    std::lock_guard<MutexType> lg(mtx);
    return takeAllChildrenOf_unsafe(blockHash);
}

std::vector<OrphanBlock> OrphanBlocks::takeAllChildrenOf_unsafe(const uint256 &blockHash)
{
    auto it = byPrevHash.find(blockHash);
    if(it == byPrevHash.end()) {
        return {};
    }

    // we make a copy of it (cheap, since it's all shared pointers)
    const std::vector<OrphanBlock> result = it->second;

    // now we drop all these blocks
    for(const auto& p: result) {
        dropBlock_unsafe(p.block->GetHash());
    }

    return result;
}

void OrphanBlocks::delOneDeepestChild_unsafe(const uint256 &hash)
{
    const auto it = byPrevHash.find(hash);
    if(it != byPrevHash.cend()) {
        delOneDeepestChild_unsafe(it->first);
    } else {
        dropBlock_unsafe(hash);
    }
}

bool OrphanBlocks::dropBlock_unsafe(const uint256 &hash)
{
    auto blockIt = byHash.find(hash);
    if(blockIt == byHash.cend()) {
        return false;
    }
    const uint256& prevBlockHash = blockIt->second.block->hashPrevBlock;
    auto prevBlockIt = byPrevHash.find(prevBlockHash);
    assert(prevBlockIt != byPrevHash.cend());
    std::vector<OrphanBlock>& prevs = prevBlockIt->second;
    if(prevs.size() == 1u) {
        // if this is the last element in the vector, just remove the vector
        byPrevHash.erase(prevBlockIt);
    } else {
        // otherwise, find that block that has this hash and remove it
        auto it = std::find_if(prevs.begin(), prevs.end(), [&](const auto& prevBlock) { return prevBlock.block->GetHash() == hash; });
        assert(it != prevs.end());
        if(it != prevs.end()) {
            prevs.erase(it);
        } else {
            LogPrintf("CRITICAL: Could not find prev block of a block that we previously found");
        }
    }
    return true;
}

void OrphanBlocks::prune_unsafe()
{
    if(byHash.empty()) {
        return;
    }

    const int randomIndex = static_cast<std::size_t>(GetRandInt(byHash.size()));

    const auto blockIt = std::next(byHash.begin(), randomIndex);

    const uint256& blockHash = blockIt->first;

    delOneDeepestChild_unsafe(blockHash);
}
