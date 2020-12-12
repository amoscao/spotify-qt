#pragma once

class VolumeButton;

#include "lib/spotify/clienthandler.hpp"
#include "util/icon.hpp"

#include <QMenu>
#include <QSlider>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWheelEvent>

class VolumeButton: public QToolButton
{
Q_OBJECT

public:
	VolumeButton(lib::Settings &settings, spt::Spotify &spotify, QWidget *parent);
	~VolumeButton();

	void updateIcon();
	void setVolume(int value);

protected:
	void wheelEvent(QWheelEvent *event) override;

private:
	QSlider *volume;
	lib::Settings &settings;
	spt::Spotify &spotify;
};