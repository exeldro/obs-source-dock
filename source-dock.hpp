#pragma once

#include <qboxlayout.h>
#include <QCheckBox>
#include <QDockWidget>
#include <QSlider>
#include <QPlainTextEdit>

#include "obs.h"
#include <../UI/obs-frontend-api/obs-frontend-api.h>

#include "media-control.hpp"
#include "obs.hpp"
#include "qt-display.hpp"
#include "volume-meter.hpp"

#define SHOW_PREVIEW 1
#define SHOW_AUDIO 2
#define SHOW_VOLUME 4
#define SHOW_MUTE 8
#define SHOW_MEDIA 16
#define SHOW_ALL 31

typedef std::function<bool(QObject *, QEvent *)> EventFilterFunc;

class OBSEventFilter : public QObject {
	Q_OBJECT
public:
	OBSEventFilter(EventFilterFunc filter_) : filter(filter_) {}

protected:
	bool eventFilter(QObject *obj, QEvent *event)
	{
		return filter(obj, event);
	}

public:
	EventFilterFunc filter;
};

class LockedCheckBox : public QCheckBox {
	Q_OBJECT

public:
	LockedCheckBox();
	explicit LockedCheckBox(QWidget *parent);
};

class VisibilityCheckBox : public QCheckBox {
	Q_OBJECT

public:
	VisibilityCheckBox();
	explicit VisibilityCheckBox(QWidget *parent);
};

class MuteCheckBox : public QCheckBox {
	Q_OBJECT
};

class SliderIgnoreScroll : public QSlider {
	Q_OBJECT

public:
	SliderIgnoreScroll(QWidget *parent = nullptr);
	SliderIgnoreScroll(Qt::Orientation orientation,
			   QWidget *parent = nullptr);

protected:
	virtual void wheelEvent(QWheelEvent *event) override;
};

class SourceDock : public QDockWidget {
	Q_OBJECT

private:
	OBSSource source;
	std::unique_ptr<OBSEventFilter> eventFilter;
	QAction *action;

	OBSQTDisplay *preview;
	VolumeMeter *volMeter;
	obs_volmeter_t *obs_volmeter;
	LockedCheckBox *locked;
	SliderIgnoreScroll *slider;
	MuteCheckBox *mute;
	MediaControl *mediaControl;
	QVBoxLayout *mainLayout;
	QWidget *volControl;
	bool switch_scene_enabled;
	QLabel *activeLabel;
	QWidget *sceneItems;
	QPushButton *propertiesButton;
	QPushButton *filtersButton;
	QPlainTextEdit *textInput;
	QTimer *textInputTimer;

	OBSSignal visibleSignal;
	OBSSignal addSignal;
	OBSSignal removeSignal;
	OBSSignal reorderSignal;
	OBSSignal refreshSignal;

	static void DrawPreview(void *data, uint32_t cx, uint32_t cy);

	static void OBSVolumeLevel(void *data,
				   const float magnitude[MAX_AUDIO_CHANNELS],
				   const float peak[MAX_AUDIO_CHANNELS],
				   const float inputPeak[MAX_AUDIO_CHANNELS]);
	static void OBSVolume(void *data, calldata_t *calldata);
	static void OBSMute(void *data, calldata_t *calldata);
	static void OBSActiveChanged(void *, calldata_t *);
	static bool AddSceneItem(obs_scene_t *scene, obs_sceneitem_t *item,
				 void *data);

	bool GetSourceRelativeXY(int mouseX, int mouseY, int &x, int &y);

	bool HandleMouseClickEvent(QMouseEvent *event);
	bool HandleMouseMoveEvent(QMouseEvent *event);
	bool HandleMouseWheelEvent(QWheelEvent *event);
	bool HandleFocusEvent(QFocusEvent *event);
	bool HandleKeyEvent(QKeyEvent *event);

	OBSEventFilter *BuildEventFilter();

private slots:
	void LockVolumeControl(bool lock);
	void MuteVolumeControl(bool mute);
	void SliderChanged(int vol);
	void SetOutputVolume(double volume);
	void SetMute(bool muted);
	void ActiveChanged();
	void VisibilityChanged(int id);
	void RefreshItems();

public:
	SourceDock(OBSSource source, QWidget *parent = nullptr);
	~SourceDock();

	void SetActive(int active);

	void setAction(QAction *action);

	OBSSource GetSource();
	void EnablePreview();
	void DisablePreview();
	bool PreviewEnabled();

	void EnableVolMeter();
	void DisableVolMeter();
	bool VolMeterEnabled();

	void EnableVolControls();
	void DisableVolControls();
	bool VolControlsEnabled();

	void EnableMediaControls();
	void DisableMediaControls();
	bool MediaControlsEnabled();

	void EnableSwitchScene();
	void DisableSwitchScene();
	bool SwitchSceneEnabled();

	void EnableShowActive();
	void DisableShowActive();
	bool ShowActiveEnabled();

	void EnableSceneItems();
	void DisableSceneItems();
	bool SceneItemsEnabled();

	void EnableProperties();
	void DisableProperties();
	bool PropertiesEnabled();

	void EnableFilters();
	void DisableFilters();
	bool FiltersEnabled();

	void EnableTextInput();
	void DisableTextInput();
	bool TextInputEnabled();
};

inline std::list<SourceDock *> source_docks;
inline std::list<QMainWindow *> source_windows;
