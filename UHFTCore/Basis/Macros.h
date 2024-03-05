// vim:ts=2:et
//===========================================================================//
//                            "Common/Macros.h":                             //
//                          Common MAQUETTE Macros                           //
//===========================================================================//
#pragma  once

//===========================================================================//
// Run-Time Checks:                                                          //
//===========================================================================//
//---------------------------------------------------------------------------//
// "CHECK_ONLY":                                                             //
// "DEBUG_ONLY":                                                             //
//---------------------------------------------------------------------------//
// Do not generate unused symbols (if only used in "assert" which is only enabl-
// ed in the Debug mode):
// "CHECK_ONLY": Symbol is used unless UNCHECKED_MODE (and thus NDEBUG) are set:
//
#ifdef CHECK_ONLY
#undef CHECK_ONLY
#endif
#if (defined(NDEBUG) && UNCHECKED_MODE)
#  define CHECK_ONLY(x)           // UnChecked Mode only
#else
#  define CHECK_ONLY(x) x         // Release, RelWithDebInfo, Debug
#endif

// "DEBUG_ONLY": Symbol is used unless NDEBUG is set, which is a STRONGER cond
// compared to "CHECK_ONLY":
#ifdef DEBUG_ONLY
#undef DEBUG_ONLY
#endif
#ifdef NDEBUG
#  define DEBUG_ONLY(x)           // UnChecked or Release Mode
#else
#  define DEBUG_ONLY(x) x         // RelWithDebInfo or Debug
#endif

//---------------------------------------------------------------------------//
// "UNUSED_PARAM" (unconditionally):                                         //
//---------------------------------------------------------------------------//
#ifdef  UNUSED_PARAM
#undef  UNUSED_PARAM
#endif
#define UNUSED_PARAM(x)

//===========================================================================//
// Logging:                                                                  //
//===========================================================================//
//---------------------------------------------------------------------------//
// "LOG_*": For any classes with "m_logger" and "m_debugLevel":              //
//---------------------------------------------------------------------------//
#ifdef  LOG_INFO
#undef  LOG_INFO
#endif
#define LOG_INFO(LogLevel, ...) \
  if (this->m_logger != nullptr && this->m_debugLevel >= LogLevel) \
    { this->m_logger->info(__VA_ARGS__); \
      this->m_logger->flush(); }

#ifdef  LOG_WARN
#undef  LOG_WARN
#endif
#define LOG_WARN(LogLevel, ...) \
  if (this->m_logger != nullptr && this->m_debugLevel >= LogLevel) \
    { this->m_logger->warn(__VA_ARGS__); \
      this->m_logger->flush(); }

#ifdef  LOG_ERROR
#undef  LOG_ERROR
#endif
#define LOG_ERROR(LogLevel, ...) \
  if (this->m_logger != nullptr && this->m_debugLevel >= LogLevel) \
    { this->m_logger->error(__VA_ARGS__); \
      this->m_logger->flush(); }

#ifdef  LOG_CRIT
#undef  LOG_CRIT
#endif
#define LOG_CRIT(LogLevel, ...) \
  if (this->m_logger != nullptr && this->m_debugLevel >= LogLevel) \
    { this->m_logger->critical(__VA_ARGS__); \
      this->m_logger->flush(); }

//---------------------------------------------------------------------------//
// Identification:                                                           //
//---------------------------------------------------------------------------//
#ifdef LOG_VERSION
#undef LOG_VERSION
#endif
#if (defined(COMMIT_NAME)  && defined(BRANCH_NAME) && \
     defined(COMPILE_DATE) && defined(REMOTE_NAME))
  #define LOG_VERSION() \
    LOG_INFO(1, "Current binary info:") \
    LOG_INFO(1, "Remote: {}", REMOTE_NAME) \
    LOG_INFO(1, "Branch: {}", BRANCH_NAME) \
    LOG_INFO(1, "Commit: {}", COMMIT_NAME) \
    LOG_INFO(1, "Compile time: {}\n", COMPILE_DATE)
#else
  LOG_ERROR(1, "NO BUILD VERSION INFO! PLEASE, RECOMPILE!\n")
#endif

//---------------------------------------------------------------------------//
// "LOGA_*": "m_logger" and "m_debugLevel" are given by "Another" class:     //
//---------------------------------------------------------------------------//
#ifdef  LOGA_INFO
#undef  LOGA_INFO
#endif
#define LOGA_INFO(Another,  LogLevel, ...) \
  if (utxx::unlikely(Another::m_debugLevel >= LogLevel)) \
    { Another::m_logger->info(__VA_ARGS__); \
      Another::m_logger->flush(); }

#ifdef  LOGA_WARN
#undef  LOGA_WARN
#endif
#define LOGA_WARN(Another,  LogLevel, ...) \
  if (utxx::unlikely(Another::m_debugLevel >= LogLevel)) \
    { Another::m_logger->warn(__VA_ARGS__); \
      Another::m_logger->flush(); }

#ifdef  LOGA_ERROR
#undef  LOGA_ERROR
#endif
#define LOGA_ERROR(Another, LogLevel, ...) \
  if (utxx::unlikely(Another::m_debugLevel >= LogLevel)) \
    { Another::m_logger->error(__VA_ARGS__); \
      Another::m_logger->flush(); }

#ifdef  LOGA_CRIT
#undef  LOGA_CRIT
#endif
#define LOGA_CRIT(Another,  LogLevel, ...) \
  if (utxx::unlikely(Another::m_debugLevel >= LogLevel)) \
    { Another::m_logger->critical(__VA_ARGS__); \
      Another::m_logger->flush(); }

//===========================================================================//
// Misc:                                                                     //
//===========================================================================//
//---------------------------------------------------------------------------//
// "WEAK_SYMBOL":                                                            //
//---------------------------------------------------------------------------//
// This is a DIRTY TRICK which allows us to declare entities (which must even-
// tually occur only once in a linked executable) in a header file (to be inc-
// luded into multiple CPP files), rather than in  a signle separate CPP file.
// Simplifies creation of header-only library components:
// XXX: It works well with GCC but produces a warning in CLang:
#ifdef  WEAK_SYMBOL
#undef  WEAK_SYMBOL
#endif
#define WEAK_SYMBOL extern __attribute__((weak))

//---------------------------------------------------------------------------//
// "STRINGIFY":                                                              //
//---------------------------------------------------------------------------//
#ifdef  STRINGIFY
#undef  STRINGIFY
#endif
#ifdef  STRINGIFY1
#undef  STRINGIFY1
#endif
#define STRINGIFY1(x) #x
#define STRINGIFY(x) STRINGIFY1(x)

//---------------------------------------------------------------------------//
// "BT_VIRTUAL":                                                             //
//---------------------------------------------------------------------------//
// XXX: Curr impl of BT requires some methods to be made "virtual" which would
// not need to be such in a more advanced implementation; hence BL_VIRTUAL mark:
//
#ifdef  BT_VIRTUAL
#undef  BT_VIRTUAL
#endif
#define BT_VIRTUAL virtual

