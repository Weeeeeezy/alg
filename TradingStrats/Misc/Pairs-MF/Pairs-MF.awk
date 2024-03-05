# vim:ts=2:et
#============================================================================#
#                  "TradingStrats/Pairs-MF/Pairs-MF.awk":                    #
#============================================================================#
BEGIN{
  FS      = ", ";
  PnL     = 0.0;
  PassSlip= 0.0;
  AggrSlip= 0.0;
  AggrQty = 0.0;
  TS      = 0.0;
  CoverTS = 0.0;
  PassLat = 0.0;
  PassLatEMA = "NaN";
}

{
  # Compute the TimeStamp if it is available:
  if ($1 ~ /^\[/)
  {
    StrTS  = substr($1, 2, 23);
    Hour   = substr(StrTS, 12, 2);
    Min    = substr(StrTS, 15, 2);
    Sec    = substr(StrTS, 18, 6);
    TS     = (Hour - 10) * 3600 + Min * 60 + Sec;
  }

  if ($1 ~ / FILL:/)
  {
    Symbol = substr($3,  8);
    Side   = substr($4,  6);
    Px     = substr($5,  4);
    FillTS = TS;
    LatMS  = 0;

    if ($1 ~ /PASSIVE FILL:/)
    {
      ExpPx     = substr($6, 11);              # This is ExpPxLast
      AggrLots  = substr($7, 10);
      Qty       = substr($8, 5);
      # Slip is POSITIVE when it is indeed a loss:
      Slip      = (Side == "Bid") ? (Px - ExpPx) : (ExpPx - Px);
      PassSlip += Qty * Slip;                  # Compatible with AggrSlip
      Slip     *= 1000.0;                      # For reporting only
      LatMS     = PassLatEMA * 1000.0;         # Last passive latency is msec
    }
    else # Aggressive Fill:
    {
      ExpPx     = substr($7,  11);             # This is ExpPxLast
      AggrLots  = substr($9,  10);
      Qty       = substr($10, 5);              # In Contracts
      AggrQty  += Qty;                         # In Futures pts
      # Slip is POSITIVE when it is indeed a loss:
      Slip      = (Side == "Bid") ? (Px - ExpPx) : (ExpPx - Px);
      AggrSlip += Qty * Slip;
      LatMS     = (FillTS - CoverTS) * 1000.0; # In milliseconds
    }

    if (Side == "Ask")
    {
      Pos[Symbol] -= Qty;
      PnL         += Qty * Px;
    }
    else
    if (Side == "Bid")
    {
      Pos[Symbol] += Qty;
      PnL         -= Qty * Px;
    }
    printf "%s,%.3f,%s,%s,%f,%f,%d,%d,%f\n",
           StrTS,FillTS, Symbol, Side, Px, ExpPx, Qty, AggrLots, Slip; #, LatMS;

  }
  else
  if ($1 ~ /TRADED: /)
  {
    # This is for ManualTrading outputs (manually inserted into the Log):
    split($1, Flds, ": ");
    Symbol =  Flds[2];
    split(Flds[3], SFlds, " ");
    Side   =  SFlds[1];
    Qty    =  SFlds[2];
    Px     =  SFlds[4];

    if (Side == "S")
    {
      Pos[Symbol] -= Qty;
      PnL         += Qty * Px;
    }
    else
    if (Side == "B")
    {
      Pos[Symbol] += Qty;
      PnL         -= Qty * Px;
    }
  }
  else
  if ($1 ~ /AggrCover/)
    # A CoveringOrder was issued at this point:
    CoverTS = TS;
  else
  if ($1 ~ /Quoted:/)
  {
    # This is used to estimate Order Modification latencies:
    match($1, /^Quoted: ([0-9]+)@[an\.0-9]+ \.\. [an\.0-9]+@([0-9]+)$/, m);

    BidID = substr($1, m[1, "start"], m[1, "length"]);
    AskID = substr($1, m[2, "start"], m[2, "length"]);

    # If this is the 1st appearance of this BidID (it has just been quoted),
    # memoise the TimeStamp:
    if (BidID != 0 && QuoteTS[BidID] == "")
      QuoteTS[BidID] = TS;
    if (AskID != 0 && QuoteTS[AskID] == "")
      QuoteTS[AskID] = TS;
  }
  else
  if ($1 ~ /OnConfirm: ReqID=/)
  {
    # The order has been confirmed -- try to get the Confirmation Latency:
    match($1, /ReqID=([0-9]+)$/, m);
    ReqID = substr($1, m[1, "start"], m[1, "length"]);
    if (QuoteTS[ReqID] != "")
    {
      PassLat = TS - QuoteTS[ReqID];

      # We use EMA smooting of PassLat:
      if (PassLatEMA == -1000000)
        PassLatEMA = PassLat;
      else
        PassLatEMA = 0.8 * PassLatEMA + 0.2 * PassLat;
    }
  }
}

END{
  for (p in Pos)
    print "Pos[", p, "]  = ", Pos[p]      > "/dev/stderr";
  print "Total Contracts = ", AggrQty     > "/dev/stderr";
  print "PnL             = ", PnL         > "/dev/stderr";

  if (AggrQty != 0)
  {
    RelPnL      = PnL      / AggrQty;
    RelPassSlip = PassSlip / AggrQty;
    RelAggrSlip = AggrSlip / AggrQty;

    print "PnL/Contract    = ", RelPnL      > "/dev/stderr";
    print "Avg.PassSlip    = ", RelPassSlip > "/dev/stderr";
    print "Avg.AggrSlip    = ", RelAggrSlip > "/dev/stderr";
  }
}
