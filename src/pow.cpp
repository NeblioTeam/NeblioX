// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2018 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <pow.h>

#include <arith_uint256.h>
#include <chain.h>
#include <primitives/block.h>
#include <uint256.h>
#include <numeric>
#include "validation_pos.h"

const CBlockIndex* GetLastBlockIndex(const CBlockIndex* pindex, bool fProofOfStake)
{
    while (pindex && pindex->pprev && (pindex->IsProofOfStake() != fProofOfStake))
        pindex = pindex->pprev;
    return pindex;
}

/**
 * Calculates the average spacing between blocks correctly, by sorting the times of the last X blocks,
 * and calculating the average from adjacent differences
 *
 * @brief CalculateActualBlockSpacingForV3
 * @param pindexLast
 * @return the average time spacing between blocks
 */
int64_t CalculateActualBlockSpacingForV3(const CBlockIndex* pindexLast, const Consensus::Params& params)
{
    static constexpr const int64_t TARGET_AVERAGE_BLOCK_COUNT = 100;

    // get the latest blocks from the blocks. The amount of blocks is: TARGET_AVERAGE_BLOCK_COUNT
    int64_t forkBlock = params.nFork4RetargetCorrectHeight;
    // we start counting block times from the fork
    int64_t numOfBlocksToAverage = pindexLast->nHeight - (forkBlock + 1);
    // minimum number of blocks to calculate a difference is 2, and max is TARGET_AVERAGE_BLOCK_COUNT
    if (numOfBlocksToAverage <= 1) {
        numOfBlocksToAverage = 2;
    }
    if (numOfBlocksToAverage > TARGET_AVERAGE_BLOCK_COUNT) {
        numOfBlocksToAverage = TARGET_AVERAGE_BLOCK_COUNT;
    }

    // push block times to a vector
    std::vector<int64_t> blockTimes;
    std::vector<int64_t> blockTimeDifferences;
    blockTimes.reserve(numOfBlocksToAverage);
    blockTimeDifferences.reserve(numOfBlocksToAverage);
    const CBlockIndex* currIndex = pindexLast;
    blockTimes.resize(numOfBlocksToAverage);
    for (int64_t i = 0; i < numOfBlocksToAverage; i++) {
        // fill the blocks in reverse order
        blockTimes.at(numOfBlocksToAverage - i - 1) = currIndex->GetBlockTime();
        // move to the previous block
        currIndex = currIndex->pprev;
    }

    // sort block times to avoid negative values
    std::sort(blockTimes.begin(), blockTimes.end());
    // calculate adjacent differences
    std::adjacent_difference(blockTimes.cbegin(), blockTimes.cend(),
                             std::back_inserter(blockTimeDifferences));
    assert(blockTimeDifferences.size() >= 2);
    // calculate the average (n-1 because adjacent differences have size n-1)
    // begin()+1 because the first value is just a copy of the first value
    return std::accumulate(blockTimeDifferences.cbegin() + 1, blockTimeDifferences.cend(), 0) /
           (numOfBlocksToAverage - 1);
}

static unsigned int GetNextTargetRequiredV1(const CBlockIndex* pindexLast, bool fProofOfStake, const Consensus::Params& params)
{
    arith_uint256 bnTargetLimit = arith_uint256((fProofOfStake ? params.posLimit : params.powLimit).ToString());

    if (pindexLast == nullptr)
        return bnTargetLimit.GetCompact(); // genesis block

    const CBlockIndex* pindexPrev = GetLastBlockIndex(pindexLast, fProofOfStake);
    if (pindexPrev->pprev == 0)
        return bnTargetLimit.GetCompact(); // first block
    const CBlockIndex* indexPrevPrev = pindexPrev->pprev;
    if (!indexPrevPrev) {
//        NLog.write(b_sev::err,
//                   "Failed to get prev prev block index, even though it's not genesis block");
        return 0;
    }
    const CBlockIndex* pindexPrevPrev = GetLastBlockIndex(indexPrevPrev, fProofOfStake);
    if (pindexPrevPrev->pprev == 0)
        return bnTargetLimit.GetCompact(); // second block

    int64_t nActualSpacing = pindexPrev->GetBlockTime() - pindexPrevPrev->GetBlockTime();

    // ppcoin: target change every block
    // ppcoin: retarget with exponential moving toward target spacing
    arith_uint256 bnNew;
    bnNew.SetCompact(pindexPrev->nBits);

    arith_uint512 bnNew512(bnNew);
    unsigned int nTS       = params.TargetSpacing(pindexLast->nHeight);
    int64_t      nInterval = params.nTargetTimespan / nTS;
    bnNew512 *= ((nInterval - 1) * nTS + nActualSpacing + nActualSpacing);
    bnNew512 /= ((nInterval + 1) * nTS);

    bnNew = bnNew512.to_uint256();

    if (bnNew > bnTargetLimit)
        bnNew = bnTargetLimit;

    return bnNew.GetCompact();
}

static unsigned int GetNextTargetRequiredV2(const CBlockIndex* pindexLast, bool fProofOfStake, const Consensus::Params& params)
{
    arith_uint256 bnTargetLimit = arith_uint256((fProofOfStake ? params.posLimit : params.powLimit).ToString());

    if (pindexLast == nullptr)
        return bnTargetLimit.GetCompact(); // genesis block

    const CBlockIndex* pindexPrev = GetLastBlockIndex(pindexLast, fProofOfStake);
    if (pindexPrev->pprev == 0)
        return bnTargetLimit.GetCompact(); // first block
    const CBlockIndex* indexPrevPrev = pindexPrev->pprev;
    if (!indexPrevPrev) {
//        NLog.write(b_sev::err,
//                   "Failed to get prev prev block index, even though it's not genesis block");
        return 0;
    }
    const CBlockIndex* pindexPrevPrev = GetLastBlockIndex(indexPrevPrev, fProofOfStake);

    int64_t      nActualSpacing = pindexPrev->GetBlockTime() - pindexPrevPrev->GetBlockTime();
    unsigned int nTS            = params.TargetSpacing(pindexLast->nHeight);
    if (nActualSpacing < 0)
        nActualSpacing = nTS;

    // ppcoin: target change every block
    // ppcoin: retarget with exponential moving toward target spacing
    arith_uint256 bnNew;
    bnNew.SetCompact(pindexPrev->nBits);
    arith_uint512 bnNew512(bnNew);

    int64_t nInterval = params.nTargetTimespan / nTS;
    bnNew512 *= ((nInterval - 1) * nTS + nActualSpacing + nActualSpacing);
    bnNew512 /= ((nInterval + 1) * nTS);

    bnNew = bnNew512.to_uint256();

    if (bnNew <= 0 || bnNew > bnTargetLimit)
        bnNew = bnTargetLimit;

    return bnNew.GetCompact();
}

static unsigned int GetNextTargetRequiredV3(const CBlockIndex* pindexLast, bool fProofOfStake, const Consensus::Params& params)
{
    arith_uint256 bnTargetLimit = arith_uint256((fProofOfStake ? params.posLimit : params.powLimit).ToString());

    if (pindexLast == nullptr)
        return bnTargetLimit.GetCompact(); // genesis block

    const CBlockIndex* pindexPrev = GetLastBlockIndex(pindexLast, fProofOfStake);
    if (pindexPrev->pprev == 0)
        return bnTargetLimit.GetCompact(); // first block
    const CBlockIndex* indexPrevPrev = pindexPrev->pprev;
    if (!indexPrevPrev) {
//        NLog.write(b_sev::err,
//                   "Failed to get prev prev block index, even though it's not genesis block");
        return 0;
    }
    const CBlockIndex* pindexPrevPrev = GetLastBlockIndex(indexPrevPrev, fProofOfStake);
    if (pindexPrevPrev->pprev == 0)
        return bnTargetLimit.GetCompact(); // second block

    int64_t nActualSpacing = CalculateActualBlockSpacingForV3(pindexLast, params);

    const unsigned int nTS = params.TargetSpacing(pindexLast->nHeight);
    if (nActualSpacing < 0)
        nActualSpacing = nTS;

    /** if any of these assert fires, it means that you changed these parameteres.
     *  Be aware that the parameters k and l are fine tuned to produce a max shift in the difficulty in
     * the range [-3%,+5%]
     * This can be calculated with:
     * ((nInterval - l + k)*nTS + (m + l)*nActualSpacing)/((nInterval + k)*nTS + m*nActualSpacing),
     * with nActualSpacing being in the range [0,FutureDrift] = [0,600] If you change any of these
     * values, make sure you tune these variables again. A very high percentage on either side makes it
     * easier to change/manipulate the difficulty when mining
     */
    assert(FutureDrift(0) == 10 * 60);
    assert(params.TargetSpacing(pindexLast->nHeight) == 30);
    assert(params.nTargetTimespan == 2 * 60 * 60);

    // ppcoin: target change every block
    // ppcoin: retarget with exponential moving toward target spacing
    arith_uint256 newTarget;
    newTarget.SetCompact(pindexPrev->nBits); // target from previous block
    arith_uint512 newTarget512(newTarget);

    int64_t nInterval = params.nTargetTimespan / nTS;

    static constexpr const int k = 15;
    static constexpr const int l = 7;
    static constexpr const int m = 90;
    newTarget512 *= (nInterval - l + k) * nTS + (m + l) * nActualSpacing;
    newTarget512 /= (nInterval + k) * nTS + m * nActualSpacing;

    newTarget = newTarget512.to_uint256();

    if (newTarget <= 0 || newTarget > bnTargetLimit)
        newTarget = bnTargetLimit;

    return newTarget.GetCompact();
}

unsigned int GetNextWorkRequired(const CBlockIndex* pindexLast, bool fProofOfStake, const Consensus::Params& params)
{
    if (!fProofOfStake && params.fPowNoRetargeting)
        return pindexLast->nBits;
    if (fProofOfStake && params.fPowNoRetargeting)
        return UintToArith256(params.powLimit).GetCompact();

    if (pindexLast->nHeight < 2000)
        return GetNextTargetRequiredV1(pindexLast, fProofOfStake, params);
    else if (pindexLast->nHeight >= params.nFork4RetargetCorrectHeight)
        return GetNextTargetRequiredV3(pindexLast, fProofOfStake, params);
    else
        return GetNextTargetRequiredV2(pindexLast, fProofOfStake, params);
}

unsigned int CalculateNextWorkRequired(const CBlockIndex* pindexLast, int64_t nFirstBlockTime, const Consensus::Params& params)
{
    if (params.fPowNoRetargeting)
        return pindexLast->nBits;

    // Limit adjustment step
    int64_t nActualTimespan = pindexLast->GetBlockTime() - nFirstBlockTime;
    if (nActualTimespan < params.nPowTargetTimespan/4)
        nActualTimespan = params.nPowTargetTimespan/4;
    if (nActualTimespan > params.nPowTargetTimespan*4)
        nActualTimespan = params.nPowTargetTimespan*4;

    // Retarget
    const arith_uint256 bnPowLimit = UintToArith256(params.powLimit);
    arith_uint256 bnNew;
    bnNew.SetCompact(pindexLast->nBits);
    bnNew *= nActualTimespan;
    bnNew /= params.nPowTargetTimespan;

    if (bnNew > bnPowLimit)
        bnNew = bnPowLimit;

    return bnNew.GetCompact();
}

bool CheckProofOfWork(uint256 hash, unsigned int nBits, const Consensus::Params& params)
{
    bool fNegative;
    bool fOverflow;
    arith_uint256 bnTarget;

    bnTarget.SetCompact(nBits, &fNegative, &fOverflow);

    // Check range
    if (fNegative || bnTarget == 0 || fOverflow || bnTarget > UintToArith256(params.powLimit))
        return false;

    // Check proof of work matches claimed amount
    if (UintToArith256(hash) > bnTarget)
        return false;

    return true;
}
