#include "panel.h"
#include "oscilloscope.h"
#include "properties.h"

#include <cmath>
#include <sstream>

#include <qcombobox.h>
#include <qcursor.h>
#include <qpopupmenu.h>
#include <qtimer.h>

namespace {

    class SyncEvent : public RT::Event {

    public:

        int callback(void) { return 0; };

    }; // class SyncEvent
} // namespace

Oscilloscope::Panel::Panel(QWidget *parent)
    : Scope(parent,Qt::WDestructiveClose), RT::Thread(0), fifo(10*1048576) {

    setCaption(QString::number(getID())+" Oscilloscope");

    adjustDataSize();
    properties = new Properties(this);

    QTimer *otimer = new QTimer(this);
    QObject::connect(otimer,SIGNAL(timeout(void)),this,SLOT(timeoutEvent(void)));
    otimer->start(25);

    resize(800,450);
    show();

    setActive(true);
}

Oscilloscope::Panel::~Panel(void) {
    while(getChannelsBegin() != getChannelsEnd())
        delete reinterpret_cast<struct channel_info *>(removeChannel(getChannelsBegin()));

    Plugin::getInstance()->removeOscilloscopePanel(this);
    delete properties;
}

void Oscilloscope::Panel::execute(void) {
  void *buffer;
  size_t nchans = getChannelCount();

  if(nchans == 0)
    return;

  size_t idx = 0;
  size_t token = nchans;
  double data[nchans];

  std::list<Scope::Channel>::iterator i, end;
  struct channel_info *info;
  for(i = getChannelsBegin(), end = getChannelsEnd(); i != end ; ++i) {
    info = reinterpret_cast<struct channel_info *>(i->getInfo());

    double value = info->block->getValue(info->type, info->index);

    if(i == getTriggerChannel()) {
      double thresholdValue = getTriggerThreshold();

      if((thresholdValue > value && thresholdValue < info->previous) ||
         (thresholdValue < value && thresholdValue > info->previous)) {
        Event::Object event(Event::THRESHOLD_CROSSING_EVENT);
        int direction = (thresholdValue > value) ? 1 : -1;

        event.setParam("block",info->block);
        event.setParam("type",&info->type);
        event.setParam("index",&info->index);
        event.setParam("direction",&direction);
        event.setParam("threshold",&thresholdValue);

        Event::Manager::getInstance()->postEventRT(&event);
      }
    }

    info->previous = value;
    data[idx++] = value;
  }

  fifo.write(&token,sizeof(token));
  fifo.write(data,sizeof(data));
}

bool Oscilloscope::Panel::setInactiveSync(void) {
    bool active = getActive();

    setActive(false);

    SyncEvent event;
    RT::System::getInstance()->postEvent(&event);

    return active;
}

void Oscilloscope::Panel::flushFifo(void) {
    char junk;
    while(fifo.read(&junk,sizeof(junk),false));
}

void Oscilloscope::Panel::adjustDataSize(void) {
    double period = RT::System::getInstance()->getPeriod()*1e-6;
    size_t size = ceil(getDivT()*getDivX()/period)+1;

    setDataSize(size);
}

void Oscilloscope::Panel::showProperties(void) {
    properties->move(mapToGlobal(rect().center())-properties->rect().center());
    properties->show();
    properties->raise();
}

void Oscilloscope::Panel::timeoutEvent(void) {
    size_t size;

    while(fifo.read(&size,sizeof(size),false)) {
        double data[size];
        if(fifo.read(data,sizeof(data)))
            setData(data,size);
    }
}

void Oscilloscope::Panel::mouseDoubleClickEvent(QMouseEvent *e) {
    if(e->button() == Qt::LeftButton && getTriggerChannel() != getChannelsEnd()) {
        double scale = height()/(getTriggerChannel()->getScale()*getDivY());
        double offset = getTriggerChannel()->getOffset();
        double threshold = (height()/2-e->y())/scale-offset;

        setTrigger(getTriggerDirection(),threshold,getTriggerChannel(),getTriggerHolding(),getTriggerHoldoff());
        properties->showDisplayTab();
    }
}

void Oscilloscope::Panel::mousePressEvent(QMouseEvent *e) {
    if(e->button() == Qt::RightButton) {
        QPopupMenu menu(this);
        menu.setItemChecked(menu.insertItem("Pause",this,SLOT(togglePause(void))),paused());
        menu.insertItem("Properties",this,SLOT(showProperties(void)));
        menu.insertSeparator();
        menu.insertItem("Exit",this,SLOT(close(void)));
        menu.setMouseTracking(true);
        menu.exec(QCursor::pos());
    }
}

void Oscilloscope::Panel::doDeferred(const Settings::Object::State &s) {
    bool active = setInactiveSync();

    for(size_t i = 0, nchans = s.loadInteger("Num Channels");i < nchans;++i) {
        std::ostringstream str;
        str << i;

        IO::Block *block = dynamic_cast<IO::Block *>(Settings::Manager::getInstance()->getObject(s.loadInteger(str.str()+" ID")));
        if(!block) continue;

        struct channel_info *info = new struct channel_info;

        info->block = block;
        info->type = s.loadInteger(str.str()+" type");
        info->index = s.loadInteger(str.str()+" index");
        info->name = QString::number(block->getID())+" "+block->getName(info->type,info->index);
        info->previous = 0.0;

        std::list<Scope::Channel>::iterator chan = insertChannel(info->name,
                                                                 s.loadDouble(str.str()+" scale"),
                                                                 s.loadDouble(str.str()+" offset"),
                                                                 QPen(QColor(s.loadString(str.str()+" pen color")),
                                                                      s.loadInteger(str.str()+" pen width"),
                                                                      static_cast<Qt::PenStyle>(s.loadInteger(str.str()+" pen style"))),
                                                                 info);

        setChannelLabel(chan,info->name+" "+properties->scaleList->text(static_cast<int>(round(3*(log10(1/chan->getScale())+1)))).simplifyWhiteSpace());
    }

    flushFifo();
    setActive(active);
}

void Oscilloscope::Panel::doLoad(const Settings::Object::State &s) {
    setDataSize(s.loadInteger("Size"));
    setDivXY(s.loadInteger("DivX"),s.loadInteger("DivY"));
    setDivT(s.loadDouble("DivT"));

    if(s.loadInteger("Maximized"))
        showMaximized();
    else if(s.loadInteger("Minimized"))
        showMinimized();

    if(paused() != s.loadInteger("Paused"))
        togglePause();

    setRefresh(s.loadInteger("Refresh"));

    resize(s.loadInteger("W"),
           s.loadInteger("H"));
    parentWidget()->move(s.loadInteger("X"),
                         s.loadInteger("Y"));
}

void Oscilloscope::Panel::doSave(Settings::Object::State &s) const {
    s.saveInteger("Size",getDataSize());
    s.saveInteger("DivX",getDivX());
    s.saveInteger("DivY",getDivY());
    s.saveDouble("DivT",getDivT());

    if(isMaximized())
        s.saveInteger("Maximized",1);
    else if(isMinimized())
        s.saveInteger("Minimized",1);

    s.saveInteger("Paused",paused());
    s.saveInteger("Refresh",getRefresh());

    QPoint pos = parentWidget()->pos();
    s.saveInteger("X",pos.x());
    s.saveInteger("Y",pos.y());
    s.saveInteger("W",width());
    s.saveInteger("H",height());

    s.saveInteger("Num Channels",getChannelCount());
    size_t n = 0;
    for(std::list<Channel>::const_iterator i = getChannelsBegin(),end = getChannelsEnd();i != end;++i) {
        std::ostringstream str;
        str << n++;

        const struct channel_info *info = reinterpret_cast<const struct channel_info *>(i->getInfo());

        s.saveInteger(str.str()+" ID",info->block->getID());
        s.saveInteger(str.str()+" type",info->type);
        s.saveInteger(str.str()+" index",info->index);

        s.saveDouble(str.str()+" scale",i->getScale());
        s.saveDouble(str.str()+" offset",i->getOffset());

        QPen pen = i->getPen();
        s.saveString(str.str()+" pen color",pen.color().name());
        s.saveInteger(str.str()+" pen style",pen.style());
        s.saveInteger(str.str()+" pen width",pen.width());
    }
}
