#include "media-slider.hpp"

#include <QStyleFactory>

#include "../../slider-absoluteset-style.hpp"


MediaSlider::MediaSlider(QWidget *parent) : QSlider(parent)
{
	setMouseTracking(true);

	QString styleName = style()->objectName();
	QStyle *style;
	style = QStyleFactory::create(styleName);
	if (!style) {
		style = new SliderAbsoluteSetStyle();
	} else {
		style = new SliderAbsoluteSetStyle(style);
	}

	style->setParent(this);
	this->setStyle(style);
}

void MediaSlider::mouseMoveEvent(QMouseEvent *event)
{
	int val = minimum() + ((maximum() - minimum()) * event->x()) / width();

	if (val > maximum())
		val = maximum();
	else if (val < minimum())
		val = minimum();

	emit mediaSliderHovered(val);
	event->accept();
	QSlider::mouseMoveEvent(event);
}

void MediaSlider::wheelEvent(QWheelEvent *event)
{
	if (!hasFocus())
		event->ignore();
	else
		QSlider::wheelEvent(event);
}
