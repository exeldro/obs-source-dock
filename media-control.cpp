#include "media-control.hpp"
#include <QToolTip>
#include <QMainWindow>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QStyle>
#include <QToolTip>
#include "../../item-widget-helpers.hpp"

MediaControl::MediaControl(OBSWeakSource source_, bool showTimeDecimals_,
			   bool showTimeRemaining_)
	: weakSource(std::move(source_)),
	  showTimeDecimals(showTimeDecimals_),
	  showTimeRemaining(showTimeRemaining_)
{
	OBSSource source = OBSGetStrongRef(weakSource);

	timer = new QTimer(this);
	connect(timer, SIGNAL(timeout()), this, SLOT(SetSliderPosition()));

	seekTimer = new QTimer(this);
	connect(seekTimer, SIGNAL(timeout()), this, SLOT(SeekTimerCallback()));

	QString sourceName = obs_source_get_name(source);
	setObjectName(sourceName);

	QVBoxLayout *mainLayout = new QVBoxLayout;
	mainLayout->setContentsMargins(4, 4, 4, 4);
	mainLayout->setSpacing(2);

	QHBoxLayout *sliderLayout = new QHBoxLayout;
	sliderLayout->setAlignment(Qt::AlignCenter);
	sliderLayout->setContentsMargins(0, 0, 0, 0);
	sliderLayout->setSpacing(2);
	timeLabel = new QLabel();
	sliderLayout->addWidget(timeLabel);
	slider = new MediaSlider();
	slider->setOrientation(Qt::Horizontal);
	slider->setTracking(false);
	slider->setMinimum(0);
	slider->setMaximum(4096);
	sliderLayout->addWidget(slider);
	durationLabel = new QLabel();
	sliderLayout->addWidget(durationLabel);

	QHBoxLayout *nameLayout = new QHBoxLayout;
	nameLayout->setAlignment(Qt::AlignLeft);
	nameLayout->setContentsMargins(0, 0, 0, 0);
	nameLayout->setSpacing(4);

	previousButton = new QPushButton();
	previousButton->setMinimumSize(22, 22);
	previousButton->setMaximumSize(22, 22);
	previousButton->setProperty("themeID", "previousIcon");
	previousButton->setIconSize(QSize(20, 20));
	nameLayout->addWidget(previousButton);

	restartButton = new QPushButton();
	restartButton->setMinimumSize(22, 22);
	restartButton->setMaximumSize(22, 22);
	restartButton->setProperty("themeID", "restartIcon");
	restartButton->setIconSize(QSize(20, 20));
	nameLayout->addWidget(restartButton);

	playPauseButton = new QPushButton();
	playPauseButton->setMinimumSize(22, 22);
	playPauseButton->setMaximumSize(22, 22);
	playPauseButton->setProperty("themeID", "playIcon");
	playPauseButton->setIconSize(QSize(20, 20));
	nameLayout->addWidget(playPauseButton);

	stopButton = new QPushButton();
	stopButton->setMinimumSize(22, 22);
	stopButton->setMaximumSize(22, 22);
	stopButton->setProperty("themeID", "stopIcon");
	stopButton->setIconSize(QSize(20, 20));
	nameLayout->addWidget(stopButton);

	nextButton = new QPushButton();
	nextButton->setMinimumSize(22, 22);
	nextButton->setMaximumSize(22, 22);
	nextButton->setProperty("themeID", "nextIcon");
	nextButton->setIconSize(QSize(20, 20));
	nameLayout->addWidget(nextButton);

	nameLabel = new QLabel();
	nameLayout->addWidget(nameLabel);

	mainLayout->addItem(sliderLayout);
	mainLayout->addItem(nameLayout);

	setLayout(mainLayout);
	nameLabel->setText(sourceName);

	slider->setValue(0);
	float time = (float)obs_source_media_get_time(source) / 1000.0f;
	float duration = (float)obs_source_media_get_duration(source) / 1000.0f;
	if (showTimeRemaining) {
		timeLabel->setText(FormatSeconds(duration));
		durationLabel->setText(FormatSeconds(duration - time));
	} else {
		timeLabel->setText(FormatSeconds(time));
		durationLabel->setText(FormatSeconds(duration));
	}
	slider->setEnabled(false);

	connect(slider, SIGNAL(sliderPressed()), this,
		SLOT(SliderClicked()));
	connect(slider, SIGNAL(mediaSliderHovered(int)), this,
		SLOT(SliderHovered(int)));
	connect(slider, SIGNAL(sliderReleased()), this,
		SLOT(SliderReleased()));
	connect(slider, SIGNAL(sliderMoved(int)), this,
		SLOT(SliderMoved(int)));

	connect(restartButton, SIGNAL(clicked()), this,
		SLOT(on_restartButton_clicked()));
	connect(playPauseButton, SIGNAL(clicked()), this,
		SLOT(on_playPauseButton_clicked()));
	connect(stopButton, SIGNAL(clicked()), this,
		SLOT(on_stopButton_clicked()));
	connect(nextButton, SIGNAL(clicked()), this,
		SLOT(on_nextButton_clicked()));
	connect(previousButton, SIGNAL(clicked()), this,
		SLOT(on_previousButton_clicked()));

	signal_handler_t *sh = obs_source_get_signal_handler(source);
	signal_handler_connect(sh, "media_play", OBSMediaPlay, this);
	signal_handler_connect(sh, "media_pause", OBSMediaPause, this);
	signal_handler_connect(sh, "media_restart", OBSMediaPlay, this);
	signal_handler_connect(sh, "media_stopped", OBSMediaStopped, this);
	signal_handler_connect(sh, "media_started", OBSMediaStarted, this);
	signal_handler_connect(sh, "media_ended", OBSMediaStopped, this);

	RefreshControls();
}

MediaControl::~MediaControl()
{
	OBSSource source = OBSGetStrongRef(weakSource);
	if (!source)
		return;
	signal_handler_t *sh = obs_source_get_signal_handler(source);
	signal_handler_disconnect(sh, "media_play", OBSMediaPlay, this);
	signal_handler_disconnect(sh, "media_pause", OBSMediaPause, this);
	signal_handler_disconnect(sh, "media_restart", OBSMediaPlay, this);
	signal_handler_disconnect(sh, "media_stopped", OBSMediaStopped, this);
	signal_handler_disconnect(sh, "media_started", OBSMediaStarted, this);
	signal_handler_disconnect(sh, "media_ended", OBSMediaStopped, this);
}

void MediaControl::OBSMediaStopped(void *data, calldata_t *calldata)
{
	UNUSED_PARAMETER(calldata);

	MediaControl *media = static_cast<MediaControl *>(data);
	QMetaObject::invokeMethod(media, "SetRestartState");
}

void MediaControl::OBSMediaPlay(void *data, calldata_t *calldata)
{
	UNUSED_PARAMETER(calldata);

	MediaControl *media = static_cast<MediaControl *>(data);
	QMetaObject::invokeMethod(media, "SetPlayingState");
}

void MediaControl::OBSMediaPause(void *data, calldata_t *calldata)
{
	UNUSED_PARAMETER(calldata);

	MediaControl *media = static_cast<MediaControl *>(data);
	QMetaObject::invokeMethod(media, "SetPausedState");
}

void MediaControl::OBSMediaStarted(void *data, calldata_t *calldata)
{
	UNUSED_PARAMETER(calldata);

	MediaControl *media = static_cast<MediaControl *>(data);
	QMetaObject::invokeMethod(media, "SetPlayingState");
}

void MediaControl::SliderClicked()
{
	OBSSource source = OBSGetStrongRef(weakSource);
	if (!source) {
		return;
	}

	obs_media_state state = obs_source_media_get_state(source);

	if (state == OBS_MEDIA_STATE_PAUSED) {
		prevPaused = true;
	} else if (state == OBS_MEDIA_STATE_PLAYING) {
		prevPaused = false;
		obs_source_media_play_pause(source, true);
		if (timer->isActive())
			timer->stop();
	}

	seek = slider->value();
	seekTimer->start(100);
}

void MediaControl::SliderReleased()
{
	OBSSource source = OBSGetStrongRef(weakSource);
	if (!source) {
		return;
	}

	if (seekTimer->isActive()) {
		seekTimer->stop();
		if (lastSeek != seek) {
			const float percent = (float)seek / float(slider->maximum());
			const int64_t seekTo = (float)percent * (float)obs_source_media_get_duration(source);
			obs_source_media_set_time(source, seekTo);
		}

		seek = lastSeek = -1;
	}

	if (!prevPaused) {
		obs_source_media_play_pause(source, false);
		if (!timer->isActive())
			timer->start(1000);
	}
}

void MediaControl::SliderHovered(int val)
{
	float percent = (float)val / float(slider->maximum());
	OBSSource source = OBSGetStrongRef(weakSource);
	float seconds = (showTimeRemaining ? 1.0 - percent : percent) *
			(obs_source_media_get_duration(source) / 1000.0f);

	QToolTip::showText(QCursor::pos(), FormatSeconds(seconds), this);
}

void MediaControl::SliderMoved(int val)
{
	if (seekTimer->isActive()) {
		seek = val;
	}
}

void MediaControl::SeekTimerCallback()
{
	if (lastSeek != seek) {
		OBSSource source = OBSGetStrongRef(weakSource);
		if (source) {
			const float percent =
				(float)seek / float(slider->maximum());
			const int64_t seekTo =
				(float)percent *
				(float)obs_source_media_get_duration(source);
			obs_source_media_set_time(source, seekTo);
		}
		lastSeek = seek;
	}
}

QString MediaControl::FormatSeconds(float totalSeconds)
{
	if (totalSeconds < 0.0f)
		totalSeconds = 0.0f;
	int totalWholeSeconds = (int)totalSeconds;
	if (totalWholeSeconds < 0)
		totalWholeSeconds = 0;
	int wholeSeconds = (int)totalWholeSeconds % 60;
	float seconds =
		(float)wholeSeconds + (totalSeconds - (float)totalWholeSeconds);
	int totalMinutes = totalWholeSeconds / 60;
	int minutes = totalMinutes % 60;
	int hours = totalMinutes / 60;

	QString text;
	if (hours > 0) {
		text.sprintf("%02d:%02d:%02d", hours, minutes, wholeSeconds);
	} else if (showTimeDecimals) {
		text.sprintf("%02d:%05.2f", minutes, seconds);
	} else {
		text.sprintf("%02d:%02d", minutes, wholeSeconds);
	}
	return text;
}

void MediaControl::StartTimer()
{
	if (!timer->isActive())
		timer->start(showTimeDecimals ? 10 : 100);
}

void MediaControl::StopTimer()
{
	if (timer->isActive())
		timer->stop();
}

void MediaControl::SetPlayingState()
{
	slider->setEnabled(true);
	playPauseButton->setProperty("themeID", "pauseIcon");
	playPauseButton->style()->unpolish(playPauseButton);
	playPauseButton->style()->polish(playPauseButton);

	prevPaused = false;

	StartTimer();
}

void MediaControl::SetPausedState()
{
	playPauseButton->setProperty("themeID", "playIcon");
	playPauseButton->style()->unpolish(playPauseButton);
	playPauseButton->style()->polish(playPauseButton);
	StopTimer();
}

void MediaControl::SetRestartState()
{
	playPauseButton->setProperty("themeID", "playIcon");
	playPauseButton->style()->unpolish(playPauseButton);
	playPauseButton->style()->polish(playPauseButton);

	slider->setValue(0);
	timeLabel->setText(FormatSeconds(0));
	durationLabel->setText(FormatSeconds(0));
	slider->setEnabled(false);

	StopTimer();
}

void MediaControl::RefreshControls()
{
	OBSSource source = OBSGetStrongRef(weakSource);
	if (!source) {
		SetRestartState();
		setEnabled(false);
		return;
	} else {
		setEnabled(true);
	}

	/*const char *id = obs_source_get_unversioned_id(source);

	if (id && *id && strcmp(id, "ffmpeg_source") == 0) {
		nextButton->hide();
		previousButton->hide();
	} else {
		nextButton->show();
		previousButton->show();
	}*/

	obs_media_state state = obs_source_media_get_state(source);

	switch (state) {
	case OBS_MEDIA_STATE_STOPPED:
	case OBS_MEDIA_STATE_ENDED:
		SetRestartState();
		break;
	case OBS_MEDIA_STATE_PLAYING:
		SetPlayingState();
		break;
	case OBS_MEDIA_STATE_PAUSED:
		SetPausedState();
		break;
	default:
		break;
	}
	SetSliderPosition();
}

void MediaControl::SetSliderPosition()
{
	OBSSource source = OBSGetStrongRef(weakSource);
	float time = (float)obs_source_media_get_time(source) / 1000.0f;
	float duration = (float)obs_source_media_get_duration(source) / 1000.0f;

	float sliderPosition =
		duration == 0.0f ? 0.0f
				 : (time / duration) * (float)slider->maximum();

	slider->setValue((int)sliderPosition);

	if (showTimeRemaining) {
		timeLabel->setText(FormatSeconds(duration));
		durationLabel->setText(FormatSeconds(duration - time));
	} else {
		timeLabel->setText(FormatSeconds(time));
		durationLabel->setText(FormatSeconds(duration));
	}
}

OBSWeakSource MediaControl::GetSource()
{
	return weakSource;
}
void MediaControl::on_restartButton_clicked()
{
	OBSSource source = OBSGetStrongRef(weakSource);
	if (source) {
		obs_source_media_restart(source);
	}
}

void MediaControl::on_playPauseButton_clicked()
{
	OBSSource source = OBSGetStrongRef(weakSource);
	obs_media_state state = obs_source_media_get_state(source);

	switch (state) {
	case OBS_MEDIA_STATE_STOPPED:
	case OBS_MEDIA_STATE_ENDED:
		obs_source_media_restart(source);
		break;
	case OBS_MEDIA_STATE_PLAYING:
		obs_source_media_play_pause(source, true);
		break;
	case OBS_MEDIA_STATE_PAUSED:
		obs_source_media_play_pause(source, false);
		break;
	default:
		break;
	}
}

void MediaControl::on_stopButton_clicked()
{
	OBSSource source = OBSGetStrongRef(weakSource);
	if (source) {
		obs_source_media_stop(source);
	}
}

void MediaControl::on_nextButton_clicked()
{
	OBSSource source = OBSGetStrongRef(weakSource);
	if (source) {
		obs_source_media_next(source);
	}
}

void MediaControl::on_previousButton_clicked()
{
	OBSSource source = OBSGetStrongRef(weakSource);
	if (source) {
		obs_source_media_previous(source);
	}
}
