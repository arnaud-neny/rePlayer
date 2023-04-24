#ifndef HANDLER_H__
#define HANDLER_H__

#include "../normalize.h"
#include "../include/nezplug/nezplug.h"

#ifdef __cplusplus
extern "C" {
#endif

#define NES_RESET_SYS_FIRST 4
#define NES_RESET_SYS_NOMAL 8
#define NES_RESET_SYS_LAST 12

PROTECTED void NESReset(NEZ_PLAY *);
PROTECTED void NESResetHandlerInstall(NEZ_NES_RESET_HANDLER**, const NEZ_NES_RESET_HANDLER *ph);
PROTECTED void NESTerminate(NEZ_PLAY *);
PROTECTED void NESTerminateHandlerInstall(NEZ_NES_TERMINATE_HANDLER**, const NEZ_NES_TERMINATE_HANDLER *ph);
PROTECTED void NESHandlerInitialize(NEZ_NES_RESET_HANDLER**, NEZ_NES_TERMINATE_HANDLER *);
PROTECTED void NESHandlerTerminate(NEZ_NES_RESET_HANDLER**, NEZ_NES_TERMINATE_HANDLER *);

#ifdef __cplusplus
}
#endif

#endif /* HANDLER_H__ */
