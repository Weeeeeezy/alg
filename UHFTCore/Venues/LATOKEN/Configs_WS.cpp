// vim:ts=2:et
//===========================================================================//
//                      "Venues/LATOKEN/Configs_WS.cpp":                     //
//===========================================================================//
#include "Venues/LATOKEN/Configs_WS.h"

using namespace std;

namespace MAQUETTE
{
namespace LATOKEN
{
  //=========================================================================//
  // "Configs_WS_OMC":                                                       //
  //=========================================================================//
  // NB: IP lists are empty, as the HostNames are IPs themselves:
  //
  map<MQTEnvT, H2WSConfig const> const Configs_WS_OMC
  {
    //-----------------------------------------------------------------------//
    { MQTEnvT::Prod,
    //-----------------------------------------------------------------------//
      { "35.246.179.64", 7681, {} }
    },
    //-----------------------------------------------------------------------//
    { MQTEnvT::Test,
    //-----------------------------------------------------------------------//
      // Aka PreProd: "trading-kafka-services-prod-1", InternalIP=10.156.0.17:
      { "35.234.103.62", 7681, {} }
    }
  };

  //=========================================================================//
  // "Configs_WS_MDC": TODO:                                                 //
  //=========================================================================//
  map<MQTEnvT, H2WSConfig const> const Configs_WS_MDC
  {
    //-----------------------------------------------------------------------//
    { MQTEnvT::Prod,
    //-----------------------------------------------------------------------//
      { "35.246.179.64", 7683, {} }
    },
    //-----------------------------------------------------------------------//
    { MQTEnvT::Test,
    //-----------------------------------------------------------------------//
      // Aka PreProd: "trading-kafka-services-prod-1", InternalIP=10.156.0.17:
      { "35.240.39.66",  7683, {} }
    }
  };

  //=========================================================================//
  // "APIKeysIntMM":                                                         //
  //=========================================================================//
  map<UserID, string const> const APIKeysIntMM
  {
    {  50863, "TavLTxK7mCEYPbkBAckzKNDB" },
    {  50872, "ULZ9A3gQZ6B3Mvns5uE4T8BA" },
    {  50997, "2F9acrbDRKuY4PuBs2TJ8gJC" },
    {  51002, "paKsDmL5k5ETVp6zxG9gQTLD" },
    {  51983, "DBJJLn6MKAqUjZjhQdMqSDpK" },
    {  53589, "5qZezdE7j8rK9M4vJPUa6GpZ" },
    {  65127, "dkqVWdMWebLEZpjY6QdZpHGW" },
    {  65128, "97vVTBthqGUKweD9hrWSsEfH" },
    {  65129, "6Kz5Zx82mhTuv4eEUEhxUDVR" },
    {  65130, "C7PFtZwJnDeqXSajvJNWay6b" },
    {  65131, "EZbU6PPXVf6sj5YrYBvpSYr5" },
    {  65132, "ThfzZcv6EU3fBFMYZQnvbYcg" },
    {  65133, "fK8jK5vCskEKt5rgkS34zDgd" },
    {  65134, "aKhKxffKktKTy7EDwZZJBnUY" },
    {  65135, "4qBYnZsUzr52g4qvr27j7Tb9" },
    {  65136, "YzHvppMtcVSPX8atALWKXVmx" },
    {  67308, "yw3qdVKYU8pBDBLG29VhvfPL" },
    {  74959, "h7kQXWqpzKNT5tnkxZLByQhp" },
    {  75067, "HxNsmVfG38ZsUByFJWQJPrnD" },
    {  80620, "8JucZRCKUzyAxjNURGuG6d6E" },
    {  80622, "gHneY4LdZbv3rWALVUTFnfm6" },
    {  80624, "stZV6HJFbMazNVsL7LMjFVer" },
    {  80625, "kjZPErxjKc9C5SWhuvJdYVKK" },
    {  80626, "9yr6FyLvUuc7r4UqchBkMAfq" },
    {  80627, "9GeLAtsraS6QaHJ45PRWybS5" },
    {  80631, "QurDns5exCzuvy8G2zRAzrFA" },
    {  80632, "4L745dgmZSTVkTXCzJuE4GrR" },
    {  80633, "fCc4F9MAMNvQFDWQu7Ybe6ng" },
    {  80635, "QPPk6AdCeVVVMt56AEVCCdTh" },
    {  80638, "7JnXTrbNjZpZJBM8H79DHGkH" },
    {  80640, "ePPMHmw3RBQ2mmJrvKnDFXxx" },
    {  80641, "r9TfkKAsy6MeUYcstgVXgysh" },
    {  80642, "UZjkdBGfQ6qXK8Gjvx5wJ9G9" },
    {  80643, "e3WpH2kfVL486JPeXt8BxTkP" },
    {  82112, "3zrJjeUqvV7TGfDjq6sEcrcJ" },
    {  93356, "8rKUvc5qpdDv4EHU7LhDtRcz" },
    { 100694, "G4Dt6YArk9UQdheW4LHvWBpb" },
    { 100696, "W2xLGKSZEwZeXUdgau5UUf2U" },
    { 106357, "bM3z8w2qMEeNSG84vHCGPEr4" },
    { 106361, "MpQMAhumga9EXd6Raw54FYer" },
    { 106364, "WG8AMkAkraQCUpPP5ntrc79Z" },
    { 106366, "4vAKvwCPSuxFTEQE9YWa9C4X" },
    { 106367, "fYcuass2T4xEFUeN87KmvPxt" },
    { 106369, "RAQ6cWU7NgJFj46WfKFSc8yf" },
    { 106371, "eqx5hyvtHYhBd4CuawCWXdsv" },
    { 106373, "MBRm4vDEevZkqwn2tRxWbSkh" },
    { 106374, "VHpAe69UWgt8A25WpsU5bSZV" },
    { 106375, "Cbub5BtXq6QPgcJZaEySgGjP" },
    { 106638, "cGHuS22nVZ9LEVYJPr6bcgVW" },
    { 106641, "U3j9bqqNQ79A7eBpnukgvD2s" },
    { 106643, "eegFYWxUweBgq274GGFGxZQH" },
    { 106644, "quEBhenWVaHzC32kRA5KPQMS" },
    { 106646, "hGKBCUsgBTHE6P4YzjtgZwgH" },
    { 106647, "qMpAnkF6ZAtWaPTUBkCXQWjy" },
    { 106648, "yF6bgpAUsfXWWLH33BcmtPFY" },
    { 106649, "gNxMZ5XMmHLk99uF2J9RgYuQ" },
    { 106652, "fQYtsWgTcUrk9EnhEmgJP3gd" },
    { 106654, "PFnJETq5QsyzQB2V97FGXXJ8" },
    { 106675, "VzXHWzh8xRuGghqrkP8GsJV6" },
    { 106676, "n7Y7Q4eNmZvWSfH24bVPY5cS" },
    { 106680, "UUAbzf7ev3GD7R2qAbzh27PU" },
    { 106681, "SUv7WLPAEA5k464cJx8uExGZ" },
    { 106683, "wTtgfMBYqGsW2ZMmgfRQvZRz" },
    { 106684, "R5MNd6AXNBxXJq3v7re7nPpA" },
    { 106685, "NSh2MzqMLHw3bM576Wnkc8yz" },
    { 106687, "sknnEAgzS5fW6y7RW6MUGEMD" },
    { 106688, "XgaGXXrS8wELNQ8XTHtw6YxR" },
    { 106690, "77Bgby8nsxrfwxGcjUK2sAyR" },
    { 107454, "9tN3b6DDcbUaZL5cQVZEBHUW" },
    { 107458, "65mYKZFQrUvqrTtzhSa2KyX5" },
    { 107460, "jpEZEJWe8cSJg84yYpyKRHmv" },
    { 107462, "sHuXYLEsuyHfggZUBq62nfKc" },
    { 107464, "gDXqQJT45a7NcwND89rEHxXQ" },
    { 107466, "3r85GTz475QzjAeUFNwHEEjr" },
    { 107468, "bXyWDMFBNbrt8PtPwa9kTzH2" },
    { 107469, "VNppGhs8hGEL7GUUThqq4KZ3" },
    { 107473, "j5qQBn3CdZQLLfLQAfd9Bn88" },
    { 107476, "5KkPsjZKZFKC8XHqKYptDthq" },
    { 107479, "3KxMzfJHWtgZ2kWuvCa9xpj9" },
    { 107484, "ysvBLaqEJyGCcaPswSGnzL5P" },
    { 107489, "L9SMvqjQLcbuZcdMbeNPpMLS" },
    { 107582, "HJ7kGedUzwb2catckCGC7v2H" },
    { 107585, "yv3ygvKkB8r8rTThWEsyzvfE" },
    { 107586, "hKXVzeUemcgzjmaBGEsWTuAd" },
    { 107588, "JEyRWF9JpAut3PFd3LHxXN4a" },
    { 107591, "X3kPzsKPuc7VcVDYvUkHqfeD" },
    { 107594, "Hb7cn7BaTTTJfjuYd5sQdasp" }
  };

  //=========================================================================//
  // "UserIDsExtMM":                                                         //
  //=========================================================================//
  vector<UserID> const UserIDsExtMM
  {
    692703,
    692704
  };
} // End namespace LATOKEN
} // End namespace MAQUETTE
