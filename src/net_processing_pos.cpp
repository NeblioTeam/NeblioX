#include "net_processing_pos.h"

IntermediateBlockIndex HeadersToIntermediateBlockIndex(std::size_t toSkip, const CBlockIndex &precedingBlockIndex, const std::vector<CBlockHeader> &headers)
{
    if(toSkip >= headers.size()) {
        return {};
    }
    assert(!headers.empty());
    assert(precedingBlockIndex.GetBlockHash() == headers[toSkip].hashPrevBlock);
    IntermediateBlockIndex nominalBlockIndex;
    std::shared_ptr<IntermediateBlockIndexEntry> prevEntry =
        std::make_shared<IntermediateBlockIndexEntry>(headers[toSkip],
                                                      precedingBlockIndex.nChainWork,
                                                      precedingBlockIndex.nHeight);
    nominalBlockIndex.InsertEntry(prevEntry);
    for(std::size_t i = toSkip + 1; i < headers.size(); i++) {
        prevEntry = std::make_shared<IntermediateBlockIndexEntry>(headers[i], prevEntry->nChainWork, prevEntry->nHeight);
        nominalBlockIndex.InsertEntry(prevEntry);
    }
    return nominalBlockIndex;
}

arith_uint256 BIChainWork(const BIVariant &bi) {
    return std::visit([](const auto& biInstance) { return biInstance->nChainWork; }, bi);
}

int BIHeight(const BIVariant &bi) {
    return std::visit([](const auto& biInstance) { return biInstance->nHeight; }, bi);
}

uint256 BIBlockHash(const BIVariant &bi) {
    return std::visit([](const auto& biInstance) { return biInstance->GetBlockHash(); }, bi);
}
