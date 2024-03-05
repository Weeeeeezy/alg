#! /usr/bin/python3 -O
#============================================================================#
#                        "Tools/MkSecDefs-Huobi.py":                         #
#============================================================================#
# Download the ExchangeInfo:
import requests
import datetime as dt
import sys
import numpy as np

fmt = '    { 0, "%s", "%s", "", "MRCXXX", "Huobi", "%s", "%s", "",\n' \
      '      "%s", "%s", \'A\', %d, %s, %d, %s, \'A\', 1.0,\n' \
      '      %i, %i, 0.0, 0, "%s"\n    },'

if len(sys.argv) != 2:
  print("Usage: %s [spot|future|coin_swap|usdt_swap]" % sys.argv[0])
  sys.exit(1)

mode = sys.argv[1]

is_spt = (mode == "spot")
is_fut = (mode == "future")
is_swp = (mode == "coin_swap") or (mode == "usdt_swap")

if mode == "spot":
  r = requests.get("https://api.huobi.pro/v1/common/symbols")
elif mode == "future":
  r = requests.get("https://api.hbdm.com/api/v1/contract_contract_info")
elif mode == "coin_swap":
  r = requests.get("https://api.hbdm.com/swap-api/v1/swap_contract_info")
elif mode == "usdt_swap":
  r = requests.get("https://api.hbdm.com/linear-swap-api/v1/swap_contract_info")
else:
  print("Unknown mode '%s'" % mode)
  exit(1)

j = r.json()
symbols = j["data"]

keys = [s["symbol"] for s in symbols]
idxs = np.argsort(keys)

for idx in idxs:
  s = symbols[idx]
  if not is_spt and (s["contract_status"] != 1):
    continue

  symbol    = s["symbol"]
  tenor     = ""
  exp_date  = 0
  exp_time  = 0
  pair      = ""

  if is_spt:
    if "underlying" in s:
      continue
    if s["state"] != "online":
      continue

    alt_sym = symbol
    segment = "SPT"
    a       = s["base-currency"]
    b       = s["quote-currency"]

    contractSz  = 1.0
    lotSz   = "1.0"
    minLots = s["min-order-amt"]
    pxStep  = "1e-%d" % s["price-precision"]

  elif is_fut:
    if s["contract_status"] != 1:
      continue

    tenor   = s["contract_type"]
    codes = {"this_week": "CW", "next_week": "NW", "quarter": "CQ", "next_quarter": "NQ"}
    segment = "FUT_%s" % codes[tenor]
    alt_sym = "%s_%s" % (symbol, codes[tenor])
    a       = symbol.lower()
    b       = "usdt"

    contractSz  = 1.0
    lotSz   = s["contract_size"]
    minLots = 1
    pxStep  = s["price_tick"]

  elif is_swp:
    if s["contract_status"] != 1:
      continue

    alt_sym = s["contract_code"]
    segment = "SWP-COIN" if (mode == "coin_swap") else "SWP-USDT"
    tenor   = "PERP"
    a       = symbol.lower()
    b       = "usd" if (mode == "coin_swap") else "usdt"

    contractSz  = 1.0
    lotSz   = s["contract_size"]
    minLots = 1
    pxStep  = s["price_tick"]

  print(fmt % (symbol, alt_sym, segment, tenor, a, b, contractSz, lotSz, minLots,
               pxStep, exp_date, exp_time, pair))

exit(0)

