/*
    Copyright (c) 2024 Anthony J. Thibault
    This software is licensed under the MIT License. See LICENSE for
    more details.
*/

// 3d gaussian splat renderer

#include <stdint.h>
#include <string>

#ifndef _WIN32
#include <unistd.h>
#include <limits.h>
#endif

#include <GL/glew.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include <SDL2/SDL_syswm.h>

#include <chrono>
#include <memory>
#include <thread>

#include <glm/glm.hpp>

#ifdef TRACY_ENABLE
#include <tracy/Tracy.hpp>
#else
#define FrameMark do {} while (0)
#endif

#include "src/appbase.h"
#include "src/log.h"
#include "src/util.h"

#ifdef _WIN32
#include <windows.h>
#endif

struct GlobalContext {
  bool quitting = false;
  SDL_Window* window = NULL;
  SDL_GLContext gl_context;
};

GlobalContext ctx;

int SDLCALL Watch(void *userdata, SDL_Event* event) {
  if (event->type == SDL_APP_WILLENTERBACKGROUND) {
    ctx.quitting = true;
  }

  return 1;
}

int main(int argc, char *argv[]) {
  hyper::Log::SetAppName("hypercore");

#ifdef SHIPPING
  // Set search path to executable directory so resources can be found
  // when launched via file association (where cwd is the file's directory)
#ifdef _WIN32
  char exe_path[MAX_PATH];
  if (GetModuleFileNameA(NULL, exe_path, MAX_PATH) > 0) {
    std::string path(exe_path);
    size_t last_slash = path.find_last_of("\\/");
    if (last_slash != std::string::npos) {
      path = path.substr(0, last_slash + 1);
      hyper::AppendSearchPath(path);
    }
  }
#else
  // On Linux/macOS, use /proc/self/exe or argv[0]
  char exe_path[PATH_MAX];
  ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
  if (len != -1) {
    exe_path[len] = '\0';
    std::string path(exe_path);
    size_t last_slash = path.find_last_of('/');
    if (last_slash != std::string::npos) {
      path = path.substr(0, last_slash + 1);
      hyper::AppendSearchPath(path);
    }
  }
#endif
#endif

  hyper::MainContext main_context;

  // Create the app using the factory function defined by the application
  std::shared_ptr<hyper::AppBase> app = hyper::AppBase::Create(main_context);
  if (!app) {
    hyper::Log::E("AppBase::Create() returned null!\n");
    return 1;
  }

  hyper::AppBase::ParseResult parse_result = app->ParseArguments(
      argc, (const char**)argv);

  switch (parse_result) {
    case hyper::AppBase::SUCCESS_RESULT:
      break;
    case hyper::AppBase::ERROR_RESULT:
      hyper::Log::E("AppBase::ParseArguments failed!\n");
      return 1;
    case hyper::AppBase::QUIT_RESULT:
      return 0;
  }

  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_JOYSTICK) != 0) {
    hyper::Log::E("Failed to initialize SDL: %s\n", SDL_GetError());
    return 1;
  }

  const int32_t kWidth = 1024;
  const int32_t kHeight = 768;

  // Allow us to use automatic linear->sRGB conversion.
  SDL_GL_SetAttribute(SDL_GL_FRAMEBUFFER_SRGB_CAPABLE, 1);

  // increase depth buffer size
  SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

  uint32_t window_flags = SDL_WINDOW_OPENGL;
  if (app->IsFullscreen()) {
    window_flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
  } else {
    window_flags |= SDL_WINDOW_RESIZABLE;
  }

#ifdef __APPLE__
  // Set OpenGL version (macOS supports max OpenGL 4.1)
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,
                      SDL_GL_CONTEXT_PROFILE_CORE);
  // Required on macOS
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS,
                      SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
#endif

  ctx.window = SDL_CreateWindow(app->app_title().c_str(),
                                SDL_WINDOWPOS_UNDEFINED,
                                SDL_WINDOWPOS_UNDEFINED, kWidth, kHeight,
                                window_flags);

  if (!ctx.window) {
    hyper::Log::E("Failed to create window: %s\n", SDL_GetError());
    return 1;
  }

  ctx.gl_context = SDL_GL_CreateContext(ctx.window);
  if (!ctx.gl_context) {
    hyper::Log::E("Failed to gl context: %s\n", SDL_GetError());
    return 1;
  }

  SDL_GL_MakeCurrent(ctx.window, ctx.gl_context);

  hyper::Log::I("GL_RENDERER = %s\n", glGetString(GL_RENDERER));
  hyper::Log::I("GL_VERSION = %s\n", glGetString(GL_VERSION));
  hyper::Log::I("GL_SHADING_LANGUAGE_VERSION = %s\n",
         glGetString(GL_SHADING_LANGUAGE_VERSION));

  GL_ERROR_CHECK("here");
  if (!glGetString(GL_VERSION)) {
    hyper::Log::E("gl get string failed: %s\n", SDL_GetError());
  }

#ifdef __linux__
  // Initialize context from the SDL window
  SDL_SysWMinfo info;
  SDL_VERSION(&info.version)
  auto ret = SDL_GetWindowWMInfo(ctx.window, &info);
  if (ret != SDL_TRUE) {
    hyper::Log::W("Failed to retrieve SDL window info: %s\n", SDL_GetError());
  } else {
    main_context.xdisplay = info.info.x11.display;
    main_context.glxDrawable = (GLXWindow)info.info.x11.window;
    main_context.glxContext = (GLXContext)ctx.gl_context;
  }
#endif

  GLenum err = glewInit();
  if (GLEW_OK != err) {
    hyper::Log::E("Error: %s\n", glewGetErrorString(err));
    return 1;
  }

  // AJT: TODO REMOVE disable vsync for benchmarks
  SDL_GL_SetSwapInterval(1);

  SDL_AddEventWatch(Watch, NULL);

  if (!app->Init()) {
    hyper::Log::E("AppBase::Init failed\n");
    return 1;
  }

  bool should_quit = false;
  app->OnQuit([&should_quit]() {
    should_quit = true;
  });

  app->OnResize([](int new_width, int new_height) {
    // SDL_RenderSetLogicalSize(ctx.renderer, new_width, new_height);
    // AJT: TODO resize texture?
  });

  uint32_t frame_count = 1;
  uint32_t frame_ticks = SDL_GetTicks();
  uint32_t last_ticks = SDL_GetTicks();
  while (!ctx.quitting && !should_quit) {
    // update dt
    uint32_t ticks = SDL_GetTicks();

    const int kFpsFrames = 100;
    if ((frame_count % kFpsFrames) == 0) {
      float delta = (ticks - frame_ticks) / 1000.0f;
      float fps = static_cast<float>(kFpsFrames) / delta;
      frame_ticks = ticks;
      app->UpdateFps(fps);
    }
    float dt = (ticks - last_ticks) / 1000.0f;
    last_ticks = ticks;

    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      app->ProcessEvent(event);
    }

    if (!app->Process(dt)) {
      hyper::Log::E("AppBase::Process failed!\n");
      return 1;
    }

    SDL_GL_MakeCurrent(ctx.window, ctx.gl_context);

    int width, height;
    SDL_GetWindowSize(ctx.window, &width, &height);
    if (!app->Render(dt, glm::ivec2(width, height))) {
      hyper::Log::E("AppBase::Render failed!\n");
      return 1;
    }

    GL_ERROR_CHECK("end of render loop");

    SDL_GL_SwapWindow(ctx.window);

    frame_count++;

    FrameMark;
  }

  SDL_DelEventWatch(Watch, NULL);
  SDL_GL_DeleteContext(ctx.gl_context);

  SDL_DestroyWindow(ctx.window);
  SDL_Quit();

  return 0;
}
