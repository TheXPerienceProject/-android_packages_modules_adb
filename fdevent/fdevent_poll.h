#pragma once

/*
 * Copyright (C) 2019 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "sysdeps.h"

#include <deque>
#include <list>
#include <mutex>
#include <unordered_map>

#include <android-base/thread_annotations.h>

#include "fdevent.h"

struct PollNode {
  fdevent* fde;
  adb_pollfd pollfd;

  explicit PollNode(fdevent* fde) : fde(fde) {
      memset(&pollfd, 0, sizeof(pollfd));
      pollfd.fd = fde->fd.get();

#if defined(__linux__)
      // Always enable POLLRDHUP, so the host server can take action when some clients disconnect.
      // Then we can avoid leaving many sockets in CLOSE_WAIT state. See http://b/23314034.
      pollfd.events = POLLRDHUP;
#endif
  }
};

struct fdevent_context_poll : public fdevent_context {
    virtual ~fdevent_context_poll() = default;

    virtual fdevent* Create(unique_fd fd, std::variant<fd_func, fd_func2> func, void* arg) final;
    virtual unique_fd Destroy(fdevent* fde) final;

    virtual void Set(fdevent* fde, unsigned events) final;
    virtual void Add(fdevent* fde, unsigned events) final;
    virtual void Del(fdevent* fde, unsigned events) final;
    virtual void SetTimeout(fdevent* fde, std::optional<std::chrono::milliseconds> timeout) final;

    virtual void Loop() final;

    virtual void CheckMainThread() final;

    virtual void Run(std::function<void()> fn) final;

    virtual void TerminateLoop() final;
    virtual size_t InstalledCount() final;

    // All operations to fdevent should happen only in the main thread.
    // That's why we don't need a lock for fdevent.
    std::unordered_map<int, PollNode> poll_node_map_;
    std::list<fdevent*> pending_list_;
    bool main_thread_valid_ = false;
    uint64_t main_thread_id_ = 0;
    uint64_t fdevent_id_ = 0;

    bool run_needs_flush_ = false;
    unique_fd run_queue_notify_fd_;
    std::mutex run_queue_mutex_;
    std::deque<std::function<void()>> run_queue_ GUARDED_BY(run_queue_mutex_);

    std::atomic<bool> terminate_loop_ = false;
};
