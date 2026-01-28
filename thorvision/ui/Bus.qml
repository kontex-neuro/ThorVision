pragma Singleton

import QtQuick

QtObject {
    signal camera_selected(int index)
    signal status_notify(string message)
}
