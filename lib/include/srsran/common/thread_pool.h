/**
 *
 * \section COPYRIGHT
 *
 * Copyright 2013-2021 Software Radio Systems Limited
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the distribution.
 *
 */

/******************************************************************************
 *  File:         thread_pool.h
 *  Description:  Implements a pool of threads. Pending tasks to execute are
 *                identified by a pointer.
 *  Reference:
 *****************************************************************************/

#ifndef SRSRAN_THREAD_POOL_H
#define SRSRAN_THREAD_POOL_H

#include "srsran/adt/move_callback.h"
#include "srsran/srslog/srslog.h"
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <stack>
#include <stdint.h>
#include <string>
#include <vector>

#include "srsran/common/threads.h"

namespace srsran {

class thread_pool
{
public:
  class worker : public thread
  {
  public:
    worker();
    ~worker() = default;
    void     setup(uint32_t id, thread_pool* parent, uint32_t prio = 0, uint32_t mask = 255);
    uint32_t get_id();
    void     release();

  protected:
    virtual void work_imp() = 0;

  private:
    uint32_t     my_id     = 0;
    thread_pool* my_parent = nullptr;

    void run_thread();
    void wait_to_start();
    void finished();
  };

  thread_pool(uint32_t nof_workers);
  void     init_worker(uint32_t id, worker*, uint32_t prio = 0, uint32_t mask = 255);
  void     stop();
  worker*  wait_worker_id(uint32_t id);
  worker*  wait_worker(uint32_t tti);
  worker*  wait_worker_nb(uint32_t tti);
  void     start_worker(worker*);
  void     start_worker(uint32_t id);
  worker*  get_worker(uint32_t id);
  uint32_t get_nof_workers();

private:
  bool find_finished_worker(uint32_t tti, uint32_t* id);

  typedef enum { STOP, IDLE, START_WORK, WORKER_READY, WORKING } worker_status;

  std::vector<worker*>                 workers     = {};
  uint32_t                             nof_workers = 0;
  uint32_t                             max_workers = 0;
  bool                                 running     = false;
  std::condition_variable              cvar_queue  = {};
  std::mutex                           mutex_queue = {};
  std::vector<worker_status>           status      = {};
  std::vector<std::condition_variable> cvar_worker = {};
};

class task_thread_pool
{
  using task_t = srsran::move_callback<void()>;

public:
  task_thread_pool(uint32_t nof_workers = 1, bool start_deferred = false, int32_t prio_ = -1, uint32_t mask_ = 255);
  task_thread_pool(const task_thread_pool&) = delete;
  task_thread_pool(task_thread_pool&&)      = delete;
  task_thread_pool& operator=(const task_thread_pool&) = delete;
  task_thread_pool& operator=(task_thread_pool&&) = delete;
  ~task_thread_pool();

  void stop();
  void start(int32_t prio_ = -1, uint32_t mask_ = 255);
  void set_nof_workers(uint32_t nof_workers);

  void     push_task(task_t&& task);
  uint32_t nof_pending_tasks() const;
  size_t   nof_workers() const { return workers.size(); }

private:
  class worker_t : public thread
  {
  public:
    explicit worker_t(task_thread_pool* parent_, uint32_t id);
    void     stop();
    bool     is_running() const { return running; }
    uint32_t id() const { return id_; }

    void run_thread() override;

  private:
    bool wait_task(task_t* task);

    task_thread_pool* parent  = nullptr;
    uint32_t          id_     = 0;
    bool              running = false;
  };

  int32_t               prio = -1;
  uint32_t              mask = 255;
  srslog::basic_logger& logger;

  std::queue<task_t>                      pending_tasks;
  std::vector<std::unique_ptr<worker_t> > workers;
  mutable std::mutex                      queue_mutex;
  std::condition_variable                 cv_empty;
  bool                                    running = false;
};

srsran::task_thread_pool& get_background_workers();

} // namespace srsran

#endif // SRSRAN_THREAD_POOL_H