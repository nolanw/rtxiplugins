#include "oscilloscope.h"
#include "panel.h"
#include "properties.h"

#include <debug.h>
#include <main_window.h>
#include <qcheckbox.h>
#include <qcombobox.h>
#include <qcursor.h>
#include <qgroupbox.h>
#include <qhbox.h>
#include <qhbuttongroup.h>
#include <qlabel.h>
#include <qlayout.h>
#include <qlineedit.h>
#include <qpainter.h>
#include <qpen.h>
#include <qpopupmenu.h>
#include <qpushbutton.h>
#include <qradiobutton.h>
#include <qspinbox.h>
#include <qtabwidget.h>
#include <qtimer.h>
#include <qvalidator.h>
#include <rt.h>
#include <workspace.h>

#include <cmath>
#include <sstream>

extern "C" Plugin::Object *createRTXIPlugin(void *) {
    return Oscilloscope::Plugin::getInstance();
}

Oscilloscope::Plugin::Plugin(void) {
    menuID = MainWindow::getInstance()->createControlMenuItem("Oscilloscope",this,SLOT(createOscilloscopePanel(void)));
}

Oscilloscope::Plugin::~Plugin(void) {
    MainWindow::getInstance()->removeControlMenuItem(menuID);
    while(panelList.size())
        delete panelList.front();
    instance = 0;
}

void Oscilloscope::Plugin::createOscilloscopePanel(void) {
    Panel *panel = new Panel(MainWindow::getInstance()->centralWidget());
    panelList.push_back(panel);
}

void Oscilloscope::Plugin::removeOscilloscopePanel(Oscilloscope::Panel *panel) {
    panelList.remove(panel);
}

void Oscilloscope::Plugin::doDeferred(const Settings::Object::State &s) {
    size_t i = 0;
    for(std::list<Panel *>::iterator j = panelList.begin(),end = panelList.end();j != end;++j)
        (*j)->deferred(s.loadState(QString::number(i++)));
}

void Oscilloscope::Plugin::doLoad(const Settings::Object::State &s) {
    for(size_t i = 0;i < static_cast<size_t>(s.loadInteger("Num Panels"));++i) {
        Panel *panel = new Panel(MainWindow::getInstance()->centralWidget());
        panelList.push_back(panel);
        panel->load(s.loadState(QString::number(i)));
    }
}

void Oscilloscope::Plugin::doSave(Settings::Object::State &s) const {
    s.saveInteger("Num Panels",panelList.size());
    size_t n = 0;
    for(std::list<Panel *>::const_iterator i = panelList.begin(),end = panelList.end();i != end;++i)
        s.saveState(QString::number(n++),(*i)->save());
}

static Mutex mutex;
Oscilloscope::Plugin *Oscilloscope::Plugin::instance = 0;

Oscilloscope::Plugin *Oscilloscope::Plugin::getInstance(void) {
    if(instance)
        return instance;

    /*************************************************************************
     * Seems like alot of hoops to jump through, but allocation isn't        *
     *   thread-safe. So effort must be taken to ensure mutual exclusion.    *
     *************************************************************************/

    Mutex::Locker lock(&::mutex);
    if(!instance)
        instance = new Plugin();

    return instance;
}
