#ifndef NET_PROCESSING_POS_H
#define NET_PROCESSING_POS_H

#include <map>
#include <cstdint>
#include <uint256.h>
#include <netaddress.h>
#include <threadsafety.h>
#include <sync.h>
#include <validation.h>

#include <boost/multi_array/index_gen.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index_container.hpp>

/**
 * Before storing things in the block index, we store some of the header data in memory
 */
struct IntermediateBlockIndexEntry
{
    arith_uint256 nChainWork{};
    CBlockHeader header;
    int nHeight;
    uint256 hash;

    IntermediateBlockIndexEntry(const CBlockHeader& blockHeader, const arith_uint256& prevWork, const int prevHeight)
    {
        header = blockHeader;
        nChainWork = prevWork + GetBlockProofFromBits(blockHeader.nBits);
        nHeight = prevHeight + 1;
        hash = blockHeader.GetHash();
    }

    uint256 GetBlockHash() const {
        return hash;
    }
};

class IntermediateBlockIndex
{
    struct HashTag
    {
    };

    struct HeightTag
    {
    };

    using IntermediaryBlockIndexContainer = boost::multi_index_container<
        std::shared_ptr<IntermediateBlockIndexEntry>,
        boost::multi_index::indexed_by<
            // height is unique because we expect only a series of consecutive headers
            boost::multi_index::ordered_unique<boost::multi_index::tag<HeightTag>,
                                               BOOST_MULTI_INDEX_MEMBER(IntermediateBlockIndexEntry,
                                                                        int,
                                                                        nHeight)>,
            boost::multi_index::ordered_unique<boost::multi_index::tag<HashTag>,
                                               BOOST_MULTI_INDEX_MEMBER(IntermediateBlockIndexEntry,
                                                                        uint256,
                                                                        hash)>
            >>;

    IntermediaryBlockIndexContainer container;

public:
    void InsertEntry(const std::shared_ptr<IntermediateBlockIndexEntry>& entry) {
        container.insert(entry);
    }

    decltype(auto) HashIndex() {
        return container.template get<HashTag>();
    }

    decltype(auto) HeightIndex() {
        return container.template get<HeightTag>();
    }

    bool empty() const {
        return container.empty();
    }

    std::size_t size() const {
        return container.size();
    }

    std::shared_ptr<IntermediateBlockIndexEntry> operator[](std::size_t idx) {
        assert(!empty());
        const auto& heightIndex = container.template get<HeightTag>();
        const int firstHeight = (*heightIndex.cbegin())->nHeight;
        return *heightIndex.find(static_cast<int>(idx) + firstHeight);
    }

    void erase_by_hash(const uint256& hash) {
        auto& hashIndex = container.template get<HashTag>();
        hashIndex.erase(hash);
    }
};

IntermediateBlockIndex HeadersToIntermediateBlockIndex(std::size_t toSkip, const CBlockIndex& precedingBlockIndex, const std::vector<CBlockHeader>& headers);


// The block index may come from one that we already have, or from one that is temporarily in the peer's claimed headers
using BIVariant = std::variant<const CBlockIndex*, std::shared_ptr<IntermediateBlockIndexEntry>>;
arith_uint256 BIChainWork(const BIVariant& bi);
int BIHeight(const BIVariant& bi);
uint256 BIBlockHash(const BIVariant& bi);

#endif // NET_PROCESSING_POS_H
