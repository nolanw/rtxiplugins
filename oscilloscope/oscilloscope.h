#ifndef OSCILLOSCOPE_H
#define OSCILLOSCOPE_H

#include "scope.h"

#include <event.h>
#include <fifo.h>
#include <io.h>
#include <mutex.h>
#include <plugin.h>
#include <qdialog.h>
#include <rt.h>
#include <settings.h>

class QButtonGroup;
class QCheckBox;
class QComboBox;
class QGroupBox;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QTabWidget;

namespace Oscilloscope {
    
    struct channel_info {
        QString name;
        IO::Block *block;
        IO::flags_t type;
        size_t index;
        double previous;
    };

    class Panel;
    class Properties;

    class Plugin : public QObject, public ::Plugin::Object, public RT::Thread {

        Q_OBJECT

        friend class Panel;

    public:

        static Plugin *getInstance(void);

    public slots:

        void createOscilloscopePanel(void);

    protected:

        virtual void doDeferred(const Settings::Object::State &);
        virtual void doLoad(const Settings::Object::State &);
        virtual void doSave(Settings::Object::State &) const;

    private:

        Plugin(void);
        ~Plugin(void);
        Plugin(const Plugin &) {};
        Plugin &operator=(const Plugin &) { return *getInstance(); };

        static Plugin *instance;

        void removeOscilloscopePanel(Panel *);

        int menuID;
        std::list<Panel *> panelList;

    }; // Plugin

}; // class Oscilloscope

#endif // OSCILLOSCOPE_H
