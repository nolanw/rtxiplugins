/*
 * Copyright (C) 2004 Boston University
 * Copyright (C) 2010 Nolan Waite
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License as
 *  published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <Istep.h>
#include <main_window.h>
#include <qlayout.h>
#include <qtooltip.h>
#include <qvalidator.h>
#include <qscrollview.h>
#include <qlabel.h>
#include <qtimer.h>
#include <qpushbutton.h>
#include <qhbox.h>
#include <qvbox.h>
#include <qapplication.h>
#include <vector>
#include <qevent.h>

// Plot a new point every PLOT_PERIOD Realtime periods.
#define PLOT_PERIOD 100

// I think this is here to synchronize the parameters between the GUI thread, 
// which might change them as a result of the user's changes, and the realtime 
// thread, which might be executing an iteration of the plugin.
namespace
{
  class SyncEvent: public RT::Event
  {
  public:
	  int callback(void)
	  {
		  return 0;
	  }
  }; // class SyncEvent
} // namespace


// You don't want to do anything to the GUI from any thread except Qt's.
// We'd normally use signals here, but (despite others' attempts) no dice.
// Qt3 signals don't work cross-thread, so instead you have to use custom 
// events. Here is a custom event class for plotting. When you want to change 
// something with the plots, make an instance of this class and give it the 
// changes you need, then have Qt hand it off to its main thread.
// See the plot and customEvent methods below for how this is used.
enum
{
  IHateThreadsEventSetAxes = 65445,
  IHateThreadsEventAppendPoint = 65446,
  IHateThreadsEventNewCurve = 65447,
  IHateThreadsEventClear = 65448,
};

class IHateThreadsEvent : public QCustomEvent
{
public:
  IHateThreadsEvent(double xmin, double xmax, double ymin, double ymax) : 
    QCustomEvent(IHateThreadsEventSetAxes)
  {
    points[0] = xmin, points[1] = xmax;
    points[2] = ymin, points[3] = ymax;
  }
  
  IHateThreadsEvent(double x, double y) : 
    QCustomEvent(IHateThreadsEventAppendPoint)
  {
    points[0] = x, points[1] = y;
  }
  
  IHateThreadsEvent(bool newCurveOrClear) : 
    QCustomEvent(newCurveOrClear ? IHateThreadsEventNewCurve : IHateThreadsEventClear)
  {}
  
  double points[4];
};
     
// RTXI calls this function to get an instance of this plugin. No
// initialization is done here; it's all in the constructor or update method.
extern "C" Plugin::Object *createRTXIPlugin(void)
{
  return new Istep();
}

// Exactly like in a plugin that uses the default GUI model, we define all of 
// our inputs, outputs, and parameters here.
// Note that there are two outputs. One is the current being sent out. The 
// other is the voltage that you send to the amplifier so that its output 
// matches this plugin's current output.
static DefaultGUIModel::variable_t vars[] =
{
  {
    "Vin",
    "",
    DefaultGUIModel::INPUT,
  },
  {
    "Vout",
    "Voltage to send to amplifier",
    DefaultGUIModel::OUTPUT,
  },
  {
    "Iout",
    "Current being sent to amplifier",
    DefaultGUIModel::OUTPUT,
  },
  {
    "Period (ms)",
    "Duration from minimum to maximum current (one cycle)",
    DefaultGUIModel::PARAMETER | DefaultGUIModel::DOUBLE,
  },
  {
    "Delay (ms)",
    "Time from beginning of cycle to first step",
    DefaultGUIModel::PARAMETER | DefaultGUIModel::DOUBLE,
  },
  {
    "Min Amp (pA)",
    "Current on first step of each cycle (linked with Step Size (pA))",
    DefaultGUIModel::PARAMETER | DefaultGUIModel::DOUBLE,
  },
  {
    "Max Amp (pA)",
    "Current on last step of each cycle (linked with Step Size (pA))",
    DefaultGUIModel::PARAMETER | DefaultGUIModel::DOUBLE,
  },
  {
    "Steps (#)",
    "How many steps to take between min and max current (linked with Step Size (pA))",
    DefaultGUIModel::PARAMETER | DefaultGUIModel::UINTEGER,
  },
  {
    "Step Size (pA)",
    "How much the current should increase on each step (linked with Steps (#))",
    DefaultGUIModel::PARAMETER | DefaultGUIModel::DOUBLE,
  },
  {
    "Cycles (#)",
    "How many times to repeat the protocol",
    DefaultGUIModel::PARAMETER | DefaultGUIModel::UINTEGER,
  },
  {
    "Pulse Duration (%)",
    "Proportion of period to apply current",
    DefaultGUIModel::PARAMETER | DefaultGUIModel::DOUBLE,
  },
  {
    "Offset (pA)",
    "DC offset to add",
    DefaultGUIModel::PARAMETER | DefaultGUIModel::DOUBLE,
  },
  {
    "Factor (pA/V)",
    "Amplifier signal factor",
    DefaultGUIModel::PARAMETER | DefaultGUIModel::DOUBLE,
  },
};

static size_t num_vars = sizeof(vars) / sizeof(DefaultGUIModel::variable_t);

// The constructor sets up the GUI, laying out the parameters on the left and 
// the two plots on the right. There's nothing special here, it's just long.
// The code for the labels and text boxes for the parameters (on the left) is 
// boilerplate ripped directly from default_gui_model, and should really be 
// refactored out of here.
Istep::Istep(void) : 
  QWidget(MainWindow::getInstance()->centralWidget()), 
  Workspace::Instance("Istep", ::vars, ::num_vars), 
  period(1.0), 
  delay(0.0), 
  Amin(-100.0), 
  Amax(100.0), 
  Nsteps(20), 
  Ncycles(1), 
  duty(50), 
  offset(0.0),
  factor(200.0),
  periodsSincePlot(0)
{
  setCaption(QString::number(getID()) + " Istep");
  
  QBoxLayout *layout = new QHBoxLayout(this); // overall GUI layout
  
  QBoxLayout *leftLayout = new QVBoxLayout();
  
  QHBox *utilityBox = new QHBox(this);
	pauseButton = new QPushButton("Pause", utilityBox);
	pauseButton->setToggleButton(true);
	QPushButton *modifyButton = new QPushButton("Modify", utilityBox);
	QPushButton *unloadButton = new QPushButton("Unload", utilityBox);
	QObject::connect(pauseButton, SIGNAL(toggled(bool)), this, SLOT(pause(bool)));
	QObject::connect(modifyButton, SIGNAL(clicked(void)), this, SLOT(modify(void)));
	QObject::connect(unloadButton, SIGNAL(clicked(void)), this, SLOT(exit(void)));
	QObject::connect(pauseButton, SIGNAL(toggled(bool)), modifyButton, SLOT(setEnabled(bool)));
	QToolTip::add(pauseButton, "Start/Stop Istep protocol");
	QToolTip::add(modifyButton, "Commit changes to parameter values and reset");
	QToolTip::add(unloadButton, "Close plug-in");
    
  // create default_gui_model GUI DO NOT EDIT
	QScrollView *sv = new QScrollView(this);
	sv->setResizePolicy(QScrollView::AutoOneFit);
	sv->setHScrollBarMode(QScrollView::AlwaysOff);

	QWidget *viewport = new QWidget(sv->viewport());
	sv->addChild(viewport);
	QGridLayout *scrollLayout = new QGridLayout(viewport, 1, 2);

	size_t nstate = 0, nparam = 0, nevent = 0, ncomment = 0;
	for (size_t i = 0; i < num_vars; i++)
	{
		if (vars[i].flags & (PARAMETER | STATE | EVENT | COMMENT))
		{
			param_t param = {0};

			param.label = new QLabel(vars[i].name, viewport);
			scrollLayout->addWidget(param.label, parameter.size(), 0);
			param.edit = new DefaultGUILineEdit(viewport);
			scrollLayout->addWidget(param.edit, parameter.size(), 1);

			QToolTip::add(param.label, vars[i].description);
			QToolTip::add(param.edit, vars[i].description);

			if (vars[i].flags & PARAMETER)
			{
				if (vars[i].flags & DOUBLE)
				{
					param.edit->setValidator(new QDoubleValidator(param.edit));
					param.type = PARAMETER | DOUBLE;
				}
				else if (vars[i].flags & UINTEGER)
				{
					QIntValidator *validator = new QIntValidator(param.edit);
					param.edit->setValidator(validator);
					validator->setBottom(0);
					param.type = PARAMETER | UINTEGER;
				}
				else if (vars[i].flags & INTEGER)
				{
					param.edit->setValidator(new QIntValidator(param.edit));
					param.type = PARAMETER | INTEGER;
				}
				else
					param.type = PARAMETER;
				param.index = nparam++;
				param.str_value = new QString;
			}
			else if (vars[i].flags & STATE)
			{
				param.edit->setReadOnly(true);
				param.type = STATE;
				param.index = nstate++;
			}
			else if (vars[i].flags & EVENT)
			{
				param.edit->setReadOnly(true);
				param.type = EVENT;
				param.index = nevent++;
			}
			else if (vars[i].flags & COMMENT)
			{
				param.type = COMMENT;
				param.index = ncomment++;
			}

			parameter[vars[i].name] = param;
		}
	}
	// end default_gui_model GUI DO NOT EDIT
	leftLayout->addWidget(sv);
	leftLayout->addWidget(utilityBox);
	layout->addLayout(leftLayout);
	
	QBoxLayout *rightLayout = new QVBoxLayout();
	rightLayout->setSpacing(15);
	rightLayout->setMargin(10);
	
	vplot = new IncrementalPlot(this);
	vplot->setMinimumSize(400, 200);
	rightLayout->addWidget(vplot, 2);
	iplot = new IncrementalPlot(this);
	iplot->setMinimumSize(400, 100);
	rightLayout->addWidget(iplot, 1);
	
	layout->addLayout(rightLayout);
	layout->setResizeMode(QLayout::Minimum);
	layout->setStretchFactor(leftLayout, 0);
	layout->setStretchFactor(rightLayout, 1);
	
	QRect thisRect = this->geometry();
	thisRect.setHeight(350);
	this->setGeometry(thisRect);
	
	// set GUI refresh rate
	QTimer *timer = new QTimer(this);
	timer->start(1000);
	QObject::connect(timer, SIGNAL(timeout(void)), this, SLOT(refresh(void)));
	show();
    
  stepSize = (Amax - Amin) / Nsteps;
  update(PERIOD);
  update(INIT);
  refresh();
}

Istep::~Istep(void) {}

// The plugin consists of two loops. The outer loop counts cycles, or
// iterations of the inner loop. The inner loop increases the current by one 
// step each time through.
// Do all time keeping in seconds. When showing time to user, though, 
// convert to milliseconds.
void Istep::execute(void)
{
  V = input(0);

  Iout = offset;
  if (cycle < Ncycles)
  {
    if (step < Nsteps)
    {
      if (timeSinceStep >= delay && 
          timeSinceStep - delay + EPS < period * (duty / 100))
      {
        Iout += Amin + (step * deltaI);
      }
          
      if (periodsSincePlot >= PLOT_PERIOD || timeSinceStep <= EPS)
          plot(timeSinceStep * 1000.0, V / 1000, Iout);
      
      periodsSincePlot++;
      timeSinceStep += dt / 1000;
      
      if (timeSinceStep + EPS >= period)
      {
        startNewCurve();
        step++;
        timeSinceStep = periodsSincePlot = 0;
      }
    }
    
    if (step == Nsteps)
    {
      cycle++;
      step = 0;
      if (cycle < Ncycles)
        clearPlot();
    }
  }
  output(0) = Iout / factor;
  output(1) = Iout;
}

// Anytime the user hits "modify" or changes the realtime period, this method 
// handles the changes. It does a bit of error detection (e.g. a negative 
// period). It links the increments parameter with the step size 
// parameter (and vice versa) and the min/max current with the step size 
// parameter.
void Istep::update(update_flags_t flag) 
{
  double oldStepSize = stepSize;
  int oldNsteps = Nsteps;
  switch(flag) {
  case INIT:
    setParameter("Period (ms)", period * 1000.0);
    setParameter("Delay (ms)", delay * 1000.0);
    setParameter("Min Amp (pA)", Amin);
    setParameter("Max Amp (pA)", Amax);
    setParameter("Steps (#)", Nsteps);
    setParameter("Step Size (pA)", stepSize);
    setParameter("Cycles (#)", Ncycles);
    setParameter("Pulse Duration (%)", duty);
    setParameter("Offset (pA)", offset);
    setParameter("Factor (pA/V)", factor);
    
    vplot->setAxisTitle(0, "V (mV)");
    
    iplot->setAxisTitle(0, "I (pA)");
    
    break;
  case MODIFY:
    period = getParameter("Period (ms)").toDouble() / 1000.0;
    delay  = getParameter("Delay (ms)").toDouble() / 1000.0;
    Amin   = getParameter("Min Amp (pA)").toDouble();
    Amax   = getParameter("Max Amp (pA)").toDouble();
    Nsteps = getParameter("Steps (#)").toInt();
    stepSize = getParameter("Step Size (pA)").toDouble();
    // If stepSize and Nsteps are both changed at once, just use Nsteps.
    if (oldNsteps != Nsteps)
    {
      stepSize = (Amax - Amin) / Nsteps;
      setParameter("Step Size (pA)", stepSize);
    }
    else if (oldStepSize != stepSize)
    {
      Nsteps = (Amax - Amin) / stepSize;
      setParameter("Steps (#)", Nsteps);
    }
    else
    {
      stepSize = (Amax - Amin) / Nsteps;
      setParameter("Step Size (pA)", stepSize);
    }
    Ncycles = getParameter("Cycles (#)").toInt();
    duty = getParameter("Pulse Duration (%)").toDouble();
    offset = getParameter("Offset (pA)").toDouble();
    factor = getParameter("Factor (pA/V)").toDouble();
    break;
  case PAUSE:
    output(0) = 0;
    output(1) = 0;
    break;
  case PERIOD:
    dt = RT::System::getInstance()->getPeriod() * 1e-6;
    break;
  default:
    break;
  }
  
  if (!(flag == INIT || flag == MODIFY))
    return;

  // Some Error Checking for fun

  if (period <= 0)
  {
    period = 1;
    setParameter("Period (ms)", period * 1000.0);
  }

  if (Amin > Amax)
  {
    Amax = Amin;
    setParameter("Min Amp (pA)", Amin);
    setParameter("Max Amp (pA)", Amax);
  }

  if (Ncycles < 1)
  {
    Ncycles = 1;
    setParameter("Cycles (#)", Ncycles);
  }
  
  if (Nsteps < 0)
  {
      Nsteps = 0;
      setParameter("Steps (#)", Nsteps);
  }
  
  if (duty < 0 || duty > 100)
  {
    duty = 0;
    setParameter("Pulse Duration (%)",duty);
  }
  
  if (delay <= 0 || delay > period * duty / 100)
  {
    delay = 0;
    setParameter("Delay (ms)", delay * 1000.0);
  }
  
  // Set up plot
  iplot->removeData();
  iplot->setAxes(0, period * 1000.0, offset + Amin, offset + Amax);

  vplot->removeData();
  vplot->setAxes(0, period * 1000.0, -100, 100);

  // Define deltaI based on params
  deltaI = (Nsteps > 1) ? (Amax - Amin) / (Nsteps - 1) : 0;
  
  // Initialize counters
  timeSinceStep = 0;
  periodsSincePlot = 0;
  step = 0;
  cycle = 0;
}

// Plots two points, one on each plot.
void Istep::plot(double t, double V, double I)
{
  newVDataPoint(t, V);
  newIDataPoint(t, I);
  periodsSincePlot = 0;
}

// The following are wrappers around creating and sending that custom event 
// class defined up top. This methods are called in the realtime thread.
void Istep::setVPlotRange(double xmin, double xmax, double ymin, double ymax)
{
  IHateThreadsEvent *pe = new IHateThreadsEvent(xmin, xmax, ymin, ymax);
  pe->setData((void *)'V');
  QApplication::postEvent(this, pe);
}

void Istep::newVDataPoint(double newx, double newy)
{
  IHateThreadsEvent *pe = new IHateThreadsEvent(newx, newy);
  pe->setData((void *)'V');
  QApplication::postEvent(this, pe);
}

void Istep::setIPlotRange(double xmin, double xmax, double ymin, double ymax)
{
  IHateThreadsEvent *pe = new IHateThreadsEvent(xmin, xmax, ymin, ymax);
  pe->setData((void *)'I');
  QApplication::postEvent(this, pe);
}

void Istep::newIDataPoint(double newx, double newy)
{
  IHateThreadsEvent *pe = new IHateThreadsEvent(newx, newy);
  pe->setData((void *)'I');
  QApplication::postEvent(this, pe);
}

void Istep::startNewCurve(void)
{
  IHateThreadsEvent *pe1 = new IHateThreadsEvent(true);
  pe1->setData((void *)'V');
  QApplication::postEvent(this, pe1);
  IHateThreadsEvent *pe2 = new IHateThreadsEvent(true);
  pe2->setData((void *)'I');
  QApplication::postEvent(this, pe2);
}

void Istep::clearPlot(void)
{
  IHateThreadsEvent *pe1 = new IHateThreadsEvent(false);
  pe1->setData((void *)'V');
  QApplication::postEvent(this, pe1);
  IHateThreadsEvent *pe2 = new IHateThreadsEvent(false);
  pe2->setData((void *)'I');
  QApplication::postEvent(this, pe2);
}

// Plot events are handled here, on the main thread.
void Istep::customEvent(QCustomEvent *e)
{
  IHateThreadsEvent *pe = (IHateThreadsEvent *)e;
  IncrementalPlot *target = (long)e->data() == 'V' ? vplot : iplot;
  switch (e->type())
  {
  case IHateThreadsEventSetAxes:
  target->setAxes(pe->points[0], pe->points[1], pe->points[2], pe->points[3]);
  break;
  
  case IHateThreadsEventAppendPoint:
  target->appendLine(pe->points[0], pe->points[1]);
  break;
  
  case IHateThreadsEventNewCurve:
  target->startNewCurve();
  break;
  
  case IHateThreadsEventClear:
  target->removeData();
  break;
  
  default:
  break;
  }
}

// From here to the end it's entirely the same as DefaultGUIModel.
void Istep::exit(void)
{
	update(EXIT);
	Plugin::Manager::getInstance()->unload(this);
}

void Istep::refresh(void)
{
  std::map<QString, param_t>::iterator i;
	for (i = parameter.begin(); i != parameter.end(); ++i)
	{
		if (i->second.type & (STATE | EVENT))
			i->second.edit->setText(QString::number(getValue(i->second.type, i->second.index)));
		else if ((i->second.type & PARAMETER) && !i->second.edit->edited() && 
		         i->second.edit->text() != *i->second.str_value)
			i->second.edit->setText(*i->second.str_value);
		else if ((i->second.type & COMMENT) && !i->second.edit->edited() && 
		         i->second.edit->text() != getValueString(COMMENT, i->second.index))
			i->second.edit->setText(getValueString(COMMENT, i->second.index));
	}
	pauseButton->setOn(!getActive());
}

// Get the updated parameters, being careful to synchronize with the realtime 
// thread.
void Istep::modify(void)
{
	bool active = getActive();

	setActive(false);

	SyncEvent event;
	RT::System::getInstance()->postEvent(&event);
  
  std::map<QString, param_t>::iterator i;
	for (i = parameter.begin(); i != parameter.end(); ++i)
	{
		if (i->second.type & COMMENT)
			Workspace::Instance::setComment(i->second.index, i->second.edit->text().latin1());
	}

	update(MODIFY);
	setActive(active);

	for (i = parameter.begin(); i != parameter.end(); ++i)
		i->second.edit->blacken();
}

QString Istep::getParameter(const QString &name)
{
	std::map<QString, param_t>::iterator n = parameter.find(name);
	if ((n != parameter.end()) && (n->second.type & PARAMETER)) {
		*n->second.str_value = n->second.edit->text();
		setValue(n->second.index, n->second.edit->text().toDouble());
		return n->second.edit->text();
	}
	return "";
}

void Istep::setParameter(const QString &name, double value)
{
	std::map<QString, param_t>::iterator n = parameter.find(name);
	if ((n != parameter.end()) && (n->second.type & PARAMETER)) {
		n->second.edit->setText(QString::number(value));
		*n->second.str_value = n->second.edit->text();
		setValue(n->second.index, n->second.edit->text().toDouble());
	}
}

void Istep::setParameter(const QString &name, const QString value)
{
	std::map<QString, param_t>::iterator n = parameter.find(name);
	if ((n != parameter.end()) && (n->second.type & PARAMETER)) {
		n->second.edit->setText(value);
		*n->second.str_value = n->second.edit->text();
		setValue(n->second.index, n->second.edit->text().toDouble());
	}
}

void Istep::pause(bool p) {
	if (pauseButton->isOn() != p)
		pauseButton->setDown(p);

	setActive(!p);
	if (p)
		update(PAUSE);
	else
		update(UNPAUSE);
}

void Istep::doLoad(const Settings::Object::State &s) {
	for (std::map<QString, param_t>::iterator i = parameter.begin(); i != parameter.end(); ++i)
		i->second.edit->setText(s.loadString(i->first));
	pauseButton->setOn(s.loadInteger("paused"));
	modify();
}

void Istep::doSave(Settings::Object::State &s) const {
	s.saveInteger("paused", pauseButton->isOn());
	std::map<QString, param_t>::const_iterator i;
	for (i = parameter.begin(); i != parameter.end(); ++i)
		s.saveString(i->first, i->second.edit->text());
}

void Istep::receiveEvent(const Event::Object *event) {
	if (event->getName() == Event::RT_PREPERIOD_EVENT)
	{
		periodEventPaused = getActive();
		setActive(false);
	}
	else if (event->getName() == Event::RT_POSTPERIOD_EVENT)
	{
    #ifdef DEBUG
		if (getActive())
			ERROR_MSG("DefaultGUIModel::receiveEvent : model unpaused during a period update\n");
    #endif
    
		update(PERIOD);
		setActive(periodEventPaused);
	}
}

