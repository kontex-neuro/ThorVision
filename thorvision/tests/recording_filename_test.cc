#include "RecordingFilename.h"

#include <iostream>

int main()
{
    const struct { const char *input; const char *expected; } cases[] = {
        {"Industrial Camera: KonteX-XC-U3", "Industrial Camera_ KonteX-XC-U3"},
        {"camera<>:\"/\\|?*", "camera_________"},
        {"Camera 1", "Camera 1"},
        {"camera. ", "camera"},
        {"...", "Camera"},
        {"", "Camera"},
        {"CON", "_CON"},
        {"lpt9.txt", "_lpt9.txt"},
        {"COM10", "COM10"},
        {"mouse\ncam", "mouse_cam"},
    };
    for (const auto &test : cases) {
        const auto actual = recording_filename_component(QString::fromUtf8(test.input));
        if (actual != QString::fromUtf8(test.expected)) {
            std::cerr << "Unexpected recording filename: " << actual.toStdString() << '\n';
            return 1;
        }
    }
    const auto unicode_name = QString::fromUtf8("\xE7\x9B\xB8\xE6\xA9\x9F");
    if (recording_filename_component(unicode_name) != unicode_name) return 1;
    std::cout << "Recording filename regression cases passed\n";
    return 0;
}
