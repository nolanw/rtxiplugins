#include <default_gui_model.h>

class Sine : public DefaultGUIModel
{

public:

    Sine(void);
    virtual ~Sine(void);

    void execute(void);

protected:

    void update(DefaultGUIModel::update_flags_t);

private:

    double t;
    double period;
    double amplitude;
    double frequency;

};
