# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.22)

# پایتونِ venv رسمی IDF را قفل می‌کنیم تا cmake (مثلاً تسک CMake در VS Code که
# بدون -DPYTHON اجرا می‌شود) به پایتونِ PATH نیفتد — روی این ماشین پایتونِ
# سیستم جابه‌جا شده و هر python.exe که از PATH بیاید قابل اعتماد نیست.
# شرط EXISTS یعنی روی هر ماشین دیگر بدون این مسیر، رفتار عادی حفظ می‌شود.
if(EXISTS "C:/Espressif/python_env/idf6.0_py3.14_env/Scripts/python.exe")
    set(PYTHON "C:/Espressif/python_env/idf6.0_py3.14_env/Scripts/python.exe"
        CACHE FILEPATH "IDF python interpreter" FORCE)
endif()

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(Smart-Home)
