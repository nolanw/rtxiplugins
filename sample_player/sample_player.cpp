/*
 * SamplePlayer
 * Read out a file of samples at some rate.
 */

#include "sample_player.h"

#include <main_window.h>

#include "worker.h"

#include <sys/time.h>
#include <sys/resource.h>

#include <qfile.h>
#include <qfiledialog.h>
#include <qgridview.h>
#include <qlabel.h>
#include <qlayout.h>
#include <qpushbutton.h>
#include <qtimer.h>
#include <qtooltip.h>
#include <qvalidator.h>
#include <qvbox.h>


/* DO NOT EDIT */
namespace
{
class SyncEvent: public RT::Event
{
public:
	int callback()
	{
		return 0;
	}
};

double get_time()
{
  struct timeval t;
  gettimeofday(&t, NULL);
  return t.tv_sec + t.tv_usec * 1e-6;
}

}
/* END DO NOT EDIT */

extern "C" Plugin::Object *createRTXIPlugin()
{
	return new SamplePlayer();
}

#define PARAM_SAMPLE_RATE "Sample rate (Hz)"

#define INITIAL_SAMPLE_RATE 50

static SamplePlayer::variable_t vars[] =
{
  {
    "Vout (sample)",
    "Sample file output",
    SamplePlayer::OUTPUT,
  },
  {
    PARAM_SAMPLE_RATE,
    "How often to output a new sample",
    SamplePlayer::PARAMETER | SamplePlayer::DOUBLE,
  },
};

static size_t num_vars = sizeof(vars) / sizeof(SamplePlayer::variable_t);

SamplePlayer::SamplePlayer(void) :
	QWidget(MainWindow::getInstance()->centralWidget()), 
	Workspace::Instance("SamplePlayer", vars, num_vars),
	worker(NULL), askedForMore(false)
{
	setCaption(QString::number(getID()) + " SamplePlayer");

	QBoxLayout *layout = new QVBoxLayout(this); // overall GUI layout
	
	QPushButton *openFileButton = new QPushButton("Choose Sample File", this);
	layout->addWidget(openFileButton);
	QObject::connect(openFileButton, SIGNAL(clicked()), 
	                 this, SLOT(setSampleFilename()));
	
	sampleFilename = new DefaultGUILineEdit(this);
	layout->addWidget(sampleFilename);
	
	// Create default_gui_model GUI DO NOT EDIT
	QGridLayout *scrollLayout = new QGridLayout(layout, 1, 2);

	size_t nstate = 0, nparam = 0, nevent = 0, ncomment = 0;
	for (size_t i = 0; i < num_vars; i++) {
		if (vars[i].flags & (PARAMETER | STATE | EVENT | COMMENT)) {
			param_t param = {NULL, NULL, 0, 0, NULL};

			param.label = new QLabel(vars[i].name, this);
			scrollLayout->addWidget(param.label, parameter.size(), 0);
			param.edit = new DefaultGUILineEdit(this);
			scrollLayout->addWidget(param.edit, parameter.size(), 1);

			QToolTip::add(param.label, vars[i].description);
			QToolTip::add(param.edit, vars[i].description);

			if (vars[i].flags & PARAMETER) {
				if (vars[i].flags & DOUBLE) {
					param.edit->setValidator(new QDoubleValidator(param.edit));
					param.type = PARAMETER | DOUBLE;
				} else if (vars[i].flags & UINTEGER) {
					QIntValidator *validator = new QIntValidator(param.edit);
					param.edit->setValidator(validator);
					validator->setBottom(0);
					param.type = PARAMETER | UINTEGER;
				} else if (vars[i].flags & INTEGER) {
					param.edit->setValidator(new QIntValidator(param.edit));
					param.type = PARAMETER | INTEGER;
				} else
					param.type = PARAMETER;
				param.index = nparam++;
				param.str_value = new QString;
			} else if (vars[i].flags & STATE) {
				param.edit->setReadOnly(true);
				param.type = STATE;
				param.index = nstate++;
			} else if (vars[i].flags & EVENT) {
				param.edit->setReadOnly(true);
				param.type = EVENT;
				param.index = nevent++;
			} else if (vars[i].flags & COMMENT) {
				param.type = COMMENT;
				param.index = ncomment++;
			}

			parameter[vars[i].name] = param;
		}
	}
	// end default_gui_model GUI DO NOT EDIT

	QHBox *utilityBox = new QHBox(this);
	
	pauseButton = new QPushButton("Pause", utilityBox);
	pauseButton->setToggleButton(true);
	QObject::connect(pauseButton, SIGNAL(toggled(bool)), 
	                 this, SLOT(pause(bool)));
	QToolTip::add(pauseButton, "Start/Stop playback");
	
	QPushButton *modifyButton = new QPushButton("Load", utilityBox);
	QObject::connect(modifyButton, SIGNAL(clicked(void)), 
	                 this, SLOT(modify(void)));
	QToolTip::add(modifyButton, "Load (or reload) samples from file");
	
	QPushButton *unloadButton = new QPushButton("Unload", utilityBox);
	QObject::connect(unloadButton, SIGNAL(clicked(void)), 
	                 this, SLOT(exit(void)));
	QObject::connect(pauseButton, SIGNAL(toggled(bool)), 
	                 modifyButton, SLOT(setEnabled(bool)));
	QToolTip::add(unloadButton, "Close plug-in");

	// add custom components to layout below default_gui_model components
	layout->addWidget(utilityBox);

	// set GUI refresh rate
	QTimer *timer = new QTimer(this);
	timer->start(1000);
	QObject::connect(timer, SIGNAL(timeout(void)), 
	                 this, SLOT(refresh(void)));
	show();

	update(INIT);
	update(PERIOD);
	refresh();
}

SamplePlayer::~SamplePlayer(void)
{
  if (worker)
  {
    worker->terminate();
    worker->wait();
    delete worker;
    worker = NULL;
  }
}

void SamplePlayer::execute(void)
{
  elapsedTime += dt_s;
  if (!askedForMore && worker != NULL && sampleQueue.size() < sampleRate * 2.0)
  {
    worker->fetchMoreSamples();
    askedForMore = true;
  }
  if (elapsedTime - lastSampleTime >= 1.0 / sampleRate)
  {
    if (!sampleQueue.empty())
    {
      sampleQueue.pop_front();
    }
    lastSampleTime = elapsedTime;
  }
  output(0) = sampleQueue.empty() ? 0.0 : sampleQueue.front();
}

void SamplePlayer::update(SamplePlayer::update_flags_t flag)
{
	switch (flag)
	{
  	case INIT:
  	sampleRate = INITIAL_SAMPLE_RATE;
	  setParameter(PARAM_SAMPLE_RATE, QString::number(sampleRate));
	  lastSampleTime = elapsedTime = 0.0;
		break;
		
  	case MODIFY:
	  sampleRate = getParameter(PARAM_SAMPLE_RATE).toDouble();
    sampleQueue.clear();
	  if (worker)
	  {
  	  worker->bail();
  	  worker->wait();
  	  delete worker;
  	  worker = NULL;
	  }
	  worker = new SampleWorker(sampleFilename->text(), this);
	  worker->start();
	  worker->fetchMoreSamples();
	  sampleFilename->blacken();
    lastSampleTime = elapsedTime = 0.0;
		break;
		
  	case PAUSE:
		output(0) = 0;
		break;
		
  	case PERIOD:
		dt_s = RT::System::getInstance()->getPeriod() * 1e-9;
		break;
		
		case EXIT:
		setActive(false);
		break;
		
  	default:
		break;
	}
}

void SamplePlayer::setSampleFilename()
{
  QFileDialog dialog(this, "Choose sample file", false);
  dialog.setMode(QFileDialog::ExistingFile);
  dialog.setViewMode(QFileDialog::List);
  if (dialog.exec() != QDialog::Accepted)
    return;
  sampleFilename->selectAll();
  sampleFilename->insert(dialog.selectedFile());
}

// required functions from default_gui_model DO NOT EDIT

void SamplePlayer::exit()
{
	update(EXIT);
	Plugin::Manager::getInstance()->unload(this);
}

void SamplePlayer::refresh()
{
  std::map<QString, param_t>::iterator i;
	for ( i = parameter.begin(); i != parameter.end(); ++i)
	{
		if (i->second.type & (STATE | EVENT))
		{
		  QString text = QString::number(getValue(i->second.type, i->second.index));
			i->second.edit->setText(text);
		}
		else if ((i->second.type & PARAMETER) &&
		         !i->second.edit->edited() &&
		         i->second.edit->text() != *i->second.str_value)
		{
			i->second.edit->setText(*i->second.str_value);
		}
		else if ((i->second.type & COMMENT) &&
		         !i->second.edit->edited() &&
		         i->second.edit->text() != getValueString(COMMENT, i->second.index))
		{
			i->second.edit->setText(getValueString(COMMENT, i->second.index));
		}
	}
	pauseButton->setOn(!getActive());
}

void SamplePlayer::modify()
{
	bool active = getActive();

	setActive(false);

	// Ensure that the realtime thread isn't in the middle of executing 
	// DefaultGUIModel::execute()
	SyncEvent event;
	RT::System::getInstance()->postEvent(&event);

  std::map<QString, param_t>::iterator i;
	for (i = parameter.begin(); i != parameter.end(); ++i)
	{
		if (i->second.type & COMMENT)
		{
			Workspace::Instance::setComment(i->second.index, 
			                                i->second.edit->text().latin1());
		}
	}

	update(MODIFY);
	setActive(active);

	for (i = parameter.begin(); i != parameter.end(); ++i)
		i->second.edit->blacken();
}

QString SamplePlayer::getComment(const QString &name)
{
	std::map<QString, param_t>::iterator n = parameter.find(name);
	if (n != parameter.end() && (n->second.type & COMMENT))
		return QString(getValueString(COMMENT, n->second.index));
	else
	  return "";
}

void SamplePlayer::setComment(const QString &name, QString comment)
{
	std::map<QString, param_t>::iterator n = parameter.find(name);
	if (n != parameter.end() && (n->second.type & COMMENT))
	{
		n->second.edit->setText(comment);
		Workspace::Instance::setComment(n->second.index, comment.latin1());
	}
}

QString SamplePlayer::getParameter(const QString &name)
{
	std::map<QString, param_t>::iterator n = parameter.find(name);
	if ((n != parameter.end()) && (n->second.type & PARAMETER))
	{
		*n->second.str_value = n->second.edit->text();
		setValue(n->second.index, n->second.edit->text().toDouble());
		return n->second.edit->text();
	}
	else
	  return "";
}

void SamplePlayer::setParameter(const QString &name, double value)
{
	std::map<QString, param_t>::iterator n = parameter.find(name);
	if ((n != parameter.end()) && (n->second.type & PARAMETER))
	{
		n->second.edit->setText(QString::number(value));
		*n->second.str_value = n->second.edit->text();
		setValue(n->second.index, n->second.edit->text().toDouble());
	}
}

void SamplePlayer::setParameter(const QString &name, const QString value)
{
	std::map<QString, param_t>::iterator n = parameter.find(name);
	if ((n != parameter.end()) && (n->second.type & PARAMETER))
	{
		n->second.edit->setText(value);
		*n->second.str_value = n->second.edit->text();
		setValue(n->second.index, n->second.edit->text().toDouble());
	}
}

void SamplePlayer::setState(const QString &name, double &ref)
{
	std::map<QString, param_t>::iterator n = parameter.find(name);
	if ((n != parameter.end()) && (n->second.type & STATE))
	{
		setData(Workspace::STATE, n->second.index, &ref);
		n->second.edit->setText(QString::number(ref));
	}
}

void SamplePlayer::setEvent(const QString &name, double &ref)
{
	std::map<QString, param_t>::iterator n = parameter.find(name);
	if ((n != parameter.end()) && (n->second.type & EVENT))
	{
		setData(Workspace::EVENT, n->second.index, &ref);
		n->second.edit->setText(QString::number(ref));
	}
}

void SamplePlayer::pause(bool p)
{
	if (pauseButton->isOn() != p)
		pauseButton->setDown(p);

	setActive(!p);
	if (p)
		update(PAUSE);
	else
		update(UNPAUSE);
}

void SamplePlayer::doLoad(const Settings::Object::State &s)
{
  std::map<QString, param_t>::iterator i;
	for (i = parameter.begin(); i != parameter.end(); ++i)
		i->second.edit->setText(s.loadString(i->first));
	pauseButton->setOn(s.loadInteger("paused"));
	sampleFilename->setText(s.loadString("filename"));
	modify();
}

void SamplePlayer::doSave(Settings::Object::State &s) const
{
	s.saveInteger("paused", pauseButton->isOn());
	std::map<QString, param_t>::const_iterator i;
	for (i = parameter.begin(); i != parameter.end(); ++i)
		s.saveString(i->first, i->second.edit->text());
	s.saveString("filename", sampleFilename->text());
}

void SamplePlayer::receiveEvent(const Event::Object *event)
{
	if (event->getName() == Event::RT_PREPERIOD_EVENT)
	{
		periodEventPaused = getActive();
		setActive(false);
	}
	else if (event->getName() == Event::RT_POSTPERIOD_EVENT)
	{
    #ifdef DEBUG
		if (getActive())
			ERROR_MSG("DefaultGUIModel::receiveEvent : "
			          "model unpaused during a period update\n");
    #endif
		update(PERIOD);
		setActive(periodEventPaused);
	}
}

void SamplePlayer::customEvent(QCustomEvent *e)
{
  if (e->type() != JobDoneEvent::code)
  {
    return;
  }
  // Ensure that the realtime thread isn't in the middle of executing 
	// DefaultGUIModel::execute()
	SyncEvent event;
	RT::System::getInstance()->postEvent(&event);
  for (size_t i = 0; i < worker->samples.size(); i++)
  {
    sampleQueue.push_back(worker->samples[i]);
  }
  askedForMore = false;
}
