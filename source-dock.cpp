#include "source-dock.hpp"
#include <obs-module.h>
#include <QGuiApplication>
#include <QLabel>
#include <QMainWindow>
#include <QMenu>
#include <QWidgetAction>
#include <QWindow>
#include <QScreen>
#include <QVBoxLayout>

#include "media-control.hpp"
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

std::map<std::string, SourceDock *> source_docks;

static bool AddSourceMenu(void *data, obs_source_t *source)
{
	QMenu *menu = static_cast<QMenu *>(data);
	auto *a = menu->addAction(QT_UTF8(obs_source_get_name(source)));
	a->setCheckable(true);
	auto it = source_docks.find(obs_source_get_name(source));
	bool exists = it != source_docks.end();
	a->setChecked(exists);
	QObject::connect(a, &QAction::triggered, [source] {
		auto it = source_docks.find(obs_source_get_name(source));
		if (it != source_docks.end()) {
			it->second->close();
			it->second->deleteLater();
			source_docks.erase(it);
		}else {
			const auto main_window = static_cast<QMainWindow *>(
				obs_frontend_get_main_window());
			auto *tmp = new SourceDock(source, main_window);
			source_docks[obs_source_get_name(source)] = tmp;
			tmp->show();
		}
	});
	return true;
}

static void LoadMenu(QMenu *menu)
{
	menu->clear();
	struct obs_frontend_source_list scenes = {};
	obs_frontend_source_list_free(&scenes);
	obs_enum_scenes(AddSourceMenu, menu);
	obs_enum_sources(AddSourceMenu, menu);
}

static void frontend_save_load(obs_data_t *save_data, bool saving, void *)
{
	if (saving) {
		obs_data_t *obj = obs_data_create();
		obs_data_array_t *docks = obs_data_array_create();
		for (const auto &it : source_docks) {
			obs_data_t *dock = obs_data_create();
			obs_data_set_string(dock, "source_name",
					    it.first.c_str());
			obs_data_array_push_back(docks, dock);
		}
		obs_data_set_array(obj, "docks", docks);
		obs_data_set_obj(save_data, "source-dock", obj);
		obs_data_array_release(docks);
		obs_data_release(obj);
	} else {
		for (const auto &it : source_docks) {
			it.second->close();
			it.second->deleteLater();
		}
		source_docks.clear();

		obs_data_t *obj =
			obs_data_get_obj(save_data, "source-dock");
		if (obj) {
			obs_data_array_t *docks =
				obs_data_get_array(obj, "docks");
			if (docks) {
				const auto main_window =
					static_cast<QMainWindow *>(
						obs_frontend_get_main_window());
				obs_frontend_push_ui_translation(
					obs_module_get_string);
				size_t count =
					obs_data_array_count(docks);
				for (size_t i = 0; i < count; i++) {
					obs_data_t *dock =
						obs_data_array_item(docks,
								    i);
					auto *s = obs_get_source_by_name(obs_data_get_string(dock, "source_name"));
					if (s) {
						auto *tmp = new SourceDock(
							s, main_window);
						source_docks[obs_source_get_name(
							s)] = tmp;
						tmp->show();
					}
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
			it.second->close();
			it.second->deleteLater();
		}
		source_docks.clear();
	}
}

static void source_rename(void *data, calldata_t *call_data)
{
	std::string new_name = calldata_string(call_data, "new_name");
	std::string prev_name = calldata_string(call_data, "prev_name");
	auto it = source_docks.find(prev_name);
	if (it != source_docks.end()) {
		source_docks[new_name] = it->second;
		it->second->Rename(new_name);
		source_docks.erase(it);
	}
}

static void source_remove(void *data, calldata_t *call_data)
{
	std::string name = obs_source_get_name(
		static_cast<obs_source_t *>(calldata_ptr(call_data, "source")));
	auto it = source_docks.find(name);
	if (it != source_docks.end()) {
		it->second->close();
		it->second->deleteLater();
		source_docks.erase(it);
	}
}

bool obs_module_load()
{
	blog(LOG_INFO, "[Source Dock] loaded version %s", PROJECT_VERSION);

	obs_frontend_add_save_callback(frontend_save_load, nullptr);
	obs_frontend_add_event_callback(frontend_event, nullptr);
	signal_handler_connect(obs_get_signal_handler(), "source_rename",
			       source_rename, nullptr);
	signal_handler_connect(obs_get_signal_handler(), "source_remove",
			       source_remove, nullptr);

	QAction *action =
		static_cast<QAction *>(obs_frontend_add_tools_menu_qaction(
			obs_module_text("SourceDock")));
	QMenu *menu = new QMenu();
	action->setMenu(menu);
	QObject::connect(menu, &QMenu::aboutToShow, [menu] { LoadMenu(menu); });
	return true;
}

void obs_module_unload()
{
	obs_frontend_remove_save_callback(frontend_save_load, nullptr);
	obs_frontend_remove_event_callback(frontend_event, nullptr);
	signal_handler_disconnect(obs_get_signal_handler(), "source_rename",
			       source_rename, nullptr);
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
	  eventFilter(BuildEventFilter())
{
	setFeatures(DockWidgetMovable | DockWidgetFloatable);
	setWindowTitle(QT_UTF8(obs_source_get_name(source)));
	setObjectName("SourceDock");
	setFloating(true);
	hide();

	QVBoxLayout *mainLayout = new QVBoxLayout(this);

	uint32_t caps = obs_source_get_output_flags(source);

	if ((caps & OBS_SOURCE_VIDEO) != 0) {

		preview = new OBSQTDisplay(this);
		preview->setObjectName(QStringLiteral("preview"));
		QSizePolicy sizePolicy1(QSizePolicy::Expanding,
					QSizePolicy::Expanding);
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
		connect(preview, &OBSQTDisplay::DisplayCreated,
			addDrawCallback);

		mainLayout->addWidget(preview);
	}else {
		preview = nullptr;
	}
	if ((caps & OBS_SOURCE_AUDIO) != 0) {
		obs_volmeter = obs_volmeter_create(OBS_FADER_LOG);
		obs_volmeter_attach_source(obs_volmeter, source);

		volMeter = new VolumeMeter(nullptr, obs_volmeter);

		obs_volmeter_add_callback(obs_volmeter, OBSVolumeLevel, this);

		mainLayout->addWidget(volMeter);

		auto *audioLayout = new QHBoxLayout(this);

		obs_data_t *settings = obs_source_get_private_settings(source);
		const bool lock = obs_data_get_bool(settings, "volume_locked");
		obs_data_release(settings);

		locked = new LockedCheckBox();
		locked->setSizePolicy(QSizePolicy::Maximum,
				      QSizePolicy::Maximum);
		locked->setFixedSize(16, 16);
		locked->setChecked(lock);
		locked->setStyleSheet("background: none");

		connect(locked, &QCheckBox::stateChanged, this,
			&SourceDock::LockVolumeControl, Qt::DirectConnection);

		slider = new SliderIgnoreScroll(Qt::Horizontal);
		slider->setSizePolicy(QSizePolicy::Expanding,
				      QSizePolicy::Preferred);
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

		signal_handler_connect(obs_source_get_signal_handler(source),
				       "mute", OBSMute, this);
		signal_handler_connect(obs_source_get_signal_handler(source),
				       "volume", OBSVolume, this);

		audioLayout->addWidget(locked);
		audioLayout->addWidget(slider);
		audioLayout->addWidget(mute);

		mainLayout->addLayout(audioLayout);
	}else {
		obs_volmeter = nullptr;
	}
	if ((caps & OBS_SOURCE_CONTROLLABLE_MEDIA) != 0) {
		mediaControl =
			new MediaControl(OBSGetWeakRef(source), true, true);
		mainLayout->addWidget(mediaControl);
	}else {
		mediaControl = nullptr;
	}

	auto *dockWidgetContents = new QWidget;
	dockWidgetContents->setLayout(mainLayout);

	setWidget(dockWidgetContents);

	obs_source_inc_showing(source);
}

SourceDock::~SourceDock()
{
	if (preview)
		obs_display_remove_draw_callback(preview->GetDisplay(), DrawPreview, this);
	if (obs_volmeter)
		obs_volmeter_remove_callback(obs_volmeter, OBSVolumeLevel, this);
	signal_handler_disconnect(obs_source_get_signal_handler(source), "mute",
			       OBSMute, this);
	signal_handler_disconnect(obs_source_get_signal_handler(source), "volume",
			       OBSVolume, this);
	obs_source_dec_showing(source);
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

void SourceDock::Rename(std::string new_name)
{
	setWindowTitle(QT_UTF8(new_name.c_str()));
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

LockedCheckBox::LockedCheckBox() {}

LockedCheckBox::LockedCheckBox(QWidget *parent) : QCheckBox(parent) {}

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
