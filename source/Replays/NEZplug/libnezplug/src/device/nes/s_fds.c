#include "s_fds.h"
#include "../../format/m_nsf.h"

PROTECTED void FDS1SoundInstall(NEZ_PLAY*);
PROTECTED void FDS2SoundInstall(NEZ_PLAY*);
PROTECTED void FDS3SoundInstall(NEZ_PLAY*);

PROTECTED void FDSSoundInstall(NEZ_PLAY *pNezPlay)
{
	switch (((NSFNSF*)pNezPlay->nsf)->fds_type)
	{
	case 1:
		FDS1SoundInstall(pNezPlay);
		break;
	case 3:
		FDS2SoundInstall(pNezPlay);
		break;
	default:
	case 2:
		FDS3SoundInstall(pNezPlay);
		break;
	}
}

#if 0
PROTECTED void FDSSelect(NEZ_PLAY *pNezPlay, unsigned type)
{
	if ((NSFNSF*)pNezPlay->nsf)
		((NSFNSF*)pNezPlay->nsf)->fds_type = type;
}
#endif
