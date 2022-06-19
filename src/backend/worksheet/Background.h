/*
	File                 : Background.h
	Project              : LabPlot
	Description          : Background
	--------------------------------------------------------------------
	SPDX-FileCopyrightText: 202 Alexander Semke <alexander.semke@web.de>
	SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef BACKGROUND_H
#define BACKGROUND_H

#include "backend/core/AbstractAspect.h"
#include "backend/lib/macros.h"

class BackgroundPrivate;
class KConfigGroup;

class Background : public AbstractAspect {
	Q_OBJECT

public:
	enum class Type { Color, Image, Pattern };
	enum class ColorStyle {
		SingleColor,
		HorizontalLinearGradient,
		VerticalLinearGradient,
		TopLeftDiagonalLinearGradient,
		BottomLeftDiagonalLinearGradient,
		RadialGradient
	};
	enum class ImageStyle { ScaledCropped, Scaled, ScaledAspectRatio, Centered, Tiled, CenterTiled };

	explicit Background(const QString& name);
	~Background() override;

	void init(const KConfigGroup&);

	void save(QXmlStreamWriter*) const override;
	bool load(XmlStreamReader*, bool preview) override;
	void loadThemeConfig(const KConfigGroup&);
	void saveThemeConfig(const KConfigGroup&) const;

    BASIC_D_ACCESSOR_DECL(double, opacity, Opacity)
	BASIC_D_ACCESSOR_DECL(Background::Type, type, Type)
	BASIC_D_ACCESSOR_DECL(Background::ColorStyle, colorStyle, ColorStyle)
	BASIC_D_ACCESSOR_DECL(Background::ImageStyle, imageStyle, ImageStyle)
	BASIC_D_ACCESSOR_DECL(Qt::BrushStyle, brushStyle, BrushStyle)
	CLASS_D_ACCESSOR_DECL(QColor, firstColor, FirstColor)
	CLASS_D_ACCESSOR_DECL(QColor, secondColor, SecondColor)
	CLASS_D_ACCESSOR_DECL(QString, fileName, FileName)

	typedef BackgroundPrivate Private;

protected:
	BackgroundPrivate* const d_ptr;

private:
	Q_DECLARE_PRIVATE(Background)

Q_SIGNALS:
	void typeChanged(Background::Type);
	void colorStyleChanged(Background::ColorStyle);
	void imageStyleChanged(Background::ImageStyle);
	void brushStyleChanged(Qt::BrushStyle);
	void firstColorChanged(const QColor&);
	void secondColorChanged(const QColor&);
	void fileNameChanged(const QString&);
	void opacityChanged(float);

	void updateRequested();
};

#endif
