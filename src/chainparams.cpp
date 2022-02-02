// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2020 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <chainparams.h>

#include <arith_uint256.h>
#include <chainparamsseeds.h>
#include <consensus/merkle.h>
#include <deploymentinfo.h>
#include <hash.h> // for signet block challenge hash
#include <util/system.h>

#include <assert.h>

#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>

static CBlock CreateGenesisBlock(const char* pszTimestamp, const CScript& genesisOutputScript, uint32_t nTime, uint32_t nNonce, uint32_t nBits, int32_t nVersion, const CAmount& genesisReward)
{
    CMutableTransaction txNew;
    txNew.nVersion = 1;
    txNew.vin.resize(1);
    txNew.vout.resize(1);
    txNew.vin[0].scriptSig = CScript() << 0 << CScriptNum(42)
                                       << std::vector<unsigned char>((const unsigned char*)pszTimestamp,
                                                                     (const unsigned char*)pszTimestamp +
                                                                         strlen(pszTimestamp));
    txNew.vout[0].nValue       = genesisReward;
    txNew.vout[0].scriptPubKey = genesisOutputScript;
    txNew.nTime = nTime;

    CBlock genesis;
    genesis.nTime    = nTime;
    genesis.nBits    = nBits;
    genesis.nNonce   = nNonce;
    genesis.nVersion = nVersion;
    genesis.vtx.push_back(MakeTransactionRef(std::move(txNew)));
    genesis.hashPrevBlock.SetNull();
    genesis.hashMerkleRoot = BlockMerkleRoot(genesis);
    return genesis;
}

/**
 * Build the genesis block. Note that the output of its generation
 * transaction cannot be spent since it did not originally exist in the
 * database.
 *
 * CBlock(hash=000000000019d6, ver=1, hashPrevBlock=00000000000000, hashMerkleRoot=4a5e1e, nTime=1231006505, nBits=1d00ffff, nNonce=2083236893, vtx=1)
 *   CTransaction(hash=4a5e1e, ver=1, vin.size=1, vout.size=1, nLockTime=0)
 *     CTxIn(COutPoint(000000, -1), coinbase 04ffff001d0104455468652054696d65732030332f4a616e2f32303039204368616e63656c6c6f72206f6e206272696e6b206f66207365636f6e64206261696c6f757420666f722062616e6b73)
 *     CTxOut(nValue=50.00000000, scriptPubKey=0x5F1DF16B2B704C8A578D0B)
 *   vMerkleTree: 4a5e1e
 */
static CBlock CreateGenesisBlock(uint32_t nTime, uint32_t nNonce, uint32_t nBits, int32_t nVersion, const CAmount& genesisReward)
{
    const char*   pszTimestamp        = "21jul2017 - Neblio First Net Launches";
    const CScript genesisOutputScript = CScript();

    return CreateGenesisBlock(pszTimestamp, genesisOutputScript, nTime, nNonce, nBits, nVersion, genesisReward);
}

/**
 * Main network on which people trade goods and services.
 */
class CMainParams : public CChainParams {
public:
    CMainParams() {
        strNetworkID = CBaseChainParams::MAIN;
        consensus.signet_blocks = false;
        consensus.signet_challenge.clear();
        consensus.nSubsidyHalvingInterval = 210000;
        consensus.BIP16Exception = uint256S("0x00000000000002dc756eebf4f49723ed8d30cc28a5f108eb94b1ba88ac4f9c22");
        consensus.BIP34Height = 1;
        consensus.BIP65Height = 1; // 000000000000000004c2b624ed5d7756c508d90fd0da2c7c679febfa6c4735f0
        // TODO(Sam): check if we can remove this
        consensus.BIP66Height = 363725; // 00000000000000000379eaa19dce8c9b722d46ae6a57c2f1a988119488b50931
        consensus.CSVHeight = 419328; // 000000000000000004a1b34462cb8aeebd5799177f7a29cf28f2d1961716b5b5
        // TODO(Sam): check when to enable this
        consensus.SegwitHeight = 40000000; // 0000000000000000001c8018d9cb3b742ef25114f27563e3fc4a1902167f9893
        // TODO(Sam): check when to enable this or remove it
        consensus.MinBIP9WarningHeight = 40000000; // segwit activation height + miner confirmation window
        consensus.powLimit = ArithToUint256(~arith_uint256(0) >> 1);
        consensus.posLimit = ArithToUint256(~arith_uint256(0) >> 20);
        consensus.nTargetTimespan = 2 * 60 * 60; // two hours
        consensus.nLastPoWBlock = 1000;
        // number of stake confirmations changed to 10
        consensus.nFork2ConfsChangedHeight = 248000;
        // Tachyon upgrade. Approx Jan 12th 2019
        consensus.nFork3TachyonHeight = 387028;
        // Retarget correction
        consensus.nFork4RetargetCorrectHeight = 1003125;
        // Enable cold-staking
        consensus.nFork5ColdStaking = 2730450;

        consensus.nStakeMinAgeV1        = 24 * 60 * 60;             // minimum age for coin age
        consensus.nStakeMinAgeV2        = consensus.nStakeMinAgeV1; // minimum age for coin age
        consensus.nStakeMaxAge          = 7 * 24 * 60 * 60;         // Maximum stake age 7 days
        consensus.nModifierInterval     = 10 * 60; // time to elapse before new modifier is computed

        consensus.nCoinbaseMaturityV1 = 30;
        consensus.nCoinbaseMaturityV2 = 10;
        consensus.nCoinbaseMaturityV3 = 120;

        consensus.nStakeTargetSpacingV1 = 2 * 60;
        consensus.nStakeTargetSpacingV2 = 30;

        // TODO(Sam): consensus.nMaxOpReturnSizeV1 = 80;
        // TODO(Sam): consensus.nMaxOpReturnSizeV2 = 4096;

        consensus.fPowAllowMinDifficultyBlocks = false;
        consensus.fPowNoRetargeting = false;
        consensus.nRuleChangeActivationThreshold = 1815; // 90% of 2016
        consensus.nMinerConfirmationWindow = 2016; // nPowTargetTimespan / nPowTargetSpacing

        // TODO(Sam): Work on deployment stuff
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].bit = 28;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime = Consensus::BIP9Deployment::NEVER_ACTIVE;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].min_activation_height = 0; // No activation delay

        // Deployment of Taproot (BIPs 340-342)
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].bit = 2;
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].nStartTime = 1619222400; // April 24th, 2021
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].nTimeout = 1628640000; // August 11th, 2021
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].min_activation_height = 709632; // Approximately November 12th, 2021

        consensus.nMinimumChainWork = uint256S("0x0000000000000000000000000000000000000000000000000005af5c4ee34581"); // 3725179
        consensus.defaultAssumeValid = uint256S("0x00000000000000000008a89e854d57e5667df88f1cdef6fde2fbca1de5b639ad"); // 691719

        /**
         * The message start string is designed to be unlikely to occur in normal data.
         * The characters are rarely used upper ASCII, not valid as UTF-8, and produce
         * a large 32-bit integer with any alignment.
         */
        pchMessageStart[0] = 0x32;
        pchMessageStart[1] = 0x5e;
        pchMessageStart[2] = 0x6f;
        pchMessageStart[3] = 0x86;
        nDefaultPort = 6325;
        nPruneAfterHeight = 100000;
        m_assumed_blockchain_size = 420;
        m_assumed_chain_state_size = 6;

        genesis = CreateGenesisBlock(1500674579, 8485, arith_uint256(consensus.powLimit.GetHex()).GetCompact(), 1, 0);
        consensus.hashGenesisBlock = genesis.GetHash();
        assert(consensus.hashGenesisBlock == uint256S("0x7286972be4dbc1463d256049b7471c252e6557e222cab9be73181d359cd28bcc"));
        assert(genesis.hashMerkleRoot == uint256S("0x203fd13214321a12b01c0d8b32c780977cf52e56ae35b7383cd389c73291aee7"));

        // Note that of those which support the service bits prefix, most only support a subset of
        // possible options.
        // This is fine at runtime as we'll fall back to using them as an addrfetch if they don't support the
        // service bits we want, but we should get them updated to support all service bits wanted by any
        // release ASAP to avoid it where possible.
        vSeeds.clear(); // TODO(Sam)
//        vSeeds.emplace_back("seed.nebl.io");
//        vSeeds.emplace_back("seed2.nebl.io");

        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,53);  // Neblio: addresses begin with 'N'
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,112); // Neblio: addresses begin with 'n'
        base58Prefixes[SECRET_KEY] =     std::vector<unsigned char>(1,128+53);
        base58Prefixes[EXT_PUBLIC_KEY] = {0x05, 0x89, 0xB4, 0x2E}; // TODO(Sam): choose a good value
        base58Prefixes[EXT_SECRET_KEY] = {0x05, 0x89, 0xAE, 0x54}; // TODO(Sam): choose a good value

        bech32_hrp = "nb";

        vFixedSeeds = std::vector<uint8_t>(std::begin(chainparams_seed_main), std::end(chainparams_seed_main));

        fDefaultConsistencyChecks = false;
        fRequireStandard = true;
        m_is_test_chain = false;
        m_is_mockable_chain = false;

        checkpointData = {
            {
             {0,       genesis.GetHash()},
             {500,     uint256S("0x00000342c5dc5f7fd4a8ef041d4df4e569bd40756405a8c336c5f42c77e097a2")},
             {1000,    uint256S("0x00000c60e3a8d27dedb15fc33d91caec5cf714fae60f24ea22a649cded8e0cca")},
             {5000,    uint256S("0x074873095a26296d4f0033f697f46bddb7c1359ffcb3461f620e346bc516a1d2")},
             {25000,   uint256S("0x9c28e51c9c21092909fe0a6ad98ae335f253fa9c8076bb3cca154b6ba5ee03ab")},
             {100000,  uint256S("0xbb13aedc5846fe5d384601ef4648492262718fc7dfe35b886ef297ea74cab8cc")},
             {150000,  uint256S("0x9a755758cc9a8d40fc36e6cc312077c8dd5b32b2c771241286099fd54fd22db0")},
             {200000,  uint256S("0xacea764bbb689e940040b229a89213e17b50b98db0514e1428acedede9c1a4c0")},
             {250000,  uint256S("0x297eda3c18c160bdb2b1465164b11ba2ee7908b209a26d3b76eac3876aa55072")},
             {260000,  uint256S("0x4d407875afd318897266c14153d856774868949c65176de9214778d5626707a0")},
             {270000,  uint256S("0x7f8ead004a853b411de63a3f30ee5a0e4c144a11dbbc00c96942eb58ff3b9a48")},
             {280000,  uint256S("0x954544adaa689ad91627822b9da976ad6f272ced95a272b41b108aabff30a3e5")},
             {285000,  uint256S("0x7c37fbdb5129db54860e57fd565f0a17b40fb8b9d070bda7368d196f63034ae5")},
             {287500,  uint256S("0x3da2de78a53afaf9dafc8cec20a7ace84c52cff994307aef4072d3d0392fe041")},
             {290000,  uint256S("0x5685d1cc15100fa0c7423b7427b9f0f22653ccd137854f3ecc6230b0d1af9ebc")},
             {295000,  uint256S("0x581aef5415de9ce8b2817bf803cf29150bd589a242c4cb97a6fd931d6f165190")},
             {300000,  uint256S("0xb2d6ef8b3ec931c48c2d42fa574a382a534014388b17eb8e0eca1a0db379e369")},
             {305000,  uint256S("0x9332baa2c500cb938024d2ec35b265bfa2928b63ae5d2d9d81ffd8cbfd75ef1d")},
             {310000,  uint256S("0x53c993efaf747fadd0ecae8b3a15292549e77223853a8dc90c18aa4664f85b6e")},
             {315000,  uint256S("0xb46b2d2681294d04a366f34eb2b9183621961432c841a155fe723deabcbf9e38")},
             {320000,  uint256S("0x82ecc41d44fefc6667119b0142ba956670bda4e15c035eefe66bfaa4362d2823")},
             {350000,  uint256S("0x7787a1240f1bff02cd3e37cfc8f4635725e26c6db7ff44e8fbee7bf31dc6d929")},
             {360000,  uint256S("0xb4b001753a4d7ec18012a5ff1cbf3f614130adbf6c3f2515d36dfc3300655c2a")},
             {387026,  uint256S("0x37ec421ce623892935d939930d61c066499b8c7eb55606be67219a576d925b67")},
             {387027,  uint256S("0x1a7a41f757451fa32acb0aa31e262398d660e90994b8e17f164dd201718c8f5d")},
             {387028,  uint256S("0xac7d44244ff394255f4c1f99664b26cd015d3d10bddbb8a86727ff848faa6acf")},
             {387029,  uint256S("0x7e4655517659f78cd2e870305e42353ea5bcf9ac1aaa79c1254f9222993c12d5")},
             {387030,  uint256S("0xae375a05ca92fe78e2768352eebb358b12fc0c2c65263d7ac29e4fe723636f81")},
             {390000,  uint256S("0xcd035c9899d22c414f79a345c1b96fd9342d1beb5f80f1dbad6a6244b5d3d5b8")},
             {400000,  uint256S("0x7ae908b0c5351fae59fcff7ab4fe0e23f4e7630ed895822676f3ee551262d82d")},
             {500000,  uint256S("0x92b5c16c99769dcad4c2d4548426037b35894ef57ff1bf2516575440e1f87d4f")},
             {600000,  uint256S("0x69c4acf177368eeb40155e7b03d07b7a6579620320d5de2554db99d0f4908b97")},
             {685000,  uint256S("0xa276d5697372e71f597dca34c40391747186ce3fda96ee1875376b4b0f625881")},
             {700000,  uint256S("0x8b5806c169fb7d3345e9f02ee0a38538cc4ab5884177002c1e9528058c5eab40")},
             {800000,  uint256S("0x71e29af1056d1e8e217382f433d017406db7f0e03eb1995429a9edb741120643")},
             {900000,  uint256S("0x8757e0670d5db26a9b540c616ae1c208bda9f4c3b3270754a36c867aa238206b")},
             {1000000, uint256S("0x0ef9d1ce85a1e8209f735f1574bbe0ed0aaca34f0c6052a65443aada25be94a8")},
             {1003123, uint256S("0xf2ec975040b2a5b1a1bf0c722b685596755e6021680661589aa7f8585d283700")},
             {1003124, uint256S("0xd9d451b69134e2d7682014fb5366bb662b3e753b23722cb34326c09aa1c22762")},
             {1003125, uint256S("0x0faaf5119ab9eb3a22e0984d6cba6cebc8d7bae25342401c782ab4fa413c326e")},
             {1003126, uint256S("0x8f21fc3e383c5ec61dec1f171a0b49eea25dccbb28755214a0d45e73dccb7c56")},
             {1003127, uint256S("0x5aaf45ff165d066f84d55399fda3c4458234f94cf32b0cfdcc7f9bbcc814585d")},
             {1100000, uint256S("0xb726814d624b9a1b77e4edfb43ec4c8c47d5cfe4a2c7644812074fb5ac01f252")},
             {1120000, uint256S("0x8c33837e3657a73aa3a89fa9f31cc565b6d075ddcb246de1cf5d9db90574e344")},
             {1130000, uint256S("0xd953fc97fedf8e580211f1156b82b50f6da37c59e26c7d57dcfed9fbfd489ef8")},
             {1200000, uint256S("0x901c6205092ac4fff321de8241badaf54da4c1f3f7c421b06a442f2a887d88ce")},
             {1300000, uint256S("0xc0d0115689b9687cb03d7520ed45e5500e792a83cd3842034b5f9e26fda6d3ce")},
             {1400000, uint256S("0x4697721a360aa7909e7badf528b3223add193943f1444524284b9a31501cd88a")},
             {1500000, uint256S("0xdc3445dfd8e1f57f42011e6b1d63352a69347c830dc1fab36c699dc6a211b48f")},
             {1600000, uint256S("0xb3970d20ca506d31d191f6422150c5e65696ef55bbc51df844171681ed79693f")},
             {1700000, uint256S("0x67490f7265f5fc8d29a36ebb066a7f4dee724bfa9b7691b8e420544385556c68")},
             {1800000, uint256S("0x820f5b448a49b8273d60377f047eb45b1764cd0a00bf8c219f555b49b9751c66")},
             {1900000, uint256S("0x70ff2582c9ef327a71f5215d58d3ad2b6473b3649b2c018cc1ff524b672d69a2")},
             {2000000, uint256S("0xc2a644527223b80000f11b9a821e398ab99483d71c3cb1304e9c267b64c7b85a")},
             {2100000, uint256S("0xd5e7791acc99afc500679205df06bfb62b298040645f247f41eaf2acb42868cb")},
             {2200000, uint256S("0x8791a85a7ec96571070a589978a99cc2cc0e06c5345056698604e7e793759d08")},
             {2300000, uint256S("0x575ca59268e10b92cfedca6059a388043882f95442b7290012bf8a333ce889c4")},
             {2400000, uint256S("0xdd8ed2992b0df4422d1fc950350c82f84d9a0862f93582f9404d5c3bb4b3a625")},
             {2500000, uint256S("0x07ad693d84ef66eaa81f96db7ad901e871ca02a76b1fabb72c1e300580dd2c71")},
             {2600000, uint256S("0x8d1855390705044b515907cc2096cd2bb4979cb18d6bf1edd26983da60387502")},
             {2687000, uint256S("0x6d2097fce84bd83b066f2a63512b8a44225314cd5f2561eac471071eae291d9a")}
            }
        };

        consensus.powHeights = std::set<int>{1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,52,53,54,55,56,57,58,59,60,61,62,63,64,65,66,67,68,69,70,71,72,73,74,75,76,77,78,79,80,81,82,83,84,85,86,87,88,89,90,91,92,93,94,95,96,97,98,99,100,101,102,103,104,105,106,107,108,109,110,111,112,113,114,115,116,117,118,119,120,121,122,123,124,125,126,127,128,129,130,131,132,133,134,135,136,137,138,139,140,141,142,143,144,145,146,147,148,149,150,151,152,153,154,155,156,157,158,159,160,161,162,163,164,165,166,167,168,169,170,171,172,173,174,175,176,177,178,179,180,181,182,183,184,185,186,187,188,189,190,191,192,193,194,195,196,197,198,199,200,201,202,203,204,205,206,207,208,209,210,211,212,213,214,215,216,217,218,219,220,221,222,223,224,225,226,227,228,229,230,231,232,233,234,235,236,237,238,239,240,241,242,243,244,245,246,247,248,249,250,251,252,253,254,255,256,257,258,259,260,261,262,263,264,265,266,267,268,269,270,271,272,273,274,275,276,277,278,279,280,281,282,283,284,285,286,287,288,289,290,291,292,293,294,295,296,297,298,299,300,301,302,303,304,305,306,307,308,309,310,311,312,313,314,315,316,317,318,319,320,321,322,323,324,325,326,327,328,329,330,331,332,333,334,335,336,337,338,339,340,341,342,343,344,345,346,347,348,349,350,351,352,353,354,355,356,357,358,359,360,361,362,363,364,365,366,367,368,369,370,371,372,373,374,375,376,377,378,379,380,381,382,383,384,385,386,387,388,389,390,391,392,393,394,395,396,397,398,399,400,401,402,403,404,405,406,407,408,409,410,411,412,413,414,415,416,417,418,419,420,421,422,423,424,425,426,427,428,429,430,431,432,433,434,435,436,437,438,439,440,441,442,443,444,445,446,447,448,449,450,451,452,453,454,455,456,457,458,459,460,461,462,463,464,465,466,467,468,469,470,471,472,473,474,475,476,477,478,479,480,481,482,483,484,485,486,487,488,489,490,491,492,493,494,495,496,497,498,499,500,501,502,503,504,505,506,507,508,509,510,511,512,513,514,515,516,517,518,519,520,521,522,523,524,525,526,527,528,529,530,531,532,533,534,535,536,537,538,539,540,541,542,543,544,545,546,547,548,549,550,551,552,553,554,555,556,557,558,559,560,561,562,563,564,565,566,567,568,569,570,571,572,573,574,575,576,577,578,579,580,581,582,583,584,585,586,587,588,589,590,591,592,593,594,595,596,597,598,599,600,601,602,603,604,605,606,607,608,609,610,611,612,613,614,615,616,617,618,619,620,621,622,623,624,625,626,627,628,629,630,631,632,633,634,635,636,637,638,639,640,641,642,643,644,645,646,647,648,649,650,651,652,653,654,655,656,657,658,659,660,661,662,663,664,665,666,667,668,669,670,671,672,673,674,675,676,677,678,679,680,681,682,683,684,685,686,687,688,689,690,691,692,693,694,695,696,697,698,699,700,701,702,703,704,705,706,707,708,709,710,711,712,713,714,715,716,717,718,719,720,721,722,723,724,725,726,727,728,729,730,731,732,733,734,735,736,737,738,739,740,741,742,743,744,745,746,747,748,749,750,751,752,753,754,755,756,757,758,759,760,761,762,763,764,765,766,767,768,769,770,771,772,773,774,775,776,777,778,779,780,781,782,783,784,785,786,787,788,789,790,791,792,793,794,795,796,797,798,799,800,801,802,803,804,805,806,807,808,809,810,811,812,813,814,815,817,818,819,820,821,822,826,827,828,829,830,831,832,834,841,842,843,844,845,846,847,848,861,862,863,870,871,876,877,878,880,881,884,894,897,898,899,900,901,906,910,919,922,923,926,927,928,929,930,940,941,949,950,951,952,957,958,959,960,961,985,986,993,997,1000};

        consensus.mapStakeModifierCheckpoints = Consensus::Params::MapStakeModifierCheckpoints
                                                                 {{0, 0xfd11f4e7},   // genesis
                                                                  {500, 0x3b54b16d}, // premine
                                                                  {1000, 0x7b238954}};

        m_assumeutxo_data = MapAssumeutxo{
         // TODO to be specified in a future patch.
        };

        chainTxData = ChainTxData{
            // Data from RPC: getchaintxstats 4096 00000000000000000008a89e854d57e5667df88f1cdef6fde2fbca1de5b639ad
            /* nTime    */ 1643290426,
            /* nTxCount */ 7744373,
            /* dTxRate  */ 0.1,
        };
    }
};

/**
 * Testnet (v3): public test network which is reset from time to time.
 */
class CTestNetParams : public CChainParams {
public:
    CTestNetParams() {
        strNetworkID = CBaseChainParams::TESTNET;
        consensus.signet_blocks = false;
        consensus.signet_challenge.clear();
        consensus.nSubsidyHalvingInterval = 210000;
        consensus.BIP16Exception = uint256S("0x00000000dd30457c001f4095d208cc1296b0eed002427aa599874af7a432b105");
        consensus.BIP34Height = 1;
        consensus.BIP65Height = 1; // 00000000007f6655f22f98e72ed80d8b06dc761d5da09df0fa1dc4be4f861eb6
        consensus.BIP66Height = 330776; // 000000002104c8c45e99a8853285a3b592602a3ccde2b832481da85e9e4ba182
        consensus.CSVHeight = 770112; // 00000000025e930139bac5c6c31a403776da130831ab85be56578f3fa75369bb
        consensus.SegwitHeight = 834624; // 00000000002b980fcd729daaa248fd9316a5200e9b367f4ff2c42453e84201ca
        consensus.MinBIP9WarningHeight = 40000000; // segwit activation height + miner confirmation window
        consensus.powLimit = ArithToUint256(~arith_uint256(0) >> 1);
        consensus.posLimit = ArithToUint256(~arith_uint256(0) >> 20);
        consensus.nTargetTimespan = 2 * 60 * 60; // two hours
        consensus.nLastPoWBlock = 1000;
//        consensus.nPowTargetSpacing = 10 * 60;
        // number of stake confirmations changed to 10
        consensus.nFork2ConfsChangedHeight = 0;
        // Tachyon upgrade. Approx Jan 12th 2019
        consensus.nFork3TachyonHeight = 110100;
        // Retarget correction
        consensus.nFork4RetargetCorrectHeight = 1163000;
        // Enable cold-staking
        consensus.nFork5ColdStaking = 2386991;

        consensus.nStakeMinAgeV1        = 60;             // minimum age for coin age
        consensus.nStakeMinAgeV2        = 24 * 60 * 60;   // minimum age for coin age
        consensus.nStakeMaxAge          = 7 * 24 * 60 * 60;         // Maximum stake age 7 days
        consensus.nModifierInterval     = 10 * 60; // time to elapse before new modifier is computed

        consensus.nCoinbaseMaturityV1 = 10;
        consensus.nCoinbaseMaturityV2 = 10;
        consensus.nCoinbaseMaturityV3 = 120;

        consensus.nStakeTargetSpacingV1 = 2 * 60;
        consensus.nStakeTargetSpacingV2 = 30;

        consensus.fPowAllowMinDifficultyBlocks = true;
        consensus.fPowNoRetargeting = false;
        consensus.nRuleChangeActivationThreshold = 1512; // 75% for testchains
        consensus.nMinerConfirmationWindow = 2016; // nPowTargetTimespan / nPowTargetSpacing

        // TODO(Sam): Work on deployment stuff
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].bit = 28;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime = Consensus::BIP9Deployment::NEVER_ACTIVE;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].min_activation_height = 0; // No activation delay

        // Deployment of Taproot (BIPs 340-342)
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].bit = 2;
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].nStartTime = 1619222400; // April 24th, 2021
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].nTimeout = 1628640000; // August 11th, 2021
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].min_activation_height = 0; // No activation delay

        consensus.nMinimumChainWork = uint256S("0x0000000000000000000000000000000000000000000005180c3bd8290da33a1a");
        consensus.defaultAssumeValid = uint256S("0x0000000000004ae2f3896ca8ecd41c460a35bf6184e145d91558cece1c688a76"); // 2010000

        pchMessageStart[0] = 0x1b;
        pchMessageStart[1] = 0xba;
        pchMessageStart[2] = 0x63;
        pchMessageStart[3] = 0xc5;
        nDefaultPort = 16325;
        nPruneAfterHeight = 1000;
        m_assumed_blockchain_size = 40;
        m_assumed_chain_state_size = 2;

        genesis = CreateGenesisBlock(1500674579, 8485, arith_uint256(consensus.powLimit.GetHex()).GetCompact(), 1, 0);
        consensus.hashGenesisBlock = genesis.GetHash();
        assert(consensus.hashGenesisBlock == uint256S("0x7286972be4dbc1463d256049b7471c252e6557e222cab9be73181d359cd28bcc"));
        assert(genesis.hashMerkleRoot == uint256S("0x203fd13214321a12b01c0d8b32c780977cf52e56ae35b7383cd389c73291aee7"));

        vFixedSeeds.clear();
        vSeeds.clear();
        // nodes with support for servicebits filtering should be at the top
//        vSeeds.emplace_back("testnet-seed.bitcoin.jonasschnelli.ch.");
//        vSeeds.emplace_back("seed.tbtc.petertodd.org.");
//        vSeeds.emplace_back("seed.testnet.bitcoin.sprovoost.nl.");
//        vSeeds.emplace_back("testnet-seed.bluematt.me."); // Just a static list of stable node(s), only supports x9

        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,65);
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,127);
        base58Prefixes[SECRET_KEY] =     std::vector<unsigned char>(1,128+65);
        base58Prefixes[EXT_PUBLIC_KEY] = {0x06, 0x35, 0x87, 0xCF};
        base58Prefixes[EXT_SECRET_KEY] = {0x06, 0x35, 0x83, 0x94};

        bech32_hrp = "tnb";

        vFixedSeeds = std::vector<uint8_t>(std::begin(chainparams_seed_test), std::end(chainparams_seed_test));

        fDefaultConsistencyChecks = false;
        fRequireStandard = false;
        m_is_test_chain = true;
        m_is_mockable_chain = false;

        checkpointData = {
            {
             {0,       genesis.GetHash()},
             {1,       uint256S("0x0e2eecad99db0eab96abbd7e2de769d92483a090eefcefc014b802d31131a0ce")},
             {500,     uint256S("0x0000006939777fded9640797f3008d9fca5d6e177e440655ba10f8a900cabe61")},
             {1000,    uint256S("0x000004715d8818cea9c2e5e9a727eb2f950964eb0d1060e1d5effd44c2ca45df")},
             {100000,  uint256S("0x1fdbb9642e997fa13df3b0c11c95e959a2606ef9bc6c431e942cf3fc74ed344d")},
             {200000,  uint256S("0xf4072b1e5b7ede5b33c82045b13f225b41ff3d8262e03ea5ed9521290e2d5e42")},
             {300000,  uint256S("0x448d74d70dea376576217ef72518f18f289ab4680f6714cdac8a3903f7a2cacf")},
             {400000,  uint256S("0x09c3bd420fa43ab4e591b0629ed8fe0e86fc264939483d6b7cb0a59f05020953")},
             {500000,  uint256S("0xae87c4f158e07623b88aa089f2de3e3437352873293febcfa1585b07e823d955")},
             {600000,  uint256S("0x3c7dbe265d43da7834c3f291e031dda89ef6c74f2950f0af15acf33768831f91")},
             {700000,  uint256S("0xa5bcfb2d5d52e8c0bdce1ae11019a7819d4d626e6836f1980fe6b5ce13c10039")},
             {800000,  uint256S("0x13a2c603fbdb4ced718d6f7bba60b335651ddb832fbe8e11962e454c6625e20f")},
             {900000,  uint256S("0xe5c4d6f1fbd90b6a2af9a02f1e947422a4c5a8756c34d7f0e45f57b341e47156")},
             {1000000, uint256S("0x806506a6eafe00e213c666a8c8fd14dac0c6d6a52e0f05a4d175633361e5e377")},
             {1100000, uint256S("0x397b5e6e0e95d74d7c01064feae627d11a2a99d08ebf91200dbb9d94b1d4ee26")},
             {1200000, uint256S("0x54e813b81516c1a6169ff81abaec2715e13b2ec0796db4fcc510be1e0805d21e")},
             {1300000, uint256S("0x75da223a32b31b3bbb1f32ab33ad5079b70698902ebed5594bebc02ffecb74a8")},
             {1400000, uint256S("0x064c16b9c408e40f020ca455255e58da98b019eb424554259407d7461c5258e2")},
             {1500000, uint256S("0x1fc65c5e904c0dda39a26826df0feaa1d35f5d49657acee2d1674271f38b2100")},
             {1600000, uint256S("0x8510acea950aa7e2da8d287bacc66cca6056bf89f5f0d70109fd92adaf1023d9")},
             {1700000, uint256S("0x65738a87a454cfe97b8200149cd4be7199d1ceff30b18778bd79d222203962ce")},
             {1801000, uint256S("0x406fc58723c11eae128c85174e81b5b6b333eaf683ff4f6ca34bbd8cee3b24f5")},
             {2521000, uint256S("0xd3dc0dd25f4850fa8a607620620959e1970e7bcfe9b36ffd8df3bda1004e5cab")},
             {2581300, uint256S("0xe90b2a55da410f834e047a1f2c1d1901f6beeba2a366a6ce05b01112e9973432")}
            }
        };

        consensus.powHeights = std::set<int>{1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,52,53,54,55,56,57,58,59,60,61,62,63,64,65,66,67,68,69,70,71,72,73,74,75,76,77,78,79,80,81,82,83,84,85,86,87,88,89,90,91,92,93,94,95,96,97,98,99,100,101,102,103,104,105,106,107,108,109,110,111,112,113,114,115,116,117,118,119,120,121,122,123,124,125,126,127,128,129,130,131,132,133,134,135,136,137,138,139,140,141,142,143,144,145,146,147,148,149,150,151,152,153,154,155,156,157,158,159,160,161,162,163,164,165,166,167,168,169,170,171,172,173,174,175,176,177,178,179,180,181,182,183,184,185,186,187,188,189,190,191,192,193,194,195,196,197,198,199,200,201,202,203,204,205,206,207,208,209,210,211,212,213,214,215,216,217,218,219,220,221,222,223,224,225,226,227,228,229,230,231,232,233,234,235,236,237,238,239,240,241,242,243,244,245,246,247,248,249,250,251,252,253,254,255,256,257,258,259,260,261,262,263,264,265,266,267,268,269,270,271,272,273,274,275,276,277,278,279,280,281,282,283,284,285,286,287,288,289,290,291,292,293,294,295,296,297,298,299,300,301,302,303,304,305,306,307,308,309,310,311,312,313,314,315,316,317,318,319,320,321,322,323,324,325,326,327,328,329,330,331,332,333,334,335,336,337,338,339,340,341,342,343,344,345,346,347,348,349,350,351,352,353,354,355,356,357,358,359,360,361,362,363,364,365,366,367,368,369,370,371,372,373,374,375,376,377,378,379,380,381,382,383,384,385,386,387,388,389,390,391,392,393,394,395,396,397,398,399,400,401,402,403,404,405,406,407,408,409,410,411,412,413,414,415,416,417,418,419,420,421,422,423,424,425,426,427,428,429,430,431,432,433,434,435,436,437,438,439,440,441,442,443,444,445,446,447,448,449,450,451,452,453,454,455,456,457,458,459,460,461,462,463,464,465,466,467,468,469,470,471,472,473,474,475,476,477,478,479,480,481,482,483,484,485,486,487,488,489,490,491,492,493,494,495,496,497,498,499,500,501,502,503,504,505,506,507,508,509,510,511,512,513,514,515,516,517,518,519,520,521,522,523,524,525,526,527,528,529,530,531,532,533,534,535,536,537,538,539,542,576,578,584,597,599,601,607,609,610,611,612,619,620,622,635,639,640,641,644,645,646,650,651,653,659,661,662,664,665,670,677,686,693,697,698,699,701,705,706,708,709,711,712,713,717,719,720,724,733,734,736,740,741,742,744,749,750,752,753,754,756,757,758,759,760,761,766,767,770,773,774,775,777,778,782,784,785,791,792,793,794,795,796,801,802,805,806,807,808,809,810,811,819,821,822,823,824,825,826,827,828,830,831,832,835,838,839,840,841,842,844,848,850,851,852,855,860,862,866,868,870,875,877,878,879,880,881,882,883,884,885,886,887,888,894,895,898,899,902,904,905,910,911,916,917,919,922,923,925,926,929,930,931,933,934,935,936,937,938,940,943,950,951,952,954,956,958,959,960,961,962,963,965,968,984,985,988,994,995,996,998,999,1000};

        // Hard checkpoints of stake modifiers to ensure they are deterministic (testNet)
        consensus.mapStakeModifierCheckpoints = Consensus::Params::MapStakeModifierCheckpoints
            {{0, 0xfd11f4e7},    // genesis
             {100, 0x7bb33af1}};

        m_assumeutxo_data = MapAssumeutxo{
            // TODO to be specified in a future patch.
        };

        chainTxData = ChainTxData{
            // Data from RPC: getchaintxstats 4096 0000000000004ae2f3896ca8ecd41c460a35bf6184e145d91558cece1c688a76
            /* nTime    */ 1643290413,
            /* nTxCount */ 7527836,
            /* dTxRate  */ 0.1,
        };
    }
};

/**
 * Signet: test network with an additional consensus parameter (see BIP325).
 */
class SigNetParams : public CChainParams {
public:
    explicit SigNetParams(const ArgsManager& args) {
        std::vector<uint8_t> bin;
        vSeeds.clear();

        if (!args.IsArgSet("-signetchallenge")) {
            bin = ParseHex("512103ad5e0edad18cb1f0fc0d28a3d4f1f3e445640337489abb10404f2d1e086be430210359ef5021964fe22d6f8e05b2463c9540ce96883fe3b278760f048f5189f2e6c452ae");
//            vSeeds.emplace_back("seed.signet.bitcoin.sprovoost.nl.");

            // Hardcoded nodes can be removed once there are more DNS seeds
            vSeeds.emplace_back("178.128.221.177");
            vSeeds.emplace_back("v7ajjeirttkbnt32wpy3c6w3emwnfr3fkla7hpxcfokr3ysd3kqtzmqd.onion:38333");

            consensus.nMinimumChainWork = uint256S("0x0000000000000000000000000000000000000000000000000000008546553c03");
            consensus.defaultAssumeValid = uint256S("0x000000187d4440e5bff91488b700a140441e089a8aaea707414982460edbfe54"); // 47200
            m_assumed_blockchain_size = 1;
            m_assumed_chain_state_size = 0;
            chainTxData = ChainTxData{
                // Data from RPC: getchaintxstats 4096 000000187d4440e5bff91488b700a140441e089a8aaea707414982460edbfe54
                /* nTime    */ 1626696658,
                /* nTxCount */ 387761,
                /* dTxRate  */ 0.04035946932424404,
            };
        } else {
            const auto signet_challenge = args.GetArgs("-signetchallenge");
            if (signet_challenge.size() != 1) {
                throw std::runtime_error(strprintf("%s: -signetchallenge cannot be multiple values.", __func__));
            }
            bin = ParseHex(signet_challenge[0]);

            consensus.nMinimumChainWork = uint256{};
            consensus.defaultAssumeValid = uint256{};
            m_assumed_blockchain_size = 0;
            m_assumed_chain_state_size = 0;
            chainTxData = ChainTxData{
                0,
                0,
                0,
            };
            LogPrintf("Signet with challenge %s\n", signet_challenge[0]);
        }

        if (args.IsArgSet("-signetseednode")) {
            vSeeds = args.GetArgs("-signetseednode");
        }

        strNetworkID = CBaseChainParams::SIGNET;
        consensus.signet_blocks = true;
        consensus.signet_challenge.assign(bin.begin(), bin.end());
        consensus.nSubsidyHalvingInterval = 210000;
        consensus.BIP16Exception = uint256{};
        consensus.BIP34Height = 1;
        consensus.BIP65Height = 1;
        consensus.BIP66Height = 1;
        consensus.CSVHeight = 1;
        consensus.SegwitHeight = 1;
        consensus.nPowTargetTimespan = 14 * 24 * 60 * 60; // two weeks
//        consensus.nPowTargetSpacing = 10 * 60;
        consensus.fPowAllowMinDifficultyBlocks = false;
        consensus.fPowNoRetargeting = false;
        consensus.nRuleChangeActivationThreshold = 1815; // 90% of 2016
        consensus.nMinerConfirmationWindow = 2016; // nPowTargetTimespan / nPowTargetSpacing
        consensus.MinBIP9WarningHeight = 0;
        consensus.powLimit = uint256S("00000377ae000000000000000000000000000000000000000000000000000000");
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].bit = 28;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime = Consensus::BIP9Deployment::NEVER_ACTIVE;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].min_activation_height = 0; // No activation delay

        // Activation of Taproot (BIPs 340-342)
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].bit = 2;
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].nStartTime = Consensus::BIP9Deployment::ALWAYS_ACTIVE;
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].min_activation_height = 0; // No activation delay

        // message start is defined as the first 4 bytes of the sha256d of the block script
        CHashWriter h(SER_DISK, 0);
        h << consensus.signet_challenge;
        uint256 hash = h.GetHash();
        memcpy(pchMessageStart, hash.begin(), 4);

        nDefaultPort = 38333;
        nPruneAfterHeight = 1000;

        genesis = CreateGenesisBlock(1598918400, 52613770, 0x1e0377ae, 1, 50 * COIN);
        consensus.hashGenesisBlock = genesis.GetHash();
        // TODO(Sam)
//        assert(consensus.hashGenesisBlock == uint256S("0x00000008819873e925422c1ff0f99f7cc9bbb232af63a077a480a3633bee1ef6"));
//        assert(genesis.hashMerkleRoot == uint256S("0x4a5e1e4baab89f3a32518a88c31bc87f618f76673e2cc77ab2127b7afdeda33b"));

        vFixedSeeds.clear();

        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,111);
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,196);
        base58Prefixes[SECRET_KEY] =     std::vector<unsigned char>(1,239);
        base58Prefixes[EXT_PUBLIC_KEY] = {0x04, 0x35, 0x87, 0xCF};
        base58Prefixes[EXT_SECRET_KEY] = {0x04, 0x35, 0x83, 0x94};

        bech32_hrp = "tb";

        fDefaultConsistencyChecks = false;
        fRequireStandard = true;
        m_is_test_chain = true;
        m_is_mockable_chain = false;
    }
};

/**
 * Regression test: intended for private networks only. Has minimal difficulty to ensure that
 * blocks can be found instantly.
 */
class CRegTestParams : public CChainParams {
public:
    explicit CRegTestParams(const ArgsManager& args) {
        strNetworkID =  CBaseChainParams::REGTEST;
        consensus.signet_blocks = false;
        consensus.signet_challenge.clear();
        consensus.nSubsidyHalvingInterval = 150;
        consensus.BIP16Exception = uint256();
        consensus.BIP34Height = 1; // Always active unless overridden
        consensus.BIP65Height = 1;  // Always active unless overridden
        consensus.BIP66Height = 1;  // Always active unless overridden
        consensus.CSVHeight = 1;    // Always active unless overridden
        consensus.SegwitHeight = 1; // Always active unless overridden
        consensus.MinBIP9WarningHeight = 0;
        consensus.powLimit = uint256S("7fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
        consensus.nPowTargetTimespan = 14 * 24 * 60 * 60; // two weeks
        consensus.fPowAllowMinDifficultyBlocks = true;
        consensus.fPowNoRetargeting = true;
        consensus.nRuleChangeActivationThreshold = 108; // 75% for testchains
        consensus.nMinerConfirmationWindow = 144; // Faster than normal for regtest (144 instead of 2016)

        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].bit = 28;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].min_activation_height = 0; // No activation delay

        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].bit = 2;
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].nStartTime = Consensus::BIP9Deployment::ALWAYS_ACTIVE;
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].min_activation_height = 0; // No activation delay

        consensus.nMinimumChainWork = uint256{};
        consensus.defaultAssumeValid = uint256{};

        pchMessageStart[0] = 0xfa;
        pchMessageStart[1] = 0xbf;
        pchMessageStart[2] = 0xb5;
        pchMessageStart[3] = 0xda;
        nDefaultPort = 18444;
        nPruneAfterHeight = args.GetBoolArg("-fastprune", false) ? 100 : 1000;
        m_assumed_blockchain_size = 0;
        m_assumed_chain_state_size = 0;

        UpdateActivationParametersFromArgs(args);

        genesis = CreateGenesisBlock(1296688602, 2, 0x207fffff, 1, 50 * COIN);
        consensus.hashGenesisBlock = genesis.GetHash();
        // TODO(Sam)
//        assert(consensus.hashGenesisBlock == uint256S("0x0f9188f13cb7b2c71f2a335e3a4fc328bf5beb436012afca590b1a11466e2206"));
//        assert(genesis.hashMerkleRoot == uint256S("0x4a5e1e4baab89f3a32518a88c31bc87f618f76673e2cc77ab2127b7afdeda33b"));

        vFixedSeeds.clear(); //!< Regtest mode doesn't have any fixed seeds.
        vSeeds.clear();
        vSeeds.emplace_back("dummySeed.invalid.");

        fDefaultConsistencyChecks = true;
        fRequireStandard = true;
        m_is_test_chain = true;
        m_is_mockable_chain = true;

        checkpointData = {
            {
                {0, uint256S("0f9188f13cb7b2c71f2a335e3a4fc328bf5beb436012afca590b1a11466e2206")},
            }
        };

        m_assumeutxo_data = MapAssumeutxo{
            {
                110,
                {AssumeutxoHash{uint256S("0x1ebbf5850204c0bdb15bf030f47c7fe91d45c44c712697e4509ba67adb01c618")}, 110},
            },
            {
                200,
                {AssumeutxoHash{uint256S("0x51c8d11d8b5c1de51543c579736e786aa2736206d1e11e627568029ce092cf62")}, 200},
            },
        };

        chainTxData = ChainTxData{
            0,
            0,
            0
        };

        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,111);
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,196);
        base58Prefixes[SECRET_KEY] =     std::vector<unsigned char>(1,239);
        base58Prefixes[EXT_PUBLIC_KEY] = {0x04, 0x35, 0x87, 0xCF};
        base58Prefixes[EXT_SECRET_KEY] = {0x04, 0x35, 0x83, 0x94};

        bech32_hrp = "bcrt";
    }

    /**
     * Allows modifying the Version Bits regtest parameters.
     */
    void UpdateVersionBitsParameters(Consensus::DeploymentPos d, int64_t nStartTime, int64_t nTimeout, int min_activation_height)
    {
        consensus.vDeployments[d].nStartTime = nStartTime;
        consensus.vDeployments[d].nTimeout = nTimeout;
        consensus.vDeployments[d].min_activation_height = min_activation_height;
    }
    void UpdateActivationParametersFromArgs(const ArgsManager& args);
};

static void MaybeUpdateHeights(const ArgsManager& args, Consensus::Params& consensus)
{
    for (const std::string& arg : args.GetArgs("-testactivationheight")) {
        const auto found{arg.find('@')};
        if (found == std::string::npos) {
            throw std::runtime_error(strprintf("Invalid format (%s) for -testactivationheight=name@height.", arg));
        }
        const auto name{arg.substr(0, found)};
        const auto value{arg.substr(found + 1)};
        int32_t height;
        if (!ParseInt32(value, &height) || height < 0 || height >= std::numeric_limits<int>::max()) {
            throw std::runtime_error(strprintf("Invalid height value (%s) for -testactivationheight=name@height.", arg));
        }
        if (name == "segwit") {
            consensus.SegwitHeight = int{height};
        } else if (name == "bip34") {
            consensus.BIP34Height = int{height};
        } else if (name == "dersig") {
            consensus.BIP66Height = int{height};
        } else if (name == "cltv") {
            consensus.BIP65Height = int{height};
        } else if (name == "csv") {
            consensus.CSVHeight = int{height};
        } else {
            throw std::runtime_error(strprintf("Invalid name (%s) for -testactivationheight=name@height.", arg));
        }
    }
}

void CRegTestParams::UpdateActivationParametersFromArgs(const ArgsManager& args)
{
    MaybeUpdateHeights(args, consensus);

    if (!args.IsArgSet("-vbparams")) return;

    for (const std::string& strDeployment : args.GetArgs("-vbparams")) {
        std::vector<std::string> vDeploymentParams;
        boost::split(vDeploymentParams, strDeployment, boost::is_any_of(":"));
        if (vDeploymentParams.size() < 3 || 4 < vDeploymentParams.size()) {
            throw std::runtime_error("Version bits parameters malformed, expecting deployment:start:end[:min_activation_height]");
        }
        int64_t nStartTime, nTimeout;
        int min_activation_height = 0;
        if (!ParseInt64(vDeploymentParams[1], &nStartTime)) {
            throw std::runtime_error(strprintf("Invalid nStartTime (%s)", vDeploymentParams[1]));
        }
        if (!ParseInt64(vDeploymentParams[2], &nTimeout)) {
            throw std::runtime_error(strprintf("Invalid nTimeout (%s)", vDeploymentParams[2]));
        }
        if (vDeploymentParams.size() >= 4 && !ParseInt32(vDeploymentParams[3], &min_activation_height)) {
            throw std::runtime_error(strprintf("Invalid min_activation_height (%s)", vDeploymentParams[3]));
        }
        bool found = false;
        for (int j=0; j < (int)Consensus::MAX_VERSION_BITS_DEPLOYMENTS; ++j) {
            if (vDeploymentParams[0] == VersionBitsDeploymentInfo[j].name) {
                UpdateVersionBitsParameters(Consensus::DeploymentPos(j), nStartTime, nTimeout, min_activation_height);
                found = true;
                LogPrintf("Setting version bits activation parameters for %s to start=%ld, timeout=%ld, min_activation_height=%d\n", vDeploymentParams[0], nStartTime, nTimeout, min_activation_height);
                break;
            }
        }
        if (!found) {
            throw std::runtime_error(strprintf("Invalid deployment (%s)", vDeploymentParams[0]));
        }
    }
}

static std::unique_ptr<const CChainParams> globalChainParams;

const CChainParams &Params() {
    assert(globalChainParams);
    return *globalChainParams;
}

std::unique_ptr<const CChainParams> CreateChainParams(const ArgsManager& args, const std::string& chain)
{
    if (chain == CBaseChainParams::MAIN) {
        return std::unique_ptr<CChainParams>(new CMainParams());
    } else if (chain == CBaseChainParams::TESTNET) {
        return std::unique_ptr<CChainParams>(new CTestNetParams());
    } else if (chain == CBaseChainParams::SIGNET) {
        return std::unique_ptr<CChainParams>(new SigNetParams(args));
    } else if (chain == CBaseChainParams::REGTEST) {
        return std::unique_ptr<CChainParams>(new CRegTestParams(args));
    }
    throw std::runtime_error(strprintf("%s: Unknown chain %s.", __func__, chain));
}

void SelectParams(const std::string& network)
{
    SelectBaseParams(network);
    globalChainParams = CreateChainParams(gArgs, network);
}
