#! /usr/bin/python3 -O
#============================================================================#
#                         "Tools/MkSecDefs-BitMEX.py":                      #
#============================================================================#
# Download the ExchangeInfo:
import requests
import time

# fmt = '{%i, "%s", "%s", "", "%s",\n\
# "BitMEX", "", "SPOT", "", "%s", "%s", %s, 1.0, %s, 1.0, %s,\n\
# \'A\', 1.0, 0, 0, 0.0, 0, ""\n},'
fmt = '    { %i, "%s", "%s", "", "%s",\n'\
    '      "BitMEX", "", "SPOT", "",\n' \
    '      "%s", "%s", ' \
    '%s, 1.0, %s, 1, %s, \'A\',1.0, 0, 0, 0.0, 0, ""\n    },'

secId = 0
count = 500
start = 0

loop = True
while loop:
    r = requests.get(
            f"https://www.bitmex.com/api/v1/instrument?count={count}&start={start}")
    j = r.json()
    if len(j) < count: loop = False
    for s in j:
        if s["state"] != "Open":
            continue

        symbol = s["symbol"]
        # Skip fully synthetic ETHUSD for now
        instrCode = s["typ"]
        a = s["underlying"] if s["underlying"] != "XBT" else "BTC"
        b = s["quoteCurrency"]
        qtyCurr = "\'A\'" if symbol != "XBTUSD" else "\'B\'"
        lotSz = s["lotSize"] or 1
        pxStep = s["tickSize"]

        start += count
        print(fmt % (secId, symbol, symbol.lower(),
                instrCode, a, b, qtyCurr, lotSz, pxStep))
        secId += 1
exit(0)
