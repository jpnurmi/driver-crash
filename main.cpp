#include <windows.h>
#include <GL/gl.h>

#include <QFileInfo>
#include <QGuiApplication>
#include <QOpenGLContext>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QSurfaceFormat>

#include <sentry.h>

#include <cstring>

#if !defined(_M_X64)
#error The driver crash trigger requires Windows x64
#endif

static void crashDriver()
{
    if (IsDebuggerPresent() || !QOpenGLContext::currentContext()) {
        qWarning("A current OpenGL context and no attached debugger are required");
        return;
    }

    const struct {
        const char *tag;
        GLenum name;
    } strings[] = {
        {"gpu.vendor", GL_VENDOR},
        {"gpu.renderer", GL_RENDERER},
        {"gpu.opengl.version", GL_VERSION},
        {"gpu.glsl.version", GL_SHADING_LANGUAGE_VERSION},
    };
    for (const auto &string : strings) {
        if (const auto value = glGetString(string.name)) {
            sentry_set_tag(string.tag, reinterpret_cast<const char *>(value));
        }
    }

    auto entry = wglGetProcAddress("glCreateShader");
    MEMORY_BASIC_INFORMATION memory{};
    HMODULE module = nullptr;
    if (!entry || !VirtualQuery(reinterpret_cast<void *>(entry), &memory, sizeof(memory))
        || memory.Type != MEM_IMAGE
        || !GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS
                | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCWSTR>(entry), &module)) {
        qWarning("OpenGL driver entry is unavailable or uses an unsupported dispatch stub");
        return;
    }
    wchar_t path[MAX_PATH];
    const DWORD length = GetModuleFileNameW(module, path, MAX_PATH);
    if (!length || length == MAX_PATH) {
        qWarning("Cannot identify the OpenGL driver");
        return;
    }
    const QString name = QFileInfo(QString::fromWCharArray(path, int(length))).fileName();
    if (name.compare("opengl32.dll", Qt::CaseInsensitive) == 0
        || name.contains("opengl32sw", Qt::CaseInsensitive)) {
        qWarning("A hardware OpenGL driver is required");
        return;
    }
    qInfo("Crashing %s at %p on thread %lu", qPrintable(name), entry, GetCurrentThreadId());
    sentry_set_tag("gpu.driver", name.toUtf8().constData());

    DWORD protection = 0;
    if (!VirtualProtect(reinterpret_cast<void *>(entry), 2, PAGE_EXECUTE_READWRITE, &protection)) {
        qWarning("Cannot modify the driver entry in this process");
        return;
    }
    const unsigned char fastfail[] = {0xcd, 0x29};
    std::memcpy(reinterpret_cast<void *>(entry), fastfail, sizeof(fastfail));
    DWORD ignored = 0;
    if (!VirtualProtect(reinterpret_cast<void *>(entry), 2, protection, &ignored)
        || !FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void *>(entry), 2)) {
        qWarning("Cannot finalize the driver crash trigger");
        TerminateProcess(GetCurrentProcess(), 2);
        return;
    }

    // int 29h consumes the first x64 argument in RCX as the fast-fail reason
    using CreateShader = GLuint(APIENTRY *)(GLenum);
    reinterpret_cast<CreateShader>(entry)(FAST_FAIL_FATAL_APP_EXIT);
}

int main(int argc, char **argv)
{
    auto options = sentry_options_new();
    sentry_options_set_debug(options, 1);
    if (sentry_init(options) != 0) {
        return 1;
    }

    QCoreApplication::setAttribute(Qt::AA_UseDesktopOpenGL);
    QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);
    QSurfaceFormat format;
    format.setVersion(3, 3);
    format.setProfile(QSurfaceFormat::CoreProfile);
    QSurfaceFormat::setDefaultFormat(format);

    QGuiApplication app(argc, argv);

    QQmlApplicationEngine engine(QUrl("qrc:/Main.qml"));
    if (engine.rootObjects().isEmpty()) {
        sentry_close();
        return 1;
    }

    auto window = qobject_cast<QQuickWindow *>(engine.rootObjects().value(0));
    QObject::connect(window, SIGNAL(crashRequestedChanged()), window, SLOT(update()));
    QObject::connect(window, &QQuickWindow::beforeSynchronizing, window, [window] {
        if (window->property("crashRequested").toBool()) {
            window->setProperty("crashRequested", false);
            crashDriver();
        }
    }, Qt::DirectConnection);

    int res = app.exec();
    sentry_close();
    return res;
}
