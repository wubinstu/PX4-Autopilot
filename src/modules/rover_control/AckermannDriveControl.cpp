/****************************************************************************
 *
 *   Copyright (c) 2023 PX4 Development Team. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name PX4 nor the names of its contributors may be
 *    used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ****************************************************************************/

#include "AckermannDriveControl.hpp"

using namespace time_literals;
using namespace matrix;
namespace differential_drive_control
{

AckermannDriveControl::AckermannDriveControl(ModuleParams *parent) : ModuleParams(parent)
{
	updateParams();
}

void AckermannDriveControl::updateParams()
{
	ModuleParams::updateParams();
	_max_speed = _param_rdd_max_wheel_speed.get() * _param_rdd_wheel_radius.get();
	_max_angular_velocity = _max_speed / (_param_rdd_wheel_base.get() / 2.f);

	// _differential_drive_kinematics.setWheelBase(_param_rdd_wheel_base.get());
	// _differential_drive_kinematics.setMaxSpeed(_max_speed);
	// _differential_drive_kinematics.setMaxAngularVelocity(_max_angular_velocity);
}

void AckermannDriveControl::Update()
{
	hrt_abstime now = hrt_absolute_time();

	if (_parameter_update_sub.updated()) {
		parameter_update_s pupdate;
		_parameter_update_sub.copy(&pupdate);

		updateParams();
	}

	if (_vehicle_control_mode_sub.updated()) {
		vehicle_control_mode_s vehicle_control_mode;

		if (_vehicle_control_mode_sub.copy(&vehicle_control_mode)) {
			_armed = vehicle_control_mode.flag_armed;
			_manual_driving = vehicle_control_mode.flag_control_manual_enabled; // change this when more modes are supported
		}
	}

	// printf("AckermannDriveControl::Update()\n");

	if (_manual_driving) {
		// Manual mode
		// directly produce setpoints from the manual control setpoint (joystick)
		if (_manual_control_setpoint_sub.updated()) {
			manual_control_setpoint_s manual_control_setpoint{};

			if (_manual_control_setpoint_sub.copy(&manual_control_setpoint)) {
				_differential_drive_setpoint.timestamp = now;
				_differential_drive_setpoint.speed = manual_control_setpoint.throttle * _param_rdd_speed_scale.get() * _max_speed;
				_differential_drive_setpoint.yaw_rate = manual_control_setpoint.roll * _param_rdd_ang_velocity_scale.get() *
									_max_angular_velocity;
				_differential_drive_setpoint_pub.publish(_differential_drive_setpoint);
			}
		}
	}

	_differential_drive_setpoint_sub.update(&_differential_drive_setpoint);

	// publish data to actuator_motors (output module)
	// get the wheel speeds from the inverse kinematics class (DifferentialDriveKinematics)
	// Vector2f wheel_speeds = _differential_drive_kinematics.computeInverseKinematics(
	// 				_differential_drive_setpoint.speed,
	// 				_differential_drive_setpoint.yaw_rate);

	double steering = _differential_drive_setpoint.yaw_rate;
	double speed = _differential_drive_setpoint.speed;

	// Check if max_angular_wheel_speed is zero
	// const bool setpoint_timeout = (_differential_drive_setpoint.timestamp + 100_ms) < now;
	// const bool valid_max_speed = _param_rdd_speed_scale.get() > FLT_EPSILON;

	// if (!_armed || setpoint_timeout || !valid_max_speed) {
	// 	wheel_speeds = {}; // stop
	// }

	// wheel_speeds = matrix::constrain(wheel_speeds, -1.f, 1.f);

	actuator_motors_s actuator_motors{};
	// actuator_motors.reversible_flags = _param_r_rev.get(); // should be 3 see rc.rover_differential_defaults
	// wheel_speeds.copyTo(actuator_motors.control);
	actuator_motors.control[0] = speed;
	actuator_motors.control[1] = speed;
	actuator_motors.timestamp = now;
	_actuator_motors_pub.publish(actuator_motors);

	// publish data to actuator_servos (output module)
	actuator_servos_s actuator_servos{};
	actuator_servos.control[0] = -steering;
	actuator_servos.control[1] = -steering;
	actuator_servos.timestamp = now;
	_actuator_servos_pub.publish(actuator_servos);
}
} // namespace differential_drive_control
