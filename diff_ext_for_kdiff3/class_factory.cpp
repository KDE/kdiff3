/*
 * Copyright (c) 2003, Sergey Zorin. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:

 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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
