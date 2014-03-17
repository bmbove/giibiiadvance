/*
    GiiBiiAdvance - GBA/GB  emulator
    Copyright (C) 2011-2014 Antonio Ni�o D�az (AntonioND)

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <SDL2/SDL.h>

#include <string.h>
#include <malloc.h>

#include "../debug_utils.h"
#include "../window_handler.h"
#include "../font_utils.h"
#include "../general_utils.h"
#include "../file_utils.h"

#include "win_gba_sprviewer.h"
#include "win_main.h"
#include "win_utils.h"

#include "../gba_core/gba.h"
#include "../gba_core/memory.h"
#include "../gba_core/gba_debug_video.h"

#include "../png/png_utils.h"

//-----------------------------------------------------------------------------------

static int WinIDGBASprViewer;

#define WIN_GBA_SPRVIEWER_WIDTH  821
#define WIN_GBA_SPRVIEWER_HEIGHT 668

static int GBASprViewerCreated = 0;

//-----------------------------------------------------------------------------------

static u32 gba_sprview_selected_spr = 0;
static u32 gba_sprview_selected_page = 0;

#define GBA_SPR_ALLSPR_BUFFER_WIDTH  ((64+16)*8+16)
#define GBA_SPR_ALLSPR_BUFFER_HEIGHT ((64+16)*8+16)

static char gba_spr_allspr_buffer[GBA_SPR_ALLSPR_BUFFER_WIDTH*GBA_SPR_ALLSPR_BUFFER_HEIGHT*3];

#define GBA_SPR_ZOOMED_BUFFER_WIDTH  (64*2)
#define GBA_SPR_ZOOMED_BUFFER_HEIGHT (64*2)

static char gba_spr_zoomed_buffer[GBA_SPR_ZOOMED_BUFFER_WIDTH*GBA_SPR_ZOOMED_BUFFER_HEIGHT*3];

//-----------------------------------------------------------------------------------

static _gui_console gba_sprview_con;
static _gui_element gba_sprview_textbox;

static _gui_element gba_sprview_allspr_dumpbtn;
static _gui_element gba_sprview_pagespr_dumpbtn;
static _gui_element gba_sprview_zoomed_spr_dumpbtn;

static _gui_element gba_sprview_allspr_bmp, gba_sprview_zoomedspr_bmp;

static _gui_element gba_sprview_page0_radbtn, gba_sprview_page1_radbtn;

static _gui_element * gba_sprviwer_window_gui_elements[] = {
    &gba_sprview_allspr_bmp,
    &gba_sprview_zoomedspr_bmp,
    &gba_sprview_textbox,
    &gba_sprview_allspr_dumpbtn,
    &gba_sprview_pagespr_dumpbtn,
    &gba_sprview_zoomed_spr_dumpbtn,
    &gba_sprview_page0_radbtn,
    &gba_sprview_page1_radbtn,
    NULL
};

static _gui gba_sprviewer_window_gui = {
    gba_sprviwer_window_gui_elements,
    NULL,
    NULL
};

//----------------------------------------------------------------

void Win_GBASprViewerUpdate(void)
{
    if(GBASprViewerCreated == 0) return;

    if(Win_MainRunningGBA() == 0) return;

    GUI_ConsoleClear(&gba_sprview_con);

    static const int spr_size[4][4][2] = { //shape, size, (x,y)
        {{8,8},{16,16},{32,32},{64,64}}, //Square
        {{16,8},{32,8},{32,16},{64,32}}, //Horizontal
        {{8,16},{8,32},{16,32},{32,64}}, //Vertical
        {{0,0},{0,0},{0,0},{0,0}} //Prohibited
    };

    _oam_spr_entry_t * spr = &(((_oam_spr_entry_t*)Mem.oam)[gba_sprview_selected_spr]);
    u16 attr0 = spr->attr0;
    u16 attr1 = spr->attr1;
    u16 attr2 = spr->attr2;
    int isaffine = attr0 & BIT(8);
    u16 shape = attr0>>14;
    u16 size = attr1>>14;
    int sx = spr_size[shape][size][0];
    int sy = spr_size[shape][size][1];
    int y = (attr0&0xFF); y |= (y < 160) ? 0 : 0xFFFFFF00;
    int x = (int)(attr1&0x1FF) | ((attr1&BIT(8))?0xFFFFFE00:0);
    int mosaic = attr0 & BIT(12);
    int matrix_entry = (attr1>>9) & 0x1F;
    int mode = (attr0>>10)&3;
    int colors = (attr0&BIT(13)) ? 256:16;
    u16 tilebaseno = attr2&0x3FF;
    if(attr0&BIT(13)) tilebaseno>>= 1; //tiles need double space in 256 colors mode
    int vflip = (attr1 & BIT(13));
    int hflip = (attr1 & BIT(12));
    u16 prio = (attr2 >> 10) & 3;
    u16 palno = (attr0&BIT(13)) ?  0 : (attr2>>12);
    int doublesize = (attr0 & BIT(9));
    static const char * spr_mode[4] = {"Normal","Transp.","Window","Prohibited"};
    GUI_ConsoleModePrintf(&gba_sprview_con,0,0,"Number: %d\nType: %s\nMatrix entry: %d\nSize: %dx%d\nPosition: %d,%d\n"
        "Mode: %d - %s\nTile base: %d\nColors: %d\nPriority: %d\nPal. Number: %d\n"
        "Attr: %04X|%04X|%04X\n"
        "Other: %s%s%s%s",
        gba_sprview_selected_spr, isaffine?"Affine" :"Regular",matrix_entry, sx,sy, x,y,
        mode, spr_mode[mode],tilebaseno,colors,prio,palno,
        attr0,attr1,attr2,
        mosaic?"M":" ", hflip?"H":" ", vflip?"V":" ", doublesize?"D":" ");

    int i,j;
    for(i = 0; i < GBA_SPR_ZOOMED_BUFFER_WIDTH; i++) for(j = 0; j < GBA_SPR_ZOOMED_BUFFER_HEIGHT; j++)
    {
        gba_spr_zoomed_buffer[(j*GBA_SPR_ZOOMED_BUFFER_HEIGHT+i)*3+0] = ((i&32)^(j&32)) ? 0x80 : 0xB0;
        gba_spr_zoomed_buffer[(j*GBA_SPR_ZOOMED_BUFFER_HEIGHT+i)*3+1] = ((i&32)^(j&32)) ? 0x80 : 0xB0;
        gba_spr_zoomed_buffer[(j*GBA_SPR_ZOOMED_BUFFER_HEIGHT+i)*3+2] = ((i&32)^(j&32)) ? 0x80 : 0xB0;
    }
    GBA_Debug_PrintZoomedSpriteAt(gba_sprview_selected_spr,0,gba_spr_zoomed_buffer,
                GBA_SPR_ZOOMED_BUFFER_WIDTH,GBA_SPR_ZOOMED_BUFFER_HEIGHT,
                0,0, GBA_SPR_ZOOMED_BUFFER_WIDTH,GBA_SPR_ZOOMED_BUFFER_HEIGHT);

    GBA_Debug_PrintSpritesPage(gba_sprview_selected_page, 0, gba_spr_allspr_buffer,
                GBA_SPR_ALLSPR_BUFFER_WIDTH,GBA_SPR_ALLSPR_BUFFER_HEIGHT);
}

//----------------------------------------------------------------

static int _win_gba_sprviewer_allspr_bmp_callback(int x, int y)
{
    int x_ = (x-8) / (64+16);
    if(x_ > 7) x_ = 7;
    if(x_ < 0) x_ = 0;

    int y_ = (y-8) / (64+16);
    if(y_ > 7) y_ = 7;
    if(y_ < 0) y_ = 0;

    gba_sprview_selected_spr = (gba_sprview_selected_page*64) + (y_*8) + x_;
    return 1;
}

static void _win_gba_sprviewer_radbtn_callback(int btn_id)
{
    gba_sprview_selected_page = btn_id;
    Win_GBASprViewerUpdate();
}

//----------------------------------------------------------------

void Win_GBASprViewerRender(void)
{
    if(GBASprViewerCreated == 0) return;

    char buffer[WIN_GBA_SPRVIEWER_WIDTH*WIN_GBA_SPRVIEWER_HEIGHT*3];
    GUI_Draw(&gba_sprviewer_window_gui,buffer,WIN_GBA_SPRVIEWER_WIDTH,WIN_GBA_SPRVIEWER_HEIGHT,1);

    WH_Render(WinIDGBASprViewer, buffer);
}

int Win_GBASprViewerCallback(SDL_Event * e)
{
    if(GBASprViewerCreated == 0) return 1;

    int redraw = GUI_SendEvent(&gba_sprviewer_window_gui,e);

    int close_this = 0;

    if(e->type == SDL_WINDOWEVENT)
    {
        if(e->window.event == SDL_WINDOWEVENT_FOCUS_GAINED)
        {
            redraw = 1;
        }
        else if(e->window.event == SDL_WINDOWEVENT_EXPOSED)
        {
            redraw = 1;
        }
        else if(e->window.event == SDL_WINDOWEVENT_CLOSE)
        {
            close_this = 1;
        }
    }
    else if( e->type == SDL_KEYDOWN)
    {
        if( e->key.keysym.sym == SDLK_ESCAPE )
        {
            close_this = 1;
        }
    }

    if(close_this)
    {
        GBASprViewerCreated = 0;
        WH_Close(WinIDGBASprViewer);
        return 1;
    }

    if(redraw)
    {
        Win_GBASprViewerUpdate();
        Win_GBASprViewerRender();
        return 1;
    }

    return 0;
}

//----------------------------------------------------------------

static void _win_gba_sprviewer_page_dump_btn_callback(void)
{
    char pagebuf[GBA_SPR_ALLSPR_BUFFER_WIDTH*GBA_SPR_ALLSPR_BUFFER_HEIGHT*4];
    GBA_Debug_PrintSpritesPage(gba_sprview_selected_page, 1, pagebuf,
                GBA_SPR_ALLSPR_BUFFER_WIDTH,GBA_SPR_ALLSPR_BUFFER_HEIGHT);

    char * name = (gba_sprview_selected_page == 0) ? FU_GetNewTimestampFilename("gba_sprite_page0") :
                                                     FU_GetNewTimestampFilename("gba_sprite_page1");
    Save_PNG(name,GBA_SPR_ALLSPR_BUFFER_WIDTH,GBA_SPR_ALLSPR_BUFFER_HEIGHT,pagebuf,1);

    //Win_GBASprViewerUpdate();
}

static void _win_gba_sprviewer_allspr_dump_btn_callback(void)
{
    char * allbuf = malloc(GBA_SPR_ALLSPR_BUFFER_WIDTH*((GBA_SPR_ALLSPR_BUFFER_HEIGHT*2)-16)*4);
    if(allbuf == NULL)
        return;

    GBA_Debug_PrintSpritesPage(0, 1, allbuf,
                GBA_SPR_ALLSPR_BUFFER_WIDTH,((GBA_SPR_ALLSPR_BUFFER_HEIGHT*2)-16));

    GBA_Debug_PrintSpritesPage(1, 1, &(allbuf[GBA_SPR_ALLSPR_BUFFER_WIDTH*(GBA_SPR_ALLSPR_BUFFER_HEIGHT-16)*4]),
                GBA_SPR_ALLSPR_BUFFER_WIDTH,GBA_SPR_ALLSPR_BUFFER_HEIGHT);

    char * name = FU_GetNewTimestampFilename("gba_sprite_all");
    Save_PNG(name,GBA_SPR_ALLSPR_BUFFER_WIDTH,((GBA_SPR_ALLSPR_BUFFER_HEIGHT*2)-16),allbuf,1);

    free(allbuf);

    //Win_GBASprViewerUpdate();
}

static void _win_gba_sprviewer_zoomed_dump_btn_callback(void)
{
    static const int spr_size[4][4][2] = { //shape, size, (x,y)
        {{8,8},{16,16},{32,32},{64,64}}, //Square
        {{16,8},{32,8},{32,16},{64,32}}, //Horizontal
        {{8,16},{8,32},{16,32},{32,64}}, //Vertical
        {{0,0},{0,0},{0,0},{0,0}} //Prohibited
    };

    _oam_spr_entry_t * spr = &(((_oam_spr_entry_t*)Mem.oam)[gba_sprview_selected_spr]);
    u16 attr0 = spr->attr0;
    u16 attr1 = spr->attr1;
    u16 shape = attr0>>14;
    u16 size = attr1>>14;
    int sx = spr_size[shape][size][0];
    int sy = spr_size[shape][size][1];

    char buf[sx*sy*4];
    GBA_Debug_PrintZoomedSpriteAt(gba_sprview_selected_spr,1,buf, sx,sy, 0,0, sx,sy);

    char * name = FU_GetNewTimestampFilename("gba_sprite");
    Save_PNG(name,sx,sy,buf,1);

    //Win_GBASprViewerUpdate();
}

//----------------------------------------------------------------

int Win_GBASprViewerCreate(void)
{
    if(GBASprViewerCreated == 1)
        return 0;

    if(Win_MainRunningGBA() == 0) return 0;

    GUI_SetTextBox(&gba_sprview_textbox,&gba_sprview_con,
                   668,140, 21*FONT_12_WIDTH,12*FONT_12_HEIGHT, NULL);

    GUI_SetBitmap(&gba_sprview_allspr_bmp,6,6,GBA_SPR_ALLSPR_BUFFER_WIDTH,GBA_SPR_ALLSPR_BUFFER_HEIGHT,gba_spr_allspr_buffer,
                  _win_gba_sprviewer_allspr_bmp_callback);
    GUI_SetBitmap(&gba_sprview_zoomedspr_bmp,668,6,GBA_SPR_ZOOMED_BUFFER_WIDTH,GBA_SPR_ZOOMED_BUFFER_HEIGHT,gba_spr_zoomed_buffer,
                  NULL);

    GUI_SetRadioButton(&gba_sprview_page0_radbtn,  668,290,12*FONT_12_WIDTH,24,
                  "  0 -  63", 0, 0,  1,_win_gba_sprviewer_radbtn_callback);
    GUI_SetRadioButton(&gba_sprview_page1_radbtn,  668,321,12*FONT_12_WIDTH,24,
                  " 64 - 127", 0, 1,  0,_win_gba_sprviewer_radbtn_callback);

    GUI_SetButton(&gba_sprview_zoomed_spr_dumpbtn,668,352,FONT_12_WIDTH*13,FONT_12_HEIGHT+6,"Dump zoomed",
                  _win_gba_sprviewer_zoomed_dump_btn_callback);

    GUI_SetButton(&gba_sprview_pagespr_dumpbtn,668,383,FONT_12_WIDTH*13,FONT_12_HEIGHT+6,"Dump page",
                  _win_gba_sprviewer_page_dump_btn_callback);

    GUI_SetButton(&gba_sprview_allspr_dumpbtn,668,414,FONT_12_WIDTH*13,FONT_12_HEIGHT+6,"Dump all",
                  _win_gba_sprviewer_allspr_dump_btn_callback);

    gba_sprview_selected_spr = 0;
    gba_sprview_selected_page = 0;

    GBASprViewerCreated = 1;

    WinIDGBASprViewer = WH_Create(WIN_GBA_SPRVIEWER_WIDTH,WIN_GBA_SPRVIEWER_HEIGHT, 0,0, 0);
    WH_SetCaption(WinIDGBASprViewer,"GBA Sprite Viewer");

    WH_SetEventCallback(WinIDGBASprViewer,Win_GBASprViewerCallback);

    Win_GBASprViewerUpdate();
    Win_GBASprViewerRender();

    return 1;
}
