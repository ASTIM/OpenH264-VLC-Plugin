# Copyright (c) 2016, A.ST.I.M. S.r.l.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# * Redistributions of source code must retain the above copyright notice, this
#   list of conditions and the following disclaimer.
#
# * Redistributions in binary form must reproduce the above copyright notice,
#   this list of conditions and the following disclaimer in the documentation
#   and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

QT =
CONFIG -= qt c++11

TARGET = libopenh264_plugin
TEMPLATE = lib

SOURCES += codec_openh264.c

DISTFILES += \
    $$files(third_party/openh264/*.h, true) \
    $$files(third_party/openh264/*.hpp, true) \
    $$files(third_party/openh264/*.c, true) \
    $$files(third_party/openh264/*.cpp, true)

exists($$PWD/third_party/openh264-bin/openh264*.dll)|\
exists($$PWD/third_party/openh264-bin/libopenh264*.a)|\
exists($$PWD/third_party/openh264-bin/libopenh264*.so)|\
exists($$PWD/third_party/openh264-bin/libopenh264*.dylib) {
    message(Found openh264 binary library)
    LIBS += -L"$$PWD/third_party/openh264-bin/"
    LIBS += -lopenh264
    INCLUDEPATH += $$PWD/third_party/openh264/codec/api/svc
} else:\
exists($$shadowed($$PWD)/third_party/build/bin/*openh264*.dll)|\
exists($$shadowed($$PWD)/third_party/build/lib/*openh264*.lib)|\
exists($$shadowed($$PWD)/third_party/build/lib/*openh264*.a) {
    message(Found openh264 compiled library)
    LIBS += -L"$$shadowed($$PWD)/third_party/build/bin/"
    LIBS += -L"$$shadowed($$PWD)/third_party/build/lib/"
    LIBS += -lopenh264
    INCLUDEPATH += $$shadowed($$PWD)/third_party/build/include/wels
} else {
    CONFIG(debug, debug|release) {
        OPENH264.buildtype = BUILDTYPE=Debug
    } else {
        OPENH264.buildtype = BUILDTYPE=Release
    }

    equals(QMAKE_HOST.arch, x86) {
        OPENH264.arch = ARCH=i386
    } else:equals(QMAKE_HOST.arch, x86_64) {
        OPENH264.arch = ARCH=x86_64
    } else {
        error(UNKNOWN ARCH!)
    }

    equals(QMAKE_HOST.os, Windows) {
        OPENH264.os = OS=mingw_nt
    } else {
        error(UNKNOWN OS!)
    }

    OPENH264.cd_cmd = cd $$shadowed($$PWD) && mkdir -p openh264 && cd openh264
    OPENH264.build_cmd = make -j$$QMAKE_HOST.cpu_count -f $$PWD/third_party/openh264/Makefile $$OPENH264.buildtype $$OPENH264.arch $$OPENH264.os libraries
    OPENH264.install_cmd = make -j$$QMAKE_HOST.cpu_count -f $$PWD/third_party/openh264/Makefile $$OPENH264.buildtype $$OPENH264.arch $$OPENH264.os install-static PREFIX=$$shadowed($$PWD)/third_party/build/

    BUILD_CMD += $$escape_expand(\n)
        BUILD_CMD += $$OPENH264.cd_cmd
        BUILD_CMD += $$escape_expand(\n)
        BUILD_CMD += $$OPENH264.build_cmd
        BUILD_CMD += $$escape_expand(\n)
        BUILD_CMD += $$OPENH264.install_cmd
        BUILD_CMD += $$escape_expand(\n)

    !system(make -v) {
        message("$$escape_expand(\n)\
        OpenH264 will be compiled from sources. If you dont want this - download OpenH264 compiled binary and sources (see: https://github.com/cisco/openh264/releases )\
        and place binary in $$PWD/third_party/openh264-bin/ folder and correspondig source code to $$PWD/third_party/openh264/ folder (to include headers) and then rebuild entire project.")

        third_party-build.target = third_party-build
        third_party-build.commands = $$BUILD_CMD
        PRE_TARGETDEPS += third_party-build
        QMAKE_EXTRA_TARGETS += third_party-build
    } else {
        BUILD_CMD = $$join(BUILD_CMD, " ","$$escape_expand(\n)\
        As your system does not have `make` in the PATH you can build OpenH264 with msys2 by copy-pasting following command and rebuild entire project:\
        $$escape_expand(\n)\
        Note: you can skip this step if you have already have compiled OpenH264 library or downloaded binary library version from cisco site \
        (see: https://github.com/cisco/openh264/releases ). In this case just add binaries to $$PWD/third_party/openh264-bin/ folder and \
        correspondig source code to $$PWD/third_party/openh264/ folder (to include headers) and then rebuild entire project)\
        $$escape_expand(\n)"\
        )
        message($$BUILD_CMD)
        error(OpenH264 not found)
    }
}

# TODO: Add vlc bin check
exists($$PWD/third_party/vlc-dist/libvlccore.dll):\
exists($$PWD/third_party/vlc-dist/libvlc.dll):\
exists($$PWD/third_party/vlc-dist/sdk/include):{
    message(Found vlc dist package)
    LIBS += -L"$$PWD/third_party/vlc-dist/"
    LIBS += -lvlc -lvlccore
    INCLUDEPATH += $$PWD/third_party/vlc-dist/sdk/include
    INCLUDEPATH += $$PWD/third_party/vlc-dist/sdk/include/vlc/plugins/
} else {

}
