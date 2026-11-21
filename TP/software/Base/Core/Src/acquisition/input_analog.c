/*
 * analog_input.c
 *
 *  Created on: Nov 11, 2025
 *      Author: nicolas
 */

#include "acquisition/input_analog.h"
#include "adc.h"

extern ADC_HandleTypeDef hadc1;

#define VREF_ADC    3.3f
#define ADC_MAX_VAL 4095.0f
#define V_OFFSET    1.65f   // (I = 0 A)
#define SENSITIVITY 0.050f  // 50 mV/A

static uint16_t adc_dma_buffer[1];
volatile float current_measured = 0.0f;

void analog_init(void)
{
	// Calibration ADC
	HAL_ADCEx_Calibration_Start(&hadc1, ADC_SINGLE_ENDED);
}

// Lecture des valeurs de courant par polling
uint32_t analog_read_raw_polling(void)
{
	HAL_ADC_Start(&hadc1);
	if (HAL_ADC_PollForConversion(&hadc1, 10) == HAL_OK)
	{
		return HAL_ADC_GetValue(&hadc1);
	}
	return 0;
}

// Acquisition et conversion des valeurs de courant (numerique à analogique)
float analog_get_current(void)
{
	uint32_t raw = analog_read_raw_polling();
	float v_adc = ((float)raw * VREF_ADC) / ADC_MAX_VAL;
	float current = (v_adc - V_OFFSET) / SENSITIVITY;
	return current;
}

// Activation de la DMA
void analog_start_dma(void)
{
    HAL_ADC_Start_DMA(&hadc1, (uint32_t*)adc_dma_buffer, 1);
}

// Lecture valeur courant via la DMA
float analog_get_current_dma(void)
{
    return current_measured;
}

// Appelé automatiquement par le DMA à chaque sommet/creux de TIM1
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)
{
    if (hadc->Instance == ADC1)
    {
        // Front montant sur le GPIO de contrôle (USR_LED)
    	HAL_GPIO_WritePin(USR_LED_GPIO_Port, USR_LED_Pin, GPIO_PIN_SET);

        float v_adc = ((float)adc_dma_buffer[0] * VREF_ADC) / ADC_MAX_VAL;
        current_measured = (v_adc - V_OFFSET) / SENSITIVITY;

        // Front descendant sur le GPIO de contrôle
        HAL_GPIO_WritePin(USR_LED_GPIO_Port, USR_LED_Pin, GPIO_PIN_RESET);
    }
}
