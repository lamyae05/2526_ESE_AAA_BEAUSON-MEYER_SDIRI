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

#define CCR_NULL (CCR_MAX / 2) // 2125 (Rapport cyclique 50%)
#define RAMP_STEP 10            // Incrémentation par 10

extern TIM_HandleTypeDef htim1;

// Variables de gestion de la rampe
static volatile int ccr_current = CCR_NULL;
static volatile int ccr_target = CCR_NULL;
static volatile uint8_t motor_is_running = 0;

/*
 * Fonction d'initialisation du moteur :
 * consignes à zéro et rapports cycliques à 50% (vitesse nulle)
 */
void motor_init(void){
	motor_is_running = 0;
	ccr_current = CCR_NULL;
	ccr_target = CCR_NULL;

	// Rapport cyclique initial à 50% (vitesse nulle)
	__HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, CCR_NULL);
	__HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_2, CCR_NULL);
}

/*
 * Fonction pour appliquer un rapport cyclique en commande complémentaire décalée
 */
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

/*
 * Fonction pour démarrer les 4 signaux PWM complémentaires à 50%
 * et autorise le pilotage du moteur
 */
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

/*
 * Fonction pour arrêter la génération des 4 PWM et réinitialiser les consignes du moteur
 */
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

/*
 * Fonction pour enregistrer la nouvelle consigne de vitesse cible (atteinte progressivement par la rampe)
 */
void motor_set_speed(int target_ccr)
{
	if (target_ccr > CCR_MAX) {
		target_ccr = CCR_MAX;
	}
	else if (target_ccr < 0) {
		target_ccr = 0;
	}

	ccr_target = target_ccr;
}

/*
 * Fonction pour mettre à jour la rampe de vitesse (appelée toutes les 1 ms par TIM6)
 */
void motor_update_ramp(void)
{
    if (motor_is_running && (ccr_current != ccr_target))
    {
        if (ccr_current < ccr_target)
        {
            ccr_current += RAMP_STEP;
            if (ccr_current > ccr_target) {
            	ccr_current = ccr_target;
            }
        }
        else if (ccr_current > ccr_target)
        {
            ccr_current -= RAMP_STEP;
            if (ccr_current < ccr_target) {
            	ccr_current = ccr_target;
            }
        }
        // Application en commande complémentaire décalée
        motor_set_ccr(ccr_current);
    }
    else {
		return;
	}
}

