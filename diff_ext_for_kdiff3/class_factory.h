/*
  SPDX-FileCopyrightText: 2003-2006, Sergey Zorin. All rights reserved.
  SPDX-FileCopyrightText:  2018-2020 Michael Reeves reeves.87@gmail.com
  SPDX-License-Identifier: BSD-2-Clause
*/

#ifndef class_factory_h
#define class_factory_h

#include <shlobj.h>
#include <shlguid.h>

class CLASS_FACTORY : public IClassFactory {
  public:
    CLASS_FACTORY();
    virtual ~CLASS_FACTORY();

    //IUnknown members
    STDMETHODIMP QueryInterface(REFIID, void**);
    STDMETHODIMP_(ULONG) AddRef();
    STDMETHODIMP_(ULONG) Release();

    //ICLASS_FACTORY members
    STDMETHODIMP CreateInstance(IUnknown*, REFIID, void**);
    STDMETHODIMP LockServer(BOOL);

  private:
    ULONG _ref_count;
};

#endif //class_factory_h
