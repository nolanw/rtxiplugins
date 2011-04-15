/*
 IncrementalPlot, derived from QwtPlot, contains a container class for growing data and allows you
 to plot data asynchronously of any size from a single (x,y) point to a series of points. It will 
 also draw lines, e.g. data fits, holding line data in a separate structure.
 */

#ifndef _INCREMENTALPLOT_H_
#define _INCREMENTALPLOT_H_ 1

#include "basicplot.h"
#include <vector>
#include <qwt-qt3/qwt_array.h>
#include <qwt-qt3/qwt_plot.h>
#include <qwt-qt3/qwt_symbol.h>


class QwtPlotCurve;

class CurveData
{
  // A container class for growing data
public:

  CurveData();

  void append(double *x, double *y, int count);

  int count() const;
  int size() const;
  const double *x() const;
  const double *y() const;
  
  void truncate();

private:
  int d_count;
  QwtArray<double> d_x;
  QwtArray<double> d_y;
};

class IncrementalPlot : public BasicPlot
{
public:
  IncrementalPlot(QWidget *parent = NULL);
  void appendLine(double x, double y); // append a point to the current curve
  void startNewCurve(void);
  void removeData(void); // clears all data and lines

private:
  std::vector<QwtPlotCurve *> curves;
  CurveData l_data; // holds points defining lines
  int curCurveOffset; // how far into l_data the current curve's data starts
};

#endif // _INCREMENTALPLOT_H_

