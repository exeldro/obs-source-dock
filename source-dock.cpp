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
#include "graphics/matrix4.h"

#define QT_UTF8(str) QString::fromUtf8(str)
#define QT_TO_UTF8(str) str.toUtf8().constData()

OBS_DECLARE_MODULE()
OBS_MODULE_AUTHOR("Exeldro");
OBS_MODULE_USE_DEFAULT_LOCALE("source-dock", "en-US")

QMainWindow *GetSourceWindowByTitle(const QString window_name);

static void frontend_save_load(obs_data_t *save_data, bool saving, void *)
{
	const auto main_window =
		static_cast<QMainWindow *>(obs_frontend_get_main_window());
	if (saving) {
		obs_data_t *obj = obs_data_create();
		obs_data_array_t *docks = obs_data_array_create();
		for (const auto &it : source_docks) {
			obs_data_t *dock = obs_data_create();
			if (!it->GetSelected()) {
				obs_data_set_string(
					dock, "source_name",
					obs_source_get_name(it->GetSource()));
			}
			obs_data_set_bool(dock, "selected", it->GetSelected());
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
			obs_data_set_bool(dock, "textinput",
					  it->TextInputEnabled());
			obs_data_set_string(
				dock, "geometry",
				it->saveGeometry().toBase64().constData());
			auto *p = dynamic_cast<QMainWindow *>(it->parent());
			if (p == main_window) {
				obs_data_set_string(dock, "window", "");
			} else {
				QString wt = p->windowTitle();
				auto t = wt.toUtf8();
				const char *wtc = t.constData();
				obs_data_set_string(dock, "window", wtc);
			}
			obs_data_set_int(dock, "dockarea",
					 p->dockWidgetArea(it));
			obs_data_set_bool(dock, "floating", it->isFloating());
			obs_data_set_double(dock, "zoom", it->GetZoom());
			obs_data_set_double(dock, "scrollx", it->GetScrollX());
			obs_data_set_double(dock, "scrolly", it->GetScrollY());
			obs_data_array_push_back(docks, dock);
			obs_data_release(dock);
		}
		obs_data_set_array(obj, "docks", docks);
		obs_data_array_release(docks);
		obs_data_array_t *windows = obs_data_array_create();
		for (const auto &it : source_windows) {
			if (it->isHidden())
				continue;
			obs_data_t *window = obs_data_create();
			QString wt = it->windowTitle();
			auto t = wt.toUtf8();
			const char *wtc = t.constData();
			obs_data_set_string(window, "name", wtc);
			obs_data_set_string(
				window, "geometry",
				it->saveGeometry().toBase64().constData());
			obs_data_array_push_back(windows, window);
			obs_data_release(window);
		}
		obs_data_set_array(obj, "windows", windows);
		obs_data_array_release(windows);
		obs_data_set_bool(obj, "corner_tl",
				  main_window->corner(Qt::TopLeftCorner) ==
					  Qt::LeftDockWidgetArea);
		obs_data_set_bool(obj, "corner_tr",
				  main_window->corner(Qt::TopRightCorner) ==
					  Qt::RightDockWidgetArea);
		obs_data_set_bool(obj, "corner_br",
				  main_window->corner(Qt::BottomRightCorner) ==
					  Qt::RightDockWidgetArea);
		obs_data_set_bool(obj, "corner_bl",
				  main_window->corner(Qt::BottomLeftCorner) ==
					  Qt::LeftDockWidgetArea);
		obs_data_set_obj(save_data, "source-dock", obj);

		obs_data_release(obj);
	} else {
		for (const auto &it : source_docks) {
			it->close();
			it->deleteLater();
		}
		source_docks.clear();

		obs_data_t *obj = obs_data_get_obj(save_data, "source-dock");
		if (obj) {
			main_window->setCorner(Qt::TopLeftCorner,
					       obs_data_get_bool(obj,
								 "corner_tl")
						       ? Qt::LeftDockWidgetArea
						       : Qt::TopDockWidgetArea);
			main_window->setCorner(Qt::TopRightCorner,
					       obs_data_get_bool(obj,
								 "corner_tr")
						       ? Qt::RightDockWidgetArea
						       : Qt::TopDockWidgetArea);
			main_window->setCorner(
				Qt::BottomRightCorner,
				obs_data_get_bool(obj, "corner_br")
					? Qt::RightDockWidgetArea
					: Qt::BottomDockWidgetArea);
			main_window->setCorner(
				Qt::BottomLeftCorner,
				obs_data_get_bool(obj, "corner_bl")
					? Qt::LeftDockWidgetArea
					: Qt::BottomDockWidgetArea);
			obs_frontend_push_ui_translation(obs_module_get_string);
			obs_data_array_t *docks =
				obs_data_get_array(obj, "docks");
			if (docks) {
				size_t count = obs_data_array_count(docks);
				for (size_t i = 0; i < count; i++) {
					obs_data_t *dock =
						obs_data_array_item(docks, i);
					obs_source_t *s = nullptr;
					QString source_name;
					if (!obs_data_get_bool(dock,
							       "selected")) {
						s = obs_get_source_by_name(
							obs_data_get_string(
								dock,
								"source_name"));
						if (!s) {
							obs_data_release(dock);
							continue;
						}
					}

					const auto windowName = QT_UTF8(
						obs_data_get_string(dock,
								    "window"));
					auto window = GetSourceWindowByTitle(
						windowName);
					if (window == nullptr)
						window = main_window;
					SourceDock *tmp;
					auto title = obs_data_get_string(
						dock, "title");
					if (!title || !strlen(title)) {
						if (s)
							title = obs_source_get_name(
								s);

						tmp = new SourceDock(
							title, s == nullptr,
							window);
						tmp->SetSource(s);
						tmp->EnablePreview();
						tmp->EnableVolMeter();
						tmp->EnableVolControls();
						tmp->EnableMediaControls();
						tmp->EnableSwitchScene();
						tmp->EnableShowActive();
					} else {
						tmp = new SourceDock(
							title, s == nullptr,
							window);
						tmp->SetSource(s);
					}

					source_docks.push_back(tmp);
					if (obs_data_get_bool(dock, "preview"))
						tmp->EnablePreview();

					if (obs_data_get_bool(dock, "volmeter"))
						tmp->EnableVolMeter();
					if (obs_data_get_bool(dock,
							      "volcontrols"))
						tmp->EnableVolControls();
					if (obs_data_get_bool(dock,
							      "mediacontrols"))
						tmp->EnableMediaControls();
					if (obs_data_get_bool(dock,
							      "switchscene"))
						tmp->EnableSwitchScene();
					if (obs_data_get_bool(dock,
							      "showactive"))
						tmp->EnableShowActive();
					if (obs_data_get_bool(dock,
							      "properties"))
						tmp->EnableProperties();
					if (obs_data_get_bool(dock, "filters"))
						tmp->EnableFilters();
					if (obs_data_get_bool(dock,
							      "sceneitems"))
						tmp->EnableSceneItems();
					if (obs_data_get_bool(dock,
							      "textinput"))
						tmp->EnableTextInput();
					auto *a = static_cast<QAction *>(
						obs_frontend_add_dock(tmp));
					tmp->setAction(a);
					if (obs_data_get_bool(dock, "hidden"))
						tmp->hide();
					else
						tmp->show();
					obs_source_release(s);

					const auto dockarea =
						static_cast<Qt::DockWidgetArea>(
							obs_data_get_int(
								dock,
								"dockarea"));
					if (dockarea !=
					    window->dockWidgetArea(tmp))
						window->addDockWidget(dockarea,
								      tmp);

					const auto floating = obs_data_get_bool(
						dock, "floating");
					if (tmp->isFloating() != floating)
						tmp->setFloating(floating);

					const char *geometry =
						obs_data_get_string(dock,
								    "geometry");
					if (geometry && strlen(geometry))
						tmp->restoreGeometry(
							QByteArray::fromBase64(
								QByteArray(
									geometry)));
					tmp->SetZoom(obs_data_get_double(
						dock, "zoom"));
					tmp->SetScrollX(obs_data_get_double(
						dock, "scrollx"));
					tmp->SetScrollY(obs_data_get_double(
						dock, "scrolly"));
					obs_data_release(dock);
				}
				obs_data_array_release(docks);
			}
			obs_data_array_t *windows =
				obs_data_get_array(obj, "windows");
			if (windows) {
				size_t count = obs_data_array_count(windows);
				for (size_t i = 0; i < count; i++) {
					obs_data_t *window =
						obs_data_array_item(windows, i);
					auto mainWindow = GetSourceWindowByTitle(
						QT_UTF8(obs_data_get_string(
							window, "name")));
					if (mainWindow) {
						const char *geometry =
							obs_data_get_string(
								window,
								"geometry");
						if (geometry &&
						    strlen(geometry))
							mainWindow->restoreGeometry(
								QByteArray::fromBase64(QByteArray(
									geometry)));
					}
					obs_data_release(window);
				}
				obs_data_array_release(windows);
			}
			obs_frontend_pop_ui_translation();
			obs_data_release(obj);
		}
	}
}

static void item_select(void *p, calldata_t *calldata)
{
	auto item = (obs_sceneitem_t *)calldata_ptr(calldata, "item");
	auto source = obs_sceneitem_get_source(item);
	for (const auto &it : source_docks) {
		if (!it->GetSelected())
			continue;
		it->SetSource(source);
	}
}

obs_source_t *previous_scene = nullptr;

bool get_selected_source(obs_scene_t *obs_scene, obs_sceneitem_t *item, void *p)
{
	if (!obs_sceneitem_selected(item))
		return true;
	auto source = obs_sceneitem_get_source(item);
	*(obs_source_t **)p = source;
	return false;
}

void update_selected_source()
{
	if (!previous_scene)
		return;
	auto scene = obs_scene_from_source(previous_scene);
	obs_source_t *selected_source = nullptr;
	obs_scene_enum_items(scene, get_selected_source, &selected_source);
	if (!selected_source)
		return;
	for (const auto &it : source_docks) {
		if (!it->GetSelected())
			continue;
		it->SetSource(selected_source);
	}
}

void set_previous_scene_empty(void *p, calldata_t *calldata)
{
	if (!previous_scene)
		return;
	auto sh = obs_source_get_signal_handler(previous_scene);
	if (sh) {
		signal_handler_disconnect(sh, "item_select", item_select,
					  nullptr);
		signal_handler_disconnect(sh, "remove",
					  set_previous_scene_empty, nullptr);
		signal_handler_disconnect(sh, "destroy",
					  set_previous_scene_empty, nullptr);
	}
	previous_scene = nullptr;
}

static void frontend_event(enum obs_frontend_event event, void *)
{
	if (event == OBS_FRONTEND_EVENT_SCENE_COLLECTION_CLEANUP ||
	    event == OBS_FRONTEND_EVENT_EXIT) {
		set_previous_scene_empty(nullptr, nullptr);
		for (const auto &it : source_docks) {
			it->close();
			delete (it);
		}
		source_docks.clear();
		for (const auto &it : source_windows) {
			it->close();
			delete (it);
		}
		source_windows.clear();
	} else if (event == OBS_FRONTEND_EVENT_SCENE_CHANGED ||
		   event == OBS_FRONTEND_EVENT_PREVIEW_SCENE_CHANGED ||
		   event == OBS_FRONTEND_EVENT_STUDIO_MODE_DISABLED ||
		   event == OBS_FRONTEND_EVENT_STUDIO_MODE_ENABLED) {
		if (previous_scene) {
			set_previous_scene_empty(nullptr, nullptr);
		}
		if (obs_frontend_preview_program_mode_active()) {
			auto *preview =
				obs_frontend_get_current_preview_scene();
			if (preview) {
				auto sh =
					obs_source_get_signal_handler(preview);
				if (sh) {
					previous_scene = preview;
					signal_handler_connect(sh,
							       "item_select",
							       item_select,
							       nullptr);
					signal_handler_connect(
						sh, "remove",
						set_previous_scene_empty,
						nullptr);
					signal_handler_connect(
						sh, "destroy",
						set_previous_scene_empty,
						nullptr);
				}
			}

			std::map<obs_source_t *, int> source_active;
			for (const auto &it : source_docks) {
				if (it->ShowActiveEnabled())
					source_active[it->GetSource()] = 0;
			}
			update_selected_source();
			if (source_active.empty())
				return;
			if (preview) {
				for (auto &i : source_active) {
					if (i.first == preview) {
						i.second = 1;
						break;
					}
				}
				obs_source_enum_active_tree(
					preview,
					[](obs_source_t *parent,
					   obs_source_t *child, void *param) {
						auto m = static_cast<std::map<
							obs_source_t *, int> *>(
							param);

						for (auto &i : *m) {

							if (i.first == child) {
								i.second = 1;
								break;
							}
						}
					},
					&source_active);
				obs_source_release(preview);
			}
			if (auto *program = obs_frontend_get_current_scene()) {
				const char *scene_name =
					obs_source_get_name(program);
				for (auto &i : source_active) {
					auto *sn = obs_source_get_name(i.first);
					if (strcmp(sn, scene_name) == 0) {
						i.second = 2;
						break;
					}
				}
				obs_source_enum_active_tree(
					program,
					[](obs_source_t *parent,
					   obs_source_t *child, void *param) {
						auto m = static_cast<std::map<
							obs_source_t *, int> *>(
							param);
						const char *child_name =
							obs_source_get_name(
								child);
						if (!child_name ||
						    strlen(child_name) == 0)
							return;
						for (auto &i : *m) {
							auto *sn =
								obs_source_get_name(
									i.first);
							if (strcmp(sn,
								   child_name) ==
							    0) {
								i.second = 2;
								break;
							}
						}
					},
					&source_active);
				obs_source_release(program);
			}
			for (const auto &it : source_docks) {
				it->SetActive(
					source_active[it->GetSource().Get()]);
			}
		} else {
			auto scene = obs_frontend_get_current_scene();
			if (scene) {
				auto sh = obs_source_get_signal_handler(scene);
				if (sh) {
					previous_scene = scene;
					signal_handler_connect(sh,
							       "item_select",
							       item_select,
							       nullptr);
					signal_handler_connect(
						sh, "remove",
						set_previous_scene_empty,
						nullptr);
					signal_handler_connect(
						sh, "destroy",
						set_previous_scene_empty,
						nullptr);
				}
				obs_source_release(scene);
			}

			for (const auto &it : source_docks) {
				it->SetActive(obs_source_active(it->GetSource())
						      ? 2
						      : 0);
			}
			update_selected_source();
		}
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

	const auto action =
		static_cast<QAction *>(obs_frontend_add_tools_menu_qaction(
			obs_module_text("SourceDock")));

	auto cb = [] {
		obs_frontend_push_ui_translation(obs_module_get_string);

		const auto sdsd =
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

SourceDock::SourceDock(QString name, bool selected_, QWidget *parent)
	: QDockWidget(parent),
	  source(nullptr),
	  eventFilter(BuildEventFilter()),
	  action(nullptr),
	  zoom(1.0f),
	  scrollX(0.5f),
	  scrollY(0.5f),
	  scrollingFromX(0),
	  scrollingFromY(0),
	  preview(nullptr),
	  volMeter(nullptr),
	  obs_volmeter(nullptr),
	  mediaControl(nullptr),
	  mainLayout(new QVBoxLayout(this)),
	  volControl(nullptr),
	  switch_scene_enabled(false),
	  activeLabel(nullptr),
	  sceneItems(nullptr),
	  propertiesButton(nullptr),
	  filtersButton(nullptr),
	  textInput(nullptr),
	  selected(selected_)
{
	setFeatures(AllDockWidgetFeatures);
	setWindowTitle(name);
	setObjectName(name);
	setFloating(true);
	hide();

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
	auto newCX = scale * float(sourceCX);
	auto newCY = scale * float(sourceCY);

	auto extraCx = (window->zoom - 1.0f) * newCX;
	auto extraCy = (window->zoom - 1.0f) * newCY;
	int newCx = newCX * window->zoom;
	int newCy = newCY * window->zoom;
	x -= extraCx * window->scrollX;
	y -= extraCy * window->scrollY;
	gs_viewport_push();
	gs_projection_push();
	const bool previous = gs_set_linear_srgb(true);

	gs_ortho(0.0f, float(sourceCX), 0.0f, float(sourceCY), -100.0f, 100.0f);
	gs_set_viewport(x, y, newCx, newCy);
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
	if (source && obs_source_muted(source) != mute)
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
	bool active = source && obs_source_active(source);
	if (active) {
		activeLabel->setProperty("themeID", "good");
		activeLabel->setText(QT_UTF8(obs_module_text("Active")));
	} else if (!obs_frontend_preview_program_mode_active()) {
		activeLabel->setText(QT_UTF8(obs_module_text("NotActive")));
		activeLabel->setProperty("themeID", "");
	}

	/* force style sheet recalculation */
	QString qss = activeLabel->styleSheet();
	activeLabel->setStyleSheet("/* */");
	activeLabel->setStyleSheet(qss);
}

void SourceDock::SetActive(int active)
{
	if (!activeLabel)
		return;
	if (active == 2) {
		activeLabel->setProperty(
			"themeID", obs_frontend_preview_program_mode_active()
					   ? "error"
					   : "good");
		activeLabel->setText(QT_UTF8(obs_module_text("Active")));
	} else if (active == 1) {
		activeLabel->setProperty("themeID", "good");
		activeLabel->setText(QT_UTF8(obs_module_text("Preview")));
	} else {
		activeLabel->setText(QT_UTF8(obs_module_text("NotActive")));
		activeLabel->setProperty("themeID", "");
	}

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
	if (source)
		obs_source_set_volume(source, mul);
}

bool SourceDock::GetSourceRelativeXY(int mouseX, int mouseY, int &relX,
				     int &relY)
{
	float pixelRatio = devicePixelRatioF();

	int mouseXscaled = (int)roundf(mouseX * pixelRatio);
	int mouseYscaled = (int)roundf(mouseY * pixelRatio);

	QSize size = preview->size() * preview->devicePixelRatioF();

	uint32_t sourceCX = source ? obs_source_get_width(source) : 1;
	if (sourceCX <= 0)
		sourceCX = 1;
	uint32_t sourceCY = source ? obs_source_get_height(source) : 1;
	if (sourceCY <= 0)
		sourceCY = 1;

	int x, y;
	float scale;

	GetScaleAndCenterPos(sourceCX, sourceCY, size.width(), size.height(), x,
			     y, scale);

	auto newCX = scale * float(sourceCX);
	auto newCY = scale * float(sourceCY);

	auto extraCx = (zoom - 1.0f) * newCX;
	auto extraCy = (zoom - 1.0f) * newCY;

	scale *= zoom;

	if (x > 0) {
		relX = int(float(mouseXscaled - x + extraCx * scrollX) / scale);
		relY = int(float(mouseYscaled + extraCy * scrollY) / scale);
	} else {
		relX = int(float(mouseXscaled + extraCx * scrollX) / scale);
		relY = int(float(mouseYscaled - y + extraCy * scrollY) / scale);
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

static bool CloseFloat(float a, float b, float epsilon = 0.01)
{
	using std::abs;
	return abs(a - b) <= epsilon;
}

struct click_event {
	int32_t x;
	int32_t y;
	uint32_t modifiers;
	int32_t button;
	bool mouseUp;
	uint32_t clickCount;
};

static bool HandleSceneMouseClickEvent(obs_scene_t *scene,
				       obs_sceneitem_t *item, void *data)
{
	auto click_event = static_cast<struct click_event *>(data);

	matrix4 transform;
	matrix4 invTransform;
	vec3 transformedPos;
	vec3 pos3;
	vec3 pos3_;

	vec3_set(&pos3, click_event->x, click_event->y, 0.0f);

	obs_sceneitem_get_box_transform(item, &transform);

	matrix4_inv(&invTransform, &transform);
	vec3_transform(&transformedPos, &pos3, &invTransform);
	vec3_transform(&pos3_, &transformedPos, &transform);

	if (click_event->mouseUp ||
	    (CloseFloat(pos3.x, pos3_.x) && CloseFloat(pos3.y, pos3_.y) &&
	     transformedPos.x >= 0.0f && transformedPos.x <= 1.0f &&
	     transformedPos.y >= 0.0f && transformedPos.y <= 1.0f)) {
		auto source = obs_sceneitem_get_source(item);
		obs_mouse_event mouseEvent{};
		mouseEvent.x =
			transformedPos.x * obs_source_get_base_width(source);
		mouseEvent.y =
			transformedPos.y * obs_source_get_base_height(source);
		mouseEvent.modifiers = click_event->modifiers;
		obs_source_send_mouse_click(source, &mouseEvent,
					    click_event->button,
					    click_event->mouseUp,
					    click_event->clickCount);
	}
	return true;
}

bool SourceDock::HandleMouseClickEvent(QMouseEvent *event)
{
	const bool mouseUp = event->type() == QEvent::MouseButtonRelease;
	if (!mouseUp && event->button() == Qt::LeftButton &&
	    event->modifiers().testFlag(Qt::ControlModifier)) {
		scrollingFromX = event->x();
		scrollingFromY = event->y();
	}
	uint32_t clickCount = 1;
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

	const bool insideSource = GetSourceRelativeXY(
		event->x(), event->y(), mouseEvent.x, mouseEvent.y);

	if (source && (mouseUp || insideSource))
		obs_source_send_mouse_click(source, &mouseEvent, button,
					    mouseUp, clickCount);

	if (switch_scene_enabled && obs_source_is_scene(source)) {
		if (mouseUp) {
			if (obs_frontend_preview_program_mode_active()) {
				obs_frontend_set_current_preview_scene(source);
			} else {
				obs_frontend_set_current_scene(source);
			}
		} else if (clickCount == 2 &&
			   obs_frontend_preview_program_mode_active()) {
			obs_frontend_set_current_scene(source);
		}
	} else {
		if (obs_scene_t *scene = obs_scene_from_source(source)) {
			if (mouseUp || insideSource) {
				click_event ce{mouseEvent.x,
					       mouseEvent.y,
					       mouseEvent.modifiers,
					       button,
					       mouseUp,
					       clickCount};
				obs_scene_enum_items(
					scene, HandleSceneMouseClickEvent, &ce);
			}
		}
	}

	return true;
}

struct move_event {
	int32_t x;
	int32_t y;
	uint32_t modifiers;
	bool mouseLeave;
};

static bool HandleSceneMouseMoveEvent(obs_scene_t *scene, obs_sceneitem_t *item,
				      void *data)
{
	auto move_event = static_cast<struct move_event *>(data);

	matrix4 transform{};
	matrix4 invTransform{};
	vec3 transformedPos{};
	vec3 pos3{};
	vec3 pos3_{};

	vec3_set(&pos3, move_event->x, move_event->y, 0.0f);

	obs_sceneitem_get_box_transform(item, &transform);

	matrix4_inv(&invTransform, &transform);
	vec3_transform(&transformedPos, &pos3, &invTransform);
	vec3_transform(&pos3_, &transformedPos, &transform);

	if (CloseFloat(pos3.x, pos3_.x) && CloseFloat(pos3.y, pos3_.y) &&
	    transformedPos.x >= 0.0f && transformedPos.x <= 1.0f &&
	    transformedPos.y >= 0.0f && transformedPos.y <= 1.0f) {
		auto source = obs_sceneitem_get_source(item);
		obs_mouse_event mouseEvent{};
		mouseEvent.x =
			transformedPos.x * obs_source_get_base_width(source);
		mouseEvent.y =
			transformedPos.y * obs_source_get_base_height(source);
		mouseEvent.modifiers = move_event->modifiers;
		obs_source_send_mouse_move(source, &mouseEvent,
					   move_event->mouseLeave);
	}
	return true;
}

bool SourceDock::HandleMouseMoveEvent(QMouseEvent *event)
{
	if (!event)
		return false;
	if (!source)
		return true;
	if (event->buttons() == Qt::LeftButton &&
	    event->modifiers().testFlag(Qt::ControlModifier)) {

		QSize size = preview->size() * preview->devicePixelRatioF();
		scrollX -= float(event->x() - scrollingFromX) / size.width();
		scrollY -= float(event->y() - scrollingFromY) / size.height();
		if (scrollX < 0.0f)
			scrollX = 0.0;
		if (scrollX > 1.0f)
			scrollX = 1.0f;
		if (scrollY < 0.0f)
			scrollY = 0.0;
		if (scrollY > 1.0f)
			scrollY = 1.0f;
		scrollingFromX = event->x();
		scrollingFromY = event->y();
		return true;
	}

	struct obs_mouse_event mouseEvent = {};

	bool mouseLeave = event->type() == QEvent::Leave;

	if (!mouseLeave) {
		mouseEvent.modifiers = TranslateQtMouseEventModifiers(event);
		mouseLeave = !GetSourceRelativeXY(event->x(), event->y(),
						  mouseEvent.x, mouseEvent.y);
	}

	obs_source_send_mouse_move(source, &mouseEvent, mouseLeave);
	if (!switch_scene_enabled) {
		if (obs_scene_t *scene = obs_scene_from_source(source)) {
			move_event ce{mouseEvent.x, mouseEvent.y,
				      mouseEvent.modifiers, mouseLeave};
			obs_scene_enum_items(scene, HandleSceneMouseMoveEvent,
					     &ce);
		}
	}

	return true;
}

struct wheel_event {
	int32_t x;
	int32_t y;
	uint32_t modifiers;
	int xDelta;
	int yDelta;
};

static bool HandleSceneMouseWheelEvent(obs_scene_t *scene,
				       obs_sceneitem_t *item, void *data)
{
	auto wheel_event = static_cast<struct wheel_event *>(data);

	matrix4 transform{};
	matrix4 invTransform{};
	vec3 transformedPos{};
	vec3 pos3{};
	vec3 pos3_{};

	vec3_set(&pos3, wheel_event->x, wheel_event->y, 0.0f);

	obs_sceneitem_get_box_transform(item, &transform);

	matrix4_inv(&invTransform, &transform);
	vec3_transform(&transformedPos, &pos3, &invTransform);
	vec3_transform(&pos3_, &transformedPos, &transform);

	if (CloseFloat(pos3.x, pos3_.x) && CloseFloat(pos3.y, pos3_.y) &&
	    transformedPos.x >= 0.0f && transformedPos.x <= 1.0f &&
	    transformedPos.y >= 0.0f && transformedPos.y <= 1.0f) {
		auto source = obs_sceneitem_get_source(item);
		obs_mouse_event mouseEvent{};
		mouseEvent.x =
			transformedPos.x * obs_source_get_base_width(source);
		mouseEvent.y =
			transformedPos.y * obs_source_get_base_height(source);
		mouseEvent.modifiers = wheel_event->modifiers;
		obs_source_send_mouse_wheel(source, &mouseEvent,
					    wheel_event->xDelta,
					    wheel_event->yDelta);
	}
	return true;
}

bool SourceDock::HandleMouseWheelEvent(QWheelEvent *event)
{
	if (!source)
		return true;
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

	const bool insideSource =
		GetSourceRelativeXY(x, y, mouseEvent.x, mouseEvent.y);
	if ((QGuiApplication::keyboardModifiers() & Qt::ControlModifier) &&
	    yDelta != 0) {
		const auto factor = 1.0f + (0.0008f * yDelta);

		zoom *= factor;
		if (zoom < 1.0f)
			zoom = 1.0f;
		if (zoom > 100.0f)
			zoom = 100.0f;

	} else if (insideSource) {
		obs_source_send_mouse_wheel(source, &mouseEvent, xDelta,
					    yDelta);
		if (switch_scene_enabled) {

		} else if (obs_scene_t *scene = obs_scene_from_source(source)) {
			wheel_event ce{mouseEvent.x, mouseEvent.y,
				       mouseEvent.modifiers, xDelta, yDelta};
			obs_scene_enum_items(scene, HandleSceneMouseWheelEvent,
					     &ce);
		}
	}

	return true;
}

bool SourceDock::HandleFocusEvent(QFocusEvent *event)
{
	bool focus = event->type() == QEvent::FocusIn;

	if (source)
		obs_source_send_focus(source, focus);

	return true;
}
struct key_event {
	struct obs_key_event keyEvent;
	bool keyUp;
};

bool HandleSceneKeyEvent(obs_scene_t *scene, obs_sceneitem_t *item, void *data)
{
	auto key_event = static_cast<struct key_event *>(data);
	const auto source = obs_sceneitem_get_source(item);
	obs_source_send_key_click(source, &key_event->keyEvent,
				  key_event->keyUp);
	return true;
}

bool SourceDock::HandleKeyEvent(QKeyEvent *event)
{
	if (!source)
		return true;
	struct obs_key_event keyEvent;

	QByteArray text = event->text().toUtf8();
	keyEvent.modifiers = TranslateQtKeyboardEventModifiers(event, false);
	keyEvent.text = text.data();
	keyEvent.native_modifiers = event->nativeModifiers();
	keyEvent.native_scancode = event->nativeScanCode();
	keyEvent.native_vkey = event->nativeVirtualKey();

	bool keyUp = event->type() == QEvent::KeyRelease;

	obs_source_send_key_click(source, &keyEvent, keyUp);
	if (!switch_scene_enabled) {
		if (obs_scene_t *scene = obs_scene_from_source(source)) {
			key_event ce{keyEvent, keyUp};
			obs_scene_enum_items(scene, HandleSceneKeyEvent, &ce);
		}
	}

	return true;
}

void SourceDock::EnablePreview()
{
	if (preview != nullptr)
		return;
	preview = new OBSQTDisplay(this);
	preview->setObjectName(QStringLiteral("preview"));
	preview->setMinimumSize(QSize(24, 24));
	QSizePolicy sizePolicy1(QSizePolicy::Expanding, QSizePolicy::Expanding);
	sizePolicy1.setHorizontalStretch(0);
	sizePolicy1.setVerticalStretch(0);
	sizePolicy1.setHeightForWidth(
		preview->sizePolicy().hasHeightForWidth());
	preview->setSizePolicy(sizePolicy1);

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
	if (source)
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

	obs_volmeter = obs_volmeter_create(OBS_FADER_LOG);
	if (source)
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

void SourceDock::UpdateVolControls()
{
	if (!volControl)
		return;
	bool lock = false;
	if (obs_data_t *settings =
		    source ? obs_source_get_private_settings(source)
			   : nullptr) {
		lock = obs_data_get_bool(settings, "volume_locked");
		obs_data_release(settings);
	}
	locked->setChecked(lock);
	mute->setEnabled(!lock);
	mute->setChecked(source ? obs_source_muted(source) : false);
	slider->setEnabled(!lock);
	float mul = source ? obs_source_get_volume(source) : 0.0f;
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
}

void SourceDock::EnableVolControls()
{
	if (volControl != nullptr)
		return;

	volControl = new QWidget;
	auto *audioLayout = new QHBoxLayout(this);

	locked = new LockedCheckBox();
	locked->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
	locked->setFixedSize(16, 16);

	locked->setStyleSheet("background: none");

	connect(locked, &QCheckBox::stateChanged, this,
		&SourceDock::LockVolumeControl, Qt::DirectConnection);

	slider = new SliderIgnoreScroll(Qt::Horizontal);
	slider->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
	slider->setMinimum(0);
	slider->setMaximum(10000);
	slider->setToolTip(QT_UTF8(obs_module_text("VolumeOutput")));

	connect(slider, SIGNAL(valueChanged(int)), this,
		SLOT(SliderChanged(int)));

	mute = new MuteCheckBox();

	connect(mute, &QCheckBox::stateChanged, this,
		&SourceDock::MuteVolumeControl, Qt::DirectConnection);

	if (source) {
		const auto sh = obs_source_get_signal_handler(source);
		signal_handler_connect(sh, "mute", OBSMute, this);
		signal_handler_connect(sh, "volume", OBSVolume, this);
	}

	audioLayout->addWidget(locked);
	audioLayout->addWidget(slider);
	audioLayout->addWidget(mute);

	volControl->setLayout(audioLayout);
	mainLayout->addWidget(volControl);

	UpdateVolControls();
}

void SourceDock::DisableVolControls()
{
	if (!volControl)
		return;
	if (source) {
		const auto sh = obs_source_get_signal_handler(source);
		signal_handler_disconnect(sh, "mute", OBSMute, this);
		signal_handler_disconnect(sh, "volume", OBSVolume, this);
	}
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

void SourceDock::EnableTextInput()
{
	if (textInput)
		return;

	textInput = new QPlainTextEdit;
	textInput->setObjectName(QStringLiteral("textInput"));

	if (auto *settings = source ? obs_source_get_settings(source)
				    : nullptr) {
		textInput->setPlainText(
			QT_UTF8(obs_data_get_string(settings, "text")));
		obs_data_release(settings);
	}

	mainLayout->addWidget(textInput);
	auto changeText = [this]() {
		if (auto *settings = source ? obs_source_get_settings(source)
					    : nullptr) {
			if (textInput->toPlainText() !=
			    QT_UTF8(obs_data_get_string(settings, "text"))) {
				obs_data_set_string(
					settings, "text",
					QT_TO_UTF8(textInput->toPlainText()));
				obs_source_update(source, nullptr);
			}
			obs_data_release(settings);
		}
	};
	connect(textInput, &QPlainTextEdit::textChanged, changeText);

	textInputTimer = new QTimer(this);
	connect(textInputTimer, &QTimer::timeout, this, [=]() {
		if (auto *settings = source ? obs_source_get_settings(source)
					    : nullptr) {
			const auto text =
				QT_UTF8(obs_data_get_string(settings, "text"));
			if (textInput->toPlainText() != text) {
				textInput->setPlainText(text);
			}
			obs_data_release(settings);
		}
	});
	textInputTimer->start(1000);
}

void SourceDock::DisableTextInput()
{
	textInputTimer->stop();
	textInputTimer->deleteLater();
	textInputTimer = nullptr;

	mainLayout->removeWidget(textInput);
	textInput->deleteLater();
	textInput = nullptr;
}

bool SourceDock::TextInputEnabled()
{
	return textInput != nullptr;
}

void SourceDock::SetSource(const OBSSource source_)
{
	if (preview && source)
		obs_source_dec_showing(source);

	if (obs_volmeter)
		obs_volmeter_detach_source(obs_volmeter);

	if (volControl && source) {
		auto sh = obs_source_get_signal_handler(source);
		signal_handler_disconnect(sh, "mute", OBSMute, this);
		signal_handler_disconnect(sh, "volume", OBSVolume, this);
	}

	source = source_;

	UpdateVolControls();
	ActiveChanged();

	if (textInput) {
		if (auto *settings = source ? obs_source_get_settings(source)
					    : nullptr) {
			const auto text =
				QT_UTF8(obs_data_get_string(settings, "text"));
			if (textInput->toPlainText() != text) {
				textInput->setPlainText(text);
			}
			obs_data_release(settings);
		} else if (!textInput->toPlainText().isEmpty()) {
			textInput->setPlainText("");
		}
	}
	if (mediaControl) {
		mediaControl->SetSource(OBSGetWeakRef(source));
	}

	if (!source)
		return;

	if (volControl) {
		auto sh = obs_source_get_signal_handler(source);
		signal_handler_connect(sh, "mute", OBSMute, this);
		signal_handler_connect(sh, "volume", OBSVolume, this);
	}

	if (obs_volmeter)
		obs_volmeter_attach_source(obs_volmeter, source);

	if (preview)
		obs_source_inc_showing(source);
}

OBSSource SourceDock::GetSource()
{
	return source;
}

void SourceDock::setAction(QAction *a)
{
	action = a;
}

void SourceDock::SetZoom(float zoom)
{
	if (zoom < 1.0f)
		return;
	this->zoom = zoom;
}

void SourceDock::SetScrollX(float scroll)
{
	if (scroll < 0.0f || scroll > 1.0f)
		return;
	scrollX = scroll;
}

void SourceDock::SetScrollY(float scroll)
{
	if (scroll < 0.0f || scroll > 1.0f)
		return;
	scrollY = scroll;
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
