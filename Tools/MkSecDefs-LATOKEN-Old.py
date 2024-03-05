#! /usr/bin/python -O
# vim:ts=2:noet
#=============================================================================#
#                          "Tools/MkSecDefs-LATOKEN.py":                      #
#=============================================================================#
# Download the ExchangeInfo:
import requests
r   = requests.get("https://wallet.latoken.com/api/v2/pairs")
js  = r.json()

fmt = '    { %d, "%s", "", "%s", "MRCXXX", "LATOKEN", "",\n' \
      '      "SPOT", "", "%s", "%s", \'A\', 1.0, %s, %d, %s, \'A\', 1.0,\n' \
      '      0, 0, 0.0, 0, ""\n    },'

for r in js:
		secID  = r['id']
		symbol = r['symbol']
		descr  = r['name']

		# Get the A and B Ccys (their corresp numerical IDs are available from the
		# JSON msg, but we do not use them here):
		pair   = symbol.split("/")
		a      = pair[0]
		b      = pair[1]

		# LotSize is given as the number of decimal places:
		lotSzDec = r['amountTick']
		if lotSzDec == 0:
				lotSzStr = "1.0"
		else:
				lotSzStr = "1e-%d" % lotSzDec

		minSzLots  = r['minTokensInOrder'] * pow(10, lotSzDec)

		# PxStep is given as the number of decimal places:
		pxDec    = r['priceTick']
		if pxDec == 0:
				pxStepStr = "1.0"
		else:
				pxStepStr = "1e-%d" % pxDec

		print(fmt % (secID, symbol, descr, a, b, lotSzStr, minSzLots, pxStepStr))

exit(0)
