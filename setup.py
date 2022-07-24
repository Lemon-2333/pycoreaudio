from distutils.core import setup, Extension

modcoreaudio = Extension("CoreAudio",
                    sources = ["libpycoreaudio.cpp"],
                    extra_link_args=["-framework", "CoreAudio", "-framework", "CoreFoundation"],
                    extra_compile_args=["-std=c++11"])

setup (name = "CoreAudio",
       version = "1.0",
       description = "An interface for Apple's CoreAudio",
       ext_modules = [modcoreaudio])