To build the libmltl library on Windows:

Install the necessary build tools and dependencies:
    - Install pybind11 using pip: pip install pybind11
    - Install a C++ compiler like Visual Studio. I have Microsoft Visual C++ 2015-2022 Redistributable (x64) installed, which includes the MSVC compiler.
    - Install CMake which is used by the libmltl build system.
    - Create a CMakeLists.txt file in the libmltl source directory. This file tells CMake how to build the library.

Steps to build the extension:

1. Open "x64 Native Tools Command Prompt for VS 2022" from the Start menu. This opens a command prompt with the MSVC compiler environment set up.

2. Navigate to the directory containing your CMakeLists.txt file:
   ```
   cd path\to\libmltl
   ```

3. Create a "build" directory and navigate into it:
   ```
   mkdir build
   cd build
   ```

4. Run CMake to configure the project and generate the Visual Studio solution:
   ```
   cmake .. -G "Visual Studio 17 2022" -A x64
   ```
   This assumes you have Visual Studio 2022 installed. If you have a different version, replace "17 2022" with the appropriate version number.

5. Build the project:
   ```
   cmake --build . --config Release
   ```
   This will compile the extension module in Release mode. You can change "Release" to "Debug" if you want to build with debug symbols.

6. The compiled extension module (libmltl.pyd) will be in the Release (or Debug) directory. You can copy this file to your Python project directory or somewhere on the Python module search path.

7. You should now be able to import the module in your Python code:
   ```python
   import libmltl
   ```

TLDR:
1. Open the VS command prompt
2. Navigate to the CMakeLists.txt directory 
3. Create a build directory and navigate into it
4. Run CMake to configure and generate the VS solution
5. Build the project with cmake --build
6. Copy the .pyd file to where Python can find it
7. Import the module in Python


#Updates made to the code
1. Created CMakeLists.txt file in the libmltl directory
2. Modified parser.cc file to include the 
    - Following headers:
        #include <filesystem>
        #include <tuple>
    - read_trace_file function
        From const string &trace_file_path to const fs::path &trace_file_path. This allows the function to directly accept a std::filesystem::path object.
3. Modified parser.h file to include the 
    - Following headers:
        #include <filesystem>
    - read_trace_file function
        From const std::string & to const std::filesystem::path & to match the definition in the parser.cc file.

To use the libmltl module in your Python code - copy the generated libmltl.cp311-win_amd64.pyd file to your Python project directory or to a location where Python can find it (such as the site-packages directory).
Then, you should be able to import the libmltl module in your Python code:
```python
import libmltl
```