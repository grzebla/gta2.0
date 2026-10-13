# Known Issues

Nonblocking follow-up: WorldMesh::append_face reserves exactly the next required size on every append, which can cause quadratic allocation/copy work. Assess geometric capacity growth or batch reservation before map-scale assembly; T0013 specifies no performance threshold.
