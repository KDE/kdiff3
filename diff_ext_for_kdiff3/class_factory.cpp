/*
  SPDX-FileCopyrightText: 2003-2006, Sergey Zorin. All rights reserved.
  SPDX-FileCopyrightText:  2018-2020 Michael Reeves reeves.87@gmail.com
  SPDX-License-Identifier: BSD-2-Clause
*/
#include "class_factory.h"
#include "diff_ext.h"
#include "server.h"

CLASS_FACTORY::CLASS_FACTORY() {
  _ref_count = 0L;

  SERVER::instance()->lock();
}

CLASS_FACTORY::~CLASS_FACTORY() {
  SERVER::instance()->release();
}

STDMETHODIMP
CLASS_FACTORY::QueryInterface(REFIID riid, void** ppv) {
  HRESULT ret = E_NOINTERFACE;
  *ppv = nullptr;

  if(IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_IClassFactory)) {
    *ppv = static_cast<CLASS_FACTORY*>(this);

    AddRef();

    ret = NOERROR;
  }

  return ret;
}

STDMETHODIMP_(ULONG)
CLASS_FACTORY::AddRef() {
  return InterlockedIncrement((LPLONG)&_ref_count);
}

STDMETHODIMP_(ULONG)
CLASS_FACTORY::Release() {
  ULONG ret = 0L;

  if(InterlockedDecrement((LPLONG)&_ref_count) != 0)
    ret = _ref_count;
  else
    delete this;

  return ret;
}

STDMETHODIMP
CLASS_FACTORY::CreateInstance(IUnknown* outer, REFIID refiid, void** obj) {
  HRESULT ret = CLASS_E_NOAGGREGATION;
  *obj = 0;

  // Shell extensions typically don't support aggregation (inheritance)
  if(outer == nullptr) {
    DIFF_EXT* ext = new DIFF_EXT();

    if(ext == nullptr)
      ret = E_OUTOFMEMORY;
    else
      ret = ext->QueryInterface(refiid, obj);
  }

  return ret;
}

STDMETHODIMP
CLASS_FACTORY::LockServer(BOOL) {
  return NOERROR;
}
