/**
 *
 * @file
 *
 * @brief  Progress callback helpers implementation
 *
 * @author vitamin.caig@gmail.com
 *
 **/

// common includes
#include <progress_callback.h>

namespace Log
{
  class StubProgressCallback : public Log::ProgressCallback
  {
  public:
    void OnProgress(uint_t /*current*/) override {}

    void OnProgress(uint_t /*current*/, const String& /*message*/) override {}
  };

  ProgressCallback& ProgressCallback::Stub()
  {
    static StubProgressCallback stub;
    return stub;
  }
}  // namespace Log
