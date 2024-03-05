#! /usr/bin/python -O
#============================================================================#
#                         "Tools/MkSecDefs-Huobi-Fut.py":                    #
#============================================================================#
# Download symbols info for Huobi Futures market:
import requests
r = requests.get("https://api.hbdm.com/swap-api/v1/swap_contract_info")
j = r.json()
symbols = j["data"]

market = "Huobi"
cfi = "MRCXXX"

fmt = '    { 0, "%s", "%s", "", "%s", "%s", "SWP", "", "",\n' \
      '      "%s", "%s", \'A\', 1.0, %s, %d, %s, \'A\', 1.0,\n' \
      '      0, 0, 0.0, 0, ""\n    },'

# In Huobi futures Symbol is currency code (BTC), but all market/trades channels
# use Symbol_ContractType (BTC_CW) for subscriptions. We put it to altSymbol field
for s in symbols:
    symbol  = s["symbol"]
    altSymbol= s["contract_code"]
    status=s["contract_status"]
    lotSz   = s["contract_size"]
    minLots = 1
    pxStep  = s["price_tick"]
    ab = altSymbol.split('-')
    if status == 1:
        print(fmt % (symbol, altSymbol, cfi, market, ab[0], ab[1], lotSz, minLots, pxStep))

exit(0)

