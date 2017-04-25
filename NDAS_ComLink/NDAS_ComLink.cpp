//---------------------------------------------------------------------------
#include <vcl.h>
#pragma hdrstop
#include <time.h>
#include <time.h>
#include <stdio.h>
//---------------------------------------------------------------------------
USEFORM("main.cpp", frmMain);
USEFORM("About.cpp", frmAbout);
//---------------------------------------------------------------------------
WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
  HANDLE hWnd = NULL;
  DecimalSeparator = '.';


  hWnd = FindWindow(NULL, "NDAS ComLink");
  if(hWnd){
           ShowWindow(hWnd, SW_SHOWNORMAL);
           return 0;
  } else
        try
        {
                 Application->Initialize();
                 Application->CreateForm(__classid(TfrmMain), &frmMain);
                 Application->CreateForm(__classid(TfrmAbout), &frmAbout);
                 Application->Run();
        }
        catch (Exception &exception)
        {
                 Application->ShowException(&exception);
        }
        catch (...)
        {
                 try
                 {
                         throw Exception("");
                 }
                 catch (Exception &exception)
                 {
                         Application->ShowException(&exception);
                 }
        }


     return 0;
}
//---------------------------------------------------------------------------
