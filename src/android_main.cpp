/*
    Copyright (c) 2024 Anthony J. Thibault
    This software is licensed under the MIT License. See LICENSE for
    more details.
*/

#include <jni.h>
#include <sys/prctl.h>  // for prctl( PR_SET_NAME )
#include <sys/stat.h>
#include <sys/types.h>

#include <android/native_window_jni.h>  // for native window JNI
#include <android_native_app_glue.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>
#include <GLES3/gl3ext.h>

#include <cstdio>
#include <string>

#include "src/app.h"
#include "src/log.h"
#include "src/util.h"
#include "src/xrbuddy.h"

// AJT: ANDROID TODO: xrPerfSettingsSetPerformanceLevelEXT
// AJT: ANDROID TODO: pfnSetAndroidApplicationThreadKHR on
// XR_SESSION_STATE_READY
// see ovrApp::HandleSessionStateChanges in SceneModelXr.cpp
/*
static const int CPU_LEVEL = 2;
static const int GPU_LEVEL = 3;
*/

static const char* EglErrorString(const EGLint error) {
  switch (error) {
    case EGL_SUCCESS:
      return "EGL_SUCCESS";
    case EGL_NOT_INITIALIZED:
      return "EGL_NOT_INITIALIZED";
    case EGL_BAD_ACCESS:
      return "EGL_BAD_ACCESS";
    case EGL_BAD_ALLOC:
      return "EGL_BAD_ALLOC";
    case EGL_BAD_ATTRIBUTE:
      return "EGL_BAD_ATTRIBUTE";
    case EGL_BAD_CONTEXT:
      return "EGL_BAD_CONTEXT";
    case EGL_BAD_CONFIG:
      return "EGL_BAD_CONFIG";
    case EGL_BAD_CURRENT_SURFACE:
      return "EGL_BAD_CURRENT_SURFACE";
    case EGL_BAD_DISPLAY:
      return "EGL_BAD_DISPLAY";
    case EGL_BAD_SURFACE:
      return "EGL_BAD_SURFACE";
    case EGL_BAD_MATCH:
      return "EGL_BAD_MATCH";
    case EGL_BAD_PARAMETER:
      return "EGL_BAD_PARAMETER";
    case EGL_BAD_NATIVE_PIXMAP:
      return "EGL_BAD_NATIVE_PIXMAP";
    case EGL_BAD_NATIVE_WINDOW:
      return "EGL_BAD_NATIVE_WINDOW";
    case EGL_CONTEXT_LOST:
      return "EGL_CONTEXT_LOST";
    default:
      return "unknown";
  }
}

struct AppContext {
  AppContext() : resumed(false), session_active(false), ass_man(nullptr),
                 always_copy_assets(true) {}
  bool resumed;
  bool session_active;

  struct EGLInfo {
    EGLInfo() : major_version(0), minor_version(0), display(0), config(0),
                context(EGL_NO_CONTEXT) {}
    EGLint major_version;
    EGLint minor_version;
    EGLDisplay display;
    EGLConfig config;
    EGLContext context;
    EGLSurface tiny_surface;
  };

  EGLInfo egl;
  AAssetManager* ass_man;
  std::string external_data_path;
  bool always_copy_assets;

  void Clear() {
    resumed = false;
    session_active = false;
    egl.major_version = 0;
    egl.minor_version = 0;
    egl.display = 0;
    egl.config = 0;
    egl.context = EGL_NO_CONTEXT;
  }

  bool SetupEGLContext() {
    // create the egl context
    egl.display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    eglInitialize(egl.display, &egl.major_version, &egl.minor_version);
    Log::D("OpenGLES major_version = %d, minor_version = %d\n",
           egl.major_version, egl.minor_version);
    const int kMaxConfigs = 1024;
    EGLConfig configs[kMaxConfigs];
    EGLint num_configs = 0;
    if (!eglGetConfigs(egl.display, configs, kMaxConfigs, &num_configs)) {
      Log::E("eglGetConfigs failed: %s\n",
             EglErrorString(eglGetError()));
      return false;
    }
    const EGLint config_attribs[] = {
      EGL_RED_SIZE, 8,
      EGL_GREEN_SIZE, 8,
      EGL_BLUE_SIZE, 8,
      // need alpha for the multi-pass timewarp compositor
      EGL_ALPHA_SIZE, 8,
      EGL_DEPTH_SIZE, 0,
      EGL_STENCIL_SIZE, 0,
      EGL_SAMPLES, 0,
      EGL_NONE
    };
    egl.config = 0;
    for (int i = 0; i < num_configs; i++) {
      EGLint value = 0;
      eglGetConfigAttrib(egl.display, configs[i],
                         EGL_RENDERABLE_TYPE, &value);
      if ((value & EGL_OPENGL_ES3_BIT_KHR) != EGL_OPENGL_ES3_BIT_KHR) {
        continue;
      }
      // The pbuffer config also needs to be compatible with normal
      // window rendering so it can share textures with the window
      // context.
      eglGetConfigAttrib(egl.display, configs[i], EGL_SURFACE_TYPE, &value);
      if ((value & (EGL_WINDOW_BIT | EGL_PBUFFER_BIT)) !=
          (EGL_WINDOW_BIT | EGL_PBUFFER_BIT)) {
        continue;
      }

      int j = 0;
      for (; config_attribs[j] != EGL_NONE; j += 2) {
        eglGetConfigAttrib(egl.display, configs[i],
                           config_attribs[j], &value);
        if (value != config_attribs[j + 1]) {
          break;
        }
      }
      if (config_attribs[j] == EGL_NONE) {
        egl.config = configs[i];
        break;
      }
    }

    if (egl.config == 0) {
      Log::E("eglChooseConfig() failed: %s\n",
             EglErrorString(eglGetError()));
      return false;
    }

    EGLint context_attribs[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};
    egl.context = eglCreateContext(egl.display, egl.config,
                                   EGL_NO_CONTEXT, context_attribs);
    if (egl.context == EGL_NO_CONTEXT) {
      Log::E("eglCreateContext() failed: %s", EglErrorString(eglGetError()));
      return false;
    }

    const EGLint surface_attribs[] = {EGL_WIDTH, 16, EGL_HEIGHT, 16,
                                      EGL_NONE};
    egl.tiny_surface = eglCreatePbufferSurface(egl.display, egl.config,
                                               surface_attribs);
    if (egl.tiny_surface == EGL_NO_SURFACE) {
      Log::E("eglCreatePbufferSurface() failed: %s",
             EglErrorString(eglGetError()));
      eglDestroyContext(egl.display, egl.context);
      egl.context = EGL_NO_CONTEXT;
      return false;
    }

    if (eglMakeCurrent(egl.display, egl.tiny_surface, egl.tiny_surface,
                       egl.context) == EGL_FALSE) {
      Log::E("eglMakeCurrent() failed: %s", EglErrorString(eglGetError()));
      eglDestroySurface(egl.display, egl.tiny_surface);
      eglDestroyContext(egl.display, egl.context);
      egl.context = EGL_NO_CONTEXT;
      return false;
    }

    return true;
  }

  bool SetupAssets(android_app* app) {
    assert(app);
    assert(app->activity->assetManager);
    assert(app->activity->externalDataPath);

    ass_man = app->activity->assetManager;
    external_data_path = std::string(app->activity->externalDataPath) + "/";

    // from util.h
    AppendSearchPath(external_data_path);

    Log::D("AJT: external_data_path = \"%s\"\n",
           external_data_path.c_str());

    MakeDir("texture");
    UnpackAsset("texture/carpet.png");
    UnpackAsset("texture/sphere.png");

    MakeDir("shader");
    UnpackAsset("shader/carpet_frag.glsl");
    UnpackAsset("shader/carpet_vert.glsl");
    UnpackAsset("shader/debugdraw_frag.glsl");
    UnpackAsset("shader/debugdraw_vert.glsl");
    UnpackAsset("shader/desktop_frag.glsl");
    UnpackAsset("shader/desktop_vert.glsl");
    UnpackAsset("shader/point_frag.glsl");
    UnpackAsset("shader/point_geom.glsl");
    UnpackAsset("shader/point_vert.glsl");
    UnpackAsset("shader/presort_compute.glsl");
    UnpackAsset("shader/splat_frag.glsl");
    UnpackAsset("shader/splat_geom.glsl");
    UnpackAsset("shader/splat_vert.glsl");
    UnpackAsset("shader/text_frag.glsl");
    UnpackAsset("shader/text_vert.glsl");

    MakeDir("font");
    UnpackAsset("font/JetBrainsMono-Medium.json");
    UnpackAsset("font/JetBrainsMono-Medium.png");

    MakeDir("data");
    MakeDir("data/sh_test");
    UnpackAsset("data/sh_test/cameras.json");
    UnpackAsset("data/sh_test/cfg_args");
    UnpackAsset("data/sh_test/input.ply");
    MakeDir("data/sh_test/point_cloud");
    MakeDir("data/sh_test/point_cloud/iteration_30000");
    UnpackAsset("data/sh_test/point_cloud/iteration_30000/point_cloud.ply");
    UnpackAsset("data/sh_test/vr.json");

    MakeDir("data/livingroom");
    UnpackAsset("data/livingroom/livingroom.ply");
    UnpackAsset("data/livingroom/livingroom_vr.json");

    return true;
  }

  bool MakeDir(const std::string& dir_filename) {
    std::string filename = external_data_path + dir_filename;
    if (mkdir(filename.c_str(), 0777) != 0) {
      if (errno == EEXIST) {
        Log::D("MakeDir \"%s\" already exists\n", dir_filename.c_str());
        // dir already exists!
        return true;
      } else {
        Log::E("mkdir failed on dir \"%s\" errno = %d\n",
               filename.c_str(), errno);
        return false;
      }
    }
    Log::D("MakeDir \"%s\"\n", dir_filename.c_str());

    return true;
  }

  bool UnpackAsset(const std::string& asset_filename) {
    std::string output_filename = external_data_path + asset_filename;

    struct stat sb;
    if (stat(output_filename.c_str(), &sb) == 0) {
      if (!always_copy_assets) {
        Log::D("UnpackAsset \"%s\" already exists\n",
               asset_filename.c_str());
        return true;
      }
    }

    AAsset *asset = AAssetManager_open(ass_man, asset_filename.c_str(),
                                       AASSET_MODE_STREAMING);
    if (asset == nullptr) {
      Log::E("UnpackAsset \"%s\" AAssetManager_open failed!\n",
             asset_filename.c_str());
      return false;  // Failed to open the asset
    }

    // Create buffer for reading
    const size_t kBufferSize = 1024;
    char buffer[kBufferSize];

    // Open file for writing
    FILE *out_file = fopen(output_filename.c_str(), "w");
    if (out_file == nullptr) {
      Log::E("UnpackAsset \"%s\" fopen failed!\n", asset_filename.c_str());
      AAsset_close(asset);
      return false;  // Failed to open the output file
    }

    // Read from assets and write to file
    int bytes_read;
    while ((bytes_read = AAsset_read(asset, buffer, kBufferSize)) > 0) {
      fwrite(buffer, sizeof(char), bytes_read, out_file);
    }

    // Close the asset and the output file
    AAsset_close(asset);
    fclose(out_file);

    Log::D("UnpackAsset \"%s\"\n", asset_filename.c_str());
    return true;
  }
};

/**
 * Process the next main command.
 */
static void app_handle_cmd(struct android_app* android_app, int32_t cmd) {
  AppContext& ctx = *static_cast<AppContext*>(android_app->userData);

  switch (cmd) {
    // There is no APP_CMD_CREATE. The ANativeActivity creates the
    // application thread from onCreate(). The application thread
    // then calls android_main().
    case APP_CMD_START:
      Log::D("onStart()\n");
      Log::D("    APP_CMD_START\n");
      break;
    case APP_CMD_RESUME:
      Log::D("onResume()\n");
      Log::D("    APP_CMD_RESUME\n");
      ctx.resumed = true;
      break;
    case APP_CMD_PAUSE:
      Log::D("onPause()\n");
      Log::D("    APP_CMD_PAUSE\n");
      ctx.resumed = false;
      break;
    case APP_CMD_STOP:
      Log::D("onStop()\n");
      Log::D("    APP_CMD_STOP\n");
      break;
    case APP_CMD_DESTROY:
      Log::D("onDestroy()\n");
      Log::D("    APP_CMD_DESTROY\n");
      ctx.Clear();
      break;
    case APP_CMD_INIT_WINDOW:
      Log::D("surfaceCreated()\n");
      Log::D("    APP_CMD_INIT_WINDOW\n");
      break;
    case APP_CMD_TERM_WINDOW:
      Log::D("surfaceDestroyed()\n");
      Log::D("    APP_CMD_TERM_WINDOW\n");
      break;
  }
}

/**
 * This is the main entry point of a native application that is using
 * android_native_app_glue.  It runs in its own thread, with its own
 * event loop for receiving input events and doing other things.
 */
void android_main(struct android_app* android_app) {
  Log::SetAppName("splatapult");

  Log::D("--------------------------------------------------"
         "--------------\n");
  Log::D("android_app_entry()\n");
  Log::D("    android_main()\n");

  JNIEnv* env;
  (*android_app->activity->vm).AttachCurrentThread(&env, nullptr);

  // Note that AttachCurrentThread will reset the thread name.
  prctl(PR_SET_NAME, reinterpret_cast<uint64_t>("android_main"), 0, 0, 0);

  AppContext ctx;
  android_app->userData = &ctx;
  android_app->onAppCmd = app_handle_cmd;

  if (!ctx.SetupEGLContext()) {
    Log::E("AppContext::SetupEGLContext failed!\n");
    return;
  }

  if (!ctx.SetupAssets(android_app)) {
    Log::E("AppContext::SetupAssets failed!\n");
    return;
  }

  MainContext main_context;
  main_context.display = ctx.egl.display;
  main_context.config = ctx.egl.config;
  main_context.context = ctx.egl.context;
  main_context.androidApp = android_app;

  std::string data_path = ctx.external_data_path +
                          "data/livingroom/livingroom.ply";
  int argc = 4;
  const char* argv[] = {"splataplut", "-v", "-d", data_path.c_str()};
  App app(main_context);
  App::ParseResult parse_result = app.ParseArguments(argc, argv);
  switch (parse_result) {
    case App::SUCCESS_RESULT:
      break;
    case App::ERROR_RESULT:
      Log::E("App::ParseArguments failed!\n");
      return;
    case App::QUIT_RESULT:
      return;
  }

  if (!app.Init()) {
    Log::E("App::Init failed!\n");
    return;
  }

  while (android_app->destroyRequested == 0) {
    // Read all pending events.
    for (;;) {
      int events;
      struct android_poll_source* source;
      // If the timeout is zero, returns immediately without blocking.
      // If the timeout is negative, waits indefinitely until an event
      // appears.
      int timeout_milliseconds = 0;
      if (ctx.resumed == false && ctx.session_active == false &&
          android_app->destroyRequested == 0) {
        timeout_milliseconds = -1;
      }

      if (ALooper_pollAll(timeout_milliseconds, NULL, &events,
                          reinterpret_cast<void**>(&source)) < 0) {
        break;
      }

      // Process this event.
      if (source != NULL) {
        source->process(android_app, source);
      }
    }

    float dt = 1.0f / 72.0f;
    if (!app.Process(dt)) {
      Log::E("App::Process failed!\n");
      break;
    }

    if (!app.Render(dt, glm::ivec2(0.0f, 0.0f))) {
      Log::E("App::Render failed!\n");
      return;
    }
  }

  // AJT(TODO): DESTROY STUFF
  Log::D("Finished!\n");

  (*android_app->activity->vm).DetachCurrentThread();
}
