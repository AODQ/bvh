# bvh [![Build Status](https://travis-ci.org/madmann91/bvh.svg?branch=master)](https://travis-ci.org/madmann91/bvh)

This is a modern C++17 header-only BVH library optimized for ray-tracing. Traversal and
construction routines support different primitive types. The design is such that the
BVH holds no data and only holds nodes. There is no hardware- or platform-specific
intrinsic used. Parallelization is done using OpenMP. There is no dependency
except the C++ standard library.

## Performance

This library is carefully crafted to ensure good performance, while still
being simple, portable and high-level. These are performance results versus another
simple library available on GitHub, [brandonpelfrey/Fast-BVH](https://github.com/brandonpelfrey/Fast-BVH)
(top of the list for the search "bvh"):

|                   | brandonpelfrey/Fast-BVH | This library (fast build mode) | This library (slow build mode) |
|-------------------|-------------------------|--------------------------------|--------------------------------|
| BVH construction  |           25ms          |                18ms (-28%)     |              48ms (+92%)       |
| Rendering         |         3406ms          |              1047ms (-69%)     |             837ms (-75%)       |

Note that to get this performance, you have to set the compiler flags such that the correct
instruction set is used (`-march=native` under gcc/clang), and use the highest available optimization
level (`-O3` on gcc/clang).

In this benchmark, the fast math mode is _disabled_, but enabling it may provide additional
performance (to do this, use `-ffast-math` on gcc/clang). Note that enabling the fast math
mode is safe for this library, but it might produce different results in the rest of your code
base, particularly if you rely on a specific ordering of floating point operations.

These numbers where generated by taking the benchmark code from brandonpelfrey/Fast-BVH
and porting it to this library. Instead of using random spheres, the algorithm has been
modified to load an OBJ file and render triangles colored by their normal.
The scene used for this table is Sponza. The machine used is an AMD Threadripper 2950X
with 16 physical cores and 32 threads. Both libraries use OpenMP and run on multiple cores
(except that the construction algorithm in Fast-BVH is not multithreaded, unlike the one
in this library). The rendering resolution is 8Kx8K.

The results are not surprising as the BVH used in Fast-BVH is using the middle split technique
which is known to be seriously bad for anything but small examples or uniformly distributed
primitives. The SSE3 routines of Fast-BVH are also not better than the code generated by
the compiler (gcc 8.3.1 in this example).

Compared to [Embree](https://github.com/embree/embree), the ray-tracing kernels from Intel, this
library only supports single-ray tracing and binary BVHs. As a result, it cannot be as fast as the
stream or packet traversal algorithms of Embree. Compared to the single-ray kernels of Embree running on the
high-quality BVHs also built by Embree, this library is around 50% slower (20-30% with the post-build
optimization enabled), but the actual number can vary depending on the type of ray (e.g. coherent/incoherent)
and the scene. To match that level of performance, this library would have to implement:

  - Higher-arity BVHs (BVH4 or BVH8, with more than just two children per node),
  - Vectorization of the traversal and intersection routines.

Wider BVHs and vectorization go against the principles of simplicity and portability followed by this library
and will most likely never be implemented. Instead, algorithmic improvements such as post-build optimizations
will be investigated, as long as they offer a good compromise between implementation complexity and performance
improvements.

## Detailed Description

Since there are various algorithms for BVH traversal and construction, this library provides
several options that can be used to target real-time, interactive, or offline rendering.

### Construction Algorithms

This library contains several construction algorithms, all parallelized using OpenMP.

 - `bvh::BinnedSahBuilder`: Top-down builder using binning to compute the SAH (see
   _On fast Construction of SAH-based Bounding Volume Hierarchies_, by I. Wald). Relatively fast
   and produces medium- to high-quality trees. Can be configured by setting the number of bins.
 - `bvh::SweepSahBuilder`: Top-down builder that sorts primitives on all axes and sweeps them
   to find the split with the lowest SAH cost. Relatively slow but produces high-quality trees.
 - `bvh::LocallyOrderedClusteringBuilder`: Bottom-up builder that produces trees by sorting
   primitives on a Morton curve and performing a local search to merge them into nodes (see
   _Parallel Locally-Ordered Clustering for Bounding Volume Hierarchy Construction_,
   by D. Meister and J. Bittner). Very fast algorithm but produces medium-quality trees only.
   The search radius can be configured to change the quality/speed ratio. Additionally,
   the integer type used to compute Morton codes can also be changed when more precision
   is required.
 - `bvh::LinearBvhBuilder`: Bottom-up LBVH builder. Very fast and produces low-quality trees.
   Not substantially faster than `bvh::LocallyOrderedClusteringBuilder`, particularly when the
   search radius of that algorithm is small. Since the BVHs built by this algorithm are significantly
   worse than those produced by other algorithms, do not use this builder unless you absolutely
   need the fastest construction algorithm.
 - `bvh::SpatialSplitBvhBuilder`: Top-down SBVH builder that splits primitives to minimize overlap
   between nodes. Based on the article _Spatial Splits in Bounding Volume Hierarchies_, by M. Stich et al.
   Produces very high-quality trees, at the cost of very slow BVH builds. Use for offline rendering.
   Although it is possible to disable spatial splits and only perform object splits with this builder,
   prefer `bvh::SweepSahBuilder` for this as, unlike this builder, it does not need to sort references
   at every step.

Those algorithms only require a bounding box and center for each primitive.

### Optimization/Splitting Algorithms

Additionally, the BVH structure can be further improved by running post-build optimizations,
or pre-build triangle splitting.

 - `bvh::ParallelReinsertionOptimization`: An optimization that tries to re-insert BVH nodes
   in a way that minimizes the SAH (see _Parallel Reinsertion for Bounding Volume Hierarchy Optimization_,
   by D. Meister and J. Bittner). This can lead up to a 20% improvement in trace performance,
   at the cost of longer build times.
 - `bvh::HeuristicPrimitiveSplitting`: A pre-splitting algorithm that splits primitives at regular
   positions, inspired by _Fast Parallel Construction of High-Quality Bounding Volume Hierarchies_,
   by T. Karras and T. Aila. Works well in combination with `bvh::LinearBvhBuilder`, but may
   decrease performance for other builders on some scenes. Takes a budget of primitives to split,
   and distributes it to each primitive based on a priority heuristic.

### Traversal Algorithms

This library provides the following traversal algorithms:

 - `bvh::SingleRayTraversal`: A traversal algorithm optimized for single rays.
    Rays are classified by octant, to make the ray-box test more efficient. The
    traversal order is such that the closest node is taken first. The ray-box
    test does not use divisions, and uses FMA instructions when possible.
    
Traversal algorithms can work in two modes: closest intersection,
or any intersection (for shadow rays, usually around 20% faster).
They only require an intersector to compute primitive-ray intersections.

### Intersectors

Intersectors contained in the libray support linear collections of primitives (i.e. arrays/vectors),
and allow permuting the primitive data in such a way that no indirection is done during traversal.
Custom intersectors can be introduced when the primitive data is in another form.

## Building

There is no need to build anything, since this library is header-only.
To build the tests, type:

    mkdir build
    cd build
    cmake ..
    cmake --build .

## Installing

In order to use this library in an existing project using CMake, you can clone, add as submodule, or
even copy the source tree into some subdirectory of your project, say `my-project/contrib/bvh`.
Then, instruct CMake to visit this directory and link against the `bvh` target:

    add_subdirectory(contrib/bvh)
    target_link_libraries(my-project PUBLIC bvh)

That's all that you need to do. Dependencies will be automatically added (e.g. OpenMP, if available).

## Usage

For a basic example of how to use the API, see [this simple example](test/simple_example.cpp).
If you need to library to read your primitive data from another source (if you want to avoid
copying existing primitive data, for instance), take a look at [this example](test/custom_intersector.cpp).
Finally, if triangles are not enough for you, refer to [this file](test/custom_primitive.cpp)
in order to understand how to implement your own primitive type so that it is compatible
with the rest of the API.

## License

This library is distributed under the [MIT](LICENSE.txt) license.
