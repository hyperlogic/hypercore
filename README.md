hypercore
------------
I wouldn't call it an engine, but it's a bunch of classes that are useful for rendering simple scenes and simple assets.

Might grow into something larger in the future, but right now it's the minimum.

It's setup to build a static library, for inclusion into an 'app' as a submodule.

Requirements (Linux)
--------------------
sudo apt install build-essential libgl1-mesa-dev libglu1-mesa-dev


What's in it
---------------
* AppBase - Base class for an application, it provides the following:
  * A way to set the app_title
  * A hook to parse command line arguments
  * Callbacks for Quit & Resize events.
  * A Process function with dual stick arguments.
  * A callback for rendering ImGUi menus & UI.
  * A callback for rendering 3d content (with updated matrices)
* Import/Asset - Provides a way to load animated mesh from fbx/glb files.
  It will build a list of Mesh/BoneMesh instances along with a
  Node hierarchy and a list of Anim instances.
* Mesh/BoneMesh - provides access and Rendering for a triangle mesh.
* CheckerFloor - renders a floor quad with a texture
* DebugRenderer - provides interface to draw Lines and Transforms
* FrameBuffer - wrapper around OpenGL FBOs.
* Flycam - a smooth motion model to fly around the camera based on user input.
* Image - a way to load png files
* Log - a logging framework
* Program - a wrapper around Vertex/Fragment/Compute shader processing and linking.
* StateMachine - An extendable finite state machine template.
* TextRenderer - A way to load fonts and render text.
* Util - Math utilities not provided by glm.
* VertexBuffer - A wrapper around OpenGL buffers, incudes VAOs as well.


How to use it
-------------------
To create a project that uses hypercore, copy or git submodule it and add it to your CMakeLists.txt
```
add_subdirectory(hypercore)

add_executable(YOUR_PROJECT_NAME src/app.cpp)
target_link_libraries(YOUR_PROJECT_NAME PRIVATE hypercore)
```

Where app.h looks something like this:

```c++
#pragma once

#include "hypercore/src/appbase.h"
#include "hypercore/src/maincontext.h"

namespace hyper {
class App : public AppBase {
 public:
  explicit App(MainContext& mainContextIn);
  ~App();

  bool ParseOptionsImpl(const option::Parser& parser,
                        const std::vector<option::Option>& options) override;
  bool InitImpl() override;
  bool ProcessImpl(float dt,
                   glm::vec2 left_stick,
                   glm::vec2 right_stick,
                   glm::vec2 face_dir,
                   glm::vec2 move_dir) override;
  bool RenderImpl(const glm::mat4& camera_mat,
                  const glm::mat4& proj_mat,
                  const glm::vec4& viewport,
                  const glm::vec2& near_far) override;
  bool RenderImGuiMenuBarImpl() override;
  bool RenderImGuiImpl() override;
 protected:
  // your scene data here: Assets, Mesh etc.
}
}
```

And a app.cc that looks something like this:

```c++
#include "src/app.h"
#include <imgui.h>
#include "hypercore/src/debugrenderer.h"

namespace hyper {

// IMPORTANT: Define the factory function
std::shared_ptr<AppBase> AppBase::Create(MainContext& main_context) {
  return std::make_shared<App>(main_context);
}

App::App(MainContext& main_context_in) :
    AppBase(main_context_in) {}

App::~App() {}

bool App::ParseOptionsImpl(const option::Parser& parser,
                           const std::vector<option::Option>& options) {
  // check for command line args here
  return true;
}

bool App::InitImpl() {
  // initialize your scene here, load assets etc.
  return true;
}

bool App::ProcessImpl(float dt, glm::vec2 left_stick, glm::vec2 right_stick,
                      glm::vec2 face_dir, glm::vec2 move_dir) {
  // animate your scene here.
  return true;
}

bool App::RenderImpl(const glm::mat4& camera_mat, const glm::mat4& proj_mat,
                     const glm::vec4& viewport, const glm::vec2& near_far) {
  // render your scene here.
  return true;
}

bool App::RenderImGuiMenuBarImpl() {
  // Add DearImGui menus here
  return true;
}

bool App::RenderImGuiImpl() {
  // Render your app ui here.
  return true;
}
```


