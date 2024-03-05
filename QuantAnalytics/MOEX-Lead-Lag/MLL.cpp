// vim:ts=2
//===========================================================================//
//                                   "MLL.cpp":                              //
//       Lead-Lag Analysis of MICEX Spot and FORTS Futures Time Series       //
//===========================================================================//
#ifdef   NDEBUG
#undef   NDEBUG
#endif
#include "QuantAnalytics/MOEX-Lead-Lag/MLL.hpp"
#include "QuantAnalytics/MOEX-Lead-Lag/TickDataReader.hpp"
#include "QuantAnalytics/MOEX-Lead-Lag/HY.hpp"
#include "QuantAnalytics/MOEX-Lead-Lag/OtherMethods.hpp"
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/ini_parser.hpp>
#include <string>

namespace
{
  using namespace MLL;
  MDEntryT instr1[MaxTicks];
  MDEntryT instr2[MaxTicks];
}

//===========================================================================//
// "main":                                                                   //
//===========================================================================//
int main(int argc, char* argv[])
{
  using namespace std;
  using namespace MLL;

  //-------------------------------------------------------------------------//
  // Command-Line Params:                                                    //
  //-------------------------------------------------------------------------//
  if (argc != 2)
  {
    cerr << "PARAMETER: Config.ini" << endl;
    return 1;
  }
  try
  {
    //-----------------------------------------------------------------------//
    // Get the Property Tree:                                                //
    //-----------------------------------------------------------------------//
    string iniFile(argv[1]);

    boost::property_tree::ptree pt;
    boost::property_tree::ini_parser::read_ini(iniFile, pt);

    //-----------------------------------------------------------------------//
    // Construct the TickDataReader:                                         //
    //-----------------------------------------------------------------------//
    int debug   =  pt.get<int>("Main.DebugLevel");

    TickDataReader tdr
    (
      pt.get<int>             ("Main.Date"),      // As YYYYMMDD
      pt.get<int>             ("Main.TimeFrom"),  // As HHMM
      pt.get<int>             ("Main.TimeTo"),    // As HHMM
      GetPxSide(pt.get<string>("Main.Side")),
      debug
    );

    // Get the Exchanges:
    FormatT fmt1 = GetExch(pt.get<string>("Instr1.DataFormat"));
    FormatT fmt2 = GetExch(pt.get<string>("Instr2.DataFormat"));

    //-----------------------------------------------------------------------//
    // Construct the TimeSeries:                                             //
    //-----------------------------------------------------------------------//
    char const* symbol1 = pt.get<string>("Instr1.Symbol").data();
    tdr.GetTickData
    (
      pt.get<string>("Instr1.File").data(),
      fmt1,
      symbol1,
      instr1
    );
    VerifyTickData(instr1, symbol1, debug);

    int idxFrom = tdr.GetIdxFrom();
    int idxTo   = tdr.GetIdxTo();

    // Instr2:
    char const* symbol2 = pt.get<string>("Instr2.Symbol").data();
    tdr.GetTickData
    (
      pt.get<string>("Instr2.File").data(),
      fmt2,
      symbol2,
      instr2
    );
    VerifyTickData(instr2, symbol2, debug);

    // Intersect the range of indices with the previously-constructed one:
    idxFrom = max<int>(idxFrom, tdr.GetIdxFrom());
    idxTo   = min<int>(idxTo,   tdr.GetIdxTo());

    if (tdr.GetDebugLevel() >= 2)
    {
      cerr << "From=" << idxFrom << endl;
      cerr << "\tInstr1: Prev="  << instr1[idxFrom].m_prev << ", Curr="
           << instr1[idxFrom].m_curr << ", Next=" << instr1[idxFrom].m_next
           << endl;
      cerr << "\tInstr2: Prev="  << instr2[idxFrom].m_prev << ", Curr="
           << instr2[idxFrom].m_curr << ", Next=" << instr2[idxFrom].m_next
           << endl;
      cerr << "To="   << idxTo   << endl;
      cerr << "\tInstr1: Prev="  << instr1[idxTo  ].m_prev << ", Curr="
           << instr1[idxTo  ].m_curr << ", Next=" << instr1[idxTo  ].m_next
           << endl;
      cerr << "\tInstr2: Prev="  << instr2[idxTo  ].m_prev << ", Curr="
           << instr2[idxTo  ].m_curr << ", Next=" << instr2[idxTo  ].m_next
           << endl;
    }
    //-----------------------------------------------------------------------//
    // Get the Stats (in case we use the HY Method):                         //
    //-----------------------------------------------------------------------//
    MethodInfo mi = GetMethod(pt.get<string>("Main.Method"));

    StatsT st1 = { 0.0, 0.0, 0.0 };
    StatsT st2 = { 0.0, 0.0, 0.0 };

    if (mi.m_method == MethodT::HY)
    {
      st1 = GetStatsHY(instr1, idxFrom, idxTo);
      st2 = GetStatsHY(instr2, idxFrom, idxTo);
    }

    //-----------------------------------------------------------------------//
    // Compute the Cross-Correlation Function:                               //
    //-----------------------------------------------------------------------//
    int lMin  = pt.get<int>("Main.LagMin");
    int lMax  = pt.get<int>("Main.LagMax");
    int lStep = pt.get<int>("Main.LagStep");
    if (lStep <= 0)
      throw invalid_argument("Invalid LagStep=" + to_string(lStep));

    cout << "# AvgInterval1=" << st1.m_inter << endl;
    cout << "# AvgInterval2=" << st2.m_inter << endl;

    // Reserve the space for the results to be computed:
    int nRes  = (lMax - lMin) / lStep + 1;
    if (mi.m_method == MethodT::CondProbs)
      nRes *= 2;
    double res[nRes];

    //-----------------------------------------------------------------------//
    // Parallel Loop wrt Lead-Lag ("l"):                                     //
    //-----------------------------------------------------------------------//
#   pragma omp parallel for
    for (int l = lMin; l <= lMax; l += lStep)
    {
      int i = (l - lMin) / lStep;

      switch (mi.m_method)
      {
        case MethodT::HY:
          res[i] = HayashiYoshidaCrossCorr
                   (instr1, st1, instr2, st2, idxFrom,  idxTo, l);
          break;

        case MethodT::VAR:
          throw std::runtime_error("VAR: Unsupported");
          break;

        case MethodT::Uniform:
          res[i] = ClassicalCrossCorr
                   (instr1, instr2,  idxFrom, idxTo, l, mi.m_winSz);
          break;

        case MethodT::CondProbs:
        {
          // The result is a pair for (Bid, Ask) cond probs:
          auto   cps = CondProbs(instr1, instr2, idxFrom, idxTo, l);
          res[2*i]   = cps.first;
          res[2*i+1] = cps.second;
          break;
        }

        default:
          assert(false);
      }
    }
    // End of parallel "for" loop

    //-----------------------------------------------------------------------//
    // Now output the results:                                               //
    //-----------------------------------------------------------------------//
    for (int l = lMin; l <= lMax; l += lStep)
    {
      if (mi.m_method != MethodT::CondProbs)
      {
        // There is a single correlation value for each "l":
        int i  = (l - lMin) / lStep;
        cout << l << '\t' <<  res[i] << endl;
      }
      else
      {
        // MethodT::CondProbs: Separate Bid and Ask values for each "l":
        int i = 2 * ((l - lMin) / lStep);
        cout << l << '\t' << res[i] << '\t' << res[i+1] << endl;
      }
    }

    //-----------------------------------------------------------------------//`
    // Output the TimeSeries of Instr1 and Instr2 in GNUPlot format:         //
    //-----------------------------------------------------------------------//
    // XXX: This functionality is not generic yet:
    string seriesOut = pt.get<string>("Main.SeriesOut", "");
    if (!seriesOut.empty())
    {
      FILE* f = fopen(seriesOut.data(), "w");
      if (f == nullptr)
        throw invalid_argument("Cannot open SeriesOutFile=" + seriesOut);

      double mult1 = pt.get<double>("Instr1.PxMult");
      double mult2 = pt.get<double>("Instr2.PxMult");

      for (int i = idxFrom; i <= idxTo; i += 10)
        fprintf(f, "%d\t%lf\t%lf\n",
                i, (instr1[i].m_px * mult1), (instr2[i].m_px * mult2));
      fclose(f);
    }
  }
  catch (exception const& exc)
  {
    cerr << "ERROR: " << exc.what() << endl;
    return 1;
  }
  return 0;
}
