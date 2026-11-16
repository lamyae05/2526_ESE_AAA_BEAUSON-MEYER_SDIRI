/*
 * motor.c
 *
 *  Created on: Nov 11, 2025
 *      Author: nicolas
 */

#include "motor_control/motor.h"
#include "main.h"
#include "tim.h"
#include "stm32g4xx_it.h"S

#define CCR_MAX 4250
#define CCR_NULL (CCR_MAX / 2) // 2125 (Rapport cyclique 50%)
#define RAMP_STEP 10            // Incrémentation par 10

extern TIM_HandleTypeDef htim1;

// Variables de gestion de la rampe
static volatile int ccr_current = CCR_NULL;
static volatile int ccr_target = CCR_NULL;
static volatile uint8_t motor_is_running = 0;

void motor_set_ccr(int val_ccr)
{
    // Saturation de sécurité
    if (val_ccr > CCR_MAX) {
        val_ccr = CCR_MAX;
    } else if (val_ccr < 0) {
        val_ccr = 0;
    }

    // Commande complémentaire décalée
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, val_ccr);
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_2, CCR_MAX - val_ccr);
}

void motor_start(void)
{
	ccr_current = CCR_NULL;
	ccr_target = CCR_NULL;

	// Rapport cyclique initial à 50%
	__HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, CCR_NULL);
	__HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_2, CCR_NULL);

	// Démarrage des PWM complémentaires
	HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);
	HAL_TIMEx_PWMN_Start(&htim1, TIM_CHANNEL_1);
	HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_2);
	HAL_TIMEx_PWMN_Start(&htim1, TIM_CHANNEL_2);

	motor_is_running = 1;
}

void motor_stop(void)
{
	motor_is_running = 0;

	// Arrêt des PWM
	HAL_TIM_PWM_Stop(&htim1, TIM_CHANNEL_1);
	HAL_TIMEx_PWMN_Stop(&htim1, TIM_CHANNEL_1);
	HAL_TIM_PWM_Stop(&htim1, TIM_CHANNEL_2);
	HAL_TIMEx_PWMN_Stop(&htim1, TIM_CHANNEL_2);

	ccr_current = CCR_NULL;
	ccr_target = CCR_NULL;
}

void motor_set_speed(int target_ccr)
{
	if (target_ccr > CCR_MAX) target_ccr = CCR_MAX;
	if (target_ccr < 0) target_ccr = 0;

	ccr_target = target_ccr;

	//__HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, target_ccr);
	//__HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_2, CCR_MAX - target_ccr);
}

// Callback appelé toutes les 1 ms par le SysTick
void HAL_SYSTICK_Callback(void)
{
	if (motor_is_running)
	{
		if (ccr_current < ccr_target)
		{
			ccr_current += RAMP_STEP;
			if (ccr_current > ccr_target) ccr_current = ccr_target;
		}
		else if (ccr_current > ccr_target)
		{
			ccr_current -= RAMP_STEP;
			if (ccr_current < ccr_target) ccr_current = ccr_target;
		}

		// Commande complémentaire décalée
		__HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, ccr_current);
		__HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_2, CCR_MAX - ccr_current);
	}
}
