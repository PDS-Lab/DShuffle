#include "doca/buffer.hxx"
#include "doca/device.hxx"
#include "util/literal.hxx"

using namespace dpx::literal;

int main(int argc, char* argv[]) {
  auto d = dpx::doca::Device::open_by_ibdev_name("mlx5_0");
  size_t sz = std::atoi(argv[1]) * 1_GB;
  auto c1 = new char[sz];
  auto c2 = new char[sz];
  dpx::doca::MappedRegion mr1(d, c1, sz, DOCA_ACCESS_FLAG_PCI_READ_WRITE);
  dpx::doca::MappedRegion mr2(d, c2, sz, DOCA_ACCESS_FLAG_PCI_READ_WRITE);
  return 0;
}