#pragma once
#include <psycle/host/detail/project.hpp>
#include "Global.hpp"

class CExclusiveLock
{
public:
//Constructors / Destructors
  CExclusiveLock(CSemaphore* in_semaphore,int places, bool lock_now);
  ~CExclusiveLock();

  bool Lock(int timeout=-1);
  void UnLock();
protected:
	CSemaphore* semaphore;
	int		num_places;
	bool	locked;
};
