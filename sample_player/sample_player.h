/*
 * Read samples from a file at some rate.
 * IO is done by a worker thread. The Player asks for more samples when it gets 
 * low, and when the worker is ready the new samples are copied in.
 */

#include <event.h>
#include <plugin.h>
#include <rt.h>
#include <default_gui_model.h>
#include <workspace.h>

class SampleWorker;

#include <deque>
#include <map>

#include <qobject.h>
#include <qwidget.h>

class QLabel;
class QPushButton;


class SamplePlayer : public QWidget, 
                     public RT::Thread, 
                     public Plugin::Object, 
                     public Workspace::Instance, 
                     public Event::Handler
{
  Q_OBJECT

public:

	/* DO NOT EDIT From default_gui_model */
	static const IO::flags_t INPUT = Workspace::INPUT;
	static const IO::flags_t OUTPUT = Workspace::OUTPUT;
	static const IO::flags_t PARAMETER = Workspace::PARAMETER;
	static const IO::flags_t STATE = Workspace::STATE;
	static const IO::flags_t EVENT = Workspace::EVENT;
	static const IO::flags_t COMMENT = Workspace::COMMENT;
	static const IO::flags_t DOUBLE = Workspace::COMMENT << 1;
	static const IO::flags_t INTEGER = Workspace::COMMENT << 2;
	static const IO::flags_t UINTEGER = Workspace::COMMENT << 3;
	typedef Workspace::variable_t variable_t;

	/* END DO NOT EDIT */

	enum update_flags_t {
		INIT, /*!< The parameters need to be initialized.         */
		MODIFY, /*!< The parameters have been modified by the user. */
		PERIOD, /*!< The system period has changed.                 */
		PAUSE, /*!< The Pause button has been activated            */
		UNPAUSE, /*!< When the pause button has been deactivated     */
		EXIT,
	// add whatever additional flags you want here
	};

	SamplePlayer();
	virtual ~SamplePlayer();
	virtual void update(update_flags_t flag);
	void execute();
	virtual void customEvent(QCustomEvent *e);

public slots:
	/* DO NOT EDIT */
	void exit(void);
	void refresh(void);
	void modify(void);
	void pause(bool);
	/* END DO NOT EDIT */

protected:
	QString getParameter(const QString &name);
	void setParameter(const QString &name, double value);
	void setParameter(const QString &name, const QString value);
	QString getComment(const QString &name);
	void setComment(const QString &name, const QString comment);
	void setState(const QString &name, double &ref);
	void setEvent(const QString &name, double &ref);

private:
  double dt_s;
  double elapsedTime;
  double sampleRate;
  double lastSampleTime;
  std::deque<double> sampleQueue;
  SampleWorker *worker;
  bool askedForMore;

	// QT components
	DefaultGUILineEdit *sampleFilename;
	QPushButton *pauseButton;

  void loadSamplesFromFile(QString filename);

	/* DO NOT EDIT */
	void doLoad(const Settings::Object::State &);
	void doSave(Settings::Object::State &) const;
	void receiveEvent(const Event::Object *);
	struct param_t {
		QLabel *label;
		DefaultGUILineEdit *edit;
		IO::flags_t type;
		size_t index;
		QString *str_value;
	};
	bool periodEventPaused;
	mutable QString junk;
	std::map<QString, param_t> parameter;
	/* END DO NOT EDIT */

private slots:
	void setSampleFilename();
};

class JobDoneEvent : public QCustomEvent
{
public:
  static int const code = 31812;
  JobDoneEvent() : QCustomEvent(code) {}
};
