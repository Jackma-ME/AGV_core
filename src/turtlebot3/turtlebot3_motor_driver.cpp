/*******************************************************************************
* Copyright 2016 ROBOTIS CO., LTD.
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*     http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*******************************************************************************/

/* Authors: Yoonseok Pyo, Leon Jung, Darby Lim, HanCheol Cho */

#include "../../include/turtlebot3/turtlebot3_motor_driver.h"

Turtlebot3MotorDriver::Turtlebot3MotorDriver()
: baudrate_(BAUDRATE),
  protocol_version_(PROTOCOL_VERSION),
  left_wheel_id_(DXL_LEFT_ID),
  right_wheel_id_(DXL_RIGHT_ID),
  back_left_wheel_id_(DXL_BACK_LEFT_ID),
  back_right_wheel_id_(DXL_BACK_RIGHT_ID),
  torque_(false)
{
}

Turtlebot3MotorDriver::~Turtlebot3MotorDriver()
{
  closeDynamixel();
}

bool Turtlebot3MotorDriver::init(void)
{
  portHandler_   = dynamixel::PortHandler::getPortHandler(DEVICENAME);
  packetHandler_ = dynamixel::PacketHandler::getPacketHandler(PROTOCOL_VERSION);

  // Open port
  if (portHandler_->openPort() == false)
  {
    return false;
  }

  // Set port baudrate
  if (portHandler_->setBaudRate(baudrate_) == false)
  {
    return false;
  }

  // Enable Dynamixel Torque
  setTorque(left_wheel_id_, true);
  setTorque(right_wheel_id_, true);
  setTorque(back_left_wheel_id_, true);
  setTorque(back_right_wheel_id_, true);

  groupSyncWriteVelocity_ = new dynamixel::GroupSyncWrite(portHandler_, packetHandler_, ADDR_X_GOAL_VELOCITY, LEN_X_GOAL_VELOCITY);
  groupSyncReadEncoder_   = new dynamixel::GroupSyncRead(portHandler_, packetHandler_, ADDR_X_PRESENT_POSITION, LEN_X_PRESENT_POSITION);

  return true;
}

bool Turtlebot3MotorDriver::setTorque(uint8_t id, bool onoff)
{
  uint8_t dxl_error = 0;
  int dxl_comm_result = COMM_TX_FAIL;

  dxl_comm_result = packetHandler_->write1ByteTxRx(portHandler_, id, ADDR_X_TORQUE_ENABLE, onoff, &dxl_error);
  if(dxl_comm_result != COMM_SUCCESS)
  {
    Serial.println(packetHandler_->getTxRxResult(dxl_comm_result));
    return false;
  }
  else if(dxl_error != 0)
  {
    Serial.println(packetHandler_->getRxPacketError(dxl_error));
    return false;
  }

  torque_ = onoff;
  return true;
}

bool Turtlebot3MotorDriver::getTorque()
{
  return torque_;
}

void Turtlebot3MotorDriver::closeDynamixel(void)
{
  // Disable Dynamixel Torque
  setTorque(left_wheel_id_, false);
  setTorque(right_wheel_id_, false);
  setTorque(back_left_wheel_id_, false);
  setTorque(back_right_wheel_id_, false);

  // Close port
  portHandler_->closePort();
}

bool Turtlebot3MotorDriver::readEncoder(int32_t &left_value, int32_t &right_value)
{
  int dxl_comm_result = COMM_TX_FAIL;              // Communication result
  bool dxl_addparam_result = false;                // addParam result
  bool dxl_getdata_result = false;                 // GetParam result

  // Set parameter
  dxl_addparam_result = groupSyncReadEncoder_->addParam(left_wheel_id_);
  if (dxl_addparam_result != true)
    return false;

  dxl_addparam_result = groupSyncReadEncoder_->addParam(right_wheel_id_);
  if (dxl_addparam_result != true)
    return false;

  // Syncread present position
  dxl_comm_result = groupSyncReadEncoder_->txRxPacket();
  if (dxl_comm_result != COMM_SUCCESS)
    Serial.println(packetHandler_->getTxRxResult(dxl_comm_result));

  // Check if groupSyncRead data of Dynamixels are available
  dxl_getdata_result = groupSyncReadEncoder_->isAvailable(left_wheel_id_, ADDR_X_PRESENT_POSITION, LEN_X_PRESENT_POSITION);
  if (dxl_getdata_result != true)
    return false;

  dxl_getdata_result = groupSyncReadEncoder_->isAvailable(right_wheel_id_, ADDR_X_PRESENT_POSITION, LEN_X_PRESENT_POSITION);
  if (dxl_getdata_result != true)
    return false;

  // Get data
  left_value  = groupSyncReadEncoder_->getData(left_wheel_id_,  ADDR_X_PRESENT_POSITION, LEN_X_PRESENT_POSITION);
  right_value = groupSyncReadEncoder_->getData(right_wheel_id_, ADDR_X_PRESENT_POSITION, LEN_X_PRESENT_POSITION);

  groupSyncReadEncoder_->clearParam();
  return true;
}

bool Turtlebot3MotorDriver::writeVelocity(int64_t left_value, int64_t right_value, int64_t back_left_value, int64_t back_right_value)
{
  bool dxl_addparam_result;
  int8_t dxl_comm_result;

  int64_t value[4] = {left_value, right_value, back_left_value, back_right_value};
  uint8_t data_byte[4] = {0, };

  for (uint8_t index = 0; index < 4; index++)
  {
    data_byte[0] = DXL_LOBYTE(DXL_LOWORD(value[index]));
    data_byte[1] = DXL_HIBYTE(DXL_LOWORD(value[index]));
    data_byte[2] = DXL_LOBYTE(DXL_HIWORD(value[index]));
    data_byte[3] = DXL_HIBYTE(DXL_HIWORD(value[index]));

    dxl_addparam_result = groupSyncWriteVelocity_->addParam(index+1, (uint8_t*)&data_byte);
    if (dxl_addparam_result != true)
      return false;
  }

  dxl_comm_result = groupSyncWriteVelocity_->txPacket();
  if (dxl_comm_result != COMM_SUCCESS)
  {
    Serial.println(packetHandler_->getTxRxResult(dxl_comm_result));
    return false;
  }

  groupSyncWriteVelocity_->clearParam();
  return true;
}

bool Turtlebot3MotorDriver::controlMotor(const float wheel_separation, const float wheel_separation2, float* value)
{
  bool dxl_comm_result = false;
  
  float wheel_velocity_cmd[4];

  float lin_x_vel = value[LINEAR_X];
  float lin_y_vel = value[LINEAR_Y];
  float ang_vel = value[ANGULAR];


  wheel_velocity_cmd[LEFT]        = (lin_x_vel + lin_y_vel - (ang_vel * (wheel_separation2 - wheel_separation)));
  wheel_velocity_cmd[RIGHT]       = -(lin_x_vel - lin_y_vel + (ang_vel * (wheel_separation2 - wheel_separation)));
  wheel_velocity_cmd[BACK_LEFT]   = (lin_x_vel - lin_y_vel - (ang_vel * (wheel_separation2 - wheel_separation)));
  wheel_velocity_cmd[BACK_RIGHT]  = -(lin_x_vel + lin_y_vel + (ang_vel * (wheel_separation2 - wheel_separation)));

  wheel_velocity_cmd[LEFT]  = constrain(wheel_velocity_cmd[LEFT]  * VELOCITY_CONSTANT_VALUE, -LIMIT_X_MAX_VELOCITY, LIMIT_X_MAX_VELOCITY);
  wheel_velocity_cmd[RIGHT] = constrain(wheel_velocity_cmd[RIGHT] * VELOCITY_CONSTANT_VALUE, -LIMIT_X_MAX_VELOCITY, LIMIT_X_MAX_VELOCITY);
  wheel_velocity_cmd[BACK_LEFT]  = constrain(wheel_velocity_cmd[BACK_LEFT]  * VELOCITY_CONSTANT_VALUE, -LIMIT_X_MAX_VELOCITY, LIMIT_X_MAX_VELOCITY);
  wheel_velocity_cmd[BACK_RIGHT] = constrain(wheel_velocity_cmd[BACK_RIGHT] * VELOCITY_CONSTANT_VALUE, -LIMIT_X_MAX_VELOCITY, LIMIT_X_MAX_VELOCITY);

  dxl_comm_result = writeVelocity((int64_t)wheel_velocity_cmd[LEFT], (int64_t)wheel_velocity_cmd[RIGHT], (int64_t)wheel_velocity_cmd[BACK_LEFT], (int64_t)wheel_velocity_cmd[BACK_RIGHT]);
  if (dxl_comm_result == false)
    return false;

  return true;
}
