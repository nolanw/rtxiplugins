
#include <mux.h>

extern "C" Plugin::Object *createRTXIPlugin(void) {
    return new Mux();
}

#define PARAM_V_MAX "Vmax (V)"
#define PARAM_SCALE_FACTOR "Scale factor"
#define PARAM_OFFSET "Offset (V)"

#define INITIAL_V_MAX 10.0

static DefaultGUIModel::variable_t vars[] = {
  {
    "Vin0",
    "An input",
    DefaultGUIModel::INPUT,
  },
  {
    "Vin1",
    "Another input",
    DefaultGUIModel::INPUT,
  },
  {
    "Vin2",
    "Another input",
    DefaultGUIModel::INPUT,
  },
  {
    "Vin3",
    "Another input",
    DefaultGUIModel::INPUT,
  },
  {
    "Vin4",
    "Another input",
    DefaultGUIModel::INPUT,
  },
  {
    "Vout (muxed)",
    "Aggregated output",
    DefaultGUIModel::OUTPUT,
  },
  {
    PARAM_SCALE_FACTOR,
    "Multiply aggregate voltage over inputs by this factor",
    DefaultGUIModel::PARAMETER | DefaultGUIModel::DOUBLE,
  },
  {
    PARAM_OFFSET,
    "Add this voltage to the aggregate voltage after applying the scale "
    "factor",
    DefaultGUIModel::PARAMETER | DefaultGUIModel::DOUBLE,
  },
  {
    PARAM_V_MAX,
    "If the output (after scaling and offsetting) would exceed this, cap it",
    DefaultGUIModel::PARAMETER | DefaultGUIModel::DOUBLE,
  },
};

static size_t num_vars = sizeof(vars)/sizeof(DefaultGUIModel::variable_t);

Mux::Mux(void) : DefaultGUIModel("Mux", ::vars, ::num_vars) {
  Vmax = INITIAL_V_MAX; setParameter(PARAM_V_MAX, Vmax);
  factor = 1.0; setParameter(PARAM_SCALE_FACTOR, factor);
  offset = 0.0; setParameter(PARAM_OFFSET, offset);
  
  refresh();
}

Mux::~Mux(void) {}

void Mux::execute(void) {
  Vout = input(0) + input(1) + input(2) + input(3) + input(4);
  Vout *= factor;
  Vout += offset;
  if (Vout > Vmax) {
    Vout = Vmax;
  }
  output(0) = Vout;
}

void Mux::update(DefaultGUIModel::update_flags_t flag) {
  if (flag == PAUSE) {
    output(0) = 0;
  }
  else if (flag == MODIFY) {
    Vmax = getParameter(PARAM_V_MAX).toDouble();
    factor = getParameter(PARAM_SCALE_FACTOR).toDouble();
    offset = getParameter(PARAM_OFFSET).toDouble();
  }
}
