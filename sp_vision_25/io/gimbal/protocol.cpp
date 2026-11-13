#include "protocol.hpp"

#include <iomanip>
#include <locale>
#include <sstream>

#include "tools/crc.hpp"

namespace io
{
std::string serialize_vision_to_gimbal(const VisionToGimbal & command)
{
  std::ostringstream payload;
  payload.imbue(std::locale::classic());
  payload << "SP,mode:" << static_cast<unsigned>(command.mode)
          << std::fixed << std::setprecision(6)
          << ",yaw:" << command.yaw
          << ",yaw_vel:" << command.yaw_vel
          << ",yaw_acc:" << command.yaw_acc
          << ",pitch:" << command.pitch
          << ",pitch_vel:" << command.pitch_vel
          << ",pitch_acc:" << command.pitch_acc;

  const auto payload_text = payload.str();
  const auto crc = tools::get_crc16(
    reinterpret_cast<const uint8_t *>(payload_text.data()),
    static_cast<uint32_t>(payload_text.size()));

  std::ostringstream frame;
  frame.imbue(std::locale::classic());
  frame << payload_text << '*'
        << std::uppercase << std::hex << std::setfill('0') << std::setw(4) << crc << "\r\n";
  return frame.str();
}

}  // namespace io
