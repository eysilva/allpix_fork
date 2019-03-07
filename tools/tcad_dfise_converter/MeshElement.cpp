#include "MeshElement.hpp"

#include "core/utils/log.h"

#define MIN_VOLUME 1e-12

using namespace mesh_converter;

double MeshElement::getVolume() const {
    return volume_;
}

void MeshElement::calculate_volume() {
    if(dimension_ == 3) {
        Eigen::Matrix4d element_matrix;
        element_matrix << 1, 1, 1, 1, vertices_[0].x, vertices_[1].x, vertices_[2].x, vertices_[3].x, vertices_[0].y,
            vertices_[1].y, vertices_[2].y, vertices_[3].y, vertices_[0].z, vertices_[1].z, vertices_[2].z, vertices_[3].z;
        volume_ = (element_matrix.determinant()) / 6;
    }
    if(dimension_ == 2) {
        Eigen::Matrix3d element_matrix;
        element_matrix << 1, 1, 1, vertices_[0].y, vertices_[1].y, vertices_[2].y, vertices_[0].z, vertices_[1].z,
            vertices_[2].z;
        volume_ = (element_matrix.determinant()) / 2;
    }
}

double MeshElement::get_distance(size_t index, Point& qp) const {
    return unibn::L2Distance<Point>::compute(vertices_[index], qp);
}

bool MeshElement::validElement(double volume_cut, Point& qp) const {
    if(volume_ < MIN_VOLUME) {
        LOG(TRACE) << "Invalid tetrahedron with coplanar(3D)/colinear(2D) vertices.";
        return false;
    }
    if(std::fabs(volume_) <= volume_cut) {
        LOG(TRACE) << "Tetrahedron volume smaller than volume cut.";
        return false;
    }

    for(size_t i = 0; i < dimension_ + 1; i++) {
        std::vector<Point> sub_vertices = vertices_;
        sub_vertices[i] = qp;
        MeshElement sub_tetrahedron(dimension_, sub_vertices);
        double tetra_volume = sub_tetrahedron.getVolume();
        if(volume_ * tetra_volume >= 0) {
            continue;
        }
        if(volume_ * tetra_volume < 0) {
            LOG(TRACE) << "New mesh Point outside found element.";
            return false;
        }
    }
    return true;
}

Point MeshElement::getObservable(Point& qp) const {
    Point new_observable;
    for(size_t index = 0; index < dimension_ + 1; index++) {
        auto sub_vertices = vertices_;
        sub_vertices[index] = qp;
        MeshElement sub_tetrahedron(dimension_, sub_vertices);
        double sub_volume = sub_tetrahedron.getVolume();
        LOG(DEBUG) << "Sub volume " << index << ": " << sub_volume;
        new_observable.x = new_observable.x + (sub_volume * e_field_[index].x) / volume_;
        new_observable.y = new_observable.y + (sub_volume * e_field_[index].y) / volume_;
        new_observable.z = new_observable.z + (sub_volume * e_field_[index].z) / volume_;
    }
    LOG(DEBUG) << "Interpolated electric field: (" << new_observable.x << "," << new_observable.y << "," << new_observable.z
               << ")";
    return new_observable;
}

std::string MeshElement::print(Point& qp) const {
    std::stringstream stream;
    for(size_t index = 0; index < dimension_ + 1; index++) {
        stream << "Tetrahedron vertex (" << vertices_[index].x << ", " << vertices_[index].y << ", " << vertices_[index].z
               << ") - "
               << " Distance: " << get_distance(index, qp) << " - Electric field: (" << e_field_[index].x << ", "
               << e_field_[index].y << ", " << e_field_[index].z << ")" << std::endl;
    }
    stream << "Volume: " << volume_;
    return stream.str();
}
