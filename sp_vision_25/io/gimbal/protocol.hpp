#ifndef IO__GIMBAL__PROTOCOL_HPP
#define IO__GIMBAL__PROTOCOL_HPP

#include <cstdint>
#include <string>

namespace io
{
struct VisionToGimbal
{
  uint8_t mode;
  float yaw;
  float yaw_vel;
  float yaw_acc;
  float pitch;
  float pitch_vel;
  float pitch_acc;
};

std::string serialize_vision_to_gimbal(const VisionToGimbal & command);

}  // namespace io

#endif  // IO__GIMBAL__PROTOCOL_HPP
