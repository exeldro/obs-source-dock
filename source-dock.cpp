#include "source-dock.hpp"
#include <obs-module.h>
#include <QGuiApplication>
#include <QLabel>
#include <QMainWindow>
#include <QMenu>
#include <QPushButton>
#include <QWidgetAction>
#include <QWindow>
#include <QScreen>
#include <QVBoxLayout>

#include "media-control.hpp"
#include "source-dock-settings.hpp"
#include "version.h"

#define QT_UTF8(str) QString::fromUtf8(str)
#define QT_TO_UTF8(str) str.toUtf8().constData()

OBS_DECLARE_MODULE()
OBS_MODULE_AUTHOR("Exeldro");
OBS_MODULE_USE_DEFAULT_LOCALE("source-dock", "en-US")

/*
 * preview
 * volmeter
 * volslider
 * media controls
 */

static void frontend_save_load(obs_data_t *save_data, bool saving, void *)
{
	if (saving) {
		obs_data_t *obj = obs_data_create();
		obs_data_array_t *docks = obs_data_array_create();
		for (const auto &it : source_docks) {
			obs_data_t *dock = obs_data_create();
			obs_data_set_string(
				dock, "source_name",
				obs_source_get_name(it->GetSource()));
			obs_data_set_string(dock, "title",
					    QT_TO_UTF8(it->windowTitle()));
			obs_data_set_bool(dock, "hidden", it->isHidden());
			obs_data_set_bool(dock, "preview",
					  it->PreviewEnabled());
			obs_data_set_bool(dock, "volmeter",
					  it->VolMeterEnabled());
			obs_data_set_bool(dock, "volcontrols",
					  it->VolControlsEnabled());
			obs_data_set_bool(dock, "mediacontrols",
					  it->MediaControlsEnabled());
			obs_data_set_bool(dock, "switchscene",
					  it->SwitchSceneEnabled());
			obs_data_set_bool(dock, "showactive",
					  it->ShowActiveEnabled());
			obs_data_set_bool(dock, "sceneitems",
					  it->SceneItemsEnabled());
			obs_data_set_bool(dock, "properties",
					  it->PropertiesEnabled());
			obs_data_set_bool(dock, "filters",
					  it->FiltersEnabled());
			obs_data_array_push_back(docks, dock);
			obs_data_release(dock);
		}
		obs_data_set_array(obj, "docks", docks);
		obs_data_set_obj(save_data, "source-dock", obj);
		obs_data_array_release(docks);
		obs_data_release(obj);
	} else {
		for (const auto &it : source_docks) {
			it->close();
			it->deleteLater();
		}
		source_docks.clear();

		obs_data_t *obj = obs_data_get_obj(save_data, "source-dock");
		if (obj) {
			obs_data_array_t *docks =
				obs_data_get_array(obj, "docks");
			if (docks) {
				const auto main_window =
					static_cast<QMainWindow *>(
						obs_frontend_get_main_window());
				obs_frontend_push_ui_translation(
					obs_module_get_string);
				size_t count = obs_data_array_count(docks);
				for (size_t i = 0; i < count; i++) {
					obs_data_t *dock =
						obs_data_array_item(docks, i);
					auto *s = obs_get_source_by_name(
						obs_data_get_string(
							dock, "source_name"));
					if (s) {
						auto *tmp = new SourceDock(
							s, main_window);
						auto title =
							obs_data_get_string(
								dock, "title");
						if (!title || !strlen(title)) {
							title = obs_source_get_name(
								s);
							tmp->EnablePreview();
							tmp->EnableVolMeter();
							tmp->EnableVolControls();
							tmp->EnableMediaControls();
							tmp->EnableSwitchScene();
							tmp->EnableShowActive();
						}
						tmp->setWindowTitle(
							QT_UTF8(title));
						tmp->setObjectName(
							QT_UTF8(title));

						source_docks.push_back(tmp);
						if (obs_data_get_bool(
							    dock, "preview"))
							tmp->EnablePreview();

						if (obs_data_get_bool(
							    dock, "volmeter"))
							tmp->EnableVolMeter();
						if (obs_data_get_bool(
							    dock,
							    "volcontrols"))
							tmp->EnableVolControls();
						if (obs_data_get_bool(
							    dock,
							    "mediacontrols"))
							tmp->EnableMediaControls();
						if (obs_data_get_bool(
							    dock,
							    "switchscene"))
							tmp->EnableSwitchScene();
						if (obs_data_get_bool(
							    dock, "showactive"))
							tmp->EnableShowActive();
						if (obs_data_get_bool(
							    dock, "properties"))
							tmp->EnableProperties();
						if (obs_data_get_bool(
							    dock, "filters"))
							tmp->EnableFilters();
						if (obs_data_get_bool(
							    dock, "sceneitems"))
							tmp->EnableSceneItems();
						if (obs_data_get_bool(dock,
								      "hidden"))
							tmp->hide();
						else
							tmp->show();
						auto *a = static_cast<QAction *>(
							obs_frontend_add_dock(
								tmp));
						tmp->setAction(a);
						obs_source_release(s);
					}
					obs_data_release(dock);
				}
				obs_frontend_pop_ui_translation();
				obs_data_array_release(docks);
			}
			obs_data_release(obj);
		}
	}
}

static void frontend_event(enum obs_frontend_event event, void *)
{
	if (event == OBS_FRONTEND_EVENT_SCENE_COLLECTION_CLEANUP ||
	    event == OBS_FRONTEND_EVENT_EXIT) {
		for (const auto &it : source_docks) {
			it->close();
			delete (it);
		}
		source_docks.clear();
	}
}

static void source_remove(void *data, calldata_t *call_data)
{
	obs_source_t *source =
		static_cast<obs_source_t *>(calldata_ptr(call_data, "source"));
	for (auto it = source_docks.begin(); it != source_docks.end();) {
		if ((*it)->GetSource().Get() == source) {
			(*it)->close();
			(*it)->deleteLater();
			it = source_docks.erase(it);
		} else {
			++it;
		}
	}
}

bool obs_module_load()
{
	blog(LOG_INFO, "[Source Dock] loaded version %s", PROJECT_VERSION);

	obs_frontend_add_save_callback(frontend_save_load, nullptr);
	obs_frontend_add_event_callback(frontend_event, nullptr);
	signal_handler_connect(obs_get_signal_handler(), "source_remove",
			       source_remove, nullptr);

	QAction *action =
		static_cast<QAction *>(obs_frontend_add_tools_menu_qaction(
			obs_module_text("SourceDock")));

	auto cb = [] {
		obs_frontend_push_ui_translation(obs_module_get_string);

		SourceDockSettingsDialog *sdsd =
			new SourceDockSettingsDialog(static_cast<QMainWindow *>(
				obs_frontend_get_main_window()));
		sdsd->show();
		sdsd->setAttribute(Qt::WA_DeleteOnClose, true);
		obs_frontend_pop_ui_translation();
	};

	QAction::connect(action, &QAction::triggered, cb);
	return true;
}

void obs_module_unload()
{
	obs_frontend_remove_save_callback(frontend_save_load, nullptr);
	obs_frontend_remove_event_callback(frontend_event, nullptr);
	signal_handler_disconnect(obs_get_signal_handler(), "source_remove",
				  source_remove, nullptr);
}

MODULE_EXPORT const char *obs_module_description(void)
{
	return obs_module_text("Description");
}

MODULE_EXPORT const char *obs_module_name(void)
{
	return obs_module_text("SourceDock");
}

#define LOG_OFFSET_DB 6.0f
#define LOG_RANGE_DB 96.0f
/* equals -log10f(LOG_OFFSET_DB) */
#define LOG_OFFSET_VAL -0.77815125038364363f
/* equals -log10f(-LOG_RANGE_DB + LOG_OFFSET_DB) */
#define LOG_RANGE_VAL -2.00860017176191756f

SourceDock::SourceDock(OBSSource source_, QWidget *parent)
	: QDockWidget(parent),
	  source(source_),
	  eventFilter(BuildEventFilter()),
	  preview(nullptr),
	  volMeter(nullptr),
	  obs_volmeter(nullptr),
	  volControl(nullptr),
	  mediaControl(nullptr),
	  switch_scene_enabled(false),
	  activeLabel(nullptr),
	  sceneItems(nullptr),
	  propertiesButton(nullptr),
	  filtersButton(nullptr),
	  action(nullptr)
{
	setFeatures(AllDockWidgetFeatures);
	setWindowTitle(QT_UTF8(obs_source_get_name(source)));
	setObjectName(QT_UTF8(obs_source_get_name(source)));
	setFloating(true);
	hide();

	mainLayout = new QVBoxLayout(this);

	auto *dockWidgetContents = new QWidget;
	dockWidgetContents->setObjectName(QStringLiteral("contextContainer"));
	dockWidgetContents->setLayout(mainLayout);

	setWidget(dockWidgetContents);
}

SourceDock::~SourceDock()
{
	delete action;
	DisableFilters();
	DisableProperties();
	DisableSceneItems();
	DisableShowActive();
	DisableVolMeter();
	DisableVolControls();
	DisableMediaControls();
	DisablePreview();
}

static inline void GetScaleAndCenterPos(int baseCX, int baseCY, int windowCX,
					int windowCY, int &x, int &y,
					float &scale)
{
	int newCX, newCY;

	const double windowAspect = double(windowCX) / double(windowCY);
	const double baseAspect = double(baseCX) / double(baseCY);

	if (windowAspect > baseAspect) {
		scale = float(windowCY) / float(baseCY);
		newCX = int(double(windowCY) * baseAspect);
		newCY = windowCY;
	} else {
		scale = float(windowCX) / float(baseCX);
		newCX = windowCX;
		newCY = int(float(windowCX) / baseAspect);
	}

	x = windowCX / 2 - newCX / 2;
	y = windowCY / 2 - newCY / 2;
}

void SourceDock::DrawPreview(void *data, uint32_t cx, uint32_t cy)
{
	SourceDock *window = static_cast<SourceDock *>(data);

	if (!window->source)
		return;

	uint32_t sourceCX = obs_source_get_width(window->source);
	if (sourceCX <= 0)
		sourceCX = 1;
	uint32_t sourceCY = obs_source_get_height(window->source);
	if (sourceCY <= 0)
		sourceCY = 1;

	int x, y;
	float scale;

	GetScaleAndCenterPos(sourceCX, sourceCY, cx, cy, x, y, scale);

	int newCX = int(scale * float(sourceCX));
	int newCY = int(scale * float(sourceCY));

	gs_viewport_push();
	gs_projection_push();
	const bool previous = gs_set_linear_srgb(true);

	gs_ortho(0.0f, float(sourceCX), 0.0f, float(sourceCY), -100.0f, 100.0f);
	gs_set_viewport(x, y, newCX, newCY);
	obs_source_video_render(window->source);

	gs_set_linear_srgb(previous);
	gs_projection_pop();
	gs_viewport_pop();
}

void SourceDock::OBSVolumeLevel(void *data,
				const float magnitude[MAX_AUDIO_CHANNELS],
				const float peak[MAX_AUDIO_CHANNELS],
				const float inputPeak[MAX_AUDIO_CHANNELS])
{
	SourceDock *sourceDock = static_cast<SourceDock *>(data);
	sourceDock->volMeter->setLevels(magnitude, peak, inputPeak);
}

void SourceDock::OBSVolume(void *data, calldata_t *call_data)
{
	obs_source_t *source;
	calldata_get_ptr(call_data, "source", &source);
	double volume;
	calldata_get_float(call_data, "volume", &volume);
	SourceDock *sourceDock = static_cast<SourceDock *>(data);
	QMetaObject::invokeMethod(sourceDock, "SetOutputVolume",
				  Qt::QueuedConnection, Q_ARG(double, volume));
}

void SourceDock::OBSMute(void *data, calldata_t *call_data)
{
	obs_source_t *source;
	calldata_get_ptr(call_data, "source", &source);
	bool muted = calldata_bool(call_data, "muted");
	SourceDock *sourceDock = static_cast<SourceDock *>(data);
	QMetaObject::invokeMethod(sourceDock, "SetMute", Qt::QueuedConnection,
				  Q_ARG(bool, muted));
}

void SourceDock::OBSActiveChanged(void *data, calldata_t *call_data)
{
	SourceDock *sourceDock = static_cast<SourceDock *>(data);
	QMetaObject::invokeMethod(sourceDock, "ActiveChanged",
				  Qt::QueuedConnection);
}

void SourceDock::LockVolumeControl(bool lock)
{
	slider->setEnabled(!lock);
	mute->setEnabled(!lock);
}

void SourceDock::MuteVolumeControl(bool mute)
{
	if (obs_source_muted(source) != mute)
		obs_source_set_muted(source, mute);
}

void SourceDock::SetOutputVolume(double volume)
{
	float db = obs_mul_to_db(volume);
	float def;
	if (db >= 0.0f)
		def = 1.0f;
	else if (db <= -96.0f)
		def = 0.0f;
	else
		def = (-log10f(-db + LOG_OFFSET_DB) - LOG_RANGE_VAL) /
		      (LOG_OFFSET_VAL - LOG_RANGE_VAL);

	int val = def * 10000.0;
	slider->setValue(val);
}

void SourceDock::SetMute(bool muted)
{
	mute->setChecked(muted);
}

void SourceDock::ActiveChanged()
{
	if (!activeLabel)
		return;
	bool active = obs_source_active(source);
	activeLabel->setText(
		QT_UTF8(obs_module_text(active ? "Active" : "NotActive")));
	activeLabel->setProperty("themeID", active ? "good" : "");
	/* force style sheet recalculation */
	QString qss = activeLabel->styleSheet();
	activeLabel->setStyleSheet("/* */");
	activeLabel->setStyleSheet(qss);
}

void SourceDock::SliderChanged(int vol)
{
	float def = (float)vol / 10000.0f;
	float db;
	if (def >= 1.0f)
		db = 0.0f;
	else if (def <= 0.0f)
		db = -INFINITY;
	else
		db = -(LOG_RANGE_DB + LOG_OFFSET_DB) *
			     powf((LOG_RANGE_DB + LOG_OFFSET_DB) /
					  LOG_OFFSET_DB,
				  -def) +
		     LOG_OFFSET_DB;
	const float mul = obs_db_to_mul(db);
	obs_source_set_volume(source, mul);
}

bool SourceDock::GetSourceRelativeXY(int mouseX, int mouseY, int &relX,
				     int &relY)
{
	float pixelRatio = devicePixelRatioF();

	int mouseXscaled = (int)roundf(mouseX * pixelRatio);
	int mouseYscaled = (int)roundf(mouseY * pixelRatio);

	QSize size = preview->size() * preview->devicePixelRatioF();

	uint32_t sourceCX = obs_source_get_width(source);
	if (sourceCX <= 0)
		sourceCX = 1;
	uint32_t sourceCY = obs_source_get_height(source);
	if (sourceCY <= 0)
		sourceCY = 1;

	int x, y;
	float scale;

	GetScaleAndCenterPos(sourceCX, sourceCY, size.width(), size.height(), x,
			     y, scale);

	if (x > 0) {
		relX = int(float(mouseXscaled - x) / scale);
		relY = int(float(mouseYscaled / scale));
	} else {
		relX = int(float(mouseXscaled / scale));
		relY = int(float(mouseYscaled - y) / scale);
	}

	// Confirm mouse is inside the source
	if (relX < 0 || relX > int(sourceCX))
		return false;
	if (relY < 0 || relY > int(sourceCY))
		return false;

	return true;
}

OBSEventFilter *SourceDock::BuildEventFilter()
{
	return new OBSEventFilter([this](QObject *obj, QEvent *event) {
		UNUSED_PARAMETER(obj);

		switch (event->type()) {
		case QEvent::MouseButtonPress:
		case QEvent::MouseButtonRelease:
		case QEvent::MouseButtonDblClick:
			return this->HandleMouseClickEvent(
				static_cast<QMouseEvent *>(event));
		case QEvent::MouseMove:
		case QEvent::Enter:
		case QEvent::Leave:
			return this->HandleMouseMoveEvent(
				static_cast<QMouseEvent *>(event));

		case QEvent::Wheel:
			return this->HandleMouseWheelEvent(
				static_cast<QWheelEvent *>(event));
		case QEvent::FocusIn:
		case QEvent::FocusOut:
			return this->HandleFocusEvent(
				static_cast<QFocusEvent *>(event));
		case QEvent::KeyPress:
		case QEvent::KeyRelease:
			return this->HandleKeyEvent(
				static_cast<QKeyEvent *>(event));
		default:
			return false;
		}
	});
}

static int TranslateQtKeyboardEventModifiers(QInputEvent *event,
					     bool mouseEvent)
{
	int obsModifiers = INTERACT_NONE;

	if (event->modifiers().testFlag(Qt::ShiftModifier))
		obsModifiers |= INTERACT_SHIFT_KEY;
	if (event->modifiers().testFlag(Qt::AltModifier))
		obsModifiers |= INTERACT_ALT_KEY;
#ifdef __APPLE__
	// Mac: Meta = Control, Control = Command
	if (event->modifiers().testFlag(Qt::ControlModifier))
		obsModifiers |= INTERACT_COMMAND_KEY;
	if (event->modifiers().testFlag(Qt::MetaModifier))
		obsModifiers |= INTERACT_CONTROL_KEY;
#else
	// Handle windows key? Can a browser even trap that key?
	if (event->modifiers().testFlag(Qt::ControlModifier))
		obsModifiers |= INTERACT_CONTROL_KEY;
#endif

	if (!mouseEvent) {
		if (event->modifiers().testFlag(Qt::KeypadModifier))
			obsModifiers |= INTERACT_IS_KEY_PAD;
	}

	return obsModifiers;
}

static int TranslateQtMouseEventModifiers(QMouseEvent *event)
{
	int modifiers = TranslateQtKeyboardEventModifiers(event, true);

	if (event->buttons().testFlag(Qt::LeftButton))
		modifiers |= INTERACT_MOUSE_LEFT;
	if (event->buttons().testFlag(Qt::MiddleButton))
		modifiers |= INTERACT_MOUSE_MIDDLE;
	if (event->buttons().testFlag(Qt::RightButton))
		modifiers |= INTERACT_MOUSE_RIGHT;

	return modifiers;
}

bool SourceDock::HandleMouseClickEvent(QMouseEvent *event)
{
	bool mouseUp = event->type() == QEvent::MouseButtonRelease;
	int clickCount = 1;
	if (event->type() == QEvent::MouseButtonDblClick)
		clickCount = 2;

	struct obs_mouse_event mouseEvent = {};

	mouseEvent.modifiers = TranslateQtMouseEventModifiers(event);

	int32_t button = 0;

	switch (event->button()) {
	case Qt::LeftButton:
		button = MOUSE_LEFT;
		break;
	case Qt::MiddleButton:
		button = MOUSE_MIDDLE;
		break;
	case Qt::RightButton:
		button = MOUSE_RIGHT;
		break;
	default:
		blog(LOG_WARNING, "unknown button type %d", event->button());
		return false;
	}

	// Why doesn't this work?
	//if (event->flags().testFlag(Qt::MouseEventCreatedDoubleClick))
	//	clickCount = 2;

	bool insideSource = GetSourceRelativeXY(event->x(), event->y(),
						mouseEvent.x, mouseEvent.y);

	if (mouseUp || insideSource)
		obs_source_send_mouse_click(source, &mouseEvent, button,
					    mouseUp, clickCount);

	if (mouseUp && switch_scene_enabled) {
		obs_frontend_set_current_scene(source);
	}

	return true;
}

bool SourceDock::HandleMouseMoveEvent(QMouseEvent *event)
{
	struct obs_mouse_event mouseEvent = {};

	bool mouseLeave = event->type() == QEvent::Leave;

	if (!mouseLeave) {
		mouseEvent.modifiers = TranslateQtMouseEventModifiers(event);
		mouseLeave = !GetSourceRelativeXY(event->x(), event->y(),
						  mouseEvent.x, mouseEvent.y);
	}

	obs_source_send_mouse_move(source, &mouseEvent, mouseLeave);

	return true;
}

bool SourceDock::HandleMouseWheelEvent(QWheelEvent *event)
{
	struct obs_mouse_event mouseEvent = {};

	mouseEvent.modifiers = TranslateQtKeyboardEventModifiers(event, true);

	int xDelta = 0;
	int yDelta = 0;

	const QPoint angleDelta = event->angleDelta();
	if (!event->pixelDelta().isNull()) {
		if (angleDelta.x())
			xDelta = event->pixelDelta().x();
		else
			yDelta = event->pixelDelta().y();
	} else {
		if (angleDelta.x())
			xDelta = angleDelta.x();
		else
			yDelta = angleDelta.y();
	}

#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
	const QPointF position = event->position();
	const int x = position.x();
	const int y = position.y();
#else
	const int x = event->x();
	const int y = event->y();
#endif

	if (GetSourceRelativeXY(x, y, mouseEvent.x, mouseEvent.y)) {
		obs_source_send_mouse_wheel(source, &mouseEvent, xDelta,
					    yDelta);
	}

	return true;
}

bool SourceDock::HandleFocusEvent(QFocusEvent *event)
{
	bool focus = event->type() == QEvent::FocusIn;

	obs_source_send_focus(source, focus);

	return true;
}

bool SourceDock::HandleKeyEvent(QKeyEvent *event)
{
	struct obs_key_event keyEvent;

	QByteArray text = event->text().toUtf8();
	keyEvent.modifiers = TranslateQtKeyboardEventModifiers(event, false);
	keyEvent.text = text.data();
	keyEvent.native_modifiers = event->nativeModifiers();
	keyEvent.native_scancode = event->nativeScanCode();
	keyEvent.native_vkey = event->nativeVirtualKey();

	bool keyUp = event->type() == QEvent::KeyRelease;

	obs_source_send_key_click(source, &keyEvent, keyUp);

	return true;
}

void SourceDock::EnablePreview()
{
	if (preview != nullptr)
		return;
	uint32_t caps = obs_source_get_output_flags(source);
	if ((caps & OBS_SOURCE_VIDEO) == 0)
		return;
	preview = new OBSQTDisplay(this);
	preview->setObjectName(QStringLiteral("preview"));
	QSizePolicy sizePolicy1(QSizePolicy::Expanding, QSizePolicy::Expanding);
	sizePolicy1.setHorizontalStretch(0);
	sizePolicy1.setVerticalStretch(0);
	sizePolicy1.setHeightForWidth(
		preview->sizePolicy().hasHeightForWidth());
	preview->setSizePolicy(sizePolicy1);
	preview->setMinimumSize(QSize(24, 24));

	preview->setMouseTracking(true);
	preview->setFocusPolicy(Qt::StrongFocus);
	preview->installEventFilter(eventFilter.get());

	auto addDrawCallback = [this]() {
		obs_display_add_draw_callback(preview->GetDisplay(),
					      DrawPreview, this);
	};
	preview->show();
	connect(preview, &OBSQTDisplay::DisplayCreated, addDrawCallback);

	mainLayout->addWidget(preview);
	obs_source_inc_showing(source);
}

void SourceDock::DisablePreview()
{
	if (!preview)
		return;
	obs_display_remove_draw_callback(preview->GetDisplay(), DrawPreview,
					 this);
	mainLayout->removeWidget(preview);
	preview->deleteLater();
	preview = nullptr;
	obs_source_dec_showing(source);
}

bool SourceDock::PreviewEnabled()
{
	return preview != nullptr;
}

void SourceDock::EnableVolMeter()
{
	if (obs_volmeter != nullptr)
		return;

	uint32_t caps = obs_source_get_output_flags(source);
	if ((caps & OBS_SOURCE_AUDIO) == 0)
		return;

	obs_volmeter = obs_volmeter_create(OBS_FADER_LOG);
	obs_volmeter_attach_source(obs_volmeter, source);

	volMeter = new VolumeMeter(nullptr, obs_volmeter);

	obs_volmeter_add_callback(obs_volmeter, OBSVolumeLevel, this);

	mainLayout->addWidget(volMeter);
}
void SourceDock::DisableVolMeter()
{
	if (!obs_volmeter)
		return;
	obs_volmeter_remove_callback(obs_volmeter, OBSVolumeLevel, this);
	obs_volmeter_destroy(obs_volmeter);
	obs_volmeter = nullptr;

	volMeter->deleteLater();
	volMeter = nullptr;
}

bool SourceDock::VolMeterEnabled()
{
	return obs_volmeter != nullptr;
}

void SourceDock::EnableVolControls()
{
	if (volControl != nullptr)
		return;
	uint32_t caps = obs_source_get_output_flags(source);

	if ((caps & OBS_SOURCE_AUDIO) == 0)
		return;

	volControl = new QWidget;
	auto *audioLayout = new QHBoxLayout(this);

	obs_data_t *settings = obs_source_get_private_settings(source);
	const bool lock = obs_data_get_bool(settings, "volume_locked");
	obs_data_release(settings);

	locked = new LockedCheckBox();
	locked->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
	locked->setFixedSize(16, 16);
	locked->setChecked(lock);
	locked->setStyleSheet("background: none");

	connect(locked, &QCheckBox::stateChanged, this,
		&SourceDock::LockVolumeControl, Qt::DirectConnection);

	slider = new SliderIgnoreScroll(Qt::Horizontal);
	slider->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
	slider->setEnabled(!lock);
	slider->setMinimum(0);
	slider->setMaximum(10000);
	slider->setToolTip(QT_UTF8(obs_module_text("VolumeOutput")));
	float mul = obs_source_get_volume(source);
	float db = obs_mul_to_db(mul);
	float def;
	if (db >= 0.0f)
		def = 1.0f;
	else if (db <= -96.0f)
		def = 0.0f;
	else
		def = (-log10f(-db + LOG_OFFSET_DB) - LOG_RANGE_VAL) /
		      (LOG_OFFSET_VAL - LOG_RANGE_VAL);
	slider->setValue(def * 10000.0f);

	connect(slider, SIGNAL(valueChanged(int)), this,
		SLOT(SliderChanged(int)));

	mute = new MuteCheckBox();
	mute->setEnabled(!lock);
	mute->setChecked(obs_source_muted(source));

	connect(mute, &QCheckBox::stateChanged, this,
		&SourceDock::MuteVolumeControl, Qt::DirectConnection);

	signal_handler_connect(obs_source_get_signal_handler(source), "mute",
			       OBSMute, this);
	signal_handler_connect(obs_source_get_signal_handler(source), "volume",
			       OBSVolume, this);

	audioLayout->addWidget(locked);
	audioLayout->addWidget(slider);
	audioLayout->addWidget(mute);

	volControl->setLayout(audioLayout);
	mainLayout->addWidget(volControl);
}

void SourceDock::DisableVolControls()
{
	if (!volControl)
		return;
	signal_handler_disconnect(obs_source_get_signal_handler(source), "mute",
				  OBSMute, this);
	signal_handler_disconnect(obs_source_get_signal_handler(source),
				  "volume", OBSVolume, this);
	mainLayout->removeWidget(volControl);
	volControl->deleteLater();
	volControl = nullptr;
}
bool SourceDock::VolControlsEnabled()
{
	return volControl != nullptr;
}

void SourceDock::EnableMediaControls()
{
	if (mediaControl != nullptr)
		return;
	uint32_t caps = obs_source_get_output_flags(source);
	if ((caps & OBS_SOURCE_CONTROLLABLE_MEDIA) == 0)
		return;
	mediaControl = new MediaControl(OBSGetWeakRef(source), true, true);
	mainLayout->addWidget(mediaControl);
}

void SourceDock::DisableMediaControls()
{
	if (!mediaControl)
		return;

	mainLayout->removeWidget(mediaControl);
	mediaControl->deleteLater();
	mediaControl = nullptr;
}

bool SourceDock::MediaControlsEnabled()
{
	return mediaControl != nullptr;
}

void SourceDock::EnableSwitchScene()
{
	if (!obs_source_is_scene(source))
		return;
	switch_scene_enabled = true;
}

void SourceDock::DisableSwitchScene()
{
	switch_scene_enabled = false;
}

bool SourceDock::SwitchSceneEnabled()
{
	return switch_scene_enabled;
}

void SourceDock::EnableShowActive()
{
	if (activeLabel)
		return;

	activeLabel = new QLabel;
	activeLabel->setAlignment(Qt::AlignCenter);
	ActiveChanged();
	mainLayout->addWidget(activeLabel);
	signal_handler_t *sh = obs_source_get_signal_handler(source);
	if (sh) {
		signal_handler_connect(sh, "activate", OBSActiveChanged, this);
		signal_handler_connect(sh, "deactivate", OBSActiveChanged,
				       this);
	}
}

void SourceDock::DisableShowActive()
{
	signal_handler_t *sh = obs_source_get_signal_handler(source);
	if (sh) {
		signal_handler_disconnect(sh, "activate", OBSActiveChanged,
					  this);
		signal_handler_disconnect(sh, "deactivate", OBSActiveChanged,
					  this);
	}

	mainLayout->removeWidget(activeLabel);
	activeLabel->deleteLater();
	activeLabel = nullptr;
}
bool SourceDock::ShowActiveEnabled()
{
	return activeLabel != nullptr;
}

void SourceDock::EnableSceneItems()
{
	if (sceneItems)
		return;
	obs_scene_t *scene = obs_scene_from_source(source);
	if (!scene)
		return;

	auto layout = new QGridLayout;

	sceneItems = new QWidget;
	sceneItems->setObjectName(QStringLiteral("contextContainer"));
	sceneItems->setLayout(layout);

	obs_scene_enum_items(scene, AddSceneItem, layout);

	mainLayout->addWidget(sceneItems);

	auto itemVisible = [](void *data, calldata_t *cd) {
		const auto dock = static_cast<SourceDock *>(data);
		const auto curItem = static_cast<obs_sceneitem_t *>(
			calldata_ptr(cd, "item"));

		const int id = (int)obs_sceneitem_get_id(curItem);
		QMetaObject::invokeMethod(dock, "VisibilityChanged",
					  Qt::QueuedConnection, Q_ARG(int, id));
	};

	auto refreshItems = [](void *data, calldata_t *cd) {
		const auto dock = static_cast<SourceDock *>(data);
		QMetaObject::invokeMethod(dock, "RefreshItems",
					  Qt::QueuedConnection);
	};

	signal_handler_t *signal = obs_source_get_signal_handler(source);

	addSignal.Connect(signal, "item_add", refreshItems, this);
	removeSignal.Connect(signal, "item_remove", refreshItems, this);
	reorderSignal.Connect(signal, "reorder", refreshItems, this);
	refreshSignal.Connect(signal, "refresh", refreshItems, this);
	visibleSignal.Connect(signal, "item_visible", itemVisible, this);
}

void SourceDock::VisibilityChanged(int id)
{
	auto layout = dynamic_cast<QGridLayout *>(sceneItems->layout());
	auto count = layout->rowCount();

	for (int i = 0; i < count; i++) {
		QLayoutItem *item = layout->itemAtPosition(i, 3);
		if (!item)
			continue;
		QWidget *w = item->widget();
		if (!w)
			continue;
		if (id != w->property("id").toInt())
			continue;
		const auto scene = obs_scene_from_source(source);
		obs_sceneitem_t *sceneitem =
			obs_scene_find_sceneitem_by_id(scene, id);

		auto checkBox = dynamic_cast<QCheckBox *>(w);
		checkBox->setChecked(obs_sceneitem_visible(sceneitem));
	}
}

void SourceDock::RefreshItems()
{
	DisableSceneItems();
	EnableSceneItems();
}

bool SourceDock::AddSceneItem(obs_scene_t *scene, obs_sceneitem_t *item,
			      void *data)
{
	QGridLayout *layout = static_cast<QGridLayout *>(data);

	auto source = obs_sceneitem_get_source(item);
	int row = layout->rowCount();
	auto label = new QLabel(QT_UTF8(obs_source_get_name(source)));
	layout->addWidget(label, row, 0);

	auto prop = new QPushButton();
	prop->setObjectName(QStringLiteral("sourcePropertiesButton"));
	prop->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
	prop->setFixedSize(16, 16);
	prop->setFlat(true);
	layout->addWidget(prop, row, 1);

	auto openProps = [source]() {
		obs_frontend_open_source_properties(source);
	};

	connect(prop, &QAbstractButton::clicked, openProps);

	auto filters = new QPushButton();
	filters->setObjectName(QStringLiteral("sourceFiltersButton"));
	filters->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
	filters->setFixedSize(16, 16);
	filters->setFlat(true);
	layout->addWidget(filters, row, 2);

	auto openFilters = [source]() {
		obs_frontend_open_source_filters(source);
	};

	connect(filters, &QAbstractButton::clicked, openFilters);

	auto vis = new VisibilityCheckBox();
	vis->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
	vis->setFixedSize(16, 16);
	vis->setChecked(obs_sceneitem_visible(item));
	vis->setStyleSheet("background: none");
	vis->setProperty("id", (int)obs_sceneitem_get_id(item));
	layout->addWidget(vis, row, 3);

	auto setItemVisible = [item](bool val) {
		obs_sceneitem_set_visible(item, val);
	};

	connect(vis, &QAbstractButton::clicked, setItemVisible);
	return true;
}
void SourceDock::DisableSceneItems()
{
	if (!sceneItems)
		return;

	mainLayout->removeWidget(sceneItems);
	sceneItems->deleteLater();
	sceneItems = nullptr;
	visibleSignal.Disconnect();
	addSignal.Disconnect();
	removeSignal.Disconnect();
	reorderSignal.Disconnect();
	refreshSignal.Disconnect();
}
bool SourceDock::SceneItemsEnabled()
{
	return sceneItems != nullptr;
}

void SourceDock::EnableProperties()
{
	if (propertiesButton)
		return;

	propertiesButton = new QPushButton;
	propertiesButton->setObjectName(
		QStringLiteral("sourcePropertiesButton"));
	propertiesButton->setText(QT_UTF8(obs_module_text("Properties")));
	mainLayout->addWidget(propertiesButton);
	auto openProps = [this]() {
		obs_frontend_open_source_properties(source);
	};
	connect(propertiesButton, &QAbstractButton::clicked, openProps);
}

void SourceDock::DisableProperties()
{
	mainLayout->removeWidget(propertiesButton);
	propertiesButton->deleteLater();
	propertiesButton = nullptr;
}

bool SourceDock::PropertiesEnabled()
{
	return propertiesButton != nullptr;
}

void SourceDock::EnableFilters()
{
	if (filtersButton)
		return;

	filtersButton = new QPushButton;
	filtersButton->setObjectName(QStringLiteral("sourceFiltersButton"));
	filtersButton->setText(QT_UTF8(obs_module_text("Filters")));
	mainLayout->addWidget(filtersButton);
	auto openProps = [this]() { obs_frontend_open_source_filters(source); };
	connect(filtersButton, &QAbstractButton::clicked, openProps);
}

void SourceDock::DisableFilters()
{
	mainLayout->removeWidget(filtersButton);
	filtersButton->deleteLater();
	filtersButton = nullptr;
}

bool SourceDock::FiltersEnabled()
{
	return filtersButton != nullptr;
}

OBSSource SourceDock::GetSource()
{
	return source;
}

void SourceDock::setAction(QAction *a)
{
	action = a;
}

LockedCheckBox::LockedCheckBox() {}

LockedCheckBox::LockedCheckBox(QWidget *parent) : QCheckBox(parent) {}

VisibilityCheckBox::VisibilityCheckBox() {}

VisibilityCheckBox::VisibilityCheckBox(QWidget *parent) : QCheckBox(parent) {}

SliderIgnoreScroll::SliderIgnoreScroll(QWidget *parent) : QSlider(parent)
{
	setFocusPolicy(Qt::StrongFocus);
}

SliderIgnoreScroll::SliderIgnoreScroll(Qt::Orientation orientation,
				       QWidget *parent)
	: QSlider(parent)
{
	setFocusPolicy(Qt::StrongFocus);
	setOrientation(orientation);
}

void SliderIgnoreScroll::wheelEvent(QWheelEvent *event)
{
	if (!hasFocus())
		event->ignore();
	else
		QSlider::wheelEvent(event);
}
