#include <QtQml/qqmlextensionplugin.h>
#include <QtQml/qqml.h>
#include <gst/gst.h>
#include "gstqt6d3d11videoitem.h"

GST_DEBUG_CATEGORY (gst_qt6_d3d11_debug);

class GstQt6D3D11Plugin : public QQmlExtensionPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID QQmlExtensionInterface_iid)

public:
    void registerTypes(const char *uri) override
    {
        Q_ASSERT(uri == QLatin1String("org.freedesktop.gstreamer.Qt6D3D11VideoItem"));
        
        GST_DEBUG_CATEGORY_INIT (gst_qt6_d3d11_debug, "qt6d3d11", 0, "qt6d3d11");
        
        qmlRegisterType<GstQt6D3D11VideoItem>(uri, 1, 0, "GstD3D11Qt6VideoItem");
    }
};

#include "plugin_init.moc"
