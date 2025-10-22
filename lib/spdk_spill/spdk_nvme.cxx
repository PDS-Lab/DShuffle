#include "spdk_spill/spdk_nvme.hxx"

#include "util/fatal.hxx"
#include "util/logger.hxx"
#include "util/unreachable.hxx"

namespace dpx::spill {

NVMeDevice::NVMeDevice(const NVMeDeviceDesc &desc) : tr_id(new spdk_nvme_transport_id) {
  auto tr_id_format = desc.as_format();
  if (auto rc = spdk_nvme_transport_id_parse(tr_id, tr_id_format.c_str()); rc != 0) {
    die("Fail to parse nvme transport id, rc: {}, formatted: {}", rc, tr_id_format);
  }
  INFO("Formatted config: {}", tr_id_format);
  auto probe_cb = [](void *, const spdk_nvme_transport_id *trid, spdk_nvme_ctrlr_opts *) -> bool {
    INFO("Attaching to nvme device {}:{}", std::string_view(trid->traddr), std::string_view(trid->trsvcid));
    return true;
  };
  auto attach_cb = [](void *ctx, const spdk_nvme_transport_id *trid, spdk_nvme_ctrlr *ctrlr,
                      const spdk_nvme_ctrlr_opts *opts) -> void {
    auto dev = reinterpret_cast<NVMeDevice *>(ctx);
    dev->ctrlr = ctrlr;
    INFO("Attached to nvme device {}:{}", std::string_view(trid->traddr), std::string_view(trid->trsvcid));
    auto ctrlr_data = spdk_nvme_ctrlr_get_data(ctrlr);
    INFO("SN: {:}", std::string_view(reinterpret_cast<const char *>(ctrlr_data->sn), SPDK_NVME_CTRLR_SN_LEN));
    INFO("MN: {:}", std::string_view((const char *)ctrlr_data->mn, SPDK_NVME_CTRLR_MN_LEN));
    INFO("FR: {:}", std::string_view((const char *)ctrlr_data->fr, SPDK_NVME_CTRLR_FR_LEN));
    INFO("subnqn: {:}", std::string_view((const char *)ctrlr_data->subnqn, SPDK_NVME_NQN_FIELD_SIZE));
    if (opts->keep_alive_timeout_ms > 0) {
      INFO("Controller need to be kept alive in {} ms", opts->keep_alive_timeout_ms);
      dev->keep_alive_timeout_ms = opts->keep_alive_timeout_ms;
    } else {
      INFO("Controller does not need keep-alive command");
    }
  };
  if (auto rc = spdk_nvme_probe(tr_id, this, probe_cb, attach_cb, nullptr); rc != 0) {
    die("Fail to probe nvme device, rc: {}", rc);
  }
  for (auto ns_id = spdk_nvme_ctrlr_get_first_active_ns(ctrlr); ns_id != 0;
       ns_id = spdk_nvme_ctrlr_get_next_active_ns(ctrlr, ns_id)) {
    if (ns_id != desc.ns_id) {
      continue;
    }
    ns = spdk_nvme_ctrlr_get_ns(ctrlr, ns_id);
    if (ns == nullptr) {
      continue;
    }
    n_sector = spdk_nvme_ns_get_num_sectors(ns);
    sector_size = spdk_nvme_ns_get_sector_size(ns);
    max_xfer_size = spdk_nvme_ns_get_max_io_xfer_size(ns);
    break;
  }
  if (ns == nullptr) {
    spdk_nvme_detach(ctrlr);
    die("Fail to find namespace {} in {}, detach", desc.ns_id, std::string_view(tr_id->traddr));
  }
  INFO("Got namespace {}, {} sectors, sector size {}B, total size: {}GiB, max transfer size: {}B", desc.ns_id, n_sector,
       sector_size, n_sector * sector_size / 1000 / 1000 / 1000, max_xfer_size);
}

NVMeDevice::~NVMeDevice() {
  if (ctrlr != nullptr) {
    if (auto rc = spdk_nvme_detach(ctrlr); rc != 0) {
      die("Fail to detach with ctrlr");
    }
  }
  if (tr_id != nullptr) {
    delete tr_id;
  }
}

bool NVMeDevice::progress_admin_queue() {
  auto rc = spdk_nvme_ctrlr_process_admin_completions(ctrlr);
  if (rc < 0) {
    die("Fail to poll admin queue");
  }
  return rc > 0;
}

NVMeDeviceIOQueue::NVMeDeviceIOQueue(NVMeDevice &dev_, size_t max_io_depth_) : dev(dev_), max_io_depth(max_io_depth_) {
  spdk_nvme_io_qpair_opts io_qpair_opts;
  spdk_nvme_ctrlr_get_default_io_qpair_opts(dev.ctrlr, &io_qpair_opts, sizeof(io_qpair_opts));
  INFO("Default io depth: {}, set {}", io_qpair_opts.io_queue_size, max_io_depth);
  io_qpair_opts.io_queue_requests = max_io_depth;
  io_qpair_opts.io_queue_size = max_io_depth;
  qpair = spdk_nvme_ctrlr_alloc_io_qpair(dev.ctrlr, &io_qpair_opts, sizeof(io_qpair_opts));
  if (qpair == nullptr) {
    die("Fail to allocate io qpair");
  }
}

NVMeDeviceIOQueue::~NVMeDeviceIOQueue() {
  if (qpair != nullptr) {
    spdk_nvme_ctrlr_disconnect_io_qpair(qpair);
    spdk_nvme_ctrlr_free_io_qpair(qpair);
  }
}

bool NVMeDeviceIOQueue::progress() {
  auto rc = spdk_nvme_qpair_process_completions(qpair, 1);
  if (rc < 0) {
    die("Fail to poll io queue");
  }
  return rc > 0;
}

op_res_future_t NVMeDeviceIOQueue::submit(IOContext &ctx) {
  if (ctx.lba_count == 0 || ctx.buffer.empty()) {
    ctx.op_res.set_value(0);
    return ctx.op_res.get_future();
  }
  int rc = 0;
  auto cb = [](void *arg, const spdk_nvme_cpl *cpl) {
    TRACE("trigger io cb");
    auto ctx = reinterpret_cast<IOContext *>(arg);
    if (cpl != nullptr && spdk_nvme_cpl_is_error(cpl)) {
      ctx->op_res.set_value(-1);
    } else {
      ctx->op_res.set_value(ctx->buffer.size());
    }
  };
  switch (ctx.op) {
    case Op::Read: {
      rc = spdk_nvme_ns_cmd_read(dev.ns, qpair, ctx.buffer.data(), ctx.start_lba, ctx.lba_count, cb, &ctx, 0);
    } break;
    case Op::Write: {
      rc = spdk_nvme_ns_cmd_write(dev.ns, qpair, ctx.buffer.data(), ctx.start_lba, ctx.lba_count, cb, &ctx, 0);
    } break;
    default: {
      unreachable();
    }
  }
  if (rc < 0) {
    ctx.op_res.set_value(rc);
  }
  return ctx.op_res.get_future();
}

}  // namespace dpx::spill
