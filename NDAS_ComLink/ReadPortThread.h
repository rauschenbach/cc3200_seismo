//---------------------------------------------------------------------------

#ifndef ReadPortThreadH
#define ReadPortThreadH
//---------------------------------------------------------------------------
#include <Classes.hpp>
//---------------------------------------------------------------------------
class ReadPortThread : public TThread
{            
private:
protected:
        void __fastcall Execute();
        unsigned long tim;
public:
        __fastcall ReadPortThread(bool CreateSuspended);
        bool bPick;
};
//---------------------------------------------------------------------------
#endif
