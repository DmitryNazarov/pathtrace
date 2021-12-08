# Simple raytracer based on  San DiegoX: CSE167x Computer Graphics course

![dragon](./data/dragon.png)
![scene6](./data/scene6.png)
![sphere](./data/sphere.png)

# Build
For example using x64 Native Tools Command Prompt for VS 2019:
```
mkdir build
cd build
cmake -G "Visual Studio 16 2019" ..
cmake --build .
```

# Run
```
vulkan_pathtracer.exe -scene data\dragon.test
```

# Hotkeys:
F1 - save image to `<name-of-scene>.png` located near executable. There is the FPS camera controlled by arrows.
