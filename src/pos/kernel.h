// Copyright (c) 2012-2013 The PPCoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#ifndef PPCOIN_KERNEL_H
#define PPCOIN_KERNEL_H

#include "primitives/transaction.h"
#include "arith_uint256.h"
#include "validation.h"
#include <cstdint>

class CBlock;
class CBlockIndex;

// MODIFIER_INTERVAL_RATIO:
// ratio of group interval length between the last group and the first group
static const int MODIFIER_INTERVAL_RATIO = 3;

// Compute the hash modifier for proof-of-stake
bool ComputeNextStakeModifier(CChainState &chain_state, BlockValidationState &state,
                              const CBlockIndex* pindexPrev, uint64_t& nStakeModifier,
                              bool& fGeneratedStakeModifier);

// Check whether stake kernel meets hash target
// Sets hashProofOfStake on success return
bool CheckStakeKernelHash(CChainState &chain_state, BlockValidationState &state, const CBlockIndex *pindexPrev,
                          unsigned int nBits, const CBlockIndex& blockIndexKernel, unsigned int nTxPrevOffset,
                          const CTransaction& txPrev, const COutPoint& prevout, unsigned int nTimeTx,
                          uint256& hashProofOfStake, arith_uint256& targetProofOfStake,
                          bool fPrintProofOfStake = false);

// Check kernel hash target and coinstake signature
// Sets hashProofOfStake on success return
bool CheckProofOfStake(CChainState &chain_state, BlockValidationState &state, const CBlockIndex *pindexPrev,
                       const CTransaction& tx, unsigned int nBits, uint256& hashProofOfStake,
                       arith_uint256 &targetProofOfStake);

// Check whether the coinstake timestamp meets protocol
bool CheckCoinStakeTimestamp(int64_t nTimeBlock, int64_t nTimeTx);

// Get stake modifier checksum
uint32_t GetStakeModifierChecksum(CChainState &chain_state, const CBlockIndex* pindex);

// Check stake modifier hard checkpoints
bool CheckStakeModifierCheckpoints(CChainState &chain_state, int nHeight, unsigned int nStakeModifierChecksum);

// Get time weight using supplied timestamps
int64_t GetWeight(const CChainState &chain_state, int currentHeight, int64_t nIntervalBeginning, int64_t nIntervalEnd);

#endif // PPCOIN_KERNEL_H