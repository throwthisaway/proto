set MeshLoaderPath=..\..\MeshLoader\
set MeshExportPath=..\..\MeshExport\
emcc main.cpp^
 %MeshLoaderPath%MeshLoader.cpp^
 %MeshLoaderPath%types.cpp^
 %MeshExportPath%MeshCommon\FileReader.cpp^
 %MeshExportPath%MeshCommon\File.cpp^
 %MeshExportPath%MeshView2005\Img\Tga.cpp^
 %MeshLoaderPath%MeshSurface.cpp^
 -v -s USE_GLFW=3 -s NO_EXIT_RUNTIME=1^
 -I..\..\..\3rdparty\glm^
 -I..\..\..\3rdparty\GSL\include^
 -i%MeshLoaderPath%^
 -i%MeshExportPath%MeshCommon^
 -i%MeshExportPath%MeshView2005\Img^
 -DGLM -std=c++14 --embed-file asset^
 -o main.js
