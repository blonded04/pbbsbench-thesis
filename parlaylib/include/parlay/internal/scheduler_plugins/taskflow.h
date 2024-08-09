#ifndef PARLAY_INTERNAL_SCHEDULER_PLUGINS_TASKFLOW_H_
#define PARLAY_INTERNAL_SCHEDULER_PLUGINS_TASKFLOW_H_

#include "taskflow/taskflow/taskflow.hpp"
#include "taskflow/taskflow/algorithm/for_each.hpp"
#include "eigen/modes.h"

namespace parlay {

inline tf::Executor& tfExecutor() {
    static tf::Executor exec;
    return exec;
}

inline size_t num_workers() {
    return tfExecutor().num_workers();
}

inline size_t worker_id() {
    return tfExecutor().this_worker_id();
}

template <typename F>
inline void parallel_for(size_t start, size_t end, F&& f, long granularity, bool) {
    tf::Taskflow tf;

#if TASKFLOW_MODE == TASKFLOW_GUIDED
    tf::GuidedPartitioner execution_policy(granularity);
#elif TASKFLOW_MODE == TASKFLOW_DYNAMIC
    tf::DynamicPartitioner execution_policy(granularity);
#elif TASKFLOW_MODE == TASKFLOW_STATIC
    tf::StaticPartitioner execution_policy(granularity);
#elif TASKFLOW_MODE == TASKFLOW_RANDOM
    tf::RandomPartitioner execution_policy(granularity);
#else
    static_assert(false, "Wrong TASKFLOW_MODE mode");
#endif  // TASKFLOW_MODE
    
    tf.for_each_index(start, end, static_cast<size_t>(1), std::forward<F>(f), execution_policy);

    tfExecutor().run(tf).wait();
}

template <typename Lf, typename Rf>
inline void par_do(Lf&& left, Rf&& right, bool) {
    tf::Taskflow tf;

    tf.emplace(std::forward<Lf>(left));
    auto fut = tfExecutor().run(tf);

    std::forward<Rf>(right)();

    fut.wait();
}

inline void init_plugin_internal() {}

template <typename... Fs>
void execute_with_scheduler(Fs...) {
    struct Illegal {};
    static_assert((std::is_same_v<Illegal, Fs> && ...), "parlay::execute_with_scheduler is only available in the Parlay scheduler and is not compatible with Taskflow");
}

}  // namespace parlay

#endif  // PARLAY_INTERNAL_SCHEDULER_PLUGINS_TASKFLOW_H_
