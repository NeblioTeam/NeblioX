// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2019 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <primitives/block.h>

#include <hash.h>
#include <tinyformat.h>
#include "logging.h"
#include <crypto/common.h>
#include <crypto/scrypt.h>
#include "serialize.h"

uint256 CBlockHeader::GetHash() const
{
    uint256 thash;
    scrypt_1024_1_1_256((const char*)&(nVersion), (char*)&(thash));
    return thash;

}

bool CBlock::CBlock::IsProofOfStake() const { return (vtx.size() > 1 && vtx[1]->IsCoinStake()); }

bool CBlock::IsProofOfWork() const { return !IsProofOfStake(); }

std::string CBlock::ToString() const
{
    std::stringstream s;
    s << strprintf("CBlock(hash=%s, ver=0x%08x, hashPrevBlock=%s, hashMerkleRoot=%s, nTime=%u, nBits=%08x, nNonce=%u, vtx=%u)\n",
        GetHash().ToString(),
        nVersion,
        hashPrevBlock.ToString(),
        hashMerkleRoot.ToString(),
        nTime, nBits, nNonce,
        vtx.size());
    for (const auto& tx : vtx) {
        s << "  " << tx->ToString() << "\n";
    }
    return s.str();
}

unsigned int CBlock::GetStakeEntropyBit(const uint256 &hash)
{
    // Take last bit of block hash as entropy bit
    const unsigned int nEntropyBit = ((hash.GetUint64(0)) & UINT64_C(1));
    static const bool fDebug = false;
    if (fDebug) {
        LogPrintf("GetStakeEntropyBit: hashBlock=%s nEntropyBit=%d", hash.ToString(), int(nEntropyBit));
    }
    return nEntropyBit;
}
