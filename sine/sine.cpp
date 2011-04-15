#include <math.h>
#include <sine.h>

extern "C" Plugin::Object *createRTXIPlugin(void) {
    return new Sine();
}

const char *PARAM_AMPLITUDE = "Amplitude (V)";
const char *PARAM_FREQUENCY = "Frequency (Hz)";

static DefaultGUIModel::variable_t vars[] = {
    {
        "V (sine)",
        "V",
        DefaultGUIModel::OUTPUT,
    },
    {
        PARAM_AMPLITUDE,
        "Maximum (and, negated, minimum) voltage of sine wave",
        DefaultGUIModel::PARAMETER | DefaultGUIModel::DOUBLE,
    },
    {
        PARAM_FREQUENCY,
        "Number of waves in a second",
        DefaultGUIModel::PARAMETER | DefaultGUIModel::DOUBLE,
    },
};

static size_t num_vars = sizeof(vars)/sizeof(DefaultGUIModel::variable_t);

Sine::Sine(void)
    : DefaultGUIModel("Sine",::vars,::num_vars) {

    t = 0;
    amplitude = 1.0; setParameter(PARAM_AMPLITUDE, amplitude);
    frequency = 1.0; setParameter(PARAM_FREQUENCY, frequency);
    period = RT::System::getInstance()->getPeriod()*1e-9;

    refresh();
}

Sine::~Sine(void) {}

void Sine::execute(void) {
    t += period;
    output(0) = sin(t * frequency * 2.0 * M_PI) * amplitude;
}

void Sine::update(DefaultGUIModel::update_flags_t flag) {
    if(flag == MODIFY) {
        amplitude = getParameter(PARAM_AMPLITUDE).toDouble();
        frequency = getParameter(PARAM_FREQUENCY).toDouble();
    } else if(flag == PERIOD) {
        period = RT::System::getInstance()->getPeriod()*1e-9;
    } else if (flag == PAUSE) {
        output(0) = 0;
    }
}

