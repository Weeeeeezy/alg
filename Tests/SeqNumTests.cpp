// vim:ts=2:et
//===========================================================================//
//                         "Tests/SeqNumTests.cpp":                          //
//===========================================================================//
#include "Basis/IOUtils.h"
#include "Connectors/SeqNumBuffer.hpp"
#include <iostream>
#include <vector>
#include <iomanip>
#include <sstream>

# ifdef  __GNUC__
# pragma GCC   diagnostic push
# pragma GCC   diagnostic ignored "-Wunused-parameter"
# endif
# ifdef  __clang__
# pragma clang diagnostic push
# pragma clang diagnostic ignored "-Wunused-parameter"
# endif

using namespace MAQUETTE;
using namespace std;

namespace
{

  using Vector = vector<SeqNum>;
  constexpr int ArraySize = 64;
  constexpr int MaxChangeSize = 16;
  constexpr int MaxGap = 20;

  Vector GoodArray()
  {
      Vector result;
      for (int i = 0; i < ArraySize; ++i)
      {
          result.push_back(i + 1);
      }
      return result;
  }

  bool IsValidIndex(int aIndex)
  {
      return aIndex >= 0 && aIndex < ArraySize;
  }
  // Loose aSize elements every aStep indexes
  Vector AddGaps(const Vector& aInputVector, int aStep, int aSize)
  {
      Vector result;
      for (int i = 0; i < ArraySize; ++i)
      {
          if ((i - 1) % aStep < aSize)
              result.push_back(-1);
          else
              result.push_back(aInputVector[size_t(i)]);
      }
      return result;
  }

  // Swap every aStep element with its idx + aDistance
  Vector SwapItems(const Vector& aInputVector, int aStep, int aDistance)
  {
      Vector result;
      for (int i = 0; i < ArraySize; ++i)
      {
          if ((i - 1) % aStep == 0)
          {
              int idx = i + aDistance;
              if (IsValidIndex(idx))
                  result.push_back(aInputVector[size_t(i + aDistance)]);
              else
                  result.push_back(-1);
          }
          else if ((i - 1) % aStep == aDistance)
          {
              int idx = i - aDistance;
              if (IsValidIndex(idx))
                  result.push_back(aInputVector[size_t(i - aDistance)]);
              else
                  result.push_back(-1);
          }
          else
          {
              result.push_back(aInputVector[size_t(i)]);
          }
      }
      return result;
  }

  // Offset all elements by aDistance (positive or negative)
  Vector AddOffset(const Vector& aInputVector, int aDistance)
  {
      Vector result;
      for (int i = 0; i < ArraySize; ++i)
      {
          int idx = i + aDistance;
          if (IsValidIndex(idx))
              result.push_back(aInputVector[size_t(idx)]);
          else
              result.push_back(-1);
      }
      return result;
  }

  // Merge two vectors by taking one element from each in loop
  Vector MergeVectors(const Vector& aVector1, const Vector& aVector2)
  {
      Vector result;
      for (int i = 0; i < ArraySize; ++i)
      {
          result.push_back(aVector1[size_t(i)]);
          result.push_back(aVector2[size_t(i)]);
      }
      return result;
  }

  void PrintVector(const Vector& aVector, bool cppList = false)
  {
      if (cppList)
          cout << "{";
      for (auto item : aVector)
          cout << setw(cppList ? 2 : 3) << item << (cppList ? ", " : "");

      if (cppList)
          cout << "\b\b}";
      cout << endl;
  }

  using Type = int;
  class SeqNumOperators
  {
      Vector mResult;
  public:

      const Vector& GetResult() const
      {
          return mResult;
      }

      void operator()(Type* place)
      {
          //cout << " E" << *place;
      }

      void operator()
        (SeqNum aSeqNum, Type const& item, bool init_mode,
         utxx::time_val, utxx::time_val)
      {
          //cout << " P" << aSeqNum;
          mResult.push_back(aSeqNum);
      }
  };

  using SeqNumBuff = SeqNumBuffer<Type, SeqNumOperators, SeqNumOperators>;

  void ApplyVector(const string& aTestName, SeqNumBuff& seqNubBuff, SeqNumOperators& handler, const Vector& aVector)
  {
      for (auto item : aVector)
      {
          if (item != -1)
              seqNubBuff.Put(&handler, item, utxx::time_val(), utxx::time_val());
      }
  }

  bool ValidateResult(const string& aTestName, const Vector& aResult, const Vector& aMergedVector)
  {
      // check monotony
      SeqNum lastVal = 0;
      for (auto item : aResult)
      {
          if (item <= lastVal)
          {
              cout << "Monotony check failed";
              return false;
          }
          lastVal = item;
      }

      // check values from nowhere
      for (auto item : aResult)
      {
          bool valueFound = false;
          for (auto inputItem : aMergedVector)
          {
              if (item == inputItem)
              {
                  valueFound = true;
                  break;
              }
          }

          if (!valueFound)
          {
              cout << "Values from nowhere check failed";
              return false;
          }
      }
      return true;
  }
}

//===========================================================================//
// "main":                                                                   //
//===========================================================================//
int main()
{
    // Initialise the Logger (simple -- synchronous logging on stderr):
    shared_ptr<spdlog::logger> logger =
      IO::MkLogger("SeqNumTests_Logger", "stderr");

    // TestArrays();
    // LogLevel=9, to surely output all log msgs:
    {
        SeqNumOperators handler;
        SeqNumBuff seqNum(ArraySize, 3, &handler, logger.get(), 9, false);
        ApplyVector("SimpleTest", seqNum, handler, {1, 3, 5, 7, 9});
    }

    {
        SeqNumOperators handler;
        SeqNumBuff seqNum(ArraySize, 3, &handler, logger.get(), 9, false);
        ApplyVector("SimpleTest", seqNum, handler, {1, 3, 2});
    }

    {
        SeqNumOperators handler;
        SeqNumBuff seqNum(ArraySize, 1, &handler, logger.get(), 9, false);
        ApplyVector("SimpleTest", seqNum, handler, {1, 3, 5});
    }

    vector<pair<string, function<Vector(const Vector&, int, int)>>> vectorModifiers = {
        {"valid  ", [](const Vector& aVector, int a, int b) -> Vector { return aVector;}},
        {"gaps   ", [](const Vector& aVector, int a, int b) -> Vector { return AddGaps(aVector, a, b);}},
        {"swap   ", [](const Vector& aVector, int a, int b) -> Vector { return SwapItems(aVector, a, b);}},
        {"offset ", [](const Vector& aVector, int a, int b) -> Vector { return AddOffset(aVector, a);}},
        {"gap+off", [](const Vector& aVector, int a, int b) -> Vector { return AddGaps(AddOffset(aVector, a), b, a);}}
    };

    vector<pair<string, Vector>> testVectors;

    for (auto item1 : vectorModifiers)
    {
        for (int a = 1; a <= MaxChangeSize; a++)
        {
            for (int b = 1; b < a; b++)
            {
                testVectors.emplace_back(
                    item1.first + " " + to_string(a) + " " + to_string(b),
                    item1.second(GoodArray(), a, b));
            }
        }
    }

    int arraysCount = int(vectorModifiers.size()) * MaxChangeSize * (MaxChangeSize - 1) / 2;
    cout << "Expected tests count: " << MaxGap * arraysCount * arraysCount << endl;

    int totalTests = 0;
    int passedTests = 0;
    int failedTests = 0;
    for (int maxGap = 0; maxGap < MaxGap; maxGap++)
    {
        for (auto item1 : testVectors)
        {
            for (auto item2 : testVectors)
            {
                stringstream testName;
                testName <<
                    " id= " << totalTests <<
                    " gap=" << maxGap <<
                    " (" << item1.first << " + " << item2.first << ")";

                auto mergedVector = MergeVectors(item1.second, item2.second);

                totalTests++;

                SeqNumOperators handler;
                bool exceptionCaught = false;
                bool validatePassed = false;
                try
                {
                    SeqNumBuff seqNumTest(ArraySize, maxGap, &handler, logger.get(), 9, false);

                    //cout << testName.str() << ":";
                    //PrintVector(mergedVector);
                    //cout << endl;

                    ApplyVector(
                        testName.str(),
                        seqNumTest,
                        handler,
                        mergedVector);

                    passedTests++;
                    if (passedTests % 10000 == 0)
                        cout << "Passed " << passedTests << endl;

                    validatePassed = ValidateResult(testName.str(), handler.GetResult(), mergedVector);
                }
                catch(...)
                {
                    exceptionCaught = true;
                }

                if (exceptionCaught || !validatePassed)
                {
                    cout << "FAILED " << (exceptionCaught ? "by exception" : "") << testName.str() << ": ";
                    PrintVector(mergedVector);
                    cout << "Result:";
                    PrintVector(handler.GetResult());

                    failedTests++;
                    if (failedTests == 10)
                        return 0;
                }
            }
        }
    }

    cout << "Total tests : " << totalTests  << endl;
    cout << "Passed tests: " << passedTests << endl;
    cout << "Failed tests: " << failedTests << endl;

    return 0;
}

# ifdef  __GNUC__
# pragma GCC   diagnostic pop
# endif
# ifdef  __clang__
# pragma clang diagnostic pop
# endif
