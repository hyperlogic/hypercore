/*
    Copyright (c) 2024 Anthony J. Thibault
    This software is licensed under the MIT License. See LICENSE for
    more details.
*/

#pragma once

#if defined(__ANDROID__)
#include <jni.h>

#include <android_native_app_glue.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>
#include <GLES3/gl3ext.h>
#elif defined(__linux__)
#include <GL/glew.h>
#include <GL/glx.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/extensions/xf86vmode.h>  // for fullscreen video mode
#include <X11/extensions/Xrandr.h>     // for resolution changes

// Conflicts with core/optionparser.h
#ifdef None
#undef None
#endif

#endif

namespace hyper {

#if defined(__ANDROID__)
struct MainContext {
  EGLDisplay display;
  EGLConfig config;
  EGLContext context;
  android_app* androidApp;
  float dpi_scale;
};
#elif defined(__linux__)
struct MainContext {
  Display* xdisplay;
  uint32_t visualid;
  GLXFBConfig glxFBConfig;
  GLXDrawable glxDrawable;
  GLXContext glxContext;
  float dpi_scale;
};

#else
struct MainContext {
  float dpi_scale;
};
#endif

}  // namespace hyper
