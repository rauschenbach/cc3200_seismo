//---------------------------------------------------------------------------

#ifndef WriteCommandThreadH
#define WriteCommandThreadH
//---------------------------------------------------------------------------
#include <Classes.hpp>
//---------------------------------------------------------------------------
class WriteCommandThread : public TThread
{            
private:
      void* param;
      bool  app;
      void (*thread_func) (void*);	        /* ”казатель на функцыю, которую будем вызывать из потока */
protected:
        void __fastcall Execute();
public:
        __fastcall WriteCommandThread(void *func, void* p, bool a, bool CreateSuspended);
};
//---------------------------------------------------------------------------
#endif
