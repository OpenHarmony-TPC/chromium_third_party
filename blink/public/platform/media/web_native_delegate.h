
#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_MEDIA_WEB_NATIVE_DELEGATE_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_MEDIA_WEB_NATIVE_DELEGATE_H_

#include "third_party/blink/public/platform/web_common.h"

namespace blink {

// An interface to collect WebNativeBridge state changes and to fan out commands
// from the browser.
class BLINK_PLATFORM_EXPORT WebNativeDelegate {
 public:
  class Observer {
   public:
    // Called when the host frame is hidden (usually by tab switching).
    // Note: OnFrameHidden() is not called when the frame is closed, even though
    // IsFrameHidden() will start returning true.
    virtual void OnFrameHidden() = 0;

    // Called when the host frame is shown (usually by tab switching).
    virtual void OnFrameShown() = 0;
  };

  // Returns true if the host frame is hidden or closed.
  virtual bool IsFrameHidden() = 0;

  // Subscribe to observer callbacks. A player must use the returned |player_id|
  // for the rest of the calls below.
  virtual int AddObserver(Observer* observer) = 0;

  // Unsubscribe from observer callbacks.
  virtual void RemoveObserver(int player_id) = 0;

 protected:
  WebNativeDelegate() = default;
  virtual ~WebNativeDelegate() = default;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_PLATFORM_MEDIA_WEB_NATIVE_DELEGATE_H_
