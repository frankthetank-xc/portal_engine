# Portal Rendering Engine
This is a simple rendering engine for "2.5d" environments.

The program represents the world as a collection of vertices sectors. Each sector consists of a list of connected vertices in counterclockwise orientation, meaning that a sector can only be a single continuous polygon.

# Usage Information
## Compiling
* Written for Linux operating systems. Will probably work with cygwin/mingw
* Requires *gcc*, *SDL2*, *SDL2-image*, and *make* to build

## Execution
The only command line argument right now is the name of the file containing level data.

# Controls
- **W / Up Arrow**: Move forward
- **S / Down Arrow**: Move backward
- **A**: Strafe left
- **D**: Strafe right
- **Left Arrow**: Turn left
- **Right Arrow**: Turn right
- **Space**: Jump
- **Shift**: Walk
- **C**: Crouch
- **Mouse**: Look up/down and left/right
- **Q**: Quit
- **F**: Toggle fullscreen mode

# World definition files
A world is represented in a simple text document. The document contains the following three things, where all values in brackets are variables.

*id values must start at 0 for vertices/sectors and must increase by 1 for every row. They are discarded by the engine but used for ease of writing these files by hand.*

### Vertices
v [id] [x] [x]

### Sectors
s [id] [floorHeight] [ceilHeight] [floorTexture] [ceilTexture] [brightness] [number of walls] {list of walls}
#### Walls
[vertex0] [vertex1] [neighbor] [textureLow] [textureMiddle] [textureHigh]

*For the neighboring sectors, an x represents a wall (no neighbor), while a numbered sector id represents a portal to a neighboring sector*

### Player Info
p [starting x coordinate] [starting y coordinate] [starting sector ID]
