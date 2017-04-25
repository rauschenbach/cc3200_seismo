//---------------------------------------------------------------------------
#include <vcl.h>
#pragma hdrstop
#include "Light.h"
#pragma package(smart_init)
//---------------------------------------------------------------------------
static inline void ValidCtrCheck(TLight*)
{
 new TLight(NULL);
}


//Конструктор TShape
__fastcall TLight::TLight(TComponent* Owner) : TShape(Owner)
{
 Width = 25;//Ширина
 Height =25;//Высота
 FOnColor =  clLime;//Зеленый-включен
 FOffColor = clRed;//Красный-выключен
 FState = false;   //выключен по-умолчанию
 Shape = stEllipse;//В форме эллипса
 Pen->Color = clBlack;//Черный контур
 Pen->Width = 1;//Ширина контура
 Brush->Color=FOffColor;//Цвет заливки
}
//---------------------------------------------------------------------------------
//Запись свойства State
void __fastcall TLight::SetOnOff(const bool Value)
{
  FState = Value;
  Brush->Color=(FState)? FOnColor : FOffColor;
}
//---------------------------------------------------------------------------------
//Запись свойства OnColor
void __fastcall  TLight::SetOnColor(const TColor OnColor)
{
 FOnColor = OnColor;
 Brush->Color=(FState)? FOnColor : FOffColor;
}
//---------------------------------------------------------------------------------
//Запись свойства OffColor
void __fastcall TLight::SetOffColor(const TColor OffColor)
{
 FOffColor = OffColor;
 Brush->Color=(FState)?   FOnColor : FOffColor;
}

//---------------------------------------------------------------------------------
//Регистрация
namespace Light
{
  void __fastcall PACKAGE Register()
  {
   TComponentClass classes[1] = { __classid(TLight)};
   RegisterComponents("User",classes,0);
  }
}  