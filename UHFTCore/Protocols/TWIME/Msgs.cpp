// vim:ts=2:et
//===========================================================================//
//                          "Protocols/TWIME/Msgs.cpp":                      //
//===========================================================================//
#include "Basis/BaseTypes.hpp"
#include "Protocols/TWIME/Msgs.h"

using namespace std;

namespace MAQUETTE
{
namespace TWIME
{
  //=========================================================================//
  // "MsgSizes" Table:                                                       //
  //=========================================================================//
  // Table of Expected Sizes of All TWIME Msg Types:
  //
  uint16_t const MsgSizes[26]
  {
    // Session-Level Msgs (9: 0..8):
    sizeof(Establish),              sizeof(EstablishmentAck),
    sizeof(EstablishmentReject),    sizeof(Terminate),
    sizeof(ReTransmitRequest),      sizeof(ReTransmission),
    sizeof(Sequence),               sizeof(FloodReject),
    sizeof(SessionReject),

    // Application-Level Requests  (5:   9..13):
    sizeof(NewOrderSingle),         sizeof(NewOrderMultiLeg),
    sizeof(OrderCancelRequest),     sizeof(OrderReplaceRequest),
    sizeof(OrderMassCancelRequest),

    // Application-Level Responses (12: 14..25):
    sizeof(NewOrderSingleResponse), sizeof(NewOrderMultiLegResponse),
    sizeof(NewOrderReject),         sizeof(OrderCancelResponse),
    sizeof(OrderCancelReject),      sizeof(OrderReplaceResponse),
    sizeof(OrderReplaceReject),     sizeof(OrderMassCancelResponse),
    sizeof(ExecutionSingleReport),  sizeof(ExecutionMultiLegReport),
    sizeof(EmptyBook),              sizeof(SystemEvent)
  };

  //=========================================================================//
  // "MsgTypeNames" Table:                                                   //
  //=========================================================================//
  char const* const MsgTypeNames[26]
  {
    // Session-Level Msgs (9: 0..8):
    "Establish",                    "EstablishmentAck",
    "EstablishmentReject",          "Terminate",
    "ReTransmitRequest",            "ReTransmission",
    "Sequence",                     "FloodReject",
    "SessionReject",

    // Application-Level Requests  "5:   9..13":
    "NewOrderSingle",               "NewOrderMultiLeg",
    "OrderCancelRequest",           "OrderReplaceRequest",
    "OrderMassCancelRequest",

    // Application-Level Responses "12: 14..25":
    "NewOrderSingleResponse",       "NewOrderMultiLegResponse",
    "NewOrderReject",               "OrderCancelResponse",
    "OrderCancelReject",            "OrderReplaceResponse",
    "OrderReplaceReject",           "OrderMassCancelResponse",
    "ExecutionSingleReport",        "ExecutionMultiLegReport",
    "EmptyBook",                    "SystemEvent"
  };

  //=========================================================================//
  // "ErrCodes" Table:                                                       //
  //=========================================================================//
  // XXX: Many of the Reasons below are repeated (identical or similar strings
  // under different codes).  Still, we have to list them all:
  //
  unordered_map<int32_t, char const*> const RejectReasons
  {
    {   -1, "Error performing operation" },
    {    0, "Operation successful" },
    {    1, "User not found" },
    {    2, "Brokerage Firm code not found" },
    {    3, "Session inactive" },
    {    4, "Session halted" },
    {    5, "Error performing operation" },
    {    6, "Insufficient rights to perform operation" },
    {    7, "Wrong account Cannot perform operation" },
    {    8, "Insufficient rights to perform order deletion" },
    {    9, "Operations with orders are blocked for the firm by the Clearing "
            "Centre" },
    {   10, "Insufficient funds to reserve" },
    {   12, "Options premium exceeds the limit allowed" },
    {   13, "Total amount of positions exceeds the market limit" },
    {   14, "Order not found" },
    {   25, "Unable to add order: prohibited by the Trading Administrator" },
    {   26, "Unable to open position: prohibited by Trading Administrator" },
    {   27, "Unable to open short position: prohibited by Trading "
            "Administrator" },
    {   28, "Unable to perform operation: insufficient rights" },
    {   31, "Matching order for the same account/ITN is not allowed" },
    {   32, "Trade price exceeds the limit allowed" },
    {   33, "Operations with orders are blocked for this firm by the "
            "Clearing Administrator" },
    {   34, "Cannot perform operation: wrong client code" },
    {   35, "Invalid input parameters" },
    {   36, "Cannot perform operation: wrong underlying" },
    {   37, "Multi-leg orders cannot be moved" },
    {   38, "Negotiated orders cannot be moved" },
    {   39, "Price is not a multiple of the tick size" },
    {   40, "Unable to add Negotiated order: counterparty not found" },
    {   41, "User's trading rights have expired or are not valid yet" },
    {   42, "Operations are prohibited by Chief Trader of Clearing Firm" },
    {   44, "Clearing Firm's Chief Trader flag not found for this firm" },
    {   45, "Unable to add Negotiated orders: no RTS code found for this "
            "firm" },
    {   46, "Only Negotiated orders are allowed for this security" },
    {   47, "There was no trading in this security during the session "
            "specified" },
    {   48, "This security is being delivered Only Negotiated orders from all "
            "Brokerage Firms within the same Clearing Firm are allowed" },
    {   49, "Unable to add Negotiated order: a firm code must be specified" },
    {   50, "Order not found" },
    {   53, "Error setting input parameter: amount too large" },
    {   54, "Unable to perform operation: exceeded operations quota for this "
            "client" },
    {   56, "Unable to perform operations using this login/code pair: "
            "insufficient rights. Please contact the Trading Administrator" },
    {   57, "Unable to connect to the Exchange server: insufficient rights. "
            "Please contact the Trading Administrator" },
    {   58, "Unable to add orders without verifying client funds sufficiency: "
            "insufficient rights" },
    {   60, "Auction halted for all risk-netting instruments" },
    {   61, "Trading halted in all risk-netting instruments" },
    {   62, "Trading halted on the MOEX Derivatives Market" },
    {   63, "Auction halted in all risk-netting instruments with this "
            "underlying" },
    {   64, "Trading halted in all risk-netting instruments with this "
            "underlying" },
    {   65, "Trading halted on all boards in all securities with this "
            "underlying" },
    {   66, "Trading halted in this risk-netting instrument" },
    {   67, "Unable to open positions in this risk-netting instrument: "
            "prohibited by the Trading Administrator" },
    {   68, "Unable to add orders for all risk-netting instruments: "
            "prohibited by the Brokerage Firm" },
    {   69, "Unable to add orders for all risk-netting instruments: "
            "prohibited by the Chief Trader" },
    {   70, "Trading operation is not supported" },
    {   71, "Position size exceeds the allowable limit" },
    {   72, "Order is being moved" },
    {   73, "Aggregated buy order quantity exceeds the allowable limit" },
    {   74, "Aggregated sell order quantity exceeds the allowable limit" },
    {  200, "Collateral calculation parameters are being changed by the "
            "Trading Administrator" },
    {  201, "Collateral calculation parameters are being changed by the "
            "Trading Administrator" },
    {  202, "Collateral calculation parameters are being changed by the "
            "Trading Administrator" },
    {  203, "Collateral calculation parameters are being changed by the "
            "Trading Administrator" },
    {  204, "Collateral calculation parameters are being changed by the "
            "Trading Administrator" },
    {  205, "Collateral calculation parameters are being changed by the "
            "Trading Administrator" },
    {  206, "Collateral calculation parameters are being changed by the "
            "Trading Administrator" },
    {  207, "Collateral calculation parameters are being changed by the "
            "Trading Administrator" },
    {  208, "Collateral calculation parameters are being changed by the "
            "Trading Administrator" },
    {  310, "Unable to add order: prohibited by Clearing Administrator" },
    {  311, "Unable to open position: prohibited by Clearing Administrator" },
    {  312, "Unable to open short position: prohibited by Clearing "
            "Administrator" },
    {  314, "Unable to add orders in the client account: prohibited by the "
            "Trader" },
    {  315, "Unable to open position in the client account: prohibited by the "
            "Trader" },
    {  316, "Unable to open short position in the client account: prohibited "
            "by the Trader" },
    {  317, "Amount of buy/sell orders exceeds the limit allowed" },
    {  318, "Unable to add order for the client account: client does not have "
            "a deposit account for settlement of Money Market securities. "
            "Prohibited by Clearing Administrator" },
    {  320, "Amount of active orders exceeds the limit allowed for the client "
            "account for this security" },
    {  332, "Insufficient client funds" },
    {  333, "Insufficient Brokerage Firm funds" },
    {  334, "Insufficient Clearing Firm funds" },
    {  335, "Unable to buy: amount of securities exceeds the limit set for "
            "the client" },
    {  336, "Unable to buy: amount of securities exceeds the limit set for "
            "the Brokerage Firm" },
    {  337, "Unable to sell: amount of securities exceeds the limit set for "
            "the client" },
    {  338, "Unable to sell: amount of securities exceeds the limit set for "
            "the Brokerage Firm" },
    {  380, "Trading restricted while intraday clearing is in progress" },
    {  381, "Trading restricted while intraday clearing is in progress: "
            "cannot delete orders" },
    {  382, "Trading restricted while intraday clearing is in progress: "
            "cannot move orders" },
    {  680, "Insufficient client funds" },
    {  681, "Insufficient Clearing Firm funds" },
    { 4000, "Invalid input parameters" },
    { 4001, "Unable to perform operation: insufficient rights" },
    { 4002, "Unable to change trading limit for the client: no active trading "
            "sessions" },
    { 4004, "Unable to change trading limit for the client: client code not "
            "found" },
    { 4005, "Unable to change the trading limit for the client: insufficient "
            "funds" },
    { 4006, "Unable to set trading limit for the client: error performing "
            "operation" },
    { 4007, "Unable to set trading limit for the client: error performing "
            "operation" },
    { 4008, "Unable to set trading limit for the client: error performing "
            "operation" },
    { 4009, "Unable to set trading limit for the client: error performing "
            "operation" },
    { 4010, "Unable to set trading limit for the client: error performing "
            "operation" },
    { 4011, "Unable to set trading limit for the client: error performing "
            "operation" },
    { 4012, "Unable to set trading limit for the client: error performing "
            "operation" },
    { 4013, "Unable to set trading limit for the client: error performing "
            "operation" },
    { 4014, "Unable to change parameters: no active trading sessions" },
    { 4015, "Unable to change parameters: client code not found" },
    { 4016, "Unable to change parameters: underlying's code not found" },
    { 4017, "Unable to set trading limit for the client: amount too large" },
    { 4018, "Collateral calculation parameters are being changed by the "
            "Trading Administrator" },
    { 4021, "Unable to set requested amount of pledged funds for Clearing "
            "Firm: insufficient amount of free funds" },
    { 4022, "Unable to set requested amount of funds for Clearing Firm: "
            "insufficient amount of free funds" },
    { 4023, "Unable to change trading limit for the Brokerage Firm: no active "
            "trading sessions" },
    { 4024, "Unable to change trading limit for the Brokerage Firm: the "
            "Brokerage Firm is not registered for trading" },
    { 4025, "Unable to set requested amount of pledged funds for the "
            "Brokerage Firm: insufficient amount of free funds in the "
            "Clearing Firm" },
    { 4026, "Unable to set requested amount of funds for the Brokerage Firm: "
            "insufficient amount of free funds in the balance of the Separate "
            "Account" },
    { 4027, "Unable to set requested amount of pledged funds for the Clearing "
            "Firm: insufficient amount of pledged funds in the balance of the "
            "Separate Account" },
    { 4028, "Unable to set requested amount of funds for the Brokerage Firm: "
            "insufficient amount of free funds in the Clearing Firm" },
    { 4030, "Unable to change parameters for the Brokerage Firm: no active "
            "sessions" },
    { 4031, "Unable to change parameters for the Brokerage Firm: Brokerage "
            "Firm code not found" },
    { 4032, "Unable to change parameters for the Brokerage Firm: underlying's "
            "code not found" },
    { 4033, "Unable to change parameters for the Brokerage Firm: insufficient "
            "rights to trade this underlying" },
    { 4034, "Transfer of pledged funds from the Separate account is "
            "prohibited" },
    { 4035, "Transfer of collateral is prohibited" },
    { 4040, "Unable to change Brokerage Firm limit on risk-netting: no active "
            "sessions" },
    { 4041, "Unable to change Brokerage Firm limit on risk-netting: Brokerage "
            "Firm is not registered for trading" },
    { 4042, "Unable to change Brokerage Firm limit on risk-netting: Brokerage "
            "Firm code not found" },
    { 4043, "Unable to change Brokerage Firm limit on risk-netting: error "
            "performing operation" },
    { 4044, "Unable to change Brokerage Firm limit on risk-netting: error "
            "performing operation" },
    { 4045, "Unable to delete Brokerage Firm limit on risk-netting: error "
            "performing operation" },
    { 4046, "Unable to remove Chief Trader's restriction on trading in "
            "risk-netting instruments: insufficient rights" },
    { 4050, "Unable to process the exercise request: restricted by the Chief "
            "Trader" },
    { 4051, "Unable to process the exercise request: restricted by the "
            "Brokerage Firm" },
    { 4052, "Unable to process the exercise request: wrong client code and/or "
            "security" },
    { 4053, "Unable to process the exercise request: cannot delete orders "
            "during the intraday clearing session" },
    { 4054, "Unable to process the exercise request: cannot change orders "
            "during the intraday clearing session" },
    { 4055, "Unable to process the exercise request: order number not found" },
    { 4060, "Unable to process the exercise request: insufficient rights" },
    { 4061, "Unable to process the exercise request: deadline for submitting "
            "requests has passed" },
    { 4062, "Unable to process the exercise request: client code not found" },
    { 4063, "Unable to process the exercise request: request not found" },
    { 4064, "Unable to process the exercise request: insufficient rights" },
    { 4065, "Unable to process the exercise request: option contract not "
            "found" },
    { 4066, "Unable to process the exercise request: request to disable "
            "automatic exercise may only be submitted on the option's "
            "expiration date" },
    { 4067, "Unable to process the exercise request: error performing "
            "operation" },
    { 4068, "Unable to process the exercise request: error performing "
            "operation" },
    { 4069, "Unable to process the exercise request: error performing "
            "operation" },
    { 4070, "Unable to process the exercise request: insufficient amount of "
            "positions in the client account" },
    { 4090, "No active sessions" },
    { 4091, "Client code not found" },
    { 4092, "Underlying's code not found" },
    { 4093, "Futures contract not found" },
    { 4094, "Futures contract does not match the selected underlying" },
    { 4095, "Partial selection of futures contracts not accepted: underlying "
            "flag set 'For all'" },
    { 4096, "Unable to remove limit: no limit set" },
    { 4097, "Unable to remove: the Chief Trader's restriction cannot be "
            "removed by Brokerage Firm trader" },
    { 4098, "Security not found in the current trading session" },
    { 4099, "Both securities must have the same underlying" },
    { 4100, "Exercise date of the near leg of a multi-leg order must not be "
            "later than that of the far leg" },
    { 4101, "Unable to make a multi-leg order: lots are different" },
    { 4102, "No position to move" },
    { 4103, "The FOK order has not been fully matched" },
    { 4104, "Anonymous repo order must contain a repo type" },
    { 4105, "Order containing a repo type is restricted in this multi-leg "
            "order" },
    { 4106, "Multi-leg orders can be added only on the Money Market" },
    { 4107, "This procedure is not eligible for adding orders for multi-leg "
            "securities" },
    { 4108, "Unable to trade risk-netting instruments in T0: insufficient "
            "rights" },
    { 4109, "Rate/swap price is not a multiple of the tick size" },
    { 4110, "The near leg price differs from the settlement price" },
    { 4111, "The rate/swap price exceeds the limit allowed" },
    { 4112, "Unable to set restrictions for multi-leg futures" },
    { 4115, "Unable to transfer funds between Brokerage Firm accounts: no "
            "active sessions" },
    { 4116, "Unable to transfer funds between Brokerage Firm accounts: the "
            "donor Brokerage Firm is not registered for trading" },
    { 4117, "Unable to transfer funds between Brokerage Firms: the receiving "
            "Brokerage Firm is not registered for trading" },
    { 4118, "Broker Firm does not have sufficient amount of free funds" },
    { 4119, "Brokerage Firm does not have sufficient amount of collateral" },
    { 4120, "Insufficient amount of free funds in the balance of the Separate "
            "account" },
    { 4121, "Insufficient amount of collateral in the balance of the Separate "
            "Account" },
    { 4122, "Clearing Firm does not have sufficient amount of free funds" },
    { 4123, "Brokerage Firm does not have sufficient amount of collateral" },
    { 4124, "Brokerage Firm code not found" },
    { 4125, "Unable to transfer funds between accounts of different Clearing "
            "Firms" },
    { 4126, "Unable to transfer: error while transferring" },
    { 4128, "Brokerage firm does not have sufficient amount of free funds" },
    { 4129, "Insufficient amount of free funds in the balance of the Separate "
            "Account" },
    { 4130, "Clearing Firm does not have sufficient amount of free funds" },
    { 4131, "Brokerage Firm code not found" },
    { 4132, "Unable to withdraw: error in withdrawal logic" },
    { 4133, "No requests to cancel" },
    { 4134, "Brokerage Firm does not have sufficient amount of funds" },
    { 4135, "Clearing firm does not have sufficient amount of funds" },
    { 4136, "Prohibited to transfer pledged funds" },
    { 4137, "Brokerage Firm does not have sufficient amount of pledged funds" },
    { 4140, "Unable to transfer: position not found" },
    { 4141, "Unable to transfer: insufficient number of open positions" },
    { 4142, "Cannot transfer positions from the client account to an account "
            "with a different ITN" },
    { 4143, "Unable to transfer position: the Brokerage Firms specified "
            "belong to different Clearing Firms" },
    { 4144, "Cannot transfer positions to 'XXYY000' Brokerage Firm account" },
    { 4145, "Unable to transfer positions for the selected Brokerage Firm: "
            "restricted by the Trading Administrator" },
    { 4146, "Transferring positions in the selected securities is prohibited" },
    { 4147, "Option contract not found" }
  };

  //=========================================================================//
  // Serialisation: Session-Level Msgs:                                      //
  //=========================================================================//
  //=========================================================================//
  // Serialisation: "MsgHdr":                                                //
  //=========================================================================//
  inline ostream& operator<< (ostream& a_os, MsgHdr const& a_tmsg)
  {
    a_os <<   "BlockLength="            << a_tmsg.m_BlockLength
         << ", TemplateID="             << a_tmsg.m_TemplateID
         << ", SchemaID="               << a_tmsg.m_SchemaID
         << ", Version="                << a_tmsg.m_Version;
    return a_os;
  }

  //=========================================================================//
  // Serialisation: "Establish":                                             //
  //=========================================================================//
  ostream& operator<< (ostream& a_os, Establish const& a_tmsg)
  {
    assert(a_tmsg.m_TemplateID == Establish::s_id);

    // XXX: Be careful here, "Credentials" may not be 0-terminated:
    constexpr unsigned n = sizeof(a_tmsg.m_Credentials);
    char    buff [n+1];
    StrNCpy<true>(buff, a_tmsg.m_Credentials);

    a_os <<    "Establish("               << MsgHdr(a_tmsg)
         << ", TimeStamp="                << a_tmsg.m_TimeStamp
         << ", KeepAliveInterval="        << a_tmsg.m_KeepAliveInterval
         << ", Credentials="              << buff                     << ')';
    return a_os;
  }

  //=========================================================================//
  // Serialisation: "EstablishmentAck":                                      //
  //=========================================================================//
  ostream& operator<< (ostream& a_os, EstablishmentAck const& a_tmsg)
  {
    assert(a_tmsg.m_TemplateID == EstablishmentAck::s_id);

    a_os <<   "EstablishmentAck("         << MsgHdr(a_tmsg)
         << ", RequestTimeStamp="         << a_tmsg.m_RequestTimeStamp
         << ", KeepAliveInterval="        << a_tmsg.m_KeepAliveInterval
         << ", NextSeqNo="                << a_tmsg.m_NextSeqNo       << ')';
    return a_os;
  }

  //=========================================================================//
  // Serialisation: "EstablishmentReject":                                   //
  //=========================================================================//
  ostream& operator<< (ostream& a_os, EstablishmentReject const& a_tmsg)
  {
    assert(a_tmsg.m_TemplateID == EstablishmentReject::s_id);

    a_os <<   "EstablishmentReject("      << MsgHdr(a_tmsg)
         << ", RequestTimeStamp="         << a_tmsg.m_RequestTimeStamp
         << ", EstablishmentRejectCode="
         << int(a_tmsg.m_EstablishmentRejectCode)                     << ')';
    return a_os;
  }

  //=========================================================================//
  // Serialisation: "Terminate":                                             //
  //=========================================================================//
  ostream& operator<< (ostream& a_os, Terminate const& a_tmsg)
  {
    assert(a_tmsg.m_TemplateID == Terminate::s_id);
    a_os <<   "Terminate("                << MsgHdr(a_tmsg)
         << ", TerminationCode="          << a_tmsg.m_TerminationCode << ')';
    return a_os;
  }

  //=========================================================================//
  // Serialisation: "ReTransmitRequest":                                     //
  //=========================================================================//
  ostream& operator<< (ostream& a_os, ReTransmitRequest const& a_tmsg)
  {
    assert(a_tmsg.m_TemplateID == ReTransmitRequest::s_id);

    a_os <<    "ReTransmitRequest("       << MsgHdr(a_tmsg)
         << ", TimeStamp="                << a_tmsg.m_TimeStamp
         << ", FromSeqNo="                << a_tmsg.m_FromSeqNo
         << ", Count="                    << a_tmsg.m_Count           << ')';
    return a_os;
  }

  //=========================================================================//
  // Serialisation: "ReTransmission":                                        //
  //=========================================================================//
  ostream& operator<< (ostream& a_os, ReTransmission const& a_tmsg)
  {
    assert(a_tmsg.m_TemplateID == ReTransmission::s_id);

    a_os <<   "ReTransmission("           << MsgHdr(a_tmsg)
         << ", NextSeqNo="                << a_tmsg.m_NextSeqNo
         << ", RequestTimeStamp="         << a_tmsg.m_RequestTimeStamp
         << ", Count="                    << a_tmsg.m_Count           << ')';
    return a_os;
  }

  //=========================================================================//
  // Serialisation: "Sequence":                                              //
  //=========================================================================//
  ostream& operator<< (ostream& a_os, Sequence const& a_tmsg)
  {
    assert(a_tmsg.m_TemplateID == Sequence::s_id);
    a_os <<   "Sequence("                 << MsgHdr(a_tmsg)
         << ", NextSeqNo="                << a_tmsg.m_NextSeqNo       << ")";
    return a_os;
  }

  //=========================================================================//
  // Serialisation: "FloodReject":                                           //
  //=========================================================================//
  ostream& operator<< (ostream& a_os, FloodReject const& a_tmsg)
  {
    assert(a_tmsg.m_TemplateID == FloodReject::s_id);
    a_os <<   "FloodReject("              << MsgHdr(a_tmsg)
         << ", ClOrdID"                   << a_tmsg.m_ClOrdID
         << ", QueueSize="                << a_tmsg.m_QueueSize
         << ", PenaltyRemain="            << a_tmsg.m_PenaltyRemain   << ')';
    return a_os;
  }

  //=========================================================================//
  // Serialisation: "SessionReject":                                         //
  //=========================================================================//
  ostream& operator<< (ostream& a_os, SessionReject const& a_tmsg)
  {
    assert(a_tmsg.m_TemplateID == SessionReject::s_id);
    a_os <<   "SessionReject("            << MsgHdr(a_tmsg)
         << ", ClOrdID="                  << a_tmsg.m_ClOrdID
         << ", RefTagID="                 << a_tmsg.m_RefTagID
         << ", SessonRejectReason="       << a_tmsg.m_SessionRejectReason
         << ')';
    return a_os;
  }

  //=========================================================================//
  // Serialisation: Application-Level Request Msgs:                          //
  //=========================================================================//
  //=========================================================================//
  // Serialisation: "NewOrderSingle":                                        //
  //=========================================================================//
  ostream& operator<< (ostream& a_os, NewOrderSingle const& a_tmsg)
  {
    assert(a_tmsg.m_TemplateID == NewOrderSingle::s_id);

    // Make sure "Account" is 0-terminated:
    constexpr unsigned n = sizeof(a_tmsg.m_Account);
    char    buff [n+1];
    StrNCpy<true>(buff, a_tmsg.m_Account);

    a_os <<   "NewOrderSingle("           << MsgHdr(a_tmsg)
         << ", ClOrdID="                  << a_tmsg.m_ClOrdID
         << ", ExpireDate="               << a_tmsg.m_ExpireDate
         << ", Price="                    << a_tmsg.m_Price.m_mant
         << ", SecurityID="               << a_tmsg.m_SecurityID
         << ", ClOrdLinkID="              << a_tmsg.m_ClOrdLinkID
         << ", OrderQty="                 << a_tmsg.m_OrderQty
         << ", TimeInForce="              << int(a_tmsg.m_TimeInForce)
         << ", Side="                     << int(a_tmsg.m_Side)
         << ", CheckLimit="               << int(a_tmsg.m_CheckLimit)
         << ", Account="                  << buff                     << ')';
    return a_os;
  }

  //=========================================================================//
  // Serialisation: "NewOrderMultiLeg":                                      //
  //=========================================================================//
  ostream& operator<< (ostream& a_os, NewOrderMultiLeg const& a_tmsg)
  {
    assert(a_tmsg.m_TemplateID == NewOrderMultiLeg::s_id);

    // Make sure "Account" is 0-terminated:
    constexpr unsigned n = sizeof(a_tmsg.m_Account);
    char    buff [n+1];
    StrNCpy<true>(buff, a_tmsg.m_Account);

    a_os <<   "NewOrderMultiLeg("         << MsgHdr(a_tmsg)
         << ", ClOrdID="                  << a_tmsg.m_ClOrdID
         << ", ExpireDate="               << a_tmsg.m_ExpireDate
         << ", Price="                    << a_tmsg.m_Price.m_mant
         << ", SecurityID="               << a_tmsg.m_SecurityID
         << ", ClOrdLinkID="              << a_tmsg.m_ClOrdLinkID
         << ", OrderQty="                 << a_tmsg.m_OrderQty
         << ", TimeInForce="              << int(a_tmsg.m_TimeInForce)
         << ", Side="                     << int(a_tmsg.m_Side)
         << ", Account="                  << buff                     << ')';
    return a_os;
  }

  //=========================================================================//
  // Serialisation: "OrderCancelRequest":                                    //
  //=========================================================================//
  ostream& operator<< (ostream& a_os, OrderCancelRequest const& a_tmsg)
  {
    assert(a_tmsg.m_TemplateID == OrderCancelRequest::s_id);

    // Make sure "Account" is 0-terminated:
    constexpr unsigned n = sizeof(a_tmsg.m_Account);
    char    buff [n+1];
    StrNCpy<true>(buff, a_tmsg.m_Account);

    a_os <<   "OrderCancelRequest("       << MsgHdr(a_tmsg)
         << ", ClOrdID="                  << a_tmsg.m_ClOrdID
         << ", OrderID="                  << a_tmsg.m_OrderID
         << ", Account="                  << buff                     << ')';
    return a_os;
  }

  //=========================================================================//
  // Serialisation: "OrderReplaceRequest":                                   //
  //=========================================================================//
  ostream& operator<< (ostream& a_os, OrderReplaceRequest const& a_tmsg)
  {
    assert(a_tmsg.m_TemplateID == OrderReplaceRequest::s_id);

    // Make sure "Account" is 0-terminated:
    constexpr unsigned n = sizeof(a_tmsg.m_Account);
    char    buff [n+1];
    StrNCpy<true>(buff, a_tmsg.m_Account);

    a_os <<   "OrderReplaceRequest("      << MsgHdr(a_tmsg)
         << ", ClOrdID="                  << a_tmsg.m_ClOrdID
         << ", OrderID="                  << a_tmsg.m_OrderID
         << ", Price="                    << a_tmsg.m_Price.m_mant
         << ", OrderQty="                 << a_tmsg.m_OrderQty
         << ", ClOrdLinkID="              << a_tmsg.m_ClOrdLinkID
         << ", Mode="                     << int(a_tmsg.m_Mode)
         << ", CheckLimit="               << int(a_tmsg.m_CheckLimit)
         << ", Account="                  << buff                     << ')';
    return a_os;
  }

  //=========================================================================//
  // Serialisation: "OrderMassCancelRequest":                                //
  //=========================================================================//
  ostream& operator<< (ostream& a_os, OrderMassCancelRequest const& a_tmsg)
  {
    assert(a_tmsg.m_TemplateID == OrderMassCancelRequest::s_id);

    // Make sure "Account" and "SecurityGroup" are 0-terminated:
    constexpr unsigned n0 = sizeof(a_tmsg.m_Account);
    char    buff0[n0+1];
    StrNCpy<true>(buff0, a_tmsg.m_Account);

    constexpr unsigned n1 = sizeof(a_tmsg.m_SecurityGroup);
    char    buff1[n1+1];
    StrNCpy<true>(buff1, a_tmsg.m_SecurityGroup);

    a_os <<   "OrderMassCancelRequest("   << MsgHdr(a_tmsg)
         << ", ClOrdID="                  << a_tmsg.m_ClOrdID
         << ", ClOrdLinkID="              << a_tmsg.m_ClOrdLinkID
         << ", SecurityID="               << a_tmsg.m_SecurityID
         << ", SecurityType="             << int(a_tmsg.m_SecurityType)
         << ", Side="                     << int(a_tmsg.m_Side)
         << ", Account="                  << buff0
         << ", SecurityGroup="            << buff1                    << ')';
    return a_os;
  }

  //=========================================================================//
  // Serialisation: Application-Level Responses:                             //
  //=========================================================================//
  //=========================================================================//
  // Serialisation: "NewOrderSingleResponse":                                //
  //=========================================================================//
  ostream& operator<< (ostream& a_os, NewOrderSingleResponse const& a_tmsg)
  {
    assert(a_tmsg.m_TemplateID == NewOrderSingleResponse::s_id);

    a_os <<   "NewOrderSingleResponse("   << MsgHdr(a_tmsg)
         << ", ClOrdID="                  << a_tmsg.m_ClOrdID
         << ", TimeStamp="                << a_tmsg.m_TimeStamp
         << ", ExpireDate="               << a_tmsg.m_ExpireDate
         << ", OrderID="                  << a_tmsg.m_OrderID
         << ", Flags="                    << a_tmsg.m_Flags
         << ", Price="                    << a_tmsg.m_Price.m_mant
         << ", SecurityID="               << a_tmsg.m_SecurityID
         << ", OrderQty="                 << a_tmsg.m_OrderQty
         << ", TradingSessionID="         << a_tmsg.m_TradingSessionID
         << ", ClOrdLinkID="              << a_tmsg.m_ClOrdLinkID
         << ", Side="                     << int(a_tmsg.m_Side)       << ')';
    return a_os;
  }

  //=========================================================================//
  // Serialisation: "NewOrderMultiLegResponse":                              //
  //=========================================================================//
  ostream& operator<< (ostream& a_os, NewOrderMultiLegResponse const& a_tmsg)
  {
    assert(a_tmsg.m_TemplateID == NewOrderMultiLegResponse::s_id);

    a_os <<   "NewOrderMultiLegResponse(" << MsgHdr(a_tmsg)
         << ", ClOrdID="                  << a_tmsg.m_ClOrdID
         << ", TimeStamp="                << a_tmsg.m_TimeStamp
         << ", ExpireDate="               << a_tmsg.m_ExpireDate
         << ", OrderID="                  << a_tmsg.m_OrderID
         << ", Flags="                    << a_tmsg.m_Flags
         << ", Price="                    << a_tmsg.m_Price.m_mant
         << ", SecurityID="               << a_tmsg.m_SecurityID
         << ", OrderQty="                 << a_tmsg.m_OrderQty
         << ", TradingSessionID="         << a_tmsg.m_TradingSessionID
         << ", ClOrdLinkID="              << a_tmsg.m_ClOrdLinkID
         << ", Side="                     << int(a_tmsg.m_Side)       << ')';
    return a_os;
  }

  //=========================================================================//
  // Serialisation: "NewOrderReject":                                        //
  //=========================================================================//
  ostream& operator<< (ostream& a_os, NewOrderReject const& a_tmsg)
  {
    assert(a_tmsg.m_TemplateID == NewOrderReject::s_id);

    a_os <<   "NewOrderReject("           << MsgHdr(a_tmsg)
         << ", ClOrdID="                  << a_tmsg.m_ClOrdID
         << ", TimeStamp="                << a_tmsg.m_TimeStamp
         << ", OrdRejReason="             << a_tmsg.m_OrdRejReason    << ')';
    return a_os;
  }

  //=========================================================================//
  // Serialisation: "OrderCancelResponse":                                   //
  //=========================================================================//
  ostream& operator<< (ostream& a_os, OrderCancelResponse const& a_tmsg)
  {
    assert(a_tmsg.m_TemplateID == OrderCancelResponse::s_id);

    a_os << "  OrderCancelResponse("      << MsgHdr(a_tmsg)
         << ", ClOrdID="                  << a_tmsg.m_ClOrdID
         << ", TimeStamp="                << a_tmsg.m_TimeStamp
         << ", OrderID="                  << a_tmsg.m_OrderID
         << ", Flags="                    << a_tmsg.m_Flags
         << ", OrderQty="                 << a_tmsg.m_OrderQty
         << ", TradingSessionID="         << a_tmsg.m_TradingSessionID
         << ", ClOrdLinkID="              << a_tmsg.m_ClOrdLinkID     << ')';
    return a_os;
  }

  //=========================================================================//
  // Serialisation: "OrderCancelReject":                                     //
  //=========================================================================//
  ostream& operator<< (ostream& a_os, OrderCancelReject const& a_tmsg)
  {
    assert(a_tmsg.m_TemplateID == OrderCancelReject::s_id);

    a_os <<   "OrderCancelReject("        << MsgHdr(a_tmsg)
         << ", ClOrdID="                  << a_tmsg.m_ClOrdID
         << ", TimeStamp="                << a_tmsg.m_TimeStamp
         << ", OrdRejReason="             << a_tmsg.m_OrdRejReason    << ')';
    return a_os;
  }

  //=========================================================================//
  // Serialisation: "OrderReplaceResponse":                                  //
  //=========================================================================//
  ostream& operator<< (ostream& a_os, OrderReplaceResponse const& a_tmsg)
  {
    assert(a_tmsg.m_TemplateID == OrderReplaceResponse::s_id);

    a_os <<   "OrderReplaceResponse("     << MsgHdr(a_tmsg)
         << ", ClOrdID="                  << a_tmsg.m_ClOrdID
         << ", TimeStamp="                << a_tmsg.m_TimeStamp
         << ", OrderID="                  << a_tmsg.m_OrderID
         << ", PrevOrderID="              << a_tmsg.m_PrevOrderID
         << ", Flags="                    << a_tmsg.m_Flags
         << ", Price="                    << a_tmsg.m_Price.m_mant
         << ", OrderQty="                 << a_tmsg.m_OrderQty
         << ", TradingSessionID="         << a_tmsg.m_TradingSessionID
         << ", ClOrdLinkID="              << a_tmsg.m_ClOrdLinkID     << ')';
    return a_os;
  }

  //=========================================================================//
  // Serialisation: "OrderReplaceReject":                                    //
  //=========================================================================//
  ostream& operator<< (ostream& a_os, OrderReplaceReject const& a_tmsg)
  {
    assert(a_tmsg.m_TemplateID == OrderReplaceReject::s_id);

    a_os <<   "OrderReplaceReject("       << MsgHdr(a_tmsg)
         << ", ClOrdID="                  << a_tmsg.m_ClOrdID
         << ", TimeStamp="                << a_tmsg.m_TimeStamp
         << ", OrdRejReason="             << a_tmsg.m_OrdRejReason    << ')';
    return a_os;
  }

  //=========================================================================//
  // Serialisation: "OrderMassCancelResponse":                               //
  //=========================================================================//
  ostream& operator<< (ostream& a_os, OrderMassCancelResponse const& a_tmsg)
  {
    assert(a_tmsg.m_TemplateID == OrderMassCancelResponse::s_id);

    a_os <<   "OrderMassCancelResponse("  << MsgHdr(a_tmsg)
         << ", ClOrdID="                  << a_tmsg.m_ClOrdID
         << ", TimeStamp="                << a_tmsg.m_TimeStamp
         << ", TotalAffectedOrders="      << a_tmsg.m_TotalAffectedOrders
         << ", OrdRejectReason="          << a_tmsg.m_OrdRejReason    << ')';
    return a_os;
  }

  //=========================================================================//
  // Serialisation: "ExecutionSingleReport":                                 //
  //=========================================================================//
  ostream& operator<< (ostream& a_os, ExecutionSingleReport const& a_tmsg)
  {
    assert(a_tmsg.m_TemplateID == ExecutionSingleReport::s_id);

    a_os <<   "ExecutionSingleReport("    << MsgHdr(a_tmsg)
         << ", ClOrdID="                  << a_tmsg.m_ClOrdID
         << ", TimeStamp="                << a_tmsg.m_TimeStamp
         << ", OrderID="                  << a_tmsg.m_OrderID
         << ", TrdMatchID="               << a_tmsg.m_TrdMatchID
         << ", Flags="                    << a_tmsg.m_Flags
         << ", LastPx="                   << a_tmsg.m_LastPx.m_mant
         << ", LastQty="                  << a_tmsg.m_LastQty
         << ", OrderQty="                 << a_tmsg.m_OrderQty
         << ", TradingSessionID="         << a_tmsg.m_TradingSessionID
         << ", ClOrdLinkID="              << a_tmsg.m_ClOrdLinkID
         << ", SecurityID="               << a_tmsg.m_SecurityID
         << ", Side="                     << int(a_tmsg.m_Side)       << ')';
    return a_os;
  }

  //=========================================================================//
  // Serialisation: "ExecutionMultiLegReport":                               //
  //=========================================================================//
  ostream& operator<< (ostream& a_os, ExecutionMultiLegReport const& a_tmsg)
  {
    assert(a_tmsg.m_TemplateID == ExecutionMultiLegReport::s_id);

    a_os <<   "ExecutionMultiLegReport("  << MsgHdr(a_tmsg)
         << ", ClOrdID="                  << a_tmsg.m_ClOrdID
         << ", TimeStamp="                << a_tmsg.m_TimeStamp
         << ", OrderID="                  << a_tmsg.m_OrderID
         << ", TrdMatchID="               << a_tmsg.m_TrdMatchID
         << ", Flags="                    << a_tmsg.m_Flags
         << ", LastPx="                   << a_tmsg.m_LastPx.m_mant
         << ", LegPrice="                 << a_tmsg.m_LegPrice.m_mant
         << ", LastQty="                  << a_tmsg.m_LastQty
         << ", OrderQty="                 << a_tmsg.m_OrderQty
         << ", TradingSessionID="         << a_tmsg.m_TradingSessionID
         << ", ClOrdLinkID="              << a_tmsg.m_ClOrdLinkID
         << ", SecurityID="               << a_tmsg.m_SecurityID
         << ", Side="                     << int(a_tmsg.m_Side)       << ')';
    return a_os;
  }

  //=========================================================================//
  // Serialisation: "EmptyBook":                                             //
  //=========================================================================//
  ostream& operator<< (ostream& a_os, EmptyBook const& a_tmsg)
  {
    assert(a_tmsg.m_TemplateID == EmptyBook::s_id);

    a_os <<   "EmptyBook("                << MsgHdr(a_tmsg)
         << ", TimeStamp="                << a_tmsg.m_TimeStamp
         << ", TradingSessionID="         << a_tmsg.m_TradingSessionID
         << ')';
    return a_os;
  }

  //=========================================================================//
  // Serialisation: "SystemEvent":                                           //
  //=========================================================================//
  ostream& operator<< (ostream& a_os, SystemEvent const& a_tmsg)
  {
    assert(a_tmsg.m_TemplateID == SystemEvent::s_id);

    a_os <<   "SystemEvent("              << MsgHdr(a_tmsg)
         << ", TimeStamp="                << a_tmsg.m_TimeStamp
         << ", TradingSessionID="         << a_tmsg.m_TradingSessionID
         << ", TradSesEvent="             << int(a_tmsg.m_TradSesEvent)
         << ')';
    return a_os;
  }
} // End namespace TWIME
} // End namespace MAQUETTE
