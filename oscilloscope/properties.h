#ifndef PROPERTIES_H_AF6ZEZDM
#define PROPERTIES_H_AF6ZEZDM

#include <event.h>

#include <qdialog.h>

class QButtonGroup;
class QCheckBox;
class QComboBox;
class QGroupBox;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QTabWidget;


namespace Oscilloscope
{
  
class Panel;

class Properties : public QDialog, public Event::Handler {

    Q_OBJECT

    friend class Panel;

public:

    Properties(Panel *);
    ~Properties(void);

    void receiveEvent(const ::Event::Object *);

protected:

    void closeEvent(QCloseEvent *);

private slots:

    void activateChannel(bool);
    void apply(void);
    void buildChannelList(void);
    void okay(void);
    void showTab(void);

private:

    void applyAdvancedTab(void);
    void applyChannelTab(void);
    void applyDisplayTab(void);
    void createAdvancedTab(void);
    void createChannelTab(void);
    void createDisplayTab(void);
    void showAdvancedTab(void);
    void showChannelTab(void);
    void showDisplayTab(void);

    Panel *panel;

    QTabWidget *tabWidget;

    QSpinBox *divXSpin;
    QSpinBox *divYSpin;
    QSpinBox *rateSpin;
    QSpinBox *sizeSpin;

    QComboBox *blockList;
    QComboBox *channelList;
    QComboBox *colorList;
    QComboBox *offsetList;
    QComboBox *scaleList;
    QComboBox *styleList;
    QComboBox *typeList;
    QComboBox *widthList;
    QGroupBox *displayBox;
    QGroupBox *lineBox;
    QLineEdit *offsetEdit;
    QPushButton *activateButton;

    QButtonGroup *trigGroup;
    QComboBox *timeList;
    QComboBox *trigChanList;
    QComboBox *trigThreshList;
    QSpinBox *refreshSpin;
    QLineEdit *trigThreshEdit;
    QCheckBox *trigHoldingCheck;
    QLineEdit *trigHoldoffEdit;
    QComboBox *trigHoldoffList;

}; // class Properties

}

#endif /* end of include guard: PROPERTIES_H_AF6ZEZDM */
