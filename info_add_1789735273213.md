# CMake executable output

When `base` is configured by itself, regular executables go to `base/_bin`, test executables to `base/_test`, and probe executables to `base/_probe`. When included by the parent project, these directories are under the parent source root instead. Multi-configuration generators use these exact directories without a configuration suffix.
