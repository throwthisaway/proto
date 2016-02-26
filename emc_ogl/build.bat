set MeshLoaderPath=..\MeshLoader\
emcc main.cpp^
 %MeshLoaderPath%MeshLoader.cpp^
 %MeshLoaderPath%types.cpp^
 %MeshLoaderPath%MeshSurface.cpp -o main.html -v -s USE_GLFW=3 -s NO_EXIT_RUNTIME=1 -I..\..\3rdparty\glm -i%MeshLoaderPath% -DGLM -std=c++14
