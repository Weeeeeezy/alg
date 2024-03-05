#! /usr/bin/python -O
#=============================================================================#
#                        "Tools/MkSecDefs-FTX.py":                            #
#=============================================================================#
import requests
import sys

r    = requests.get("https://ftx.com/api/markets")

# TODO
