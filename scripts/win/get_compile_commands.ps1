cmake -B ./build-clangd -G "Ninja" -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
Move-Item ./build-clangd/compile_commands.json ./ -Force
