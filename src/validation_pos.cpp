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

bool PeercoinContextualBlockChecks(CChainState& chain_state, const CBlock &block, BlockValidationState &state, CBlockIndex *pindex, bool fJustCheck)
{
    uint256 hashProofOfStake = uint256();
    arith_uint256 targetProofOfStake = arith_uint256();
    // peercoin: verify hash target and signature of coinstake tx
    if (block.IsProofOfStake() && !CheckProofOfStake(chain_state, state, pindex->pprev, *block.vtx[1], block.nBits, hashProofOfStake, targetProofOfStake)) {
        LogPrintf("WARNING: %s: check proof-of-stake failed for block %s\n", __func__, block.GetHash().ToString());
        return false; // do not error here as we expect this during initial block download
    }

    // peercoin: compute stake entropy bit for stake modifier
    unsigned int nEntropyBit = CBlock::GetStakeEntropyBit(block.GetHash());

    // peercoin: compute stake modifier
    uint64_t nStakeModifier = 0;
    bool fGeneratedStakeModifier = false;
    if (!ComputeNextStakeModifier(chain_state, state, pindex, nStakeModifier, fGeneratedStakeModifier))
        return error("ConnectBlock() : ComputeNextStakeModifier() failed");

    // compute nStakeModifierChecksum begin
    unsigned int nFlagsBackup      = pindex->nFlags;
    uint64_t nStakeModifierBackup  = pindex->nStakeModifier;
    uint256 hashProofOfStakeBackup = pindex->hashProofOfStake;

    // set necessary pindex fields
    if (!pindex->SetStakeEntropyBit(nEntropyBit))
        return error("ConnectBlock() : SetStakeEntropyBit() failed");
    pindex->SetStakeModifier(nStakeModifier, fGeneratedStakeModifier);
    pindex->hashProofOfStake = hashProofOfStake;


    const unsigned int nStakeModifierChecksum = GetStakeModifierChecksum(chain_state, pindex);

    // undo pindex fields
    pindex->nFlags           = nFlagsBackup;
    pindex->nStakeModifier   = nStakeModifierBackup;
    pindex->hashProofOfStake = hashProofOfStakeBackup;
    // compute nStakeModifierChecksum end

    if (!CheckStakeModifierCheckpoints(chain_state.m_params.GetConsensus(), pindex->nHeight, nStakeModifierChecksum))
        return error("ConnectBlock() : Rejected by stake modifier checkpoint height=%d, modifier=0x%016llx", pindex->nHeight, nStakeModifier);

    if (fJustCheck)
        return true;


    // write everything to index
    if (block.IsProofOfStake())
    {
        pindex->prevoutStake = block.vtx[1]->vin[0].prevout;
        pindex->nStakeTime = block.vtx[1]->nTime;
        pindex->hashProofOfStake = hashProofOfStake;
    }
    if (!pindex->SetStakeEntropyBit(nEntropyBit))
        return error("ConnectBlock() : SetStakeEntropyBit() failed");
    pindex->SetStakeModifier(nStakeModifier, fGeneratedStakeModifier);
    pindex->nStakeModifierChecksum = nStakeModifierChecksum;
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


bool GetCoinAge(const CTransaction& tx, const CCoinsViewCache &view, uint64_t& nCoinAge, int64_t stakeMinAge)
{
    static const bool fDebug = false;

    arith_uint256 bnCentSecond = 0; // coin age in the unit of cent-seconds
    int64_t nSMA               = stakeMinAge;
    nCoinAge                   = 0;

    if (tx.IsCoinBase())
        return true;

    // Transaction index is required to get to block header
    if (!g_txindex)
        return false;  // Transaction index not available

    for (const CTxIn& txin : tx.vin) {
        // First try finding the previous transaction in database
        const COutPoint &prevout = txin.prevout;
        Coin coin;

        if (!view.GetCoin(prevout, coin))
            continue;  // previous transaction not in main chain

        CDiskTxPos postx;
        CTransactionRef txPrev;
        if (g_txindex->FindTxPosition(prevout.hash, postx))
        {
            CAutoFile file(OpenBlockFile(postx, true), SER_DISK, CLIENT_VERSION);
            CBlockHeader header;
            try {
                file >> header;
                fseek(file.Get(), postx.nTxOffset, SEEK_CUR);
                file >> txPrev;
            } catch (const std::exception &e) {
                return error("%s() : deserialize or I/O error in GetCoinAge()", __PRETTY_FUNCTION__);
            }
            if (txPrev->GetHash() != prevout.hash)
                return error("%s() : txid mismatch in GetCoinAge()", __PRETTY_FUNCTION__);

            if (header.GetBlockTime() + nSMA > tx.nTime)
                continue; // only count coins meeting min age requirement

            const CAmount nValueIn = txPrev->vout[txin.prevout.n].nValue;
            int nEffectiveAge = tx.nTime - txPrev->nTime;

            bnCentSecond += arith_uint256(nValueIn) * nEffectiveAge / CENT;

            if (fDebug)
                LogPrintf("coin age nValueIn=%-12lld nTimeDiff=%d bnCentSecond=%s\n", nValueIn, nEffectiveAge, bnCentSecond.ToString());
        }
        else {
            return error("%s() : tx missing in tx index in GetCoinAge()", __PRETTY_FUNCTION__);
        }
    }


    arith_uint256 bnCoinDay = bnCentSecond * CENT / COIN / (24 * 60 * 60);
    if (fDebug)
        LogPrintf("coin age bnCoinDay=%s\n", bnCoinDay.ToString());
    nCoinAge = bnCoinDay.GetLow64();
    return true;

}

// miner's coin stake reward based on coin age spent (coin-days)
CAmount GetProofOfStakeReward(int64_t nCoinAge, CAmount nFees)
{
    // CBlockLocator locator;

    int64_t nRewardCoinYear = COIN_YEAR_REWARD; // 10% reward up to end

    CAmount nSubsidy = nCoinAge * nRewardCoinYear * 33 / (365 * 33 + 8);
    LogPrintf("coin-Subsidy %s\n", std::to_string(nSubsidy));
    LogPrintf("coin-Age %s\n", std::to_string(nCoinAge));
    LogPrintf("Coin Reward %s\n", std::to_string(nRewardCoinYear));
    LogPrintf("GetProofOfStakeReward(): create=%s nCoinAge=%s\n", FormatMoney(nSubsidy), std::to_string(nCoinAge));

    return nSubsidy + nFees;
}
