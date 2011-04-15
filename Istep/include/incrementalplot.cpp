#include <qwt-qt3/qwt_plot.h>
#include <qwt-qt3/qwt_plot_canvas.h>
#include <qwt-qt3/qwt_plot_curve.h>
#include "incrementalplot.h"
#if QT_VERSION >= 0x040000
#include <qpaintengine.h>
#endif

CurveData::CurveData() :
  d_count(0), d_x(1000000), d_y(1000000)
{
}

void
CurveData::append(double *x, double *y, int count)
{
  if ((uint)d_count + count >= size())
  {
    int newSize = d_count + d_count;
    d_x.resize(newSize);
    d_y.resize(newSize);
  }

  for (register int i = 0; i < count; i++)
  {
    d_x[d_count + i] = x[i];
    d_y[d_count + i] = y[i];
  }
  d_count += count;
}

void
CurveData::truncate()
{
  d_count = 0;
}

int
CurveData::count() const
{
  return d_count;
}

int
CurveData::size() const
{
  return d_x.size();
}

const double *
CurveData::x() const
{
  return d_x.data();
}

const double *
CurveData::y() const
{
  return d_y.data();
}

IncrementalPlot::IncrementalPlot(QWidget *parent) :
  BasicPlot(parent), curCurveOffset(0)
{
  setAutoReplot(false);
}

void
IncrementalPlot::appendLine(double x, double y)
{
  if (curves.empty())
    startNewCurve();
  
  QwtPlotCurve *l_curve = curves.back();
  l_data.append(&x, &y, 1);
  l_curve->setRawData(l_data.x() + curCurveOffset, 
                      l_data.y() + curCurveOffset, 
                      l_data.count() - curCurveOffset);

  const bool cacheMode = canvas()->testPaintAttribute(QwtPlotCanvas::PaintCached);
  canvas()->setPaintAttribute(QwtPlotCanvas::PaintCached, false);
  int start = l_curve->dataSize() - 2;
  if (start < 0)
    start = 0;
  l_curve->draw(start, l_curve->dataSize() - 1);
  canvas()->setPaintAttribute(QwtPlotCanvas::PaintCached, cacheMode);
}

void
IncrementalPlot::removeData()
{
  std::vector<QwtPlotCurve *>::iterator i;
  for (i = curves.begin(); i < curves.end(); ++i)
    (*i)->attach(NULL);
  curves.resize(0);
  l_data.truncate();
  curCurveOffset = 0;
  
  replot();
}

void IncrementalPlot::startNewCurve()
{
  if (!curves.empty())
    curCurveOffset += curves.back()->dataSize();
  QwtPlotCurve *newCurve = new QwtPlotCurve("Line");
  newCurve->setStyle(QwtPlotCurve::Lines);
  newCurve->setPaintAttribute(QwtPlotCurve::PaintFiltered);
  newCurve->setPen(QColor(Qt::white));
  const QColor &c = Qt::white;
  newCurve->setSymbol(QwtSymbol(QwtSymbol::NoSymbol, QBrush(c), QPen(c), QSize(6, 6)));

  curves.push_back(newCurve);
  newCurve->attach(this);
}

