find src/ -type f -iname *.h  -exec clang-format-3.8 -i -style=file {} \;
find src/ -type f -iname *.cpp  -exec clang-format-3.8 -i -style=file {} \;