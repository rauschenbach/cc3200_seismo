//---------------------------------------------------------------------------

#ifndef FindStationsThreadH
#define FindStationsThreadH
//---------------------------------------------------------------------------
#include <Classes.hpp>
//---------------------------------------------------------------------------
int get_num_station();
void reset_num_station();
bool no_enter(void);

class FindStationsThread : public TThread
{            
private:
protected:
        void __fastcall Execute();
public:
        __fastcall FindStationsThread(bool CreateSuspended);
};
//---------------------------------------------------------------------------
#endif
