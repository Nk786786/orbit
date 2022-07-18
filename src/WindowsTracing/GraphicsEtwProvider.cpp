// Copyright (c) 2022 The Orbit Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "GraphicsEtwProvider.h"

// clang-format off
#include <guiddef.h>
#include <stdint.h>
// clang-format on

#include <absl/functional/bind_front.h>

#include <PresentData/ETW/Microsoft_Windows_D3D9.h>
#include <PresentData/ETW/Microsoft_Windows_DXGI.h>
#include <PresentData/ETW/Microsoft_Windows_Dwm_Core.h>
#include <PresentData/ETW/Microsoft_Windows_DxgKrnl.h>
#include <PresentData/ETW/Microsoft_Windows_EventMetadata.h>
#include <PresentData/ETW/Microsoft_Windows_Win32k.h>
#include <PresentData/ETW/NT_Process.h>

// clang-format off
#include <d3d9.h>
#include <dxgi.h>
// clang-format on

#include "OrbitBase/Logging.h"
#include "OrbitBase/Profiling.h"

namespace orbit_windows_tracing {

GraphicsEtwProvider::GraphicsEtwProvider(uint32_t pid, krabs::user_trace* trace,
                                         TracerListener* listener)
    : target_pid_(pid), trace_(trace), listener_(listener) {
  EnableProvider("Dxgi", Microsoft_Windows_DXGI::GUID,
                 absl::bind_front(&GraphicsEtwProvider::OnDXGIEvent, this));
  EnableProvider("D3d9", Microsoft_Windows_D3D9::GUID,
                 absl::bind_front(&GraphicsEtwProvider::OnD3d9Event, this));
  EnableProvider("DwmCore", Microsoft_Windows_Dwm_Core::GUID,
                 absl::bind_front(&GraphicsEtwProvider::OnDwmCoreEvent, this));
  EnableProvider("DwmCoreWin7", Microsoft_Windows_Dwm_Core::GUID,
                 absl::bind_front(&GraphicsEtwProvider::OnDwmCoreWin7Event, this));
  EnableProvider("DxgKrnl", Microsoft_Windows_DxgKrnl::GUID,
                 absl::bind_front(&GraphicsEtwProvider::OnDxgKrnlEvent, this));
  EnableProvider("DxgKrnlWin7Pres", Microsoft_Windows_DxgKrnl::Win7::PRESENTHISTORY_GUID,
                 absl::bind_front(&GraphicsEtwProvider::OnDxgKrnlWin7PresEvent, this));
  EnableProvider("NtProcess", NT_Process::GUID,
                 absl::bind_front(&GraphicsEtwProvider::OnNtProcessEvent, this));
  EnableProvider("WindowsEventMetadata", Microsoft_Windows_EventMetadata::GUID,
                 absl::bind_front(&GraphicsEtwProvider::OnWindowsEventMetadata, this));
  EnableProvider("Win32K", Microsoft_Windows_Win32k::GUID,
                 absl::bind_front(&GraphicsEtwProvider::OnWin32KEvent, this));
}

void GraphicsEtwProvider::EnableProvider(std::string_view name, GUID guid,
                                         krabs::provider_event_callback callback) {
  ORBIT_CHECK(!name_to_provider_.contains(name));
  name_to_provider_[name] = std::make_unique<Provider>(name, guid, target_pid_, trace_, callback);
}

// The "On[]Event" methods below are based on Intel's PresentMon project, see:
// https://github.com/GameTechDev/PresentMon/blob/main/PresentData/PresentMonTraceConsumer.cpp.
//
// The OnPresentStart/OnPresentStop methods will be added once
// https://github.com/google/orbit/pull/3934 is merged, see
// https://github.com/google/orbit/pull/3790 for reference implementation.

void GraphicsEtwProvider::OnDXGIEvent(const EVENT_RECORD& record,
                                      const krabs::trace_context& context) {
  krabs::schema schema(record, context.schema_locator);
  krabs::parser parser(schema);

  switch (record.EventHeader.EventDescriptor.Id) {
    case Microsoft_Windows_DXGI::Present_Start::Id:
    case Microsoft_Windows_DXGI::PresentMultiplaneOverlay_Start::Id: {
      auto swap_chain = parser.parse<uint64_t>(L"pIDXGISwapChain");
      auto flags = parser.parse<uint32_t>(L"Flags");
      auto sync_interval = parser.parse<uint32_t>(L"SyncInterval");
      // OnPresentStart(PresentSource::kDxgi, record.EventHeader, swap_chain, flags, sync_interval);
      break;
    }
    case Microsoft_Windows_DXGI::Present_Stop::Id:
    case Microsoft_Windows_DXGI::PresentMultiplaneOverlay_Stop::Id:
      // OnPresentStop(PresentSource::kDxgi, record.EventHeader, parser.parse<uint32_t>(L"Result"));
      break;
    default:
      break;
  }
}

void GraphicsEtwProvider::OnD3d9Event(const EVENT_RECORD& record,
                                      const krabs::trace_context& context) {
  krabs::schema schema(record, context.schema_locator);
  krabs::parser parser(schema);

  switch (record.EventHeader.EventDescriptor.Id) {
    case Microsoft_Windows_D3D9::Present_Start::Id: {
      auto swap_chain = parser.parse<uint64_t>(L"pSwapchain");
      auto flags = parser.parse<uint32_t>(L"Flags");

      uint32_t dxgi_present_flags = 0;
      if (flags & D3DPRESENT_DONOTFLIP) dxgi_present_flags |= DXGI_PRESENT_DO_NOT_SEQUENCE;
      if (flags & D3DPRESENT_DONOTWAIT) dxgi_present_flags |= DXGI_PRESENT_DO_NOT_WAIT;
      if (flags & D3DPRESENT_FLIPRESTART) dxgi_present_flags |= DXGI_PRESENT_RESTART;

      int32_t sync_interval = -1;
      if (flags & D3DPRESENT_FORCEIMMEDIATE) {
        sync_interval = 0;
      }

      // OnPresentStart(PresentSource::kD3d9, record.EventHeader, swap_chain, flags, sync_interval);
      break;
    }
    case Microsoft_Windows_D3D9::Present_Stop::Id:
      // OnPresentStop(PresentSource::kD3d9, record.EventHeader, parser.parse<uint32_t>(L"Result"));
      break;
    default:
      break;
  }
}

void GraphicsEtwProvider::OnDwmCoreEvent(const EVENT_RECORD& record,
                                         const krabs::trace_context& context) {}

void GraphicsEtwProvider::OnDwmCoreWin7Event(const EVENT_RECORD& record,
                                             const krabs::trace_context& context) {}

void GraphicsEtwProvider::OnDxgKrnlEvent(const EVENT_RECORD& record,
                                         const krabs::trace_context& context) {}

void GraphicsEtwProvider::OnDxgKrnlWin7PresEvent(const EVENT_RECORD& record,
                                                 const krabs::trace_context& context) {}

void GraphicsEtwProvider::OnNtProcessEvent(const EVENT_RECORD& record,
                                           const krabs::trace_context& context) {}

void GraphicsEtwProvider::OnWindowsEventMetadata(const EVENT_RECORD& record,
                                                 const krabs::trace_context& context) {}

void GraphicsEtwProvider::OnWin32KEvent(const EVENT_RECORD& record,
                                        const krabs::trace_context& context) {}

void GraphicsEtwProvider::OutputStats() {
  ORBIT_LOG("--- GraphicsEtwProvider stats ---");
  for (const auto& [unused_name, provider] : name_to_provider_) {
    provider->Log();
  }
}

GraphicsEtwProvider::Provider::Provider(std::string_view name, GUID guid, uint32_t target_pid,
                                        krabs::user_trace* trace,
                                        krabs::provider_event_callback callback)
    : name_(name), krabs_provider_(guid), target_pid_(target_pid), callback_(callback) {
  krabs_provider_.add_on_event_callback(absl::bind_front(&Provider::OnEvent, this));
  trace->enable(krabs_provider_);
}

void GraphicsEtwProvider::Provider::OnEvent(const EVENT_RECORD& record,
                                            const krabs::trace_context& context) {
  ++num_events_received_;
  if (record.EventHeader.ProcessId == target_pid_) {
    ++num_events_processed_;
    // Relay interesting event to provided callback.
    callback_(record, context);
  }
}

void GraphicsEtwProvider::Provider::Log() {
  ORBIT_LOG("%s: %u/%u", name_, num_events_processed_, num_events_received_);
}

}  // namespace orbit_windows_tracing
