// Calculate variance ratio between sine wave and input.
// Copyright 2011 Nolan Waite

#include <default_gui_model.h>
#include <vector>

class QCustomEvent;


class Variancer : public DefaultGUIModel
{
public:
  Variancer(void);
  virtual ~Variancer(void);

  void execute(void);
  void customEvent(QCustomEvent *e);

protected:
  void update(DefaultGUIModel::update_flags_t);

private:
  double dt_s;
  double age;
  double sampleTime;
  double sineRate;
  double sineOut;
  
  std::vector<double> samplesIn, samplesOut;
  bool varianceCalculated;
};
