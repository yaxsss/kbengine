## vs2022编译报错"T_ESCAPE_URLENCODED 未声明的标识符"
打开`x64 Native Tools Command Prompt vs 2022`, 进入`kbengine\kbe\src\lib\dependencies\apr`目录中
执行`cl.exe /nologo /W3 /EHsc /Od /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /FD /I ".\include" /Fo.\LibD\gen_test_char /Fe.\LibD\gen_test_char.exe .\tools\gen_test_char.c `
执行`.\LibD\gen_test_char.exe > .\include\apr_escape_test_char.h`

