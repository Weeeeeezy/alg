{
  # Skip the first two lines (counted from 1):
  if (FNR <= 2)
    next;

  # All done if we found the following line (pos counted from 0):
  if (substr($0, 0, 1) == "(")
    exit 0;

  # Get the numerical Flds:
  secID   = int($1)
  qtyDec  = int($4)
  minQty  = strtonum($5)
  pxDec   = int($6)

  lotSz   = 1.0;
  for (i = 0; i < qtyDec; ++i) lotSz  /= 10.0;

  minLots = int(minQty / lotSz);

  # For the string flds, need to strip outthe spaces:
  match($2, /[A-Z0-9]+/);
  ccyA = substr($2, RSTART, RLENGTH);

  match($3, /[A-Z0-9]+/);
  ccyB = substr($3, RSTART, RLENGTH);

  # Generate the output:
  printf("    { %d, \"%s/%s\", \"\", \"\", \"MRCXXX\", \"LATOKEN\", \"\", " \
         "\"SPOT\", \"\",\n" \
         "      \"%s\", \"%s\", 'A', 1.0, 1e-%d, %d, 1e-%d, 'A', 1.0, " \
         "0, 0, 0.0, 0, \"\"\n    },\n",
         secID, ccyA, ccyB, ccyA, ccyB, qtyDec, minLots, pxDec);
}
