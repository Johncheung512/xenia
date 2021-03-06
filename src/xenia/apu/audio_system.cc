/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2013 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#include "xenia/apu/audio_system.h"

#include "xenia/apu/audio_driver.h"
#include "xenia/base/logging.h"
#include "xenia/base/math.h"
#include "xenia/base/ring_buffer.h"
#include "xenia/base/string_buffer.h"
#include "xenia/cpu/processor.h"
#include "xenia/cpu/thread_state.h"
#include "xenia/emulator.h"
#include "xenia/kernel/objects/xthread.h"
#include "xenia/profiling.h"

// As with normal Microsoft, there are like twelve different ways to access
// the audio APIs. Early games use XMA*() methods almost exclusively to touch
// decoders. Later games use XAudio*() and direct memory writes to the XMA
// structures (as opposed to the XMA* calls), meaning that we have to support
// both.
//
// For ease of implementation, most audio related processing is handled in
// AudioSystem, and the functions here call off to it.
// The XMA*() functions just manipulate the audio system in the guest context
// and let the normal AudioSystem handling take it, to prevent duplicate
// implementations. They can be found in xboxkrnl_audio_xma.cc

namespace xe {
namespace apu {

using namespace xe::cpu;

AudioSystem::AudioSystem(Emulator* emulator)
    : emulator_(emulator),
      memory_(emulator->memory()),
      worker_running_(false) {
  std::memset(clients_, 0, sizeof(clients_));
  for (size_t i = 0; i < kMaximumClientCount; ++i) {
    unused_clients_.push(i);
  }
  for (size_t i = 0; i < kMaximumClientCount; ++i) {
    client_semaphores_[i] = CreateSemaphore(NULL, 0, kMaximumQueuedFrames, NULL);
    wait_handles_[i] = client_semaphores_[i];
  }
  shutdown_event_ = CreateEvent(NULL, TRUE, FALSE, NULL);
  wait_handles_[kMaximumClientCount] = shutdown_event_;
}

AudioSystem::~AudioSystem() {
  for (size_t i = 0; i < kMaximumClientCount; ++i) {
    CloseHandle(client_semaphores_[i]);
  }
  CloseHandle(shutdown_event_);
}

X_STATUS AudioSystem::Setup() {
  processor_ = emulator_->processor();

  worker_running_ = true;
  worker_thread_ =
      kernel::object_ref<kernel::XHostThread>(new kernel::XHostThread(
          emulator()->kernel_state(), 128 * 1024, 0, [this]() {
            WorkerThreadMain();
            return 0;
          }));
  worker_thread_->set_name("Audio Worker");
  worker_thread_->Create();

  return X_STATUS_SUCCESS;
}

void AudioSystem::WorkerThreadMain() {
  // Initialize driver and ringbuffer.
  Initialize();

  auto processor = emulator_->processor();

  // Main run loop.
  while (worker_running_) {
    auto result =
        WaitForMultipleObjectsEx(DWORD(xe::countof(wait_handles_)),
                                 wait_handles_, FALSE, INFINITE, FALSE);
    if (result == WAIT_FAILED ||
        result == WAIT_OBJECT_0 + kMaximumClientCount) {
      continue;
    }

    size_t pumped = 0;
    if (result >= WAIT_OBJECT_0 &&
        result <= WAIT_OBJECT_0 + (kMaximumClientCount - 1)) {
      size_t index = result - WAIT_OBJECT_0;
      do {
        lock_.lock();
        uint32_t client_callback = clients_[index].callback;
        uint32_t client_callback_arg = clients_[index].wrapped_callback_arg;
        lock_.unlock();

        if (client_callback) {
          SCOPE_profile_cpu_i("apu", "xe::apu::AudioSystem->client_callback");
          uint64_t args[] = {client_callback_arg};
          processor->Execute(worker_thread_->thread_state(), client_callback,
                             args, xe::countof(args));
        }
        pumped++;
        index++;
      } while (index < kMaximumClientCount &&
               WaitForSingleObject(client_semaphores_[index], 0) ==
                   WAIT_OBJECT_0);
    }

    if (!worker_running_) {
      break;
    }

    if (!pumped) {
      SCOPE_profile_cpu_i("apu", "Sleep");
      Sleep(500);
    }
  }
  worker_running_ = false;

  // TODO(benvanik): call module API to kill?
}

void AudioSystem::Initialize() {}

void AudioSystem::Shutdown() {
  worker_running_ = false;
  SetEvent(shutdown_event_);
  worker_thread_->Wait(0, 0, 0, nullptr);
  worker_thread_.reset();
}

X_STATUS AudioSystem::RegisterClient(uint32_t callback, uint32_t callback_arg,
                                     size_t* out_index) {
  assert_true(unused_clients_.size());
  std::lock_guard<xe::mutex> lock(lock_);

  auto index = unused_clients_.front();

  auto client_semaphore = client_semaphores_[index];
  BOOL ret = ReleaseSemaphore(client_semaphore, kMaximumQueuedFrames, NULL);
  assert_true(ret == TRUE);

  AudioDriver* driver;
  auto result = CreateDriver(index, client_semaphore, &driver);
  if (XFAILED(result)) {
    return result;
  }
  assert_not_null(driver);

  unused_clients_.pop();

  uint32_t ptr = memory()->SystemHeapAlloc(0x4);
  xe::store_and_swap<uint32_t>(memory()->TranslateVirtual(ptr), callback_arg);

  clients_[index] = {driver, callback, callback_arg, ptr};

  if (out_index) {
    *out_index = index;
  }

  return X_STATUS_SUCCESS;
}

void AudioSystem::SubmitFrame(size_t index, uint32_t samples_ptr) {
  SCOPE_profile_cpu_f("apu");

  std::lock_guard<xe::mutex> lock(lock_);
  assert_true(index < kMaximumClientCount);
  assert_true(clients_[index].driver != NULL);
  (clients_[index].driver)->SubmitFrame(samples_ptr);
}

void AudioSystem::UnregisterClient(size_t index) {
  SCOPE_profile_cpu_f("apu");

  std::lock_guard<xe::mutex> lock(lock_);
  assert_true(index < kMaximumClientCount);
  DestroyDriver(clients_[index].driver);
  clients_[index] = {0};
  unused_clients_.push(index);

  // drain the semaphore of its count
  auto client_semaphore = client_semaphores_[index];
  DWORD wait_result;
  do {
    wait_result = WaitForSingleObject(client_semaphore, 0);
  } while (wait_result == WAIT_OBJECT_0);
  assert_true(wait_result == WAIT_TIMEOUT);
}

}  // namespace apu
}  // namespace xe
