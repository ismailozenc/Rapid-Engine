# Rapid Engine

**Rapid Engine** is a game engine written in C, using the Raylib library, that includes a visual scripting language called **CoreGraph**. Designed for speed and full control with node-based logic.

<img width="2559" height="1528" alt="image" src="https://github.com/user-attachments/assets/57f2e0e4-f970-4a57-afbe-eed5b6e57feb" />


## ⚡ Core Features

- 💡 **CoreGraph**: Node-based scripting language
- 🖼️ Custom UI with the Raylib library
- 🎯 Real-time interaction and graph editing
- ⚙️ Basic language constructs such as variables, arithmetic, logic, conditionals, and loops
- 🎮 Spawning and moving sprites
- ✂️ Hitbox editor: Visual polygon hitbox creation
- 💾 Save, build and run systems
---


## 🔍 In-Depth Capabilities

- Interpreter for the CoreGraph language
- Viewport that shows either the CoreGraph Editor or the Game Screen
- File management system in bottom menu
- Real-time log for error and debug messages
- Variables menu for viewing information about the current variables and their values
- Different pin types: flow, dropdown, text box and more
- Custom window management top menu with settings


## 🧩 All node types:

- Create num
- Create string
- Create bool
- Create color
- Create sprite
- Get var
- Set var
- Start
- Loop Tick
- On Button
- Create custom
- Call custom
- Spawn
- Destroy
- Move To
- Force
- Branch
- Loop
- Comparison
- Gate
- Arithmetic
- Prop Texture
- Prop Rectangle
- Prop Circle
- Print
- Draw Line
- Literal num
- Literal string
- Literal bool
- Literal color


## 🧪 In Development

- Sprite collision events
- Hitbox editor improvements
- Sprite sheet editor
- Helper function nodes
- Exporting game as .exe
- Ongoing CoreGraph improvements and optimizations


## ⚠️ Note: Rapid Engine is not packaged for public release yet, but you can build and run it manually:

```gcc Engine.c ProjectManager.c CGEditor.c Nodes.c Interpreter.c HitboxEditor.c raylib/lib/libraylib.a -o RapidEngine.exe -Iraylib/include -lopengl32 -lgdi32 -lwinmm -mwindows; ./RapidEngine.exe```

Currently, the engine hardcodes the opened file path, even though there is a working project manager. Loading other files requires manual code changes.
