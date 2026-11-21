/*
 * input_analog.h
 *
 *  Created on: Nov 11, 2025
 *      Author: nicolas
 */

#ifndef INC_INPUT_ANALOG_H_
#define INC_INPUT_ANALOG_H_

#include <stdint.h>
#include "main.h"

void analog_init(void);
uint32_t analog_read_raw_polling(void);
float analog_get_current(void);
void analog_start_dma(void);
float analog_get_current_dma(void);
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc);


#endif /* INC_INPUT_ANALOG_H_ */
