/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "Crypto.h"
#include "jsfriendapi.h"
#include "nsCOMPtr.h"
#include "nsIRandomGenerator.h"
#include "nsPIDOMWindow.h"
#include "MainThreadUtils.h"
#include "nsXULAppAPI.h"

#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/CryptoBinding.h"
#include "nsServiceManagerUtils.h"

using mozilla::dom::ContentChild;

namespace mozilla {
namespace dom {

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(Crypto)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_INTERFACE_MAP_ENTRY(nsIDOMCrypto)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(Crypto)
NS_IMPL_CYCLE_COLLECTING_RELEASE(Crypto)

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(Crypto, mWindow, mSubtle)

Crypto::Crypto()
{
  MOZ_COUNT_CTOR(Crypto);
  SetIsDOMBinding();
}

Crypto::~Crypto()
{
  MOZ_COUNT_DTOR(Crypto);
}

void
Crypto::Init(nsIDOMWindow* aWindow)
{
  mWindow = do_QueryInterface(aWindow);
  MOZ_ASSERT(mWindow);
}

/* virtual */ JSObject*
Crypto::WrapObject(JSContext* aCx)
{
  return CryptoBinding::Wrap(aCx, this);
}

void
Crypto::GetRandomValues(JSContext* aCx, const ArrayBufferView& aArray,
			JS::MutableHandle<JSObject*> aRetval,
			ErrorResult& aRv)
{
  NS_ABORT_IF_FALSE(NS_IsMainThread(), "Called on the wrong thread");

  JS::Rooted<JSObject*> view(aCx, aArray.Obj());

  // Throw if the wrong type of ArrayBufferView is passed in
  // (Part of the Web Crypto API spec)
  switch (JS_GetArrayBufferViewType(view)) {
    case js::Scalar::Int8:
    case js::Scalar::Uint8:
    case js::Scalar::Uint8Clamped:
    case js::Scalar::Int16:
    case js::Scalar::Uint16:
    case js::Scalar::Int32:
    case js::Scalar::Uint32:
      break;
    default:
      aRv.Throw(NS_ERROR_DOM_TYPE_MISMATCH_ERR);
      return;
  }

  aArray.ComputeLengthAndData();
  uint32_t dataLen = aArray.Length();
  if (dataLen == 0) {
    NS_WARNING("ArrayBufferView length is 0, cannot continue");
    aRetval.set(view);
    return;
  } else if (dataLen > 65536) {
    aRv.Throw(NS_ERROR_DOM_QUOTA_EXCEEDED_ERR);
    return;
  }

  uint8_t* data = aArray.Data();

  if (XRE_GetProcessType() != GeckoProcessType_Default) {
    InfallibleTArray<uint8_t> randomValues;
    // Tell the parent process to generate random values via PContent
    ContentChild* cc = ContentChild::GetSingleton();
    if (!cc->SendGetRandomValues(dataLen, &randomValues) ||
        randomValues.Length() == 0) {
      aRv.Throw(NS_ERROR_FAILURE);
      return;
    }
    NS_ASSERTION(dataLen == randomValues.Length(),
                 "Invalid length returned from parent process!");
    memcpy(data, randomValues.Elements(), dataLen);
  } else {
    uint8_t *buf = GetRandomValues(dataLen);

    if (!buf) {
      aRv.Throw(NS_ERROR_FAILURE);
      return;
    }

    memcpy(data, buf, dataLen);
    NS_Free(buf);
  }

  aRetval.set(view);
}

SubtleCrypto*
Crypto::Subtle()
{
  if(!mSubtle) {
    mSubtle = new SubtleCrypto(GetParentObject());
  }
  return mSubtle;
}

#ifndef MOZ_DISABLE_CRYPTOLEGACY
// Stub out the legacy nsIDOMCrypto methods. The actual
// implementations are in security/manager/ssl/src/nsCrypto.{cpp,h}

NS_IMETHODIMP
Crypto::GetEnableSmartCardEvents(bool *aEnableSmartCardEvents)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
Crypto::SetEnableSmartCardEvents(bool aEnableSmartCardEvents)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

bool
Crypto::EnableSmartCardEvents()
{
  return false;
}

void
Crypto::SetEnableSmartCardEvents(bool aEnable, ErrorResult& aRv)
{
  aRv.Throw(NS_ERROR_NOT_IMPLEMENTED);
}

void
Crypto::GetVersion(nsString& aVersion)
{
}

mozilla::dom::CRMFObject*
Crypto::GenerateCRMFRequest(JSContext* aContext,
                            const nsCString& aReqDN,
                            const nsCString& aRegToken,
                            const nsCString& aAuthenticator,
                            const nsCString& aEaCert,
                            const nsCString& aJsCallback,
                            const Sequence<JS::Value>& aArgs,
                            ErrorResult& aRv)
{
  aRv.Throw(NS_ERROR_NOT_IMPLEMENTED);
  return nullptr;
}

void
Crypto::ImportUserCertificates(const nsAString& aNickname,
                               const nsAString& aCmmfResponse,
                               bool aDoForcedBackup,
                               nsAString& aReturn,
                               ErrorResult& aRv)
{
  aRv.Throw(NS_ERROR_NOT_IMPLEMENTED);
}

void
Crypto::SignText(JSContext* aContext,
                 const nsAString& aStringToSign,
                 const nsAString& aCaOption,
                 const Sequence<nsCString>& aArgs,
                 nsAString& aReturn)

{
  aReturn.AssignLiteral("error:internalError");
}

void
Crypto::Logout(ErrorResult& aRv)
{
  aRv.Throw(NS_ERROR_NOT_IMPLEMENTED);
}

#endif

/* static */ uint8_t*
Crypto::GetRandomValues(uint32_t aLength)
{
  nsCOMPtr<nsIRandomGenerator> randomGenerator;
  nsresult rv;
  randomGenerator = do_GetService("@mozilla.org/security/random-generator;1");
  NS_ENSURE_TRUE(randomGenerator, nullptr);

  uint8_t* buf;
  rv = randomGenerator->GenerateRandomBytes(aLength, &buf);

  NS_ENSURE_SUCCESS(rv, nullptr);

  return buf;
}

} // namespace dom
} // namespace mozilla
