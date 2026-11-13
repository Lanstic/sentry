#include <cassert>
#include <string>

#include "io/gimbal/protocol.hpp"

int main()
{
  io::VisionToGimbal command{
    1, 0.12F, 0.03F, 0.5F, -0.08F, 0.01F, 0.2F};

  assert(
    io::serialize_vision_to_gimbal(command) ==
    "SP,mode:1,yaw:0.120000,yaw_vel:0.030000,yaw_acc:0.500000,"
    "pitch:-0.080000,pitch_vel:0.010000,pitch_acc:0.200000*338C\r\n");

  io::VisionToGimbal idle{0, 0, 0, 0, 0, 0, 0};
  assert(
    io::serialize_vision_to_gimbal(idle) ==
    "SP,mode:0,yaw:0.000000,yaw_vel:0.000000,yaw_acc:0.000000,"
    "pitch:0.000000,pitch_vel:0.000000,pitch_acc:0.000000*78DC\r\n");

  return 0;
}
