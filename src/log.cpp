/*
    Copyright (c) 2024 Anthony J. Thibault
    This software is licensed under the MIT License. See LICENSE for more details.
*/

#include "src/log.h"

#include <cstdio>
#include <cstdarg>
#include <string>

#ifdef __ANDROID__
#include <android/log.h>
#endif

static Log::LogLevel level = Log::Verbose;
static const size_t APP_NAME_SIZE = 1024;
static char app_name[APP_NAME_SIZE] = "Core";

#ifdef __ANDROID__
static int log_vprintf(int prio, const char *fmt, va_list args) {
  char buffer[4096];
  int rc = vsnprintf(buffer, sizeof(buffer), fmt, args);
  __android_log_write(prio, app_name, buffer);
  return rc;
}
#else
static int log_vprintf(const char *fmt, va_list args) {
  char buffer[4096];
  int rc = vsnprintf(buffer, sizeof(buffer), fmt, args);
  fwrite(buffer, rc, 1, stdout);
  fflush(stdout);
  return rc;
}
#endif

int Log::printf(const char *fmt, ...) {
#ifdef __ANDROID__
  return 0;
#else
  char buffer[4096];
  va_list args;
  va_start(args, fmt);
  int rc = vsnprintf(buffer, sizeof(buffer), fmt, args);
  va_end(args);

  fwrite(buffer, rc, 1, stdout);
  fflush(stdout);

  return rc;
#endif
}

void Log::SetLevel(LogLevel level_in) {
  level = level_in;
}

void Log::SetAppName(const std::string& app_name_in) {
  snprintf(app_name, APP_NAME_SIZE, "%s", app_name_in.c_str());
}

void Log::V(const char *fmt, ...) {
  if (level <= Log::Verbose) {
    Log::printf("[VERBOSE] ");
    va_list args;
    va_start(args, fmt);
#ifdef __ANDROID__
    log_vprintf(ANDROID_LOG_VERBOSE, fmt, args);
#else
    log_vprintf(fmt, args);
#endif
    va_end(args);
  }
}

void Log::D(const char *fmt, ...) {
  if (level <= Log::Debug) {
    Log::printf("[DEBUG] ");
    va_list args;
    va_start(args, fmt);
#ifdef __ANDROID__
    log_vprintf(ANDROID_LOG_DEBUG, fmt, args);
#else
    log_vprintf(fmt, args);
#endif
    va_end(args);
  }
}

void Log::I(const char *fmt, ...) {
  if (level <= Log::Info) {
    Log::printf("[INFO] ");
    va_list args;
    va_start(args, fmt);
#ifdef __ANDROID__
    log_vprintf(ANDROID_LOG_INFO, fmt, args);
#else
    log_vprintf(fmt, args);
#endif
    va_end(args);
  }
}

void Log::W(const char *fmt, ...) {
  if (level <= Log::Warning) {
    Log::printf("[WARNING] ");
    va_list args;
    va_start(args, fmt);
#ifdef __ANDROID__
    log_vprintf(ANDROID_LOG_WARN, fmt, args);
#else
    log_vprintf(fmt, args);
#endif
    va_end(args);
  }
}

void Log::E(const char *fmt, ...) {
  if (level <= Log::Error) {
    Log::printf("[ERROR] ");
    va_list args;
    va_start(args, fmt);
#ifdef __ANDROID__
    log_vprintf(ANDROID_LOG_ERROR, fmt, args);
#else
    log_vprintf(fmt, args);
#endif
    va_end(args);
  }
}


