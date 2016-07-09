/*
 * Copyright (c) 2003, Sergey Zorin. All rights reserved.
 *
 * This software is distributable under the BSD license. See the terms
 * of the BSD license in the LICENSE file provided with this software.
 *
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
  *ppv = 0;

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
  if(outer == 0) {
    DIFF_EXT* ext = new DIFF_EXT();
  
    if(ext == 0)
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
