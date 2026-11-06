#pragma once

#include <QPainterPath>
#include <QString>

// Parses SVG path data ("M10 10 H90 a5 5 0 0 1 5 5 Z") into a QPainterPath.
// Supports M L H V C S Q T A Z (absolute and relative), including rotated arcs.
namespace SvgPath {

QPainterPath parse(const QString &data);

} // namespace SvgPath
