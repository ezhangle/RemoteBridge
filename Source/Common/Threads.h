/*
 * Copyright (C) 2010-2015 Nektra S.A., Buenos Aires, Argentina.
 * All rights reserved. Contact: http://www.nektra.com
 *
 *
 * This file is part of Remote Bridge
 *
 *
 * Commercial License Usage
 * ------------------------
 * Licensees holding valid commercial Remote Bridge licenses may use this
 * file in accordance with the commercial license agreement provided with
 * the Software or, alternatively, in accordance with the terms contained
 * in a written agreement between you and Nektra.  For licensing terms and
 * conditions see http://www.nektra.com/licensing/. Use the contact form
 * at http://www.nektra.com/contact/ for further information.
 *
 *
 * GNU General Public License Usage
 * --------------------------------
 * Alternatively, this file may be used under the terms of the GNU General
 * Public License version 3.0 as published by the Free Software Foundation
 * and appearing in the file LICENSE.GPL included in the packaging of this
 * file.  Please visit http://www.gnu.org/copyleft/gpl.html and review the
 * information to ensure the GNU General Public License version 3.0
 * requirements will be met.
 *
 **/

#ifndef _NKT_THREADS_H
#define _NKT_THREADS_H

#include <windows.h>
#include "WaitableObjects.h"

//-----------------------------------------------------------

class CNktThread;
class CNktWorkerThread;

//-----------------------------------------------------------

typedef VOID (__cdecl *LPNKT_WRKTHREAD_ROUTINE)(CNktWorkerThread *lpWrkThread, LPVOID lpParam);

//-----------------------------------------------------------

class CNktThread
{
public:
  CNktThread();
  virtual ~CNktThread();

  virtual BOOL Start(__in BOOL bSuspended=FALSE);
  virtual BOOL Stop(__in DWORD dwTimeout=INFINITE);
  virtual BOOL StopAsync();
  virtual BOOL Pause();
  virtual BOOL Resume();
  virtual BOOL IsRunning();
  virtual BOOL CheckForAbort(__in DWORD dwTimeout=0, __in DWORD dwEventCount=0,
                             __out_opt LPHANDLE lphEventList=NULL, __out_opt LPDWORD lpdwHitEvent=NULL);
  virtual BOOL MsgCheckForAbort(__in DWORD dwTimeout=INFINITE, __in DWORD dwEventCount=0,
                                __out_opt LPHANDLE lphEventList=NULL,
                                __out_opt LPDWORD lpdwHitEvent=NULL);

  static VOID SetThreadName(__in DWORD dwThreadId, __in_z_opt LPCSTR szName);
  virtual VOID SetThreadName(__in_z_opt LPCSTR szName);

  virtual BOOL SetPriority(__in int nPriority);
  virtual int GetPriority();

  virtual HANDLE GetKillEvent()
    {
    return cKillEvent.GetEventHandle();
    };

  virtual HANDLE GetThreadHandle()
    {
    return hThread;
    };
  virtual DWORD GetThreadId()
    {
    return dwThreadId;
    };

  virtual BOOL Wait(__in DWORD dwTimeout=0, __in DWORD dwEventCount=0,
                    __out_opt LPHANDLE lphEventList=NULL, __out_opt LPDWORD lpdwHitEvent=NULL);
  virtual BOOL MsgWait(__in DWORD dwTimeout=INFINITE, __in DWORD dwEventCount=0,
                       __out_opt LPHANDLE lphEventList=NULL, __out_opt LPDWORD lpdwHitEvent=NULL);
  virtual BOOL CoWait(__in DWORD dwTimeout=INFINITE, __in DWORD dwEventCount=0,
                      __out_opt LPHANDLE lphEventList=NULL, __out_opt LPDWORD lpdwHitEvent=NULL);

  virtual VOID ThreadProc()=0;

  static DWORD MsgWaitAndDispatchMessages(__in DWORD dwTimeout, __in DWORD dwEventsCount, __in LPHANDLE lpHandles);
  static DWORD CoWaitAndDispatchMessages(__in DWORD dwTimeout, __in DWORD dwEventsCount, __in LPHANDLE lpHandles);

protected:
  int nPriority;
  HANDLE hThread;
  DWORD dwThreadId;
  CNktEvent cKillEvent, cThreadEndedOK;
  DWORD dwInternalFlags;
  LPVOID lpLink;
};

//-----------------------------------------------------------

class CNktWorkerThread : public CNktThread
{
public:
  CNktWorkerThread(__in LPNKT_WRKTHREAD_ROUTINE lpStartRoutine, __in LPVOID lpParam=NULL);
  virtual ~CNktWorkerThread();

  virtual BOOL SetRoutine(__in LPNKT_WRKTHREAD_ROUTINE lpStartRoutine, __in LPVOID lpParam=NULL);

private:
  VOID ThreadProc();

private:
  LPNKT_WRKTHREAD_ROUTINE lpStartRoutine;
  LPVOID lpParam;
};

template <class TClass>
class TNktClassWorkerThread : public CNktThread
{
public:
  TNktClassWorkerThread()
    {
    lpObject = NULL;
    lpStartRoutine = NULL;
    };

  virtual BOOL Start(__in BOOL bSuspended=FALSE)
    {
    if (lpObject==NULL || (lpStartRoutine==NULL && lpStartRoutineWithParam==NULL))
      return FALSE;
    return CNktThread::Start(bSuspended);
    };
  
  virtual BOOL Start(__in TClass *_lpObject, __in VOID (TClass::* _lpStartRoutine)(),
                     __in BOOL bSuspended=FALSE)
    {
    lpObject = _lpObject;
    lpStartRoutine = _lpStartRoutine;
    lpStartRoutineWithParam = NULL;
    nParam = 0;
    return Start(bSuspended);
    }

  virtual BOOL Start(__in TClass *_lpObject, __in VOID (TClass::* _lpStartRoutine)(SIZE_T),
                     __in SIZE_T _nParam, __in BOOL bSuspended=FALSE)
    {
    lpObject = _lpObject;
    lpStartRoutine = NULL;
    lpStartRoutineWithParam = _lpStartRoutine;
    nParam = _nParam;
    return Start(bSuspended);
    }

private:
  virtual VOID ThreadProc()
    {
    if (lpStartRoutine != NULL)
      (*lpObject.*lpStartRoutine)();
    else
      (*lpObject.*lpStartRoutineWithParam)(nParam);
    return;
    }

private:
  TClass *lpObject;
  VOID (TClass::* lpStartRoutine)();
  VOID (TClass::* lpStartRoutineWithParam)(SIZE_T);
  SIZE_T nParam;
};

//-----------------------------------------------------------

class CNktThreadPool
{
public:
  CNktThreadPool();
  virtual ~CNktThreadPool();

  BOOL Initialize(__in ULONG nMinWorkerThreads, __in ULONG nWorkerThreadsCreateAhead,
                  __in ULONG nThreadShutdownThresholdMs);
  VOID Finalize();

  BOOL IsInitialized();

  BOOL QueueTask(__in LPTHREAD_START_ROUTINE lpRoutine, __in LPVOID lpContext);

  virtual LPVOID OnThreadStarted();
  virtual VOID OnThreadTerminated(__in LPVOID lpContext);

  virtual VOID OnTaskTerminated(__in LPVOID lpContext, __in HRESULT hReturnValue) = 0;
  virtual VOID OnTaskCancelled(__in LPVOID lpContext) = 0;
  virtual VOID OnTaskExceptionError(__in LPVOID lpContext, __in DWORD dwException,
                                    __in struct _EXCEPTION_POINTERS *excPtr) = 0;

private:
  typedef TNktClassWorkerThread<CNktThreadPool> CWorkerThread;

  typedef struct tagDLLIST_ITEM {
    struct tagDLLIST_ITEM *lpNext, *lpPrev;
  } DLLIST_ITEM;

  typedef struct tagWORKITEM {
    DLLIST_ITEM sLink;
    LPTHREAD_START_ROUTINE lpRoutine;
    LPVOID lpContext;
  } WORKITEM;

private:
  DWORD InitializeWorker();
  VOID RemoveWorker(__in CWorkerThread *lpWorker);
  VOID RemoveTask(__in WORKITEM *lpWorkItem);
  VOID ExecuteTask(__in WORKITEM *lpWorkItem);
  VOID CancelTask(__in WORKITEM *lpWorkItem);
  VOID WorkerThreadProc(__in SIZE_T nParam);

private:
  CNktFastMutex cMtx;
  HANDLE hIOCP;
  ULONG nMinWorkerThreads;
  ULONG nWorkerThreadsCreateAhead;
  ULONG nThreadShutdownThresholdMs;
  struct {
    LONG volatile nMtx;
    DLLIST_ITEM sList;
  } sWorkItems;
  struct {
    LONG volatile nMtx;
    CWorkerThread **lplpWorkerThreadsList;
    SIZE_T nCount, nSize;
  } sActiveThreads;
  LONG volatile nInUse;
};

//-----------------------------------------------------------

#endif //_NKT_THREADS_H
