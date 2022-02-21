#include "validation_pos.h"

#include "primitives/block.h"
#include "pos/kernel.h"
#include "script/standard.h"
#include "result.h"
#include "validation.h"
#include "index/txindex.h"
#include "index/disktxpos.h"
#include "node/blockstorage.h"
#include "consensus/amount.h"
#include "chainparams.h"
#include "util/moneystr.h"

static const int64_t COIN_YEAR_REWARD = 10 * CENT; // 10%

extern std::set<CBlockIndex*> setDirtyBlockIndex;

void UpdateBlockIndexWithPoSData(CBlockIndex* pindex, const CTransactionRef& coinstake, const uint256& hashProofOfStake)
{
    pindex->prevoutStake = coinstake->vin[0].prevout;
    pindex->nStakeTime = coinstake->nTime;
    pindex->hashProofOfStake = hashProofOfStake;
}

void UpdateBlockIndexWithModifierData(CBlockIndex* pindex, const bool fEntropyBit, const uint64_t nStakeModifier, const bool fGeneratedStakeModifier, const uint32_t nStakeModifierChecksum)
{
    pindex->SetStakeEntropyBit(fEntropyBit);
    pindex->SetStakeModifier(nStakeModifier, fGeneratedStakeModifier);
    pindex->nStakeModifierChecksum = nStakeModifierChecksum;
}

struct EvalPoSOutput {
    EvalPoSOutput(const uint256& HashProofOfStake, const bool fGeneratedStakeModifier, const uint64_t StakeModifier, bool EntropyBit, uint32_t StakeModifierChecksum)
        : hashProofOfStake(HashProofOfStake)
        , fGeneratedStakeModifier(fGeneratedStakeModifier)
        , nStakeModifier(StakeModifier)
        , fEntropyBit(EntropyBit)
        , nStakeModifierChecksum(StakeModifierChecksum) {}

    uint256 hashProofOfStake;
    bool fGeneratedStakeModifier;
    uint64_t nStakeModifier;
    bool fEntropyBit{false};
    uint32_t nStakeModifierChecksum;
};

Result<EvalPoSOutput, std::string> CheckPoSBlockAndEvalPoSParams(const CChainState& chain_state, const uint256& blockHash, const std::optional<CTransactionRef>& coinstake, uint32_t BlocknBits, BlockValidationState &state, const CBlockIndex *pindex) {
    uint256 hashProofOfStake = uint256();
    // peercoin: verify hash target and signature of coinstake tx
    if (coinstake && !CheckProofOfStake(chain_state, state, pindex->pprev, **coinstake, BlocknBits, hashProofOfStake)) {
        LogPrintf("WARNING: %s: check proof-of-stake failed for block %s\n", __func__, blockHash.ToString());
        return Err(std::string("CheckProofOfStake failed for block")); // do not error here as we expect this during initial block download
    }

    // peercoin: compute stake entropy bit for stake modifier
    const bool fEntropyBit = CBlock::GetStakeEntropyBit(blockHash);

    // peercoin: compute stake modifier
    uint64_t nStakeModifier = 0;
    bool fGeneratedStakeModifier = false;
    if (!ComputeNextStakeModifier(chain_state, state, pindex, nStakeModifier, fGeneratedStakeModifier)) {
        LogPrintf("ConnectBlock() : ComputeNextStakeModifier() failed");
        return Err(std::string("ComputeNextStakeModifier() failed"));
    }

    const uint32_t blockFlags = CBlockIndex::ConstructFlags(pindex->IsProofOfStake(), fEntropyBit, fGeneratedStakeModifier);
    const std::optional<uint32_t> prevChecksum = pindex->pprev ? std::make_optional(pindex->pprev->nStakeModifierChecksum) : std::nullopt;
    const uint32_t nStakeModifierChecksum = GetStakeModifierChecksum(prevChecksum, pindex->IsProofOfStake(), hashProofOfStake, nStakeModifier, blockFlags);

    if (!CheckStakeModifierCheckpoints(chain_state.m_params.GetConsensus(), pindex->nHeight, nStakeModifierChecksum)) {
        return Err(strprintf("ConnectBlock() : Rejected by stake modifier checkpoint height=%d, modifier=0x%016llx", pindex->nHeight, nStakeModifier));
    }

    return Ok(EvalPoSOutput(hashProofOfStake, fGeneratedStakeModifier, nStakeModifier, fEntropyBit, nStakeModifierChecksum));
}

bool NeblioContextualBlockChecks(const CChainState& chain_state, const uint256& blockHash, const std::optional<CTransactionRef>& coinstake, uint32_t BlocknBits, BlockValidationState &state, CBlockIndex *pindex, bool fJustCheck)
{
    const Result<EvalPoSOutput, std::string> PoSResultWrapped = CheckPoSBlockAndEvalPoSParams(chain_state, blockHash, coinstake, BlocknBits, state, pindex);
    if(PoSResultWrapped.isErr()) {
        return false;
    }
    const EvalPoSOutput PoSResult = PoSResultWrapped.UNWRAP();


    if (fJustCheck)
        return true;

    // write everything to index
    if (coinstake)
    {
        UpdateBlockIndexWithPoSData(pindex, *coinstake, PoSResult.hashProofOfStake);
    }
    UpdateBlockIndexWithModifierData(pindex, PoSResult.fEntropyBit, PoSResult.nStakeModifier, PoSResult.fGeneratedStakeModifier, PoSResult.nStakeModifierChecksum);
    setDirtyBlockIndex.insert(pindex);  // queue a write to disk

    return true;
}

enum class ColdStakeKeyExtractionError
{
    KeySizeInvalid,
};

static Result<CPubKey, ColdStakeKeyExtractionError> ExtractColdStakePubKey(const CBlock& block)
{
    const CTxIn& coinstakeKernel = block.vtx[1]->vin[0];
    int          start           = 1 + (int)*coinstakeKernel.scriptSig.begin(); // skip sig
    start += 1 + (int)*(coinstakeKernel.scriptSig.begin() + start);             // skip flag
    const auto beg = coinstakeKernel.scriptSig.begin() + start + 1;
    const auto end = coinstakeKernel.scriptSig.end();
    if (beg > end) {
        return Err(ColdStakeKeyExtractionError::KeySizeInvalid);
    }
    CPubKey pubkey(std::vector<uint8_t>(beg, end));
    return Ok(pubkey);
}

bool CheckBlockSignature(const CBlock &block)
{
    typedef std::vector<uint8_t> valtype;

    if (block.IsProofOfWork())
        return block.vchBlockSig.empty();

    std::vector<valtype> vSolutions;


    const CTxOut& txout = block.vtx[1]->vout[1];

    const TxoutType whichType = Solver(txout.scriptPubKey, vSolutions);

    if (whichType == TxoutType::PUBKEY) {
        const valtype& vchPubKey = vSolutions[0];
        const CPubKey key(vchPubKey.begin(), vchPubKey.end());
        if (!key.IsFullyValid())
            return false;
        if (block.vchBlockSig.empty())
            return false;
        return key.Verify(block.GetHash(), block.vchBlockSig);
    } else if (whichType == TxoutType::COLDSTAKE) {
        auto keyResult = ExtractColdStakePubKey(block);
        if (keyResult.isErr()) {
            return error("CheckBlockSignature(): ColdStaking key extraction failed");
        }
        const CPubKey key = keyResult.unwrap(RESULT_PRE);
        return key.Verify(block.GetHash(), block.vchBlockSig);
    } else {
        return error("CheckBlockSignature(): Failed to solve for scriptPubKey type");
    }

    return error("CheckBlockSignature(): Failed to verify block signature of type %s", GetTxnOutputType(whichType));
}


bool GetCoinAge(const CChainState& chain_state, const CTransaction& tx, const CCoinsViewCache &view, uint64_t& nCoinAge, int64_t stakeMinAge)
{
    AssertLockHeld(cs_main);

    arith_uint256 bnCentSecond = 0; // coin age in the unit of cent-seconds
    int64_t nSMA               = stakeMinAge;
    nCoinAge                   = 0;

    if (tx.IsCoinBase())
        return true;

    // Transaction index is required to get to block header
//    if (!g_txindex)
//        return false;  // Transaction index not available

    for (const CTxIn& txin : tx.vin) {
        // First try finding the previous transaction in database
        const COutPoint &prevout = txin.prevout;
        Coin coin;

        if (!view.GetCoin(prevout, coin))
            continue;  // previous transaction not in main chain

//        CDiskTxPos postx;
//        CTransactionRef txPrev;
//        if (g_txindex->FindTxPosition(prevout.hash, postx))
        {
//            CAutoFile file(OpenBlockFile(postx, true), SER_DISK, CLIENT_VERSION);
//            CBlockHeader header;
//            try {
//                file >> header;
//                fseek(file.Get(), postx.nTxOffset, SEEK_CUR);
//                file >> txPrev;
//            } catch (const std::exception &e) {
//                return error("%s() : deserialize or I/O error in GetCoinAge()", __PRETTY_FUNCTION__);
//            }
//            if (txPrev->GetHash() != prevout.hash)
//                return error("%s() : txid mismatch in GetCoinAge()", __PRETTY_FUNCTION__);

            CBlockIndex* pindex = chain_state.m_chain[coin.nHeight];
            if(!pindex) {
                continue;
            }
            if (pindex->GetBlockTime() + nSMA > tx.nTime)
                continue; // only count coins meeting min age requirement

            const CAmount nValueIn = coin.out.nValue;
            int nEffectiveAge = tx.nTime - coin.nTime;

            bnCentSecond += arith_uint256(nValueIn) * nEffectiveAge / CENT;

            LogPrint(BCLog::VALIDATION, "coin age nValueIn=%-12lld nTimeDiff=%d bnCentSecond=%s\n", nValueIn, nEffectiveAge, bnCentSecond.ToString());
        }
//        else {
//            return error("%s() : tx missing in tx index in GetCoinAge()", __PRETTY_FUNCTION__);
//        }
    }


    arith_uint256 bnCoinDay = bnCentSecond * CENT / COIN / (24 * 60 * 60);
    LogPrint(BCLog::VALIDATION, "coin age bnCoinDay=%s\n", bnCoinDay.ToString());
    nCoinAge = bnCoinDay.GetLow64();
    return true;
}

// miner's coin stake reward based on coin age spent (coin-days)
CAmount GetProofOfStakeReward(int64_t nCoinAge, CAmount nFees)
{
    // CBlockLocator locator;

    int64_t nRewardCoinYear = COIN_YEAR_REWARD; // 10% reward up to end

    CAmount nSubsidy = nCoinAge * nRewardCoinYear * 33 / (365 * 33 + 8);
//    LogPrintf("coin-Subsidy %s\n", std::to_string(nSubsidy));
//    LogPrintf("coin-Age %s\n", std::to_string(nCoinAge));
//    LogPrintf("Coin Reward %s\n", std::to_string(nRewardCoinYear));
//    LogPrintf("GetProofOfStakeReward(): create=%s nCoinAge=%s\n", FormatMoney(nSubsidy), std::to_string(nCoinAge));

    return nSubsidy + nFees;
}
