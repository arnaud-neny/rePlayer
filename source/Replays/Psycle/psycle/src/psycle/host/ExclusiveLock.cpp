#include <psycle/host/detail/project.private.hpp>
#include "ExclusiveLock.hpp"

///////////////////////////////// Implementation //////////////////////////////

CExclusiveLock::CExclusiveLock(CSemaphore* in_semaphore, int places, bool lock_now)
:semaphore(in_semaphore), num_places(places), locked(false) {
	if(lock_now) Lock();
}

CExclusiveLock::~CExclusiveLock()
{
	if (locked) { UnLock(); }
}
bool CExclusiveLock::Lock(int timeout) {
	if (locked) return true;
	locked = true;
	for(int i=0;i<num_places;i++) locked &= (bool)semaphore->Lock(timeout);
	return locked;
}
void CExclusiveLock::UnLock() {
	if(!locked) return;
	for(int i=0;i<num_places;i++) semaphore->Unlock();
	locked=false;
}
