// Copyright (c) 2017 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.
//
// ---------------------------------------------------------------------------
//
// This file was generated by the CEF translator tool. If making changes by
// hand only do so within the body of existing method and function
// implementations. See the translator.README.txt file in the tools directory
// for more information.
//
// $hash=dffd7542aef11aa1adb55bd8c5dd6c29081f5caf$
//

#include "libcef_dll/cpptoc/views/menu_button_pressed_lock_cpptoc.h"

// CONSTRUCTOR - Do not edit by hand.

CefMenuButtonPressedLockCppToC::CefMenuButtonPressedLockCppToC() {}

template <>
CefRefPtr<CefMenuButtonPressedLock>
CefCppToCRefCounted<CefMenuButtonPressedLockCppToC,
                    CefMenuButtonPressedLock,
                    cef_menu_button_pressed_lock_t>::
    UnwrapDerived(CefWrapperType type, cef_menu_button_pressed_lock_t* s) {
  NOTREACHED() << "Unexpected class type: " << type;
  return NULL;
}

#if DCHECK_IS_ON()
template <>
base::AtomicRefCount CefCppToCRefCounted<
    CefMenuButtonPressedLockCppToC,
    CefMenuButtonPressedLock,
    cef_menu_button_pressed_lock_t>::DebugObjCt ATOMIC_DECLARATION;
#endif

template <>
CefWrapperType
    CefCppToCRefCounted<CefMenuButtonPressedLockCppToC,
                        CefMenuButtonPressedLock,
                        cef_menu_button_pressed_lock_t>::kWrapperType =
        WT_MENU_BUTTON_PRESSED_LOCK;
