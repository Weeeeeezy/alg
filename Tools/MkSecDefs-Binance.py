#! /usr/bin/python3 -O
#============================================================================#
#                         "Tools/MkSecDefs-Binance.py":                      #
#============================================================================#
# FIXME: This script is not fully up-to-date; its output requires manual post-
# processing:
#
# Download the ExchangeInfo:
import requests
import datetime as dt

mode = "FutC"
fmt = '    { 0, "%s", "%s", "", "MRCXXX", "Binance", "%s", "%s", "",\n' \
      '      "%s", "%s", \'A\', %d, %s, %d, %s, \'A\', 1.0,\n' \
      '      %i, %i, 0.0, 0, "%s"\n    },'

if mode == "Spt":
  # Spot:
  r = requests.get("https://api.binance.com/api/v1/exchangeInfo")
elif mode == "FutT":
  # USDT-settled Futures:
  r = requests.get("https://fapi.binance.com/fapi/v1/exchangeInfo")
elif mode == "FutC":
  # COIN-settled Futures
  r = requests.get("https://dapi.binance.com/dapi/v1/exchangeInfo")
else:
  print("Unknown mode '%s'" % mode)
  exit(1)

j = r.json()
symbols = j["symbols"]

for s in symbols:
  if (mode != "FutC") and (s["status"] != "TRADING"):
    continue

  symbol  = s["symbol"]
  ft      = "" if (mode == "Spt") else "FUT"
  t       = "SPOT" if (mode == "Spt") else (s["contractType"] if (mode == "FutC") else "")
  a       = s["baseAsset"]
  b       = s["quoteAsset"]
  pair    = s["pair"] if (mode == "FutC") else ""
  contractSz = float(s["contractSize"]) if (mode == "FutC") else 1.0
  lotSz   = None
  minLots = None
  pxStep  = None

  if (mode == "FutC"):
    deliv = int(s["deliveryDate"]) / 1000 # it's in milliseconds
    exp = dt.datetime.utcfromtimestamp(deliv)
    exp_date = 10000 * exp.year + 100 * exp.month + exp.day
    exp_time = 3600 * exp.hour + 60 * exp.minute + exp.second
  else:
    exp_date = 0
    exp_time = 0

  filters = s["filters"]
  for f in filters:
      if   f["filterType"] == "PRICE_FILTER":
          pxStep  = f["tickSize"]
      elif f["filterType"] == "LOT_SIZE":
          lotSz   = f["stepSize"]
          minLots = float(f["minQty"]) / float(lotSz)

  print(fmt % (symbol, symbol.lower(), ft, t, a, b, contractSz, lotSz, minLots,
                pxStep, exp_date, exp_time, pair))

exit(0)

