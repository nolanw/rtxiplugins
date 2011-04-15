#include "properties.h"
#include "oscilloscope.h"
#include "panel.h"

#include <main_window.h>
#include <workspace.h>

#include <cmath>

#include <qhbuttongroup.h>
#include <qcheckbox.h>
#include <qcombobox.h>
#include <qgroupbox.h>
#include <qhbox.h>
#include <qlabel.h>
#include <qlayout.h>
#include <qlineedit.h>
#include <qpainter.h>
#include <qpushbutton.h>
#include <qradiobutton.h>
#include <qspinbox.h>
#include <qtabwidget.h>
#include <qvalidator.h>

Oscilloscope::Properties::Properties(Oscilloscope::Panel *parent)
    : QDialog(MainWindow::getInstance()), panel(parent) {
    setCaption(QString::number(parent->getID())+" Oscilloscope Properties");

    QBoxLayout *layout = new QVBoxLayout(this);

    tabWidget = new QTabWidget(this);
    layout->addWidget(tabWidget);
    QObject::connect(tabWidget,SIGNAL(currentChanged(QWidget *)),
                     this,SLOT(showTab(void)));

    QHBox *hbox = new QHBox(this);
    layout->addWidget(hbox);

    QPushButton *applyButton = new QPushButton("Apply",hbox);
    QObject::connect(applyButton,SIGNAL(clicked(void)),this,SLOT(apply(void)));
    QPushButton *okayButton = new QPushButton("Okay",hbox);
    QObject::connect(okayButton,SIGNAL(clicked(void)),this,SLOT(okay(void)));
    QPushButton *cancelButton = new QPushButton("Cancel",hbox);
    QObject::connect(cancelButton,SIGNAL(clicked(void)),this,SLOT(close(void)));

    createChannelTab();
    createDisplayTab();
    createAdvancedTab();
}

Oscilloscope::Properties::~Properties(void) {}

void Oscilloscope::Properties::receiveEvent(const ::Event::Object *event) {
    if(event->getName() == Event::IO_BLOCK_INSERT_EVENT) {
        IO::Block *block = reinterpret_cast<IO::Block *>(event->getParam("block"));

        if(block) {
            // Update the list of blocks
            blockList->insertItem(block->getName()+QString(" ")+QString::number(block->getID()));
            panel->blocks.push_back(block);

            if(blockList->count() == 1)
                buildChannelList();
        }
    } else if(event->getName() == Event::IO_BLOCK_REMOVE_EVENT) {
        IO::Block *block = reinterpret_cast<IO::Block *>(event->getParam("block"));

        if(block) {
            // Find the index of the block in the blocks vector
            size_t index;
            for(index = 0;index < panel->blocks.size() && panel->blocks[index] != block;++index);

            if(index < panel->blocks.size()) {
                bool found = false;
                // Stop displaying channels coming from the removed block

                for(std::list<Scope::Channel>::iterator i = panel->getChannelsBegin(),end = panel->getChannelsEnd();i != end;) {
                    struct channel_info *info = reinterpret_cast<struct channel_info *>(i->getInfo());
                    if(info->block == block) {
                        found = true;

                        // If triggering on this channel disable triggering
                        if(i->getLabel() == panel->getTriggerChannel()->getLabel()) {
                            panel->setTrigger(Scope::NONE,
                                              panel->getTriggerThreshold(),
                                              panel->getChannelsEnd(),
                                              panel->getTriggerHolding(),
                                              panel->getTriggerHoldoff());
                            showDisplayTab();
                        }

                        struct channel_info *info = reinterpret_cast<struct channel_info *>(i->getInfo());

                        std::list<Scope::Channel>::iterator chan = i++;

                        bool active = panel->setInactiveSync();
                        panel->removeChannel(chan);
                        panel->flushFifo();
                        panel->setActive(active);

                        delete info;
                    } else
                        ++i;
                }

                // Update the list of blocks
                size_t current = blockList->currentItem();
                blockList->removeItem(index);
                panel->blocks.erase(panel->blocks.begin()+index);

                if(current == index) {
                    blockList->setCurrentItem(0);
                    buildChannelList();
                }

                showTab();
            } else
                DEBUG_MSG("Oscilloscope::Properties::receiveEvent : removed block never inserted\n");
        }
    } else if(event->getName() == Event::RT_POSTPERIOD_EVENT) {
        panel->setPeriod(RT::System::getInstance()->getPeriod()*1e-6);
        panel->adjustDataSize();

        showTab();
    }
}

void Oscilloscope::Properties::closeEvent(QCloseEvent *e) {
    e->ignore();
    hide();
}

void Oscilloscope::Properties::activateChannel(bool active) {
    bool enable = active && blockList->count() && channelList->count();
    displayBox->setEnabled(enable);
    lineBox->setEnabled(enable);
}

void Oscilloscope::Properties::apply(void) {
    switch(tabWidget->currentPageIndex()) {
      case 0:
          applyChannelTab();
          break;
      case 1:
          applyDisplayTab();
          break;
      case 2:
          applyAdvancedTab();
          break;
      default:
          ERROR_MSG("Oscilloscope::Properties::showTab : invalid tab\n");
    }
}

void Oscilloscope::Properties::buildChannelList(void) {
    channelList->clear();
    if(!blockList->count())
        return;

    if(blockList->currentItem() < 0)
        blockList->setCurrentItem(0);

    IO::Block *block = panel->blocks[blockList->currentItem()];
    IO::flags_t type;
    switch(typeList->currentItem()) {
      case 0:
          type = Workspace::INPUT;
          break;
      case 1:
          type = Workspace::OUTPUT;
          break;
      case 2:
          type = Workspace::PARAMETER;
          break;
      case 3:
          type = Workspace::STATE;
          break;
      default:
          ERROR_MSG("Oscilloscope::Properties::buildChannelList : invalid type selection\n");
          type = Workspace::INPUT;
    }

    for(size_t i = 0;i < block->getCount(type);++i)
        channelList->insertItem(block->getName(type,i));

    showChannelTab();
}

void Oscilloscope::Properties::okay(void) {
    apply();
    close();
}

void Oscilloscope::Properties::showTab(void) {
    switch(tabWidget->currentPageIndex()) {
      case 0:
          showChannelTab();
          break;
      case 1:
          showDisplayTab();
          break;
      case 2:
          showAdvancedTab();
          break;
      default:
          ERROR_MSG("Oscilloscope::Properties::showTab : invalid tab\n");
    }
}

void Oscilloscope::Properties::applyAdvancedTab(void) {
    //panel->setDataSize(sizeSpin->value());
    panel->setDivXY(divXSpin->value(),divYSpin->value());

    panel->adjustDataSize();

    showAdvancedTab();
}

void Oscilloscope::Properties::applyChannelTab(void) {
    if(blockList->count() <= 0 || channelList->count() <= 0)
        return;

    IO::Block *block = panel->blocks[blockList->currentItem()];
    IO::flags_t type;
    switch(typeList->currentItem()) {
      case 0:
          type = Workspace::INPUT;
          break;
      case 1:
          type = Workspace::OUTPUT;
          break;
      case 2:
          type = Workspace::PARAMETER;
          break;
      case 3:
          type = Workspace::STATE;
          break;
      default:
          ERROR_MSG("Oscilloscope::Properties::applyChannelTab : invalid type\n");
          typeList->setCurrentItem(0);
          type = Workspace::INPUT;
    }

    struct channel_info *info;
    std::list<Scope::Channel>::iterator i = panel->getChannelsBegin();
    for(std::list<Scope::Channel>::iterator end = panel->getChannelsEnd();i != end;++i) {
        info = reinterpret_cast<struct channel_info *>(i->getInfo());
        if(info->block == block && info->type == type && info->index == static_cast<size_t>(channelList->currentItem())) break;
    }

    if(!activateButton->isOn()) {
        if(i != panel->getChannelsEnd()) {

            // If triggering on this channel disable triggering
            if(i->getLabel() == panel->getTriggerChannel()->getLabel())
                panel->setTrigger(Scope::NONE,
                                  panel->getTriggerThreshold(),
                                  panel->getChannelsEnd(),
                                  panel->getTriggerHolding(),
                                  panel->getTriggerHoldoff());

            bool active = panel->setInactiveSync();
            panel->removeChannel(i);
            panel->flushFifo();
            panel->setActive(active);

            delete info;
        }
    } else {
        if(i == panel->getChannelsEnd()) {
            info = new struct channel_info;

            info->block = block;
            info->type = type;
            info->index = channelList->currentItem();
            info->previous = 0.0;

            info->name = QString::number(block->getID())+" "+block->getName(type,channelList->currentItem());

            bool active = panel->setInactiveSync();

            i = panel->insertChannel(info->name+" 2 V/div",2.0,0.0,QPen(Qt::red,1,Qt::SolidLine),info);

            panel->flushFifo();
            panel->setActive(active);
        }

        double scale;
        switch(scaleList->currentItem()%3) {
          case 0:
              scale = pow(10,1-scaleList->currentItem()/3);
              break;
          case 1:
              scale = 5*pow(10,-scaleList->currentItem()/3);
              break;
          case 2:
              scale = 2*pow(10,-scaleList->currentItem()/3);
              break;
          default:
              ERROR_MSG("Oscilloscope::Properties::applyChannelTab : invalid scale selection\n");
              scale = 2.0;
        }
        if(scale != i->getScale()) {
            panel->setChannelScale(i,scale);
            panel->setChannelLabel(i,info->name+" "+scaleList->currentText().simplifyWhiteSpace());
        }
        panel->setChannelOffset(i,offsetEdit->text().toDouble()*pow(10,-3*offsetList->currentItem()));

        QPen pen;
        switch(colorList->currentItem()) {
          case 0:
              pen.setColor(Qt::red);
              break;
          case 1:
              pen.setColor(Qt::yellow);
              break;
          case 2:
              pen.setColor(Qt::green);
              break;
          case 3:
              pen.setColor(Qt::blue);
              break;
          case 4:
              pen.setColor(Qt::magenta);
              break;
          case 5:
              pen.setColor(Qt::cyan);
              break;
          case 6:
              pen.setColor(Qt::black);
              break;
          default:
              ERROR_MSG("Oscilloscope::Properties::applyChannelTab : invalid color selection\n");
              pen.setColor(Qt::red);
        }
        pen.setWidth(widthList->currentItem()+1);
        switch(styleList->currentItem()) {
          case 0:
              pen.setStyle(Qt::SolidLine);
              break;
          case 1:
              pen.setStyle(Qt::DashLine);
              break;
          case 2:
              pen.setStyle(Qt::DotLine);
              break;
          case 3:
              pen.setStyle(Qt::DashDotLine);
              break;
          case 4:
              pen.setStyle(Qt::DashDotDotLine);
              break;
          default:
              ERROR_MSG("Oscilloscope::Properties::applyChannelTab : invalid style selection\n");
              pen.setStyle(Qt::SolidLine);
        }
        panel->setChannelPen(i,pen);


        //i->label.setColor(i->getPen().color());
        //for(std::vector<QCanvasLine>::iterator j = i->lines.begin(),end = i->lines.end();j != end;++j)
        //    j->setPen(info->pen);

        /*
        if(&*i == panel->trigChan)
            panel->trigLine->setPoints(0,panel->val2pix(panel->trigThresh,*i),
                                       width(),panel->val2pix(panel->trigThresh,*i));
        */
    }

    showChannelTab();
}

void Oscilloscope::Properties::applyDisplayTab(void) {
    panel->setRefresh(refreshSpin->value());

    double divT;
    if(timeList->currentItem()%3 == 1)
        divT = 2*pow(10,3-timeList->currentItem()/3);
    else if(timeList->currentItem()%3 == 2)
        divT = pow(10,3-timeList->currentItem()/3);
    else
        divT = 5*pow(10,3-timeList->currentItem()/3);
    panel->setDivT(divT);

    panel->adjustDataSize();

    Scope::trig_t trigDirection = static_cast<Scope::trig_t>(trigGroup->id(trigGroup->selected()));
    double trigThreshold = trigThreshEdit->text().toDouble()*pow(10,-3*trigThreshList->currentItem());

    std::list<Scope::Channel>::iterator trigChannel = panel->getChannelsEnd();
    for(std::list<Scope::Channel>::iterator i = panel->getChannelsBegin(), end = panel->getChannelsEnd();i != end;++i)
        if(i->getLabel() == trigChanList->currentText()) {
            trigChannel = i;
            break;
        }
    if(trigChannel == panel->getChannelsEnd())
        trigDirection = Scope::NONE;

    bool trigHolding = trigHoldingCheck->isChecked();
    double trigHoldoff = trigHoldoffEdit->text().toDouble()*pow(10,-3*trigHoldoffList->currentItem());

    panel->setTrigger(trigDirection,trigThreshold,trigChannel,trigHolding,trigHoldoff);

    showDisplayTab();
}

void Oscilloscope::Properties::createAdvancedTab(void) {
    QWidget *advancedTab = new QWidget(tabWidget);
    tabWidget->addTab(advancedTab,"Advanced");

    QBoxLayout *layout = new QVBoxLayout(advancedTab);

    QGroupBox *resBox = new QGroupBox("Data Properties",advancedTab);
    layout->addWidget(resBox);

    QBoxLayout *resLayout = new QVBoxLayout(resBox);
    resLayout->setMargin(15);

    QHBox *hbox0 = new QHBox(resBox);
    resLayout->addWidget(hbox0);
    (new QLabel("Down Sampling Rate: ",hbox0))->setFixedWidth(130);
    rateSpin = new QSpinBox(hbox0);
    rateSpin->setMinValue(1);
    rateSpin->setMaxValue(100);
    //rateSpin->setValue(panel->rate);
    rateSpin->setEnabled(false);

    QHBox *hbox1 = new QHBox(resBox);
    resLayout->addWidget(hbox1);
    (new QLabel("Data Buffer Size: ",hbox1))->setFixedWidth(130);
    sizeSpin = new QSpinBox(hbox1);
    sizeSpin->setMinValue(500);
    sizeSpin->setMaxValue(50000);
    //sizeSpin->setValue(panel->getDataSize());
    sizeSpin->setEnabled(false);

    QGroupBox *gridBox = new QGroupBox("Grid Properties",advancedTab);
    layout->addWidget(gridBox);

    QBoxLayout *gridLayout = new QVBoxLayout(gridBox);
    gridLayout->setMargin(15);

    QHBox *hbox2 = new QHBox(gridBox);
    gridLayout->addWidget(hbox2);
    (new QLabel("  X Divisions: ",hbox2))->setFixedWidth(125);
    divXSpin = new QSpinBox(hbox2);
    divXSpin->setMinValue(1);
    divXSpin->setMaxValue(25);

    QHBox *hbox3 = new QHBox(gridBox);
    gridLayout->addWidget(hbox3);
    (new QLabel("  Y Divisions: ",hbox3))->setFixedWidth(125);
    divYSpin = new QSpinBox(hbox3);
    divYSpin->setMinValue(1);
    divYSpin->setMaxValue(25);
}

struct block_list_info_t {
    QComboBox *blockList;
    std::vector<IO::Block *> *blocks;
};

static void buildBlockList(IO::Block *block,void *arg) {
    block_list_info_t *info = static_cast<block_list_info_t *>(arg);
    info->blockList->insertItem(block->getName()+QString(" ")+QString::number(block->getID()));
    info->blocks->push_back(block);
}

void Oscilloscope::Properties::createChannelTab(void) {
    QWidget *channelTab = new QWidget(tabWidget);
    tabWidget->addTab(channelTab,"Channel");

    QBoxLayout *layout = new QVBoxLayout(channelTab);

    QHBox *hbox0 = new QHBox(channelTab);
    layout->addWidget(hbox0);
    (new QLabel("Channel:",hbox0))->setFixedWidth(60);
    blockList = new QComboBox(hbox0);
    block_list_info_t info = { blockList, &panel->blocks };
    IO::Connector::getInstance()->foreachBlock(::buildBlockList,&info);
    QObject::connect(blockList,SIGNAL(activated(int)),this,SLOT(buildChannelList(void)));
    typeList = new QComboBox(hbox0);
    typeList->insertItem("Input");
    typeList->insertItem("Output");
    typeList->insertItem("Parameter");
    typeList->insertItem("State");
    QObject::connect(typeList,SIGNAL(activated(int)),this,SLOT(buildChannelList(void)));
    channelList = new QComboBox(hbox0);
    QObject::connect(channelList,SIGNAL(activated(int)),this,SLOT(showTab(void)));
    activateButton = new QPushButton("Active",hbox0);
    activateButton->setToggleButton(true);
    activateButton->setFixedWidth(55);
    QObject::connect(activateButton,SIGNAL(toggled(bool)),this,SLOT(activateChannel(bool)));

    displayBox = new QGroupBox("Display Properties",channelTab);
    layout->addWidget(displayBox);

    QBoxLayout *displayLayout = new QVBoxLayout(displayBox);
    displayLayout->setMargin(15);

    QHBox *hbox1 = new QHBox(displayBox);
    displayLayout->addWidget(hbox1);
    (new QLabel("   Scale: ",hbox1))->setFixedWidth(125);
    scaleList = new QComboBox(hbox1);
    scaleList->insertItem(" 10  V/div");
    scaleList->insertItem("  5  V/div");
    scaleList->insertItem("  2  V/div");
    scaleList->insertItem("  1  V/div");
    scaleList->insertItem("500 mV/div");
    scaleList->insertItem("200 mV/div");
    scaleList->insertItem("100 mV/div");
    scaleList->insertItem(" 50 mV/div");
    scaleList->insertItem(" 20 mV/div");
    scaleList->insertItem(" 10 mV/div");
    scaleList->insertItem("  5 mV/div");
    scaleList->insertItem("  2 mV/div");
    scaleList->insertItem("  1 mV/div");
    scaleList->insertItem("500 uV/div");
    scaleList->insertItem("200 uV/div");
    scaleList->insertItem("100 uV/div");
    scaleList->insertItem(" 50 uV/div");
    scaleList->insertItem(" 20 uV/div");
    scaleList->insertItem(" 10 uV/div");
    scaleList->insertItem("  5 uV/div");
    scaleList->insertItem("  2 uV/div");
    scaleList->insertItem("  1 uV/div");
    scaleList->insertItem("500 nV/div");
    scaleList->insertItem("200 nV/div");
    scaleList->insertItem("100 nV/div");
    scaleList->insertItem(" 50 nV/div");
    scaleList->insertItem(" 20 nV/div");
    scaleList->insertItem(" 10 nV/div");
    scaleList->insertItem("  5 nV/div");
    scaleList->insertItem("  2 nV/div");
    scaleList->insertItem("  1 nV/div");
    scaleList->insertItem("500 pV/div");
    scaleList->insertItem("200 pV/div");
    scaleList->insertItem("100 pV/div");
    scaleList->insertItem(" 50 pV/div");
    scaleList->insertItem(" 20 pV/div");
    scaleList->insertItem(" 10 pV/div");
    scaleList->insertItem("  5 pV/div");
    scaleList->insertItem("  2 pV/div");
    scaleList->insertItem("  1 pV/div");

    QHBox *hbox2 = new QHBox(displayBox);
    displayLayout->addWidget(hbox2);
    (new QLabel("   Offset: ",hbox2))->setFixedWidth(125);
    offsetEdit = new QLineEdit(hbox2);
    offsetEdit->setValidator(new QDoubleValidator(offsetEdit));
    offsetList = new QComboBox(hbox2);
    offsetList->insertItem("V");
    offsetList->insertItem("mV");
    offsetList->insertItem("uV");
    offsetList->insertItem("nV");
    offsetList->insertItem("pV");

    lineBox = new QGroupBox("Line Properties",channelTab);
    layout->addWidget(lineBox);

    QBoxLayout *lineLayout = new QVBoxLayout(lineBox);
    lineLayout->setMargin(15);

    QHBox *hbox3 = new QHBox(lineBox);
    lineLayout->addWidget(hbox3);
    (new QLabel("   Color: ",hbox3))->setFixedWidth(125);
    colorList = new QComboBox(hbox3);
    QPixmap tmp(25,25);
    tmp.fill(Qt::red);
    colorList->insertItem(tmp," Red");
    tmp.fill(Qt::yellow);
    colorList->insertItem(tmp," Yellow");
    tmp.fill(Qt::green);
    colorList->insertItem(tmp," Green");
    tmp.fill(Qt::blue);
    colorList->insertItem(tmp," Blue");
    tmp.fill(Qt::magenta);
    colorList->insertItem(tmp," Magenta");
    tmp.fill(Qt::cyan);
    colorList->insertItem(tmp," Cyan");
    tmp.fill(Qt::black);
    colorList->insertItem(tmp," Black");

    QHBox *hbox4 = new QHBox(lineBox);
    lineLayout->addWidget(hbox4);
    (new QLabel("   Width: ",hbox4))->setFixedWidth(125);
    widthList = new QComboBox(hbox4);
    tmp.fill(Qt::white);
    QPainter painter(&tmp);
    for(int i=1;i<6;i++) {
        painter.setPen(QPen(Qt::black,i));
        painter.drawLine(0,12,25,12);
        widthList->insertItem(tmp,QString::number(i)+QString(" Pixels"));
    }

    QHBox *hbox5 = new QHBox(lineBox);
    lineLayout->addWidget(hbox5);
    (new QLabel("   Style: ",hbox5))->setFixedWidth(125);
    styleList = new QComboBox(hbox5);
    tmp.fill(Qt::white);
    painter.setPen(QPen(Qt::black,3,Qt::SolidLine));
    painter.drawLine(0,12,25,12);
    styleList->insertItem(tmp,QString(" Solid"));
    tmp.fill(Qt::white);
    painter.setPen(QPen(Qt::black,3,Qt::DashLine));
    painter.drawLine(0,12,25,12);
    styleList->insertItem(tmp,QString(" Dash"));
    tmp.fill(Qt::white);
    painter.setPen(QPen(Qt::black,3,Qt::DotLine));
    painter.drawLine(0,12,25,12);
    styleList->insertItem(tmp,QString(" Dot"));
    tmp.fill(Qt::white);
    painter.setPen(QPen(Qt::black,3,Qt::DashDotLine));
    painter.drawLine(0,12,25,12);
    styleList->insertItem(tmp,QString(" Dash Dot"));
    tmp.fill(Qt::white);
    painter.setPen(QPen(Qt::black,3,Qt::DashDotDotLine));
    painter.drawLine(0,12,25,12);
    styleList->insertItem(tmp,QString(" Dash Dot Dot"));

    buildChannelList();
}

void Oscilloscope::Properties::createDisplayTab(void) {

    QWidget *displayTab = new QWidget(tabWidget);
    tabWidget->addTab(displayTab,"Display");

    QBoxLayout *layout = new QVBoxLayout(displayTab);

    QGroupBox *timeBox = new QGroupBox("Time Properties",displayTab);
    layout->addWidget(timeBox);

    QBoxLayout *timeLayout = new QVBoxLayout(timeBox);
    timeLayout->setMargin(15);

    QHBox *hbox0 = new QHBox(timeBox);
    timeLayout->addWidget(hbox0);
    (new QLabel("Time Scale:",hbox0))->setFixedWidth(125);
    timeList = new QComboBox(hbox0);
    timeList->insertItem("  5  s/div");
    timeList->insertItem("  2  s/div");
    timeList->insertItem("  1  s/div");
    timeList->insertItem("500 ms/div");
    timeList->insertItem("200 ms/div");
    timeList->insertItem("100 ms/div");
    timeList->insertItem(" 50 ms/div");
    timeList->insertItem(" 20 ms/div");
    timeList->insertItem(" 10 ms/div");
    timeList->insertItem("  5 ms/div");
    timeList->insertItem("  2 ms/div");
    timeList->insertItem("  1 ms/div");
    timeList->insertItem("500 us/div");
    timeList->insertItem("200 us/div");
    timeList->insertItem("100 us/div");
    timeList->insertItem(" 50 us/div");
    timeList->insertItem(" 20 us/div");
    timeList->insertItem(" 10 us/div");
    timeList->insertItem("  5 us/div");
    timeList->insertItem("  2 us/div");
    timeList->insertItem("  1 us/div");

    QHBox *hbox1 = new QHBox(timeBox);
    timeLayout->addWidget(hbox1);
    (new QLabel("Screen Refresh:",hbox1))->setFixedWidth(125);
    refreshSpin = new QSpinBox(hbox1);
    refreshSpin->setMinValue(10);
    refreshSpin->setMaxValue(10000);

    QGroupBox *triggerBox = new QGroupBox("Trigger Properties",displayTab);
    layout->addWidget(triggerBox);

    QBoxLayout *triggerLayout = new QVBoxLayout(triggerBox);
    triggerLayout->setMargin(15);

    QHBox *hbox2 = new QHBox(triggerBox);
    triggerLayout->addWidget(hbox2);
    (new QLabel("Trigger:",hbox2))->setFixedWidth(125);
    trigGroup = new QHButtonGroup(hbox2);
    trigGroup->setRadioButtonExclusive(true);
    trigGroup->setLineWidth(0);
    trigGroup->insert(new QRadioButton("Off",trigGroup),Scope::NONE);
    trigGroup->insert(new QRadioButton("+",trigGroup),Scope::POS);
    trigGroup->insert(new QRadioButton("-",trigGroup),Scope::NEG);

    QHBox *hbox3 = new QHBox(triggerBox);
    triggerLayout->addWidget(hbox3);
    (new QLabel("Trigger Channel:",hbox3))->setFixedWidth(125);
    trigChanList = new QComboBox(hbox3);

    QHBox *hbox4 = new QHBox(triggerBox);
    triggerLayout->addWidget(hbox4);
    (new QLabel("Trigger Threshold:",hbox4))->setFixedWidth(125);
    trigThreshEdit = new QLineEdit(hbox4);
    trigThreshEdit->setValidator(new QDoubleValidator(trigThreshEdit));
    trigThreshList = new QComboBox(hbox4);
    trigThreshList->insertItem("V");
    trigThreshList->insertItem("mV");
    trigThreshList->insertItem("uV");
    trigThreshList->insertItem("nV");
    trigThreshList->insertItem("pV");

    QHBox *hbox5 = new QHBox(triggerBox);
    triggerLayout->addWidget(hbox5);
    (new QLabel("Trigger Holding:",hbox5))->setFixedWidth(125);
    trigHoldingCheck = new QCheckBox(hbox5);

    QHBox *hbox6 = new QHBox(triggerBox);
    triggerLayout->addWidget(hbox6);
    (new QLabel("Trigger Holdoff:",hbox6))->setFixedWidth(125);
    trigHoldoffEdit = new QLineEdit(hbox6);
    trigHoldoffEdit->setValidator(new QDoubleValidator(trigHoldoffEdit));
    trigHoldoffList = new QComboBox(hbox6);
    trigHoldoffList->insertItem("ms");
    trigHoldoffList->insertItem("us");
    trigHoldoffList->insertItem("ns");
}

void Oscilloscope::Properties::showAdvancedTab(void) {
    //rateSpin->setValue(panel->rate);
    //sizeSpin->setValue(panel->getDataSize());

    divXSpin->setValue(panel->getDivX());
    divYSpin->setValue(panel->getDivY());
}

void Oscilloscope::Properties::showChannelTab(void) {

    IO::flags_t type;
    switch(typeList->currentItem()) {
      case 0:
          type = Workspace::INPUT;
          break;
      case 1:
          type = Workspace::OUTPUT;
          break;
      case 2:
          type = Workspace::PARAMETER;
          break;
      case 3:
          type = Workspace::STATE;
          break;
      default:
          ERROR_MSG("Oscilloscope::Properties::showChannelTab : invalid type\n");
          typeList->setCurrentItem(0);
          type = Workspace::INPUT;
    }

    bool found = false;

    for(std::list<Scope::Channel>::iterator i = panel->getChannelsBegin(),end = panel->getChannelsEnd();i != end;++i) {
        struct channel_info *info = reinterpret_cast<struct channel_info *>(i->getInfo());
        if(!info) continue;
        if(info->block && info->block == panel->blocks[blockList->currentItem()] &&
           info->type == type && info->index == static_cast<size_t>(channelList->currentItem())) {
            found = true;

            scaleList->setCurrentItem(static_cast<int>(round(3*(log10(1/i->getScale())+1))));

            double offset = i->getOffset();
            int offsetUnits = 0;
            if(offset)
                while(fabs(offset) < 1) {
                    offset *= 1000;
                    offsetUnits++;
                }
            offsetEdit->setText(QString::number(offset));
            offsetList->setCurrentItem(offsetUnits);

            if(i->getPen().color() == Qt::red)
                colorList->setCurrentItem(0);
            else if(i->getPen().color() == Qt::yellow)
                colorList->setCurrentItem(1);
            else if(i->getPen().color() == Qt::green)
                colorList->setCurrentItem(2);
            else if(i->getPen().color() == Qt::blue)
                colorList->setCurrentItem(3);
            else if(i->getPen().color() == Qt::magenta)
                colorList->setCurrentItem(4);
            else if(i->getPen().color() == Qt::cyan)
                colorList->setCurrentItem(5);
            else if(i->getPen().color() == Qt::black)
                colorList->setCurrentItem(6);
            else {
                ERROR_MSG("Oscilloscope::Properties::displayChannelTab : invalid color selection\n");
                colorList->setCurrentItem(0);
            }

            switch(i->getPen().style()) {
              case Qt::SolidLine:
                  styleList->setCurrentItem(0);
                  break;
              case Qt::DashLine:
                  styleList->setCurrentItem(1);
                  break;
              case Qt::DotLine:
                  styleList->setCurrentItem(2);
                  break;
              case Qt::DashDotLine:
                  styleList->setCurrentItem(3);
                  break;
              case Qt::DashDotDotLine:
                  styleList->setCurrentItem(4);
                  break;
              default:
                  ERROR_MSG("Oscilloscope::Properties::displayChannelTab : invalid style selection\n");
                  styleList->setCurrentItem(0);
            }

            break;
        }
    }

    activateButton->setOn(found);
    displayBox->setEnabled(found);
    lineBox->setEnabled(found);
    if(!found) {
        scaleList->setCurrentItem(2);
        offsetEdit->setText(QString::number(0));
        offsetList->setCurrentItem(0);
        colorList->setCurrentItem(0);
        widthList->setCurrentItem(0);
        styleList->setCurrentItem(0);
    }
}

void Oscilloscope::Properties::showDisplayTab(void) {
    timeList->setCurrentItem(static_cast<int>(round(3*log10(1/panel->getDivT())+11)));
    refreshSpin->setValue(panel->getRefresh());

    static_cast<QRadioButton *>(trigGroup->find(static_cast<int>(panel->getTriggerDirection())))->setChecked(true);

    QString name;
    trigChanList->clear();
    for(std::list<Scope::Channel>::iterator i = panel->getChannelsBegin(), end = panel->getChannelsEnd();i != end;++i) {
        trigChanList->insertItem(i->getLabel());
        if(i == panel->getTriggerChannel())
            trigChanList->setCurrentItem(trigChanList->count()-1);
    }
    trigChanList->insertItem("<None>");
    if(panel->getTriggerChannel() == panel->getChannelsEnd())
        trigChanList->setCurrentItem(trigChanList->count()-1);

    int trigThreshUnits = 0;
    double trigThresh = panel->getTriggerThreshold();
    if(trigThresh != 0.0)
        while(fabs(trigThresh) < 1) {
            trigThresh *= 1000;
            ++trigThreshUnits;
        }
    trigThreshList->setCurrentItem(trigThreshUnits);
    trigThreshEdit->setText(QString::number(trigThresh));
    trigHoldingCheck->setChecked(panel->getTriggerHolding());
    int trigHoldoffUnits = 0;
    double trigHoldoff = panel->getTriggerHoldoff();
    if(trigHoldoff != 0.0)
        while(fabs(trigHoldoff) < 1) {
            trigHoldoff *= 1000;
            ++trigHoldoffUnits;
        }
    trigHoldoffList->setCurrentItem(trigHoldoffUnits);
    trigHoldoffEdit->setText(QString::number(trigHoldoff));
}
