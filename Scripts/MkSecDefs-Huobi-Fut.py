#! /usr/bin/python -O
#============================================================================#
#                         "Tools/MkSecDefs-Huobi-Fut.py":                    #
#============================================================================#
# Download symbols info for Huobi Futures market:
import requests
r = requests.get("https://api.hbdm.com/api/v1/contract_contract_info")
j = r.json()
symbols = j["data"]

market = "Huobi"
cfi = "FXXXXX"
b = "usdt"
contractTypes = {"this_week": "_CW", "next_week": "_NW", "quarter": "_CQ"}

fmt = '    { 0, "%s", "%s", "", "%s", "%s", "%s", "", "",\n' \
      '      "%s", "%s", \'A\', 1.0, %s, %d, %s, \'A\', 1.0,\n' \
      '      0, 0, 0.0, 0, ""\n    },'

# In Huobi futures Symbol is currency code (BTC), but all market/trades channels
# use Symbol_ContractType (BTC_CW) for subscriptions. We put it to altSymbol field
for s in symbols:
    symbol  = s["symbol"]
    a       = symbol.lower()
    contract= s["contract_type"]
    lotSz   = s["contract_size"]
    minLots = 1
    pxStep  = s["price_tick"]

    print(fmt % (symbol, symbol + contractTypes[contract], cfi, market, contract, a, b, lotSz, minLots, pxStep))

exit(0)

