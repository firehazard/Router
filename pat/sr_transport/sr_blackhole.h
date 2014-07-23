/*-----------------------------------------------------------------------------
 * file:  sr_blackhole.h
 * date:  Mon Nov 17 14:21:34 PST 2003 
 * Author: Martin Casado 
 *
 * Description:
 *
 *---------------------------------------------------------------------------*/

#ifndef SR_BLACKHOLE_H
#define SR_BLACKHOLE_H

#include "sr_router.h"

void sr_blackhole(struct sr_core* sr, uint8_t* packet /* given */, uint8_t reason);

#endif  /* SR_BLACKHOLE_H */
