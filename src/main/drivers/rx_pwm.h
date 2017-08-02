/*
 * This file is part of Cleanflight.
 *
 * Cleanflight is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Cleanflight is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Cleanflight.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include "drivers/io_types.h"

typedef enum {
    INPUT_FILTERING_DISABLED = 0,
    INPUT_FILTERING_ENABLED
} inputFilteringMode_e;

#define PPM_RCVR_TIMEOUT            0
#define PWM_INPUT_PORT_COUNT        8

typedef struct ppmConfig_s {
    ioTag_t ioTag;
} ppmConfig_t;

typedef struct pwmConfig_s {
    ioTag_t ioTags[PWM_INPUT_PORT_COUNT];
    inputFilteringMode_e inputFilteringMode;
} pwmConfig_t;

void ppmRxInit(const ppmConfig_t *ppmConfig, uint8_t pwmProtocol);
void pwmRxInit(const pwmConfig_t *pwmConfig);

uint16_t pwmRead(uint8_t channel);
uint16_t ppmRead(uint8_t channel);

bool isPPMDataBeingReceived(void);
void resetPPMDataReceivedState(void);

bool isPWMDataBeingReceived(void);
