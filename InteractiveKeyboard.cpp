
/* 
 * Copyright (C) 2016 Meena Erian <meena5.erian@gmail.com>
 */

//#define DEBUG_MODE
#include <windows.h>
#include <stdio.h>
#ifdef DEBUG_MODE
#include <iostream>
#include <fstream>
using namespace std;
#endif // DEBUG_MODE
class KeyboardReaction
{
public:
    char type; //0 for sequence key shortcut, 1 for synchronous key shortcut
    unsigned char keys[32]; //[required] each byte is a key
    unsigned nKeys; //number of keys stored in 'unsigned char keys'
    char cmd[256]; //[required] ShellExecute command
    char wClassName[32]; //[optional] Window class name
    char show; //1 to run in SW_SHOW mode, 0 for SW_HIDE
    KeyboardReaction * next;
    KeyboardReaction()
    {
        nKeys=0;
        next=0;
    }
    void run()
    {
        ShellExecute(NULL, "open",cmd,NULL, NULL, show?SW_SHOW:SW_HIDE);
    }
};

/**required container functions**/
//clear()/////
//push_back(element)/////
//size()/////
//KeyboardReaction&operator[unsigned]/////
class KeyboardReactionContainer
{
    unsigned ElementsInContainer;
    KeyboardReaction * pFirst;
public:
    KeyboardReactionContainer()
    {
        pFirst=0;
        ElementsInContainer=0;
    }
    KeyboardReaction&operator[](unsigned id)
    {
        KeyboardReaction * r = pFirst;
        while(id>0)
        {
            r = r[0].next;
            id--;
        }
        return r[0];
    }
    void push_back(KeyboardReaction kl)
    {
        if(!ElementsInContainer)
        {
            pFirst=(KeyboardReaction*)GlobalAlloc(0, sizeof(KeyboardReaction));
            pFirst[0]=kl;
        }
        else
        {
            this[0][ElementsInContainer-1].next = (KeyboardReaction*)GlobalAlloc(0, sizeof(KeyboardReaction));
            this[0][ElementsInContainer-1].next[0] = kl;
        }
        ElementsInContainer++;
    }
    unsigned clear()
    {
        unsigned ElementsDestroyed=0;
        KeyboardReaction* pkl=pFirst;
        KeyboardReaction* temp;
        while(pkl)
        {
            temp = pkl;
            pkl = pkl[0].next;
            GlobalFree(temp);
            ElementsDestroyed++;
        }
        ElementsInContainer=0;
        pFirst=0;
        return ElementsDestroyed;
    }
    unsigned size(){return ElementsInContainer;}
};

KeyboardReactionContainer InstalledKeyboardReactions, ActiveKeyboardReactions;

void LoadKeyboardReactions()
{
    InstalledKeyboardReactions.clear();//clear key shortcuts to reload them
    DWORD dwAttrib = GetFileAttributes("KeyboardReactions.dat");
    if(dwAttrib == INVALID_FILE_ATTRIBUTES || dwAttrib == FILE_ATTRIBUTE_DIRECTORY)exit(0);
    else
    {
        FILE *fp = fopen("KeyboardReactions.dat","r");
        char c;
        KeyboardReaction tempk;
        char step=0;
        int pointer=0;

        while(!feof(fp))
        {
            c = fgetc(fp);
            if(!step)//"~" or "&" (required)
            {
                if(c=='~')tempk.type=0;
                else if(c=='&')tempk.type=1;
                else step--;//exit(1);
                step++;// switch to the keys section
            }
            else if(step==1)//keys section (required | variable size, ends with #)
            {
                if(c=='#')
                {
                    if(pointer<2)exit(2);// terminate the application if this required field was not found
                    step++;// switch to cmd
                    pointer=0;// reset for next step
                }
                else
                {
                    unsigned char v;
                    if((c>='0'&&c<='9'))v = c-'0';
                    else if(c>='A'&&c<='F')v = c - '7';
                    else exit(3);
                    if(pointer%2)tempk.keys[pointer/2]+=v;//least significant 4 bit
                    else
                    {
                        tempk.keys[pointer/2]=v*16;//most significant
                        tempk.nKeys++;
                    }
                    pointer++;
                }
            }
            else if(step==2)//command line (required | variable size, ends with * or #)
            {
                if(c=='*')
                {
                    if(!pointer)exit(4);// terminate the application if this required field was not found
                    step+=2; //skip class name
                    tempk.cmd[pointer]=0; // end cmd
                    pointer=0;// reset for next step
                    tempk.show=1;
                    tempk.wClassName[0]=0; //skip class name
                }
                else if(c=='#')
                {
                    if(!pointer)exit(4);// terminate the application if this required field was not found
                    step++; // switch to class name
                    tempk.cmd[pointer]=0;// end cmd
                    pointer=0;// reset for next step
                }
                else
                {
                    tempk.cmd[pointer]=c;
                    if(pointer==255)exit(5);
                    pointer++;
                }
            }
            else if(step==3)//window class name (optional)
            {
                if(c=='*')
                {
                    if(!pointer)exit(4);// terminate the application if this required field was not found
                    step++;
                    tempk.wClassName[pointer]=0;
                    pointer=0;
                    tempk.show=1;
                }
                else
                {
                    tempk.wClassName[pointer]=c;
                    if(pointer==255)exit(5);
                    pointer++;
                }
            }
            else if(step==4)//H for SW_HIDE (optional)
            {
                if(c=='H')tempk.show=0;
                else if(c==10)
                {
                    InstalledKeyboardReactions.push_back(tempk);
                    tempk.nKeys=0;
                    step=0;
                }
                //else...
            }
        }
        fclose(fp);
        if(!InstalledKeyboardReactions.size())exit(0);
    }
}

int main()
{
    HWND ActiveWindow, NewActiveWindow;
    char ActiveWindowClassName[32], NewActiveWindowClassName[32];
    BYTE k1[256];
    BYTE k2[256];
    //  ******* Thoughts *******
    //save key-press events only. Note: it may include more than one key-press and some key-release.
    //Keys like "Ctrl", "Shift", "Alt" sends two key-press events (it's not a problem for sync KeyboardReactions. the problem is with sequence shortcuts)
    //to solve the above problem, ignore the side-specific key events
    //Ignore VK_LSHIFT VK_RSHIFT VK_LCONTROL VK_RCONTROL VK_LMENU VK_RMENU

    //On each key-press event, check if any of the pressed keys is the last in any of the loaded KeyboardReactions
    bool changed=false;
    BYTE PressedKeys[8];
    unsigned nPressedKeys=0;
    BYTE LastPressed[32];
    unsigned nLastPressed=0;
    DWORD dwAttrib;
    LoadKeyboardReactions();
#ifdef DEBUG_MODE
    ofstream file("InstalledKeyboardReactions.txt");
    for(int i=0; i<InstalledKeyboardReactions.size(); i++)
    {
        InstalledKeyboardReactions[i].keys[InstalledKeyboardReactions[i].nKeys]=0;
        file<<"Keys:"<<InstalledKeyboardReactions[i].keys<<endl;
        file<<"nKeys:"<<InstalledKeyboardReactions[i].nKeys<<endl;
        file<<"CMD:"<<InstalledKeyboardReactions[i].cmd<<endl;
        file<<"wClassName:"<<InstalledKeyboardReactions[i].wClassName<<endl;
        file<<"\n\n";
    }
    file.close();
#endif // DEBUG_MODE
    while(1)
    {
        /**                           scanning algorithm
        /**
        /** if(the ActiveWidow was switched to a new class name),
        /**   compare the new active window class with all InstalledKeyboardReactions
        /**    to rebuild the ActiveKeyboardReactions container
        /** if(a key was pressed),
        /**   check if it matches the last key of any of the ActiveKeyboardReactions;
        /**   if(it does), resume the scanning as in the previous version
        /**
        /**/

        Sleep(24);
        /*********************************Keyboard Scanner***********************************/
        GetKeyState(0);
        GetKeyboardState(k1);
        for(int i=0; i<256; i++)
        {
            if(k1[i]!=k2[i])//when keyboard state change
            {
                if(!changed)changed=true;
                if(k1[i]-3>k2[i])
                {
                    if(i!=VK_SHIFT && i!=VK_CONTROL && i!=VK_MENU)
                    {
                        #ifdef DEBUG_MODE
                        cout<<"keypress detected!\n"<<"VK_Code:"<<(HANDLE)i<<endl;
                        #endif // DEBUG_MODE
                        PressedKeys[nPressedKeys]=i;
                        nPressedKeys++;
                    }
                }
            }
        }
        if(changed)
        {
            /**Reactions**/
            if(nPressedKeys)
            {
                for(unsigned int ip=0; ip<nPressedKeys; ip++)//check if any of the pressed keys is the last in any of the loaded KeyboardReactions
                {
                    for(unsigned int il=0; il<ActiveKeyboardReactions.size(); il++)
                    {
                        if(PressedKeys[ip]==ActiveKeyboardReactions[il].keys[ActiveKeyboardReactions[il].nKeys-1])
                        {
                            if(ActiveKeyboardReactions[il].type)//sync
                            {
                               bool Active=true;
                               for(unsigned int i=0; i<ActiveKeyboardReactions[il].nKeys-1; i++)
                               {
                                   if(k1[(int)ActiveKeyboardReactions[il].keys[i]]<2)
                                   {
                                       Active=false;
                                       break;
                                   }
                               }
                               if(Active)ActiveKeyboardReactions[il].run();
                            }
                            else//sequence
                            {
                                #ifdef DEBUG_MODE
                                cout<<"Possible sequence match detected!\n";
                                #endif // DEBUG_MODE
                                bool Active=true;
                                int lpi=nLastPressed-1;
                                int kli=ActiveKeyboardReactions[il].nKeys-2;
                                while(lpi>=0 && kli>=0)
                                {
                                    if(ActiveKeyboardReactions[il].keys[kli]!=LastPressed[lpi])
                                    {
                                        #ifdef DEBUG_MODE
                                        cout<<"Wrong! No match between Keylog "<<(HWND)ActiveKeyboardReactions[il].keys[kli]<<" and "<< (HWND)LastPressed[lpi]<<endl;
                                        #endif // DEBUG_MODE
                                        Active=false;
                                        break;
                                    }
                                    lpi--;
                                    kli--;
                                }
                                if(Active)ActiveKeyboardReactions[il].run();
                            }
                        }
                    }
                }
                int LastPressedShift = nLastPressed + nPressedKeys - 32;
                for(int from=LastPressedShift, to=0; from>0&&from<32; from++,to++)
                {
                    LastPressed[to] = LastPressed[from];
                }
                if(LastPressedShift)nLastPressed-=LastPressedShift;
                for(unsigned ilp=nLastPressed, ipk=0; ipk<nPressedKeys; ilp++, ipk++)
                {
                    LastPressed[ilp] = PressedKeys[ipk];
                    nLastPressed++;
                }
                nPressedKeys=0;
            }
            /**End Reactions**/
            for(int i=0; i<256; i++)k2[i]=k1[i];//clear history
            changed=false;//clear history
        }
        /******************************Active Window Scanner*********************************/
        NewActiveWindow = GetForegroundWindow();
        if(NewActiveWindow!=ActiveWindow)
        {
            #ifdef DEBUG_MODE
            cout<<"Foreground window switched\n";
            #endif // DEBUG_MODE
            GetClassNameA(NewActiveWindow, NewActiveWindowClassName, 32);
            if(strcmp(NewActiveWindowClassName, ActiveWindowClassName)!=0)
            {

                /**the active window was switched to a window of another class name**/
                ActiveKeyboardReactions.clear();
                unsigned i=0;
                for(KeyboardReaction * k = &InstalledKeyboardReactions[0]; i<InstalledKeyboardReactions.size(); i++, k = k[0].next)
                {
                    #ifdef DEBUG_MODE
                    cout<<"-InstalledKeyboardReactions["<<i<<"].wClassName = \""<<k[0].wClassName<<"\";\n";
                    #endif // DEBUG_MODE
                    if(k[0].wClassName[0]==0 || !strcmp(k[0].wClassName, NewActiveWindowClassName))
                        ActiveKeyboardReactions.push_back(k[0]);
                }
                strcpy(ActiveWindowClassName, NewActiveWindowClassName);
                #ifdef DEBUG_MODE
                cout<<"Window switched to a window of class name : "<<NewActiveWindowClassName<<endl;
                cout<<ActiveKeyboardReactions.size()<<" KeyboardReactions are currently active.\n";
                #endif // DEBUG_MODE
            }
            #ifdef DEBUG_MODE
            else
            cout<<"But foreground window class name was not changed.\n";
            #endif // DEBUG_MODE
            ActiveWindow = NewActiveWindow;
        }
        /********************************Data File Scanner***********************************/
        dwAttrib = GetFileAttributes("ReloadKeyboardReactions.dat");
        if(dwAttrib != INVALID_FILE_ATTRIBUTES && dwAttrib != FILE_ATTRIBUTE_DIRECTORY)
        {
            DeleteFileA("ReloadKeyboardReactions.dat");
            LoadKeyboardReactions();
        }
    }
}

/**                           KeyboardReactions script language protocol
 **
 ** +Keyboard reactions must be saved in a file named KeyboardReactions.dat in the same directory
 ** +Each line represent a KeyboardReaction each KeyboardReaction/shortcut consistes of the following
 **    -Shortcut-type indicator [Required] (One character. either '~' or '&')
 **      '~' for sequence-key shortcut and '&' for synchronous-key shortcut.
 **    -Keys [Required] (One or more pairs of hexadecimal characters. Letters must be in upper case)
 **      Representing either the sequence of keystrokes for '~' or the syncronouse keys for '&' 
 **    -A '#' character to indicate the end of the keys parameter [Required]
 **    -The command-line or path/fileName of the item to be opened/executed
 **    -A '#' character followed by window class name [Optional]
 **      A class name of a window to enable the shortcut on 
 **      (omit this parameter to make a gloabal shortcut)
 **    -A '*' character to indicate the end of the of the shortcut-action parameters [Required]
 **    -A 'H' to run the action on SW_HIDE mode [Optional]
 **    -A newline character to indecate the end of the KeyboardReactions entry [Required].
 ** +Note: spaces could only be found on the command line (if needed)
 ** +The command line is limited to 255 ASCII characters
 ** +Each shortcut is limited to 32 keys
 ** +If a file named ReloadKeyboardReactions.dat is created in the same directory,
 **   the application reloads KeyboardReactions.dat and deletes ReloadKeyboardReactions.dat.
 **/
