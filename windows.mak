
fbxsdkinclude="C:\Program Files\Autodesk\FBX\FBX SDK\2019.0\include"
fbxsdklib="C:\Program Files\Autodesk\FBX\FBX SDK\2019.0\lib\vs2015\x64\release"
disabled=/wd4477\
		 /wd4244\
		 /wd4334\
		 /wd4305\
		 /wd4101\
		 /D_CRT_SECURE_NO_WARNINGS

.SILENT:
all: start bindir shaders game2 end

bindir:
	if not exist bin (mkdir bin)
	if not exist bin\SDL2.dll (copy usr\lib\*.dll bin\ >nul 2>&1)

shaders:
	usr\bin\lineify.exe -d src\shaders\* > src\shaders.h

wbfbx:
	cl /nologo /O2 /TP /Gd /MT /I$(fbxsdkinclude) \
	/EHsc /fp:fast /W3 /c $(disabled)\
	/DWB_FBX_IMPLEMENTATION src\wb_fbx.cc
	lib /NOLOGO wb_fbx.obj

game2: src/main.c
	cl /nologo /TC /Zi /Gd /MT /I"usr/include" /I$(fbxsdkinclude) \
	/EHsc /fp:fast /W3 $(disabled)\
		$? /Fe"bin/pbr_test.exe" /Fd"bin/pbr_test.pdb" \
		/link /NOLOGO /INCREMENTAL:NO /SUBSYSTEM:CONSOLE /LIBPATH:"usr/lib"\
		/LIBPATH:$(fbxsdklib) \
		kernel32.lib user32.lib SDL2.lib SDL2main.lib wb_fbx.lib libfbxsdk-mt.lib

start:
	usr\bin\ctime.exe -begin usr/bin/pbr_test.ctm

end:
	del *.obj >nul 2>&1
	usr\bin\ctime.exe -end usr/bin/pbr_test.ctm
