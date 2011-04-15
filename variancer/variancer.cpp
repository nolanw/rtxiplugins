#include <variancer.h>
#include <iostream>
#include <math.h>

#include <qapplication.h>
#include <qevent.h>


extern "C" Plugin::Object *createRTXIPlugin(void)
{
  return new Variancer();
}

#define PARAM_SINE_RATE "Sine rate (Hz)"
#define PARAM_SAMPLE_TIME "Sample time (s)"
#define PARAM_VARIANCE_RATIO "Variance, sine/in (V^2)"

#define INITIAL_SINE_RATE 50.0
#define INITIAL_SAMPLE_TIME 3.0
#define INITIAL_VARIANCE_RATIO 0.0

static DefaultGUIModel::variable_t vars[] =
{
  {
    "Vin (variancer)",
    "Input",
    DefaultGUIModel::INPUT,
  },
  {
    "Vout (variancer)",
    "Output",
    DefaultGUIModel::OUTPUT,
  },
  {
    PARAM_SINE_RATE,
    "Sine rate (Hz)",
    DefaultGUIModel::PARAMETER | DefaultGUIModel::DOUBLE,
  },
  {
    PARAM_SAMPLE_TIME,
    "How long to record input before calculating variance (s)",
    DefaultGUIModel::PARAMETER | DefaultGUIModel::DOUBLE,
  },
  {
    PARAM_VARIANCE_RATIO,
    "Ratio of variances, sinusoid over input (V^2)",
    DefaultGUIModel::PARAMETER | DefaultGUIModel::DOUBLE,
  }
};

static size_t num_vars = sizeof(vars) / sizeof(DefaultGUIModel::variable_t);


class VariancerEvent : public QCustomEvent
{
public:
  static const int EventType = QEvent::User + 2349;
  
  VariancerEvent(double sampleIn, double sampleOut) : 
    QCustomEvent(EventType), _sampleIn(sampleIn), _sampleOut(sampleOut)
  {
    _calculateVariance = false;
  }
  
  VariancerEvent(bool calculateVariance) :
    QCustomEvent(EventType), _calculateVariance(calculateVariance)
  {}
  
  double sampleIn();
  double sampleOut();
  bool calculateVariance();
  
private:
  double _sampleIn, _sampleOut;
  bool _calculateVariance;
};

double VariancerEvent::sampleIn()
{
  return _sampleIn;
}

double VariancerEvent::sampleOut()
{
  return _sampleOut;
}

bool VariancerEvent::calculateVariance()
{
  return _calculateVariance;
}


Variancer::Variancer(void)
  : DefaultGUIModel("Variancer", ::vars, ::num_vars), varianceCalculated(false)
{
  update(PERIOD);
  setParameter(PARAM_SINE_RATE, INITIAL_SINE_RATE);
  setParameter(PARAM_SAMPLE_TIME, INITIAL_SAMPLE_TIME);
  setParameter(PARAM_VARIANCE_RATIO, INITIAL_VARIANCE_RATIO);
  update(MODIFY);

  refresh();
}

Variancer::~Variancer(void) {}

void Variancer::execute(void)
{
  age += dt_s;
  
  if (age < sampleTime)
  {
    sineOut = sin(2.0 * M_PI * sineRate * age);
    QApplication::postEvent(this, new VariancerEvent(input(0), sineOut));
    output(0) = sineOut;
  }
  else if (!varianceCalculated)
  {
    QApplication::postEvent(this, new VariancerEvent(true));
    output(0) = 0.0;
  }
  else
    output(0) = 0.0;
}

void Variancer::update(DefaultGUIModel::update_flags_t flag)
{
  switch (flag)
  {
    case MODIFY:
    sineRate = getParameter(PARAM_SINE_RATE).toDouble();
    samplesIn.clear();
    samplesOut.clear();
    sampleTime = getParameter(PARAM_SAMPLE_TIME).toDouble();
    age = 0.0;
    varianceCalculated = false;
    break;
    
    case PAUSE:
    output(0) = 0;
    break;
    
    case PERIOD:
    dt_s = RT::System::getInstance()->getPeriod() * 1e-9;
    break;
    
    default:
    break;
  }
}

void Variancer::customEvent(QCustomEvent *e)
{
  if (e->type() != VariancerEvent::EventType)
    return;
  
  VariancerEvent *ve = (VariancerEvent *)e;
  if (!ve->calculateVariance())
  {
    samplesIn.push_back(ve->sampleIn());
    samplesOut.push_back(ve->sampleOut());
  }
  else
  {
    varianceCalculated = true;
    double meanIn = 0;
    double meanOut = 0;
    for (size_t i = 0; i < samplesIn.size(); i++)
    {
      meanIn += samplesIn[i];
      meanOut += samplesOut[i];
    }
    double varianceIn = 0;
    double varianceOut = 0;
    for (size_t i = 0; i < samplesIn.size(); i++)
    {
      varianceIn += pow(samplesIn[i] - meanIn, 2);
      varianceOut += pow(samplesOut[i] - meanOut, 2);
    }
    varianceIn /= samplesIn.size();
    varianceOut /= samplesOut.size();
    if (varianceOut != 0.0)
      setParameter(PARAM_VARIANCE_RATIO, varianceIn / varianceOut);
    else
      setParameter(PARAM_VARIANCE_RATIO, 0.0);
  }
}
