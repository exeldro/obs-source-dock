#pragma once

#include <QTimer>
#include <obs.hpp>
#include <qicon.h>
#include <QLabel>
#include <QPushButton>

#include "media-slider.hpp"


class MediaControl : public QWidget {
	Q_OBJECT

private:
	OBSWeakSource weakSource;
	QLabel *nameLabel;
	MediaSlider *slider;
	QPushButton *restartButton;
	QPushButton *playPauseButton;
	QPushButton *previousButton;
	QPushButton *stopButton;
	QPushButton *nextButton;
	QLabel *timeLabel;
	QLabel *durationLabel;
	QTimer *timer;
	QTimer *seekTimer;
	int seek;
	int lastSeek;
	bool prevPaused = false;
	bool showTimeDecimals = false;
	bool showTimeRemaining = false;

	QString FormatSeconds(float totalSeconds);
	void StartTimer();
	void StopTimer();
	void RefreshControls();

	static void OBSMediaStopped(void *data, calldata_t *calldata);
	static void OBSMediaPlay(void *data, calldata_t *calldata);
	static void OBSMediaPause(void *data, calldata_t *calldata);
	static void OBSMediaStarted(void *data, calldata_t *calldata);

private slots:
	void on_restartButton_clicked();
	void on_playPauseButton_clicked();
	void on_stopButton_clicked();
	void on_nextButton_clicked();
	void on_previousButton_clicked();
	void SliderClicked();
	void SliderReleased();
	void SliderHovered(int val);
	void SliderMoved(int val);
	void SetSliderPosition();
	void SetPlayingState();
	void SetPausedState();
	void SetRestartState();
	void SeekTimerCallback();

public:
	explicit MediaControl(OBSWeakSource source, bool showMs,
			      bool showTimeRemaining);
	~MediaControl();
	OBSWeakSource GetSource();
	void SetSource(OBSWeakSource source);
};
