#ifndef KERNEL_ENGINE_ISOLATOR_H
#define KERNEL_ENGINE_ISOLATOR_H

#include <string>
#include <stdint.h>
#include <set>

namespace dos {

// cpu isolator implemented by cgroup
class CpuIsolator {

public:
  // cpu path is cpu subsystem path
  // cpu acct path is cpuacct subsystem path
  CpuIsolator(const std::string& cpu_path,
              const std::string& cpu_acct_path);
  ~CpuIsolator();
  // attach pid this isolator
  // like echo pid >> path/cgroup.proc
  bool Attach(int32_t pid);
  // assign quota to cpu subsystem
  // by update the cpu.share file
  bool AssignQuota(int32_t quota);
  // set the max cpu peroid that used
  // by update the cpu.cfs_peroid_quota
  bool AssignLimit(int32_t limit);
  // if path does not exist then create it
  bool Init();
  // Get the cpu used in a peroid
  bool GetCpuUsage(int32_t* used);
  // Get all pids in this subsystem
  bool GetPids(std::set<int32_t>* pids);
  // rmdir this subsystem 
  bool Destroy();
private:
  std::string cpu_path_;
  std::string cpu_acct_path_;
  int32_t quota_;
  int32_t limit_;
};

// memory isolator implemented by cgroup
class MemoryIsolator {

};

// device io isolator implemented by cgroup
// 
class DeviceIoIsolator {};

// network io isolator implemented by cgroup
class NetworkIoIsolator {};

} // namespace dos

#endif
