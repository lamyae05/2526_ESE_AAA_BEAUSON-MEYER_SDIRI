# TP Actionneurs et Automatique Appliquée - ESE1

**Binôme :** Lamyae SDIRI & William BEAUSON-MEYER  
**Matériel :** Carte Nucleo-STM32G474RE, carte hacheur complet (bras IRF540N), Machine à Courant Continu (MCC) couplée

---

## 1. Organisation du code

Nous avons organisé notre projet avec l'architecture modulaire fournie sous STM32CubeIDE afin de séparer la gestion matérielle de la logique applicative :
- **`app.c` :** Centralise l'initialisation des périphériques et des modules logiciels (Shell, moteur, ADC/DMA) au démarrage.
- **`user_interface/shell.c` :** Terminal série via `USART2` (115200 bauds) géré par interruption (`HAL_UART_RxCpltCallback`) pour envoyer des commandes sans bloquer le microcontrôleur.
- **`motor_control/motor.c` :** Configuration des rapports cycliques sur `TIM1` (commande complémentaire décalée) et gestion de la rampe d'accélération.
- **`acquisition/input_analog.c` :** Mesure du courant moteur sur `ADC1` (en polling dans un premier temps, puis via `TIM1` + `DMA`). 

---

## 2. Commande MCC basique (Partie 6)

### 2.1. Configuration des 4 PWM et calcul du temps mort (`TIM1`)

Pour piloter les 4 MOSFETs du pont en H en **commande complémentaire décalée**, nous avons configuré le timer avancé `TIM1` sur les canaux `CH1`/`CH1N` (`PA8`/`PB13`) pour le bras U et `CH2`/`CH2N` (`PA9`/`PB14`) pour le bras V :

- **Fréquence et résolution :** L'horloge système tourne à f_CLK = 170 MHz (`PSC = 0`). Pour obtenir une fréquence PWM de 20 kHz en mode **Center-Aligned** (compteur triangulaire montant puis descendant), on fixe :
  $$\text{ARR} = \frac{170\text{ MHz}}{2 \times 20\text{ kHz}} = 4250$$
  On dispose ainsi de 4250 pas de réglage, ce qui dépasse largement la résolution minimale de 10 bits demandée ($1024$ pas).
- **Principe de la commande complémentaire décalée :** Contrairement à une commande complémentaire classique, les deux bras travaillent en opposition autour de 50 % : on applique `CCR1 = val_ccr` sur le bras U et `CCR2 = ARR - val_ccr` sur le bras V. Ainsi, pour un rapport cyclique de 50 % (`CCR = 2125`), les deux bras commutent en même temps et la tension moyenne aux bornes du moteur est nulle ($U_m = 0\text{ V}$).

  ![PWM complémentaire non décalée à 60%](6-pwm_complementaire_non_decale_60%25.png)  
  *Figure 1 — Premier essai en commande complémentaire classique (non décalée) à un rapport cyclique de 60 % : les signaux des deux bras sont en phase.*

  ![PWM complémentaire décalée](6-pwm_complementaire_decalee.png)  
  *Figure 2 — Passage en commande complémentaire décalée (`CCR2 = ARR - CCR1`) : les créneaux des bras U et V sont bien centrés et en opposition.*

- **Choix et calcul du temps mort (*Dead-time*) :**
  Afin d'éviter qu'un transistor haut et un transistor bas d'un même bras conduisent simultanément lors des commutations (ce qui mettrait le bus continu en court-circuit), il faut insérer un temps mort supérieur au temps de blocage réel de la chaîne de commande :
  1. D'après la datasheet du MOSFET **`IRF540N`**, le temps de retard au blocage est $t_{d(\text{off})} \approx 39\text{ ns}$ et le temps de descente est $t_f \approx 35\text{ ns}$, soit environ $75\text{ ns}$ pour ouvrir complètement le transistor.
  2. Sur le schéma KiCad du hacheur, la commande passe par un optocoupleur driver **`HCPL-315J`** (avec une résistance de grille de $30\ \Omega$), qui ajoute un délai de propagation différentiel ($|t_{PLH} - t_{PHL}|$ pouvant atteindre $\approx 100\text{ à }150\text{ ns}$).
  3. En prenant une marge de sécurité, nous avons choisi un temps mort d'environ **$300\text{ ns}$**. Avec une horloge timer à $170\text{ MHz}$ ($T_{DTS} \approx 5,88\text{ ns}$), cela correspond à une valeur de registre `DTG` d'environ $51$ ($51 \times 5,88\text{ ns} \approx 300\text{ ns}$).

  ![Visualisation du Dead-Time à l'oscilloscope](6-PWM_DeadTime.png)  
  *Figure 3 — Zoom sur une commutation entre `CH1` et `CH1N` : on mesure bien le temps mort d'environ $300\text{ ns}$ entre le front descendant d'un transistor et le front montant du transistor complémentaire.*

### 2.2. Premiers tests en boucle ouverte (`SET_CCR`)

- Nous avons implémenté la commande `SET_CCR <0-4250>` qui sature la valeur entrée entre `0` et `CCR_MAX` (`4250`) et met directement à jour `CCR1` et `CCR2`.
- **Observations lors des essais :** À 50 % (`2125`), le moteur reste bien à l'arrêt. Lorsqu'on envoie brutalement `SET_CCR 2975` (70 %), le moteur démarre d'un coup sec et on observe un fort pic de courant sur l'alimentation. Ce test met en évidence le besoin d'une rampe de démarrage pour limiter l'appel de courant.

---

## 3. Commande avec rampe d'accélération (Partie 7.1)

Pour éviter les à-coups de courant, nous avons ajouté trois commandes dans le shell :
- `start` : Initialise les rapports cycliques à 50 % (`CCR_NULL = 2125`), active les 4 sorties PWM (`HAL_TIM_PWM_Start` et `HAL_TIMEx_PWMN_Start`) et autorise le fonctionnement du moteur.
- `stop` : Désactive les 4 sorties PWM et remet les consignes à 50 %.
- `speed <0-4250>` : Met à jour la consigne cible `ccr_target` sans modifier immédiatement le timer.

**Fonctionnement de la rampe :** Sur notre projet CubeMX, la base de temps système de $1\text{ ms}$ est gérée par **`TIM6`**. Nous avons donc appelé notre fonction `motor_update_ramp()` dans l'interruption de période de `TIM6` (`HAL_TIM_PeriodElapsedCallback`). Toutes les millisecondes, `ccr_current` rejoint `ccr_target` par pas de `RAMP_STEP = 10` (soit environ $212\text{ ms}$ pour passer de 50 % à 100 %), ce qui rend les accélérations beaucoup plus souples.

---

## 4. Mesure de courant (Partie 7.2)

### 4.1. Étude du schéma KiCad et du capteur

En analysant le schéma KiCad de la carte hacheur (`Arm_IRF540N` et `pwr supply`) et le brochage CubeMX du STM32G474RE :
- **Capteurs présents sur la carte :** Deux capteurs à effet Hall **`LEM GO 10-SME/SP3`** alimentés en $3,3\text{ V}$ (`ISO_3V3` via le régulateur `AZ1117-3.3`) :
  - **`PA1` (`ADC1_IN2`) :** Mesure `U_Imes`, le courant en sortie du bras U (`U304`), qui correspond directement au courant d'induit de la MCC.
  - **`PC2` (`ADC1_IN8`) :** Mesure `Bus_Imes`, le courant consommé sur le bus continu.
- **Fonction de transfert (`LEM GO 10-SME/SP3`) :**
  - Sensibilité nominale : $S_N = 50\text{ mV/A}$ ($0,050\text{ V/A}$) pour une plage de $\pm 25\text{ A}$.
  - Tension de référence à courant nul : $U_{\text{ref}} = 1,65\text{ V}$ théorique. Au repos (PWM coupées), nous avons relevé une valeur brute ADC autour de `2065` LSB, soit un offset réel de $V_{\text{offset}} \approx 1,664\text{ V}$.
  - Formule implantée dans `input_analog.c` (ADC 12 bits sous $3,3\text{ V}$) :
    $$I_{\text{mes}} = \frac{\left(N_{\text{ADC}} \times \frac{3,3}{4095}\right) - 1,664}{0,050}$$

### 4.2. Acquisition en Polling puis synchronisation `TIM1 -> ADC1 -> DMA`

1. **Tests en Polling :** Après calibration de l'ADC (`HAL_ADCEx_Calibration_Start`), les mesures via la commande `get_i` étaient très bruitées et instables d'un appel à l'autre. Comme le courant moteur présente une ondulation triangulaire à $20\text{ kHz}$, une lecture logicielle asynchrone tombe de manière aléatoire sur un sommet, un creux ou une commutation.

   ![Mesure courant en polling sur l'oscilloscope](7.2-oscillo_mesure_courant_polling.png)  
   *Figure 4 — Relevé oscilloscope lors de la mesure en polling : le signal du capteur `U_Imes` présente l'ondulation triangulaire du courant à $20\text{ kHz}$, tandis que l'instant d'échantillonnage logiciel n'est pas synchronisé avec la PWM.*

   ![Résultats terminal en polling](7.2-Terminal_polling.png)  
   *Figure 5 — Valeurs affichées dans le terminal en mode polling : on observe des écarts importants entre deux appels successifs de `get_i`.*

2. **Passage au DMA synchronisé sur `TIM1` :**
   - En mode *Center-Aligned*, le courant atteint sa valeur moyenne exacte lorsque le compteur `TIM1_CNT` passe par ses extrema (`0` et `ARR`), c'est-à-dire au milieu des créneaux PWM, loin des fronts de commutation des transistors.
   - Nous avons configuré `TIM1` pour générer un signal matériel `TRGO` sur chaque `Update Event`, déclenchant automatiquement une conversion de `ADC1`. Le contrôleur `DMA` (en mode **Circular**, largeur `Half Word`) transfère directement le résultat en mémoire sans intervention du CPU.
   - **Vérification à l'oscilloscope :** Dans l'interruption de fin de conversion DMA (`HAL_ADC_ConvCpltCallback`), nous faisons basculer la broche **`PA5` (`USR_LED`)** pour visualiser l'instant précis de la conversion.

   ![Mesure courant synchronisée en DMA à l'oscilloscope](7.2-oscillo_mesure_courant_dma.png)  
   *Figure 6 — Synchronisation `TIM1 -> ADC -> DMA` : l'impulsion générée sur `PA5` dans le callback DMA tombe exactement au milieu du créneau PWM (valeur moyenne du triangle de courant), loin des fronts de commutation.*

   ![Résultats terminal en DMA](7.2-Terminal_dma.png)  
   *Figure 7 — Validation en charge (`speed 2550`, soit 60 % de rapport cyclique) : `get_i` affiche une valeur stable autour de $2,0\text{ A}$, cohérente avec la tension relevée sur la sonde du capteur à l'oscilloscope.*

---

## 5. Récapitulatif des commandes Shell

| Commande | Paramètre | Rôle |
| :--- | :--- | :--- |
| `help` | — | Affiche la liste des commandes disponibles |
| `start` | — | Active les 4 PWM avec un rapport cyclique initial de 50 % |
| `stop` | — | Coupe les 4 PWM et remet les consignes à 50 % |
| `SET_CCR` | `<0-4250>` | Applique directement un rapport cyclique sans rampe |
| `speed` | `<0-4250>` | Définit une consigne de vitesse atteinte via la rampe (`TIM6`) |
| `get_i` | — | Affiche le courant d'induit moyen mesuré par l'ADC/DMA (en A) |

## 6. Suite du TP (Reste à faire)
* **7.3. Mesure de vitesse :** Configuration de `TIM3` en *Encoder Interface Mode* (`PA6`, `PA4`, `PC8`) pour lire le codeur incrémental et déterminer la constante de temps mécanique du moteur[cite: 5, 21, 118].
* **Asservissement :** Implémentation des correcteurs PI numériques (boucle de courant rapide dans l'interruption DMA et boucle de vitesse dans `asserv.c`)[cite: 12, 127, 138].
* <img width="455" height="422" alt="7 2-Terminal_polling" src="https://github.com/user-attachments/assets/f59dc71d-1488-409d-b925-47b9eb3febf7" />
<img width="1112" height="846" alt="7 2-Terminal_dma" src="https://github.com/user-attachments/assets/e0a5c6ca-093c-43f5-a545-af78afea5161" />
<img width="800" height="480" alt="7 2-oscillo_mesure_courant_polling" src="https://github.com/user-attachments/assets/de91d979-00a7-4fdc-baec-9db769534cfc" />
<img width="800" height="480" alt="7 2-oscillo_mesure_courant_dma" src="https://github.com/user-attachments/assets/0d75b8f8-7667-465c-831e-4e5a21dfa8b3" />
<img width="800" height="480" alt="6-PWM_DeadTime" src="https://github.com/user-attachments/assets/c629f43b-4c3f-469a-8576-af6446558e39" />
<img width="800" height="480" alt="6-pwm_complementaire_non_decale_60pour100" src="https://github.com/user-attachments/assets/caed4bdb-2aff-4064-a469-b2ac6b54495a" />
<img width="800" height="480" alt="6-pwm_complementaire_decalee" src="https://github.com/user-attachments/assets/6e53629f-3a36-49a3-9093-57d7237cbf37" />

