#include "imageio.h"

#include <QGuiApplication>

#include <cstdint>
#include <cstdlib>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size)
{
    static int argc = 1;
    static char name[] = "fuzz_load_image";
    static char* argv[] = {name, nullptr};
    static QGuiApplication* app = [] {
        qputenv("QT_QPA_PLATFORM", "offscreen");
        return new QGuiApplication(argc, argv);
    }();
    (void)app;
    const app::LoadResult r = app::loadImageData(QByteArray(reinterpret_cast<const char*>(data), int(size)));
    if (r.ok && (r.image.width() <= 0 || r.image.height() <= 0))
        std::abort();
    return 0;
}
