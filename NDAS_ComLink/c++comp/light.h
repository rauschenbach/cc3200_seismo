#ifndef LightH
#define LightH
//--------------------------------------------------------------------------------
#include <SysUtils.hpp>
#include <Controls.hpp>
#include <Classes.hpp>
#include <Forms.hpp>
#include <ExtCtrls.hpp>
//--------------------------------------------------------------------------------
class PACKAGE TLight : public TShape
{
 private:
  // Объявления членов данных свойств
  bool      FState;
  TColor    FOnColor;
  TColor    FOffColor;
  //Запись членов данных
  void  __fastcall SetOnOff(const bool Value);
  void  __fastcall SetOnColor(const TColor OnColor);
  void  __fastcall SetOffColor(const TColor OffColor);
 protected:
 public:
    __fastcall TLight(TComponent* Owner);
 //Объявление свойств
 __published:

   __property bool State =
   { read=FState,write=SetOnOff};
   __property TColor OnColor =
   { read=FOnColor,write=SetOnColor};
   __property TColor OffColor=
   { read= FOffColor,write=SetOffColor};
};   
//--------------------------------------------------------------------------------
#endif









 
