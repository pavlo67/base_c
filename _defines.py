#!/usr/bin/env python3
from pathlib import Path
import json

src = Path(__file__).with_name("_defines.h")
dst = Path(__file__).with_name("_defines_copy.h")

# читаємо і відкидаємо рядки, що починаються з //
lines = src.read_text(encoding="utf-8").splitlines()
filtered_lines = []

for line in lines:
    if line.lstrip().startswith("//"):
        continue
    filtered_lines.append(line)

text = "\n".join(filtered_lines) + "\n"

# перетворюємо в коректний C++ string literal
cpp_string = json.dumps(text, ensure_ascii=False)

dst.write_text(
    f"""#ifndef VISION__DEFINES_COPY_H
#define VISION__DEFINES_COPY_H

#include <string_view>

inline constexpr std::string_view VISION_DEFINES_COPY = {cpp_string};

#endif
""",
    encoding="utf-8",
)