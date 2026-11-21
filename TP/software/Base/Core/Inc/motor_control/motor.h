/*
 * motor.h
 *
 *  Created on: Nov 11, 2025
 *      Author: nicolas
 */

#ifndef INC_MOTOR_CONTROL_MOTOR_H_
#define INC_MOTOR_CONTROL_MOTOR_H_

#define CCR_MAX 4250

#include <stdint.h>

void motor_init(void);
void motor_set_ccr(int val_ccr);
void motor_start(void);
void motor_stop(void);
void motor_set_speed(int target_ccr);
void motor_update_ramp(void);


#endif /* INC_MOTOR_CONTROL_MOTOR_H_ */
