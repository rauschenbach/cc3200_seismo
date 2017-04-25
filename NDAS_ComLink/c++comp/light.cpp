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


//����������� TShape
__fastcall TLight::TLight(TComponent* Owner) : TShape(Owner)
{
 Width = 25;//������
 Height =25;//������
 FOnColor =  clLime;//�������-�������
 FOffColor = clRed;//�������-��������
 FState = false;   //�������� ��-���������
 Shape = stEllipse;//� ����� �������
 Pen->Color = clBlack;//������ ������
 Pen->Width = 1;//������ �������
 Brush->Color=FOffColor;//���� �������
}
//---------------------------------------------------------------------------------
//������ �������� State
void __fastcall TLight::SetOnOff(const bool Value)
{
  FState = Value;
  Brush->Color=(FState)? FOnColor : FOffColor;
}
//---------------------------------------------------------------------------------
//������ �������� OnColor
void __fastcall  TLight::SetOnColor(const TColor OnColor)
{
 FOnColor = OnColor;
 Brush->Color=(FState)? FOnColor : FOffColor;
}
//---------------------------------------------------------------------------------
//������ �������� OffColor
void __fastcall TLight::SetOffColor(const TColor OffColor)
{
 FOffColor = OffColor;
 Brush->Color=(FState)?   FOnColor : FOffColor;
}

//---------------------------------------------------------------------------------
//�����������
namespace Light
{
  void __fastcall PACKAGE Register()
  {
   TComponentClass classes[1] = { __classid(TLight)};
   RegisterComponents("User",classes,0);
  }
}  