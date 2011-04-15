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

/*
Istep increases current stepwise according to parameters you specify. It also 
displays output current and input voltage on nearby plots, so you can examine 
your handiwork as it happens and fiddle if something's not right.

Several parameters affect one another. Changing the minimum or maximum current 
will automatically update the step size. Changing the number of steps will also 
update the step size (and vice versa). Changing two parameters to incompatible 
values (e.g. a step size not equal to ((Imax - Imin) / #steps)) will result in 
one of them arbitrarily being chosen.

This is a considerably edited version of the Istep plugin available on the 
RTXI website.

Anything that mentions the "default GUI model" is stuff that really shouldn't 
have to be here, but unfortunately we're stuck with it for now. It handles the 
parameters set by the user (the labels and text boxes on the left).
*/

#include "include/incrementalplot.h"
#include <string>
#include <map>
#include <qobject.h>
#include <qstring.h>
#include <qwidget.h>
#include <rt.h>
#include <plugin.h>
#include <workspace.h>
#include <event.h>
#include <default_gui_model.h>

using namespace std;

class QLabel;
class QPushButton;

class Istep : public QWidget, public RT::Thread, public Plugin::Object, public Workspace::Instance, public Event::Handler
{

Q_OBJECT // macro needed if slots are implemented

public:

	// from default_gui_model
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

  enum update_flags_t
  {
	  INIT, /*!< The parameters need to be initialized.         */
	  MODIFY, /*!< The parameters have been modified by the user. */
	  PERIOD, /*!< The system period has changed.                 */
	  PAUSE, /*!< The Pause button has been activated            */
	  UNPAUSE, /*!< When the pause button has been deactivated     */
	  EXIT,
	  // add whatever additional flags you want here
	};

  Istep(void);
  virtual ~Istep(void);
  virtual void update(update_flags_t flag);
  virtual void execute(void);
  
  void customEvent(QCustomEvent *e);

public slots:

	void exit(void);
	void refresh(void);
	void modify(void);
	void pause(bool);

protected:

	QString getParameter(const QString &name);
	void setParameter(const QString &name, double value);
	void setParameter(const QString &name, const QString value);
	QString getComment(const QString &name);
	void setComment(const QString &name, const QString comment);
	void setState(const QString &name, double &ref);
	void setEvent(const QString &name, double &ref);

private:
  #define EPS 1e-9
  double V, Iout;

  double dt;
  double period;
  double delay;
  double Amin, Amax;
  int Nsteps;
  double stepSize;
  int Ncycles;
  int cycle;
  double duty;
  double offset;
  double factor;

  double deltaI;
  double step, timeSinceStep;
  
  long periodsSincePlot;
  void plot(double t, double V, double I);
	void setVPlotRange(double xmin, double xmax, double ymin, double ymax);
  void newVDataPoint(double newx, double newy);
  void setIPlotRange(double xmin, double xmax, double ymin, double ymax);
  void newIDataPoint(double newx, double newy);
  void startNewCurve(void);
  void clearPlot(void);
  
  // QT components
	QPushButton *pauseButton;
	IncrementalPlot *vplot, *iplot;
  
  // Plugin functions
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
};
