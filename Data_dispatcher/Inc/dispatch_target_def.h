/*
 * dispatch_target_def.h
 *
 *  Created on: Jun 7, 2026
 *      Author: ADMIN
 */

#ifndef INC_DISPATCH_TARGET_DEF_H_
#define INC_DISPATCH_TARGET_DEF_H_

#include "stdint.h"

typedef uint32_t DispatchTypeU32_t;

#define DISPATCH_NONE		0x00
#define DISPATCH_LOGGER		0x01
#define DISPATCH_SD			0x02
#define DISPATCH_PACKET		0x04
#define DISPATCH_ALL		(DISPATCH_PACKET | DISPATCH_SD) /* 0x06 */

#endif /* INC_DISPATCH_TARGET_DEF_H_ */
