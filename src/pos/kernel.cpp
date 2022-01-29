// Copyright (c) 2012-2013 The PPCoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "kernel.h"
#include "primitives/block.h"
#include "txdb.h"
#include "arith_uint256.h"
#include "hash.h"
#include "consensus/validation.h"
#include "validation.h"
#include "index/txindex.h"
#include "index/disktxpos.h"
#include "timedata.h"
#include "chainparams.h"
#include "node/blockstorage.h"
#include <cinttypes>

using namespace std;

// Get time weight
int64_t GetWeight(const CChainState &chain_state, const int currentHeight, int64_t nIntervalBeginning, int64_t nIntervalEnd)
{
    // Kernel hash weight starts from 0 at the min age
    // this change increases active coins participating the hash and helps
    // to secure the network when proof-of-stake difficulty is low
    unsigned int nSMA = chain_state.m_params.GetConsensus().StakeMinAge(currentHeight);
    return min(nIntervalEnd - nIntervalBeginning - nSMA, chain_state.m_params.GetConsensus().StakeMaxAge());
}

// Get the last stake modifier and its generation time from a given block
static bool GetLastStakeModifier(const CBlockIndex* pindex, uint64_t& nStakeModifier,
                                 int64_t& nModifierTime)
{
    if (!pindex)
        return error("GetLastStakeModifier: null pindex");
    while (pindex && pindex->pprev && !pindex->GeneratedStakeModifier())
        pindex = pindex->pprev;
    if (!pindex->GeneratedStakeModifier())
        return error("GetLastStakeModifier: no generation at genesis block");
    nStakeModifier = pindex->nStakeModifier;
    nModifierTime  = pindex->GetBlockTime();
    return true;
}

// Get selection interval section (in seconds)
static int64_t GetStakeModifierSelectionIntervalSection(const CChainState &chain_state, int nSection)
{
    assert(nSection >= 0 && nSection < 64);

    const auto& cons = chain_state.m_params.GetConsensus();

    return (cons.StakeModifierInterval() * 63 / (63 + ((63 - nSection) * (MODIFIER_INTERVAL_RATIO - 1))));
}

// Get stake modifier selection interval (in seconds)
static int64_t GetStakeModifierSelectionInterval(const CChainState &chain_state)
{
    int64_t nSelectionInterval = 0;
    for (int nSection = 0; nSection < 64; nSection++)
        nSelectionInterval += GetStakeModifierSelectionIntervalSection(chain_state, nSection);

    return nSelectionInterval;
}

//// select a block from the candidate blocks in vSortedByTimestamp, excluding
//// already selected blocks in vSelectedBlocks, and with timestamp up to
//// nSelectionIntervalStop.
//static bool SelectBlockFromCandidates(CChainState &chain_state, BlockValidationState &/*state*/,
//                                      vector<pair<int64_t, uint256>>& vSortedByTimestamp,
//                                      map<uint256, const CBlockIndex*>& mapSelectedBlocks,
//                                      int64_t nSelectionIntervalStop, uint64_t nStakeModifierPrev,
//                                      const CBlockIndex** pindexSelected)
//{
//    static const bool fDebug = false;

//    bool    fSelected = false;
//    arith_uint256 hashBest{};
//    *pindexSelected   = (const CBlockIndex*)0;
//    for (const std::pair<int64_t, uint256>& item: vSortedByTimestamp) {
//        const CBlockIndex* pindex = chain_state.m_blockman.LookupBlockIndex(item.second);
//        if (!pindex)
//            return error("SelectBlockFromCandidates: failed to find block index for candidate block %s",
//                         item.second.ToString().c_str());
//        if (fSelected && pindex->GetBlockTime() > nSelectionIntervalStop)
//            break;
//        if (mapSelectedBlocks.count(pindex->GetBlockHash()) > 0)
//            continue;
//        // compute the selection hash by hashing its proof-hash and the
//        // previous proof-of-stake modifier
//        CDataStream ss(SER_GETHASH, 0);
//        ss << pindex->hashProofOfStake << nStakeModifierPrev;
//        arith_uint256 hashSelection = UintToArith256(Hash(ss.str()));
//        // the selection hash is divided by 2**32 so that proof-of-stake block
//        // is always favored over proof-of-work block. this is to preserve
//        // the energy efficiency property
//        if (pindex->IsProofOfStake())
//            hashSelection >>= 32;
//        if (fSelected && hashSelection < hashBest) {
//            hashBest        = hashSelection;
//            *pindexSelected = (const CBlockIndex*)pindex;
//        } else if (!fSelected) {
//            fSelected       = true;
//            hashBest        = hashSelection;
//            *pindexSelected = (const CBlockIndex*)pindex;
//        }
//    }
//    if (fDebug)
//        printf("SelectBlockFromCandidates: selection hash=%s\n", hashBest.ToString().c_str());
//    return fSelected;
//}

// select a block from the candidate blocks in vSortedByTimestamp, excluding
// already selected blocks in vSelectedBlocks, and with timestamp up to
// nSelectionIntervalStop.
static bool SelectBlockFromCandidates(
    CChainState &chain_state, BlockValidationState &/*state*/,
    vector<pair<int64_t, uint256> >& vSortedByTimestamp,
    map<uint256, const CBlockIndex*>& mapSelectedBlocks,
    int64_t nSelectionIntervalStop, uint64_t nStakeModifierPrev,
    const CBlockIndex** pindexSelected)
{
    bool fSelected = false;
    arith_uint256 hashBest = 0;
    *pindexSelected = (const CBlockIndex*) 0;
    for (const auto& item : vSortedByTimestamp)
    {
        const CBlockIndex* pindex = chain_state.m_blockman.LookupBlockIndex(item.second);
        if (!pindex)
            return error("SelectBlockFromCandidates: failed to find block index for candidate block %s",
                         item.second.ToString().c_str());
        if (fSelected && pindex->GetBlockTime() > nSelectionIntervalStop)
            break;
        if (mapSelectedBlocks.count(pindex->GetBlockHash()) > 0)
            continue;
        // compute the selection hash by hashing its proof-hash and the
        // previous proof-of-stake modifier
        uint256 hashProof = pindex->IsProofOfStake() ? pindex->hashProofOfStake : pindex->GetBlockHash();
        CDataStream ss(SER_GETHASH, 0);
        ss << hashProof << nStakeModifierPrev;
        arith_uint256 hashSelection = UintToArith256(Hash(ss.str()));
        // the selection hash is divided by 2**32 so that proof-of-stake block
        // is always favored over proof-of-work block. this is to preserve
        // the energy efficiency property
        if (pindex->IsProofOfStake())
            hashSelection >>= 32;
        if (fSelected && hashSelection < hashBest)
        {
            hashBest = hashSelection;
            *pindexSelected = (const CBlockIndex*) pindex;
        }
        else if (!fSelected)
        {
            fSelected = true;
            hashBest = hashSelection;
            *pindexSelected = (const CBlockIndex*) pindex;
        }
    }
    if (gArgs.GetBoolArg("-debug", false) && gArgs.GetBoolArg("-printstakemodifier", false))
        LogPrintf("SelectBlockFromCandidates: selection hash=%s\n", hashBest.ToString());
    return fSelected;
}

// Stake Modifier (hash modifier of proof-of-stake):
// The purpose of stake modifier is to prevent a txout (coin) owner from
// computing future proof-of-stake generated by this txout at the time
// of transaction confirmation. To meet kernel protocol, the txout
// must hash with a future stake modifier to generate the proof.
// Stake modifier consists of bits each of which is contributed from a
// selected block of a given block group in the past.
// The selection of a block is based on a hash of the block's proof-hash and
// the previous stake modifier.
// Stake modifier is recomputed at a fixed time interval instead of every
// block. This is to make it difficult for an attacker to gain control of
// additional bits in the stake modifier, even after generating a chain of
// blocks.
bool ComputeNextStakeModifier(CChainState &chain_state, BlockValidationState &state,
                              const CBlockIndex* pindexCurrent, uint64_t& nStakeModifier,
                              bool& fGeneratedStakeModifier)
{
    static const bool fDebug = false;
    const CBlockIndex* pindexPrev = pindexCurrent->pprev;
    nStakeModifier          = 0;
    fGeneratedStakeModifier = false;
    if (!pindexPrev) {
        fGeneratedStakeModifier = true;
        return true; // genesis block's modifier is 0
    }

    const int currentHeight = pindexPrev->nHeight + 1;

    // First find current stake modifier and its generation block time
    // if it's not old enough, return the same stake modifier
    int64_t nModifierTime = 0;
    if (!GetLastStakeModifier(pindexPrev, nStakeModifier, nModifierTime))
        return error("ComputeNextStakeModifier: unable to get last modifier");
    if (fDebug) {
        printf("ComputeNextStakeModifier: prev modifier=0x%016" PRIx64 " time=%s\n", nStakeModifier,
               FormatISO8601DateTime(nModifierTime).c_str());
    }

    const auto& cons = chain_state.m_params.GetConsensus();

    if (nModifierTime / cons.StakeModifierInterval() >= pindexPrev->GetBlockTime() / cons.StakeModifierInterval())
        return true;

    // Sort candidate blocks by timestamp
    vector<pair<int64_t, uint256>> vSortedByTimestamp;
    unsigned int                   nTS = cons.TargetSpacing(currentHeight);
    vSortedByTimestamp.reserve(64 * cons.StakeModifierInterval() / nTS);
    int64_t nSelectionInterval = GetStakeModifierSelectionInterval(chain_state);
    int64_t nSelectionIntervalStart =
        (pindexPrev->GetBlockTime() / cons.StakeModifierInterval()) * cons.StakeModifierInterval() - nSelectionInterval;
    const CBlockIndex* pindex = pindexPrev;
    while (pindex && pindex->GetBlockTime() >= nSelectionIntervalStart) {
        vSortedByTimestamp.push_back(make_pair(pindex->GetBlockTime(), pindex->GetBlockHash()));
        pindex = pindex->pprev;
    }
    int nHeightFirstCandidate = pindex ? (pindex->nHeight + 1) : 0;
    reverse(vSortedByTimestamp.begin(), vSortedByTimestamp.end());
    sort(vSortedByTimestamp.begin(), vSortedByTimestamp.end(), [] (const pair<int64_t, uint256> &a, const pair<int64_t, uint256> &b)
         {
             if (a.first != b.first)
                 return a.first < b.first;
             return UintToArith256(a.second) < UintToArith256(b.second);
         });

    // Select 64 blocks from candidate blocks to generate stake modifier
    uint64_t                         nStakeModifierNew      = 0;
    int64_t                          nSelectionIntervalStop = nSelectionIntervalStart;
    map<uint256, const CBlockIndex*> mapSelectedBlocks;
    for (int nRound = 0; nRound < min(64, (int)vSortedByTimestamp.size()); nRound++) {
        // add an interval section to the current selection round
        nSelectionIntervalStop += GetStakeModifierSelectionIntervalSection(chain_state, nRound);
        // select a block from the candidates of current round
        if (!SelectBlockFromCandidates(chain_state, state, vSortedByTimestamp, mapSelectedBlocks, nSelectionIntervalStop,
                                       nStakeModifier, &pindex))
            return error("ComputeNextStakeModifier: unable to select block at round %d", nRound);
        // write the entropy bit of the selected block
        nStakeModifierNew |= (((uint64_t)pindex->GetStakeEntropyBit()) << nRound);
        // add the selected block from candidates to selected list
        mapSelectedBlocks.insert(make_pair(pindex->GetBlockHash(), pindex));
        if (fDebug) {
            printf("ComputeNextStakeModifier: selected round %d stop=%s height=%d bit=%d\n", nRound,
                   FormatISO8601DateTime(nSelectionIntervalStop).c_str(), pindex->nHeight,
                   pindex->GetStakeEntropyBit());
        }
    }

    // Print selection map for visualization of the selected blocks
    if (fDebug) {
        string strSelectionMap = "";
        // '-' indicates proof-of-work blocks not selected
        strSelectionMap.insert(0, pindexPrev->nHeight - nHeightFirstCandidate + 1, '-');
        pindex = pindexPrev;
        while (pindex && pindex->nHeight >= nHeightFirstCandidate) {
            // '=' indicates proof-of-stake blocks not selected
            if (pindex->IsProofOfStake())
                strSelectionMap.replace(pindex->nHeight - nHeightFirstCandidate, 1, "=");
            pindex = pindex->pprev;
        }
        for (const std::pair<const uint256, const CBlockIndex*>& item: mapSelectedBlocks) {
            // 'S' indicates selected proof-of-stake blocks
            // 'W' indicates selected proof-of-work blocks
            strSelectionMap.replace(item.second->nHeight - nHeightFirstCandidate, 1,
                                    item.second->IsProofOfStake() ? "S" : "W");
        }
        printf("ComputeNextStakeModifier: selection height [%d, %d] map %s\n", nHeightFirstCandidate,
               pindexPrev->nHeight, strSelectionMap.c_str());
    }
    if (fDebug) {
        printf("ComputeNextStakeModifier: new modifier=0x%016" PRIx64 " time=%s\n", nStakeModifierNew,
               FormatISO8601DateTime(pindexPrev->GetBlockTime()).c_str());
    }

    nStakeModifier          = nStakeModifierNew;
    fGeneratedStakeModifier = true;
    return true;
}

// The stake modifier used to hash for a stake kernel is chosen as the stake
// modifier about a selection interval later than the coin generating the kernel
static bool GetKernelStakeModifier(CChainState &chain_state, BlockValidationState &/*state*/,
                                   const CBlockIndex* pindexPrev,
                                   const CBlockIndex& kernelBlockIndex, uint64_t& nStakeModifier,
                                   int& nStakeModifierHeight, int64_t& nStakeModifierTime,
                                   bool fPrintProofOfStake)
{
    int currentHeight = pindexPrev->nHeight + 1;
    nStakeModifier = 0;
    const uint256 hashBlockFrom                        = kernelBlockIndex.GetBlockHash();
    const CBlockIndex* pindexFrom                      = &kernelBlockIndex;
    nStakeModifierHeight                               = pindexFrom->nHeight;
    nStakeModifierTime                                 = pindexFrom->GetBlockTime();
    int64_t            nStakeModifierSelectionInterval = GetStakeModifierSelectionInterval(chain_state);
    unsigned int       nSMA                            = chain_state.m_params.GetConsensus().StakeMinAge(currentHeight);
    const CBlockIndex* pindex                          = pindexFrom;
    // loop to find the stake modifier later by a selection interval
    while (nStakeModifierTime < pindexFrom->GetBlockTime() + nStakeModifierSelectionInterval) {
        if (!chain_state.m_chain.Next(pindex)) { // reached best block; may happen if node is behind on block chain
            if (fPrintProofOfStake ||
                (pindex->GetBlockTime() + nSMA - nStakeModifierSelectionInterval > GetAdjustedTime()))
                return error(
                    "GetKernelStakeModifier() : reached best block %s at height %d from block %s",
                    pindex->GetBlockHash().ToString().c_str(), pindex->nHeight,
                    hashBlockFrom.ToString().c_str());
            else
                return false;
        }
        pindex = chain_state.m_chain.Next(pindex);
        if (pindex->GeneratedStakeModifier()) {
            nStakeModifierHeight = pindex->nHeight;
            nStakeModifierTime   = pindex->GetBlockTime();
        }
    }
    nStakeModifier = pindex->nStakeModifier;
    return true;
}

// ppcoin kernel protocol
// coinstake must meet hash target according to the protocol:
// kernel (input 0) must meet the formula
//     hash(nStakeModifier + txPrev.block.nTime + txPrev.offset + txPrev.nTime + txPrev.vout.n + nTime) <
//     bnTarget * nCoinDayWeight
// this ensures that the chance of getting a coinstake is proportional to the
// amount of coin age one owns.
// The reason this hash is chosen is the following:
//   nStakeModifier: scrambles computation to make it very difficult to precompute
//                  future proof-of-stake at the time of the coin's confirmation
//   txPrev.block.nTime: prevent nodes from guessing a good timestamp to
//                       generate transaction for future advantage
//   txPrev.offset: offset of txPrev inside block, to reduce the chance of
//                  nodes generating coinstake at the same time
//   txPrev.nTime: reduce the chance of nodes generating coinstake at the same
//                 time
//   txPrev.vout.n: output number of txPrev, to reduce the chance of nodes
//                  generating coinstake at the same time
//   block/tx hash should not be used here as they can be generated in vast
//   quantities so as to generate blocks faster, degrading the system back into
//   a proof-of-work situation.
//
bool CheckStakeKernelHash(CChainState &chain_state, BlockValidationState &state, const CBlockIndex *pindexPrev, unsigned int nBits,
                          const CBlockIndex& blockIndexKernel, unsigned int nTxPrevOffset,
                          const CTransaction& txKernel, const COutPoint& prevout, unsigned int nTimeTx,
                          uint256& hashProofOfStake, arith_uint256& targetProofOfStake,
                          bool fPrintProofOfStake)
{
    if (nTimeTx < txKernel.nTime) // Transaction timestamp violation
        return error("CheckStakeKernelHash() : nTime violation");

    const int currentHeight = pindexPrev->nHeight + 1;

    unsigned int nTimeBlockFrom = blockIndexKernel.GetBlockTime();
    unsigned int nSMA           = chain_state.m_params.GetConsensus().StakeMinAge(currentHeight);
    if (nTimeBlockFrom + nSMA > nTimeTx) // Min age requirement
        return error("CheckStakeKernelHash() : min age violation");

    arith_uint256 bnTargetPerCoinDay;
    bnTargetPerCoinDay.SetCompact(nBits);
    CAmount nValueIn = txKernel.vout[prevout.n].nValue;

    arith_uint256 bnCoinDayWeight =
        arith_uint256(nValueIn) * GetWeight(chain_state, currentHeight, (int64_t)txKernel.nTime, (int64_t)nTimeTx) / COIN / (24 * 60 * 60);

    targetProofOfStake = bnCoinDayWeight * bnTargetPerCoinDay;

    // Calculate hash
    CDataStream ss(SER_GETHASH, 0);
    uint64_t    nStakeModifier       = 0;
    int         nStakeModifierHeight = 0;
    int64_t     nStakeModifierTime   = 0;

    if (!GetKernelStakeModifier(chain_state, state, pindexPrev, blockIndexKernel, nStakeModifier, nStakeModifierHeight, nStakeModifierTime,
                                fPrintProofOfStake))
        return false;
    ss << nStakeModifier << nTimeBlockFrom << nTxPrevOffset << txKernel.nTime << prevout.n << nTimeTx;
    hashProofOfStake = Hash(ss.str());
    if (fPrintProofOfStake) {
        LogPrintf("CheckStakeKernelHash() : using modifier %s"
               " at height=%d timestamp=%s for block from height=%d timestamp=%s\n",
               arith_uint256(nStakeModifier).ToString(), nStakeModifierHeight,
               FormatISO8601DateTime(nStakeModifierTime).c_str(),
               blockIndexKernel.nHeight,
               FormatISO8601DateTime(blockIndexKernel.GetBlockTime()).c_str());
        LogPrintf(
            "CheckStakeKernelHash() : check modifier=%s"
            " nTimeBlockFrom=%u nTxPrevOffset=%u nTimeTxPrev=%u nPrevout=%u nTimeTx=%u hashProof=%s\n",
            arith_uint256(nStakeModifier).ToString(), nTimeBlockFrom, nTxPrevOffset, txKernel.nTime, prevout.n, nTimeTx,
            hashProofOfStake.ToString().c_str());
    }

    static const arith_uint256 MAX_UINT256 = ~arith_uint256(0);

    // Now check if proof-of-stake hash meets target protocol
    /* Previously Neblio used CBigNum for this calculation, which creates at certain
     * blocks that have the below product of size larger than 256-bit,
     * which would lead to the below check to fail.
     * However, this does not mean the product is smaller.
     * Since hashProofOfStake is 256-bit, an overflow of the product will mean
     * that the target is correct. No need to use bigger types.
     */
    const bool correctTarget = UintToArith256(hashProofOfStake) <= bnCoinDayWeight * bnTargetPerCoinDay;
    const bool wouldOverflow = bnTargetPerCoinDay == 0 ? false : bnCoinDayWeight > MAX_UINT256 / bnTargetPerCoinDay;
    if (!correctTarget && !wouldOverflow) {
        return false;
    }

    if (fPrintProofOfStake) {
        printf("CheckStakeKernelHash() : using modifier 0x%016" PRIx64
               " at height=%d timestamp=%s for block from height=%d timestamp=%s\n",
               nStakeModifier, nStakeModifierHeight, FormatISO8601DateTime(nStakeModifierTime).c_str(),
               blockIndexKernel.nHeight,
               FormatISO8601DateTime(blockIndexKernel.GetBlockTime()).c_str());
        printf(
            "CheckStakeKernelHash() : pass modifier=0x%016" PRIx64
            " nTimeBlockFrom=%u nTxPrevOffset=%u nTimeTxPrev=%u nPrevout=%u nTimeTx=%u hashProof=%s\n",
            nStakeModifier, nTimeBlockFrom, nTxPrevOffset, txKernel.nTime, prevout.n, nTimeTx,
            hashProofOfStake.ToString().c_str());
    }
    return true;
}

// Check kernel hash target and coinstake signature
bool CheckProofOfStake(CChainState &chain_state, BlockValidationState &state, const CBlockIndex *pindexPrev,
                       const CTransaction& tx, unsigned int nBits, uint256& hashProofOfStake,
                       arith_uint256& targetProofOfStake)
{
    if (!tx.IsCoinStake()
        || tx.vin.size() < 1) {
        LogPrintf("ERROR: %s: malformed-txn %s\n", __func__, tx.GetHash().ToString());
        return state.Invalid(BlockValidationResult::DOS_100, "malformed-txn");
    }


    // Transaction index is required to get tx position in block
    if (!g_txindex)
        return error("CheckProofOfStake() : transaction index not available");

    // Kernel (input 0) must match the stake hash target per coin age (nBits)
    const CTxIn& txin = tx.vin[0];

    // TODO(Sam): Kernels (stake inputs) are accepted here even if they're spent;
    //            this is not correct, but is probably OK because an error will occur
    //            in connect block indicating that the input is double-spent; this must be checked
    // TODO(Sam): given the discussion from above, the right way to do this is to check the history
    //            of the current block, and not the CoinsTip as done below. Same mechanism with VIU
    //            Perhaps VIU is good enough. All this must be checked.

    // Get transaction index for the previous transaction
    CDiskTxPos postx;
    if (!g_txindex->FindTxPosition(txin.prevout.hash, postx))
        return error("CheckProofOfStake() : tx index not found");  // tx index not found

    // Read txPrev and header of its block
    CBlockHeader header;
    CTransactionRef txPrev;
    {
        CAutoFile file(OpenBlockFile(postx, true), SER_DISK, CLIENT_VERSION);
        try {
            file >> header;
            fseek(file.Get(), postx.nTxOffset, SEEK_CUR);
            file >> txPrev;
        } catch (std::exception &e) {
            return error("%s() : deserialize or I/O error in CheckProofOfStake()", __PRETTY_FUNCTION__);
        }
        if (txPrev->GetHash() != txin.prevout.hash)
            return error("%s() : txid mismatch in CheckProofOfStake()", __PRETTY_FUNCTION__);
    }

    //    Coin coin;
    //    if (!chain_state.CoinsTip().GetCoin(txin.prevout, coin)) {
    //        LogPrintf("CheckProofOfStake() : INFO: read txPrev failed for prevout: %s", txin.prevout.ToString());
    //        return state.Invalid(BlockValidationResult::DOS_1, "prevout-not-found");
    //            // previous
    //            // transaction not
    //            // in main chain,
    //            // may occur during
    //            // initial download
    //    }

    auto pindexKernelIt = chain_state.m_blockman.m_block_index.find(header.GetHash());
    if(pindexKernelIt == chain_state.m_blockman.m_block_index.cend()) {
        LogPrintf("ERROR: %s: invalid-prevout\n", __func__);
        return state.Invalid(BlockValidationResult::DOS_100, "invalid-prevout");
    }
    const CBlockIndex* pindexKernel = pindexKernelIt->second;

//    const CBlockIndex *pindexKernel = chain_state.m_chain[coin.nHeight];
//    if (!pindexKernel) {
//        LogPrintf("ERROR: %s: invalid-prevout\n", __func__);
//        return state.Invalid(BlockValidationResult::DOS_100, "invalid-prevout");
//    }

    const CTxOut& prevOut = txPrev->vout[tx.vin[0].prevout.n];
    const CScript kernelPubKey = prevOut.scriptPubKey;
    const CAmount amount = prevOut.nValue;

    const CScript &scriptSig = txin.scriptSig;
    const CScriptWitness *witness = &txin.scriptWitness;
    ScriptError serror = SCRIPT_ERR_OK;
    // Verify signature
    const TransactionSignatureChecker checker(&tx, 0, amount, PrecomputedTransactionData(tx), MissingDataBehavior::FAIL);
    if (!VerifyScript(scriptSig, kernelPubKey, witness, 0, checker, &serror)) {
        LogPrintf("ERROR: %s: verify-script-failed, txn %s, reason %s\n", __func__, tx.GetHash().ToString(), ScriptErrorString(serror));
        return state.Invalid(BlockValidationResult::DOS_100, "verify-cs-script-failed");
    }


//    CTransactionRef txPrev;
//    uint256 pindexKernelBlockHash;
//    if(pindexKernelBlockHash != pindexKernel->GetBlockHash()) {
//        LogPrintf("Unexpected error: The coin_state and the txindex return different block hashes for prevout: %s; (in order: %s vs %s)", txin.prevout.ToString(), pindexKernel->GetBlockHash().ToString(), pindexKernelBlockHash.ToString());
//        return state.Invalid(BlockValidationResult::DOS_1, "prevout-kernel-not-found1");
//    }

//    if(!g_txindex->FindTx(txin.prevout.hash, pindexKernelBlockHash, txPrev)) {
//        return state.Invalid(BlockValidationResult::DOS_1, "prevout-kernel-not-found2");
//    }

    if(!txPrev) {
        return state.Invalid(BlockValidationResult::DOS_1, "prevout-kernel-not-found3");
    }

    static const bool fDebug = false;

    if (!CheckStakeKernelHash(chain_state, state, pindexPrev, nBits, *pindexKernel,
                              postx.nTxOffset + CBlockHeader::NORMAL_SERIALIZE_SIZE,
                              *txPrev, txin.prevout, tx.nTime,
                              hashProofOfStake, targetProofOfStake, fDebug))
    {
        // may occur during initial download or if behind on block chain sync
        LogPrintf("CheckProofOfStake() : INFO: check kernel failed on coinstake %s, hashProof=%s",
                 tx.GetHash().ToString(),
                 hashProofOfStake.ToString());
        return state.Invalid(BlockValidationResult::DOS_1, "prevout-not-found");
    }

    return true;
}


// Check whether the coinstake timestamp meets protocol
bool CheckCoinStakeTimestamp(int64_t nTimeBlock, int64_t nTimeTx)
{
    // v0.3 protocol
    return (nTimeBlock == nTimeTx);
}

// Get stake modifier checksum
uint32_t GetStakeModifierChecksum(CChainState &chain_state, const CBlockIndex* pindex)
{
    assert(pindex->pprev || pindex->GetBlockHash() == chain_state.m_params.GetConsensus().hashGenesisBlock);
    // Hash previous checksum with flags, hashProofOfStake and nStakeModifier
    CDataStream ss(SER_GETHASH, 0);
    if (auto prev = pindex->pprev)
        ss << prev->nStakeModifierChecksum;
    ss << pindex->nFlags << ArithToUint256(pindex->IsProofOfStake() ? UintToArith256(pindex->hashProofOfStake) : 0) << pindex->nStakeModifier;
    arith_uint256 hashChecksum = UintToArith256(Hash(ss.str()));
    hashChecksum >>= (256 - 32);
    return hashChecksum.GetLow64();
}

// Check stake modifier hard checkpoints
bool CheckStakeModifierCheckpoints(CChainState &chain_state, int nHeight, unsigned int nStakeModifierChecksum)
{
    const Consensus::Params::MapStakeModifierCheckpoints& checkpoints = chain_state.m_params.GetConsensus().StakeModifierCheckpoints();

    auto it = checkpoints.find(nHeight);
    if (it != checkpoints.cend())
        return nStakeModifierChecksum == it->second;
    return true;
}