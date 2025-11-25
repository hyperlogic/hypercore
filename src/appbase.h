/*
    Copyright (c) 2024 Anthony J. Thibault
    This software is licensed under the MIT License. See LICENSE for
    more details.
*/

#pragma once

#include <functional>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "src/maincontext.h"
#include "src/optionparser.h"


union SDL_Event;

namespace hyper {

class CheckerFloor;
class DebugRenderer;
class FlyCam;
struct FrameBuffer;
class InputBuddy;
class Program;
class TextRenderer;
struct Texture;

class AppBase {
 public:
  // Factory function that applications must implement
  static std::shared_ptr<AppBase> Create(MainContext& main_context);

  explicit AppBase(MainContext& mainContextIn);
  virtual ~AppBase();

  enum ParseResult {
    SUCCESS_RESULT,
    ERROR_RESULT,
    QUIT_RESULT
  };

  const std::string& app_title() const { return app_title_; }
  ParseResult ParseArguments(int argc, const char* argv[]);
  bool Init();
  bool IsFullscreen() const { return opt_.fullscreen; }
  void UpdateFps(float fps);
  void ProcessEvent(const SDL_Event& event);
  void InitImGui();
  void ShutdownImGui();
  void RenderImGui();
  bool Process(float dt);
  bool Render(float dt, const glm::ivec2& window_size);

  using VoidCallback = std::function<void()>;
  void OnQuit(const VoidCallback& cb);

  using ResizeCallback = std::function<void(int, int)>;
  void OnResize(const ResizeCallback& cb);

  struct Options {
    enum class FrameBuffer {
      Default,
      HalfFloat,
      Float
    };
    bool fullscreen = false;
    FrameBuffer frameBuffer = FrameBuffer::Default;
    bool drawDebug = true;
    bool debugLogging = false;
    bool drawFps = true;
    bool useFlyCam = true;
  };

  virtual bool ParseOptionsImpl(const option::Parser& parser,
                                const std::vector<option::Option>& options) = 0;
  virtual bool InitImpl() = 0;
  virtual bool ProcessImpl(float dt,
                           glm::vec2 left_stick,
                           glm::vec2 right_stick,
                           glm::vec2 face_dir,
                           glm::vec2 move_dir) = 0;
  virtual bool RenderImpl(const glm::mat4& camera_mat,
                          const glm::mat4& proj_mat,
                          const glm::vec4& viewport,
                          const glm::vec2& near_far) = 0;
  virtual bool RenderImGuiMenuBarImpl() = 0;
  virtual bool RenderImGuiImpl() = 0;

 protected:
  std::string app_title_;
  MainContext& main_context_;
  bool imgui_setup_;
  std::vector<option::Descriptor> usage_;
  Options opt_;
  std::shared_ptr<DebugRenderer> debug_renderer_;
  std::shared_ptr<TextRenderer> text_renderer_;

  std::shared_ptr<FlyCam> fly_cam_;

  std::shared_ptr<Program> desktop_program_;
  std::shared_ptr<FrameBuffer> fbo_;
  glm::ivec2 fbo_size_ = {0, 0};
  std::shared_ptr<Texture> fbo_color_tex_;

  std::shared_ptr<InputBuddy> input_buddy_;

  glm::vec2 virtual_left_stick_;
  glm::vec2 virtual_right_stick_;
  glm::vec2 mouse_look_stick_;
  bool mouse_look_;
  bool shift_down_;
  float virtual_roll_;
  float virtual_up_;
  uint32_t fps_text_;
  uint32_t frame_num_;

  VoidCallback quit_callback_;
  ResizeCallback resize_callback_;

  std::vector<float> fps_vec_;

  std::shared_ptr<CheckerFloor> floor_;

  bool scene_z_up_ = false;
  bool scene_cm_units_ = false;
  glm::mat4 scene_mat_;
};

}  // namespace hyper
