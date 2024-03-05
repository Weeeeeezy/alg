#! /usr/bin/python -O
#============================================================================#
#                           "Tools/MkSecDefs-OKEX.py":                       #
#============================================================================#
import requests

#----------------------------------------------------------------------------#
# Spot:                                                                      #
#----------------------------------------------------------------------------#
r    = requests.get("https://www.okex.com/api/spot/v3/instruments")
spot = r.json()

fmtS = '    { 0, "%s", "", "", "MRCXXX", "OKEX", "", "SPOT", "", "%s", "%s"' \
       ',\n'  \
       '      \'A\', 1.0, %s, %d, %s, \'A\', 1.0, 0, 0, 0.0, 0, ""\n    },'

print ('    //-------------------------------------------------------------' \
       '----------//')
print ('    // Spot:                                                       ' \
       '          //')
print ('    //-------------------------------------------------------------' \
       '----------//')
for s in spot:
    symbol  = s['instrument_id']
    a       = s['base_currency']
    b       = s['quote_currency']
    lotSz   = s['size_increment']
    minLots = float(s['min_size']) / float(lotSz)
    pxStep  = s['tick_size']
    print(fmtS % (symbol, a, b, lotSz, minLots, pxStep))

#----------------------------------------------------------------------------#
# Futures:                                                                   #
#----------------------------------------------------------------------------#
r    = requests.get("https://www.okex.com/api/futures/v3/instruments")
futs = r.json()

# NB: Futures expiration time is 16:00 HK (GMT+8), ie 08:00 GMT (no DST);
# that is, 28800 sec on the expiration date:
fmtF = '    { 0, "%s", "", "", "FXXXXX", "OKEX", "FUT", "%s", "",\n' \
       '      "%s", "%s",\n' \
       '      \'B\', %s, %s, 1, %s, \'A\', 1.0, %s, 28800, 0.0, 0, ""\n    },'

print ('    //-------------------------------------------------------------' \
       '----------//')
print ('    // Futures:                                                    ' \
       '          //')
print ('    //-------------------------------------------------------------' \
       '----------//')
for f in futs:
    symbol    = f['instrument_id']
    a         = f['underlying_index']
    b         = f['quote_currency']
    contrQtyB = f['contract_val']
    lotSz     = f['trade_increment']
    pxStep    = f['tick_size']
    expire    = f['delivery'].replace('-', '')
    # NB: We install the shortened ExpirDate (w/o the "20" prefix) as Tenor:
    tenor     = expire[2:]

    print(fmtF % (symbol, tenor, a, b, contrQtyB, lotSz, pxStep, expire))

#-----------------------------------------------------------------------------#
# Perpetual Swaps / Futures:                                                  #
#-----------------------------------------------------------------------------#
# XXX: In OKEX terminology, they are called "Swaps", but we simply treat them
# as Futures w/o Expiration time:
#
r     = requests.get("https://www.okex.com/api/swap/v3/instruments")
perps = r.json()

# NB: We install "+oo" for the Tenor:
fmtP = '    { 0, "%s", "", "", "FXXXXX", "OKEX", "PERP", "+oo", "",\n' \
       '      "%s", "%s", ' \
       '\'B\', %s, %s, 1, %s, \'A\', 1.0, 0, 0, 0.0, 0,  ""\n    },'

print ('    //-------------------------------------------------------------' \
       '----------//')
print ('    // Perpetuals:                                                 ' \
       '          //')
print ('    //-------------------------------------------------------------' \
       '----------//')
for p in perps:
    symbol    = p['instrument_id']
    a0        = p['underlying_index']
    a1        = p['coin']
    b         = p['quote_currency']
    contrQtyB = p['contract_val']
    lotSz     = p['size_increment']
    pxStep    = p['tick_size']

    if a0 != a1:
        print ('WARNING: Symbol=%s: UndIdx=%s, Coin=%s' % symbol, a0, a1)
        next

    print(fmtP % (symbol, a0, b, contrQtyB, lotSz, pxStep))

exit(0)
