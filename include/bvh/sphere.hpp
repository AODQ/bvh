#ifndef BVH_SPHERE_HPP
#define BVH_SPHERE_HPP

#include <optional>

#include "bvh/vector.hpp"
#include "bvh/bounding_box.hpp"
#include "bvh/ray.hpp"

namespace bvh {

/// Sphere primitive defined by a center and a radius.
template <typename Scalar>
struct Sphere  {
    struct Intersection {
        Scalar  t;

        Scalar distance() const { return t; }
    };

    using ScalarType       = Scalar;
    using IntersectionType = Intersection;

    Vector3<Scalar> origin;
    Scalar radius;

    Sphere() = default;
    Sphere(const Vector3<Scalar>& origin, Scalar radius)
        : origin(origin), radius(radius)
    {}

    Vector3<Scalar> center() const {
        return origin;
    }

    BoundingBox<Scalar> bbox() const {
        return BoundingBox<Scalar>(origin - Vector3<Scalar>(radius), origin + Vector3<Scalar>(radius));
    }

    std::optional<Intersection> intersect(const Ray<Scalar>& ray) const {
        auto oc = ray.origin - origin;
        auto a = dot(ray.direction, ray.direction);
        auto b = 2 * dot(ray.direction, oc);
        auto c = dot(oc, oc) - radius * radius;

        auto delta = b * b - 4 * a * c;
        if (delta >= 0) {
            auto inv = Scalar(0.5) / a;
            auto t0 = -(b + std::sqrt(delta)) * inv;
            auto t1 = -(b - std::sqrt(delta)) * inv;
            auto t = std::fmin(t0 > ray.tmin ? t0 : t1, t1 > ray.tmin ? t1 : t0);
            if (t > ray.tmin && t < ray.tmax)
                return std::make_optional(Intersection(t));
        }

        return std::nullopt;
    }
};

} // namespace bvh

#endif