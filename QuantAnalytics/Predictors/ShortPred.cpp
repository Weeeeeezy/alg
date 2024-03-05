// vim:ts=2:et
//===========================================================================//
//                             "ShortPred.cpp":                              //
//       Short-Term (Next-Tick) Price Predictor for Si and USD/RUB_TOM       //
//===========================================================================//
#include "Predictors.h"
#include "Basis/ConfigUtils.hpp"
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/ini_parser.hpp>
#include <iostream>

using namespace std;
using namespace MAQUETTE;

int main(int argc, char* argv[])
{
  try
  {
    //-----------------------------------------------------------------------//
    // Open the Config File:                                                 //
    //-----------------------------------------------------------------------//
    if (argc != 2)
    {
      cerr << "Parameter: Config.ini" << endl;
      return 1;
    }
    string iniFile(argv[1]);

    boost::property_tree::ptree pt;
    boost::property_tree::ini_parser::read_ini(iniFile, pt);

    // Get the Instruments etc:
    string futInstr   = pt.get<string>("Main.Fut");
    string spotInstr1 = pt.get<string>("Main.Spot1");  // XXX: For now
    int debugLevel    = pt.get<int>   ("Main.DebugLevel");

    //-----------------------------------------------------------------------//
    // Read the Raw Learning Set and the BackTesting Set in:                 //
    //-----------------------------------------------------------------------//
    boost::property_tree::ptree& learnTree =
      GetParamsSubTree(pt, "Learning");
    boost::property_tree::ptree& btTree    =
      GetParamsSubTree(pt, "BackTesting");

    vector<Predictors::RawTickPMF> rawLearnData;
    vector<Predictors::RawTickPMF> rawBTData;

    // Reserve sufficiently large vector sizes for the Src DataSets:
    rawLearnData.reserve(1024 * 1024);
    rawBTData   .reserve( 256 * 1024);

    //-----------------------------------------------------------------------//
    // For both Learning and Back-Testing DataSets:                          //
    //-----------------------------------------------------------------------//
    for (int i = 0; i < 2; ++i)
    {
      if (debugLevel >= 2)
        cerr << "==> " << ((i == 0) ? "Learning" : "BackTest")
             << " DataSet: " << endl;

      boost::property_tree::ptree&    src  =
        (i == 0) ? learnTree    : btTree;
      vector<Predictors::RawTickPMF>& targ =
        (i == 0) ? rawLearnData : rawBTData;

      //--------------------------------------------------------------------//
      // There may be multiple Src files specified for each DataSet:        //
      //--------------------------------------------------------------------//
      TraverseParamsTree
      (
        src,

        [&src, &targ,  &futInstr, &spotInstr1, debugLevel]
        (string const& a_key, string const& a_val) -> bool
        {
          if (strncmp(a_key.data(), "DataSetFile", 11) == 0)
          {
            char const* fileName = a_val.data();
            size_t      before   = targ.size();
            if (debugLevel >= 2)
              cerr << "Loading: " << fileName << " ... ";

            //---------------------------------------------------------------//
            // Actually load the data:                                       //
            //---------------------------------------------------------------//
            Predictors::MkRawTicksPMF
              (fileName, futInstr.data(), spotInstr1.data(), &targ);

            size_t      after    = targ.size();
            if (debugLevel >= 2)
              cerr << (after - before) << " RawTicks loaded" << endl;
          }
          return true;
        }
      );
      if (debugLevel >= 2)
        cerr << "Total "     << ((i == 0) ? "Learning" : "BackTest")
             << " DataSet: " << targ.size() << " RawTicks" << endl;
    }

    //-----------------------------------------------------------------------//
    // Construct the Regression Matrices and the RHSs:                       //
    //-----------------------------------------------------------------------//
    // The Regression Matrices and RHSs are initially empty:
    vector<double> ML, RL, MB, RB;

    Predictors::InstrT targInstr =
    Predictors::InstrT::from_string(pt.get<string>("Main.Target"));

    Predictors::RegrSideT regrSide =
    Predictors::RegrSideT::from_string(pt.get<string>("Main.RegrSide"));

    int    nearMS      = pt.get<int>   ("Main.NearMS");
    int    farMS       = pt.get<int>   ("Main.FarMS");
    int    NF          = pt.get<int>   ("Main.NF");
    int    NS          = pt.get<int>   ("Main.NS");
    int    OBDepthF    = pt.get<int>   ("Main.OBDepthF");
    int    OBDepthS    = pt.get<int>   ("Main.OBDepthS");
    double log0        = pt.get<double>("Main.Log0");
    bool   relaxedEval = pt.get<bool>  ("Main.RelaxedEval");
    string resFile     = pt.get<string>("Main.ResultsFile");

    vector<double>        evalBuff;
    Predictors::RegrSideT evalSide =
      relaxedEval
      ? regrSide
      : Predictors::RegrSideT(Predictors::RegrSideT::UNDEFINED);

    //-----------------------------------------------------------------------//
    // Invoke the Regresson Construction Funcs:                              //
    //-----------------------------------------------------------------------//
    // For the Learning Set:
    auto szsL = Predictors::MkRegrPMF
    (
      targInstr, regrSide, NF,   OBDepthF,     NS,  OBDepthS,
      nearMS,    farMS,    log0, rawLearnData, &ML, &RL
    );
    cerr << "Learning Matrix: " << szsL.first << " * " << szsL.second << endl;

    // For the BackTesting Set:
    auto szsB = Predictors::MkRegrPMF
    (
      targInstr, regrSide, NF,   OBDepthF,     NS,  OBDepthS,
      nearMS,    farMS,    log0, rawBTData,    &MB, &RB
    );
    cerr << "BackTest Matrix: " << szsB.first << " * " << szsB.second << endl;

    //-----------------------------------------------------------------------//
    // Output the Sets:                                                      //
    //-----------------------------------------------------------------------//
    string learnFile = pt.get<string>("Learning.OutputFile",    "");
    string btFile    = pt.get<string>("BackTesting.OutputFile", "");
    if (!learnFile.empty())
      Predictors::OutputRegr(ML, RL, learnFile.data());
    if (!btFile.empty())
      Predictors::OutputRegr(MB, RB, btFile.data());

    //-----------------------------------------------------------------------//
    // Invoke the Solver:                                                    //
    //-----------------------------------------------------------------------//
    Predictors::SolveRegrLSM(&ML, &RL);

    // Output the Solution:
    Predictors::OutputSolutionPMF
      (NF, OBDepthF, NS, OBDepthS, RL, resFile.data());

    // Evaluate the Success Rate:
    double res = Predictors::EvalRegr(MB, RB, RL, &evalBuff, evalSide);
    cout << "Success Rate: " << res << endl;

    // All Done!
    return 0;
  }
  catch (exception const& exc)
  {
    cerr << "EXCEPTION: " << exc.what() << endl;
    return 1;
  }
}
