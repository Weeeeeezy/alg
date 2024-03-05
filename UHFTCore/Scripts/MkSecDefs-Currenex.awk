{
  A = substr($1,1,3);
  B = substr($1,5,3);
  printf "    { 0, \"%s\", \"\", \"\", \"MRCXXX\", \"Currenex\", \"\", \"SPOT\", \"\", \"%s\", \"%s\",\n", $1, A, B;
  printf "      'A', 1.0, 1.0, 1, 1e-%d, 'A', 1, 0, 79200, 0.0, 0, \"\"\n", $2;
  printf "    },\n"
}
