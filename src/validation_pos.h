#ifndef VALIDATION_POS_H
#define VALIDATION_POS_H

#include "consensus/validation.h"

class CBlockIndex;
class CChainState;
class CTransaction;
class CCoinsViewCache;

inline int64_t PastDrift(int64_t nTime) { return nTime - 10 * 60; }   // up to 10 minutes from the past
inline int64_t FutureDrift(int64_t nTime) { return nTime + 10 * 60; } // up to 10 minutes in the future

// These checks can only be done when all previous block have been added.
bool PeercoinContextualBlockChecks(CChainState& chain_state, const CBlock& block, BlockValidationState& state, CBlockIndex* pindex, bool fJustCheck);

bool CheckBlockSignature(const CBlock& block);

bool GetCoinAge(const CTransaction& tx, const CCoinsViewCache &view, uint64_t& nCoinAge, int64_t stakeMinAge);

CAmount GetProofOfStakeReward(int64_t nCoinAge, CAmount nFees);

#endif // VALIDATION_POS_H
