#pragma once

#include "../mainwindow.hpp"

#include <QIcon>
#include <QString>

class Icon
{
public:
	static QIcon get(const QString &name);
	static bool useFallbackIcons;

private:
	Icon() = default;
};