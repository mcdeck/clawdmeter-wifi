#pragma once
// Stub: the fork has no perf profiler. face.cpp and performer.cpp include this.
enum PerfWindow { PERF_FACE_RISE, PERF_FACE_SETTLE, PERF_FACE_DROP,
                  PERF_PERFORMER_WALK, PERF_PERFORMER_SCENE };
#define perf_begin(w) ((void)(w))
#define perf_end(w)   ((void)(w))
