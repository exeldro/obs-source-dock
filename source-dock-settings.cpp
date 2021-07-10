
#include "source-dock-settings.hpp"

#include <QLabel>

#include <obs-module.h>
#include <QCheckBox>
#include <QCompleter>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QTextEdit>

#include "source-dock.hpp"

#define QT_UTF8(str) QString::fromUtf8(str)
#define QT_TO_UTF8(str) str.toUtf8().constData()

SourceDockSettingsDialog::SourceDockSettingsDialog(QMainWindow *parent)
	: QDialog(parent)
{
	int idx = 0;
	mainLayout = new QGridLayout;
	mainLayout->setContentsMargins(0, 0, 0, 0);
	QLabel *label = new QLabel(obs_module_text("Source"));
	label->setStyleSheet("font-weight: bold;");
	mainLayout->addWidget(label, 0, idx++, Qt::AlignCenter);
	label = new QLabel(obs_module_text("Title"));
	label->setStyleSheet("font-weight: bold;");
	mainLayout->addWidget(label, 0, idx++, Qt::AlignCenter);
	label = new VerticalLabel(obs_module_text("Preview"));
	label->setStyleSheet("font-weight: bold;");
	mainLayout->addWidget(label, 0, idx++, Qt::AlignCenter);
	label = new VerticalLabel(obs_module_text("VolumeMeter"));
	label->setStyleSheet("font-weight: bold;");
	mainLayout->addWidget(label, 0, idx++, Qt::AlignCenter);
	label = new VerticalLabel(obs_module_text("AudioControls"));
	label->setStyleSheet("font-weight: bold;");
	mainLayout->addWidget(label, 0, idx++, Qt::AlignCenter);
	label = new VerticalLabel(obs_module_text("MediaControls"));
	label->setStyleSheet("font-weight: bold;");
	mainLayout->addWidget(label, 0, idx++, Qt::AlignCenter);
	label = new VerticalLabel(obs_module_text("SwitchScene"));
	label->setStyleSheet("font-weight: bold;");
	mainLayout->addWidget(label, 0, idx++, Qt::AlignCenter);
	label = new VerticalLabel(obs_module_text("ShowActive"));
	label->setStyleSheet("font-weight: bold;");
	mainLayout->addWidget(label, 0, idx++, Qt::AlignCenter);

	selectBoxColumn = idx;

	QCheckBox *checkbox = new QCheckBox;
	mainLayout->addWidget(checkbox, 0, idx++, Qt::AlignCenter);
	mainLayout->setColumnStretch(0, 1);
	mainLayout->setColumnStretch(1, 1);

	connect(checkbox, &QCheckBox::stateChanged,
		[this]() { SelectAllChanged(); });

	idx = 0;
	sourceCombo = new QComboBox();
	sourceCombo->setEditable(true);
	auto *completer = sourceCombo->completer();
	completer->setCaseSensitivity(Qt::CaseInsensitive);
	completer->setFilterMode(Qt::MatchContains);
	completer->setCompletionMode(QCompleter::PopupCompletion);
	sourceCombo->addItem("", QByteArray(""));
	obs_enum_scenes(AddSource, sourceCombo);
	obs_enum_sources(AddSource, sourceCombo);
	mainLayout->addWidget(sourceCombo, 1, idx++);

	connect(sourceCombo, SIGNAL(editTextChanged(const QString &)),
		SLOT(RefreshTable()));

	titleEdit = new QLineEdit();
	mainLayout->addWidget(titleEdit, 1, idx++);

	previewCheckBox = new QCheckBox();
	previewCheckBox->setChecked(true);
	mainLayout->addWidget(previewCheckBox, 1, idx++);

	volMeterCheckBox = new QCheckBox();
	volMeterCheckBox->setChecked(true);
	mainLayout->addWidget(volMeterCheckBox, 1, idx++);

	volControlsCheckBox = new QCheckBox();
	volControlsCheckBox->setChecked(true);
	mainLayout->addWidget(volControlsCheckBox, 1, idx++);

	mediaControlsCheckBox = new QCheckBox();
	mediaControlsCheckBox->setChecked(true);
	mainLayout->addWidget(mediaControlsCheckBox, 1, idx++);

	switchSceneCheckBox = new QCheckBox();
	switchSceneCheckBox->setChecked(true);
	mainLayout->addWidget(switchSceneCheckBox, 1, idx++);

	showActiveCheckBox = new QCheckBox();
	showActiveCheckBox->setChecked(true);
	mainLayout->addWidget(showActiveCheckBox, 1, idx++);

	QPushButton *addButton = new QPushButton(obs_module_text("Add"));
	connect(addButton, &QPushButton::clicked, [this]() { AddClicked(); });
	mainLayout->addWidget(addButton, 1, idx++, Qt::AlignCenter);

	RefreshTable();

	QWidget *controlArea = new QWidget;
	controlArea->setLayout(mainLayout);
	controlArea->setSizePolicy(QSizePolicy::Preferred,
				   QSizePolicy::Preferred);

	QVBoxLayout *vlayout = new QVBoxLayout;
	vlayout->addWidget(controlArea);
	//vlayout->setAlignment(controlArea, Qt::AlignTop);
	QWidget *widget = new QWidget;
	widget->setLayout(vlayout);
	widget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);

	QScrollArea *scrollArea = new QScrollArea;
	scrollArea->setWidget(widget);
	scrollArea->setWidgetResizable(true);

	QPushButton *closeButton = new QPushButton(obs_module_text("Close"));
	QPushButton *deleteButton = new QPushButton(obs_module_text("Delete"));

	QHBoxLayout *bottomLayout = new QHBoxLayout;
	bottomLayout->addWidget(deleteButton, 0, Qt::AlignLeft);
	bottomLayout->addWidget(closeButton, 0, Qt::AlignRight);

	connect(deleteButton, &QPushButton::clicked,
		[this]() { DeleteClicked(); });
	connect(closeButton, &QPushButton::clicked, [this]() { close(); });

	vlayout = new QVBoxLayout;
	vlayout->setContentsMargins(11, 11, 11, 11);
	vlayout->addWidget(scrollArea);
	vlayout->addLayout(bottomLayout);
	setLayout(vlayout);

	setWindowTitle(obs_module_text("SourceDocks"));
	setSizeGripEnabled(true);

	setMinimumSize(200, 200);
}

SourceDockSettingsDialog::~SourceDockSettingsDialog() {}

void SourceDockSettingsDialog::AddClicked()
{
	const auto sn = sourceCombo->currentText();
	if (sn.isEmpty())
		return;
	const auto t2 = sn.toUtf8();
	const auto t3 = t2.constData();
	if (!t3 || !strlen(t3))
		return;
	auto title = titleEdit->text();
	if (title.isEmpty())
		title = sn;
	obs_source_t *source = obs_get_source_by_name(t3);
	if (!source)
		return;

	const auto main_window =
		static_cast<QMainWindow *>(obs_frontend_get_main_window());
	auto *tmp = new SourceDock(source, main_window);
	tmp->setWindowTitle(title);
	if (previewCheckBox->isChecked())
		tmp->EnablePreview();
	if (volMeterCheckBox->isChecked())
		tmp->EnableVolMeter();
	if (volControlsCheckBox->isChecked())
		tmp->EnableVolControls();
	if (mediaControlsCheckBox->isChecked())
		tmp->EnableMediaControls();
	if (switchSceneCheckBox->isChecked())
		tmp->EnableSwitchScene();
	if (showActiveCheckBox->isChecked())
		tmp->EnableShowActive();
	source_docks.push_back(tmp);
	tmp->show();
	obs_source_release(source);
	RefreshTable();
}

void SourceDockSettingsDialog::RefreshTable()
{
	for (auto row = mainLayout->rowCount() - 1; row >= 2; row--) {
		for (auto col = mainLayout->columnCount() - 1; col >= 0;
		     col--) {
			auto *item = mainLayout->itemAtPosition(row, col);
			if (item) {
				mainLayout->removeItem(item);
				delete item->widget();
				delete item;
			}
		}
	}
	auto row = 2;
	SourceDock *dock = nullptr;
	const auto sourceName = sourceCombo->currentText();
	const auto title = titleEdit->text();
	for (const auto &it : source_docks) {
		if (it->isHidden())
			it->show();
		if (!sourceName.isEmpty() &&
		    !QString::fromUtf8(obs_source_get_name(it->GetSource()))
			     .contains(sourceName, Qt::CaseInsensitive))
			continue;
		QString t = it->windowTitle();
		if (!title.isEmpty() && !t.contains(title, Qt::CaseInsensitive))
			continue;
		auto col = 0;
		auto *label = new QLabel(QString::fromUtf8(
			obs_source_get_name(it->GetSource())));
		mainLayout->addWidget(label, row, col++);

		label = new QLabel(t);
		mainLayout->addWidget(label, row, col++);

		dock = it;

		auto *checkBox = new QCheckBox;
		checkBox->setChecked(dock->PreviewEnabled());
		connect(checkBox, &QCheckBox::stateChanged, [checkBox, dock]() {
			if (checkBox->isChecked()) {
				dock->EnablePreview();
				if (!dock->PreviewEnabled())
					checkBox->setChecked(false);
			} else {
				dock->DisablePreview();
			}
		});
		mainLayout->addWidget(checkBox, row, col++);

		checkBox = new QCheckBox;
		checkBox->setChecked(dock->VolMeterEnabled());
		connect(checkBox, &QCheckBox::stateChanged, [checkBox, dock]() {
			if (checkBox->isChecked()) {
				dock->EnableVolMeter();
				if (!dock->VolMeterEnabled())
					checkBox->setChecked(false);
			} else {
				dock->DisableVolMeter();
			}
		});
		mainLayout->addWidget(checkBox, row, col++);

		checkBox = new QCheckBox;
		checkBox->setChecked(dock->VolControlsEnabled());
		connect(checkBox, &QCheckBox::stateChanged, [checkBox, dock]() {
			if (checkBox->isChecked()) {
				dock->EnableVolControls();
				if (!dock->VolControlsEnabled())
					checkBox->setChecked(false);
			} else {
				dock->DisableVolControls();
			}
		});
		mainLayout->addWidget(checkBox, row, col++);

		checkBox = new QCheckBox;
		checkBox->setChecked(dock->MediaControlsEnabled());
		connect(checkBox, &QCheckBox::stateChanged, [checkBox, dock]() {
			if (checkBox->isChecked()) {
				dock->EnableMediaControls();
				if (!dock->MediaControlsEnabled())
					checkBox->setChecked(false);
			} else {
				dock->DisableMediaControls();
			}
		});
		mainLayout->addWidget(checkBox, row, col++);

		checkBox = new QCheckBox;
		checkBox->setChecked(dock->SwitchSceneEnabled());
		connect(checkBox, &QCheckBox::stateChanged, [checkBox, dock]() {
			if (checkBox->isChecked()) {
				dock->EnableSwitchScene();
				if (!dock->SwitchSceneEnabled())
					checkBox->setChecked(false);
			} else {
				dock->DisableSwitchScene();
			}
		});
		mainLayout->addWidget(checkBox, row, col++);

		checkBox = new QCheckBox;
		checkBox->setChecked(dock->ShowActiveEnabled());
		connect(checkBox, &QCheckBox::stateChanged, [checkBox, dock]() {
			if (checkBox->isChecked()) {
				dock->EnableShowActive();
				if (!dock->ShowActiveEnabled())
					checkBox->setChecked(false);
			} else {
				dock->DisableShowActive();
			}
		});
		mainLayout->addWidget(checkBox, row, col++);

		checkBox = new QCheckBox;
		mainLayout->addWidget(checkBox, row, col++, Qt::AlignCenter);
		row++;
	}
}

void SourceDockSettingsDialog::mouseDoubleClickEvent(QMouseEvent *event)
{
	QWidget *widget = childAt(event->pos());
	if (!widget)
		return;
	int index = mainLayout->indexOf(widget);
	if (index < 0)
		return;

	int row, column, row_span, col_span;
	mainLayout->getItemPosition(index, &row, &column, &row_span, &col_span);
	if (row < 2)
		return;
	QLayoutItem *item = mainLayout->itemAtPosition(row, 0);
	if (!item)
		return;
	auto label = dynamic_cast<QLabel *>(item->widget());
	if (!label)
		return;
	const QString sourceName = label->text();
	if (sourceName.isEmpty())
		return;

	item = mainLayout->itemAtPosition(row, 0);
	if (!item)
		return;
	label = dynamic_cast<QLabel *>(item->widget());
	if (!label)
		return;
	const QString title = label->text();
	if (title.isEmpty())
		return;

	sourceCombo->setCurrentText(sourceName);
	titleEdit->setText(title);
}

void SourceDockSettingsDialog::DeleteClicked()
{
	for (auto row = 2; row < mainLayout->rowCount(); row++) {
		auto *item = mainLayout->itemAtPosition(row, selectBoxColumn);
		if (!item)
			continue;
		auto *checkBox = dynamic_cast<QCheckBox *>(item->widget());
		if (!checkBox || !checkBox->isChecked())
			continue;

		item = mainLayout->itemAtPosition(row, 0);
		auto *label = dynamic_cast<QLabel *>(item->widget());
		if (!label)
			continue;
		std::string sourceName = label->text().toUtf8().constData();
		item = mainLayout->itemAtPosition(row, 1);
		label = dynamic_cast<QLabel *>(item->widget());
		if (!label)
			continue;
		auto title = label->text();
		for (auto it = source_docks.begin();
		     it != source_docks.end();) {
			if ((*it)->windowTitle() != title) {
				++it;
				continue;
			}
			if (sourceName !=
			    obs_source_get_name((*it)->GetSource())) {
				++it;
				continue;
			}
			(*it)->close();
			(*it)->deleteLater();
			it = source_docks.erase(it);
		}
	}
	RefreshTable();
}

void SourceDockSettingsDialog::SelectAllChanged()
{
	auto *item = mainLayout->itemAtPosition(0, selectBoxColumn);
	auto *checkBox = dynamic_cast<QCheckBox *>(item->widget());
	bool checked = checkBox && checkBox->isChecked();
	for (auto row = 2; row < mainLayout->rowCount(); row++) {
		item = mainLayout->itemAtPosition(row, selectBoxColumn);
		if (!item)
			continue;
		auto *checkBox = dynamic_cast<QCheckBox *>(item->widget());
		if (!checkBox)
			continue;
		checkBox->setChecked(checked);
	}
}

bool SourceDockSettingsDialog::AddSource(void *data, obs_source_t *source)
{
	const char *sn = obs_source_get_name(source);
	auto sourceCombo = static_cast<QComboBox *>(data);
	sourceCombo->addItem(QT_UTF8(sn), QByteArray(sn));
	return true;
}

VerticalLabel::VerticalLabel(QWidget *parent) : QLabel(parent) {}

VerticalLabel::VerticalLabel(const QString &text, QWidget *parent)
	: QLabel(text, parent)
{
}

void VerticalLabel::paintEvent(QPaintEvent *)
{
	QPainter painter(this);
	//painter.setPen(Qt::black);
	//painter.setBrush(Qt::Dense1Pattern);

	painter.rotate(90);

	painter.drawText(0, 0, text());
}

QSize VerticalLabel::minimumSizeHint() const
{
	QSize s = QLabel::minimumSizeHint();
	return QSize(s.height(), s.width());
}

QSize VerticalLabel::sizeHint() const
{
	QSize s = QLabel::sizeHint();
	return QSize(s.height(), s.width());
}
