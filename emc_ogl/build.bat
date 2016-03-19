set MeshLoaderPath=..\..\MeshLoader\
emcc main.cpp^
 %MeshLoaderPath%MeshLoader.cpp^
 %MeshLoaderPath%types.cpp^
 %MeshLoaderPath%MeshSurface.cpp -v -s USE_GLFW=3 -s NO_EXIT_RUNTIME=1^
 -I..\..\..\3rdparty\glm^
 -I..\..\..\3rdparty\GSL\include^
 -i%MeshLoaderPath% -DGLM -std=c++14 --embed-file asset^
 -o main.js
