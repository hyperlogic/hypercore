/*
    Copyright (c) 2024 Anthony J. Thibault
    This software is licensed under the MIT License. See LICENSE for
    more details.
*/

#include "src/appbase.h"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#ifndef __ANDROID__
#define USE_SDL
#endif

#include "src/glincludes.h"

#ifdef USE_SDL
#include <SDL2/SDL.h>
#endif

#include <imgui.h>
#include <imgui_impl_sdl2.h>
#include <imgui_impl_opengl3.h>

#ifdef TRACY_ENABLE
#include <tracy/Tracy.hpp>
#else
#define ZoneScoped
#define ZoneScopedNC(NAME, COLOR)
#endif

#include "src/checkerfloor.h"
#include "src/debugrenderer.h"
#include "src/flycam.h"
#include "src/framebuffer.h"
#include "src/import.h"
#include "src/inputbuddy.h"
#include "src/log.h"
#include "src/mesh.h"
#include "src/optionparser.h"
#include "src/program.h"
#include "src/textrenderer.h"
#include "src/texture.h"
#include "src/util.h"

namespace hyper {

enum optionIndex {
  UNKNOWN,
  FULLSCREEN,
  DEBUG,
  HELP,
  FP16,
  FP32,
};

// Custom argument validator for required arguments
struct Arg: public option::Arg {
  static option::ArgStatus Required(const option::Option& option,
                                    bool msg) {
    if (option.arg != 0) {
      return option::ARG_OK;
    }

    if (msg) {
      std::cerr << "Option '" << option.name
                << "' requires an argument\n";
    }
    return option::ARG_ILLEGAL;
  }
};

const float Z_NEAR = 0.1f;
const float Z_FAR = 1000.0f;
const float FOVY = glm::radians(45.0f);

const float MOVE_SPEED = 2.5f;
const float ROT_SPEED = 1.15f;

const glm::vec4 WHITE = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
const glm::vec4 BLACK = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
const int TEXT_NUM_ROWS = 25;

const glm::vec3 EYE_POS = glm::vec3(5.0f, 1.5f, 5.0f);
const glm::vec3 CHAR_POS_OFFSET = glm::vec3(0.0f, 0.5f, 0.0f);

static void Clear(glm::ivec2 windowSize, bool setViewport = true) {
  int width = windowSize.x;
  int height = windowSize.y;
  if (setViewport) {
    glViewport(0, 0, width, height);
  }

  // pre-multiplied alpha blending
  glEnable(GL_BLEND);
  glBlendEquation(GL_FUNC_ADD);
  glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

  glm::vec4 clearColor(0.0f, 0.0f, 0.0f, 1.0f);
  glClearColor(clearColor.r, clearColor.g, clearColor.b, clearColor.a);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  // NOTE: if depth buffer has less then 24 bits, it can mess up
  // splat rendering.
  glEnable(GL_DEPTH_TEST);
}

// Draw a textured quad over the entire screen.
static void RenderDesktop(glm::ivec2 windowSize,
                          std::shared_ptr<Program> desktopProgram,
                          uint32_t colorTexture, bool adjustAspect) {
  int width = windowSize.x;
  int height = windowSize.y;

  glViewport(0, 0, width, height);
  glm::vec4 clearColor(0.0f, 0.0f, 0.0f, 1.0f);
  glClearColor(clearColor.x, clearColor.y, clearColor.z, clearColor.w);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glm::mat4 projMat = glm::ortho(0.0f, static_cast<float>(width),
                                 0.0f, static_cast<float>(height),
                                 -10.0f, 10.0f);

  if (colorTexture > 0) {
    desktopProgram->Bind();
    desktopProgram->SetUniform("modelViewProjMat", projMat);
    desktopProgram->SetUniform("color", glm::vec4(1.0f));

    // use texture unit 0 for colorTexture
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, colorTexture);
    desktopProgram->SetUniform("colorTexture", 0);

    glm::vec2 xyLowerLeft(0.0f, 0.0f);
    glm::vec2 xyUpperRight(static_cast<float>(width),
                           static_cast<float>(height));
    if (adjustAspect) {
      xyLowerLeft = glm::vec2(0.0f, (height - width) / 2.0f);
      xyUpperRight = glm::vec2(static_cast<float>(width),
                               (height + width) / 2.0f);
    }
    glm::vec2 uvLowerLeft(0.0f, 0.0f);
    glm::vec2 uvUpperRight(1.0f, 1.0f);

    float depth = -9.0f;
    glm::vec3 positions[] = {
      glm::vec3(xyLowerLeft, depth),
      glm::vec3(xyUpperRight.x, xyLowerLeft.y, depth),
      glm::vec3(xyUpperRight, depth),
      glm::vec3(xyLowerLeft.x, xyUpperRight.y, depth)
    };
    desktopProgram->SetAttrib("position", positions);

    glm::vec2 uvs[] = {
      uvLowerLeft,
      glm::vec2(uvUpperRight.x, uvLowerLeft.y),
      uvUpperRight,
      glm::vec2(uvLowerLeft.x, uvUpperRight.y)
    };
    desktopProgram->SetAttrib("uv", uvs);

    const size_t NUM_INDICES = 6;
    uint16_t indices[NUM_INDICES] = {0, 1, 2, 0, 2, 3};
    glDrawElements(GL_TRIANGLES, NUM_INDICES, GL_UNSIGNED_SHORT, indices);
  }
}

AppBase::AppBase(MainContext& main_context_in)
    : app_title_("hypercore"),
      main_context_(main_context_in),
      imgui_setup_(false),
      virtual_left_stick_(glm::vec2(0.0f, 0.0f)),
      virtual_right_stick_(glm::vec2(0.0f, 0.0f)),
      mouse_look_stick_(glm::vec2(0.0f, 0.0f)),
      mouse_look_(false),
      virtual_roll_(0.0f),
      virtual_up_(0.0f),
      frame_num_(0),
      quit_callback_(nullptr),
      resize_callback_(nullptr),
      fps_vec_(),
      floor_(nullptr),
      scene_z_up_(false),
      scene_cm_units_(false),
      scene_mat_(1.0f) {
  usage_.push_back({ UNKNOWN, 0, "", "", option::Arg::None,
      "USAGE: assimpbuddy [options]\n\nOptions:" });
  usage_.push_back({ HELP, 0, "h", "help", option::Arg::None,
      "  -h, --help        Print usage and exit." });
  usage_.push_back({ FULLSCREEN, 0, "f", "fullscren", option::Arg::None,
      "  -f, --fullscreen  Launch window in fullscreen." });
  usage_.push_back({ DEBUG, 0, "d", "debug", option::Arg::None,
      "  -d, --debug       Enable verbose debug logging." });
  usage_.push_back({ FP16, 0, "", "fp16", option::Arg::None,
      "  --fp16            Use 16-bit half-precision floating frame "
      "buffer, to reduce color banding artifacts" });
  usage_.push_back({ FP32, 0, "", "fp32", option::Arg::None,
      "  --fp32            Use 32-bit floating point frame buffer, "
      "to reduce color banding even more" });
}

AppBase::~AppBase() {
  if (imgui_setup_) {
    ShutdownImGui();
  }
}

// Note: AppBase::Create() must be defined by the application

AppBase::ParseResult AppBase::ParseArguments(int argc, const char* argv[]) {
  // optionparser expects a null item at the end.
  usage_.push_back({ 0, 0, 0, 0, 0, 0});

  // skip program name
  if (argc > 0) {
    argc--;
    argv++;
  }
  option::Stats stats(usage_.data(), argc, argv);
  std::vector<option::Option> options(stats.options_max);
  std::vector<option::Option> buffer(stats.buffer_max);
  option::Parser parser(usage_.data(), argc, argv, options.data(),
                        buffer.data());

  if (parser.error()) {
    return ERROR_RESULT;
  }

  if (options[HELP]) {
    option::printUsage(std::cout, usage_.data());
    return QUIT_RESULT;
  }

  if (options[FULLSCREEN]) {
    opt_.fullscreen = true;
  }

  if (options[DEBUG]) {
    opt_.debugLogging = true;
  }

  if (options[FP32]) {
    opt_.frameBuffer = Options::FrameBuffer::Float;
  } else if (options[FP16]) {
    opt_.frameBuffer = Options::FrameBuffer::HalfFloat;
  }

  bool unknown_option_found = false;
  for (option::Option* opt = options[UNKNOWN]; opt; opt = opt->next()) {
    unknown_option_found = true;
    std::cout << "Unknown option: "
              << std::string(opt->name, opt->namelen) << "\n";
  }
  if (unknown_option_found) {
    return ERROR_RESULT;
  }

  Log::SetLevel(opt_.debugLogging ? Log::Debug : Log::Warning);

  if (!ParseOptionsImpl(parser, options)) {
    Log::E("ParseOptionsImpl error!");
    return ERROR_RESULT;
  }

  return SUCCESS_RESULT;
}

bool AppBase::Init() {
  InitImGui();
  bool is_framebuffer_srgb_enabled = false;

  floor_ = std::make_shared<CheckerFloor>(
      MakeMat4(1.0f, glm::identity<glm::quat>(),
               glm::vec3(0.0f, 0.0f, 0.0f)));
  if (!floor_->Init(is_framebuffer_srgb_enabled)) {
    Log::E("Error: CheckerFloor::Init failed!\n");
    return false;
  }

#if !defined(__ANDROID__) && !defined(__APPLE__)
  // AJT: ANDROID: TODO: make sure colors are accurate on android.
  if (is_framebuffer_srgb_enabled) {
    // necessary for proper color conversion
    glEnable(GL_FRAMEBUFFER_SRGB);
  } else {
    glDisable(GL_FRAMEBUFFER_SRGB);
  }

  GLenum err = glewInit();
  if (GLEW_OK != err) {
    Log::E("Error: %s\n", glewGetErrorString(err));
    return false;
  }
#endif

  debug_renderer_ = std::make_shared<DebugRenderer>();
  if (!debug_renderer_->Init()) {
    Log::E("DebugRenderer Init failed\n");
    return false;
  }

  text_renderer_ = std::make_shared<TextRenderer>();
  if (!text_renderer_->Init("font/JetBrainsMono-Medium.json",
                            "font/JetBrainsMono-Medium.png")) {
    Log::E("TextRenderer Init failed\n");
    return false;
  }

  glm::mat4 fly_cam_mat = MakeMat4(glm::quat(), glm::vec3(0.0f, 2.0f, 7.0f));
  glm::mat4 floor_mat(1.0f);

  glm::vec3 fly_cam_pos, fly_cam_scale, floor_mat_up;
  glm::quat fly_cam_rot;
  floor_mat_up = glm::vec3(floor_mat[1]);
  Decompose(fly_cam_mat, &fly_cam_scale, &fly_cam_rot, &fly_cam_pos);
  fly_cam_ = std::make_shared<FlyCam>(floor_mat_up, fly_cam_pos, fly_cam_rot,
                                      MOVE_SPEED, ROT_SPEED);

  if (opt_.frameBuffer != Options::FrameBuffer::Default) {
    desktop_program_ = std::make_shared<Program>();
    if (!desktop_program_->LoadVertFrag("shader/desktop_vert.glsl",
                                        "shader/desktop_frag.glsl")) {
      Log::E("Error loading desktop shader!\n");
      return 1;
    }
  }

#ifdef USE_SDL
  input_buddy_ = std::make_shared<InputBuddy>();

  input_buddy_->OnQuit([this]() {
    // forward this back to main
    quit_callback_();
  });

  input_buddy_->OnResize([this](int new_width, int new_height) {
    glViewport(0, 0, new_width, new_height);
    resize_callback_(new_width, new_height);
  });

  input_buddy_->OnKey(SDLK_ESCAPE, [this](bool down, uint16_t mod) {
    quit_callback_();
  });

  input_buddy_->OnKey(SDLK_F1, [this](bool down, uint16_t mod) {
    if (down) {
      opt_.drawFps = !opt_.drawFps;
    }
  });

  input_buddy_->OnKey(SDLK_a, [this](bool down, uint16_t mod) {
    virtual_left_stick_.x += down ? -1.0f : 1.0f;
  });

  input_buddy_->OnKey(SDLK_d, [this](bool down, uint16_t mod) {
    virtual_left_stick_.x += down ? 1.0f : -1.0f;
  });

  input_buddy_->OnKey(SDLK_w, [this](bool down, uint16_t mod) {
    virtual_left_stick_.y += down ? 1.0f : -1.0f;
  });

  input_buddy_->OnKey(SDLK_s, [this](bool down, uint16_t mod) {
    virtual_left_stick_.y += down ? -1.0f : 1.0f;
  });

  input_buddy_->OnKey(SDLK_f, [this](bool down, uint16_t mod) {
    if (down) {
      opt_.useFlyCam = !opt_.useFlyCam;
    }
  });

  input_buddy_->OnKey(SDLK_LEFT, [this](bool down, uint16_t mod) {
    virtual_right_stick_.x += down ? -1.0f : 1.0f;
  });

  input_buddy_->OnKey(SDLK_RIGHT, [this](bool down, uint16_t mod) {
    virtual_right_stick_.x += down ? 1.0f : -1.0f;
  });

  input_buddy_->OnKey(SDLK_UP, [this](bool down, uint16_t mod) {
    virtual_right_stick_.y += down ? 1.0f : -1.0f;
  });

  input_buddy_->OnKey(SDLK_DOWN, [this](bool down, uint16_t mod) {
    virtual_right_stick_.y += down ? -1.0f : 1.0f;
  });

  input_buddy_->OnKey(SDLK_q, [this](bool down, uint16_t mod) {
    virtual_roll_ += down ? -1.0f : 1.0f;
  });

  input_buddy_->OnKey(SDLK_e, [this](bool down, uint16_t mod) {
    virtual_roll_ += down ? 1.0f : -1.0f;
  });

  input_buddy_->OnKey(SDLK_t, [this](bool down, uint16_t mod) {
    virtual_up_ += down ? 1.0f : -1.0f;
  });

  input_buddy_->OnKey(SDLK_g, [this](bool down, uint16_t mod) {
    virtual_up_ += down ? -1.0f : 1.0f;
  });

  input_buddy_->OnKey(SDLK_LSHIFT, [this](bool down, uint16_t mod) {
    shift_down_ = down;
  });

  input_buddy_->OnMouseButton([this](uint8_t button, bool down,
                                     glm::ivec2 pos) {
    if (button == 3) {  // right button
      if (mouse_look_ != down) {
        input_buddy_->SetRelativeMouseMode(down);
      }
      mouse_look_ = down;
    }
  });

  input_buddy_->OnMouseMotion([this](glm::ivec2 pos, glm::ivec2 rel) {
    if (mouse_look_) {
      const float kMouseSensitivity = 0.001f;
      mouse_look_stick_.x += rel.x * kMouseSensitivity;
      mouse_look_stick_.y -= rel.y * kMouseSensitivity;
    }
  });
#endif  // USE_SDL

  fps_text_ = text_renderer_->AddScreenTextWithDropShadow(
      glm::ivec2(0, 1), static_cast<int>(TEXT_NUM_ROWS), WHITE, BLACK,
      "fps:");

  return InitImpl();
}

void AppBase::ProcessEvent(const SDL_Event& event) {
#ifdef USE_SDL
  ImGui_ImplSDL2_ProcessEvent(&event);
  input_buddy_->ProcessEvent(event);
#endif
}

void AppBase::UpdateFps(float fps) {
  std::string text = "fps: " + std::to_string(static_cast<int>(fps));
  text_renderer_->RemoveText(fps_text_);
  fps_text_ = text_renderer_->AddScreenTextWithDropShadow(
      glm::ivec2(0, 1), TEXT_NUM_ROWS, WHITE, BLACK, text);
}

bool AppBase::Process(float dt) {
  ZoneScopedNC("Process", tracy::Color::DarkGreen);

#ifdef USE_SDL
  InputBuddy::Joypad joypad = input_buddy_->GetJoypad();

  if (opt_.useFlyCam) {
    float roll = 0.0f;
    roll -= joypad.lb ? 1.0f : 0.0f;
    roll += joypad.rb ? 1.0f : 0.0f;
    fly_cam_->Process(
        glm::clamp(joypad.left_stick + virtual_left_stick_, -1.0f, 1.0f),
        glm::clamp(joypad.right_stick + virtual_right_stick_, -1.0f, 1.0f) +
            mouse_look_stick_ / (dt > 0.0f ? dt : 1.0f),
        glm::clamp(roll + virtual_roll_, -1.0f, 1.0f),
        glm::clamp(virtual_up_, -1.0f, 1.0f), dt);
    mouse_look_stick_ = glm::vec2(0.0f, 0.0f);

    glm::vec2 zero(0.0f, 0.0f);
    return ProcessImpl(dt, zero, zero, zero, zero);
  } else {
#else
  {
#endif
    ZoneScopedNC("Inference", tracy::Color::Red4);

    glm::mat4 char_mat = glm::identity<glm::mat4>();
    glm::vec3 char_pos = glm::vec3(0.0f, 0.0f, 0.0f) + CHAR_POS_OFFSET;
    glm::mat4 camera_mat = glm::inverse(
        glm::lookAt(EYE_POS, char_pos, glm::vec3(0.0f, 1.0f, 0.0f)));

    glm::vec2 ls = glm::clamp(joypad.left_stick + virtual_left_stick_,
                              -1.0f, 1.0f);
    glm::vec2 rs = glm::clamp(joypad.right_stick + virtual_right_stick_,
                              -1.0f, 1.0f);

    glm::vec2 left_stick(ls.x, -ls.y);
    glm::vec2 right_stick(rs.x, -rs.y);

    // move with left stick, aim with right stick.
    glm::vec2 move_dir = ToPlane(XformVec(camera_mat, FromPlane(left_stick)));
    glm::vec2 face_dir;
    if (glm::length(rs) > 0.1f) {
      face_dir = ToPlane(XformVec(camera_mat, FromPlane(right_stick)));
    } else {
      face_dir = move_dir;
    }

    return ProcessImpl(dt, left_stick, right_stick, move_dir, face_dir);
  }
}

bool AppBase::Render(float dt, const glm::ivec2& window_size) {
  ZoneScopedNC("Render", tracy::Color::Blue);

  int width = window_size.x;
  int height = window_size.y;
  bool retVal = true;

  {
    // lazy init of fbo, fbo is only used for HalfFloat, Float option.
    if (opt_.frameBuffer != Options::FrameBuffer::Default &&
        fbo_size_ != window_size) {
      fbo_ = std::make_shared<FrameBuffer>();

      Texture::Params tex_params;
      tex_params.min_filter = FilterType::Nearest;
      tex_params.mag_filter = FilterType::Nearest;
      tex_params.s_wrap = WrapType::ClampToEdge;
      tex_params.t_wrap = WrapType::ClampToEdge;
      if (opt_.frameBuffer == Options::FrameBuffer::HalfFloat) {
        fbo_color_tex_ = std::make_shared<Texture>(window_size.x,
                                                   window_size.y,
                                                   GL_RGBA16F, GL_RGBA,
                                                   GL_HALF_FLOAT,
                                                   tex_params);
      } else if (opt_.frameBuffer == Options::FrameBuffer::Float) {
        fbo_color_tex_ = std::make_shared<Texture>(window_size.x,
                                                   window_size.y,
                                                   GL_RGBA32F, GL_RGBA,
                                                   GL_FLOAT,
                                                   tex_params);
      } else {
        Log::E("BAD opt_.frameBuffer type!\n");
      }

      fbo_->AttachColor(fbo_color_tex_);

      fbo_size_ = window_size;
    }

    if (opt_.frameBuffer != Options::FrameBuffer::Default && fbo_) {
      fbo_->Bind();
    }

    Clear(window_size, true);

    glm::mat4 camera_mat;

    if (opt_.useFlyCam) {
      camera_mat = fly_cam_->GetCameraMat();
    } else {
      glm::vec3 char_pos = glm::vec3(0.0f, 0.0f, 0.0f) + CHAR_POS_OFFSET;
      camera_mat = glm::inverse(
          glm::lookAt(EYE_POS, char_pos, glm::vec3(0.0f, 1.0f, 0.0f)));
    }

    // apply scene_mat
    camera_mat = glm::inverse(scene_mat_) * camera_mat;

    glm::vec4 viewport(0.0f, 0.0f, static_cast<float>(width),
                       static_cast<float>(height));
    glm::vec2 near_far(Z_NEAR, Z_FAR);
    glm::mat4 proj_mat = glm::perspective(
        FOVY, static_cast<float>(width) / static_cast<float>(height),
        Z_NEAR, Z_FAR);

    // draw the origin axes.
    glm::mat4 origin_mat(1.0f);
    debug_renderer_->Transform(origin_mat, scene_cm_units_ ? 100.0f : 1.0f);

    // keep the floor aligned to world up, and world units.
    if (!scene_hide_floor_) {
      floor_->Render(scene_mat_ * camera_mat, proj_mat, viewport, near_far);
    }

    if (opt_.drawDebug) {
      debug_renderer_->Render(camera_mat, proj_mat, viewport, near_far);
    }

    if (opt_.drawFps) {
      text_renderer_->Render(camera_mat, proj_mat, viewport, near_far);
    }

    if (!RenderImpl(camera_mat, proj_mat, viewport, near_far)) {
      Log::E("Error in RenderImpl\n");
      retVal = false;
    }

    if (opt_.frameBuffer != Options::FrameBuffer::Default && fbo_) {
      // render fbo colorTexture as a full screen quad to the default fbo
      glBindFramebuffer(GL_FRAMEBUFFER, 0);
      Clear(window_size, true);
      RenderDesktop(window_size, desktop_program_,
                    fbo_->GetColorTexture()->texture, false);
    }
  }

  debug_renderer_->EndFrame();

  RenderImGui();

  frame_num_++;

  return retVal;
}

void AppBase::OnQuit(const VoidCallback& cb) {
  quit_callback_ = cb;
}

void AppBase::OnResize(const ResizeCallback& cb) {
  resize_callback_ = cb;
}

void AppBase::InitImGui() {
  imgui_setup_ = true;
#ifdef USE_SDL
  // Setup Dear ImGui context
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO(); (void)io;
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

  // Save imgui.ini in the same directory as the executable
  char* base_path = SDL_GetBasePath();
  if (base_path) {
    imgui_ini_path_ = std::string(base_path) + "imgui.ini";
    SDL_free(base_path);
    io.IniFilename = imgui_ini_path_.c_str();
  }

#ifdef __linux__
  // font is too small on linux
  io.FontGlobalScale = 2.0f;  // Scale everything by 2x
#endif

  // Setup Dear ImGui style
  ImGui::StyleColorsDark();

  // Setup Platform/Renderer backends
  SDL_Window* window = SDL_GL_GetCurrentWindow();
  SDL_GLContext gl_context = SDL_GL_GetCurrentContext();
  ImGui_ImplSDL2_InitForOpenGL(window, gl_context);

  const char* glsl_version = "#version 130";
#ifdef __APPLE__
  glsl_version = "#version 150";
#endif
  ImGui_ImplOpenGL3_Init(glsl_version);
#endif
}

void AppBase::ShutdownImGui() {
#ifdef USE_SDL
  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplSDL2_Shutdown();
  ImGui::DestroyContext();
#endif
}

void AppBase::RenderImGui() {
#ifdef USE_SDL
  // Start the Dear ImGui frame
  ImGui_ImplOpenGL3_NewFrame();
  ImGui_ImplSDL2_NewFrame();
  ImGui::NewFrame();

  // Main menu bar
  if (ImGui::BeginMainMenuBar()) {
    if (ImGui::BeginMenu("Options")) {
      ImGui::MenuItem("Draw FPS", nullptr, &opt_.drawFps);
      ImGui::MenuItem("Draw Debug", nullptr, &opt_.drawDebug);
      ImGui::MenuItem("Use Fly Cam", nullptr, &opt_.useFlyCam);
      ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Scene")) {
      ImGui::MenuItem("Hide Floor", nullptr, &scene_hide_floor_);
      bool scene_mat_changed = false;
      if (ImGui::MenuItem("Z-up", nullptr, &scene_z_up_)) {
        scene_mat_changed = true;
      }
      if (ImGui::MenuItem("CM Units", nullptr, &scene_cm_units_)) {
        scene_mat_changed = true;
      }
      if (scene_mat_changed) {
        glm::quat scene_rot(1.0f, 0.0f, 0.0f, 0.0f);
        if (scene_z_up_) {
          scene_rot = glm::quat(-0.7071f, 0.7071f, 0.0f, 0.0f);
        }
        glm::vec3 scene_scale(1.0f, 1.0f, 1.0f);
        if (scene_cm_units_) {
          scene_scale = glm::vec3(0.01f, 0.01f, 0.01f);
        }
        scene_mat_ = MakeMat4(scene_scale, scene_rot,
                              glm::vec3(0.0f, 0.0f, 0.0f));
      }
      ImGui::EndMenu();
    }
    if (!RenderImGuiMenuBarImpl()) {
      Log::E("Error in RenderImGuiMenuBarImpl()!\n");
    }
    ImGui::EndMainMenuBar();
  }

  if (!RenderImGuiImpl()) {
    Log::E("Error in RenderImGuiImpl()!\n");
  }

  // Rendering
  ImGui::Render();
  ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
#endif
}

}  // namespace hyper
