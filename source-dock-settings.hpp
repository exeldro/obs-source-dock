

#include <qcheckbox.h>
#include <QComboBox>
#include <QDialog>
#include <QGridLayout>
#include <QLabel>
#include <QMainWindow>
#include <qtextedit.h>

#include "obs.h"

class SourceDockSettingsDialog : public QDialog {
	Q_OBJECT
	QGridLayout *mainLayout;
	QComboBox *sourceCombo;
	QLineEdit *titleEdit;
	QCheckBox *visibleCheckBox;
	QCheckBox *previewCheckBox;
	QCheckBox *volMeterCheckBox;
	QCheckBox *volControlsCheckBox;
	QCheckBox *mediaControlsCheckBox;
	QCheckBox *switchSceneCheckBox;
	QCheckBox *showActiveCheckBox;
	QCheckBox *sceneItemsCheckBox;
	QCheckBox *propertiesCheckBox;
	QCheckBox *filtersCheckBox;

	int selectBoxColumn;

	static bool AddSource(void *data, obs_source_t *source);

	void AddClicked();
	void DeleteClicked();
	void SelectAllChanged();

public:
	SourceDockSettingsDialog(QMainWindow *parent = nullptr);
	~SourceDockSettingsDialog();
public slots:
	void RefreshTable();

protected:
	virtual void mouseDoubleClickEvent(QMouseEvent *event) override;
};

class VerticalLabel : public QLabel {
	Q_OBJECT

public:
	explicit VerticalLabel(QWidget *parent = 0);
	explicit VerticalLabel(const QString &text, QWidget *parent = 0);

protected:
	void paintEvent(QPaintEvent *);
	QSize sizeHint() const;
	QSize minimumSizeHint() const;
};
