#! /usr/bin/python -O
#=============================================================================#
#                        "Tools/MkSecDefs-BitFinex.py":                       #
#=============================================================================#
import requests
import sys

#-----------------------------------------------------------------------------#
# Spot:                                                                       #
#-----------------------------------------------------------------------------#
r    = requests.get("https://api.bitfinex.com/v1/symbols_details")
spot = r.json()

# NB: "Symbol" is prefixed with 't', "AltSymbol" is not:
fmt  = '    { 0, "t%s", "%s", "", "MRCXXX", "BitFinex", "", "SPOT", "",\n' \
       '      "%s", "%s", ' \
       '\'A\', 1.0, %s, 1, 1e-%s, \'A\', 1.0, 0, 0, 0.0, 0, ""\n    },'

# allCcys = {}

def rewriteCcy(ccy):
    res = ccy
    if res[-2:] == "F0":
        res =  res[:-2]
    if res == "UST":
        res = "USDT"
    return res

for s in spot:
    symbol  = s['pair']
    lotSz   = s['minimum_order_size']
    pxPrec  = s['price_precision']
    onMarg  = s['margin']

    # XXX: Symbol needs to be post-processed! First, capitalise it:
    symbol  = symbol.upper()

    # Need to extract 'A' and 'B' ccys from the symbol:
    if len(symbol) == 6:
        # First of all, if "symbol" is of length 6, then each 3-char part
        # stands for a ccy ('A' and 'B'):
        a  = symbol[:3]
        b  = symbol[3:]
    else:
        # Otherwise, there must be an explicit ':' separator in "symbol":
        ab = symbol.split(':')
        if len(ab) != 2:
            sys.stderr.write("%s: Cannot get A/B ccys\n" % symbol)
            next
        a  = ab[0]
        b  = ab[1]

    # Perform some re-writing (XXX: we do not have the exact mapping of BitFinex
    # ccy names to other exchanges' ccy names, the following is only a guess):
    a = rewriteCcy(a)
    b = rewriteCcy(b)

    # Store these Ccys:
#   allCcys[a] = None
#   allCcys[b] = None

    print(fmt % (symbol, symbol, a, b, lotSz, pxPrec))

#for ccy in allCcys:
#    print(ccy)

exit(0)
