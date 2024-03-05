#! /usr/bin/python3 -O
#=============================================================================#
#                          "Tools/MkSecDefs-LMAX.py":                         #
#=============================================================================#
import sys
import csv

if len(sys.argv) != 2:
    print("Usage: %s <csv file>" % sys.argv[0])

fmt  = '    { %6i, "%s", "", "", "MRCXXX", "LMAX", "", "SPOT", "",\n' \
       '      "%s", "%s", ' \
       '\'A\', %.1f, %.1f, 1, %.0e, \'A\', 1.0, 0, 79200, 0.0, 0, ""\n    },'

with open(sys.argv[1], 'r') as csvfile:
  reader = csv.reader(csvfile)

  first = True
  for row in reader:
      if first:
          # skip header row
          first = False
          continue

      id = int(row[1])
      symbol = row[2]
      contract_size = int(row[3])
      tick_size = float(row[4])
      quote_ccy = row[9]

      ab = symbol.split('/')
      if len(ab) != 2:
          sys.stderr.write("%s: Cannot get A/B ccys\n" % symbol)
          next
      a  = ab[0]
      b  = ab[1]

      if b == "USDm":
        b = "USD"

      if quote_ccy != b:
        sys.stderr.write("Quote = %s, but b = %s\n" % (quote_ccy, b))

      print(fmt % (id, symbol, a, b, 1.0, contract_size, tick_size))

exit(0)
