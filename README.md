# Rapid Engine

![GitHub contributors](https://img.shields.io/github/contributors/EmilDimov93/Rapid-Engine)
![GitHub commits](https://img.shields.io/github/commit-activity/m/EmilDimov93/Rapid-Engine)
![Total Commits](https://img.shields.io/github/commit-activity/t/EmilDimov93/Rapid-Engine)
![GitHub last commit](https://img.shields.io/github/last-commit/EmilDimov93/Rapid-Engine)
![GitHub license](https://img.shields.io/github/license/EmilDimov93/Rapid-Engine)
![Zlib License](https://img.shields.io/badge/license-Zlib-lightgrey)
![GitHub repo size](https://img.shields.io/github/repo-size/EmilDimov93/Rapid-Engine)
![C](https://img.shields.io/badge/language-C-555555?style=flat-square)
![Raylib](https://img.shields.io/badge/Library-Raylib-ff69b4?style=flat-square)

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

| Category   | Type                    |
|------------|-------------------------|
| Variable   | Create number           |
| Variable   | Create string           |
| Variable   | Create bool             |
| Variable   | Create color            |
| Event      | Event Start             |
| Event      | Event Tick              |
| Event      | Event On Button         |
| Event      | Create Custom Event     |
| Event      | Call Custom Event       |
| Get        | Get variable            |
| Get        | Get Screen Width        |
| Get        | Get Screen Height       |
| Get        | Get Mouse X             |
| Get        | Get Mouse Y             |
| Get        | Get Random Number       |
| Set        | Set variable            |
| Set        | Set Background          |
| Set        | Set FPS                 |
| Flow       | Branch                  |
| Flow       | Loop                    |
| Flow       | Delay                   |
| Flow       | Flip Flop               |
| Flow       | Break                   |
| Flow       | Return                  |
| Sprite     | Create sprite           |
| Sprite     | Set Sprite Position     |
| Sprite     | Set Sprite Rotation     |
| Sprite     | Set Sprite Texture      |
| Sprite     | Set Sprite Size         |
| Sprite     | Spawn sprite            |
| Sprite     | Destroy sprite          |
| Sprite     | Move To                 |
| Sprite     | Force                   |
| Prop       | Draw Prop Texture       |
| Prop       | Draw Prop Rectangle     |
| Prop       | Draw Prop Circle        |
| Logical    | Comparison              |
| Logical    | Gate                    |
| Logical    | Arithmetic              |
| Debug      | Print To Log            |
| Debug      | Draw Debug Line         |
| Literal    | Literal number          |
| Literal    | Literal string          |
| Literal    | Literal bool            |
| Literal    | Literal color           |


## 🧪 In Development

- Sprite collision events
- Hitbox editor improvements
- Sprite sheet editor
- Helper function nodes
- Exporting game as .exe
- Cross platform support
- New CoreGraph nodes


## ⚠️ Note: Rapid Engine is not packaged for public release yet, but you can build and run it manually:

```bash
gcc unity.c raylib/lib/libraylib.a -o RapidEngine.exe -Iraylib/include -lopengl32 -lgdi32 -lwinmm -mwindows
```

```bash
./RapidEngine.exe
```
