#include "selfdrive/ui/qt/widgets/map_helpers.h"

#include <QDebug>

QGeoCoordinate to_QGeoCoordinate(const QMapbox::Coordinate &in) {
  return QGeoCoordinate(in.first, in.second);
}

 QList<Eigen::Vector3d> coordinates_to_camera_view(
  const cereal::LiveLocationKalman::Measurement::Reader &calibratedOrientationECEF,
  const cereal::LiveLocationKalman::Measurement::Reader &positionECEF,
  const QList<QGeoCoordinate> &coordinates) {
  QList<Eigen::Vector3d> line;

  Eigen::Vector3d local_ecef(positionECEF.getValue()[0], positionECEF.getValue()[1], positionECEF.getValue()[2]);
  double alt = ecef2geodetic({.x = local_ecef[0], .y = local_ecef[1], .z = local_ecef[2]}).alt;

  Eigen::Vector3d orient(calibratedOrientationECEF.getValue()[0], calibratedOrientationECEF.getValue()[1], calibratedOrientationECEF.getValue()[2]);
  Eigen::Matrix3d local_from_ecef = euler2rot(orient);
  qDebug() << "";

  for (auto &coord : coordinates) {
    Geodetic g = {.lat = coord.latitude(), .lon = coord.longitude(), .alt = alt};
    ECEF e = geodetic2ecef(g);
    Eigen::Vector3d local = local_from_ecef * (e.to_vector() - local_ecef);
    if (local[0] < 5) {
        continue;
    }

    if (local[0] > 500) {
        break;
    }
    qDebug() << local[0] << local[1] << local[2];
    line.push_back(local);
  }

  return line;
}

QMapbox::CoordinatesCollections model_to_collection(
  const cereal::LiveLocationKalman::Measurement::Reader &calibratedOrientationECEF,
  const cereal::LiveLocationKalman::Measurement::Reader &positionECEF,
  const cereal::ModelDataV2::XYZTData::Reader &line){

  Eigen::Vector3d ecef(positionECEF.getValue()[0], positionECEF.getValue()[1], positionECEF.getValue()[2]);
  Eigen::Vector3d orient(calibratedOrientationECEF.getValue()[0], calibratedOrientationECEF.getValue()[1], calibratedOrientationECEF.getValue()[2]);
  Eigen::Matrix3d ecef_from_local = euler2rot(orient).transpose();

  QMapbox::Coordinates coordinates;
  auto x = line.getX();
  auto y = line.getY();
  auto z = line.getZ();
  for (int i = 0; i < x.size(); i++){
    Eigen::Vector3d point_ecef = ecef_from_local * Eigen::Vector3d(x[i], y[i], z[i]) + ecef;
    Geodetic point_geodetic = ecef2geodetic((ECEF){.x = point_ecef[0], .y = point_ecef[1], .z = point_ecef[2]});
    QMapbox::Coordinate coordinate(point_geodetic.lat, point_geodetic.lon);
    coordinates.push_back(coordinate);
  }

  QMapbox::CoordinatesCollection collection;
  collection.push_back(coordinates);

  QMapbox::CoordinatesCollections collections;
  collections.push_back(collection);
  return collections;
}

QMapbox::CoordinatesCollections coordinate_to_collection(QMapbox::Coordinate c){
  QMapbox::Coordinates coordinates;
  coordinates.push_back(c);

  QMapbox::CoordinatesCollection collection;
  collection.push_back(coordinates);

  QMapbox::CoordinatesCollections collections;
  collections.push_back(collection);
  return collections;
}

QMapbox::CoordinatesCollections coordinate_list_to_collection(QList<QGeoCoordinate> coordinate_list) {
  QMapbox::Coordinates coordinates;

  for (auto &c : coordinate_list){
    QMapbox::Coordinate coordinate(c.latitude(), c.longitude());
    coordinates.push_back(coordinate);
  }

  QMapbox::CoordinatesCollection collection;
  collection.push_back(coordinates);

  QMapbox::CoordinatesCollections collections;
  collections.push_back(collection);
  return collections;
}

static QGeoCoordinate sub(QGeoCoordinate v, QGeoCoordinate w){
  return QGeoCoordinate(v.latitude() - w.latitude(), v.longitude() - w.longitude());
}

static QGeoCoordinate add(QGeoCoordinate v, QGeoCoordinate w){
  return QGeoCoordinate(v.latitude() + w.latitude(), v.longitude() + w.longitude());
}

static QGeoCoordinate mul(QGeoCoordinate v, float c){
  return QGeoCoordinate(c * v.latitude(), c * v.longitude());
}

static float dot(QGeoCoordinate v, QGeoCoordinate w){
  return v.latitude() * w.latitude() + v.longitude() * w.longitude();
}

float minimum_distance(QGeoCoordinate a, QGeoCoordinate b, QGeoCoordinate p) {
  const QGeoCoordinate ap = sub(p, a);
  const QGeoCoordinate ab = sub(b, a);
  const float t = std::clamp(dot(ap, ab) / dot(ab, ab), 0.0f, 1.0f);
  const QGeoCoordinate projection = add(a, mul(ab, t));
  return projection.distanceTo(p);
}
