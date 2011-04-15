#ifndef PANEL_H_33HZ64CI
#define PANEL_H_33HZ64CI

#include "scope.h"

#include <fifo.h>
#include <io.h>
#include <rt.h>
#include <settings.h>

#include <qwidget.h>

#include <vector>

namespace Oscilloscope
{

class Properties;

class Panel : public Scope, public RT::Thread, public virtual Settings::Object {

    Q_OBJECT

    friend class Properties;

public:

    Panel(QWidget *);
    virtual ~Panel(void);

    void execute(void);

    bool setInactiveSync(void);
    void flushFifo(void);
    void adjustDataSize(void);

    void doDeferred(const Settings::Object::State &);
    void doLoad(const Settings::Object::State &);
    void doSave(Settings::Object::State &) const;

public slots:

    void showProperties(void);
    void timeoutEvent(void);

protected:

    void mouseDoubleClickEvent(QMouseEvent *);
    void mousePressEvent(QMouseEvent *);

private:

    Fifo fifo;
    Properties *properties;
    std::vector<IO::Block *> blocks;

};

}

#endif /* end of include guard: PANEL_H_33HZ64CI */
