QT =

#QMAKE_CFLAGS = -x c -fvisibility=hidden
#QMAKE_CFLAGS += -std=gnu99

#QMAKE_LFLAGS += -static -static-libgcc -static-libstdc++

CONFIG -= qt

TARGET = libopenh264_plugin
TEMPLATE = lib

SOURCES += codec_openh264.cpp

exists($$PWD/third_party/openh264-bin/openh264*.dll)|\
exists($$PWD/third_party/openh264-bin/libopenh264*.a)|\
exists($$PWD/third_party/openh264-bin/libopenh264*.so)|\
exists($$PWD/third_party/openh264-bin/libopenh264*.dylib) {
    message(Found openh264 binary library!)
    LIBS += -L$$PWD/third_party/openh264-bin/
    LIBS += -lopenh264
    INCLUDEPATH += $$PWD/third_party/openh264/codec/api/svc
} else:\
exists($$shadowed($$PWD)/third_party/build/bin/*openh264*.dll)|\
exists($$shadowed($$PWD)/third_party/build/lib/*openh264*.lib)|\
exists($$shadowed($$PWD)/third_party/build/lib/*openh264*.a) {
    message(Found openh264 compiled library!)
    LIBS += -L$$shadowed($$PWD)/third_party/build/bin/
    LIBS += -L$$shadowed($$PWD)/third_party/build/lib/
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
    }
}
