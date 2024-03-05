#! /usr/bin/python3 -O
#============================================================================#
#                         "Tools/MkSecDefs-Kraken.py":                       #
#============================================================================#
# Download the ExchangeInfo:
import requests
import datetime as dt
import sys

mode = "Spot"

fmt = '    { 0, "%s", "%s", "", "MRCXXX", "Kraken' + mode + '", "%s", "%s", "",\n' \
      '      "%s", "%s", \'A\', %d, %s, %d, %s, \'A\', 1.0,\n' \
      '      %i, %i, 0.0, 0, "%s"\n    },'

if mode == "Spot":
  # Spot
  r = requests.get("https://api.kraken.com/0/public/AssetPairs")
else:
  print("Unknown mode '%s'" % mode)
  exit(1)

j = r.json()
symbols = j["result"]

for key in symbols:
    s = symbols[key]
    if not ("wsname" in s):
        continue

    symbol  = s["wsname"]
    ft      = "" if (mode == "Spot") else "FUT"
    t       = "SPOT" if (mode == "Spot") else ""
    a       = s["base"]
    b       = s["quote"]
    pair    = ""
    contractSz = 1.0
    lotSz   = '1.0'
    if s["lot"] != "unit":
      print("Error: non-unit lot for %s" % key)
      sys.exit(1)

    order_min = float(s["ordermin"])
    minLots = int(order_min) if order_min > 1.0 else 1
    pxStep  = '1e-%i' % s["pair_decimals"]

    exp_date = 0
    exp_time = 0

    print(fmt % (symbol, key, ft, t, a, b, contractSz, lotSz, minLots,
                 pxStep, exp_date, exp_time, pair))

exit(0)

