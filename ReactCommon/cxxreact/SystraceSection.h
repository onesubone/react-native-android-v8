// Copyright 2004-present Facebook. All Rights Reserved.

#pragma once

#include <android/log.h>
#include <string>
#ifdef WITH_FBSYSTRACE
#include <fbsystrace.h>
#endif

using namespace std;

namespace facebook {
namespace react {
/**
 * This is a convenience class to avoid lots of verbose profiling
 * #ifdefs.  If WITH_FBSYSTRACE is not defined, the optimizer will
 * remove this completely.  If it is defined, it will behave as
 * FbSystraceSection, with the right tag provided.
 */
struct SystraceSection {
private:
    std::string log;
    inline void initLog() {

    }

    template<typename T>
    inline void initLog(const T &ch) {
        log.append(ch);
    }

    template<typename T, typename... ConvertsToStringPiece>
    inline void initLog(const T &ch, const ConvertsToStringPiece&... rest) {
        log.append(ch);
        log.append(" ");
        initLog(rest...);
    }
public:
  template<typename... ConvertsToStringPiece>
  explicit SystraceSection(const char* name, const ConvertsToStringPiece&... args)
#ifdef WITH_FBSYSTRACE
    : m_section(TRACE_TAG_REACT_CXX_BRIDGE, name, args...), log("")
#else
    : log("")
#endif
  {
    initLog(name, args...);
    __android_log_print(ANDROID_LOG_DEBUG, "V8Application", "[Start] %s",  log.c_str());
  }

  ~SystraceSection() {
    __android_log_print(ANDROID_LOG_DEBUG, "V8Application", "[End] %s",  log.c_str());
  }
private:
#ifdef WITH_FBSYSTRACE
  fbsystrace::FbSystraceSection m_section;
#endif
};

}}
