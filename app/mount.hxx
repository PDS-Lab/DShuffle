#pragma once

#include <cstdlib>
#include <format>

inline int mount(std::string dev, std::string mount_point) {
  return system(std::format("sudo mount {} {}", dev, mount_point).c_str());
}

inline int umount(std::string mount_point) { return system(std::format("sudo umount {}", mount_point).c_str()); }
